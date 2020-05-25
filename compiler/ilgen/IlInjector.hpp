/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#ifndef OMR_ILINJECTOR_INCL
#define OMR_ILINJECTOR_INCL


#ifndef TR_ILINJECTOR_DEFINED
#define TR_ILINJECTOR_DEFINED
#define PUT_OMR_ILINJECTOR_INTO_TR
#endif

#include <stdint.h>
#include "env/jittypes.h"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "ilgen/IlGen.hpp"

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class FrontEnd; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlInjector; }
namespace TR { class MethodBuilder; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }
namespace TR { class IlType; }
namespace TR { class TypeDictionary; }

namespace OMR
{

class IlInjector : public TR_IlGenerator
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   IlInjector(TR::TypeDictionary *types);
   IlInjector(TR::IlInjector *source);
   virtual ~IlInjector() { };

   virtual void initPrimitiveTypes();
   virtual void initialize(TR::IlGeneratorMethodDetails * details,
                           TR::ResolvedMethodSymbol     * methodSymbol,
                           TR::FrontEnd                 * fe,
                           TR::SymbolReferenceTable     * symRefTab);

   bool                           genIL();
   TR::Block                    * getCurrentBlock();
   virtual TR::ResolvedMethodSymbol * methodSymbol() const { return _methodSymbol; }
   virtual int32_t currentByteCodeIndex()                  { return -1; }

   // Many tests should just need to define their own version of this function
   virtual bool                   injectIL() = 0;

   virtual bool                   isMethodBuilder() { return false; }
   virtual TR::MethodBuilder    * asMethodBuilder() { return NULL; }

   TR::Compilation              * comp()             const { return _comp; }
   TR::IlGeneratorMethodDetails * details()          const { return _details; }
   TR::FrontEnd                 * fe()               const { return _fe; }
   TR::SymbolReferenceTable     * symRefTab()              { return _symRefTab; }
   TR::CFG                      * cfg();
   TR::TypeDictionary           * typeDictionary()         { return _types; }

   TR::Block                   ** blocks()           const { return _blocks; }
   int32_t                        numBlocks()        const { return _numBlocks; }

   // helpers to inject IL
   void                           generateToBlock(int32_t b);
   void                           allocateBlocks(int32_t num);
   TR::Block                    * newBlock();
   void                           createBlocks(int32_t num);
   TR::Node                     * parameter(int32_t slot, TR::IlType *dt);
   TR::SymbolReference          * newTemp(TR::IlType *dt);
   TR::Node                     * iconst(int32_t value);
   TR::Node                     * lconst(int64_t value);
   TR::Node                     * bconst(int8_t value);
   TR::Node                     * sconst(int16_t value);
   TR::Node                     * aconst(uintptr_t value);
   TR::Node                     * dconst(double value);
   TR::Node                     * fconst(float value);
   TR::Node                     * staticAddress(void *address);
   void                           storeToTemp(TR::SymbolReference *tempSymRef, TR::Node *value);
   TR::Node                     * loadTemp(TR::SymbolReference *tempSymRef);
   TR::Node                     * i2l(TR::Node *n);
   TR::Node                     * iu2l(TR::Node *n);

   void                           ifjump(TR::ILOpCodes op, TR::Node *first, TR::Node *second, TR::Block *targetBlock);
   void                           ifjump(TR::ILOpCodes op, TR::Node *first, TR::Node *second, int32_t targetBlockNumber);
   TR::Node                     * shiftLeftBy(TR::Node *value, int32_t shift);
   TR::Node                     * multiplyBy(TR::Node *value, int64_t factor);
   TR::Node                     * arrayLoad(TR::Node *base, TR::Node *index, TR::IlType *dt);
   void                           returnValue(TR::Node *value);
   void                           returnNoValue();

   TR::TreeTop                  * genTreeTop(TR::Node *n);
   TR::Block                    * block(int32_t num) { return _blocks[num]; }
   void                           gotoBlock(TR::Block *block);
   void                           branchToBlock(int32_t num) { gotoBlock(_blocks[num]); }
   void                           generateFallThrough();
   TR::Node                     * createWithoutSymRef(TR::ILOpCodes opCode, uint32_t numArgs, ...);

private:
   void                           validateTargetBlock();

   TR::Block                    * block(int32_t num) const { return _blocks[num]; }

protected:
   // data
   //
   TR::TypeDictionary           * _types;
   TR::Compilation              * _comp;
   TR::FrontEnd                 * _fe;
   TR::SymbolReferenceTable     * _symRefTab;

   TR::IlGeneratorMethodDetails * _details;
   TR::ResolvedMethodSymbol     * _methodSymbol;

   TR::Block                    * _currentBlock;
   int32_t                        _currentBlockNumber;
   int32_t                        _numBlocks;

   TR::Block                   ** _blocks;
   bool                           _blocksAllocatedUpFront;

public:
   TR::IlType                   * NoType;
   TR::IlType                   * Int8;
   TR::IlType                   * Int16;
   TR::IlType                   * Int32;
   TR::IlType                   * Int64;
   TR::IlType                   * Word;
   TR::IlType                   * Float;
   TR::IlType                   * Double;
   TR::IlType                   * Address;
   TR::IlType                   * VectorInt8;
   TR::IlType                   * VectorInt16;
   TR::IlType                   * VectorInt32;
   TR::IlType                   * VectorInt64;
   TR::IlType                   * VectorFloat;
   TR::IlType                   * VectorDouble;
   };

} // namespace OMR


#ifdef PUT_OMR_ILINJECTOR_INTO_TR

namespace TR
{
   class IlInjector : public OMR::IlInjector
      {
      public:
         IlInjector(TR::TypeDictionary *types)
            : OMR::IlInjector(types)
            { }

         IlInjector(TR::IlInjector *source)
            : OMR::IlInjector(source)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILINJECTOR_INTO_TR)

#endif // !defined(OMR_ILINJECTOR_INCL)
