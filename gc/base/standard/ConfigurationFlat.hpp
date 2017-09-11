/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONFIGURATIONFLAT_HPP_)
#define CONFIGURATIONFLAT_HPP_

#include "omrcfg.h"

#include "ConfigurationStandard.hpp"

#if defined(OMR_GC_MODRON_STANDARD)

class MM_ConfigurationFlat : public MM_ConfigurationStandard {
public:
protected:
private:
public:
	static MM_Configuration* newInstance(MM_EnvironmentBase* env);

	virtual MM_MemorySpace* createDefaultMemorySpace(MM_EnvironmentBase* env, MM_Heap* heap, MM_InitializationParameters* parameters);

	MM_ConfigurationFlat(MM_EnvironmentBase* env)
		: MM_ConfigurationStandard(env, env->getExtensions()->configurationOptions._gcPolicy, STANDARD_REGION_SIZE_BYTES)
	{
		_typeId = __FUNCTION__;
	};
	
protected:
private:
};

#endif /* defined(OMR_GC_MODRON_STANDARD) */

#endif /* CONFIGURATIONFLAT_HPP_ */
