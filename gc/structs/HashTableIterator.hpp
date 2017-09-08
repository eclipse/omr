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
