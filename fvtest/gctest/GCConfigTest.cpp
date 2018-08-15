/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#include "GCConfigTest.hpp"
#include "ObjectAllocationModel.hpp"
#include "ObjectModel.hpp"
#include "omrExampleVM.hpp"
#include "omrgc.h"
#include "SlotObject.hpp"
#include "StandardWriteBarrier.hpp"
#include "VerboseWriterChain.hpp"

//#define OMRGCTEST_PRINTFILE

#define MAX_NAME_LENGTH 512
#define OMRGCTEST_CHECK_RT(rt) \
	if (0 != (rt)) {\
		goto done;\
	}
#define STRINGFY(str) DO_STRINGFY(str)
#define DO_STRINGFY(str) #str

const char *gcTests[] = {
		"fvtest/gctest/configuration/sample_GC_config.xml",
		"fvtest/gctest/configuration/test_system_gc.xml",
		"fvtest/gctest/configuration/gencon_GC_config.xml",
		"fvtest/gctest/configuration/gencon_GC_backout_config.xml",
		"fvtest/gctest/configuration/evacuator_small_GC_config.xml",
		"fvtest/gctest/configuration/evacuator_large_GC_config.xml",
		"fvtest/gctest/configuration/scavenger_GC_config.xml",
		"fvtest/gctest/configuration/scavenger_GC_backout_config.xml",
		"fvtest/gctest/configuration/global_GC_config.xml",
		"fvtest/gctest/configuration/optavgpause_GC_config.xml"
};

const char *perfTests[] = {
		"perftest/gctest/configuration/21645_core.20150126.202455.11862202.0001.xml",
		"perftest/gctest/configuration/24404_core.20140723.091737.5812.0002.xml"
};

void
GCConfigTest::SetUp()
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	printMemUsed("Setup()", gcTestEnv->portLib);

	gcTestEnv->log("Configuration File: %s\n", GetParam());
	MM_StartupManagerTestExample startupManager(exampleVM->_omrVM, GetParam());

	/* Initialize heap and collector */
	omr_error_t rc = OMR_GC_IntializeHeapAndCollector(exampleVM->_omrVM, &startupManager);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_GC_IntializeHeapAndCollector failed, rc=" << rc;

	/* Attach calling thread to the VM */
	rc = OMR_Thread_Init(exampleVM->_omrVM, NULL, &exampleVM->_omrVMThread, "OMRTestThread");
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_Thread_Init failed, rc=" << rc;

	/* Kick off the dispatcher threads */
	rc = OMR_GC_InitializeDispatcherThreads(exampleVM->_omrVMThread);
	ASSERT_EQ(OMR_ERROR_NONE, rc) << "Setup(): OMR_GC_InitializeDispatcherThreads failed, rc=" << rc;

	/* Instantiate collector interface */
	env = MM_EnvironmentBase::getEnvironment(exampleVM->_omrVMThread);
	cli = startupManager.createCollectorLanguageInterface(env);
	if (NULL == cli) {
		FAIL() << "Failed to instantiate collector interface.";
	}

	/* load config file */
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
	gcTestEnv->log("Verbose File: %s\n", verboseFile);
	gcTestEnv->log(LEVEL_VERBOSE, "Verbose GC log name: %s; numOfFiles: %d; numOfCycles: %d.\n", verboseFile, numOfFiles, numOfCycles);
	verboseManager->enableVerboseGC();
	verboseManager->setInitializedTime(omrtime_hires_clock());

	/* Initialize root table */
	exampleVM->rootTable = hashTableNew(
			exampleVM->_omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(RootEntry), 0, 0, OMRMEM_CATEGORY_MM,
			rootTableHashFn, rootTableHashEqualFn, NULL, NULL);

	/* Initialize object table */
	exampleVM->objectTable = hashTableNew(
			exampleVM->_omrVM->_runtime->_portLibrary, OMR_GET_CALLSITE(), 0, sizeof(ObjectEntry), 0, 0, OMRMEM_CATEGORY_MM,
			objectTableHashFn, objectTableHashEqualFn, NULL, NULL);
}

