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


#if !defined(SCANCLASSESMODE_HPP_)
#define SCANCLASSESMODE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "AtomicOperations.hpp"

#if defined(OMR_GC_DYNAMIC_CLASS_UNLOADING)

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_ScanClassesMode : public MM_Base
{
private:
	volatile uintptr_t _scanClassesMode;

public:
	MMINLINE ScanClassesMode getScanClassesMode() { return (ScanClassesMode)_scanClassesMode; }

	MMINLINE bool switchScanClassesMode(ScanClassesMode oldMode, ScanClassesMode newMode)
	{
		if ((oldMode == (ScanClassesMode)_scanClassesMode) &&
		    (oldMode == (ScanClassesMode)MM_AtomicOperations::lockCompareExchange(&_scanClassesMode,
																   (uintptr_t)oldMode, 
																   (uintptr_t)newMode))) {
			return true;
		}
		return false;
	};
	
	MMINLINE void setScanClassesMode(ScanClassesMode mode) {
		MM_AtomicOperations::set((uintptr_t *)&_scanClassesMode, (uintptr_t)mode);
	}
	
	/**
	 * Create a ScanClassesMode object.
	 */
	MM_ScanClassesMode() :
		MM_Base(),
		_scanClassesMode((uintptr_t)SCAN_CLASSES_DISABLED)
	{}

};

#endif	/* OMR_GC_DYNAMIC_CLASS_UNLOADING */

#endif /* SCANCLASSESMODE_HPP_ */
