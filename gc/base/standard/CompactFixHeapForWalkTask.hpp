/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2014
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

#if !defined(COMPACTFIXHEAPFORWALKTASK_HPP_)
#define COMPACTFIXHEAPFORWALKTASK_HPP_

#include "omrmodroncore.h"

#include "ParallelTask.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)

class MM_CompactScheme;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CompactFixHeapForWalkTask : public MM_ParallelTask
{
private:
	MM_CompactScheme *_compactScheme;

public:
	virtual uintptr_t getVMStateID() { return J9VMSTATE_GC_COMPACT_FIX_HEAP_FOR_WALK; };
	
	virtual void run(MM_EnvironmentBase *env);

	MM_CompactFixHeapForWalkTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_CompactScheme *compactScheme) :
		MM_ParallelTask(env, dispatcher),
		_compactScheme(compactScheme)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* OMR_GC_MODRON_COMPACTION */

#endif /* COMPACTFIXHEAPFORWALKTASK_HPP_ */

