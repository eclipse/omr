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
#if defined(OMR_GC_ARRAYLETS)
	union {
		uintptr_t _index;					/**< index into arraylet */
		fomrobject_t *_scanPtr;			/**< scan pointer into non-arraylet reference slot */
	};
#if defined(OMR_GC_HYBRID_ARRAYLETS)
	bool _contiguous; /**< whether or not the array being iterated is contiguous */
#endif /* OMR_GC_HYBRID_ARRAYLETS */
#else
	fomrobject_t *_scanPtr;				/**< scan pointer into next reference slot */
#endif /* OMR_GC_ARRAYLETS */

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
