/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/MethodBuilderImpl.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/BytecodeBuilderImpl.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"


OMR::BytecodeBuilder::BytecodeBuilder(TR::BytecodeBuilderImpl *impl, TR::MethodBuilder *mb, TR::TypeDictionary *types)
   : TR::IlBuilder(impl, mb, types)
   { }

TR::BytecodeBuilderImpl *
OMR::BytecodeBuilder::impl()
   {
   return static_cast<TR::BytecodeBuilderImpl *>(_impl);
   }

int32_t
OMR::BytecodeBuilder::bcIndex()
   {
   return impl()->bcIndex();
   }

OMR::VirtualMachineState *
OMR::BytecodeBuilder::initialVMState()
   {
   return impl()->initialVMState();
   }

char *
OMR::BytecodeBuilder::name()
   {
   return impl()->name();
   }

OMR::VirtualMachineState *
OMR::BytecodeBuilder::vmState()
   {
   return impl()->vmState();
   }

// Must be called *after* all code has been added to the bytecode builder
// Also, current VM state is assumed to be what should propagate to the fallthrough builder
void
OMR::BytecodeBuilder::AddFallThroughBuilder(TR::BytecodeBuilder *ftb)
   {
   impl()->AddFallThroughBuilder(ftb->impl());
   }

// AddSuccessorBuilders() should be called with a list of TR::BytecodeBuilder ** pointers.
// Each one of these pointers could be changed by AddSuccessorBuilders() in the case where
// some operations need to be inserted along the control flow edges to synchronize the
// vm state from "this" builder to the target BytecodeBuilder. For this reason, the actual
// control flow edges should be created (i.e. with Goto, IfCmp*, etc.) *after* calling
// AddSuccessorBuilders, and the target used when creating those flow edges should take
// into account that AddSuccessorBuilders may change the builder object provided.
void
OMR::BytecodeBuilder::AddSuccessorBuilders(uint32_t numExits, ...)
   {
   va_list exits;
   va_start(exits, numExits);
   for (int32_t e=0;e < numExits;e++)
      {
      TR::BytecodeBuilder **builder = (TR::BytecodeBuilder **) va_arg(exits, TR::BytecodeBuilder **);
      TR::BytecodeBuilderImpl *bbi = (*builder)->impl();
      impl()->AddSuccessorBuilder(&bbi);
      *builder = bbi->client();
      }
   va_end(exits);
   }

void
OMR::BytecodeBuilder::Goto(TR::BytecodeBuilder **dest)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   Goto(*dest);
   }

void
OMR::BytecodeBuilder::Goto(TR::BytecodeBuilder *dest)
   {
   AddSuccessorBuilder(&dest);
   this->OMR::IlBuilder::Goto(dest);
   }

void
OMR::BytecodeBuilder::IfCmpEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpEqualZero(*dest, c);
   }

void
OMR::BytecodeBuilder::IfCmpEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpEqualZero(dest, c);
   }

void
OMR::BytecodeBuilder::IfCmpNotEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpNotEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpNotEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpNotEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpNotEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpNotEqualZero(*dest, c);
   }

void
OMR::BytecodeBuilder::IfCmpNotEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpNotEqualZero(dest, c);
   }

void
OMR::BytecodeBuilder::IfCmpLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpLessThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpLessThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpUnsignedLessThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedLessThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpLessOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpLessOrEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpUnsignedLessOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedLessOrEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpGreaterThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpGreaterThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpUnsignedGreaterThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedGreaterThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpGreaterOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpGreaterOrEqual(dest, v1, v2);
   }
void
OMR::BytecodeBuilder::IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = methodBuilder()->OrphanBytecodeBuilder(impl()->bcIndex(), impl()->name());
   IfCmpUnsignedGreaterOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilder::IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedGreaterOrEqual(dest, v1, v2);
   }
