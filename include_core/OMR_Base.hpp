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

#if !defined(OMR_BASE_HPP_)
#define OMR_BASE_HPP_

#include "omrcomp.h"

class OMR_Base
{
private:
protected:
public:
	void *operator new(size_t size, void *memoryPtr)
	{
		return memoryPtr;
	}

	/**
	 * Create OMR_Base object.
	 */
	OMR_Base() {}

};

#endif /* OMR_BASE_HPP_ */
