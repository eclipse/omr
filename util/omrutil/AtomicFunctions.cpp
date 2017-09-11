/*******************************************************************************
 * Copyright (c) 1998, 2015 IBM Corp. and others
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

#include "omrutilbase.h"
#include "AtomicSupport.hpp"

uintptr_t
compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue)
{
	return VM_AtomicSupport::lockCompareExchange(location, oldValue, newValue);
}

uint32_t
compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue)
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
 
uintptr_t
addAtomic(volatile uintptr_t *address, uintptr_t addend)
{
	return VM_AtomicSupport::add(address, addend);
}

uintptr_t
subtractAtomic(volatile uintptr_t *address, uintptr_t value)
{
	return VM_AtomicSupport::subtract(address, value);
}