void
GCConfigTest::TearDown()
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	/* Free root hash table */
	if (NULL != exampleVM->rootTable) {
		hashTableFree(exampleVM->rootTable);
		exampleVM->rootTable = NULL;
	}

	/* Free object hash table */
	if (NULL != exampleVM->objectTable) {
		hashTableForEachDo(exampleVM->objectTable, objectTableFreeFn, exampleVM);
		hashTableFree(exampleVM->objectTable);
		exampleVM->objectTable = NULL;
	}

	/* close verboseManager and clean up verbose files */
	if (NULL != verboseManager) {
		verboseManager->closeStreams(env);
		verboseManager->disableVerboseGC();
		verboseManager->kill(env);
		verboseManager = NULL;
	}
	if ((NULL != verboseFile) && (false == gcTestEnv->keepLog)) {
		if (0 == numOfFiles) {
			J9FileStat buf;
			int32_t fileStatRC = -1;
			fileStatRC = omrfile_stat(verboseFile, 0, &buf);
			if (0 == fileStatRC) {
				if (1 == buf.isFile) {
					omrfile_unlink(verboseFile);
				}
			}
		} else {
			for (int32_t seq = 1; seq <= (int32_t)numOfFiles; seq++) {
				char verboseFileSeq[MAX_NAME_LENGTH];
				omrstr_printf(verboseFileSeq, MAX_NAME_LENGTH, "%s.%03zu", verboseFile, seq);
				J9FileStat buf;
				int32_t fileStatRC = -1;
				fileStatRC = omrfile_stat(verboseFileSeq, 0, &buf);
				if (0 > fileStatRC) {
					if (1 != buf.isFile) {
						break;
					}
				}
				omrfile_unlink(verboseFileSeq);
			}
		}
	}
	omrmem_free_memory((void *)verboseFile);
	verboseFile = NULL;

	cli->kill(env);

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

	exampleVM->_omrVMThread = NULL;

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
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
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
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
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
	const char *namePrefixStr = node.attribute(xs.namePrefix).value();
	const char *typeStr = node.attribute(xs.type).value();
	const char *parentStr = node.parent().name();
	const char *parentTypeStr = node.parent().attribute(xs.type).value();
	OMRGCObjectType objType = INVALID;

	if (0 == strcmp(typeStr, "root")) {
		if (0 == strcmp(parentStr, "allocation")) {
			objType = ROOT;
		} else {
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: root object %s can not be child of other object!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else if (0 == strcmp(typeStr, "normal")) {
		if ((0 == strcmp(parentTypeStr, "root")) || (0 == strcmp(parentTypeStr, "normal"))) {
			objType = NORMAL;
		} else {
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: normal object %s has to be child of root or normal object!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else if (0 == strcmp(typeStr, "garbage")) {
		if (0 == strcmp(parentStr, "allocation")) {
			objType = GARBAGE_ROOT;
		} else if ((0 == strcmp(parentTypeStr, "normal")) || (0 == strcmp(parentTypeStr, "root"))){
		objType = GARBAGE_TOP;
		} else if (0 == strcmp(parentTypeStr, "garbage")) {
			objType = GARBAGE_CHILD;
		} else {
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: garbage object %s has to be child of root, normal or allocation node!\n", __FILE__, __LINE__, namePrefixStr);
		}
	} else {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: object %s type should be root, normal or garbage .\n", __FILE__, __LINE__, namePrefixStr);
	}

	return objType;
}

ObjectEntry *
GCConfigTest::allocateHelper(const char *objName, uintptr_t size)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(exampleVM->_omrVMThread);

	ObjectEntry objEntry;
	objEntry.numOfRef = 0;
	objEntry.name = objName;
	objEntry.objPtr = NULL;

	uint8_t objectAllocationModelSpace[sizeof(MM_ObjectAllocationModel)];
	MM_ObjectAllocationModel *noGc = new(objectAllocationModelSpace)
			MM_ObjectAllocationModel(env, size, MM_ObjectAllocationModel::selectObjectAllocationFlags(false, false, false, true));
	objEntry.objPtr = OMR_GC_AllocateObject(exampleVM->_omrVMThread, noGc);

	if (NULL == objEntry.objPtr) {
		gcTestEnv->log("No free memory to allocate %s of size 0x%llx, GC start.\n", objName, size);
		MM_ObjectAllocationModel *withGc = new(objectAllocationModelSpace)
				MM_ObjectAllocationModel(env, size, MM_ObjectAllocationModel::selectObjectAllocationFlags(false, false, false, false));
		objEntry.objPtr = OMR_GC_AllocateObject(exampleVM->_omrVMThread, withGc);
	}

	ObjectEntry *newEntry = NULL;
	if (NULL != objEntry.objPtr) {
		uintptr_t consumedSize = env->getExtensions()->objectModel.getConsumedSizeInBytesWithHeader(objEntry.objPtr);
		uintptr_t adjustedSize = env->getExtensions()->objectModel.adjustSizeInBytes(size);
		if (consumedSize == adjustedSize) {
			gcTestEnv->log(LEVEL_VERBOSE, "Allocate object name: %s(%p[0x%llx])\n", objEntry.name, objEntry.objPtr, consumedSize);
		} else {
			gcTestEnv->log(LEVEL_ERROR, "Consumed size for allocated object name: %s(%p[0x%llx]) != adjusted request size [0x%llx].\n", objEntry.name, objEntry.objPtr, consumedSize, adjustedSize);
		}
		newEntry = add(&objEntry);
	} else {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d No free memory after a GC. Failed to allocate object %s of size 0x%llx.\n", __FILE__, __LINE__, objName, size);
	}

	return newEntry;
}

ObjectEntry *
GCConfigTest::createObject(const char *namePrefix, OMRGCObjectType objType, int32_t depth, int32_t nthInRow, uintptr_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	ObjectEntry *objEntry = NULL;

	char *objName = (char *)omrmem_allocate_memory(MAX_NAME_LENGTH, OMRMEM_CATEGORY_MM);
	if (NULL == objName) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
		goto done;
	}
	omrstr_printf(objName, MAX_NAME_LENGTH, "%s_%d_%d", namePrefix, depth, nthInRow);

	objEntry = find(objName);
	if (NULL != objEntry) {
		gcTestEnv->log(LEVEL_VERBOSE, "Found object %s in object table.\n", objEntry->name);
		omrmem_free_memory(objName);
	} else {
		objEntry = allocateHelper(objName, size);
		if (NULL != objEntry) {
			/* Keep count of the new allocated non-garbage object size for garbage insertion. If the object exists in objectTable, its size is ignored. */
			if ((ROOT == objType) || (NORMAL == objType)) {
				gp.accumulatedSize += env->getExtensions()->objectModel.getSizeInBytesWithHeader(objEntry->objPtr);
			}
		} else {
			omrmem_free_memory(objName);
		}
	}

done:
	return objEntry;
}

int32_t
GCConfigTest::createFixedSizeTree(ObjectEntry **rootEntryIndirectPtr, const char *namePrefixStr, OMRGCObjectType objType, uintptr_t totalSize, uintptr_t objSize, int32_t breadth)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	*rootEntryIndirectPtr = NULL;

	int32_t rt = 0;
	uintptr_t bytesLeft = totalSize;
	if (bytesLeft < objSize) {
		objSize = bytesLeft;
		if (objSize <= 0) {
			return rt;
		}
	}
	bytesLeft -= objSize;
	rt = 1;

	int32_t depth = 1;
	int32_t numOfParents = 1;

	/* allocate the root object and add it to the object table */
	*rootEntryIndirectPtr = createObject(namePrefixStr, objType, 0, 0, objSize);
	if (NULL == *rootEntryIndirectPtr) {
		goto done;
	}

	/* add it to the root table */
	RootEntry rootEntry;
	rootEntry.name = (*rootEntryIndirectPtr)->name;
	rootEntry.rootPtr = (*rootEntryIndirectPtr)->objPtr;
	if (NULL == hashTableAdd(exampleVM->rootTable, &rootEntry)) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to add new root entry %s to root table!\n", __FILE__, __LINE__, rootEntry.name);
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
			ObjectEntry *parentEntry = find(parentName);
			if (NULL == parentEntry) {
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
				goto done;
			}
			for (int32_t k = 0; k < breadth; k++) {
				if (bytesLeft < objSize) {
					objSize = bytesLeft;
					if (objSize <= 0) {
						rt = 0;
						goto done;
					}
				}
				ObjectEntry *childEntry = createObject(namePrefixStr, objType, depth, nthInRow, objSize);
				if (NULL == childEntry) {
					goto done;
				}
				parentEntry = find(parentName);
				if (NULL == parentEntry) {
					gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
					goto done;
				}
				if (0 != attachChildEntry(parentEntry, childEntry)) {
					goto done;
				}
				bytesLeft -= objSize;
				nthInRow += 1;
			}
			parentEntry = find(parentName);
			if (NULL == parentEntry) {
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
				goto done;
			}
		}
		numOfParents = nthInRow;
		depth += 1;
	}
	*rootEntryIndirectPtr = find(rootEntry.name);
	if (NULL == *rootEntryIndirectPtr) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find root of fixed-size tree %s in object table.\n", __FILE__, __LINE__, rootEntry.name);
		goto done;
	}
	rt = 0;
