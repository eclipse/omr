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

#include <string.h>
#include "omrExampleVM.hpp"

/**
 * Hash table callback for retrieving hash value of an entry
 *
 * @param[in] entry Entry to hash
 * @param[in] userData Data that can be passed along, unused in this callback
 */
uintptr_t
rootTableHashFn(void *entry, void *userData)
{
	const char *name = ((RootEntry *)entry)->name;
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
uintptr_t
rootTableHashEqualFn(void *leftEntry, void *rightEntry, void *userData)
{
	RootEntry *loe = (RootEntry *)leftEntry;
	RootEntry *roe = (RootEntry *)rightEntry;
	return (0 == strcmp(loe->name, roe->name));
}

/**
 * Hash table callback for retrieving hash value of an entry
 *
 * @param[in] entry Entry to hash
 * @param[in] userData Data that can be passed along, unused in this callback
 */
uintptr_t
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
uintptr_t
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
uintptr_t
objectTableFreeFn(void *entry, void *userData)
{
	OMR_VM_Example *exampleVM = (OMR_VM_Example *)userData;
	OMRPORT_ACCESS_FROM_OMRVM(exampleVM->_omrVM);
	ObjectEntry *objEntry = (ObjectEntry *)entry;

	omrmem_free_memory((void *)objEntry->name);
	objEntry->name = NULL;

	return 0;
}
