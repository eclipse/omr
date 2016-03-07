/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2014
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

#if !defined(BASENONVIRTUAL_HPP_)
#define BASENONVIRTUAL_HPP_

#include <stdlib.h>
#include <stddef.h>
#include "Base.hpp"

class MM_BaseNonVirtual : public MM_Base
{
private:
protected:
	/* Used by DDR to figure out runtime types, this is opt-in
	 * and has to be done by the constructor of each subclass.
	 * e.g. _typeId = __FUNCTION__;
	 */
	const char* _typeId;
	
public:
	
	/**
	 * Create a Base Non Virtual object.
	 */
	MM_BaseNonVirtual()
	{
		_typeId = NULL; // If NULL DDR will print the static (compile-time) type.
	};
};

#endif /* BASENONVIRTUAL_HPP_ */