done:
	return rt;
}

int32_t
GCConfigTest::processObjNode(pugi::xml_node node, const char *namePrefixStr, OMRGCObjectType objType, AttributeElem *numOfFieldsElem, AttributeElem *breadthElem, int32_t depth)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	int32_t rt = 1;
	int32_t numOfParents = 0;

	/* Create objects and store them in the root table or the slot object based on objType. */
	if ((ROOT == objType) || (GARBAGE_ROOT == objType)) {
		for (int32_t i = 0; i < breadthElem->value; i++) {
			uintptr_t sizeCalculated = numOfFieldsElem->value * sizeof(fomrobject_t) + sizeof(uintptr_t);
			ObjectEntry *objectEntry = createObject(namePrefixStr, objType, 0, i, sizeCalculated);
			if (NULL == objectEntry) {
				goto done;
			}
			numOfFieldsElem = numOfFieldsElem->linkNext;

			/* Add object to root hash table. If it's the root of the garbage tree, temporarily keep it as root for allocating
			 * the rest of the tree. Remove it from the root set after the entire garbage tree is allocated.*/
			RootEntry rEntry;
			rEntry.name = objectEntry->name;
			rEntry.rootPtr = objectEntry->objPtr;
			if (NULL == hashTableAdd(exampleVM->rootTable, &rEntry)) {
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to add new root entry to root table!\n", __FILE__, __LINE__);
				goto done;
			}

			/* insert garbage per node */
			if ((ROOT == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
				int32_t grt = insertGarbage();
				OMRGCTEST_CHECK_RT(grt);
			}
		}
	} else if ((NORMAL == objType) || (GARBAGE_TOP == objType) || (GARBAGE_CHILD == objType)) {
		char parentName[MAX_NAME_LENGTH];
		omrstr_printf(parentName, MAX_NAME_LENGTH, "%s_%d_%d", node.parent().attribute(xs.namePrefix).value(), 0, 0);
		for (int32_t i = 0; i < breadthElem->value; i++) {
			uintptr_t sizeCalculated = numOfFieldsElem->value * sizeof(fomrobject_t) + sizeof(uintptr_t);
			ObjectEntry *childEntry = createObject(namePrefixStr, objType, 0, i, sizeCalculated);
			if (NULL == childEntry) {
				goto done;
			}
			ObjectEntry *parentEntry = find(parentName);
			if (NULL == parentEntry) {
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
				goto done;
			}
			if (0 != attachChildEntry(parentEntry, childEntry)) {
				goto done;
			}
			numOfFieldsElem = numOfFieldsElem->linkNext;

			/* insert garbage per node */
			if ((NORMAL == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
				int32_t grt = insertGarbage();
				OMRGCTEST_CHECK_RT(grt);
			}
		}
	} else {
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
			for (int32_t k = 0; k < breadthElem->value; k++) {
				uintptr_t sizeCalculated = sizeof(omrobjectptr_t) + numOfFieldsElem->value * sizeof(fomrobject_t);
				ObjectEntry *childEntry = createObject(namePrefixStr, objType, i, nthInRow, sizeCalculated);
				if (NULL == childEntry) {
					goto done;
				}
				ObjectEntry *parentEntry = find(parentName);
				if (NULL == parentEntry) {
					gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
					goto done;
				}
				if (0 != attachChildEntry(parentEntry, childEntry)) {
					goto done;
				}
				numOfFieldsElem = numOfFieldsElem->linkNext;
				nthInRow += 1;

				/* insert garbage per node */
				if ((NORMAL == objType) && (0 == strcmp(gp.frequency, "perObject"))) {
					int32_t grt = insertGarbage();
					OMRGCTEST_CHECK_RT(grt);
				}
			}
		}
		numOfParents = nthInRow;
		breadthElem = breadthElem->linkNext;
	}
	rt = 0;
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

	gcTestEnv->log(LEVEL_VERBOSE, "Inserting garbage %s of 0x%llx (%f%% of previous object/tree size 0x%llx) ...\n", fullNamePrefix, totalSize, gp.percentage, gp.accumulatedSize);

	ObjectEntry *rootObjectEntry = NULL;
	rt = createFixedSizeTree(&rootObjectEntry, fullNamePrefix, GARBAGE_ROOT, totalSize, objSize, breadth);
	OMRGCTEST_CHECK_RT(rt);

	/* remove garbage root object from the root set */
	if (NULL != rootObjectEntry) {
		rt = removeObjectFromRootTable(rootObjectEntry->name);
		OMRGCTEST_CHECK_RT(rt);
	}

	/* reset accumulatedSize */
	gp.accumulatedSize = 0;

done:
	return rt;
}

