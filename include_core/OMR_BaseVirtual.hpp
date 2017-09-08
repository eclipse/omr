/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#if !defined(OMR_BASEVIRTUAL_HPP_)
#define OMR_BASEVIRTUAL_HPP_

/*
 * @ddr_namespace: default
 */

#include "OMR_Base.hpp"

class OMR_BaseVirtual : public OMR_Base
{
private:
protected:
	/* Used by DDR to figure out runtime types, this is opt-in
	 * and has to be done by the constructor of each subclass.
	 * e.g. _typeId = __FUNCTION__;
	 */
	const char *_typeId;

public:
	/**
	 * Create a Base Virtual object.
	 */
	OMR_BaseVirtual()
	{
		_typeId = NULL; // If NULL DDR will print the static (compile-time) type.
	};

	/*
	 * Required to force OMR_BaseVirtual to have a vtable, otherwise
	 * field offsets are wrong in DDR (due to addition of the vpointer
	 * in derived classes). Using a virtual destructor causes linking
	 * issues because we never use -lstdc++ (outside tests) and the
	 * delete implementation will be missing (e.g. needed by stack allocation)
	 */
	virtual void emptyMethod() { /* No implementation */ };
};

#endif /* OMR_BASEVIRTUAL_HPP_ */
