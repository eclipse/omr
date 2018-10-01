/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "ilgen/BytecodeBuilderRecorder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "infra/Assert.hpp"
#include "compile/Compilation.hpp"
#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/JitBuilderRecorder.hpp"

OMR::BytecodeBuilderRecorder::BytecodeBuilderRecorder(TR::MethodBuilder *methodBuilder,
                                                      int32_t bcIndex,
                                                      char *name)
   : TR::IlBuilder(methodBuilder, methodBuilder->typeDictionary()),
   _bcIndex(bcIndex),
   _name(name),
   _initialVMState(0),
   _vmState(0)
   {
   TR::JitBuilderRecorder *rec = recorder();
   if (rec)
      {
      rec->BeginStatement(asBytecodeBuilder(), StatementName::STATEMENT_NEWILBUILDER);
      rec->Builder(asBytecodeBuilder());
      rec->EndStatement();
      }
   }

TR::BytecodeBuilder *
OMR::BytecodeBuilderRecorder::asBytecodeBuilder()
   {
   return static_cast<TR::BytecodeBuilder *>(this);
   }


void
OMR::BytecodeBuilderRecorder::addSuccessorBuilder(TR::BytecodeBuilder **b)
   {
   TR_ASSERT(*b != NULL, "cannot add null successor BytecodeBuilder");

   TR::BytecodeBuilder *savedBuilder = *b;
   transferVMState(b);            // may change what *b points at!

   _methodBuilder->addBytecodeBuilderToWorklist(savedBuilder);
   }

// Must be called *after* all code has been added to the bytecode builder
// Also, current VM state is assumed to be what should propagate to the fallthrough builder
void
OMR::BytecodeBuilderRecorder::AddFallThroughBuilder(TR::BytecodeBuilder *ftb)
   {
   TR_ASSERT(comesBack(), "builder does not appear to have a fall through path");

   addSuccessorBuilder(&ftb);
   }

// AddSuccessorBuilders() should be called with a list of TR::BytecodeBuilder ** pointers.
// Each one of these pointers could be changed by AddSuccessorBuilders() in the case where
// some operations need to be inserted along the control flow edges to synchronize the
// vm state from "this" builder to the target BytecodeBuilder. For this reason, the actual
// control flow edges should be created (i.e. with Goto, IfCmp*, etc.) *after* calling
// AddSuccessorBuilders, and the target used when creating those flow edges should take
// into account that AddSuccessorBuilders may change the builder object provided.
void
OMR::BytecodeBuilderRecorder::AddSuccessorBuilders(uint32_t numExits, ...)
   {
   va_list exits;
   va_start(exits, numExits);
   for (int32_t e=0;e < numExits;e++)
      {
      TR::BytecodeBuilder **builder = (TR::BytecodeBuilder **) va_arg(exits, TR::BytecodeBuilder **);
      addSuccessorBuilder(builder);
      }
   va_end(exits);
   }

void
OMR::BytecodeBuilderRecorder::propagateVMState(TR::VirtualMachineState *fromVMState)
   {
   _initialVMState = fromVMState->MakeCopy();
   _vmState = fromVMState->MakeCopy();
   }

// transferVMState needs to be called before the actual transfer operation (Goto, IfCmp,
// etc.) is created because we may need to insert a builder object along that control
// flow edge to synchronize the vm state at the target (in the case of a merge point).
// On return, the object pointed at by the "b" parameter may have changed. The caller
// should direct control for this edge to whatever the parameter passed to "b" is
// pointing at on return
void
OMR::BytecodeBuilderRecorder::transferVMState(TR::BytecodeBuilder **b)
   {
   TR_ASSERT(_vmState != NULL, "asked to transfer a NULL vmState from builder %p [ bci %d ]", this, bcIndex());
   TR::BytecodeBuilder *bcb = *b;
   if (bcb->initialVMState())
      {
      // there is already a vm state at the target builder
      // so we need to synchronize this builder's vm state with the target builder's vm state
      // for example, the local variables holding the elements on the operand stack may not match
      // create an intermediate builder object to do that work
      TR::BytecodeBuilder *intermediateBuilder = _methodBuilder->OrphanBytecodeBuilder(bcb->bcIndex(), bcb->name());

      _vmState->MergeInto(bcb->initialVMState(), intermediateBuilder);

      TR::IlBuilder *tgtb = bcb;
      intermediateBuilder->IlBuilder::Goto(&tgtb);

      *b = intermediateBuilder; // branches should direct towards intermediateBuilder not original *b
      }
   else
      {
      bcb->propagateVMState(_vmState);
      }
   }

void
OMR::BytecodeBuilderRecorder::Goto(TR::BytecodeBuilder **dest)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   Goto(*dest);
   }

void
OMR::BytecodeBuilderRecorder::Goto(TR::BytecodeBuilder *dest)
   {
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::Goto(dest);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpEqualZero(*dest, c);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpEqualZero(dest, c);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpNotEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpNotEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpNotEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpNotEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpNotEqualZero(TR::BytecodeBuilder **dest, TR::IlValue *c)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpNotEqualZero(*dest, c);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpNotEqualZero(TR::BytecodeBuilder *dest, TR::IlValue *c)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpNotEqualZero(dest, c);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpLessThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpLessThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedLessThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpUnsignedLessThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedLessThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedLessThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpLessOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpLessOrEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpUnsignedLessOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedLessOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedLessOrEqual(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpGreaterThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpGreaterThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedGreaterThan(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpUnsignedGreaterThan(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedGreaterThan(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedGreaterThan(dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpGreaterOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpGreaterOrEqual(dest, v1, v2);
   }
void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder **dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   if (*dest == NULL)
      *dest = _methodBuilder->OrphanBytecodeBuilder(_bcIndex, _name);
   IfCmpUnsignedGreaterOrEqual(*dest, v1, v2);
   }

void
OMR::BytecodeBuilderRecorder::IfCmpUnsignedGreaterOrEqual(TR::BytecodeBuilder *dest, TR::IlValue *v1, TR::IlValue *v2)
   {
   TR_ASSERT(dest != NULL, "service cannot be called with NULL destination builder");
   AddSuccessorBuilder(&dest);
   OMR::IlBuilder::IfCmpUnsignedGreaterOrEqual(dest, v1, v2);
   }
