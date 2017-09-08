/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Structs
 */

#include "HashTableIterator.hpp"

#include "Debug.hpp"

/**
 * \brief       Get the next slot of a J9HashTable
 * \ingroup     GC_Structs
 *  
 * @return      A slot pointer (NULL when no more slots)
 * 
 *	Continues the interation of all nodes in a J9HashTable
 *      
 */
void **
GC_HashTableIterator::nextSlot()
{
	void **value;
	
	if (_firstIteration) {
		_firstIteration = false;

		value = (void **)hashTableStartDo(_hashTable, &_handle);
	} else {
		value = (void **)hashTableNextDo(&_handle);
	}

	return value;
}

/**
 * \brief       Remove the current slot in a J9HashTable
 * \ingroup     GC_Structs
 *  
 *	Removes the current slot in a J9HashTable (not valid in Out Of Process) 
 *      
 */
void 
GC_HashTableIterator::removeSlot()
{
	hashTableDoRemove(&_handle);
}

void
GC_HashTableIterator::disableTableGrowth()
{
	hashTableSetFlag(_hashTable, J9HASH_TABLE_DO_NOT_REHASH);
}

/**
 * Re-enable table growth which has been disabled by disableTableGrowth().
 */
void
GC_HashTableIterator::enableTableGrowth()
{
	hashTableResetFlag(_hashTable, J9HASH_TABLE_DO_NOT_REHASH);
}
