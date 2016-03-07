/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1998, 2015
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

#include "omrutilbase.h"
#include "AtomicSupport.hpp"

uintptr_t
compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue, uintptr_t *spinlock)
{
	return VM_AtomicSupport::lockCompareExchange(location, oldValue, newValue);
}

uintptr_t
compareAndSwapUDATANoSpinlock(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue)
{
	return VM_AtomicSupport::lockCompareExchange(location, oldValue, newValue);
}

uint32_t
compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue, uintptr_t *spinlock)
{
	return VM_AtomicSupport::lockCompareExchangeU32(location, oldValue, newValue);
}

uint32_t
compareAndSwapU32NoSpinlock(uint32_t *location, uint32_t oldValue, uint32_t newValue)
{
	return VM_AtomicSupport::lockCompareExchangeU32(location, oldValue, newValue);
}

void
issueReadBarrier(void)
{
	VM_AtomicSupport::readBarrier();
}

void
issueReadWriteBarrier(void)
{
	VM_AtomicSupport::readWriteBarrier();
}

void
issueWriteBarrier(void)
{
	VM_AtomicSupport::writeBarrier();
}
