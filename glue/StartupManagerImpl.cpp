/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConfigurationLanguageInterfaceImpl.hpp"
#if defined(OMR_GC_SEGREGATED_HEAP)
#include "ConfigurationSegregated.hpp"
#endif /* OMR_GC_SEGREGATED_HEAP */
#include "ConfigurationFlat.hpp"
#include "VerboseManagerImpl.hpp"

#include "StartupManagerImpl.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)
#define OMR_SEGREGATEDHEAP "-Xgcpolicy:segregated"
#define OMR_SEGREGATEDHEAP_LENGTH 21
#endif /* OMR_GC_SEGREGATED_HEAP */

bool
MM_StartupManagerImpl::handleOption(MM_GCExtensionsBase *extensions, char *option)
{
	bool result = MM_StartupManager::handleOption(extensions, option);

	if (!result) {
#if defined(OMR_GC_SEGREGATED_HEAP)
		if (0 == strncmp(option, OMR_SEGREGATEDHEAP, OMR_SEGREGATEDHEAP_LENGTH)) {
			/* OMRTODO: when we have a flag in extensions to use a segregated heap,
			 * perhaps we should set it to true here..
			 */
			_useSegregatedGC = true;
			result = true;
		}
#endif /* OMR_GC_SEGREGATED_HEAP */
	}

	return result;
}

MM_ConfigurationLanguageInterface *
MM_StartupManagerImpl::createConfigurationLanguageInterface(MM_EnvironmentBase *env)
{
	return MM_ConfigurationLanguageInterfaceImpl::newInstance(env);
}

MM_CollectorLanguageInterface *
MM_StartupManagerImpl::createCollectorLanguageInterface(MM_EnvironmentBase *env)
{
	return MM_CollectorLanguageInterfaceImpl::newInstance(env);
}

MM_VerboseManagerBase *
MM_StartupManagerImpl::createVerboseManager(MM_EnvironmentBase* env)
{
	return MM_VerboseManagerImpl::newInstance(env, env->getOmrVM());
}

char *
MM_StartupManagerImpl::getOptions(void)
{
	return getenv("OMR_GC_OPTIONS");
}

MM_Configuration *
MM_StartupManagerImpl::createConfiguration(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface *cli)
{
#if defined(OMR_GC_SEGREGATED_HEAP)
	if (_useSegregatedGC) {
		return MM_ConfigurationSegregated::newInstance(env, cli);
	} else
#endif /* OMR_GC_SEGREGATED_HEAP */
	{
		return MM_ConfigurationFlat::newInstance(env, cli);
	}
}
