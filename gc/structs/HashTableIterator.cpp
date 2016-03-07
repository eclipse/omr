/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