int32_t
GCConfigTest::attachChildEntry(ObjectEntry *parentEntry, ObjectEntry *childEntry)
{
	int32_t rc = 0;
	MM_GCExtensionsBase *extensions = (MM_GCExtensionsBase *)exampleVM->_omrVM->_gcOmrVMExtensions;
	uintptr_t size = extensions->objectModel.getConsumedSizeInBytesWithHeader(parentEntry->objPtr);
	fomrobject_t *firstSlot = (fomrobject_t *)parentEntry->objPtr + 1;
	fomrobject_t *endSlot = (fomrobject_t *)((uint8_t *)parentEntry->objPtr + size);
	uintptr_t slotCount = endSlot - firstSlot;

	if ((uint32_t)parentEntry->numOfRef < slotCount) {
		fomrobject_t *childSlot = firstSlot + parentEntry->numOfRef;
		standardWriteBarrierStore(exampleVM->_omrVMThread, parentEntry->objPtr, childSlot, childEntry->objPtr);
		gcTestEnv->log(LEVEL_VERBOSE, "\tadd child %s(%p[0x%llx]) to parent %s(%p[0x%llx]) slot %p[%llx].\n", 
		               childEntry->name, childEntry->objPtr, childEntry->objPtr->header.raw(), parentEntry->name, parentEntry->objPtr, parentEntry->objPtr->header.raw(), childSlot, (uintptr_t)*childSlot);
		parentEntry->numOfRef += 1;
	} else {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: numOfFields %d defined for %s(%p[0x%llx]) is not enough to hold child reference for %s(%p[0x%llx]).\n",
				__FILE__, __LINE__, parentEntry->numOfRef, parentEntry->name, parentEntry->objPtr, parentEntry->objPtr->header.raw(), childEntry->name, childEntry->objPtr, childEntry->objPtr->header.raw());
		rc = 1;
	}
	return rc;
}

