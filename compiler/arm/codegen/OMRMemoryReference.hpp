/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_ARM_MEMORY_REFERENCE_INCL
#define OMR_ARM_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MEMREF_CONNECTOR
#define OMR_MEMREF_CONNECTOR
namespace OMR { namespace ARM { class MemoryReference; } }
namespace OMR { typedef OMR::ARM::MemoryReference MemoryReferenceConnector; }
#else
#error OMR::ARM::MemoryReference expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMemoryReference.hpp"

#include <stdint.h>
#include "codegen/ARMOps.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class ARMPairedRelocation; }
namespace TR { class CodeGenerator; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class SymbolReference; }
namespace TR { class UnresolvedDataSnippet; }

namespace OMR
{

namespace ARM
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReference
   {
   TR::Register *_baseRegister;
   TR::Node *_baseNode;
   TR::Register *_indexRegister;
   TR::Node *_indexNode;
   TR::Register *_modBase;

   TR::UnresolvedDataSnippet *_unresolvedSnippet;
   TR::ARMPairedRelocation *_staticRelocation;
   TR::SymbolReference _symbolReference;

   uint8_t                       _flag;
   uint8_t                       _length;
   uint8_t                       _scale;

   public:

   TR_ALLOC(TR_Memory::MemoryReference)

   typedef enum
      {
      TR_ARMMemoryReferenceControl_Base_Modifiable = 1,
      TR_ARMMemoryReferenceControl_Index_Modifiable = 2,
      TR_ARMMemoryReferenceControl_Immediate_Pre_Indexed = 4,
      TR_ARMMemoryReferenceControl_Post_Indexed = 8,
      } TR_ARMMemoryReferenceControl;

   MemoryReference(TR::CodeGenerator *cg);

   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         TR::CodeGenerator *cg);

   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         uint8_t scale,
         TR::CodeGenerator *cg);

   MemoryReference(
         TR::Register *br,
         int32_t disp,
         TR::CodeGenerator *cg);

   TR::Register *getBaseRegister() {return _baseRegister;}
   TR::Register *setBaseRegister(TR::Register *br) {return (_baseRegister = br);}

   TR::Node *getBaseNode() {return _baseNode;}
   TR::Node *setBaseNode(TR::Node *bn) {return (_baseNode = bn);}

   TR::Register *getIndexRegister() {return _indexRegister;}
   TR::Register *setIndexRegister(TR::Register *ir) {return (_indexRegister = ir);}

   TR::Register *getModBase() {return _modBase;}
   TR::Register *setModBase(TR::Register *r) {return (_modBase=r);}

   TR::Node *getIndexNode() {return _indexNode;}
   TR::Node *setIndexNode(TR::Node *in) {return (_indexNode = in);}

   bool useIndexedForm()  {return((_indexRegister==NULL)?false:true);}

   uint8_t getScale() { return _scale; }

   bool hasDelayedOffset()
      {
      if (_symbolReference.getSymbol() != NULL &&
          _symbolReference.getSymbol()->isRegisterMappedSymbol())
         return(true);
      return(false);
      }

   int32_t getOffset()
      {
      int32_t displacement = _symbolReference.getOffset();
      if (_symbolReference.getSymbol() != NULL &&
          _symbolReference.getSymbol()->isRegisterMappedSymbol())
         displacement += _symbolReference.getSymbol()->getOffset();
      return(displacement);
      }

   bool isBaseModifiable()
      {
      return (_flag & TR_ARMMemoryReferenceControl_Base_Modifiable) != 0;
      }
   void setBaseModifiable()           {_flag |= TR_ARMMemoryReferenceControl_Base_Modifiable;}
   void clearBaseModifiable()         {_flag &= ~TR_ARMMemoryReferenceControl_Base_Modifiable;}

   bool isIndexModifiable()
      {
      return (_flag & TR_ARMMemoryReferenceControl_Index_Modifiable) != 0;
	  }
   void setIndexModifiable()          {_flag |= TR_ARMMemoryReferenceControl_Index_Modifiable;}
   void clearIndexModifiable()        {_flag &= ~TR_ARMMemoryReferenceControl_Index_Modifiable;}

   bool isImmediatePreIndexed()
      {
      return (_flag & TR_ARMMemoryReferenceControl_Immediate_Pre_Indexed) != 0;
      }
   void setImmediatePreIndexed()      {_flag |= TR_ARMMemoryReferenceControl_Immediate_Pre_Indexed;}
   void clearImmediatePreIndexed()    {_flag &= ~TR_ARMMemoryReferenceControl_Immediate_Pre_Indexed;}

   bool isPostIndexed()
      {
      return (_flag & TR_ARMMemoryReferenceControl_Post_Indexed) != 0;
      }
   void setPostIndexed()      {_flag |= TR_ARMMemoryReferenceControl_Post_Indexed;}
   void clearPostIndexed()    {_flag &= ~TR_ARMMemoryReferenceControl_Post_Indexed;}

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *s)
      {
      return (_unresolvedSnippet = s);
      }

   TR::ARMPairedRelocation *getStaticRelocation() {return _staticRelocation;}
   TR::ARMPairedRelocation *setStaticRelocation(TR::ARMPairedRelocation *r)
      {
      return (_staticRelocation = r);
      }

   bool refsRegister(TR::Register *reg)
      {
      if (reg == _baseRegister ||
          reg == _indexRegister ||
          reg == _modBase)
         {
         return true;
         }
      return false;
      }

   void blockRegisters()
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }
      if (_modBase != NULL)
         {
         _modBase->block();
         }
      }

   void unblockRegisters()
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      if (_modBase != NULL)
         {
         _modBase->unblock();
         }
      }

   TR::SymbolReference &getSymbolReference() {return _symbolReference;}

   void setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg);

   MemoryReference(TR::Node *rootLoadOrStore, uint32_t len, TR::CodeGenerator *cg);

   MemoryReference(TR::Node *node, TR::SymbolReference *symRef, uint32_t len, TR::CodeGenerator *cg);

   MemoryReference(TR::MemoryReference& mr, int32_t n, uint32_t len, TR::CodeGenerator *cg);

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   void fixupVFPOffset(TR::Node *node, TR::CodeGenerator *cg);
#endif

   void addToOffset(TR::Node * node, intptr_t amount, TR::CodeGenerator *cg);

   void adjustForResolution(TR::CodeGenerator *cg);

   uint32_t estimateBinaryLength(TR_ARMOpCodes op);

   void decNodeReferenceCounts();

   void incRegisterTotalUseCounts(TR::CodeGenerator *cg);

   void populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg);

   void consolidateRegisters(TR::Register *reg, TR::Node *subTree, bool m, TR::CodeGenerator *cg);

   void assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

   uint8_t *generateBinaryEncoding(TR::Instruction *ci, uint8_t *modRM, TR::CodeGenerator *cg);

   uint8_t *encodeLargeARMConstant(uint32_t *wcursor, uint8_t *cursor, TR::Instruction *currentInstruction, TR::CodeGenerator *cg);
   };
}
}
#endif
