/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(OBJECTMODEL_HPP_)
#define OBJECTMODEL_HPP_

/*
 * @ddr_namespace: default
 */

#include "Object.hpp"
#include "Bits.hpp"
#include "ObjectModelBase.hpp"
#include "ObjectModelDelegate.hpp"

class MM_GCExtensionsBase;

/**
 * Provides an example of an object model, sufficient to support basic GC test code (GCConfigTest)
 * and to illustrate some of the required facets of an OMR object model.
 */
class GC_ObjectModel : public GC_ObjectModelBase
{
/*
 * Member data and types
 */
private:
protected:
public:

/*
 * Member functions
 */
private:
protected:
public:
	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel. An implementation of this method is
	 * required. Object models that require initialization after instantiation should perform this
	 * initialization here. Otherwise, the implementation should simply return true.
	 *
	 * @return TRUE on success, FALSE on failure
	 */
	virtual bool initialize(MM_GCExtensionsBase *extensions) { return true; }

	/**
	 * Tear down the receiver. A (possibly empty) implementation of this method is required. Any resources
	 * acquired for the object model in the initialize() implementation should be disposed of here.
	 */
	virtual void tearDown(MM_GCExtensionsBase *extensions) {}

	/**
	 * Constructor.
	 */
	GC_ObjectModel()
		: GC_ObjectModelBase()
	{}
};
#endif /* OBJECTMODEL_HPP_ */
