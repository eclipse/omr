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

/**
 * @file
 * @ingroup GC_Base
 */

#ifndef VALIDATOR_HPP_
#define VALIDATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "BaseVirtual.hpp"

class MM_EnvironmentBase;

class MM_Validator : public MM_BaseVirtual {
/* data members */
private:
protected:
public:
	
/* function members */
private:
protected:
public:
	/**
	 * Abstract function called in case of a crash while the validator is running.
	 * @param env[in] the current thread
	 */
	virtual void threadCrash(MM_EnvironmentBase* env) = 0;

	MM_Validator() 
		: MM_BaseVirtual()
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* VALIDATOR_HPP_ */
