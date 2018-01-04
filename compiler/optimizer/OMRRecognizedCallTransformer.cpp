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

#include "optimizer/RecognizedCallTransformer.hpp"

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Checklist.hpp"       // for NodeChecklist, etc
#include "codegen/CodeGenerator.hpp" // for CodeGenerator

TR::Optimization* OMR::RecognizedCallTransformer::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::RecognizedCallTransformer(manager);
   }

int32_t OMR::RecognizedCallTransformer::perform()
   {
   TR::NodeChecklist visited(comp());
   for (auto treetop = comp()->getMethodSymbol()->getFirstTreeTop(); treetop != NULL; treetop = treetop->getNextTreeTop())
      {
      if (treetop->getNode()->getNumChildren() > 0)
         {
         auto node = treetop->getNode()->getFirstChild();
         if (node && node->getOpCode().isCall() && !visited.contains(node))
            {
            if (isInlineable(treetop) &&
                performTransformation(comp(), "%s Transforming recognized call node [" POINTER_PRINTF_FORMAT "]\n", optDetailString(), node))
               {
               visited.add(node);
               transform(treetop);
               }
            }
         }
      }
   return 0;
   }

const char* OMR::RecognizedCallTransformer::optDetailString() const throw()
   {
   return "O^O RECOGNIZED CALL TRANSFORMER:";
   }