int32_t
GCConfigTest::removeObjectFromRootTable(const char *name)
{
	int32_t rt = 1;
	RootEntry searchEntry;
	searchEntry.name = name;
	RootEntry *rootEntry = (RootEntry *)hashTableFind(exampleVM->rootTable, &searchEntry);
	if (NULL == rootEntry) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to find root object %s in root table.\n", __FILE__, __LINE__, name);
		goto done;
	}
	if (0 != hashTableRemove(exampleVM->rootTable, rootEntry)) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to remove root object %s from root table!\n", __FILE__, __LINE__, name);
		goto done;
	}
	gcTestEnv->log(LEVEL_VERBOSE, "Remove object %s(%p[0x%llx]) from root table.\n", name, rootEntry->rootPtr, rootEntry->rootPtr->header.raw());

	rt = 0;
done:
	return rt;
}

int32_t
GCConfigTest::removeObjectFromObjectTable(const char *name)
{
	int32_t rt = 1;
	ObjectEntry *foundEntry = find(name);
	if (NULL == foundEntry) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to find object %s in object table.\n", __FILE__, __LINE__, name);
		goto done;
	}
	if (0 != hashTableRemove(exampleVM->objectTable, foundEntry)) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to remove object %s from object table!\n", __FILE__, __LINE__, name);
		goto done;
	}
	gcTestEnv->log(LEVEL_VERBOSE, "Remove object %s(%p[0x%llx]) from object table.\n", name, foundEntry->objPtr, foundEntry->objPtr->header.raw());

	rt = 0;
