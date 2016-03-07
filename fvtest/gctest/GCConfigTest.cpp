/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "GCConfigTest.hpp"
#include "VerboseWriterChain.hpp"
#include "ObjectIterator.hpp"
#include "omrgc.h"

//#define OMRGCTEST_DEBUG
//#define OMRGCTEST_PRINTFILE

#define MAX_NAME_LENGTH 512
#define OMRGCTEST_CHECK_RT(rt) \
	if (0 != (rt)) {\
		goto done;\
	}

void
GCConfigTest::SetUp()
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	printMemUsed("Setup()", gcTestEnv->portLib);

	/* Initialize heap and collector */
	MM_StartupManagerTestExample startupManager(exampleVM->_omrVM, GetParam());
	omr_error_t rc = OMR_GC_IntializeHeapAndCollector(exampleVM->_omrVM, &startupManager);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_GC_IntializeHeapAndCollector failed, rc=" << rc;

	/* Attach calling thread to the VM */
	exampleVM->_omrVMThread = NULL;
	rc = OMR_Thread_Init(exampleVM->_omrVM, NULL, &exampleVM->_omrVMThread, "OMRTestThread");
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_Thread_Init failed, rc=" << rc;

	/* Kick off the dispatcher threads */
	rc = OMR_GC_InitializeDispatcherThreads(exampleVM->_omrVMThread);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_GC_InitializeDispatcherThreads failed, rc=" << rc;
	env = MM_EnvironmentBase::getEnvironment(exampleVM->_omrVMThread);

	/* load config file */
	omrtty_printf("Configuration File: %s\n", GetParam());
#if defined(OMRGCTEST_PRINTFILE)
	printFile(GetParam());
#endif
	pugi::xml_parse_result result = doc.load_file(GetParam());
	if (!result) {
		FAIL() << "Failed to load test configuration file (" << GetParam() << ") with error description: " << result.description() << ".";
	}

	/* parse verbose information and initialize verbose manager */
	pugi::xml_node optionNode = doc.select_node("/gc-config/option").node();
	const char *verboseFileNamePrefix = optionNode.attribute("verboseLog").value();
	numOfFiles = (uintptr_t)optionNode.attribute("numOfFiles").as_int();
	uintptr_t numOfCycles = (uintptr_t)optionNode.attribute("numOfCycles").as_int();
	if (0 == strcmp(verboseFileNamePrefix, "")) {
		verboseFileNamePrefix = "VerboseGCOutput";
	}
	verboseFile = (char *)omrmem_allocate_memory(MAX_NAME_LENGTH, OMRMEM_CATEGORY_MM);
	if (NULL == verboseFile) {
		FAIL() << "Failed to allocate native memory.";
	}
	omrstr_printf(verboseFile, MAX_NAME_LENGTH, "%s_%d_%lld.xml", verboseFileNamePrefix, omrsysinfo_get_pid(), omrtime_current_time_millis());
	verboseManager = MM_VerboseManager::newInstance(env, exampleVM->_omrVM);
	verboseManager->configureVerboseGC(exampleVM->_omrVM, verboseFile, numOfFiles, numOfCycles);
	omrtty_printf("Verbose File: %s\n", verboseFile);
#if defined(OMRGCTEST_DEBUG)
	omrtty_printf("Verbose GC log name: %s; numOfFiles: %d; numOfCycles: %d.\n", verboseFile, numOfFiles, numOfCycles);
#endif
	verboseManager->enableVerboseGC();
	verboseManager->setInitializedTime(omrtime_hires_clock());

	/* create object table */
	objectTable.create();

	/* create root table */
	exampleVM->rootTable = hashTableNew(
		gcTestEnv->portLib, OMR_GET_CALLSITE(), 0, sizeof(RootEntry), 0, 0, OMRMEM_CATEGORY_MM,
		rootTableHashFn, rootTableHashEqualFn, NULL, NULL);
}

void
GCConfigTest::TearDown()
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	/* free root table */
	hashTableFree(exampleVM->rootTable);

	/* free object table */
	objectTable.free();

	/* close verboseManager */
	verboseManager->closeStreams(env);
	verboseManager->disableVerboseGC();
	verboseManager->kill(env);
	omrmem_free_memory((void *)verboseFile);

	/* Shut down the dispatcher threads */
	omr_error_t rc = OMR_GC_ShutdownDispatcherThreads(exampleVM->_omrVMThread);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "TearDown(): OMR_GC_ShutdownDispatcherThreads failed, rc=" << rc;

	/* Shut down collector */
	rc = OMR_GC_ShutdownCollector(exampleVM->_omrVMThread);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "TearDown(): OMR_GC_ShutdownCollector failed, rc=" << rc;

	/* Detach from VM */
	rc = OMR_Thread_Free(exampleVM->_omrVMThread);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "TearDown(): OMR_Thread_Free failed, rc=" << rc;

	/* Shut down heap */
	rc = OMR_GC_ShutdownHeap(exampleVM->_omrVM);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "TearDown(): OMR_GC_ShutdownHeap failed, rc=" << rc;

	printMemUsed("TearDown()", gcTestEnv->portLib);
}

