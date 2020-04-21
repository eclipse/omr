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

#ifndef OMR_AMD64_MEMORY_REFERENCE_INCL
#define OMR_AMD64_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MEMREF_CONNECTOR
#define OMR_MEMREF_CONNECTOR
namespace OMR { namespace X86 { namespace AMD64 { class MemoryReference; } } }
namespace OMR { typedef OMR::X86::AMD64::MemoryReference MemoryReferenceConnector; }
#else
#error OMR::X86::AMD64::MemoryReference expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/x/codegen/OMRMemoryReference.hpp"

#include <stdint.h>
#include "env/jittypes.h"

class TR_ScratchRegisterManager;
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }

namespace OMR
{

namespace X86
{

namespace AMD64
{

class OMR_EXTENSIBLE MemoryReference : public OMR::X86::MemoryReference
   {

   TR::Register *_addressRegister;   // Used when extra loads are required to compute the address
   bool _forceRIPRelative;           // Force use of RIP-relative addressing form

   public:

#if defined(TR_TARGET_64BIT)
   virtual void decNodeReferenceCounts(TR::CodeGenerator *cg);
   virtual void useRegisters(TR::Instruction  *instr, TR::CodeGenerator *cg);
   virtual void assignRegisters(TR::Instruction  *currentInstruction, TR::CodeGenerator *cg);
   virtual uint32_t estimateBinaryLength(TR::CodeGenerator *cg);
   virtual uint8_t *generateBinaryEncoding(uint8_t *modRM, TR::Instruction  *containingInstruction, TR::CodeGenerator *cg);
#endif
   TR::Register *getAddressRegister(){ return _addressRegister; }

   /**
    * @details
    *    This API forces the use of a RIP-relative addressing mode for this memory reference.
    *    The expectation in using this API is that the underlying memory reference can
    *    be validly represented in a RIP-relative mode.  In particular, it cannot contain
    *    a base or index register, nor have a SIB byte.  While not enforced here, the
    *    memory reference is checked for a valid RIP-relative form during binary encoding
    *    and will fatally assert if a RIP-relative addressing mode cannot be generated.
    */
   void setForceRIPRelative(bool b) {_forceRIPRelative = b;}
   bool getForceRIPRelative() { return _forceRIPRelative; }

   public: // Constructors
   TR_ALLOC(TR_Memory::MemoryReference)

   MemoryReference(TR::CodeGenerator *cg);

   MemoryReference(TR::Register *br, TR::SymbolReference *sr, TR::Register *ir, uint8_t s, TR::CodeGenerator *cg);

   MemoryReference(TR::Register *br, TR::Register *ir, uint8_t s, TR::CodeGenerator *cg);

   MemoryReference(TR::Register *br, intptr_t disp, TR::CodeGenerator *cg);

   MemoryReference(intptr_t disp, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm = NULL);

   MemoryReference(TR::Register *br, TR::Register *ir, uint8_t s, intptr_t disp, TR::CodeGenerator *cg);

   MemoryReference(TR::X86DataSnippet *cds, TR::CodeGenerator *cg);

   MemoryReference(TR::LabelSymbol    *label, TR::CodeGenerator *cg);

   MemoryReference(TR::Node *rootLoadOrStore, TR::CodeGenerator *cg, bool canRematerializeAddressAdds, TR_ScratchRegisterManager *srm = NULL);

   /**
    * @brief Construct a MemoryReference
    *
    * @param[in] symRef : SymbolReference of memory access
    * @param[in] cg : CodeGenerator object
    * @param[in] srm : optional ScratchRegisterManager to use to allocate overflow address register;
    *                  NULL (the default) will use the CodeGenerator allocator
    */
   MemoryReference(TR::SymbolReference *symRef, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm = NULL);

   /**
    * @brief Construct a MemoryReference
    *
    * @param[in] symRef : SymbolReference of memory access
    * @param[in] cg : CodeGenerator object
    * @param[in] forceRIPRelative : true forces use of RIP-relative addressing if applicable;
    *                               false leaves it to the MemoryReference to decide
    * @param[in] srm : optional ScratchRegisterManager to use to allocate overflow address register;
    *                  NULL (the default) will use the CodeGenerator allocator
    */
   MemoryReference(TR::SymbolReference *symRef, TR::CodeGenerator *cg, bool forceRIPRelative, TR_ScratchRegisterManager *srm = NULL);

   /**
    * @brief Construct a MemoryReference
    *
    * @param[in] symRef : SymbolReference of memory access
    * @param[in] displacement : additional displacement to include on the memory reference displacement
    * @param[in] cg : CodeGenerator object
    * @param[in] srm : optional ScratchRegisterManager to use to allocate overflow address register;
    *                  NULL (the default) will use the CodeGenerator allocator
    */
   MemoryReference(TR::SymbolReference *symRef, intptr_t displacement, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm = NULL);

   /**
    * @brief Construct a MemoryReference
    *
    * @param[in] symRef : SymbolReference of memory access
    * @param[in] displacement : additional displacement to include on the memory reference displacement
    * @param[in] cg : CodeGenerator object
    * @param[in] forceRIPRelative : true forces use of RIP-relative addressing if applicable;
    *                               false leaves it to the MemoryReference to decide
    * @param[in] srm : optional ScratchRegisterManager to use to allocate overflow address register;
    *                  NULL (the default) will use the CodeGenerator allocator
    */
   MemoryReference(TR::SymbolReference *symRef, intptr_t displacement, TR::CodeGenerator *cg, bool forceRIPRelative, TR_ScratchRegisterManager *srm = NULL);

   MemoryReference(TR::MemoryReference& mr, intptr_t n, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm = NULL);

   virtual void blockRegisters()
      {
      if (_addressRegister)
         {
         _addressRegister->block();
         }
      OMR::X86::MemoryReference::blockRegisters();
      }

   virtual void unblockRegisters()
      {
      if (_addressRegister)
         {
         _addressRegister->unblock();
         }
      OMR::X86::MemoryReference::unblockRegisters();
      }

   void addMetaDataForCodeAddressWithLoad(uint8_t *displacementLocation, TR::Instruction *containingInstruction, TR::CodeGenerator *cg, TR::SymbolReference *srCopy);
   void addMetaDataForCodeAddressDisplacementOnly(intptr_t displacement, uint8_t *cursor, TR::CodeGenerator *cg);

   protected:

#if defined(TR_TARGET_64BIT)
   bool needsAddressLoadInstruction(intptr_t nextInstructionAddress, TR::CodeGenerator *cg);
   void finishInitialization(TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm);
#else
   bool needsAddressLoadInstruction(intptr_t rip) { return false; }
   void finishInitialization(TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm) {}
#endif

   };
} // AMD64

} // X86

} // OMR

#endif
