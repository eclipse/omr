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

#if !defined(CARDCLEANER_HPP_)
#define CARDCLEANER_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"

#if defined (OMR_GC_HEAP_CARD_TABLE)

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Base
 */

class MM_CardCleaner : public MM_BaseVirtual
{
public:
protected:
private:

public:
	/**
	 * Clean a range of addresses (typically within a span of a card)
	 * The action for each object/slot found may is specific - subclass has to overload this method
	 * Note that the caller does not clean the card.  It merely invokes the receiver if it is dirty
	 * so the implementation must mark it as clean, if it has that responsibility.
	 *
	 * @param[in] env A thread (typically the thread initializing the GC)
	 * @param[in] lowAddress low address of the range to be cleaned
	 * @param[in] highAddress high address of the range to be cleaned 
	 * @param cardToClean[in/out] The card which we are cleaning
	 */	
	virtual void clean(MM_EnvironmentBase *env, void *lowAddress, void *highAddress, Card *cardToClean) = 0;
	
	/**
	 * Return the uintptr_t  corresponding to the VMState for this card cleaner.
	 * @note All card cleanders must implement this method - the IDs are defined in @ref j9modron.h
	 */
	virtual uintptr_t getVMStateID() = 0;
	
	/**
	 * Create a CardTable object.
	 */
	MM_CardCleaner()
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}

protected:
private:
};

#endif /* defined (OMR_GC_HEAP_CARD_TABLE) */

#endif /* CARDCLEANER_HPP_ */
