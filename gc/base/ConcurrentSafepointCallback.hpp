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

#if !defined(CONCURRENTSAFEPOINTCALLBACK_HPP_)
#define CONCURRENTSAFEPOINTCALLBACK_HPP_

#include "omrcfg.h"


#include "omrcomp.h"
#include "BaseVirtual.hpp"

#if defined (OMR_GC_MODRON_CONCURRENT_MARK)

class MM_EnvironmentBase;
class MM_EnvironmentStandard;

typedef void (*SafepointCallbackHandler)(struct OMR_VMThread * currentThread, void * userData);

class MM_ConcurrentSafepointCallback : public MM_BaseVirtual
{
private:
protected:
	SafepointCallbackHandler _handler;
	void * _userData;
public:

private:
protected:

public:
#if defined(AIXPPC) || defined(LINUXPPC)
	virtual void registerCallback(MM_EnvironmentBase *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC = false);
#else
	virtual void registerCallback(MM_EnvironmentBase *env,  SafepointCallbackHandler handler, void *userData);
#endif /* defined(AIXPPC) || defined(LINUXPPC) */

	virtual void requestCallback(MM_EnvironmentStandard *env);

	virtual void cancelCallback(MM_EnvironmentStandard *env);

	/* Providing this no-op class as a concrete implementation reduces the glue burden on client languages. */
	static MM_ConcurrentSafepointCallback *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	/**
	 * Create a MM_ConcurrentSafepointCallback object
	 */
	MM_ConcurrentSafepointCallback(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		,_handler(NULL)
		,_userData(NULL)
	{
		_typeId = __FUNCTION__;

	}
};

#endif /* defined (OMR_GC_MODRON_CONCURRENT_MARK)*/

#endif /* CONCURRENTSAFEPOINTCALLBACK_HPP_ */
