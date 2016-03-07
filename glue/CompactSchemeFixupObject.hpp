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
 
#ifndef COMPACTSCHEMEOBJECTFIXUP_HPP_
#define COMPACTSCHEMEOBJECTFIXUP_HPP_

#include "omrcfg.h"
#include "objectdescription.h"

#include "CompactScheme.hpp"
#include "GCExtensionsBase.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)

class MM_CompactSchemeFixupObject {
public:
protected:
private:
	OMR_VM *_omrVM;
	MM_GCExtensionsBase *_extensions;
	MM_CompactScheme *_compactScheme;
public:

	/**
	 * Perform fixup for a single object
	 * @param env[in] the current thread
	 * @param objectPtr pointer to object for fixup
	 */
	void fixupObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);

	static void verifyForwardingPtr(omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr);

	MM_CompactSchemeFixupObject(MM_EnvironmentBase* env, MM_CompactScheme *compactScheme)
	:
		_omrVM(env->getOmrVM()),
		_extensions(env->getExtensions()),
		_compactScheme(compactScheme)
	{}

protected:
private:
};

#endif /* OMR_GC_MODRON_COMPACTION */

#endif /* COMPACTSCHEMEOBJECTFIXUP_HPP_ */
