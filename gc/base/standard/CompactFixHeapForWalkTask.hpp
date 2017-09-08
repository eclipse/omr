/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

