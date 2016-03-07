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

#if !defined(COMPACTTASK_HPP_)
#define COMPACTTASK_HPP_

#include "objectdescription.h"

#include "ParallelTask.hpp"

#if defined(OMR_GC_MODRON_COMPACTION)

class MM_Dispatcher;
class MM_CompactScheme;
class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ParallelCompactTask : public MM_ParallelTask
{
private:
	MM_CompactScheme *_compactScheme;
	bool _rebuildMarkBits;
	bool _aggressive;

public:
	virtual uintptr_t getVMStateID();
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	/**
	 * Create an ParallelCompactTask object.
	 */
	MM_ParallelCompactTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher, MM_CompactScheme *compactScheme, bool rebuildMarkBits, bool aggressive) :
		MM_ParallelTask(env, dispatcher),
		_compactScheme(compactScheme),
		_rebuildMarkBits(rebuildMarkBits),
		_aggressive(aggressive)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* OMR_GC_MODRON_COMPACTION */

#endif /* COMPACTTASK_HPP_ */
