/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "AllocateDescription.hpp"
#include "CollectorLanguageInterface.hpp"
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

typedef struct XmlStr {
	const char *object;
	const char *namePrefix;
	const char *type;
	const char *numOfFields;
	const char *breadth;
	const char *depth;
	const char *garbagePolicy;
	const char *percentage;
	const char *frequency;
	const char *structure;
} XmlStr;

class GCConfigTest : public ::testing::Test, public ::testing::WithParamInterface<const char *>
{
	/*
	 * Data members
	 */
protected:
	OMR_VM_Example *exampleVM;
	MM_EnvironmentBase *env;
	MM_CollectorLanguageInterface *cli;
	pugi::xml_document doc;
	GarbagePolicy gp;
	XmlStr xs;

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
	ObjectEntry *allocateHelper(const char *objName, uintptr_t size);
	ObjectEntry *createObject(const char *namePrefix, OMRGCObjectType objType, int32_t depth, int32_t nthInRow, uintptr_t size);
	int32_t createFixedSizeTree(ObjectEntry **objectEntry, const char *namePrefixStr, OMRGCObjectType objType, uintptr_t totalSize, uintptr_t objSize, int32_t breadth);
	int32_t processObjNode(pugi::xml_node node, const char *namePrefixStr, OMRGCObjectType objType, AttributeElem *numOfFieldsElem, AttributeElem *breadthElem, int32_t depth);
	int32_t insertGarbage();
	int32_t attachChildEntry(ObjectEntry *parentEntry, ObjectEntry *childEntry);
	int32_t removeObjectFromRootTable(const char *name);
	int32_t removeObjectFromObjectTable(const char *name);
	int32_t removeObjectFromParentSlot(const char *name, ObjectEntry *parentEntry);
	int32_t allocationWalker(pugi::xml_node node);
#if defined(OMRGCTEST_PRINTFILE)
	void printFile(const char *name);
#endif
	int32_t verifyVerboseGC(pugi::xpath_node_set verboseGCs);
	int32_t parseGarbagePolicy(pugi::xml_node node);
	int32_t triggerOperation(pugi::xml_node node);
	int32_t iniXMLStr(const char *configStyle);

	/* This implementation assumes that existing entries hashed into the rootTable and objectTable can
	 * be moved whenever new entries are added. This complicates the usage of ObjectEntry pointers that
	 * are returned from find() and add(), because these pointers may become invalid if new entries are
	 * subsequently added while the pointers are in scope. To deal with this, it is necessary to refresh
	 * these pointers by calling find() after executing code that adds new entries.
	 *
	 * Moreover it is assumed that whenever an existing entry is moved during table reconfiguration the
	 * memory location that it was moved from is freed for reuse so that any reference to data through
	 * the original pointer is also invalid. So refreshing a hashed entry pointer using the key embedded
	 * in the entry may not have the intended result. It is necessary to retain the key that was used to
	 * find() or add() the entry in order to refresh the entry pointer using find(key).
	 *
	 * Also, for these reasons, use of GC_ObjectIterator in mutator (GCConfigTest) code is strongly
	 * discouraged.
	 */

	ObjectEntry *
	find(const char *name)
	{
		ObjectEntry searchEntry;
		searchEntry.name = name;
		return (ObjectEntry *)hashTableFind(exampleVM->objectTable, &searchEntry);
	}

	ObjectEntry *
	add(ObjectEntry *objectEntry)
	{
		ObjectEntry *hashedEntry = (ObjectEntry *) hashTableAdd(exampleVM->objectTable, objectEntry);
		if (NULL == hashedEntry) {
			gcTestEnv->log(LEVEL_ERROR, "%s:%d Failed to add new object %s to hash table!\n", __FILE__, __LINE__, objectEntry->name);
		}
		return hashedEntry;
	}

	virtual void SetUp();
	virtual void TearDown();

public:
	GCConfigTest()
		: ::testing::Test()
		, ::testing::WithParamInterface<const char *>()
		, exampleVM(&(gcTestEnv->exampleVM))
		, env(NULL)
		, cli(NULL)
		, verboseManager(NULL)
		, verboseFile(NULL)
		, numOfFiles(0)
	{
		gp.namePrefix = NULL;
		gp.percentage = 0.0f;
		gp.frequency = "none";
		gp.structure = NULL;
		gp.garbageSeq = 0;
		gp.accumulatedSize = 0;

		xs.object = NULL;
		xs.namePrefix = NULL;
		xs.type = NULL;
		xs.numOfFields = NULL;
		xs.breadth = NULL;
		xs.depth = NULL;
		xs.garbagePolicy = NULL;
		xs.percentage = NULL;
		xs.frequency = NULL;
		xs.structure = NULL;
	}
};
