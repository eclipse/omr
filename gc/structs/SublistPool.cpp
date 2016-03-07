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

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"

#include <string.h>

#include "SublistPool.hpp"

#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"
#include "SublistFragment.hpp"
#include "SublistPuddle.hpp"

/**
 * Initialize the sublist pool default values and internal structure.
 * Allocate and initialize all internal values and support structures for the subpool to be ready for
 * use.
 * 
 * @return true if the initialization is successful, false otherwise.
 */
bool
MM_SublistPool::initialize(MM_EnvironmentBase *env, MM_AllocationCategory::Enum category)
{
	memset(this, 0, sizeof(*this));
	_allocCategory = category;
	return (!omrthread_monitor_init_with_name(&_mutex, 0, "MM_SublistPool"));
}

/**
 * Tear down an initialized sublist.
 */
void
MM_SublistPool::tearDown(MM_EnvironmentBase *env)
{
	MM_SublistPuddle *puddle, *nextPuddle;

	if(_mutex) {
		omrthread_monitor_destroy(_mutex);
	}

	/* Free all puddles associated to the sublist */
	puddle = _list;
	while(puddle) {
		nextPuddle = puddle->getNext();
		MM_SublistPuddle::kill(env, puddle);
		puddle = nextPuddle;
	}
}

/**
 * Allocate a new puddle for the current sublist pool.
 * 
 * @return The newly allocated puddle if successful, NULL otherwise.
 * 
 * @note the newly allocated puddle is NOT attached to the list.
 */
MM_SublistPuddle *
MM_SublistPool::createNewPuddle(MM_EnvironmentBase *env)
{
	uintptr_t puddleSize;

	/* If the sublist has a maximum size, be sure we aren't attempting to grow beyond it */
	if(_maxSize) {
		puddleSize = _maxSize - _currentSize;
		if(0 == puddleSize) {
			return NULL;
		}
		/* If the available size to grow is greater than the suggested size, reduce */
		if(puddleSize > _growSize) {
			puddleSize = _growSize;
		}
	} else {
		/* No limit on the grow size - use the suggested grow size */
		puddleSize = _growSize;
	}

	/* Check that the determined grow size is valid */
	if(0 == puddleSize) {
		return NULL;
	}

	/* Get a new puddle to add to the sublist pool */
	return MM_SublistPuddle::newInstance(env, puddleSize, this, _allocCategory);	
}

/**
 * Allocate a new fragment from a sublist.
 * Reserve memory from the sublist and update the fragment.  If there is no room available
 * in the current sublist memory, allocate a new sublist puddle (until the maximum sublist size is reached).
 * 
 * @return true if the fragment allocate is successful, false otherwise.
 */
bool
MM_SublistPool::allocate(MM_EnvironmentBase *env, MM_SublistFragment *fragment)
{
	uintptr_t puddleSize = 0;
	MM_SublistPuddle *emptyPuddle = NULL;

	/* Attempt to allocate a fragment from the current allocation puddle. If successful, we are done. */
	if(_allocPuddle && _allocPuddle->allocate(fragment)) {
		return true;
	}

	/* No new fragment is available.  Lock the sublist and allocate a new puddle */
	omrthread_monitor_enter(_mutex);

	/* Another thread may have allocated a puddle while attempting to get the lock
	 * Check by attempting to allocate the puddle again 
	 */
	if(_allocPuddle && _allocPuddle->allocate(fragment)) {
		omrthread_monitor_exit(_mutex);
		return true;
	}

	/* Are there any puddles past the alloc puddle? */
	if (_allocPuddle && _allocPuddle->getNext()) {
		/* Yes - it's guaranteed to be empty */
		emptyPuddle = _allocPuddle->getNext();
		Assert_MM_true(NULL != emptyPuddle);
		Assert_MM_true(emptyPuddle->isEmpty());
	} else {
		/* No - we need to allocate a new puddle */

		/* If the sublist has a maximum size, be sure we aren't attempting to grow beyond it */
		if(_maxSize) {
			puddleSize = _maxSize - _currentSize;
			if(0 == puddleSize) {
				omrthread_monitor_exit(_mutex);
				return false;
			}
			/* If the available size to grow is greater than the suggested size, reduce */
			if(puddleSize > _growSize) {
				puddleSize = _growSize;
			}
		} else {
			/* No limit on the grow size - use the suggested grow size */
			puddleSize = _growSize;
		}
	
		/* Check that the determined grow size is valid */
		if(0 == puddleSize) {
			omrthread_monitor_exit(_mutex);
			return false;
		}
	
		/* Get a new puddle to add to the sublist pool */
		if(NULL == (emptyPuddle = MM_SublistPuddle::newInstance(env, puddleSize, this, _allocCategory))) {
			omrthread_monitor_exit(_mutex);
			return false;
		}
		Assert_MM_true(emptyPuddle->isEmpty());
		Assert_MM_true(NULL == emptyPuddle->getNext());
	 	_currentSize += emptyPuddle->totalSize();
	}
 
 	/* Allocate the fragment from the puddle. We are guaranteed to succeed because
 	 * other threads don't have access yet -- either because it's new, or because
 	 * they can't allocate from anything other than _allocPuddle
 	 */
	bool mustSucceed = emptyPuddle->allocate(fragment);
 	Assert_MM_true(mustSucceed);

 	/* Now that we have allocated our fragment, it is safe to expose the puddle to the rest of the VM */
	if (NULL == _list) {
		/* This is the first puddle. Make it the head of the list. */
		/* (The list is empty, so there must not be an _allocPuddle) */
		Assert_MM_true(NULL == _allocPuddle);
		_list = emptyPuddle;
	} else {
		/* Add this puddle to the tail of the list */
		/* (The list is non-empty so there must be an _allocPuddle) */
		Assert_MM_true(NULL != _allocPuddle);
		Assert_MM_true(NULL == _allocPuddle->getNext());
		_allocPuddle->setNext(emptyPuddle);
	}
	_allocPuddle = emptyPuddle;
	Assert_MM_true(NULL == _allocPuddle->getNext());

	omrthread_monitor_exit(_mutex);

	return true;
}

