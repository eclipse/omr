/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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
	virtual void registerCallback(MM_EnvironmentStandard *env, SafepointCallbackHandler handler, void *userData, bool cancelAfterGC = false);
#else
	virtual void registerCallback(MM_EnvironmentStandard *env,  SafepointCallbackHandler handler, void *userData);
#endif /* defined(AIXPPC) || defined(LINUXPPC) */

	virtual void requestCallback(MM_EnvironmentStandard *env);

	virtual void cancelCallback(MM_EnvironmentStandard *env);

	/* Providing this no-op class as a concrete implementation reduces the glue burden on client languages. */
	static MM_ConcurrentSafepointCallback *newInstance(MM_EnvironmentStandard *env);
	virtual void kill(MM_EnvironmentBase *env);
	/**
	 * Create a MM_ConcurrentSafepointCallback object
	 */
	MM_ConcurrentSafepointCallback(MM_EnvironmentStandard *env)
		: MM_BaseVirtual()
		,_handler(NULL)
		,_userData(NULL)
	{
		_typeId = __FUNCTION__;

	}
};

#endif /* defined (OMR_GC_MODRON_CONCURRENT_MARK)*/

#endif /* CONCURRENTSAFEPOINTCALLBACK_HPP_ */
