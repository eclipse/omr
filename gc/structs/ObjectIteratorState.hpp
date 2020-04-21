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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Structs
 */
 
#if !defined(OBJECTITERATORSTATE_HPP_)
#define OBJECTITERATORSTATE_HPP_

#include "omrcfg.h"
#include "objectdescription.h"

/**
 * Defines the class for storing ObjectIterator state. It is necessary to use this common
 * class to allow inlining of some costly virtual methods, and also to reduce storage costs.
 */
class GC_ObjectIteratorState
{
	/* Data Members */
private:
protected:
public:
	omrobjectptr_t _objectPtr;				/**< pointer to the array object being scanned */
	union {
		uintptr_t _index;					/**< index into arraylet */
		fomrobject_t *_scanPtr;			/**< scan pointer into non-arraylet reference slot */
	};
	bool _contiguous; /**< whether or not the array being iterated is contiguous */

	fomrobject_t *_endPtr;				/**< points past last reference slot */
	uintptr_t *_descriptionPtr;				/**< pointer to next description word */
	uintptr_t _description;					/**< current description word shifted to next slot */
	uintptr_t _descriptionIndex;			/**< iteration for next description slot */

	/* Member Functions */
private:
protected:
public:
	GC_ObjectIteratorState() {}
};

#endif /* OBJECTITERATORSTATE_HPP_ */
