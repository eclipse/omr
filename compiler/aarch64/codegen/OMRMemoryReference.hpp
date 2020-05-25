/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef OMR_ARM64_MEMORY_REFERENCE_INCL
#define OMR_ARM64_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MEMREF_CONNECTOR
#define OMR_MEMREF_CONNECTOR
namespace OMR { namespace ARM64 { class MemoryReference; } }
namespace OMR { typedef OMR::ARM64::MemoryReference MemoryReferenceConnector; }
#else
#error OMR::ARM64::MemoryReference expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/Register.hpp"
#include "env/TRMemory.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class UnresolvedDataSnippet; }

namespace OMR
{

namespace ARM64
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReference
   {
   TR::Register *_baseRegister;
   TR::Node *_baseNode;
   TR::Register *_indexRegister;
   TR::Node *_indexNode;
   int32_t _offset;

   TR::UnresolvedDataSnippet *_unresolvedSnippet;
   TR::SymbolReference *_symbolReference;

   // Any downstream project can use this extra register to associate an additional
   // register to a memory reference that can be used for project-specific purposes.
   // This register is in addition to the base and index registers and isn't directly
   // part of the addressing expression in the memory reference.
   // An example of what this could be used for is to help synthesize unresolved data
   // reference addresses at runtime.
   TR::Register *_extraRegister;

   uint8_t _flag;
   uint8_t _length;
   uint8_t _scale;

   public:

   TR_ALLOC(TR_Memory::MemoryReference)

   typedef enum
      {
      TR_ARM64MemoryReferenceControl_Base_Modifiable  = 0x01,
      TR_ARM64MemoryReferenceControl_Index_Modifiable = 0x02,
      /* To be added more if necessary */
      } TR_ARM64MemoryReferenceControl;

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] ir : index register
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] ir : index register
    * @param[in] scale : scale of index
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         TR::Register *ir,
         uint8_t scale,
         TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] br : base register
    * @param[in] disp : displacement
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(
         TR::Register *br,
         int32_t disp,
         TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] node : load or store node
    * @param[in] len : length
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *node, uint32_t len, TR::CodeGenerator *cg);

   /**
    * @brief Constructor
    * @param[in] node : node
    * @param[in] symRef : symbol reference
    * @param[in] len : length
    * @param[in] cg : CodeGenerator object
    */
   MemoryReference(TR::Node *node, TR::SymbolReference *symRef, uint32_t len, TR::CodeGenerator *cg);

   /**
    * @brief Gets base register
    * @return base register
    */
   TR::Register *getBaseRegister() {return _baseRegister;}
   /**
    * @brief Sets base register
    * @param[in] br : base register
    * @return base register
    */
   TR::Register *setBaseRegister(TR::Register *br) {return (_baseRegister = br);}

   /**
    * @brief Gets base node
    * @return base node
    */
   TR::Node *getBaseNode() {return _baseNode;}
   /**
    * @brief Sets base node
    * @param[in] bn : base node
    * @return base node
    */
   TR::Node *setBaseNode(TR::Node *bn) {return (_baseNode = bn);}

   /**
    * @brief Gets index register
    * @return index register
    */
   TR::Register *getIndexRegister() {return _indexRegister;}
   /**
    * @brief Sets index register
    * @param[in] ir : index register
    * @return index register
    */
   TR::Register *setIndexRegister(TR::Register *ir) {return (_indexRegister = ir);}

   /**
    * @brief Gets index node
    * @return index node
    */
   TR::Node *getIndexNode() {return _indexNode;}
   /**
    * @brief Sets index node
    * @param[in] in : index node
    * @return index node
    */
   TR::Node *setIndexNode(TR::Node *in) {return (_indexNode = in);}

   /**
    * @brief Gets extra register
    * @return extra register
    */
   TR::Register *getExtraRegister() {return _extraRegister;}
   /**
    * @brief Sets extra register
    * @param[in] er : extra register
    * @return extra register
    */
   TR::Register *setExtraRegister(TR::Register *er) {return (_extraRegister = er);}

   /**
    * @brief Gets length
    * @return length
    */
   uint32_t getLength() {return _length;}
   /**
    * @brief Sets length
    * @param[in] len : length
    * @return length
    */
   uint32_t setLength(uint32_t len) {return (_length = len);}

   /**
    * @brief Gets scale
    * @return scale
    */
   uint8_t getScale() { return _scale; }

   /**
    * @brief Uses indexed form or not
    * @return true when index form is used
    */
   bool useIndexedForm() { return (_indexRegister != NULL); }

   /**
    * @brief Has delayed offset or not
    * @return true when it has delayed offset
    */
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
    * @brief Gets offset
    * @param[in] withRegSym : add offset of register mapped symbol if true
    * @return offset
    */
   int32_t getOffset(bool withRegSym = false)
      {
      int32_t displacement = _offset;
      if (withRegSym &&
          _symbolReference->getSymbol() != NULL &&
          _symbolReference->getSymbol()->isRegisterMappedSymbol())
         displacement += _symbolReference->getSymbol()->getOffset();

      return displacement;
      }
   /**
    * @brief Sets offset
    * @param[in] o : offset
    * @return offset
    */
   int32_t setOffset(int32_t o) {return _offset = o;}

   /**
    * @brief Answers if MemoryReference refs specified register
    * @param[in] reg : register
    * @return true if MemoryReference refs the register, false otherwise
    */
   bool refsRegister(TR::Register *reg)
      {
      return (reg == _baseRegister ||
              reg == _indexRegister ||
              reg == _extraRegister);
      }

   /**
    * @brief Blocks registers used by MemoryReference
    */
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
      if (_extraRegister != NULL)
         {
         _extraRegister->block();
         }
      }

   /**
    * @brief Unblocks registers used by MemoryReference
    */
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
      if (_extraRegister != NULL)
         {
         _extraRegister->unblock();
         }
      }

   /**
    * @brief Base register is modifiable or not
    * @return true when base register is modifiable
    */
   bool isBaseModifiable()
      {
      return ((_flag & TR_ARM64MemoryReferenceControl_Base_Modifiable) != 0);
      }
   /**
    * @brief Sets the BaseModifiable flag
    */
   void setBaseModifiable() {_flag |= TR_ARM64MemoryReferenceControl_Base_Modifiable;}
   /**
    * @brief Clears the BaseModifiable flag
    */
   void clearBaseModifiable() {_flag &= ~TR_ARM64MemoryReferenceControl_Base_Modifiable;}

   /**
    * @brief Index register is modifiable or not
    * @return true when index register is modifiable
    */
   bool isIndexModifiable()
      {
      return ((_flag & TR_ARM64MemoryReferenceControl_Index_Modifiable) != 0);
      }
   /**
    * @brief Sets the IndexModifiable flag
    */
   void setIndexModifiable() {_flag |= TR_ARM64MemoryReferenceControl_Index_Modifiable;}
   /**
    * @brief Clears the IndexModifiable flag
    */
   void clearIndexModifiable() {_flag &= ~TR_ARM64MemoryReferenceControl_Index_Modifiable;}

   /**
    * @brief Gets the unresolved data snippet
    * @return the unresolved data snippet
    */
   TR::UnresolvedDataSnippet *getUnresolvedSnippet() {return _unresolvedSnippet;}
   /**
    * @brief Sets an unresolved data snippet
    * @return the unresolved data snippet
    */
   TR::UnresolvedDataSnippet *setUnresolvedSnippet(TR::UnresolvedDataSnippet *s)
      {
      return (_unresolvedSnippet = s);
      }

   /**
    * @brief Gets the symbol reference
    * @return the symbol reference
    */
   TR::SymbolReference *getSymbolReference() {return _symbolReference;}

   /**
    * @brief Sets symbol
    * @param[in] symbol : symbol to be set
    * @param[in] cg : CodeGenerator
    */
   void setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg);

   /**
    * @brief Adds to offset
    * @param[in] node : node
    * @param[in] amount : amount to be added to offset
    * @param[in] cg : CodeGenerator
    */
   void addToOffset(TR::Node *node, intptr_t amount, TR::CodeGenerator *cg);

   /**
    * @brief Decrements node reference counts
    * @param[in] cg : CodeGenerator
    */
   void decNodeReferenceCounts(TR::CodeGenerator *cg);

   /**
    * @brief Populates memory reference
    * @param[in] subTree : sub-tree node
    * @param[in] cg : CodeGenerator
    */
   void populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg);

   /**
    * @brief Consolidates registers
    * @param[in] srcReg : source register
    * @param[in] srcTree : source tree node
    * @param[in] srcModifiable : true if modifiable
    * @param[in] cg : CodeGenerator
    */
   void consolidateRegisters(TR::Register *srcReg, TR::Node *srcTree, bool srcModifiable, TR::CodeGenerator *cg);

   /**
    * @brief Increment totalUseCounts of registers in MemoryReference
    * @param[in] cg : CodeGenerator
    */
   void incRegisterTotalUseCounts(TR::CodeGenerator *cg);

   /**
    * @brief Assigns registers
    * @param[in] currentInstruction : current instruction
    * @param[in] cg : CodeGenerator
    */
   void assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg);

   /**
    * @brief Estimates the length of generated binary
    * @param[in] op : opcode of the instruction to attach this memory reference to
    * @return estimated binary length
    */
   uint32_t estimateBinaryLength(TR::InstOpCode op);

   /**
    * @brief Generates binary encoding
    * @param[in] ci : current instruction
    * @param[in] cursor : instruction cursor
    * @param[in] cg : CodeGenerator
    * @return instruction cursor after encoding
    */
   uint8_t *generateBinaryEncoding(TR::Instruction *ci, uint8_t *cursor, TR::CodeGenerator *cg);
   };

} // ARM64
} // OMR

#endif
