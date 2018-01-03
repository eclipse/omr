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

#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "env/FrontEnd.hpp"
#include "infra/List.hpp"
#include "il/Block.hpp"
#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilderImpl.hpp"
#include "ilgen/IlValueImpl.hpp"
#include "ilgen/IlTypeImpl.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/MethodBuilderImpl.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/BytecodeBuilderImpl.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"


OMR::BytecodeBuilderImpl::BytecodeBuilderImpl(TR::MethodBuilderImpl *methodBuilder,
                                              int32_t bcIndex,
                                              char *name)
   : TR::IlBuilderImpl(methodBuilder, methodBuilder->typeDictionary()),
   _bcIndex(bcIndex),
   _name(name),
   _fallThroughBuilder(0),
   _initialVMState(0),
   _vmState(0)
   {
   _successorBuilders = new (PERSISTENT_NEW) List<TR::BytecodeBuilderImpl>(_types->trMemory());
   initialize(methodBuilder->details(), methodBuilder->methodSymbol(),
              methodBuilder->fe(), methodBuilder->symRefTab());
   initSequence();
   }

TR::BytecodeBuilder *
OMR::BytecodeBuilderImpl::client()
   {
   return static_cast<TR::BytecodeBuilder *>(_clientBuilder);
   }

void
OMR::BytecodeBuilderImpl::initialize(TR::IlGeneratorMethodDetails * details,
                                    TR::ResolvedMethodSymbol     * methodSymbol,
                                    TR::FrontEnd                 * fe,
                                    TR::SymbolReferenceTable     * symRefTab)
    {
    this->OMR::IlInjector::initialize(details, methodSymbol, fe, symRefTab);

    //addBytecodeBuilderToList relies on _comp and it won't be ready until now
    _methodBuilder->addToAllBytecodeBuildersList(static_cast<TR::BytecodeBuilderImpl *>(this));
    }

void
OMR::BytecodeBuilderImpl::appendBlock(TR::Block *block, bool addEdge)
   {
   if (block == NULL)
      {
      block = emptyBlock();
      }

   block->setByteCodeIndex(bcIndex(), _comp);
   return TR::IlBuilderImpl::appendBlock(block, addEdge);
   }

uint32_t
OMR::BytecodeBuilderImpl::countBlocks()
   {
   // only count each block once
   if (_count > -1)
      return _count;

   TraceIL("[ %p ] TR::BytecodeBuilderImpl::countBlocks 0\n", this);

   _count = TR::IlBuilderImpl::countBlocks();

   if (NULL != _fallThroughBuilder)
      _methodBuilder->addToBlockCountingWorklist(_fallThroughBuilder);

   ListIterator<TR::BytecodeBuilderImpl> iter(_successorBuilders);
   for (TR::BytecodeBuilderImpl *builder = iter.getFirst(); !iter.atEnd(); builder = iter.getNext())
      if (builder->_count < 0)
         _methodBuilder->addToBlockCountingWorklist(builder);

   TraceIL("[ %p ] TR::BytecodeBuilderImpl::countBlocks %d\n", this, _count);

   return _count;
   }

bool
OMR::BytecodeBuilderImpl::connectTrees()
   {
   if (!_connectedTrees)
      {
      TraceIL("[ %p ] TR::BytecodeBuilderImpl::connectTrees\n", this);
      bool rc = TR::IlBuilderImpl::connectTrees();
      addAllSuccessorBuildersToWorklist();
      return rc;
      }

   return true;
   }


void
OMR::BytecodeBuilderImpl::addAllSuccessorBuildersToWorklist()
   {
   if (NULL != _fallThroughBuilder)
      _methodBuilder->addToTreeConnectingWorklist(_fallThroughBuilder);

   // iterate over _successorBuilders
   ListIterator<TR::BytecodeBuilderImpl> iter(_successorBuilders);
   for (TR::BytecodeBuilderImpl *builder = iter.getFirst(); !iter.atEnd(); builder = iter.getNext())
      _methodBuilder->addToTreeConnectingWorklist(builder);
   }