void
GCConfigTest::freeAttributeList(AttributeElem *root)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	AttributeElem *cur = root;
	while (NULL != cur) {
		J9_LINKED_LIST_REMOVE(root, cur);
		omrmem_free_memory(cur);
		cur = root;
	}
}

int32_t
GCConfigTest::parseAttribute(AttributeElem **root, const char *attrStr)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	const char DELIM = ',';
	char *copyOfAttrStr = (char *)omrmem_allocate_memory(strlen(attrStr) + 1, OMRMEM_CATEGORY_MM);
	char *attrStart = NULL;
	char *attrEnd = NULL;

	if (NULL == copyOfAttrStr) {
		rt = 1;
		omrtty_printf("%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
		goto done;
	}
	strcpy(copyOfAttrStr, attrStr);
	attrStart = copyOfAttrStr;
	attrEnd = attrStart;
	while (NULL != attrEnd) {
		attrEnd = strchr(attrStart, DELIM);
		int32_t attr = 0;
		if (NULL != attrEnd) {
			*attrEnd = '\0';
			attr = atoi(attrStart);
			attrStart = attrEnd + 1;
		} else {
			attr = atoi(attrStart);
		}
		AttributeElem *attrNode = (AttributeElem *)omrmem_allocate_memory(sizeof(AttributeElem), OMRMEM_CATEGORY_MM);
		if (NULL == attrNode) {
			rt = 1;
			omrtty_printf("%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
			goto done;
		}
		attrNode->value = attr;
		J9_LINKED_LIST_ADD_LAST(*root, attrNode);
	}
	omrmem_free_memory(copyOfAttrStr);

done:
	return rt;
}

OMRGCObjectType
GCConfigTest::parseObjectType(pugi::xml_node node)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	const char *namePrefixStr = node.attribute("namePrefix").value();
	const char *typeStr = node.attribute("type").value();
	const char *parentStr = node.parent().name();
	const char *parentTypeStr = node.parent().attribute("type").value();
	OMRGCObjectType objType = INVALID;

	if (0 == strcmp(typeStr, "root")) {
		if (0 == strcmp(parentStr, "allocation")) {
			objType = ROOT;
		} else {
			omrtty_printf("%s:%d Invalid XML input: root object %s can not be child of other object!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else if (0 == strcmp(typeStr, "normal")) {
		if ((0 == strcmp(parentTypeStr, "root")) || (0 == strcmp(parentTypeStr, "normal"))) {
			objType = NORMAL;
		} else {
			omrtty_printf("%s:%d Invalid XML input: normal object %s has to be child of root or normal object!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else if (0 == strcmp(typeStr, "garbage")) {
		if (0 == strcmp(parentStr, "allocation")) {
			objType = GARBAGE_ROOT;
		} else if ((0 == strcmp(parentTypeStr, "normal")) || (0 == strcmp(parentTypeStr, "root"))){
		objType = GARBAGE_TOP;
		} else if (0 == strcmp(parentTypeStr, "garbage")) {
			objType = GARBAGE_CHILD;
		} else {
			omrtty_printf("%s:%d Invalid XML input: garbage object %s has to be child of root, normal or allocation node!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else {
		omrtty_printf("%s:%d Invalid XML input: object %s type should be root, normal or garbage .\n", __FILE__, __LINE__, namePrefixStr);
	}

	return objType;
}

int32_t
GCConfigTest::allocateHelper(omrobjectptr_t *obj, char *objName, uintptr_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	uintptr_t actualSize = 0;
	MM_ObjectAllocationInterface *allocationInterface = env->_objectAllocationInterface;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t allocatedFlags = 0;
	uintptr_t sizeAdjusted = extensions->objectModel.adjustSizeInBytes(size);
	MM_AllocateDescription mm_allocdescription(sizeAdjusted, allocatedFlags, true, true);

	*obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), false);

	if (NULL == *obj) {
		omrtty_printf("No free memory to allocate %s, GC start.\n", objName);
		*obj = (omrobjectptr_t)allocationInterface->allocateObject(env, &mm_allocdescription, env->getMemorySpace(), true);
		env->unwindExclusiveVMAccessForGC();
		if (NULL == *obj) {
			rt = 1;
			omrtty_printf("%s:%d No free memory after a GC. Failed to allocate object %s.\n", __FILE__, __LINE__, objName);
			goto done;
		}
		verboseManager->getWriterChain()->endOfCycle(env);
	}

	actualSize = mm_allocdescription.getBytesRequested();

	if (NULL != *obj) {
		memset(*obj, 0, actualSize);
	}

	/* set size in header */
	extensions->objectModel.setObjectSize(*obj, actualSize);

#if defined(OMRGCTEST_DEBUG)
		omrtty_printf("Allocate object name: %s; size: %dB; objPtr: %llu\n", objName, actualSize, *obj);
#endif

done:
	return rt;
}

int32_t
GCConfigTest::createObject(omrobjectptr_t *obj, char **objName, const char *namePrefix, OMRGCObjectType objType, int32_t depth, int32_t nthInRow, uintptr_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	ObjectEntry *objEntry = NULL;

	*objName = (char *)omrmem_allocate_memory(MAX_NAME_LENGTH, OMRMEM_CATEGORY_MM);
	if (NULL == *objName) {
		rt = 1;
		omrtty_printf("%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
		goto done;
	}
	omrstr_printf(*objName, MAX_NAME_LENGTH, "%s_%d_%d", namePrefix, depth, nthInRow);

	if (0 == objectTable.find(&objEntry, *objName)) {
#if defined(OMRGCTEST_DEBUG)
		omrtty_printf("Find object %s in hash table.\n", *objName);
#endif
		*obj = objEntry->objPtr;
	} else {
		rt = allocateHelper(obj, *objName, size);
		OMRGCTEST_CHECK_RT(rt);

		/* keep object in table */
		rt = objectTable.add(*objName, *obj, 0);
		OMRGCTEST_CHECK_RT(rt)

		/* Keep count of the new allocated non-garbage object size for garbage insertion. If the object exists in objectTable, its size is ignored. */
		if ((ROOT == objType) || (NORMAL == objType)) {
			gp.accumulatedSize += env->getExtensions()->objectModel.getSizeInBytesWithHeader(*obj);
		}
	}

done:
	return rt;
}

int32_t
GCConfigTest::createFixedSizeTree(omrobjectptr_t *rootObj, const char *namePrefixStr, OMRGCObjectType objType, uintptr_t totalSize, uintptr_t objSize, int32_t breadth)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	OMR_VM *omrVMPtr = exampleVM->_omrVM;
	int32_t depth = 1;
	int32_t numOfParents = 1;
	uintptr_t bytesLeft = totalSize;

	if (bytesLeft < objSize) {
		objSize = bytesLeft;
		if (objSize <= 0) {
			return rt;
		}
	}

	/* allocate the root object */
	char *rootObjName = NULL;
	rt = createObject(rootObj, &rootObjName, namePrefixStr, objType, 0, 0, objSize);
	OMRGCTEST_CHECK_RT(rt);
	bytesLeft -= objSize;

	/* Add object to root hash table. If it's the root of the garbage tree, temporarily keep it as root for allocating
	 * the rest of the tree. Remove it from the root set after the entire garbage tree is allocated.*/
	RootEntry rEntry;
	rEntry.name = rootObjName;
	rEntry.rootPtr = *rootObj;
	if (NULL == hashTableAdd(exampleVM->rootTable, &rEntry)) {
		rt = 1;
		omrtty_printf("%s:%d Failed to add new root entry to root table!\n", __FILE__, __LINE__);
		goto done;
	}

	/* create the rest of the tree */
	if (ROOT == objType) {
		objType = NORMAL;
	} else if ((GARBAGE_TOP == objType) || (GARBAGE_ROOT == objType)) {
		objType = GARBAGE_CHILD;
	}
	while (bytesLeft > 0) {
		int32_t nthInRow = 0;
		for (int32_t j = 0; j < numOfParents; j++) {
			char parentName[MAX_NAME_LENGTH];
			omrstr_printf(parentName, sizeof(parentName), "%s_%d_%d", namePrefixStr, (depth - 1), j);
			ObjectEntry *parentEntry;
			rt = objectTable.find(&parentEntry, parentName);
			if (0 != rt) {
				omrtty_printf("%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
				goto done;
			}
			omrobjectptr_t parent = parentEntry->objPtr;
			GC_ObjectIterator objectIterator(omrVMPtr, parent);
			objectIterator.restore(parentEntry->numOfRef);
			for (int32_t k = 0; k < breadth; k++) {
				if (bytesLeft < objSize) {
					objSize = bytesLeft;
					if (objSize <= 0) {
						goto done;
					}
				}
				omrobjectptr_t obj;
				char *objName = NULL;
				rt = createObject(&obj, &objName, namePrefixStr, objType, depth, nthInRow, objSize);
				OMRGCTEST_CHECK_RT(rt);
				bytesLeft -= objSize;
				nthInRow += 1;
				GC_SlotObject *slotObject = NULL;
				if (NULL != (slotObject = objectIterator.nextSlot())) {
#if defined(OMRGCTEST_DEBUG)
					omrtty_printf("\tadd to parent(%llu) slot %d.\n", parent, parentEntry->numOfRef + k);
#endif
					slotObject->writeReferenceToSlot(obj);
				} else {
					rt = 1;
					omrtty_printf("%s:%d Invalid XML input: numOfFields defined for %s is not enough to hold all children references.\n", __FILE__, __LINE__, parentName);
					goto done;
				}
			}
			parentEntry->numOfRef += breadth;
		}
		numOfParents = nthInRow;
		depth += 1;
	}

done:
	return rt;
}

int32_t
GCConfigTest::processObjNode(pugi::xml_node node, const char *namePrefixStr, OMRGCObjectType objType, AttributeElem *numOfFieldsElem, AttributeElem *breadthElem, int32_t depth)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	int32_t rt = 0;
	OMR_VM *omrVMPtr = exampleVM->_omrVM;
	int32_t numOfParents = 0;

	/* Create objects and store them in the root table or the slot object based on objType. */
	if ((ROOT == objType) || (GARBAGE_ROOT == objType)) {
		for (int32_t i = 0; i < breadthElem->value; i++) {
			omrobjectptr_t obj;
			char *objName = NULL;

			uintptr_t sizeCalculated = numOfFieldsElem->value * sizeof(fomrobject_t) + sizeof(uintptr_t);
			rt = createObject(&obj, &objName, namePrefixStr, objType, 0, i, sizeCalculated);
			OMRGCTEST_CHECK_RT(rt);
			numOfFieldsElem = numOfFieldsElem->linkNext;

			/* Add object to root hash table. If it's the root of the garbage tree, temporarily keep it as root for allocating
			 * the rest of the tree. Remove it from the root set after the entire garbage tree is allocated.*/
			RootEntry rEntry;
			rEntry.name = objName;
			rEntry.rootPtr = obj;
			if (NULL == hashTableAdd(exampleVM->rootTable, &rEntry)) {
				rt = 1;
				omrtty_printf("%s:%d Failed to add new root entry to root table!\n", __FILE__, __LINE__);
				goto done;
			}

			/* insert garbage per node */
			if ((ROOT == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
				rt = insertGarbage();
				OMRGCTEST_CHECK_RT(rt);
			}
		}
	} else if ((NORMAL == objType) || (GARBAGE_TOP == objType) || (GARBAGE_CHILD == objType)) {
		char parentName[MAX_NAME_LENGTH];
		omrstr_printf(parentName, MAX_NAME_LENGTH, "%s_%d_%d", node.parent().attribute("namePrefix").value(), 0, 0);
		ObjectEntry *parentEntry;
		rt = objectTable.find(&parentEntry, parentName);
		if (0 != rt) {
			omrtty_printf("%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
			goto done;
		}
		omrobjectptr_t parent = parentEntry->objPtr;
		GC_ObjectIterator objectIterator(omrVMPtr, parent);
		objectIterator.restore(parentEntry->numOfRef);
		for (int32_t i = 0; i < breadthElem->value; i++) {
			omrobjectptr_t obj;
			char *objName = NULL;
			uintptr_t sizeCalculated = numOfFieldsElem->value * sizeof(fomrobject_t) + sizeof(uintptr_t);
			rt = createObject(&obj, &objName, namePrefixStr, objType, 0, i, sizeCalculated);
			OMRGCTEST_CHECK_RT(rt);
			numOfFieldsElem = numOfFieldsElem->linkNext;
			GC_SlotObject *slotObject = NULL;
			if (NULL != (slotObject = objectIterator.nextSlot())) {
#if defined(OMRGCTEST_DEBUG)
				omrtty_printf("\tadd to slot of parent(%llu) %d\n", parent, parentEntry->numOfRef + i);
#endif
				/* Add object to parent's slot. If it's the top of the garbage tree, temporarily keep it in the slot until
				 * the rest of the tree is allocated. */
				slotObject->writeReferenceToSlot(obj);
			} else {
				rt = 1;
				omrtty_printf("%s:%d Invalid XML input: numOfFields defined for %s is not enough to hold all children references.\n", __FILE__, __LINE__, parentName);
				goto done;
			}

			/* insert garbage per node */
			if ((NORMAL == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
				rt = insertGarbage();
				OMRGCTEST_CHECK_RT(rt);
			}
		}
		parentEntry->numOfRef += breadthElem->value;
	} else {
		rt = 1;
		goto done;
	}

	/* Create the rest of the tree, if any, defined with depth. First, we set the child object with correct objType. */
	if (ROOT == objType) {
		objType = NORMAL;
	} else if ((GARBAGE_TOP == objType) || (GARBAGE_ROOT == objType)) {
		objType = GARBAGE_CHILD;
	}
	numOfParents = breadthElem->value;
	breadthElem = breadthElem->linkNext;
	for (int32_t i = 1; i < depth; i++) {
		int32_t nthInRow = 0;
		for (int32_t j = 0; j < numOfParents; j++) {
			char parentName[MAX_NAME_LENGTH];
			omrstr_printf(parentName, sizeof(parentName), "%s_%d_%d", namePrefixStr, (i - 1), j);
			ObjectEntry *parentEntry;
			rt = objectTable.find(&parentEntry, parentName);
			if (0 != rt) {
				omrtty_printf("%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
				goto done;
			}
			omrobjectptr_t parent = parentEntry->objPtr;
			GC_ObjectIterator objectIterator(omrVMPtr, parent);
			objectIterator.restore(parentEntry->numOfRef);
			for (int32_t k = 0; k < breadthElem->value; k++) {
				omrobjectptr_t obj;
				char *objName = NULL;
				uintptr_t sizeCalculated = numOfFieldsElem->value * sizeof(fomrobject_t) + sizeof(uintptr_t);
				rt = createObject(&obj, &objName, namePrefixStr, objType, i, nthInRow, sizeCalculated);
				OMRGCTEST_CHECK_RT(rt);
				numOfFieldsElem = numOfFieldsElem->linkNext;
				nthInRow += 1;
				GC_SlotObject *slotObject = NULL;
				if (NULL != (slotObject = objectIterator.nextSlot())) {
#if defined(OMRGCTEST_DEBUG)
					omrtty_printf("\tadd to parent(%llu) slot %d.\n", parent, parentEntry->numOfRef + k);
#endif
					slotObject->writeReferenceToSlot(obj);
				} else {
					rt = 1;
					omrtty_printf("%s:%d Invalid XML input: numOfFields defined for %s is not enough to hold all children references.\n", __FILE__, __LINE__, parentName);
					goto done;
				}

				/* insert garbage per node */
				if ((NORMAL == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
					rt = insertGarbage();
					OMRGCTEST_CHECK_RT(rt);
				}
			}
			parentEntry->numOfRef += breadthElem->value;
		}
		numOfParents = nthInRow;
		breadthElem = breadthElem->linkNext;
	}

done:
	return rt;
}

int32_t
GCConfigTest::insertGarbage()
{
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM->_omrVM);
	int32_t rt = 0;

	char fullNamePrefix[MAX_NAME_LENGTH];
	omrstr_printf(fullNamePrefix, sizeof(fullNamePrefix), "%s_%d", gp.namePrefix, gp.garbageSeq++);
	int32_t breadth = 2;
	uintptr_t totalSize = (uintptr_t)(gp.accumulatedSize * gp.percentage) / 100;
	uintptr_t objSize = 0;
	if (0 == strcmp(gp.structure, "node")) {
		objSize = totalSize;
	} else if (0 == strcmp(gp.structure, "tree")) {
		objSize = 64;
	}

#if defined(OMRGCTEST_DEBUG)
		omrtty_printf("Inserting garbage %s of %dB (%f%% of previous object/tree size %dB) ...\n", fullNamePrefix, totalSize, gp.percentage, gp.accumulatedSize);
#endif

	omrobjectptr_t rootObj = NULL;
	rt = createFixedSizeTree(&rootObj, fullNamePrefix, GARBAGE_ROOT, totalSize, objSize, breadth);
	OMRGCTEST_CHECK_RT(rt);

	/* remove garbage root object from the root set */
	if (NULL != rootObj) {
		char rootObjName[MAX_NAME_LENGTH];
		omrstr_printf(rootObjName, MAX_NAME_LENGTH, "%s_%d_%d", fullNamePrefix, 0, 0);
		rt = removeObjectFromRootTable(rootObjName);
		OMRGCTEST_CHECK_RT(rt);
	}

	/* reset accumulatedSize */
	gp.accumulatedSize = 0;

done:
	return rt;
}

int32_t
GCConfigTest::removeObjectFromRootTable(const char *name)
{
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM->_omrVM);
	int32_t rt = 0;
	RootEntry searchEntry;
	RootEntry *rEntryPtr = NULL;
	searchEntry.name = name;
	rEntryPtr = (RootEntry *)hashTableFind(exampleVM->rootTable, &searchEntry);
	if (NULL == rEntryPtr) {
		rt = 1;
		omrtty_printf("%s:%d Failed to find root object %s in root table.\n", __FILE__, __LINE__, name);
		goto done;
	}
	if (0 != hashTableRemove(exampleVM->rootTable, rEntryPtr)) {
		rt = 1;
		omrtty_printf("%s:%d Failed to remove root object %s from root table!\n", __FILE__, __LINE__, name);
		goto done;
	}
#if defined(OMRGCTEST_DEBUG)
	omrtty_printf("Remove object(%s) from root table.\n", name);
#endif

done:
	return rt;
}

int32_t
GCConfigTest::removeObjectFromParentSlot(const char *name, omrobjectptr_t parentPtr)
{
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM->_omrVM);
	int32_t rt = 1;
	GC_ObjectIterator objectIterator(exampleVM->_omrVM, parentPtr);
	GC_SlotObject *slotObject = NULL;

	ObjectEntry *objEntry;
	rt = objectTable.find(&objEntry, name);
	if (0 != rt) {
		omrtty_printf("%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, name);
		goto done;
	}

	while (NULL != (slotObject = objectIterator.nextSlot())) {
		if (objEntry->objPtr == slotObject->readReferenceFromSlot()) {
			slotObject->writeReferenceToSlot(NULL);
			rt = 0;
			break;
		}
	}
	if (0 != rt) {
		omrtty_printf("%s:%d Failed to find object %s from its parent's slot.\n", __FILE__, __LINE__, name);
		goto done;
	}

#if defined(OMRGCTEST_DEBUG)
	omrtty_printf("Remove object %s from its parent's slot.\n", name);
#endif

done:
	return rt;
}

int32_t
GCConfigTest::allocationWalker(pugi::xml_node node)
{
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM->_omrVM);
	int32_t rt = 0;
	AttributeElem *numOfFieldsElem = NULL;
	AttributeElem *breadthElem = NULL;
	int32_t depth = 0;
	OMRGCObjectType objType = INVALID;

	const char *namePrefixStr = node.attribute("namePrefix").value();
	const char *numOfFieldsStr = node.attribute("numOfFields").value();
	const char *typeStr = node.attribute("type").value();
	const char *breadthStr = node.attribute("breadth").value();
	const char *depthStr = node.attribute("depth").value();

	if (0 != strcmp(node.name(), "object")) {
		/* allow non-object node nested inside allocation? */
		goto done;
	}
	if ((0 == strcmp(namePrefixStr, "")) || (0 == strcmp(typeStr, "")) || (0 == strcmp(numOfFieldsStr, ""))) {
		rt = 1;
		omrtty_printf("%s:%d Invalid XML input: please specify namePrefix, type and numOfFields for object %s.\n", __FILE__, __LINE__, namePrefixStr);
		goto done;
	}
	/* set default value for breadth and depth to 1 */
	if (0 == strcmp(breadthStr, "")) {
		breadthStr = "1";
	}
	if (0 == strcmp(depthStr, "")) {
		depthStr = "1";
	}

	depth = atoi(depthStr);
	objType = parseObjectType(node);
	rt = parseAttribute(&numOfFieldsElem, numOfFieldsStr);
	OMRGCTEST_CHECK_RT(rt)
	rt = parseAttribute(&breadthElem, breadthStr);
	OMRGCTEST_CHECK_RT(rt);

	/* process current xml node, perform allocation for single object or object tree */
	rt = processObjNode(node, namePrefixStr, objType, numOfFieldsElem, breadthElem, depth);
	OMRGCTEST_CHECK_RT(rt);

	/* only single object can contain nested child object */
	if ((0 == strcmp(breadthStr, "1")) && (0 == strcmp(depthStr, "1"))) {
		for (pugi::xml_node childNode = node.first_child(); childNode; childNode = childNode.next_sibling()) {
			rt = allocationWalker(childNode);
			OMRGCTEST_CHECK_RT(rt);
		}
	}

	/* After the entire garbage tree is allocated, remove garbage root object from the root set
	 * or remove garbage top object from the slot of its parent object. */
	if (GARBAGE_ROOT == objType) {
		for (int32_t i = 0; i < breadthElem->value; i++) {
			char objName[MAX_NAME_LENGTH];
			omrstr_printf(objName, MAX_NAME_LENGTH, "%s_%d_%d", namePrefixStr, 0, i);
			rt = removeObjectFromRootTable(objName);
			OMRGCTEST_CHECK_RT(rt);
		}
	} else if (GARBAGE_TOP == objType) {
		char parentName[MAX_NAME_LENGTH];
		omrstr_printf(parentName, MAX_NAME_LENGTH, "%s_%d_%d", node.parent().attribute("namePrefix").value(), 0, 0);
		ObjectEntry *parentEntry;
		rt = objectTable.find(&parentEntry, parentName);
		if (0 != rt) {
			omrtty_printf("%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
			goto done;
		}
		omrobjectptr_t parentPtr = parentEntry->objPtr;
		for (int32_t i = 0; i < breadthElem->value; i++) {
			char objName[MAX_NAME_LENGTH];
			omrstr_printf(objName, MAX_NAME_LENGTH, "%s_%d_%d", namePrefixStr, 0, i);
			rt = removeObjectFromParentSlot(objName, parentPtr);
			OMRGCTEST_CHECK_RT(rt);
		}
	}

	/* insert garbage per tree */
	if ((0 == strcmp(gp.frequency, "perRootStruct")) && (ROOT == objType)){
		rt = insertGarbage();
		OMRGCTEST_CHECK_RT(rt);
	}

done:
	freeAttributeList(breadthElem);
	freeAttributeList(numOfFieldsElem);
	return rt;
}

#if defined(OMRGCTEST_PRINTFILE)
void
printFile(const char *name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	intptr_t fileDescriptor = omrfile_open(name, EsOpenRead, 0444);
	char readBuf[2048];
	while (readBuf == omrfile_read_text(fileDescriptor, readBuf, 2048)) {
		omrtty_printf("%s", readBuf);
	}
	omrfile_close(fileDescriptor);
	omrtty_printf("\n");
}
#endif

int32_t
GCConfigTest::verifyVerboseGC(pugi::xpath_node_set verboseGCs)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	uintptr_t seq = 1;
	size_t numOfNode = verboseGCs.size();
	bool *isFound = (bool *)omrmem_allocate_memory(sizeof(int32_t) * numOfNode, OMRMEM_CATEGORY_MM);
	if (NULL == isFound) {
		rt = 1;
		omrtty_printf("%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
		goto done;
	}
	for (size_t i = 0; i < numOfNode; i++) {
		isFound[i] = false;
	}

	/* Loop through multiple files if rolling log is enabled */
	do {
		pugi::xml_document verboseDoc;
		if (0 == numOfFiles) {
			verboseDoc.load_file(verboseFile);
			omrtty_printf("Parsing verbose log %s:\n", verboseFile);
#if defined(OMRGCTEST_PRINTFILE)
			printFile(verboseFile);
#endif
		} else {
			char currentVerboseFile[MAX_NAME_LENGTH];
			omrstr_printf(currentVerboseFile, MAX_NAME_LENGTH, "%s.%03zu", verboseFile, seq++);
			pugi::xml_parse_result result = verboseDoc.load_file(currentVerboseFile);
			if (pugi::status_file_not_found == result.status) {
				break;
			}
			omrtty_printf("Parsing verbose log %s:\n", currentVerboseFile);
#if defined(OMRGCTEST_PRINTFILE)
			printFile(currentVerboseFile);
#endif
		}

		/* verify each xquery criteria */
		int32_t i = 0;
		for (pugi::xpath_node_set::const_iterator it = verboseGCs.begin(); it != verboseGCs.end(); ++it) {
			const char *xpathNodesStr = it->node().attribute("xpathNodes").value();
			const char *xqueryStr = it->node().attribute("xquery").value();

			pugi::xpath_query resultQuery(xqueryStr);
			if (!resultQuery) {
				rt = 1;
				omrtty_printf("%s:%d Invalid xquery string \"%s\" specified in configuration file.\n", __FILE__, __LINE__, xqueryStr);
				goto done;
			}

			pugi::xpath_query nodeQuery(xpathNodesStr);
			if (!nodeQuery) {
				rt = 1;
				omrtty_printf("%s:%d Invalid xpathNodes string \"%s\" specified in configuration file.\n", __FILE__, __LINE__, xpathNodesStr);
				goto done;
			}

			pugi::xpath_node_set xpathNodes = nodeQuery.evaluate_node_set(verboseDoc);
			if (!xpathNodes.empty()) {
				bool isPassed = true;
				omrtty_printf("Verifying xquery \"%s\" on xpath \"%s\":\n", xqueryStr, xpathNodesStr);
				for (pugi::xpath_node_set::const_iterator it = xpathNodes.begin(); it != xpathNodes.end(); ++it) {
					pugi::xml_node node = it->node();
					if (!resultQuery.evaluate_boolean(node)) {
						rt = 1;
						isPassed = false;
						omrtty_printf("\t*FAILED* on node <%s", node.name());
						for (pugi::xml_attribute attr = node.first_attribute(); attr; attr = attr.next_attribute()) {
							omrtty_printf("\t%s=%s", attr.name(), attr.value());
						}
						omrtty_printf(">\n");
					}
				}
				if (isPassed) {
					omrtty_printf("*PASSED*\n");
				}
				isFound[i] = true;
			}
			i += 1;
		}
		omrtty_printf("\n");
	} while (seq <= numOfFiles);

	for (size_t i = 0; i < numOfNode; i++) {
		if (!isFound[i]) {
			rt = 1;
			omrtty_printf("*FAILED* Could not find xpath node \"%s\" in verbose output.\n", verboseGCs[i].node().attribute("xpathNodes").value());
		}
	}

done:
	omrmem_free_memory((void *)isFound);
	return rt;
}

int32_t
GCConfigTest::parseGarbagePolicy(pugi::xml_node node)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	gp.namePrefix = NULL;
	gp.percentage = 0.0f;
	gp.frequency = "none";
	gp.structure = NULL;
	gp.accumulatedSize = 0;

	if (!node.empty()) {
		gp.namePrefix = node.attribute("namePrefix").value();
		if (0 == strcmp(gp.namePrefix, "")) {
			gp.namePrefix = "GARBAGE";
		}

		const char* percentageStr = node.attribute("percentage").value();
		if (0 == strcmp(percentageStr, "")) {
			percentageStr = "100";
		}
		gp.percentage = (float)atof(percentageStr);

		gp.frequency = node.attribute("frequency").value();
		if (0 == strcmp(gp.frequency, "")) {
			gp.frequency = "perRootStruct";
		}
		if ((0 != strcmp(gp.frequency, "perObject")) && (0 != strcmp(gp.frequency, "perRootStruct"))){
			rt = 1;
			omrtty_printf("%s:%d Invalid XML input: the valid value of attribute \"frequency\" is \"perObject\" or \"perRootStruct\".\n", __FILE__, __LINE__);
			goto done;
		}

		gp.structure = node.attribute("structure").value();
		if (0 == strcmp(gp.structure, "")) {
			gp.structure = "node";
		}
		if ((0 != strcmp(gp.structure, "node")) && (0 != strcmp(gp.structure, "tree"))){
			rt = 1;
			omrtty_printf("%s:%d Invalid XML input: the valid value of attribute \"structure\" is \"node\" or \"tree\".\n", __FILE__, __LINE__);
			goto done;
		}
	}

done:
	return rt;
}

int32_t
GCConfigTest::triggerOperation(pugi::xml_node node)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	for (; node; node = node.next_sibling()) {
		if (0 == strcmp(node.name(), "systemCollect")) {
			const char *gcCodeStr = node.attribute("gcCode").value();
			if (0 == strcmp(gcCodeStr, "")) {
				/* gcCode defaults to J9MMCONSTANT_IMPLICIT_GC_DEFAULT */
				gcCodeStr = "0";
			}
			uint32_t gcCode = (uint32_t)atoi(gcCodeStr);
			omrtty_printf("Invoking gc system collect with gcCode %d...\n", gcCode);
			rt = (int32_t)OMR_GC_SystemCollect(exampleVM->_omrVMThread, gcCode);
			if (OMR_ERROR_NONE != rt) {
				omrtty_printf("%s:%d Failed to perform OMR_GC_SystemCollect with error code %d.\n", __FILE__, __LINE__, rt);
				goto done;
			}
			OMRGCTEST_CHECK_RT(rt);
			verboseManager->getWriterChain()->endOfCycle(env);
		}
	}
done:
	return rt;
}

TEST_P(GCConfigTest, test)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	int32_t rt = 0;

	pugi::xml_node configNode = doc.select_node("/gc-config").node().first_child();
	if (0 == strcmp(configNode.name(), "option")) {
		configNode = configNode.next_sibling();
	}
	for (; configNode; configNode = configNode.next_sibling()) {
		if (0 == strcmp(configNode.name(), "allocation")) {
			omrtty_printf("\n+++++++++++++++++++++++++++Allocation+++++++++++++++++++++++++++\n");
			rt = parseGarbagePolicy(configNode.child("garbagePolicy"));
			ASSERT_EQ(0, rt) << "Failed to parse garbage policy.";
			pugi::xpath_node_set objects = configNode.select_nodes("object");
			int64_t startTime = omrtime_current_time_millis();
			for (pugi::xpath_node_set::const_iterator it = objects.begin(); it != objects.end(); ++it) {
				rt = allocationWalker(it->node());
				ASSERT_EQ(0, rt) << "Failed to perform allocation.";
			}
			omrtty_printf("Time elapsed in allocation: %lld ms\n", (omrtime_current_time_millis() - startTime));
		} else if (0 == strcmp(configNode.name(), "verification")) {
			omrtty_printf("\n++++++++++++++++++++++++++Verification++++++++++++++++++++++++++\n");
			/* verboseGC verification */
			pugi::xpath_node_set verboseGCs = configNode.select_nodes("verboseGC");
			rt = verifyVerboseGC(verboseGCs);
			ASSERT_EQ(0, rt) << "Failed in verbose GC verification.";
			omrtty_printf("[ Verification Successful ]\n\n");
		} else if (0 == strcmp(configNode.name(), "operation")) {
			omrtty_printf("\n++++++++++++++++++++++++++++Operation+++++++++++++++++++++++++++\n");
			rt = triggerOperation(configNode.first_child());
			ASSERT_EQ(0, rt) << "Failed to perform gc operation.";
		} else if (0 == strcmp(configNode.name(), "mutation")) {
			/*TODO*/
		} else {
			FAIL() << "Invalid XML input: unrecognized XML node \"" << configNode.name() << "\" in configuration file.";
		}
	}
}

INSTANTIATE_TEST_CASE_P(configFile, GCConfigTest,
	::testing::ValuesIn(gcTestEnv->params));