done:
	return rt;
}

int32_t
GCConfigTest::removeObjectFromParentSlot(const char *name, ObjectEntry *parentEntry)
{
	MM_GCExtensionsBase *extensions = (MM_GCExtensionsBase *)exampleVM->_omrVM->_gcOmrVMExtensions;
	uintptr_t size = extensions->objectModel.getConsumedSizeInBytesWithHeader(parentEntry->objPtr);
	fomrobject_t *currentSlot = (fomrobject_t *)parentEntry->objPtr + 1;
	fomrobject_t *endSlot = (fomrobject_t *)((uint8_t *)parentEntry->objPtr + size);

	int32_t rt = 1;
	ObjectEntry *objEntry = find(name);
	if (NULL == objEntry) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, name);
		goto done;
	}

	while (currentSlot < endSlot) {
		GC_SlotObject slotObject(exampleVM->_omrVM, currentSlot);
		if (objEntry->objPtr == slotObject.readReferenceFromSlot()) {
			gcTestEnv->log(LEVEL_VERBOSE, "Remove object %s(%p[0x%llx]) from parent %s(%p[0x%llx]) slot %p.\n", name, objEntry->objPtr, objEntry->objPtr->header.raw(), parentEntry->name, parentEntry->objPtr, parentEntry->objPtr->header.raw(), slotObject.readAddressFromSlot());
			slotObject.writeReferenceToSlot(NULL);
			rt = 0;
			break;
		}
		currentSlot += 1;
	}
	if (0 != rt) {
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to remove object %s from its parent's slot.\n", __FILE__, __LINE__, name);
		goto done;
	}

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

	const char *namePrefixStr = node.attribute(xs.namePrefix).value();
	const char *numOfFieldsStr = node.attribute(xs.numOfFields).value();
	const char *typeStr = node.attribute(xs.type).value();
	const char *breadthStr = node.attribute(xs.breadth).value();
	const char *depthStr = node.attribute(xs.depth).value();

	if (0 != strcmp(node.name(), xs.object)) {
		/* allow non-object node nested inside allocation? */
		goto done;
	}
	if ((0 == strcmp(namePrefixStr, "")) || (0 == strcmp(typeStr, ""))) {
		rt = 1;
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: please specify namePrefix and type for object %s.\n", __FILE__, __LINE__, namePrefixStr);
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
		omrstr_printf(parentName, MAX_NAME_LENGTH, "%s_%d_%d", node.parent().attribute(xs.namePrefix).value(), 0, 0);
		ObjectEntry *parentEntry = find(parentName);
		if (NULL == parentEntry) {
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Could not find object %s in hash table.\n", __FILE__, __LINE__, parentName);
			goto done;
		}
		for (int32_t i = 0; i < breadthElem->value; i++) {
			char objName[MAX_NAME_LENGTH];
			omrstr_printf(objName, MAX_NAME_LENGTH, "%s_%d_%d", namePrefixStr, 0, i);
			rt = removeObjectFromParentSlot(objName, parentEntry);
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
		gcTestEnv->log("%s", readBuf);
	}
	omrfile_close(fileDescriptor);
	gcTestEnv->log("\n");
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
		gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to allocate native memory.\n", __FILE__, __LINE__);
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
			gcTestEnv->log("Parsing verbose log %s:\n", verboseFile);
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
			gcTestEnv->log("Parsing verbose log %s:\n", currentVerboseFile);
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
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid xquery string \"%s\" specified in configuration file.\n", __FILE__, __LINE__, xqueryStr);
				goto done;
			}

			pugi::xpath_query nodeQuery(xpathNodesStr);
			if (!nodeQuery) {
				rt = 1;
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid xpathNodes string \"%s\" specified in configuration file.\n", __FILE__, __LINE__, xpathNodesStr);
				goto done;
			}

			pugi::xpath_node_set xpathNodes = nodeQuery.evaluate_node_set(verboseDoc);
			if (!xpathNodes.empty()) {
				bool isPassed = true;
				gcTestEnv->log("Verifying xquery \"%s\" on xpath \"%s\":\n", xqueryStr, xpathNodesStr);
				for (pugi::xpath_node_set::const_iterator it = xpathNodes.begin(); it != xpathNodes.end(); ++it) {
					pugi::xml_node node = it->node();
					if (!resultQuery.evaluate_boolean(node)) {
						rt = 1;
						isPassed = false;
						gcTestEnv->log(LEVEL_ERROR, "\t*FAILED* on node <%s", node.name());
						for (pugi::xml_attribute attr = node.first_attribute(); attr; attr = attr.next_attribute()) {
							gcTestEnv->log(LEVEL_ERROR, "\t%s=%s", attr.name(), attr.value());
						}
						gcTestEnv->log(LEVEL_ERROR, ">\n");
					}
				}
				if (isPassed) {
					gcTestEnv->log("*PASSED*\n");
				}
				isFound[i] = true;
			}
			i += 1;
		}
		gcTestEnv->log("\n");
	} while (seq <= numOfFiles);

	for (size_t i = 0; i < numOfNode; i++) {
		if (!isFound[i]) {
			rt = 1;
			gcTestEnv->log(LEVEL_ERROR, "*FAILED* Could not find xpath node \"%s\" in verbose output.\n", verboseGCs[i].node().attribute("xpathNodes").value());
		}
	}

