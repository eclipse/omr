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

#ifndef BYTECODEITERATOR_INCL
#define BYTECODEITERATOR_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "infra/Link.hpp"

class TR_Memory;
namespace TR { class Block; }
namespace TR { class ResolvedMethodSymbol; }


template <typename ByteCode, typename ResolvedMethod> class TR_ByteCodeIterator
   {
public:
   TR_ByteCodeIterator(TR::ResolvedMethodSymbol *resolvedMethodSym, ResolvedMethod *m, TR::Compilation *comp)
      : _methodSymbol(resolvedMethodSym),
        _method(m),
        _compilation(comp),
        _trMemory(comp->trMemory()),
        _maxByteCodeIndex(m->maxBytecodeIndex())
      { }

   struct TryCatchInfo : TR_Link<TryCatchInfo>
      {
      void *operator new(size_t s, void *p) { return p; }

      TryCatchInfo(uint16_t s, uint16_t e, uint16_t h, uint32_t c) :
         _startIndex(s),
         _endIndex(e),
         _handlerIndex(h),
         _catchType(c),
         _firstBlock(0),
         _lastBlock(0),
         _catchBlock(0)
         {
         }

      void initialize(uint16_t s, uint16_t e, uint16_t h, uint32_t c)
         {
         _startIndex = s;
         _endIndex = e;
         _handlerIndex = h;
         _catchType = c;
         }

      bool operator==(TryCatchInfo & o)
         {
         return _handlerIndex == o._handlerIndex && _catchType == o._catchType;
         }

      uint16_t      _startIndex;
      uint16_t      _endIndex;
      uint16_t      _handlerIndex;
      uint32_t      _catchType;
      TR::Block    * _firstBlock;
      TR::Block    * _lastBlock;
      TR::Block    * _catchBlock;
      };

   TR::Compilation *comp()                const       { return _compilation; }
   TR_Memory *trMemory()                   const       { return _trMemory; }
   virtual TR::ResolvedMethodSymbol *methodSymbol() const   { return _methodSymbol; }
   ResolvedMethod          *method()       const       { return _method; }

   int32_t  bcIndex()                      const       { return _bcIndex; }
   void     setIndex(int32_t i)                        { _bcIndex = i; }

   int32_t  maxByteCodeIndex()             const       { return _maxByteCodeIndex; }



   // Abstract Methods - subclasses must provide these
   ByteCode first();
   ByteCode next();
   ByteCode current();

protected:
   // Abstract Methods - subclasses must provide these
   void printByteCodePrologue();
   void printByteCode();
   void printByteCodeEpilogue();

   bool isBranch();
   int32_t branchDesination(int32_t base);

   TR::ResolvedMethodSymbol * _methodSymbol;
   ResolvedMethod          * _method;
   TR::Compilation        * _compilation;
   TR_Memory               * _trMemory;
   int32_t             const _maxByteCodeIndex;


   int32_t                   _bcIndex;
   };

#endif
