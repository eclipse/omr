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

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

class TR_OpaqueClassBlock;
namespace TR { class Machine; }

#define IMM64_LOAD_SIZE (10)
#define MAX_MEMREF_SIZE (6) // ModRM + SIB + disp32
#define MAX_INSTRUCTION_SIZE (15) // See x86-64 processor manual vol 3 sec 1.1

// Hack markers
#define REGISTERS_CAN_CHANGE_AFTER_INITIALIZATION (true)


OMR::X86::AMD64::MemoryReference::MemoryReference(TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::Register *br, TR::SymbolReference *sr, TR::Register *ir, uint8_t s, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(br, sr, ir, s, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::Register *br, TR::Register *ir, uint8_t s, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(br, ir, s, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::Register *br, intptr_t disp, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(br, disp, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(intptr_t disp, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(disp, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::Register *br, TR::Register *ir, uint8_t s, intptr_t disp, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(br, ir, s, disp, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::X86DataSnippet *cds, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(cds, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::LabelSymbol *label, TR::CodeGenerator *cg):
      OMR::X86::MemoryReference(label, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, NULL);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::Node *rootLoadOrStore, TR::CodeGenerator *cg, bool canRematerializeAddressAdds, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(rootLoadOrStore, cg, canRematerializeAddressAdds),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::SymbolReference *symRef, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(symRef, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::SymbolReference *symRef, TR::CodeGenerator *cg, bool forceRIPRelative, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(symRef, cg),
   _forceRIPRelative(forceRIPRelative)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::SymbolReference *symRef, intptr_t displacement, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(symRef, displacement, cg),
   _forceRIPRelative(false)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::SymbolReference *symRef, intptr_t displacement, TR::CodeGenerator *cg, bool forceRIPRelative, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(symRef, displacement, cg),
   _forceRIPRelative(forceRIPRelative)
   {
   self()->finishInitialization(cg, srm);
   }

OMR::X86::AMD64::MemoryReference::MemoryReference(TR::MemoryReference& mr, intptr_t n, TR::CodeGenerator *cg, TR_ScratchRegisterManager *srm):
      OMR::X86::MemoryReference(mr, n, cg),
   _forceRIPRelative(mr.getForceRIPRelative())
   {
   self()->finishInitialization(cg, srm);
   }

void OMR::X86::AMD64::MemoryReference::finishInitialization(
      TR::CodeGenerator *cg,
      TR_ScratchRegisterManager *srm)
   {
   TR::Machine *machine = cg->machine();
   TR::SymbolReference &sr = self()->getSymbolReference();
   TR::Compilation *comp = cg->comp();

   // Figure out whether we need to allocate a register for the address
   //
   bool mightNeedAddressRegister;
   if (_forceRIPRelative)
      {
      // Assume the consumer knows the target is within RIP range.
      // Binary encoding will fatally assert otherwise.
      //
      mightNeedAddressRegister = false;
      }
   else if (self()->getDataSnippet())
      {
      // Assume snippets are in RIP range
      //
      mightNeedAddressRegister = false;
      }
   else if (!self()->getBaseRegister()  &&
            !self()->getIndexRegister() &&
            (cg->needRelocationsForStatics()            ||
             cg->needClassAndMethodPointerRelocations() ||
             cg->needRelocationsForBodyInfoData()       ||
             cg->needRelocationsForPersistentInfoData()))
      {
      mightNeedAddressRegister = true;
      }
   else if (self()->getBaseRegister() == cg->getFrameRegister())
      {
      // We should never see stack frames 2GB in size, so don't waste a register.
      // (Also, for the same reason, there's no need to consider the frame
      // pointer adjustment.)
      //
      mightNeedAddressRegister = false;
      }
   else if (sr.getSymbol() != NULL && sr.isUnresolved())
      {
      // Once resolved, the address could be anything, so be conservative.
      //
      mightNeedAddressRegister = true;
      }
   else if (comp->getOption(TR_EnableHCR) && sr.getSymbol() && sr.getSymbol()->isClassObject())
      {
      // Can't do any displacement-based tests because the displacement can change
      //
      mightNeedAddressRegister = true;
      }
   else
      {
      // [offset]
      // [scale*index + offset]
      // [base + offset]
      // [base+index+offset]
      // TODO: if the offset is just barely over 32 bits, maybe we could
      // get away with this, where a+b = offset64:
      //
      //    lea R4, [R2 + a]
      //    xxx R1, [R4 + scale*R3 + b]
      //
      mightNeedAddressRegister = !IS_32BIT_SIGNED(self()->getDisplacement());
      }

   // If we might need it, allocate it
   //
   if (mightNeedAddressRegister)
      {
      // TODO:AMD64: Can we neglect the GC maps?
      // TODO:AMD64: Will this register be "live" enough, even though nothing
      // happens between allocate and stopUsing?
      //
      if (srm)
         {
         _addressRegister = srm->findOrCreateScratchRegister();
         }
      else
         {
         _addressRegister = cg->allocateRegister();
         cg->stopUsingRegister(_addressRegister); // See comments in decNodeReferenceCounts
         }
      }
   else
      {
      _addressRegister = NULL;
      }

   }

void OMR::X86::AMD64::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator *cg)
   {
   OMR::X86::MemoryReference::decNodeReferenceCounts(cg);
   // TODO:AMD64: This seems like the natural place to stop using
   // _addressRegister.  The trouble is that _addressRegister may be allocated
   // in cases where the caller has no idea that a register is being allocated,
   // and hence doesn't even call decNodeReferenceCounts.  Thus, we need to
   // find another way to do it.
   //
   // (For now we just call stopUsingRegister immediately after allocating it.)
   }

void OMR::X86::AMD64::MemoryReference::useRegisters(TR::Instruction  *instr, TR::CodeGenerator *cg)
   {
   OMR::X86::MemoryReference::useRegisters(instr, cg);
   if (_addressRegister != NULL)
      {
      instr->useRegister(_addressRegister);
      }
   }

bool OMR::X86::AMD64::MemoryReference::needsAddressLoadInstruction(intptr_t nextInstructionAddress, TR::CodeGenerator * cg)
   {
   TR::SymbolReference &sr = self()->getSymbolReference();
   intptr_t displacement = self()->getDisplacement();

   if (_forceRIPRelative)
      {
      return false;
      }
   else if (sr.getSymbol() != NULL && sr.isUnresolved())
      {
      return sr.getSymbol()->isShadow() ? false : true;
      }
   else if (_baseRegister || _indexRegister)
      return !IS_32BIT_SIGNED(displacement);
   else if (cg->needClassAndMethodPointerRelocations())
      return true;
   else if (sr.getSymbol() && sr.getSymbol()->isRecompilationCounter() && cg->needRelocationsForBodyInfoData())
      return true;
   else if (sr.getSymbol() && sr.getSymbol()->isCountForRecompile() && cg->needRelocationsForPersistentInfoData())
      return true;
   else if (cg->comp()->getOption(TR_EnableHCR) && sr.getSymbol() && sr.getSymbol()->isClassObject())
      return true; // If a class gets replaced, it may no longer fit in an immediate
   else if (IS_32BIT_SIGNED(displacement))
      return false;
   else if (IS_32BIT_RIP(displacement, nextInstructionAddress))
      return false;
   else
      return true;
   }

void OMR::X86::AMD64::MemoryReference::assignRegisters(TR::Instruction  *currentInstruction, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: If currentInstruction has a target register, then we can use
   // that instead of assigning a new register.  This would have the side
   // benefit of allowing us to reinstate rematerialization of addresses.
   //
   if (_addressRegister == NULL)
      {
      // No special logic needed; just call inherited logic.
      //
      OMR::X86::MemoryReference::assignRegisters(currentInstruction, cg);
      }
   else
      {
      // Allocate _addressRegister with _baseRegister and _indexRegister blocked
      //
      TR::RealRegister *assignedAddressRegister = _addressRegister->getAssignedRealRegister();

      if (assignedAddressRegister == NULL)
         {
         if (_baseRegister != NULL)
            _baseRegister->block();
         if (_indexRegister != NULL)
            _indexRegister->block();

         assignedAddressRegister = assignGPRegister(currentInstruction, _addressRegister, TR_WordReg, cg);

         if (_indexRegister != NULL)
            _indexRegister->unblock();
         if (_baseRegister != NULL)
            _baseRegister->unblock();
         }

      // Allocate _baseRegister and _indexRegister with _addressRegister blocked
      //
      _addressRegister->block();
      OMR::X86::MemoryReference::assignRegisters(currentInstruction, cg);
      _addressRegister->unblock();

      // Stop using the address register
      //
      if (_addressRegister->decFutureUseCount() == 0 &&
          assignedAddressRegister->getState() != TR::RealRegister::Locked)
         {
         _addressRegister->setAssignedRegister(NULL);
         assignedAddressRegister->setState(TR::RealRegister::Unlatched);
         }

      // Save the assigned register
      //
      _addressRegister = assignedAddressRegister;
      }
   }

uint32_t OMR::X86::AMD64::MemoryReference::estimateBinaryLength(TR::CodeGenerator *cg)
   {
   uint32_t estimate;

   if (0 && REGISTERS_CAN_CHANGE_AFTER_INITIALIZATION && self()->getBaseRegister() && self()->getIndexRegister() && _addressRegister)
      {
      // We thought we might need _addressRegister during initialization
      // because _baseRegister or _indexRegister NULL.  However, someone
      // subsequently changed the registers, and we can't handle
      // [base+index+disp64] anyway, so don't even try.
      //
      // TODO:AMD64: Always pass the regs in the constructor, rather than using
      // the vanilla constructor and setting the regs afterward.
      //
      _addressRegister = NULL;
      }

   if (_addressRegister == NULL)
      {
      // Just use inherited logic
      //
      estimate = OMR::X86::MemoryReference::estimateBinaryLength(cg);

      // For [disp32], AMD64 needs a SIB byte
      //
      if (_baseRegister == NULL && _indexRegister == NULL)
         estimate += 1;
      }
   else
      {

      // TODO:AMD64: Should be able to do a tighter estimate than this
      //
      // (Note: we assume the memory needed is always bigger after adding the
      // great big load instruction.  Thus, the size we use for the estimate is
      // the size after adding the big load instruction.)
      //
      estimate = IMM64_LOAD_SIZE + MAX_MEMREF_SIZE;

      }

   return estimate;

   }


void
OMR::X86::AMD64::MemoryReference::addMetaDataForCodeAddressWithLoad(
      uint8_t *displacementLocation,
      TR::Instruction *containingInstruction,
      TR::CodeGenerator *cg,
      TR::SymbolReference *srCopy)
   {
   intptr_t displacement = self()->getDisplacement();

   if (_symbolReference.getSymbol())
      {
      TR::SymbolReference &sr = *srCopy;
      if (self()->getUnresolvedDataSnippet())
         {
         TR::Compilation *comp = cg->comp();
         if (comp->getOption(TR_EnableHCR)
             && (!sr.getSymbol()->isStatic()
                 || !sr.getSymbol()->isClassObject()
                 || cg->wantToPatchClassPointer(NULL, containingInstruction->getBinaryEncoding()))) // unresolved
            {
            cg->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(containingInstruction->getBinaryEncoding());
            }
         }
      else if ((sr.getSymbol()->isClassObject()))
         {
         if (sr.getSymbol()->isStatic())
            {
            if (cg->needClassAndMethodPointerRelocations())
               {
               if (cg->comp()->getOption(TR_UseSymbolValidationManager))
                  {
                  cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation,
                                                                                            (uint8_t *)sr.getSymbol()->castToStaticSymbol()->getStaticAddress(),
                                                                                            (uint8_t *)TR::SymbolType::typeClass,
                                                                                            TR_SymbolFromManager,
                                                                                            cg),
                                                                                   __FILE__, __LINE__,
                                                                                   containingInstruction->getNode());
                  }
               else
                  {
                  cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation, (uint8_t *)srCopy,
                                                                                            (uint8_t *)(uintptr_t)containingInstruction->getNode()->getInlinedSiteIndex(),
                                                                                            TR_ClassAddress, cg),__FILE__, __LINE__,
                                                                                            containingInstruction->getNode());
                  }
               }

            if (cg->wantToPatchClassPointer(NULL, displacementLocation)) // may not point to beginning of class
               {
               cg->jitAddPicToPatchOnClassRedefinition(((void *)displacement), displacementLocation);
               }
            }
         }
      else if (sr.getSymbol()->isCountForRecompile())
         {
         if (cg->needRelocationsForPersistentInfoData())
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(
                                   displacementLocation, (uint8_t *) TR_CountForRecompile, TR_GlobalValue, cg),
                                 __FILE__,
                                 __LINE__,
                                 containingInstruction->getNode());
         }
      else if (sr.getSymbol()->isRecompilationCounter())
         {
         if (cg->needRelocationsForBodyInfoData())
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation, 0, TR_BodyInfoAddress, cg),
                                 __FILE__,__LINE__,containingInstruction->getNode());
         }
      else if (sr.getSymbol()->isGCRPatchPoint())
         {
         if (cg->needRelocationsForStatics())
            {
            TR::ExternalRelocation* r= new (cg->trHeapMemory())
                           TR::ExternalRelocation(displacementLocation,
                                                      0,
                                                      TR_AbsoluteMethodAddress, cg);
            cg->addExternalRelocation(r, __FILE__, __LINE__, containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isCompiledMethod())
         {
         if (cg->needRelocationsForStatics())
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation, 0, TR_RamMethod, cg),
                                 __FILE__,__LINE__,containingInstruction->getNode());
         }
      else if (sr.getSymbol()->isStartPC())
         {
         if (cg->needRelocationsForStatics())
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation, 0, TR_AbsoluteMethodAddress, cg),
                                 __FILE__,__LINE__,containingInstruction->getNode());
         }
      else if (sr.getSymbol()->isDebugCounter())
         {
         TR::DebugCounterBase *counter = cg->comp()->getCounterFromStaticAddress(&sr);
         if (counter == NULL)
            {
            cg->comp()->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::X86::AMD64::MemoryReference::addMetaDataForCodeAddressWithLoad\n");
            }
         TR::DebugCounter::generateRelocation(cg->comp(),
                                              displacementLocation,
                                              containingInstruction->getNode(),
                                              counter);
         }
      }
   else
      {
      if (self()->needsCodeAbsoluteExternalRelocation())
         {
         cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(displacementLocation,
                                                                                 (uint8_t *)0,
                                                                                 TR_AbsoluteMethodAddress, cg),
                              __FILE__,
                              __LINE__,
                              containingInstruction->getNode());
         }
      }

   }


