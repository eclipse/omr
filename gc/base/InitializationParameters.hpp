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

#if !defined(INITIALIZATIONPARAMETERS_HPP_)
#define INITIALIZATIONPARAMETERS_HPP_

#include "modronbase.h"

#include "BaseNonVirtual.hpp"

/**
 * Wrapper class for passing memoryspace parameters into newInstance() methods
 */
class MM_InitializationParameters : public MM_BaseNonVirtual {
public:
	uintptr_t _minimumSpaceSize;
	uintptr_t _minimumNewSpaceSize, _initialNewSpaceSize, _maximumNewSpaceSize;
	uintptr_t _minimumOldSpaceSize, _initialOldSpaceSize, _maximumOldSpaceSize;
	uintptr_t _maximumSpaceSize;

	MMINLINE void clear()
	{
		_minimumSpaceSize = 0;
		_minimumNewSpaceSize = 0;
		_initialNewSpaceSize = 0;
		_maximumNewSpaceSize = 0;
		_minimumOldSpaceSize = 0;
		_initialOldSpaceSize = 0;
		_maximumOldSpaceSize = 0;
		_maximumSpaceSize = 0;
	}

	MM_InitializationParameters()
		: MM_BaseNonVirtual()
	{
		_typeId = __FUNCTION__;
		clear();
	}
};

#endif /* INITIALIZATIONPARAMETERS_HPP_ */
