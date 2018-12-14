/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(OBJECTALLOCATIONMODEL_HPP_)
#define OBJECTALLOCATIONMODEL_HPP_

#include "AllocateInitialization.hpp"
#include "ObjectModel.hpp"

/**
 * Class definition for the Java object allocation model.
 */
class MM_ObjectAllocationModel : public MM_AllocateInitialization
{
	/*
	 * Member data and types
	 */
public:
	/**
	 * Define object allocation categories. These are represented in MM_AllocateInitialization
	 * objects and are used in GC_ObjectModel::initializeAllocation() to determine how to
	 * initialize the header of a newly allocated object.
	 */
	enum {
		allocation_category_example
	};

protected:
private:

	/*
	 * Member functions
	 */
private:
protected:
public:
	/**
	 * Initializer.
	 */
	MMINLINE omrobjectptr_t
	initializeObject(MM_EnvironmentBase *env, void *allocatedBytes)
	{
		omrobjectptr_t objectPtr = (omrobjectptr_t)allocatedBytes;

		if (NULL != objectPtr) {
			objectPtr->header.sizeInBytes((ObjectSize)getAllocateDescription()->getBytesRequested());
		}

		return objectPtr;
	}

	/**
	 * Constructor.
	 */
	MM_ObjectAllocationModel(MM_EnvironmentBase *env,  uintptr_t requiredSizeInBytes, uintptr_t allocateObjectFlags = 0)
		: MM_AllocateInitialization(env, allocation_category_example, requiredSizeInBytes, allocateObjectFlags)
	{}
};
#endif /* OBJECTALLOCATIONMODEL_HPP_ */
