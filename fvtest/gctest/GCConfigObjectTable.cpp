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

#include "GCConfigObjectTable.hpp"

static uintptr_t objectTableHashFn(void *entry, void *userData);
static uintptr_t objectTableHashEqualFn(void *leftEntry, void *rightEntry, void *userData);
static uintptr_t objectTableFreeFn(void *entry, void *userData);

/**
 * Hash table callback for retrieving hash value of an entry
 *
 * @param[in] entry Entry to hash
 * @param[in] userData Data that can be passed along, unused in this callback
 */
static uintptr_t
objectTableHashFn(void *entry, void *userData)
{
	const char *name = ((ObjectEntry *)entry)->name;
	uintptr_t length = strlen(name);
	uintptr_t hash = 0;
	uintptr_t i;

	for (i = 0; i < length; i++) {
		hash = (hash << 5) - hash + name[i];
	}

	return hash;
}

/**
 * Hash table callback for entry comparison
 *
 * @param[in] leftEntry Left entry to compare
 * @param[in] rightEntry Right entry to compare
 * @param[in] userData Data that can be passed along, unused in this callback
 *
 * @return True if the entries are deemed equal, that is if the string representation of their
 * keys are identical.
 *
 */
static uintptr_t
objectTableHashEqualFn(void *leftEntry, void *rightEntry, void *userData)
{
	ObjectEntry *loe = (ObjectEntry *)leftEntry;
	ObjectEntry *roe = (ObjectEntry *)rightEntry;
	return (0 == strcmp(loe->name, roe->name));
}

/**
 * Hash table callback used when about to free the hash table, this ensures the memory
 * used by the ObjectEntry is freed up (and not just the hash table itself).
 *
 * @param[in] entry The entry to free
 * @param[in] userData Data that can be passed along, unused in this callback
 */
static uintptr_t
objectTableFreeFn(void *entry, void *userData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	ObjectEntry *objEntry = (ObjectEntry *)entry;

	omrmem_free_memory((void *)objEntry->name);
	objEntry->name = NULL;

	return 0;
}

void
GCConfigObjectTable::create()
{
	objectTable = hashTableNew(
		portLib, OMR_GET_CALLSITE(), 0, sizeof(ObjectEntry), 0, 0, OMRMEM_CATEGORY_MM,
		objectTableHashFn, objectTableHashEqualFn, NULL, NULL);
}

void
GCConfigObjectTable::free()
{
	if (NULL != objectTable) {
		hashTableForEachDo(objectTable, objectTableFreeFn, NULL);
		hashTableFree(objectTable);
	}
}

int32_t
GCConfigObjectTable::find(ObjectEntry **entry, const char *name)
{
	int32_t rt = 0;
	ObjectEntry searchEntry;
	searchEntry.name = name;
	*entry = (ObjectEntry *)hashTableFind(objectTable, &searchEntry);
	if (NULL == *entry) {
		rt = 1;
	}
	return rt;
}

int32_t
GCConfigObjectTable::add(const char *name, omrobjectptr_t obj, int32_t numOfRef)
{
	OMRPORT_ACCESS_FROM_OMRPORT(gcTestEnv->portLib);
	int32_t rt = 0;
	ObjectEntry objEntry;
	objEntry.name = name;
	objEntry.objPtr = obj;
	objEntry.numOfRef = numOfRef;
	if (NULL == hashTableAdd(objectTable, &objEntry)) {
		rt = 1;
		omrtty_printf("%s:%d Failed to add new object %s to hash table!\n", __FILE__, __LINE__, name);
	}
	return rt;
}
