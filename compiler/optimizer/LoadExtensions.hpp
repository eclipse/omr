/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef LOADEXTENSIONS_HPP_
#define LOADEXTENSIONS_HPP_

#include <stdint.h>                           // for int32_t
#include "codegen/CodeGenerator.hpp"          // for CodeGenerator
#include "compile/Compilation.hpp"            // for Compilation
#include "il/DataTypes.hpp"                   // for TR::DataType
#include "il/ILOps.hpp"                       // for ILOpCode
#include "il/Node.hpp"                        // for Node, etc
#include "il/Node_inlines.hpp"                // for Node::getType
#include "il/Symbol.hpp"                      // for Symbol
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/Optimization_inlines.hpp" // for Optimization inlines
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_BitVector;
class TR_UseDefInfo;

class TR_LoadExtensions : public TR::Optimization
   {
private:
   int32_t * _counts;
   int _seenLoads;
   TR_BitVector *_signExtensionFlags;
   TR_UseDefInfo *_useDefInfo;

   /* \brief
    *    Keeps track of all nodes which should be excluded from consideration in this optimization.
    */
   TR::SparseBitVector _excludedNodes;

   enum overrideFlags
      {
      narrowOverride  = 0x01,
      regLoadOverride = 0x02,
      totalFlags      = 0x04
      };

public:
   // TR_ALLOC(TR_Memory::Optimization) will stack-allocate for now

   TR_LoadExtensions(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoadExtensions(manager);
      }

   int32_t perform();
   virtual const char * optDetailString() const throw();

   inline TR_BitVector * getFlags() { return _signExtensionFlags; }
   static bool supportedConstLoad(TR::Node *load, TR::Compilation * c);
   static bool supportedType(TR::Node* node)
      {
      //The z codegen doesn't generate the correct instructions (yet) to allow this
      //TR::comp()->getOption(TR_DisableDirectStaticAccessOnZ) &&

      if ( !TR::comp()->cg()->getAccessStaticsIndirectly() && node->getOpCode().hasSymbolReference() && node->getSymbol()->isStatic() &&
           node->getOpCode().isLoadDirect() && ! (node->getOpCode().isInt() || node->getOpCode().isLong()))
         return false;

      return node->getType().isIntegral() || node->getType().isAddress();
      }
private:
   inline bool countIsSigned(TR::Node* load)
      {
      if (load->getType().isAddress() || load->getType().isAggregate())
         return false;
      else
         return _counts[load->getGlobalIndex()]/totalFlags >= 0; //bias towards signed load
      }

   inline void setOverrideOpt(TR::Node* node, overrideFlags overrideType)
      {
      if (trace()) traceMsg(comp(), "Setting override on node %p at %d\n", node, node->getGlobalIndex());
      _counts[node->getGlobalIndex()] |= overrideType;
      }

   inline bool isOverriden(TR::Node* node, overrideFlags overrideType)
      {
      if (trace()) traceMsg(comp(), "Checking override on node %p at %d\n", node, node->getGlobalIndex());
      return _counts[node->getGlobalIndex()] & overrideType;
      }

   void countLoad(TR::Node* load, TR::Node* conversion);
   void countLoadExtensions(TR::Node *parent, vcount_t visitCount);
   bool detectUnneededConversionPattern(TR::Node* conversion, TR::Node* child, bool& mustForce64);
   bool detectReverseNeededConversionPattern(TR::Node* parent, TR::Node* conversion);
   void setPreferredExtension(TR::Node *node, vcount_t visitCount);
   ncount_t indexNodesForCodegen(TR::Node *parent, ncount_t nodeCount, vcount_t visitCount);
   };

#endif /* LOADEXTENSIONS_HPP_ */
