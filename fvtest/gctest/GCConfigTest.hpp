/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include "AllocateDescription.hpp"
#include "GCConfigObjectTable.hpp"
#include "GCExtensionsBase.hpp"
#include "gcTestHelpers.hpp"
#include "GlobalCollector.hpp"
#include "omrlinkedlist.h"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "pugixml.hpp"
#include "StartupManagerTestExample.hpp"
#include "VerboseManager.hpp"

enum OMRGCObjectType {
	INVALID = 0,
	ROOT,
	NORMAL,
	GARBAGE_CHILD,
	GARBAGE_TOP,
	GARBAGE_ROOT
};

typedef struct AttributeElem {
	int32_t value;
	struct AttributeElem *linkNext;
	struct AttributeElem *linkPrevious;
} AttributeElem;

typedef struct GarbagePolicy {
	const char *namePrefix;
	float percentage;
	const char *frequency;
	const char *structure;
	int32_t garbageSeq;
	uintptr_t accumulatedSize;
} GarbagePolicy;

class GCConfigTest : public ::testing::Test, public ::testing::WithParamInterface<const char *>
{
	/*
	 * Data members
	 */
protected:
	OMR_VM_Example *exampleVM;
	MM_EnvironmentBase *env;
	pugi::xml_document doc;
	GCConfigObjectTable objectTable;
	GarbagePolicy gp;

	/* verbose log options */
	MM_VerboseManager *verboseManager;
	char *verboseFile;
	uintptr_t numOfFiles;

	/*
	 * Function members
	 */
protected:
	void freeAttributeList(AttributeElem *root);
	int32_t parseAttribute(AttributeElem **root, const char *attrStr);
	OMRGCObjectType parseObjectType(pugi::xml_node node);
	int32_t allocateHelper(omrobjectptr_t *obj, char *objName, uintptr_t size);
	int32_t createObject(omrobjectptr_t *obj, char **objName, const char *namePrefix, OMRGCObjectType objType, int32_t depth, int32_t nthInRow, uintptr_t size);
	int32_t createFixedSizeTree(omrobjectptr_t *obj, const char *namePrefixStr, OMRGCObjectType objType, uintptr_t totalSize, uintptr_t objSize, int32_t breadth);
	int32_t processObjNode(pugi::xml_node node, const char *namePrefixStr, OMRGCObjectType objType, AttributeElem *numOfFieldsElem, AttributeElem *breadthElem, int32_t depth);
	int32_t insertGarbage();
	int32_t removeObjectFromRootTable(const char *name);
	int32_t removeObjectFromParentSlot(const char *name, omrobjectptr_t parentPtr);
	int32_t allocationWalker(pugi::xml_node node);
#if defined(OMRGCTEST_PRINTFILE)
	void printFile(const char *name);
#endif
	int32_t verifyVerboseGC(pugi::xpath_node_set verboseGCs);
	int32_t parseGarbagePolicy(pugi::xml_node node);
	int32_t triggerOperation(pugi::xml_node node);

	virtual void SetUp();
	virtual void TearDown();

public:
	GCConfigTest() : ::testing::Test(), ::testing::WithParamInterface<const char *>(), exampleVM(&(gcTestEnv->exampleVM)), env(NULL), objectTable(gcTestEnv->portLib), verboseManager(NULL), verboseFile(NULL), numOfFiles(0)
	{
		gp.namePrefix = NULL;
		gp.percentage = 0.0f;
		gp.frequency = "none";
		gp.structure = NULL;
		gp.garbageSeq = 0;
		gp.accumulatedSize = 0;
	}
};
