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