/**
 * Allocate a single entry in the sublist.
 * 
 * @return A slot in the sublist if successful, NULL otherwise.
 * 
 * @note The allocate is made without any locks or contention checking.
 */
uintptr_t *
MM_SublistPool::allocateElementNoContention(MM_EnvironmentBase *env)
{
	uintptr_t *element;
	MM_SublistPuddle *emptyPuddle;

	/* Allocate a new fragment from the current alloc puddle (if successful, we are done) */
	if(_allocPuddle && (NULL != (element = _allocPuddle->allocateElementNoContention()))) {
		return element;
	}

	/* Any puddles past the alloc puddle are guaranteed to
	 * be empty. Attempt to use one of those puddles before
	 * creating a new one.
	 */	
	if(_allocPuddle && _allocPuddle->getNext()) {
		emptyPuddle = _allocPuddle->getNext();	
	} else {
		/* No new fragment is available.  Allocate a new puddle */
		if(NULL == (emptyPuddle = createNewPuddle(env))) {
			return NULL;
		}
		_currentSize += emptyPuddle->totalSize();		

		/* Link the new puddle into the list */
		if (_allocPuddle) {
			_allocPuddle->setNext(emptyPuddle);	
		}
		if (_list == NULL) {
			_list = emptyPuddle;	
		}
	}
	
 	_allocPuddle = emptyPuddle;

	element = _allocPuddle->allocateElementNoContention();

	return element;
}
 