done:
	omrmem_free_memory((void *)isFound);
	return rt;
}

int32_t
GCConfigTest::parseGarbagePolicy(pugi::xml_node node)
{
	int32_t rt = 0;
	gp.namePrefix = NULL;
	gp.percentage = 0.0f;
	gp.frequency = "none";
	gp.structure = NULL;
	gp.accumulatedSize = 0;

	if (!node.empty()) {
		gp.namePrefix = node.attribute(xs.namePrefix).value();
		if (0 == strcmp(gp.namePrefix, "")) {
			gp.namePrefix = "GARBAGE";
		}

		const char* percentageStr = node.attribute(xs.percentage).value();
		if (0 == strcmp(percentageStr, "")) {
			percentageStr = "100";
		}
		gp.percentage = (float)atof(percentageStr);

		gp.frequency = node.attribute(xs.frequency).value();
		if (0 == strcmp(gp.frequency, "")) {
			gp.frequency = "perRootStruct";
		}
		if ((0 != strcmp(gp.frequency, "perObject")) && (0 != strcmp(gp.frequency, "perRootStruct"))){
			rt = 1;
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: the valid value of attribute \"frequency\" is \"perObject\" or \"perRootStruct\".\n", __FILE__, __LINE__);
			goto done;
		}

		gp.structure = node.attribute(xs.structure).value();
		if (0 == strcmp(gp.structure, "")) {
			gp.structure = "node";
		}
		if ((0 != strcmp(gp.structure, "node")) && (0 != strcmp(gp.structure, "tree"))){
			rt = 1;
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Invalid XML input: the valid value of attribute \"structure\" is \"node\" or \"tree\".\n", __FILE__, __LINE__);
			goto done;
		}
	}

done:
	return rt;
}

int32_t
GCConfigTest::triggerOperation(pugi::xml_node node)
{
	int32_t rt = 0;
	for (; node; node = node.next_sibling()) {
		if (0 == strcmp(node.name(), "systemCollect")) {
			const char *gcCodeStr = node.attribute("gcCode").value();
			if (0 == strcmp(gcCodeStr, "")) {
				/* gcCode defaults to J9MMCONSTANT_IMPLICIT_GC_DEFAULT */
				gcCodeStr = "0";
			}
			uint32_t gcCode = (uint32_t)atoi(gcCodeStr);
			gcTestEnv->log("Invoking gc system collect with gcCode %d...\n", gcCode);
			rt = (int32_t)OMR_GC_SystemCollect(exampleVM->_omrVMThread, gcCode);
			if (OMR_ERROR_NONE != rt) {
				gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to perform OMR_GC_SystemCollect with error code %d.\n", __FILE__, __LINE__, rt);
				goto done;
			}
			OMRGCTEST_CHECK_RT(rt);
			verboseManager->getWriterChain()->endOfCycle(env);
		}
	}
done:
	return rt;
}

