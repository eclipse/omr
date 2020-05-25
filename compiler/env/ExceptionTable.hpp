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

#ifndef EXCEPTIONTABLE_INCL
#define EXCEPTIONTABLE_INCL

#include "infra/Array.hpp"

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/List.hpp"

class TR_ResolvedMethod;
namespace TR { class Block; }
namespace TR { class Compilation; }
template <class T> class TR_Array;

struct TR_PCMapEntry
   {
   TR_ALLOC(TR_Memory::PCMapEntry)

   uint16_t _instructionStartPC;
   uint16_t _instructionEndPC;
   uint16_t _bytecodeStartPC;
   uint16_t _bytecodeEndPC;
   };

struct TR_ExceptionTableEntry
   {
   TR_ALLOC(TR_Memory::ExceptionTableEntry)

   TR_ResolvedMethod *   _method;
   uint32_t              _instructionStartPC;
   uint32_t              _instructionEndPC;
   uint32_t              _instructionHandlerPC;
   uint32_t              _catchType;
   bool                  _isSyntheticHandler; // indicate whether the exception handler is inserted by the compiler rather than existing in the source code
   TR_ByteCodeInfo       _byteCodeInfo;
   };

struct TR_ExceptionTableEntryIterator
   {
   TR_ALLOC(TR_Memory::ExceptionTableEntryIterator)

   TR_ExceptionTableEntryIterator(TR::Compilation *comp);

   TR_ExceptionTableEntry * getFirst();
   TR_ExceptionTableEntry * getNext();

   uint32_t                 size();
private:
   TR_ExceptionTableEntry * getCurrent();
   void                     addSnippetRanges(List<TR_ExceptionTableEntry> &, TR::Block *, TR::Block *, uint32_t, TR_ResolvedMethod *, TR::Compilation *);

   TR::Compilation *                          _compilation;
   TR_Array<List<TR_ExceptionTableEntry> > * _tableEntries;
   ListIterator<TR_ExceptionTableEntry>      _entryIterator;
   int32_t                                   _inlineDepth;
   uint32_t                                  _handlerIndex;
   };

#endif