// Must be called *after* all code has been added to the bytecode builder
// Also, current VM state is assumed to be what should propagate to the fallthrough builder
void
OMR::BytecodeBuilderImpl::AddFallThroughBuilder(TR::BytecodeBuilderImpl *ftb)
   {
   TR_ASSERT(comesBack(), "builder does not appear to have a fall through path");

   TraceIL("IlBuilder[ %p ]:: fallThrough successor [ %p ]\n", this, ftb);

   TR::BytecodeBuilderImpl *b = ftb;
   transferVMState(&b);    // may change what b points at!

   if (b != ftb)
      TraceIL("IlBuilder[ %p ]:: fallThrough successor changed to [ %p ]\n", this, b);

   _fallThroughBuilder = b;
   _methodBuilder->addBytecodeBuilderToWorklist(ftb);

   // add explicit goto and register the actual fall-through block
   TR::IlBuilderImpl *tgtb = b;
   TR::IlBuilderImpl::Goto(&tgtb);
   }

// AddSuccessorBuilders() should be called with a list of TR::BytecodeBuilderImpl ** pointers.
// Each one of these pointers could be changed by AddSuccessorBuilders() in the case where
// some operations need to be inserted along the control flow edges to synchronize the
// vm state from "this" builder to the target BytecodeBuilderImpl. For this reason, the actual
// control flow edges should be created (i.e. with Goto, IfCmp*, etc.) *after* calling
// AddSuccessorBuilders, and the target used when creating those flow edges should take
// into account that AddSuccessorBuilders may change the builder object provided.
void
OMR::BytecodeBuilderImpl::AddSuccessorBuilder(TR::BytecodeBuilderImpl **builder)
   {
   if ((*builder)->bcIndex() < bcIndex()) //If the successor has a bcIndex < than the current bcIndex this may be a loop
      _methodSymbol->setMayHaveLoops(true);
   transferVMState(builder);            // may change what builder points at!
   _successorBuilders->add(*builder);   // must be the bytecode builder that comes back from transferVMState()
   _methodBuilder->addBytecodeBuilderToWorklist(*builder);
   TraceIL("IlBuilder[ %p ]:: successor [ %p ]\n", this, *builder);
   }

/* TODO: why isn't this in IlBuilderImpl? */
void
OMR::BytecodeBuilderImpl::setHandlerInfo(uint32_t catchType)
   {
   TR::Block *catchBlock = getEntry();
   catchBlock->setIsCold();
   catchBlock->setHandlerInfo(catchType, comp()->getInlineDepth(), -1, _methodSymbol->getResolvedMethod(), comp());
   }

void
OMR::BytecodeBuilderImpl::propagateVMState(OMR::VirtualMachineState *fromVMState)
   {
   _initialVMState = (OMR::VirtualMachineState *) fromVMState->MakeCopy();
   _vmState = (OMR::VirtualMachineState *) fromVMState->MakeCopy();
   }

// transferVMState needs to be called before the actual transfer operation (Goto, IfCmp,
// etc.) is created because we may need to insert a builder object along that control
// flow edge to synchronize the vm state at the target (in the case of a merge point).
// On return, the object pointed at by the "b" parameter may have changed. The caller
// should direct control for this edge to whatever the parameter passed to "b" is
// pointing at on return
void
OMR::BytecodeBuilderImpl::transferVMState(TR::BytecodeBuilderImpl **b)
   {
   TR_ASSERT(_vmState != NULL, "asked to transfer a NULL vmState from builder %p [ bci %d ]", this, bcIndex());
   if ((*b)->initialVMState())
      {
      // there is already a vm state at the target builder
      // so we need to synchronize this builder's vm state with the target builder's vm state
      // for example, the local variables holding the elements on the operand stack may not match
      // create an intermediate builder object to do that work
      TR::BytecodeBuilderImpl *intermediateBuilder = _methodBuilder->orphanBytecodeBuilderImpl((*b)->bcIndex(), (*b)->_name);

      _vmState->MergeInto((*b)->initialVMState(), intermediateBuilder->client());
      TraceIL("IlBuilder[ %p ]:: transferVMState merged vm state on way to [ %p ] using [ %p ]\n", this, *b, intermediateBuilder);

      TR::IlBuilderImpl *tgtb = *b;
      intermediateBuilder->IlBuilderImpl::Goto(&tgtb);
      intermediateBuilder->_fallThroughBuilder = *b;
      TraceIL("IlBuilder[ %p ]:: fallThrough successor [ %p ]\n", intermediateBuilder, *b);
      *b = intermediateBuilder; // branches should direct towards intermediateBuilder not original *b
      }
   else
      {
      (*b)->propagateVMState(_vmState);
      }
   }
