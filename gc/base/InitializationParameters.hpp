/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 ******************************************************************************/

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
