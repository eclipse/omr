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

#ifndef OMR_TREETOP_INCL
#define OMR_TREETOP_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREETOP_CONNECTOR
#define OMR_TREETOP_CONNECTOR
namespace OMR { class TreeTop; }
namespace OMR { typedef OMR::TreeTop TreeTopConnector; }
#endif

#include <stddef.h>
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"

namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class SymbolReference; }
namespace TR { class TreeTop; }

namespace OMR
{

class OMR_EXTENSIBLE TreeTop
   {

public:
   friend class TR_DebugExt;
   TR_ALLOC_WITHOUT_NEW(TR_Memory::TreeTop)

   /// Downcast to concrete type
   TR::TreeTop * self();

   static TR::TreeTop *create(TR::Compilation *comp);
   static TR::TreeTop *create(TR::Compilation *comp, TR::Node *node, TR::TreeTop *next = NULL, TR::TreeTop *prev = NULL);
   static TR::TreeTop *create(TR::Compilation *comp, TR::TreeTop *precedingTreeTop, TR::Node *node);

   static TR::TreeTop *createIncTree  (TR::Compilation * comp, TR::Node *, TR::SymbolReference *, int32_t incAmount, TR::TreeTop *precedingTreeTop = NULL, bool isRecompCounter = false);
   static TR::TreeTop *createResetTree(TR::Compilation * comp, TR::Node *, TR::SymbolReference *, int32_t resetAmount, TR::TreeTop *precedingTreeTop = NULL, bool isRecompCounter = false);

   TR::TreeTop *duplicateTree();

   static void insertTreeTops(TR::Compilation *comp, TR::TreeTop* beforeInsertionPoint, TR::TreeTop *firstTree, TR::TreeTop *lastTree);
   void insertTreeTopsAfterMe(TR::TreeTop *firstTree, TR::TreeTop *lastTree = NULL);
   void insertTreeTopsBeforeMe(TR::TreeTop *firstTree, TR::TreeTop *lastTree = NULL);
   static void removeDeadTrees(TR::Compilation * comp, TR::TreeTop* list[]);
   static void removeDeadTrees(TR::Compilation * comp, TR::TreeTop* first, TR::TreeTop* last);

   void * operator new(size_t s, bool trace, TR_Memory *m);
   void operator delete(void *ptr, bool trace, TR_Memory *m);

   explicit TreeTop(
            TR::Node  *node = NULL,
            TR::TreeTop *next = NULL,
            TR::TreeTop *prev = NULL) :
   _pNext(next), _pPrev(prev), _pNode(node) {} ;

   TreeTop(TR::TreeTop *precedingTreeTop, TR::Node *node, TR::Compilation *c);

   TR::TreeTop *getNextTreeTop();
   TR::TreeTop *setNextTreeTop(TR::TreeTop *p);
   TR::TreeTop *getPrevTreeTop();
   TR::TreeTop *setPrevTreeTop(TR::TreeTop *p);
   TR::Node    *getNode();
   TR::Node    *setNode(TR::Node *p);

   void join(TR::TreeTop * p);
   TR::TreeTop* insertAfter(TR::TreeTop *tt);  // tt is inserted after this
   TR::TreeTop* insertBefore(TR::TreeTop *tt); // tt is inserted before this
   void unlink(bool decRefCountRecursively);   // unlink and single out "this" tree; only one tree is unlinked

   bool isPossibleDef();

   void insertNewTreeTop(TR::TreeTop *beforeNewTreeTop, TR::TreeTop * afterNewTreeTop);

   TR::TreeTop *getExtendedBlockExitTreeTop();
   TR::Block   *getEnclosingBlock(bool forward=false);
   TR::TreeTop *getNextRealTreeTop();
   TR::TreeTop *getPrevRealTreeTop();

   bool isLegalToChangeBranchDestination(TR::Compilation *);
   bool adjustBranchOrSwitchTreeTop(TR::Compilation *, TR::TreeTop *, TR::TreeTop *);

   TR::Instruction *getLastInstruction() {return *(TR::Instruction **)((char *)self() - sizeof(void *)); }
   void setLastInstruction(TR::Instruction *i){ *(TR::Instruction **)((char *)self() - sizeof(void *)) = i;}

protected:
   TR::TreeTop * _pNext;
   TR::TreeTop * _pPrev;
   TR::Node    * _pNode;

   };

}

#endif
