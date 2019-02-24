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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/FrontEnd.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/NJIlGenerator.hpp"
#include "infra/Cfg.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "ras/ILValidator.hpp"

#define OPT_DETAILS "O^O ILGEN: "

OMR::IlGenerator::IlGenerator()
   : TR_IlGenerator(),
   _comp(NULL),
   _fe(NULL),
   _symRefTab(NULL),
   _details(NULL),
   _methodSymbol(NULL)
   {
   }

void
OMR::IlGenerator::initialize(TR::IlGeneratorMethodDetails * details,
                            TR::ResolvedMethodSymbol     * methodSymbol,
                            TR::FrontEnd                 * fe,
                            TR::SymbolReferenceTable     * symRefTab)
   {
   _details = details;
   _methodSymbol = methodSymbol;
   _fe = fe;
   _symRefTab = symRefTab;
   _comp = TR::comp();
   }

TR::CFG *
OMR::IlGenerator::cfg()
   {
   return _methodSymbol->getFlowGraph();
   }

bool
OMR::IlGenerator::genIL()
   {
   _comp->reportILGeneratorPhase();

   TR::StackMemoryRegion stackMemoryRegion(*_comp->trMemory());

   _comp->setCurrentIlGenerator(this);
   bool success = injectIL();
   _comp->setCurrentIlGenerator(0);

#if !defined(DISABLE_CFG_CHECK)
   if (success && _comp->getOption(TR_UseILValidator))
      {
      /* Setup the ILValidator for the current Compilation Thread. */
      _comp->setILValidator(createILValidatorObject(_comp));
      }
#endif

   return success;
   }


// following are common helpers to inject IL

TR::TreeTop *
OMR::IlGenerator::genTreeTop(TR::Node *n)
   {
   if (!n->getOpCode().isTreeTop())
      n = TR::Node::create(TR::treetop, 1, n);
   return getCurrentBlock()->append(TR::TreeTop::create(comp(), n));
   }

TR::SymbolReference *
OMR::IlGenerator::newTemp(TR::DataType dt)
   {
   return symRefTab()->createTemporary(_methodSymbol, dt);
   }

TR::Node *
OMR::IlGenerator::parameter(int32_t slot, TR::DataType dt)
   {
   return TR::Node::createLoad(symRefTab()->findOrCreateAutoSymbol(_methodSymbol, slot, dt, true, false, true));
   }

TR::Node *
OMR::IlGenerator::iconst(int32_t value)
   {
   return TR::Node::iconst(value);
   }

TR::Node *
OMR::IlGenerator::lconst(int64_t value)
   {
   return TR::Node::lconst(value);
   }

TR::Node *
OMR::IlGenerator::bconst(int8_t value)
   {
   return TR::Node::bconst(value);
   }

TR::Node *
OMR::IlGenerator::sconst(int16_t value)
   {
   return TR::Node::sconst(value);
   }

TR::Node *
OMR::IlGenerator::aconst(uintptrj_t value)
   {
   return TR::Node::aconst(value);
   }

TR::Node *
OMR::IlGenerator::dconst(double value)
   {
   TR::Node *r = TR::Node::create(0, TR::dconst);
   r->setDouble(value);
   return r;
   }

TR::Node *
OMR::IlGenerator::fconst(float value)
   {
   TR::Node *r = TR::Node::create(0, TR::fconst);
   r->setFloat(value);
   return r;
   }

TR::Node *
OMR::IlGenerator::staticAddress(void *address)
   {
   return TR::Node::aconst((uintptrj_t)address);
   }

void
OMR::IlGenerator::storeToTemp(TR::SymbolReference *tempSymRef, TR::Node *value)
   {
   genTreeTop(TR::Node::createStore(tempSymRef, value));
   }


TR::Node *
OMR::IlGenerator::loadTemp(TR::SymbolReference *tempSymRef)
   {
   return TR::Node::createLoad(tempSymRef);
   }

/*
 * Note:
 * This function should be removed and tests depending on this should be using
 * OMR::Node::createWithoutSymRef() instead once it is available publicly.
 * */
// TR::Node *
// OMR::IlGenerator::createWithoutSymRef(TR::ILOpCodes opCode, uint16_t numArgs, ...)
//    {
//    TR_ASSERT(numArgs > 0, "Must be called with at least one child, but numChildArgs = %d", numArgs);
//    va_list args;
//    va_start(args, numArgs);
//    TR::Node * result = TR::Node::create(opCode, numArgs);
//    for (int i = 0; i < numArgs; ++i)
//       {
//       TR::Node * child = va_arg(args, TR::Node *);
//       TR_ASSERT(child != NULL, "Child %d must be non NULL", i);
//       result->setAndIncChild(i,child);
//       }
//    return result;
//    }
