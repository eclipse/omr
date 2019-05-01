/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef COMPACTLOCALS_INCL
#define COMPACTLOCALS_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "infra/IGNode.hpp"
#include "infra/vector.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BitVector;
class TR_HashTabInt;
class TR_InterferenceGraph;
class TR_Liveness;
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class ResolvedMethodSymbol; }
template <class T> class TR_Array;

/*
 * Class TR_CompactLocals
 * ======================
 *
 * Compact locals tries to minimize the stack size required by the 
 * compiled method. This is done by computing interferences between the 
 * live ranges of the locals/temps in the method before code generation 
 * is done; based on these interferences, locals that are never live 
 * simultaneously can be mapped on to the same stack slot. 
 * Liveness information is used by this analysis.
 */

class TR_CompactLocals : public TR::Optimization
   {
   TR_BitVector                   *_liveVars;
   TR_BitVector                   *_prevLiveVars;
   TR_BitVector                   *_temp;
   TR::vector<TR_IGNode *, TR::Region&> *_localIndexToIGNode;
   TR_InterferenceGraph           *_localsIG;
   TR_HashTabInt                  *_visit;
   TR_Array<TR::AutomaticSymbol *> *_callerLiveSyms;

   void processNodeInPreorder(TR::Node *root, vcount_t visitCount, TR_Liveness *liveLocals, TR::Block *block, bool directChildOfTreeTop);
   void createInterferenceBetween(TR_BitVector *bv);
   void createInterferenceBetween(TR_BitVector *bv1, TR_BitVector *bv2);
   void createInterferenceBetweenLocals(int32_t localIndex);
   void doCompactLocals();

public:
   TR_CompactLocals(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CompactLocals(manager);
      }

   bool eligibleLocal(TR::AutomaticSymbol * localSym);
   void assignColorsToSymbols(TR_BitVector *bv);

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();
   };

#endif
