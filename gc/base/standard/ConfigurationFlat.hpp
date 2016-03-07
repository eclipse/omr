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
	static MM_Configuration* newInstance(MM_EnvironmentBase* env, MM_ConfigurationLanguageInterface* configurationLanguageInterface);

	virtual MM_MemorySpace* createDefaultMemorySpace(MM_EnvironmentBase* env, MM_Heap* heap, MM_InitializationParameters* parameters);

	MM_ConfigurationFlat(MM_EnvironmentBase* env, MM_ConfigurationLanguageInterface* configurationLanguageInterface)
		: MM_ConfigurationStandard(env, configurationLanguageInterface)
	{
		_typeId = __FUNCTION__;
	};
	
protected:
private:
};

#endif /* defined(OMR_GC_MODRON_STANDARD) */

#endif /* CONFIGURATIONFLAT_HPP_ */
