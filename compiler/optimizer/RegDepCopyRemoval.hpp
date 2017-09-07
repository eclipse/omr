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

#ifndef REGDEPCOPYREMOVAL_INCL
#define REGDEPCOPYREMOVAL_INCL

#include "env/TRMemory.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include <vector>

namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class TreeTop; }
namespace TR { class NodeChecklist; }
template <typename T> class TR_Array;

namespace TR
{

// Tries to remove redundant copies that are implicit in the commoning
// relationships within individual sets of outgoing register dependencies.
//
// Copies arise when the value of a single node has to be put into multiple
// registers. Because each node has its value held in a single virtual
// register, and no single virtual register can be identified with multiple
// distinct real registers at a particular program point, the evaluator is
// forced when generating postconditions to choose which real register wins.
// Local RA makes a good effort to put the node's result directly into the
// winning register, but the other needs a copy.
//
// It's common for register dependencies in general, and ones that cause this
// copying in particular, to be repeated in an extended block. In that case,
// the evaluator repeats the copy. But that copy has already been done!
//
// This optimization reifies the earlier copy as a node that will evaluate into
// a fresh virtual register, and then reuses the same copy node where possible
// in later dependencies to avoid repeating the same copies.

class RegDepCopyRemoval : public TR::Optimization
   {
   public:

   RegDepCopyRemoval(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) RegDepCopyRemoval(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private:

   enum RegDepState
      {
      REGDEP_ABSENT,          // Not specified in the current regdeps
      REGDEP_IGNORED,         // Specified, but ignore (e.g. register pairs)
      REGDEP_UNDECIDED,       // Preferred handling not yet determined
      REGDEP_NODE_ORIGINAL,   // Leave the reg dep unchanged
      REGDEP_NODE_FRESH_COPY, // Make a fresh copy for this reg dep
      REGDEP_NODE_REUSE_COPY, // Reuse a previous copy for this reg dep
      };

   // Data for a particular register, extracted from a GlRegDeps currently
   // under consideration.
   struct RegDepInfo
      {
      TR::Node *node;   // Toplevel [ai]RegLoad/PassThrough node for this register
      TR::Node *value;  // The node that supplies the value of the register
      RegDepState state;
      int childIndex;   // Position of this dependency within _regDeps
      };

   // The last node whose value was to be put into a register, along with the
   // actual node selected in order to provide that value (either the original,
   // or a copy).
   struct NodeChoice
      {
      TR::Node *original;
      TR::Node *selected;
      };

   const char *registerName(TR_GlobalRegisterNumber reg);
   void rangeCheckRegister(TR_GlobalRegisterNumber reg);
   RegDepInfo &getRegDepInfo(TR_GlobalRegisterNumber reg);
   NodeChoice &getNodeChoice(TR_GlobalRegisterNumber reg);

   void discardAllNodeChoices();
   void discardNodeChoice(TR_GlobalRegisterNumber reg);
   void rememberNodeChoice(TR_GlobalRegisterNumber reg, TR::Node *selected);

   void processRegDeps(TR::Node *deps, TR::TreeTop *depTT);
   void clearRegDepInfo();
   void readRegDeps();
   void ignoreRegister(TR_GlobalRegisterNumber reg);
   void selectNodesToReuse(TR::NodeChecklist &usedNodes);
   void selectNodesToCopy(TR::NodeChecklist &usedNodes);
   void updateRegDeps(TR::NodeChecklist &usedNodes);
   void reuseCopy(TR_GlobalRegisterNumber reg);
   void makeFreshCopy(TR_GlobalRegisterNumber reg);
   void updateSingleRegDep(TR_GlobalRegisterNumber reg, TR::Node *selected);
   void generateRegcopyDebugCounter(const char *category);

   const TR_GlobalRegisterNumber _regBegin;
   const TR_GlobalRegisterNumber _regEnd;
   std::vector<RegDepInfo, TR::typed_allocator<RegDepInfo, TR::Allocator> > _regDepInfoTable;
   std::vector<NodeChoice, TR::typed_allocator<NodeChoice, TR::Allocator> > _nodeChoiceTable;
   TR::TreeTop *_treetop;
   TR::Node *_regDeps;
   };

}

#endif // REGDEPCOPYREMOVAL_INCL
