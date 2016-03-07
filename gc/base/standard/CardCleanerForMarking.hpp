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

#if !defined(CARDCLEANERFORMARKING_HPP_)
#define CARDCLEANERFORMARKING_HPP_

#include "omrcfg.h"

#if defined (OMR_GC_HEAP_CARD_TABLE)

#include "CardCleaner.hpp"
#include "CollectorLanguageInterface.hpp"
#include "EnvironmentStandard.hpp"
#include "HeapMapIterator.hpp"
#include "MarkingScheme.hpp"

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Modron_Standard
 */

class MM_CardCleanerForMarking : public MM_CardCleaner
{
public:
protected:
private:
	MM_MarkingScheme *_markingScheme;
public:
protected:
	/**
	 * Clean a range of addresses (typically within a span of a card)
	 * The class specifically is used for Incremental-Update style of marking
	 *
	 * @param[in] env A thread (typically the thread initializing the GC)
	 * @param[in] lowAddress low address of the range to be cleaned
	 * @param[in] highAddress high address of the range to be cleaned
	 */
	virtual void clean(MM_EnvironmentBase *envModron, void *lowAddress, void *highAddress, Card *cardToClean)
	{
		MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envModron);
		MM_GCExtensionsBase *extensions = env->getExtensions();

		/* card may be marked dirty in WP overflow, so it is important to mark it clean before any scan */
		*cardToClean = CARD_CLEAN;
		/* previous line value MUST be physically written to memory (relevant for concurrent cleaning) - force such write if necessary */
		MM_AtomicOperations::sync();

		MM_HeapMapIterator markedObjectIterator(extensions, _markingScheme->getMarkMap(), (uintptr_t *)lowAddress, (uintptr_t *)highAddress);
		omrobjectptr_t object = NULL;
		while (NULL != (object = markedObjectIterator.nextObject())) {
			_markingScheme->scanObject(env, object, MM_CollectorLanguageInterface::SCAN_REASON_OVERFLOWED_OBJECT);
		}
	}

	/**
	 * @see MM_CardCleaner::getVMStateID()
	 */
	virtual uintptr_t getVMStateID() { return J9VMSTATE_GC_CARD_CLEANER_FOR_MARKING; }
	
public:

	/**
	 * Create a CardCleaner object specific for Incremental-Update style of marking
	 */
	MM_CardCleanerForMarking(MM_MarkingScheme *markingScheme)
		: MM_CardCleaner()
		, _markingScheme(markingScheme)
	{
		_typeId = __FUNCTION__;
	}

private:
};

#endif /* defined (OMR_GC_HEAP_CARD_TABLE)*/

#endif /* CARDCLEANERFORMARKING_HPP_ */
