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

#if DEBUG

#ifndef DOMINATORVERIFY_INCL
#define DOMINATORVERIFY_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "optimizer/DominatorsChk.hpp"

class TR_BitVector;
class TR_Dominators;
namespace TR { class Block; }

// This class can be used to verify :
// 1. If the Dominators info computed by the simple O(n^2) algorithm
//    in Muchnick is correct. The Dominator info for each node is
//    verified by comparing with the dominator tree.
// 2. If the Dominators computed by the efficient algorithm and the
//    expensive O(n^2) algorithm are the same. The immediate
//    dominators computed by each of the algorithms are compared for each node
//    in the flow graph. Note that if the expensive algorithm is correct
//    (Verification 1) and the two algorithms are consistent
//    (Verification 2), then the efficient algorithm is proven correct.
//
class TR_DominatorVerifier
   {
   public:
   TR_ALLOC(TR_Memory::DominatorVerifier)

   TR_DominatorVerifier(TR_Dominators&);

   private:

   bool    bothImplementationsConsistent;
   bool    expensiveAlgorithmCorrect;

   bool    areBothImplementationsConsistent(TR_DominatorsChk&, TR_Dominators&);
   bool    isExpensiveAlgorithmCorrect(TR_DominatorsChk&);
   bool    dominates(TR::Block *, TR::Block *);
   void    compareWithPredsOf(TR::Block *, TR::Block *);

   TR::Compilation * comp()          {return _compilation;}
   TR_Memory *      trMemory()      { return comp()->trMemory(); }
   TR_StackMemory   trStackMemory() { return trMemory(); }

   TR::Compilation              *_compilation;
   TR_DominatorsChk::BBInfoChk *_dominatorsChkInfo;
   TR_BitVector                *_nodesSeenOnEveryPath;
   TR_BitVector                *_nodesSeenOnCurrentPath;
   int32_t                      _numBlocks;
   vcount_t                      _visitCount;
   TR_Dominators               *_dominators;
   };

#endif

#endif
