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
 * @ingroup GC_Modron_Env
 */

#if !defined(ENVIRONMENTSTANDARD_HPP_)
#define ENVIRONMENTSTANDARD_HPP_

#include "omrcfg.h"
#include "j9nongenerated.h"
#include "omrport.h"
#include "modronopt.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

class MM_CopyScanCacheStandard;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Env
 */
class MM_EnvironmentStandard : public MM_EnvironmentBase
{
/* Data Section */
public:
	MM_CopyScanCacheStandard *_survivorCopyScanCache; /**< the current copy cache for flipping */
	MM_CopyScanCacheStandard *_scanCache; /**< the current scan cache */
	MM_CopyScanCacheStandard *_deferredScanCache; /**< a copy cache about to be pushed to scan queue, but before that may be merged with some other caches that collectively form contiguous memory */
	MM_CopyScanCacheStandard *_deferredCopyCache; /**< a copy cache about to be pushed to scan queue, but before that may be merged with some other caches that collectively form contiguous memory */
	MM_CopyScanCacheStandard *_tenureCopyScanCache; /**< the current copy cache for tenuring */
	MM_CopyScanCacheStandard *_effectiveCopyScanCache; /**< the the copy cache the received the most recently copied object, or NULL if no object copied in copy() */
	J9VMGC_SublistFragment _scavengerRememberedSet;
	void *_tenureTLHRemainderBase;  /**< base and top pointers of the last unused tenure TLH copy cache, that might be reused  on next copy refresh */
	void *_tenureTLHRemainderTop;
	bool _loaAllocation;  /** true, if tenure TLH remainder is in LOA (TODO: try preventing remainder creation in LOA) */
	void *_survivorTLHRemainderBase; /**< base and top pointers of the last unused survivor TLH copy cache, that might be reused  on next copy refresh */
	void *_survivorTLHRemainderTop;

	/* TODO: Temporary hiding place for thread specific GC structures */
	bool _threadCleaningCards;

protected:

private:
	
/* Functionality Section */
public:
	static MM_EnvironmentStandard *newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *vmThread);

	MMINLINE static MM_EnvironmentStandard *getEnvironment(OMR_VMThread *omrVMThread) { return static_cast<MM_EnvironmentStandard*>(omrVMThread->_gcOmrVMThreadExtensions); }
	MMINLINE static MM_EnvironmentStandard *getEnvironment(MM_EnvironmentBase *env) { return static_cast<MM_EnvironmentStandard*>(env); }

	MM_EnvironmentStandard(OMR_VMThread *omrVMThread) :
		MM_EnvironmentBase(omrVMThread)
		,_survivorCopyScanCache(NULL)
		,_scanCache(NULL)
		,_deferredScanCache(NULL)
		,_deferredCopyCache(NULL)
		,_tenureCopyScanCache(NULL)
		,_effectiveCopyScanCache(NULL)
		,_tenureTLHRemainderBase(NULL)
		,_tenureTLHRemainderTop(NULL)
		,_loaAllocation(false)
		,_survivorTLHRemainderBase(NULL)
		,_survivorTLHRemainderTop(NULL)
		,_threadCleaningCards(false)
	{
		_typeId = __FUNCTION__;
	}

protected:
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	virtual void tearDown(MM_GCExtensionsBase *extensions);
	
private:
	
};

#endif /* ENVIRONMENT_HPP_ */
