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
