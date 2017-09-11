/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "env/PersistentAllocator.hpp"

OMR::PersistentAllocator::PersistentAllocator(const TR::PersistentAllocatorKit &allocatorKit) :
   _rawAllocator(allocatorKit.rawAllocator)
   {
   }

void *
OMR::PersistentAllocator::allocate(size_t size, const std::nothrow_t tag, void * hint) throw()
   {
   return _rawAllocator.allocate(size, tag, hint);
   }

void *
OMR::PersistentAllocator::allocate(size_t size, void * hint)
   {
   return _rawAllocator.allocate(size, hint);
   }

void
OMR::PersistentAllocator::deallocate(void * p, const size_t sizeHint) throw()
   {
   _rawAllocator.deallocate(p, sizeHint);
   }
