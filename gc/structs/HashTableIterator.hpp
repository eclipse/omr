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

#if !defined(HASHTABLEITERATOR_HPP_)
#define HASHTABLEITERATOR_HPP_

#include "omrcomp.h"
#include "modronbase.h"
#include "omrhashtable.h"
#include "hashtable_api.h"

/**
 * Iterate over all slots in a J9HashTable.  The hash table is actually backed
 * by a pool, and as long as no slots are being deleted (as in the out-of-process
 * case), we can just use the pool iterator.
 * 
 * @ingroup GC_Structs
 */
class GC_HashTableIterator
{
	J9HashTable *_hashTable;
	J9HashTableState _handle;
	bool _firstIteration;	

public:
	GC_HashTableIterator(J9HashTable *hashTable)
	{
		initialize(hashTable);
	}

	void **nextSlot();

	virtual void removeSlot();
	
	/**
	 * Prevent the hash table from growing. This allows the iteration to be interrupted and more
	 * elements may be added to the table before resuming. Elements still should not be deleted
	 * while iteration is interrupted.
	 */
	void disableTableGrowth();
	
	/**
	 * Re-enable table growth which has been disabled by disableTableGrowth().
	 */
	void enableTableGrowth();

	/**
	 * Reuse this iterator on a different hashTable
	 */
	MMINLINE void 
	initialize(J9HashTable *hashTable)
	{
		_firstIteration = true;
		_hashTable = hashTable;
	}
};

#endif /* HASHTABLEITERATOR_HPP_ */
