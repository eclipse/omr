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

#ifndef INLINEBLOCK_INCL
#define INLINEBLOCK_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"

class TR_FrontEnd;
namespace TR { class Block; }
namespace TR { class Compilation; }
namespace TR { class TreeTop; }
template <class T> class List;

class TR_InlineBlock
   {
   public:

   TR_ALLOC(TR_Memory::Inliner);

   TR_InlineBlock(int32_t BCIndex, int32_t originalBlockNum) : _BCIndex(BCIndex), _originalBlockNum(originalBlockNum) {};

   int32_t _BCIndex;
   int32_t _originalBlockNum;
   };

class TR_InlineBlocks
   {
   public:

   friend class TR_InlinerTracer;

   TR_ALLOC(TR_Memory::Inliner);

   TR_InlineBlocks(TR_FrontEnd *, TR::Compilation * );
   void addBlock(TR::Block *);
   void addExceptionBlock(TR::Block *);
   bool isInList(int32_t);
   bool isInExceptionList(int32_t);
   void dump(TR::FILE *);

   int32_t getNumBlocks() { return _numBlocks; }
   int32_t getNumExceptionBlocks() { return _numExceptionBlocks;}

   void setCallNodeTreeTop(TR::TreeTop *tt) { _callNodeTreeTop = tt; }
   TR::TreeTop *getCallNodeTreeTop() { return _callNodeTreeTop; }

   bool hasGeneratedRestartTree() { return _generatedRestartTree != NULL;}
   TR::TreeTop *getGeneratedRestartTree() { return _generatedRestartTree;}
   TR::TreeTop *setGeneratedRestartTree(TR::TreeTop *tt) { _generatedRestartTree=tt; return tt; }

   void setLowestBCIndex(int32_t i) { _lowestBCIndex=i; }
   void setHighestBCIndex(int32_t i) { _highestBCIndex=i; }
   int32_t getLowestBCIndex() { return _lowestBCIndex; }
   int32_t getHighestBCIndex() { return _highestBCIndex; }

   // Public due to access requirement of Inliner Tracer.
   List<TR_InlineBlock> *_inlineBlocks;
   List<TR_InlineBlock> *_exceptionBlocks;

   private:

   int32_t _numBlocks;
   int32_t _numExceptionBlocks;
   TR::TreeTop *_callNodeTreeTop;
   TR::TreeTop *_generatedRestartTree;

   int32_t _lowestBCIndex;
   int32_t _highestBCIndex;

   TR::Compilation *_comp;
   TR_FrontEnd *fe;
   };
#endif