int32_t
GCConfigTest::iniXMLStr(const char *configStyle)
{
	int32_t rt = 0;
	if ((0 == strcmp(configStyle, "nor")) || (0 == strcmp(configStyle, ""))) {
		xs.object = "object";
		xs.namePrefix = "namePrefix";
		xs.type = "type";
		xs.numOfFields = "numOfFields";
		xs.breadth = "breadth";
		xs.depth = "depth";
		xs.garbagePolicy = "garbagePolicy";
		xs.percentage = "percentage";
		xs.frequency = "frequency";
		xs.structure = "structure";
	} else if (0 == strcmp(configStyle, "min")) {
		xs.object = "o";
		xs.namePrefix = "n";
		xs.type = "t";
		xs.numOfFields = "f";
		xs.breadth = "b";
		xs.depth = "d";
		xs.garbagePolicy = "gp";
		xs.percentage = "per";
		xs.frequency = "fre";
		xs.structure = "str";
	} else {
		rt = 1;
	}
	return rt;
}

TEST_P(GCConfigTest, test)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);

	int32_t rt = 0;

	pugi::xml_node configNode = doc.select_node("/gc-config").node();
	const char *configStyle = configNode.attribute("style").value();
	ASSERT_EQ(0, iniXMLStr(configStyle)) << "Invalid XML input: unrecognized gc-config style \"" << configStyle << "\".";

	pugi::xml_node configChild = configNode.first_child();
	if (0 == strcmp(configChild.name(), "option")) {
		configChild = configChild.next_sibling();
	}
	for (; configChild; configChild = configChild.next_sibling()) {
		if (0 == strcmp(configChild.name(), "allocation")) {
			gcTestEnv->log("\n+++++++++++++++++++++++++++Allocation+++++++++++++++++++++++++++\n");
			rt = parseGarbagePolicy(configChild.child(xs.garbagePolicy));
			ASSERT_EQ(0, rt) << "Failed to parse garbage policy.";
			pugi::xpath_node_set objects = configChild.select_nodes(xs.object);
			int64_t startTime = omrtime_current_time_millis();
			for (pugi::xpath_node_set::const_iterator it = objects.begin(); it != objects.end(); ++it) {
				rt = allocationWalker(it->node());
				ASSERT_EQ(0, rt) << "Failed to perform allocation.";
			}
			gcTestEnv->log("Time elapsed in allocation: %lld ms\n", (omrtime_current_time_millis() - startTime));
		} else if (0 == strcmp(configChild.name(), "verification")) {
			gcTestEnv->log("\n++++++++++++++++++++++++++Verification++++++++++++++++++++++++++\n");
			/* verboseGC verification */
			char verboseNodeSet[MAX_NAME_LENGTH];
			/* select verboseGC nodes with right spec info */
			omrstr_printf(verboseNodeSet, MAX_NAME_LENGTH, "verboseGC[not(@spec) or @spec = '%s']", STRINGFY(SPEC));
			pugi::xpath_node_set verboseGCs = configChild.select_nodes(verboseNodeSet);
			rt = verifyVerboseGC(verboseGCs);
			ASSERT_EQ(0, rt) << "Failed in verbose GC verification.";
			gcTestEnv->log("[ Verification Successful ]\n\n");
		} else if (0 == strcmp(configChild.name(), "operation")) {
			gcTestEnv->log("\n++++++++++++++++++++++++++++Operation+++++++++++++++++++++++++++\n");
			rt = triggerOperation(configChild.first_child());
			ASSERT_EQ(0, rt) << "Failed to perform gc operation.";
		} else if (0 == strcmp(configChild.name(), "mutation")) {
			/*TODO*/
		} else {
			FAIL() << "Invalid XML input: unrecognized XML node \"" << configChild.name() << "\" in configuration file.";
		}
	}
}

INSTANTIATE_TEST_CASE_P(gcFunctionalTest,GCConfigTest,
        ::testing::ValuesIn(gcTests));

INSTANTIATE_TEST_CASE_P(perfTest,GCConfigTest,
        ::testing::ValuesIn(perfTests));