void
MM_SublistPool::compact(MM_EnvironmentBase *env)
{
	MM_SublistPuddle *currentPuddle, *nextPuddle;
	MM_SublistPuddle *sourcePuddle, *destinationPuddle;
	MM_SublistPuddle *lastPuddle = NULL;

	/* Use the list of puddles to iterate through and reset the list pointer to NULL
	 * as we will add puddles back
	 */
	currentPuddle = _list;
	_list = NULL;

	/* Run through the list of puddles deciding whether they should be compacted into one another,
	 * killed or added directly to the list
	 */
	destinationPuddle = NULL;
	while(currentPuddle) {
		nextPuddle = currentPuddle->getNext();

		if(currentPuddle->isEmpty()) {
			/* The puddle is empty, free it and move to the next one */
			MM_SublistPuddle::kill(env, currentPuddle);
			currentPuddle = nextPuddle;
			continue;
		}
		if(currentPuddle->isFull()) {
			/* The puddle is full, add it to the list and move to the next one */
			currentPuddle->setNext(_list);
			if (_list == NULL) {
				lastPuddle = currentPuddle;
			}
			_list = currentPuddle;
			currentPuddle = nextPuddle;
			continue;
		}

		/* The puddle must be partially full */
		if(!destinationPuddle) {
			/* There is no current merge candidate, record the current puddle */
			destinationPuddle = currentPuddle;
		} else {
			/* We already have a puddle that is partially full.  Find which puddle has the lesser
			 * number of allocated elements, and copy it into the other puddle.  It may be this is
			 * only a partial copy
			 */
			if(destinationPuddle->consumedSize() >= currentPuddle->consumedSize()) {
				sourcePuddle = currentPuddle;
			} else {
				sourcePuddle = destinationPuddle;
				destinationPuddle = currentPuddle;
			}

			/* Merge the source into the destination */
			destinationPuddle->merge(sourcePuddle);

			if(destinationPuddle->isFull()) {
				/* The destination is full, no longer a merge candidate.  Add it to the
				 * puddle list and reset the destination merge candidate
				 */
				destinationPuddle->setNext(_list);
				if (_list == NULL) {
					lastPuddle = destinationPuddle;
				}
				_list = destinationPuddle;

				/* Determine what to do with the source puddle */
				if(sourcePuddle->isEmpty()) {
					/* The source puddle is empty - kill it and set the destination puddle to NULL
					 * (we have no destination candidate as it is full)
					 */
					_currentSize -= sourcePuddle->totalSize();
					MM_SublistPuddle::kill(env, sourcePuddle);
					destinationPuddle = NULL;
				} else {
					/* The destination is full and the source is not - the source becomes the new
					 * destination candidate
					 */
					destinationPuddle = sourcePuddle;
				}
			} else {
				/* If we couldn't fill the destination, the source puddle must be empty - kill it */
				_currentSize -= sourcePuddle->totalSize();
				MM_SublistPuddle::kill(env, sourcePuddle);
			}
		}
		/* Move to the next puddle */
		currentPuddle = nextPuddle;
	}

	/* If there is a destination puddle after compacting the list, it is the alloc puddle. */
	if(destinationPuddle) {
		if (lastPuddle) {
			lastPuddle->setNext(destinationPuddle);	
		} else {
			_list = destinationPuddle;
		}
		destinationPuddle->setNext(NULL);
		_allocPuddle = destinationPuddle;
	} else {
		_allocPuddle = lastPuddle;	
	}
}

/**
 * Clear the sublist pool of all entries.
 * Resets the sublist puddle list to NULL, freeing all underlying puddles.
 * 
 * @note assumes an exclusive use scenario, does not lock any structures/access.
 * @note does not clear any fragments which may have cached ranges within the puddles.
 */
void
MM_SublistPool::clear(MM_EnvironmentBase *env)
{
	MM_SublistPuddle *puddle = NULL;

	/* Reset the size */
	_currentSize = 0;

	/* Free the puddles and reset the lists to NULL */
	puddle = _list;
	while(NULL != puddle) {
		MM_SublistPuddle* nextPuddle = puddle->getNext();
		MM_SublistPuddle::kill(env, puddle);
		puddle = nextPuddle;
	}
	
	puddle = _previousList;
	while(NULL != puddle) {
		MM_SublistPuddle* nextPuddle = puddle->getNext();
		MM_SublistPuddle::kill(env, puddle);
		puddle = nextPuddle;
	}
	_list = NULL;
	_allocPuddle = NULL;
	_previousList = NULL;
	_count = 0;
}

/**
 * Count the number of elements consumed in a sublist pool
 * Requires all fragments to be flushed for _count to be 
 * correct.
 */
uintptr_t
MM_SublistPool::countElements()
{	
	return _count;
}

void
MM_SublistPool::startProcessingSublist() 
{
	Assert_MM_true(NULL == _previousList);
	_previousList = _list;

	MM_SublistPuddle* tail = _allocPuddle;
	if (NULL == tail) {
		_list = NULL;
		_allocPuddle = NULL;
	} else {
		_list = tail->getNext();
		tail->setNext(NULL);
		_allocPuddle = _list;
		
		/* if there is an _allocPuddle it must be empty at this point */
		Assert_MM_true( (NULL == _allocPuddle) || (_allocPuddle->isEmpty()) ); 
	}
}

MM_SublistPuddle *
MM_SublistPool::popPreviousPuddle(MM_SublistPuddle * returnedPuddle)
{
	omrthread_monitor_enter(_mutex);

	/* return returnedPuddle to the list of used puddles */
	if (NULL != returnedPuddle) {
		Assert_MM_true(NULL == returnedPuddle->getNext());
		returnedPuddle->setNext(_list);
		_list = returnedPuddle;

		/* It's illegal to have a non-empty list without an _allocPuddle. If 
		 * this is the only puddle in the pool, make it the _allocPuddle. 
		 */
		if (NULL == _allocPuddle) {
			_allocPuddle = returnedPuddle;
			Assert_MM_true(NULL == _allocPuddle->getNext());
		}
	}

	/* pop an element from the previous list */
	MM_SublistPuddle *result = _previousList;
	if (NULL != result) {
		_previousList = result->getNext();
		result->setNext(NULL);
	}
	
	omrthread_monitor_exit(_mutex);
	
	return result;
}
