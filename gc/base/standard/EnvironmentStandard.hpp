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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
#include "SublistFragment.hpp"

class MM_CopyScanCacheStandard;
class MM_Evacuator;

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
#if defined(OMR_GC_MODRON_SCAVENGER)
	J9VMGC_SublistFragment _scavengerRememberedSet;
#endif
	void *_tenureTLHRemainderBase;  /**< base and top pointers of the last unused tenure TLH copy cache, that might be reused  on next copy refresh */
	void *_tenureTLHRemainderTop;
	bool _loaAllocation;  /** true, if tenure TLH remainder is in LOA (TODO: try preventing remainder creation in LOA) */
	void *_survivorTLHRemainderBase; /**< base and top pointers of the last unused survivor TLH copy cache, that might be reused  on next copy refresh */
	void *_survivorTLHRemainderTop;

protected:

private:
	MM_Evacuator *_evacuator;
	
/* Functionality Section */
public:
	static MM_EnvironmentStandard *newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *vmThread);
	
	virtual void flushNonAllocationCaches();
	virtual void flushGCCaches();
	
	virtual void initializeGCThread() {
		/* before a thread turning into a GC one, it shortly acted as a mutator (during thread attach sequence),
		 * which means it may have allocated or exacuted an object access barrier. */
		flushGCCaches();
	}

	MMINLINE static MM_EnvironmentStandard *getEnvironment(OMR_VMThread *omrVMThread) { return static_cast<MM_EnvironmentStandard*>(omrVMThread->_gcOmrVMThreadExtensions); }
	MMINLINE static MM_EnvironmentStandard *getEnvironment(MM_EnvironmentBase *env) { return static_cast<MM_EnvironmentStandard*>(env); }

	MMINLINE void setEvacuator(MM_Evacuator *evacuator) { _evacuator = evacuator; }
	MMINLINE MM_Evacuator *getEvacuator() { return _evacuator; }

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
		,_evacuator(NULL)
	{
		_typeId = __FUNCTION__;
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Flush (to global stores) the local remembered object caches in thread-local GC_Environment
	 */
	void flushRememberedSet()
	{
		MM_SublistFragment::flush(&_scavengerRememberedSet);
	}
#endif

protected:
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	virtual void tearDown(MM_GCExtensionsBase *extensions);
	
private:
	
};

#endif /* ENVIRONMENT_HPP_ */