void
OMR::X86::AMD64::MemoryReference::addMetaDataForCodeAddressDisplacementOnly(
      intptr_t displacement,
      uint8_t *cursor,
      TR::CodeGenerator *cg)
   {

   if (IS_32BIT_SIGNED(displacement) && !_forceRIPRelative)
      {
      if (_symbolReference.getSymbol() && _symbolReference.getSymbol()->isClassObject()
         && cg->wantToPatchClassPointer(NULL, cursor)) // may not point to beginning of class
         {
         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)displacement), cursor);
         }
      }

   }


uint8_t *
OMR::X86::AMD64::MemoryReference::generateBinaryEncoding(
      uint8_t *modRM,
      TR::Instruction *containingInstruction,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference &sr = self()->getSymbolReference();
   intptr_t displacement = self()->getDisplacement();

   if (comp->getOption(TR_TraceCG))
      {
      diagnostic("OMR::X86::AMD64::MemoryReference " POINTER_PRINTF_FORMAT " with address register %s in instruction " POINTER_PRINTF_FORMAT "\n",
         this,
         _addressRegister
            ? cg->getDebug()->getName(_addressRegister, TR_DoubleWordReg)
            : "(none)",
         containingInstruction
         );
      }

   TR_ASSERT(containingInstruction != NULL, "AMD64: must have containing instruction to insert address load");

   // Enforcing RIP-relative addressing can only happen for specific forms of the memory reference,
   // namely where neither a base nor index register have been provided and a SIB byte is not present.
   // Check and assert if this is an invalid form for RIP-relative addressing.
   //
   if (_forceRIPRelative)
      {
      TR_ASSERT_FATAL(!self()->getBaseRegister() && !self()->getIndexRegister() && !self()->isForceSIBByte(),
                      "malformed memory reference for RIP-relative addressing");
      }

   // If a RIP relative addressing mode might be used, compute the address of the next
   // instruction based on where the modRM byte is
   //
   // [RIP+xxx] has 4-byte offset, no SIB, may have an immediate.
   // See x86-64 Architecture Programmer's Manual, Volume 3, sections 1.1 and 1.7.1.
   //
   // address of next instruction = modRM + 4 (disp32) + sizeof(immediate for this instruction: 0, 1, or 4) + 1
   //
   intptr_t nextInstructionAddress = (intptr_t)(modRM + 5) + containingInstruction->getOpCode().info().ImmediateSize();

   if (self()->getDataSnippet() || self()->getLabel())
      {
      // The inherited logic has a special case for RIP-based ConstantDataSnippet and label references.
      //
      return OMR::X86::MemoryReference::generateBinaryEncoding(modRM, containingInstruction, cg);
      }
   else if (self()->needsAddressLoadInstruction(nextInstructionAddress, cg))
      {
      TR_ASSERT(_addressRegister != NULL, "OMR::X86::AMD64::MemoryReference should have allocated an address register");

      TR::Instruction *addressLoadInstruction;

      uint8_t *displacementLocation = containingInstruction->getBinaryEncoding()+2;

      // Create a mov immediate to load the address
      //
      TR::SymbolReference *symRef = NULL;
      if (_symbolReference.getSymbol())
         {
         // Clone the symbol reference because we're going to clobber it shortly
         //
         symRef = new (cg->trHeapMemory()) TR::SymbolReference(cg->symRefTab(), _symbolReference, 0);

         addressLoadInstruction = generateRegImm64SymInstruction(
            containingInstruction->getPrev(),
            MOV8RegImm64,
            _addressRegister,
            (!self()->getUnresolvedDataSnippet() &&
              sr.getSymbol()->isStatic() &&
              sr.getSymbol()->isClassObject() &&
              cg->needClassAndMethodPointerRelocations())?
                  (uint64_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(comp, (TR_OpaqueClassBlock*)displacement) : displacement,
            symRef,
            cg
            );

         if (self()->getUnresolvedDataSnippet())
            {
            self()->getUnresolvedDataSnippet()->setDataReferenceInstruction(addressLoadInstruction);
            self()->getUnresolvedDataSnippet()->setDataSymbolReference(symRef);
            }
         }
      else
         {
         TR_ASSERT(!self()->getUnresolvedDataSnippet(), "Unresolved references should always have a symbol");

         addressLoadInstruction = generateRegImm64Instruction(
            containingInstruction->getPrev(),
            MOV8RegImm64,
            _addressRegister,
            displacement,
            cg
            );
         }

      self()->addMetaDataForCodeAddressWithLoad(displacementLocation, containingInstruction, cg, symRef);

      // addressLoadInstruction's node should be that of containingInstruction
      //
      addressLoadInstruction->setNode(_baseNode ? _baseNode : containingInstruction->getNode());
      if (cg->comp()->target().isSMP() && self()->getUnresolvedDataSnippet())
         {
         // Also adjust the node of the TR::X86PatchableCodeAlignmentInstruction
         //
         TR_ASSERT((addressLoadInstruction->getPrev()->getKind() == TR::Instruction::IsPatchableCodeAlignment) ||
                (addressLoadInstruction->getPrev()->getKind() == TR::Instruction::IsBoundaryAvoidance),
            "Expected TR::X86PatchableCodeAlignmentInstruction or TR::X86BoundaryAvoidance before unresolved memory reference instruction");
         addressLoadInstruction->getPrev()->setNode(containingInstruction->getNode());
         }

      // Emit the instruction to load the address over top of the
      // already-emitted binary for containingInstruction.
      //
      uint8_t *cursor = containingInstruction->getBinaryEncoding();
      cg->setBinaryBufferCursor(cursor);
      cursor = addressLoadInstruction->generateBinaryEncoding();
      cg->setBinaryBufferCursor(cursor);

      if (self()->getBaseRegister() && self()->getIndexRegister())
         {
         TR::Instruction  *addressAddInstruction = generateRegRegInstruction(addressLoadInstruction, ADD8RegReg, self()->getAddressRegister(), self()->getBaseRegister(), cg);
         cursor = addressAddInstruction->generateBinaryEncoding();
         cg->setBinaryBufferCursor(cursor);
         }

      // If it's unresolved, tell the snippet where the data reference is
      //
      if (self()->getUnresolvedDataSnippet())
         self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor-8);

      // Transform this memref from [whatever + offset64] to [whatever + _addressRegister]
      // Also transforms [base + index + offset64] to [_addressRegister + index]
      //
      if (_indexRegister == NULL)
         {
         _indexRegister = _addressRegister;
         _indexNode     = NULL;
         _stride        = 0;
         }
      else
         {
         _baseRegister = _addressRegister;
         _baseNode     = NULL;
         }

      _flags.reset(MemRef_ForceWideDisplacement);
      _flags.reset(MemRef_NeedExternalCodeAbsoluteRelocation); // TODO:AMD64: Do I need this?
      _symbolReference.setSymbol(NULL);
      _symbolReference.setOffset(0);
      self()->setUnresolvedDataSnippet(NULL); // Otherwise it will get damaged when we re-emit containingInstruction

      // Indicate to caller that it must try again to emit its binary
      //
      return NULL;
      }
   else if (_baseRegister == NULL && _indexRegister == NULL)
      {
      uint8_t *cursor = modRM+1;
      if (IS_32BIT_SIGNED(displacement) && !_forceRIPRelative)
         {
         // Addresses in the low 2GB (or high 2GB) can be accessed absolutely using a SIB byte
         //
         self()->ModRM(modRM)->setBase()->setHasSIB();
         self()->SIB(cursor++)->setScale()->setNoIndex()->setIndexDisp32();
         *(uint32_t*)cursor = (uint32_t)displacement;
         }
      else
         {
         // Use RIP-relative addressing
         //
         TR_ASSERT_FATAL(IS_32BIT_RIP(displacement, nextInstructionAddress), "destination displacement out of RIP-relative range");
         TR_ASSERT(!(comp->getOption(TR_EnableHCR) && sr.getSymbol() && sr.getSymbol()->isClassObject()),
            "HCR runtime assumptions currently can't patch RIP-relative offsets");
         self()->ModRM(modRM)->setIndexOnlyDisp32();
         *(uint32_t*)cursor = (uint32_t)(displacement - (intptr_t)nextInstructionAddress);
         }

      self()->addMetaDataForCodeAddressDisplacementOnly(displacement, cursor, cg);

      // Unresolved shadows whose base object is explicitly NULL need to report the
      // offset of the disp32 field.
      //
      // NOTE: this may not work in the highly unlikely case of a field offset > 64k
      //       where the implicit NULLCHK will not fire.
      //
      if (self()->getUnresolvedDataSnippet())
         {
         self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor);
         traceMsg(comp, "found unresolved shadow with NULL base object : data reference instruction=%p, cursor=%p\n",
            self()->getUnresolvedDataSnippet()->getDataReferenceInstruction(), cursor);
         }

      return cursor+4;
      }
   else
      {
      // The inherited binary encoding logic will work as-is
      //
      return OMR::X86::MemoryReference::generateBinaryEncoding(modRM, containingInstruction, cg);
      }

   TR_ASSERT(0, "Should never get here");
   }
