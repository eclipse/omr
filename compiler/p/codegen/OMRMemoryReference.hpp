/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_Power_MEMORY_REFERENCE_INCL
#define OMR_Power_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MEMREF_CONNECTOR
#define OMR_MEMREF_CONNECTOR
namespace OMR { namespace Power { class MemoryReference; } }
namespace OMR { typedef OMR::Power::MemoryReference MemoryReferenceConnector; }
#else
#error OMR::Power::MemoryReference expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/Register.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class PPCPairedRelocation; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class UnresolvedDataSnippet; }

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReference
   {
   TR::Register *_baseRegister;
   TR::Node *_baseNode;
   TR::Register *_indexRegister;
   TR::Node *_indexNode;
   TR::Register *_modBase;
   intptr_t _offset;

   TR::UnresolvedDataSnippet *_unresolvedSnippet;
   TR::PPCPairedRelocation *_staticRelocation;
   TR::SymbolReference *_symbolReference;
   TR::RegisterDependencyConditions *_conditions;

   uint8_t _flag;
   uint8_t _length;

   public:

   TR_ALLOC(TR_Memory::MemoryReference)

   typedef enum
      {
      TR_PPCMemoryReferenceControl_Base_Modifiable             = 0x01,
      TR_PPCMemoryReferenceControl_Index_Modifiable            = 0x02,
      TR_PPCMemoryReferenceControl_Static_TOC                  = 0x04,
      TR_PPCMemoryReferenceControl_Late_Xform                  = 0x08,
      TR_PPCMemoryReferenceControl_OffsetRequiresWordAlignment = 0x10,
      TR_PPCMemoryReferenceControl_DelayedOffsetDone           = 0x20
      } TR_PPCMemoryReferenceControl;

   MemoryReference(TR::CodeGenerator *cg);

   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         uint8_t len,
         TR::CodeGenerator *cg);

   MemoryReference(
         TR::Register *br,
         int32_t disp,
         uint8_t len,
         TR::CodeGenerator *cg);

   virtual TR::RegisterDependencyConditions *getConditions() { return _conditions; }

   TR::SymbolReference *getSymbolReference() {return _symbolReference;}
   TR::SymbolReference *setSymbolReference(TR::SymbolReference *symRef) {return (_symbolReference = symRef);}

   TR::Register *getBaseRegister() {return _baseRegister;}
   TR::Register *setBaseRegister(TR::Register *br) {return (_baseRegister = br);}

   TR::Node *getBaseNode() {return _baseNode;}
   TR::Node *setBaseNode(TR::Node *bn) {return (_baseNode = bn);}

   TR::Register *getIndexRegister() {return _indexRegister;}
   TR::Register *setIndexRegister(TR::Register *ir) {return (_indexRegister = ir);}

   TR::Node *getIndexNode() {return _indexNode;}
   TR::Node *setIndexNode(TR::Node *in) {return (_indexNode = in);}

   TR::Register *getModBase() {return _modBase;}
   TR::Register *setModBase(TR::Register *r) {return (_modBase=r);}

   uint32_t getLength() {return _length;}
   uint32_t setLength(uint32_t len) {return (_length = len);}

   bool useIndexedForm();

   bool hasDelayedOffset()
      {
      if (_symbolReference->getSymbol() != NULL &&
         _symbolReference->getSymbol()->isRegisterMappedSymbol())
         {
         return true;
         }

      return false;
      }

   /**
    * \brief Gets the flag indicating whether the delayed offset has already been applied.
    *
    * When this flag is set, the delayed offset of this MemoryReference from its register-mapped
    * symbol has already been applied to this MemoryReference's internal offset and should not be
    * applied again. When true, getOffset() and getOffset(TR::Compilation&) are guaranteed to
    * return the same value.
    *
    * \see setDelayedOffsetDone()
    */
   bool isDelayedOffsetDone() { return (_flag & TR_PPCMemoryReferenceControl_DelayedOffsetDone) != 0; }

   /**
    * \brief Sets the flag to indicate that the delayed offset has been applied.
    *
    * \see isDelayedOffsetDone()
    */
   void setDelayedOffsetDone() { _flag |= TR_PPCMemoryReferenceControl_DelayedOffsetDone; }

   int32_t setOffset(int32_t o) {return _offset = o;}
   int32_t getOffset() {return _offset;}

   int32_t getOffset(TR::Compilation& comp);

   bool isBaseModifiable()
      {
      if ((_flag&TR_PPCMemoryReferenceControl_Base_Modifiable) != 0)
         {
         return true;
         }

      return false;
      }
   void setBaseModifiable() {_flag |= TR_PPCMemoryReferenceControl_Base_Modifiable;}
   void clearBaseModifiable() {_flag &= ~TR_PPCMemoryReferenceControl_Base_Modifiable;}

   bool isIndexModifiable()
      {
      if ((_flag&TR_PPCMemoryReferenceControl_Index_Modifiable) != 0)
         {
         return true;
         }

      return false;
      }
   void setIndexModifiable() {_flag |= TR_PPCMemoryReferenceControl_Index_Modifiable;}
   void clearIndexModifiable() {_flag &= ~TR_PPCMemoryReferenceControl_Index_Modifiable;}

   bool offsetRequireWordAlignment()
         {
         if ((_flag&TR_PPCMemoryReferenceControl_OffsetRequiresWordAlignment) != 0)
            return true;
         return false;
         }
   void setOffsetRequiresWordAlignment(TR::Node *node, TR::CodeGenerator *cg, TR::Instruction *cursor);

   bool isUsingStaticTOC()
      {
      if ((_flag&TR_PPCMemoryReferenceControl_Static_TOC) != 0)
         {
         return true;
         }
      return false;
      }
   void setUsingStaticTOC() {_flag |= TR_PPCMemoryReferenceControl_Static_TOC;}
   void clearUingStaticTOC() {_flag &= ~TR_PPCMemoryReferenceControl_Static_TOC;}

   bool isTOCAccess();

   int32_t getTOCOffset();

   bool isUsingDelayedIndexedForm()
      {
      if ((_flag&TR_PPCMemoryReferenceControl_Late_Xform) != 0)
         {
         return true ;
         }

      return false ;
      }
   void setUsingDelayedIndexedForm() {_flag |= TR_PPCMemoryReferenceControl_Late_Xform;}
   void clearUingDelayedIndexedForm() {_flag &= ~TR_PPCMemoryReferenceControl_Late_Xform;}

   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *s)
      {
      return (_unresolvedSnippet = s);
      }

   TR::PPCPairedRelocation *getStaticRelocation() {return _staticRelocation;}
   TR::PPCPairedRelocation *setStaticRelocation(TR::PPCPairedRelocation *r)
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

   void setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg);

   void checkRegisters(TR::CodeGenerator *cg);

   MemoryReference(TR::Node *rootLoadOrStore, uint32_t len, TR::CodeGenerator *cg);

   MemoryReference(TR::Node *node, TR::SymbolReference *symRef, uint32_t len, TR::CodeGenerator *cg);

   MemoryReference(TR::Node *node, TR::MemoryReference& mr, int32_t n, uint32_t len, TR::CodeGenerator *cg);

   void accessStaticItem(TR::Node *node, TR::SymbolReference *ref, bool isStore, TR::CodeGenerator *cg);

   void addToOffset(TR::Node * node, intptr_t amount, TR::CodeGenerator *cg);

   void forceIndexedForm(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *cursor = 0);

   void adjustForResolution(TR::CodeGenerator *cg);

   void decNodeReferenceCounts(TR::CodeGenerator *cg);

   void bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg);

   void populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg);

   void consolidateRegisters(TR::Register *reg, TR::Node *subTree, bool m, TR::CodeGenerator *cg);

   void assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

   void mapOpCode(TR::Instruction *currentInstruction);

   TR::Instruction *expandInstruction(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);
   TR::Instruction *expandForUnresolvedSnippet(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

   };
}
}
#endif
