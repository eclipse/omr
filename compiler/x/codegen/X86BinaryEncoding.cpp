/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <algorithm>                               // for std::find, etc
#include <stddef.h>                                // for NULL
#include <stdint.h>                                // for uint8_t, int32_t, etc
#include "codegen/BackingStore.hpp"                // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                    // for TR_FrontEnd, etc
#include "codegen/Instruction.hpp"                 // for EnlargementResult, etc
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"                     // for Machine
#include "codegen/MemoryReference.hpp"             // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"                     // for Snippet
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"                 // for Compilation, etc
#include "compile/ResolvedMethod.hpp"              // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"                  // for PersistentInfo
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                          // for intptrj_t, uintptrj_t
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::aconst, etc
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "il/symbol/MethodSymbol.hpp"              // for MethodSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"              // for StaticSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/List.hpp"                          // for List
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "ras/DebugCounter.hpp"                    // for TR::DebugCounter, etc
#include "runtime/Runtime.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                    // for TR_X86OpCode, etc
#include "x/codegen/X86Ops_inlines.hpp"
#include "codegen/StaticRelocation.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#include "env/VMJ9.h"
#endif

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

int32_t memoryBarrierRequired(
      TR_X86OpCode &op,
      TR::MemoryReference *mr,
      TR::CodeGenerator *cg,
      bool onlyAskingAboutFences)
   {
   TR::Compilation *comp = cg->comp();
   if (!TR::Compiler->target.isSMP())
      return NoFence;

   // LOCKed instructions are serializing instructions and implicitly handle
   // any required memory barriers.
   //
   if (op.needsLockPrefix())
      return NoFence;

   if (!onlyAskingAboutFences &&
       mr->requiresLockPrefix())
      {
      TR_ASSERT(op.supportsLockPrefix(), "cannot emit a lock prefix with this type of instruction");
      return LockPrefix;
      }

   int32_t barrier = NoFence;

   TR::SymbolReference&  symRef = mr->getSymbolReference();
   TR::Symbol            *sym    = symRef.getSymbol();

   static char *mbou = feGetEnv("TR_MemoryBarriersOnUnresolved");

   // Unresolved references are assumed to be volatile until they can be proven otherwise.
   // The memory barrier will be removed by PicBuilder when the reference is resolved and
   // proven to be non-volatile.
   //
   if ((symRef.isUnresolved() && mbou) ||
       (sym && sym->isVolatile() && !mr->ignoreVolatile()))
      {
      if (op.sourceIsMemRef())
         {
         if (op.modifiesSource())
         {
            // use lock or as default fence unless specified to use mfence
            if (!comp->getOption(TR_X86UseMFENCE))
               barrier |= LockOR;
            else
               barrier |= kMemoryFence;
         }
         else if (cg->getX86ProcessorInfo().requiresLFENCE())
            barrier |= kLoadFence;
         }
      else
         {
         if (op.modifiesTarget())
         {
            // use lock or as default fence unless specified to use mfence
            if (!comp->getOption(TR_X86UseMFENCE))
               barrier |= LockOR;
            else
               barrier |= kMemoryFence;
         }
         else if (op.usesTarget() && cg->getX86ProcessorInfo().requiresLFENCE())
            barrier |= kLoadFence;
         }
      }

   static char *disableExplicitFences = feGetEnv("TR_DisableExplicitFences");
   if (barrier)
      {
      if ((!cg->getX86ProcessorInfo().supportsLFence() ||
           !cg->getX86ProcessorInfo().supportsMFence()) || disableExplicitFences)
         {
         if (op.supportsLockPrefix())
            barrier |= LockPrefix;
         else
            barrier |= LockOR;
         }
      }

   return barrier;
   }


int32_t estimateMemoryBarrierBinaryLength(int32_t barrier, TR::CodeGenerator *cg)
   {
   int32_t length = 0;

   if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport))
      return 0;

   if (barrier & LockOR)
      length = 5;
   else if ((barrier & kLoadFence) && cg->getX86ProcessorInfo().requiresLFENCE())
      length = TR_X86OpCode(LFENCE).length();
   else if ((barrier & kMemoryFence) == kMemoryFence)
      length = TR_X86OpCode(MFENCE).length();
   else if (barrier & kStoreFence)
      length = TR_X86OpCode(SFENCE).length();

   return length;
   }


uint8_t getMemoryBarrierBinaryLengthLowerBound(int32_t barrier, TR::CodeGenerator *cg)
   {
   return estimateMemoryBarrierBinaryLength(barrier, cg);
   }


// -----------------------------------------------------------------------------
// OMR::X86::Instruction:: member functions
bool OMR::X86::Instruction::needsRepPrefix()
   {
   return self()->getOpCode().needsRepPrefix();
   }

bool OMR::X86::Instruction::needsLockPrefix()
   {
   return self()->getOpCode().needsLockPrefix();
   }

uint8_t* OMR::X86::Instruction::generateBinaryEncoding()
   {
   // Normally the loop only executes once,
   // it is needed to avoid recursive calls when generateOperand() requests to regenerate the binary code by returning NULL;
   for(;;)
      {
      uint8_t *instructionStart = self()->cg()->getBinaryBufferCursor();
      uint8_t *cursor           = instructionStart;
      self()->setBinaryEncoding(instructionStart);
      if (self()->needsRepPrefix())
         {
         *cursor++ = 0xf3;
         }
      if (self()->needsLockPrefix())
         {
         *cursor++ = 0xf0;
         }
      cursor = self()->generateRepeatedRexPrefix(cursor);
      cursor = self()->getOpCode().binary(cursor, self()->rexBits());
      cursor = self()->generateOperand(cursor);
      // cursor is normal not NULL, and hence we can finish binary encoding and exit the loop
      // cursor is NULL when generateOperand() requests to regenerate the binary code, which may happen during encoding of memref with unresolved symbols on 64-bit
      if (cursor)
         {
         self()->getOpCode().finalize(instructionStart);
         self()->setBinaryLength(cursor - instructionStart);
         self()->cg()->addAccumulatedInstructionLengthError(self()->getEstimatedBinaryLength() - self()->getBinaryLength());
         return cursor;
         }
      }
   }

int32_t OMR::X86::Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   self()->setEstimatedBinaryLength(self()->getOpCode().length(self()->rexBits()) + (self()->needsRepPrefix() ? 1 : 0));
   return currentEstimate + self()->getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86PaddingInstruction:: member functions

uint8_t *TR::X86PaddingInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;
   cursor = cg()->generatePadding(cursor, _length, this, _properties);
   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   return cursor;
   }

int32_t TR::X86PaddingInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(_length);
   return currentEstimate + getEstimatedBinaryLength();
   }

TR::Snippet *TR::X86PaddingSnippetInstruction::getSnippetForGC()
   {
   return _unresolvedSnippet;
   }


// -----------------------------------------------------------------------------
// TR::X86BoundaryAvoidanceInstruction:: member functions

OMR::X86::EnlargementResult
TR::X86BoundaryAvoidanceInstruction::enlarge(
      int32_t requestedEnlargementSize,
      int32_t maxEnlargementSize,
      bool allowPartialEnlargement)
   {
   static char *disableMinPadding = feGetEnv("TR_DisableBoundaryAvoidanceMinPadding");
   if (disableMinPadding)
     return OMR::X86::EnlargementResult(0, 0);

   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement) || requestedEnlargementSize < 1)
      return OMR::X86::EnlargementResult(0, 0);

   int32_t enlargementSize = std::min(requestedEnlargementSize, maxEnlargementSize);

   _minPaddingLength += enlargementSize;
   setEstimatedBinaryLength(getEstimatedBinaryLength() + enlargementSize);
   return OMR::X86::EnlargementResult(enlargementSize, enlargementSize);
   }


int32_t TR::X86BoundaryAvoidanceInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   uint8_t totalLength = _minPaddingLength;

   // Conceivably, each atomic region could add length-1 bytes of padding
   // TODO:AMD64: This is usually very conservative; can we do better?
   //
   for (const TR_AtomicRegion *cur=_atomicRegions; cur->getLength() > 0; cur++)
      {
      totalLength += cur->getLength()-1;
      }

   totalLength = std::max(totalLength, _boundarySpacing);
   totalLength = std::min(totalLength, _maxPadding);
   totalLength += getSizeOfProtectiveNop();
   setEstimatedBinaryLength(totalLength);
   return currentEstimate + getEstimatedBinaryLength();
   }


uint8_t *TR::X86BoundaryAvoidanceInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();

   int32_t modulo = ((uintptrj_t)(instructionStart + _minPaddingLength)) % _boundarySpacing;
   int32_t padLength = 0;

   const TR_AtomicRegion *cur = _atomicRegions;
   while (cur->getLength() > 0)
      {
      TR_ASSERT(cur->getLength() <= _boundarySpacing, "Atomic regions of %d bytes can't fit between %d-byte boundaries", cur->getLength(), _boundarySpacing);
      int32_t start = cur->getStart() + padLength + modulo;
      int32_t end   = start + cur->getLength()-1;
      if (start / _boundarySpacing == end / _boundarySpacing)
         {
         // This atomic region can be safely patched
         //
         cur++;
         }
      else
         {
         // Atomic region crosses a boundary; adjust padLength and start over
         //
         int32_t newPadLength = betterPadLength(padLength, cur, start);
         if (newPadLength > _maxPadding)
            {
            // Can't accomodate this region, so skip it
            }
         else
            {
            padLength = newPadLength;
            }

         // Restart checking atomic regions
         //
         cur = _atomicRegions;
         }
      }

   setBinaryLength(_minPaddingLength + padLength + getSizeOfProtectiveNop());
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   uint8_t * afterPadding = cg()->generatePadding(instructionStart, _minPaddingLength + padLength, this);
   return cg()->generatePadding(afterPadding, getSizeOfProtectiveNop(), this);
   }


// -----------------------------------------------------------------------------
// TR::X86PatchableCodeAlignment

int32_t
TR::X86PatchableCodeAlignmentInstruction::betterPadLength(
      int32_t oldPadLength,
      const TR_AtomicRegion *unaccommodatedRegion,
      int32_t unaccommodatedRegionStart)
   {
   int32_t result = TR::X86BoundaryAvoidanceInstruction::betterPadLength(oldPadLength, unaccommodatedRegion, unaccommodatedRegionStart);
   TR_ASSERT(result < getBoundarySpacing(), "Patchable code has no viable padding that will let it be updated atomically");
   return result;
   }


// -----------------------------------------------------------------------------
// TR::X86AlignmentInstruction:: member functions

OMR::X86::EnlargementResult
TR::X86AlignmentInstruction::enlarge(
      int32_t requestedEnlargementSize,
      int32_t maxEnlargementSize,
      bool allowPartialEnlargement)
   {
   static char *disableMinPadding = feGetEnv("TR_DisableAlignmentMinPadding");
   if (disableMinPadding)
     return OMR::X86::EnlargementResult(0, 0);

   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement) || requestedEnlargementSize < 1)
      return OMR::X86::EnlargementResult(0, 0);

   int32_t enlargementSize = std::min(requestedEnlargementSize, maxEnlargementSize);

   _minPaddingLength += enlargementSize;
   setEstimatedBinaryLength(getEstimatedBinaryLength() + enlargementSize);
   return OMR::X86::EnlargementResult(enlargementSize, enlargementSize);
   }

int32_t TR::X86AlignmentInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   // Worst case padding is _boundary-1 regardless of margin
   //
   setEstimatedBinaryLength(_boundary - 1 + _minPaddingLength);
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t *TR::X86AlignmentInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor = instructionStart;

   intptrj_t paddingLength = cg()->alignment(cursor + _minPaddingLength, _boundary, _margin);
   cursor = cg()->generatePadding(cursor, _minPaddingLength + paddingLength, this);

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86LabelInstruction:: member functions

void
TR::X86LabelInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (!getOpCode().hasRelativeBranchDisplacement() &&
       !(getOpCodeValue() == LABEL))
      {
      if (getReloType() == TR_AbsoluteMethodAddress)
         {
         cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress, __FILE__, __LINE__, getNode());
         }
      }
   }


uint8_t *TR::X86LabelInstruction::generateBinaryEncoding()
   {

   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   TR::LabelSymbol *label = getLabelSymbol();
   uint8_t *cursor = instructionStart;
   TR::Compilation *comp = cg()->comp();

   uint8_t *immediateCursor;

   if (getOpCode().hasRelativeBranchDisplacement())
      {
      int32_t distance;
      if (label == NULL)
         {
         // Dummy instruction - stays a long branch and gets a zero distance
         //
         cursor = getOpCode().binary(instructionStart, self()->rexBits());
         immediateCursor = cursor;
         *(uint32_t *)cursor = 0;
         cursor += 4;
         }
      else
         {
         if (label->getCodeLocation() != NULL)
            {
            // Actual distance if target address is known exactly.
            //
            distance = label->getCodeLocation() - (cursor + IA32LengthOfShortBranch);
            }
         else
            {
            // Conservative estimate of distance if target address is not known exactly.
            // (e.g., a forward relative branch)
            //
            distance = cg()->getBinaryBufferStart()

                       // +4 == Temporary and possibly incomplete fix for WebSphere problem,
                       //       the buffer start is -4 from the start of the method
                       //
                       + 4
                       + label->getEstimatedCodeLocation()
                       - (cursor + IA32LengthOfShortBranch +
                        cg()->getAccumulatedInstructionLengthError());
            }

         TR_ASSERT(getOpCodeValue() != XBEGIN4 || !_permitShortening, "XBEGIN4 cannot be shortened and can only be used with a label instruction that cannot shorten - use generateLongLabel!\n");

         if (distance >= -128 && distance <= 127 &&
             getOpCode().isBranchOp() && _permitShortening)
            {
            // Convert long branch to short branch.
            //
            if (!getOpCode().isShortBranchOp())
               {
               getOpCode().convertLongBranchToShort();
               }

            cursor = getOpCode().binary(instructionStart, self()->rexBits());
            immediateCursor = cursor;

            if (label->getCodeLocation() != NULL)
               {
               *(int8_t *)cursor = (int8_t)distance;
               }
            else
               {
               cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative8BitRelocation(cursor, label));
               *cursor = (uint8_t)(-(intptrj_t)(cursor+1));
               }

            cursor += 1;
            }
         else
            {
            if (getOpCode().isShortBranchOp())
               {
               // If a short branch was explicitly requested but the displacement
               // is too large then abort the compile.  This should NEVER happen
               // in practice because short form branch instructions should
               // only be used in controlled circumstances (e.g., PICs) where
               // distances are guaranteed.
               //
               TR_ASSERT(0, "short form branch displacement too large: instr=%p, distance=%d\n", this, distance);
               comp->failCompilation<TR::CompilationException>("short form branch displacement too large");
               }

            cursor = getOpCode().binary(instructionStart, self()->rexBits());
            immediateCursor = cursor;

            if (label->getCodeLocation() != NULL)
               {
               *(int32_t *)cursor = distance + IA32LengthOfShortBranch - getOpCode().length(self()->rexBits()) - 4;
               }
            else
               {
               cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
               *(uint32_t *)cursor = (uint32_t)(-(intptrj_t)(cursor+4));
               }

            cursor += 4;
            }
         }
      }
   else if (getOpCodeValue() == LABEL)
      {
      label->setCodeLocation(instructionStart);
      immediateCursor = cursor;
      }
   else // assume absolute code address referencing instruction like push label
      {
      cursor = getOpCode().binary(instructionStart, self()->rexBits());
      immediateCursor = cursor;
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, label));
      *(uint32_t *)cursor = 0;
      cursor += 4;
      }

   addMetaDataForCodeAddress(immediateCursor);

   setBinaryLength(cursor - instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   setBinaryEncoding(instructionStart);
   return cursor;
   }


uint8_t TR::X86LabelInstruction::getBinaryLengthLowerBound()
   {
   if (getOpCodeValue() == LABEL)
      return 0;

   if (getOpCodeValue() == VirtualGuardNOP)
      {
      return 0;
      }

   if (getOpCode().isBranchOp())
      return getOpCode().length(self()->rexBits()) + (_permitShortening ? 0 : 4);
   return getOpCode().length(self()->rexBits()) + 4; // assume absolute code reference
   }


OMR::X86::EnlargementResult
TR::X86LabelInstruction::enlarge(
      int32_t requestedEnlargementSize,
      int32_t maxEnlargementSize,
      bool allowPartialEnlargement)
   {
   static char *disableJMPExpansion = feGetEnv("TR_DisableJMPExpansion");
   if (disableJMPExpansion)
      return OMR::X86::EnlargementResult(0, 0);

   // enlargement for label instructions operates on the following principle:
   // in the X86 codegen a branch instruction has a size eg JMP4 and that size is the
   // maximum requested size - we never make a JMP1 a JMP4 for example - we trust
   // people choosing a lower maximum size know what they are doing
   // the binary encoding will, usually try to use the smallest instruction it can
   // and so if a JMP4 can be encoded as a JMP1 it will be
   // this function prevents this instruction shrinking by calling prohibitShortening()
   // this increases the size lowerbound of the instruction, thus "enlarging" the
   // instruction and, hopefully, making it a suitable candidate for patching

   if (!getOpCode().isBranchOp())
      return OMR::X86::EnlargementResult(0, 0);

   if (!getOpCode().hasIntImmediate() || !_permitShortening || getOpCodeValue() == VirtualGuardNOP)
      return OMR::X86::EnlargementResult(0, 0);

   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement)
       || (requestedEnlargementSize > 4 && !allowPartialEnlargement)
       || maxEnlargementSize < 4)
      return OMR::X86::EnlargementResult(0, 0);

   prohibitShortening();
   setEstimatedBinaryLength(getEstimatedBinaryLength() + 4);
   return OMR::X86::EnlargementResult(4, 4);
   }

int32_t TR::X86LabelInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   TR::Compilation *comp = cg()->comp();
   if (getOpCode().isBranchOp())
      {
      uint32_t immediateLength = 1;
      if (getOpCode().hasIntImmediate())
         {
         immediateLength = 4;
         if (getLabelSymbol() && getLabelSymbol()->getEstimatedCodeLocation())
            {
            int32_t distance = getLabelSymbol()->getEstimatedCodeLocation() - (currentEstimate + IA32LengthOfShortBranch);
            if (distance >= -128 && distance < 0 && _permitShortening)
               {
               immediateLength = 0; // really 1, but for conditional branches (all excep JMP4) the opcode entry will be 1 too big for short branch
                                    // because the short branch op is 1 byte, but the long branch op for conditionals is 2
               if (getOpCodeValue() == JMP4)
                  {
                  immediateLength = 1;
                  }
               }
            }
         }
      setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + immediateLength);
      }
   else if (getOpCodeValue() == LABEL)
      {
      if (!comp->getOption(TR_DisableLateEdgeSplitting) && _symbol->getVMThreadRestoringLabel() && _symbol->isVMThreadLive())
         {
         // If this label is the second of a split pair, and vmThread is live at this label,
         // then we need a load of the vmThread immediately before this label.
         // Generate it, and include its size estimate.
         //
         TR::Register         *vmThreadVirtualReg = cg()->getVMThreadRegister();
         TR::RealRegister     *vmThreadRealReg    = cg()->machine()->getX86RealRegister(cg()->getProperties().getMethodMetaDataRegister());
         if (vmThreadRealReg->getHasBeenAssignedInMethod() || !performTransformation(comp, "O^O LATE EDGE SPLITTING: Skip vmThread reload at %p because ebp was never assigned\n", this)) // TODO: could we use getModified instead?
            {
            if (vmThreadVirtualReg->getBackingStorage() == NULL)
               {
               // copied from RegisterDependency.cpp
               vmThreadVirtualReg->setBackingStorage(cg()->allocateVMThreadSpill());
               cg()->getSpilledIntRegisters().push_front(vmThreadVirtualReg);
               }
            // Set spill instruction to the "spill in prolog" value.
            cg()->setVMThreadSpillInstruction((TR::Instruction *)0xffffffff);
            if (cg()->getTraceRAOption(TR_TraceRALateEdgeSplitting))
               traceMsg(comp, "LATE EDGE SPLITTING: Store ebp in prologue\n");

            TR::Instruction *ebpLoad = generateRegMemInstruction(getPrev(), LRegMem(), vmThreadRealReg,
                                                                  generateX86MemoryReference(vmThreadVirtualReg->getBackingStorage()->getSymbolReference(), cg()), cg());
            currentEstimate = ebpLoad->estimateBinaryLength(currentEstimate);
            if (cg()->getTraceRAOption(TR_TraceRALateEdgeSplitting))
               traceMsg(comp, "LATE EDGE SPLITTING: Added vmThread rematerialization %s before label %s\n",
                  cg()->getDebug()->getName(ebpLoad),
                  cg()->getDebug()->getName(_symbol));

            TR::Instruction *bump = cg()->generateDebugCounter(ebpLoad, "cg.lateSplitEdges:reloaded", 1, TR::DebugCounter::Expensive);
            if (bump != ebpLoad)
               currentEstimate = bump->estimateBinaryLength(currentEstimate);
            }
         }
      getLabelSymbol()->setEstimatedCodeLocation(currentEstimate);
      }
   else // assume absolute code reference
      {
      setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + 4); // full offset for absolute address reference
      }
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86FenceInstruction:: member functions

void
TR::X86FenceInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   cg()->addProjectSpecializedRelocation( cursor, 0, NULL, TR_AbsoluteMethodAddress,
      __FILE__, __LINE__, _fenceNode);

   }


uint8_t *TR::X86FenceInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   int i;

   if (_fenceNode->getRelocationType() == TR_AbsoluteAddress)
      {
      for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
         {
         *(uint8_t **)(_fenceNode->getRelocationDestination(i)) = instructionStart;
         }
      }
   else if (_fenceNode->getRelocationType() == TR_ExternalAbsoluteAddress)
      {
      for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
         {
         *(uint8_t **)(_fenceNode->getRelocationDestination(i)) = instructionStart;
         addMetaDataForCodeAddress( (uint8_t *)_fenceNode->getRelocationDestination(i) );
         }
      }
   else if (_fenceNode->getRelocationType() == TR_EntryRelative32Bit)
      {
      for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
         {
         *(uint32_t *)(_fenceNode->getRelocationDestination(i)) = cg()->getCodeLength();
         }
      }
   else // entryrelative16bit
      {
      for (i = 0; i < _fenceNode->getNumRelocations(); ++i)
         {
         *(uint16_t *)(_fenceNode->getRelocationDestination(i)) = (uint16_t)cg()->getCodeLength();
         }
      }

   setBinaryEncoding(instructionStart);

   return instructionStart;
   }


// -----------------------------------------------------------------------------
// TR::X86RestoreVMThreadInstruction:: member functions

int32_t TR::X86RestoreVMThreadInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   if (TR::Compiler->target.is64Bit())
      setEstimatedBinaryLength(9);
   else
      setEstimatedBinaryLength(7);
   return currentEstimate + getEstimatedBinaryLength();
   }

uint8_t TR::X86RestoreVMThreadInstruction::getBinaryLengthLowerBound()
   {
   if (TR::Compiler->target.is64Bit())
      return 9;
   else
      return 7;
   }

uint8_t *TR::X86RestoreVMThreadInstruction::generateBinaryEncoding()
   {
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();
   uint8_t *cursor           = instructionStart;

   if (TR::Compiler->target.is64Bit())
      {
      // mov  rbp,qword ptr fs:[00000000h] ; 64 48 8B 2C 25 00 00 00 00
      *cursor++ = 0x64; *cursor++ = 0x48; *cursor++ = 0x8b; *cursor++ = 0x2c; *cursor++ = 0x25;
      *cursor++ = 0x00; *cursor++ = 0x00; *cursor++ = 0x00; *cursor++ = 0x00;
      }
   else
      {
      // mov ebp,dword ptr fs:[0] ; 64 8b 2d 00 00 00 00
      *cursor++ = 0x64; *cursor++ = 0x8b; *cursor++ = 0x2d;
      *cursor++ = 0x00; *cursor++ = 0x00; *cursor++ = 0x00; *cursor++ = 0x00;
      }

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }

#ifdef J9_PROJECT_SPECIFIC
// -----------------------------------------------------------------------------
// TR::X86VirtualGuardNOPInstruction

int32_t TR::X86VirtualGuardNOPInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(TR::Compiler->target.is64Bit() ? 5 : 6); //TODO:AMD64: What if patched instruction needs a Rex?  Is 6 enough?
   return currentEstimate + getEstimatedBinaryLength();
   }


uint8_t *TR::X86VirtualGuardNOPInstruction::generateBinaryEncoding()
   {
   TR::Compilation *comp = cg()->comp();
   uint8_t *instructionStart = cg()->getBinaryBufferCursor();

   uint8_t        *cursor           = instructionStart;
   TR::LabelSymbol *label            = getLabelSymbol();
   int32_t         offset;
   uint8_t        *patchCursor      = cursor;
   TR::Instruction *guardForPatching = cg()->getVirtualGuardForPatching(this);

   static char *disableOSRNOPs = feGetEnv("TR_disableOSRGuardNOPs");
   if (disableOSRNOPs && getNode()->isOSRGuard())
      {
      setBinaryLength(0);
      setBinaryEncoding(instructionStart);
      cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength());
      return cursor;
      }

   // a previous guard is patching to the same destination and we can recycle the patch point
   // so setup the patching location to use this previous guard and generate no instructions
   // ourselves
   if (guardForPatching != this)
      {
      _site->setLocation(guardForPatching->getBinaryEncoding());
      setBinaryLength(0);
      setBinaryEncoding(cursor);
      if (label->getCodeLocation() == NULL)
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation
                                   ((uint8_t *) (&_site->getDestination()), label));
         }
      else
         {
         _site->setDestination(label->getCodeLocation());
         }
      cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
      return cursor;
      }

   _site->setLocation(patchCursor);
   if (label->getCodeLocation() == NULL)
      {
      // Conservative offset estimate
      //
      offset = cg()->getBinaryBufferStart() +
               label->getEstimatedCodeLocation() -
               (patchCursor + IA32LengthOfShortBranch +
                cg()->getAccumulatedInstructionLengthError());

      // Can't call _site->setDestination because we don't know the destination
      // yet, so use a relocation instead.
      //
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation
                                ((uint8_t *) (&_site->getDestination()), label));
      }
   else
      {
      offset = label->getCodeLocation() - (patchCursor + IA32LengthOfShortBranch);
      _site->setDestination(label->getCodeLocation());
      }

   // guards that do not require atomic patching have a more relaxed sizing constraing since they are only patched while all threads are stopped
   bool requiresAtomicPatching = this->requiresAtomicPatching();
   uint8_t next = requiresAtomicPatching ?
                  cg()->sizeOfInstructionToBePatched(this) :
                  cg()->sizeOfInstructionToBePatchedHCRGuard(this);

   //If the current instruction is already a patchable instruction, we cannot use it as a vgnop guard site.
   //We will need to insert nops in this case.
   //
   TR::Instruction *instToBePatched = cg()->getInstructionToBePatched(this);

   int32_t shortJumpWidth = 2;
   int32_t longJumpWidth = TR::Compiler->target.is64Bit() ? 5 : 6;

   _nopSize = 0;

   if (offset >= -128 && offset <= 127)
      {
      if (next < shortJumpWidth || !instToBePatched)
         _nopSize = shortJumpWidth;
      }
   else
      {
      if (next < longJumpWidth || !instToBePatched)
         _nopSize = longJumpWidth;
      }

   if (cg()->nopsAlsoProcessedByRelocations())
      {
      _nopSize = longJumpWidth;
      }
   // in the case of an HCR guard merged with a Profiled Guard, we know the instruction to patch
   // is the Profield Guard conditional jump. Because the HCR Guard jumps to the same destination
   // we are guaranteed that the patch site will be big enough for the required jump and so we
   // force the patch size to longJumpWidth to suppress NOP emission
   else if (getNode()->isProfiledGuard())
      {
      _nopSize = 0;
      }

   // HCR guards are special because they occur in stop the world so we don't require atomic
   // nop sequences - all other guards require a single instruction to prevent possible inconsistent
   // state seen by the processor while decoding when patching occurs
   if (instToBePatched && _nopSize > 0 && !cg()->nopsAlsoProcessedByRelocations())
      {
      TR_ASSERT(_nopSize > next, "Emitting nops when the instruction is big enough is always wrong!");
      OMR::X86::EnlargementResult enlargement = instToBePatched->enlarge(_nopSize - next, getEstimatedBinaryLength(), !requiresAtomicPatching);
      if (enlargement.getPatchGrowth() + next >= _nopSize)
         {
         _nopSize = 0;
         }
      else if (!requiresAtomicPatching)
         {
         _nopSize -= next + enlargement.getPatchGrowth();
         if (_nopSize < 0)
            _nopSize = 0;
         }
      else
         {
         TR_ASSERT(enlargement.getPatchGrowth() == 0, "Instructions should only partially enlarge when dealing with an HCR guard - any other partial enlargement is a waste of code cache!");
         }
      // donate some of our estimated size to the newly englarged instruction
      // the instruction to be patched will have already taken the extra size when computing enlarge
      if (enlargement.getEncodingGrowth() > 0)
         setEstimatedBinaryLength(getEstimatedBinaryLength() - enlargement.getEncodingGrowth());
      }

   cursor = cg()->generatePadding(cursor, _nopSize, this,
             requiresAtomicPatching ? TR_AtomicNoOpPadding : TR_NoOpPadding);

   setBinaryLength(cursor - instructionStart);
   setBinaryEncoding(instructionStart);
   cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
   return cursor;
   }
#endif

// -----------------------------------------------------------------------------
// TR::X86ImmInstruction:: member functions

void
TR::X86ImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (getOpCode().hasIntImmediate())
      {
      if (needsAOTRelocation())
         {
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, 0, TR_BodyInfoAddress, cg()),
                                __FILE__,
                                __LINE__,
                                getNode());
         }

      if (getReloKind() != -1) // TODO: need to change Body info one to use this
         {
         switch (getReloKind())
            {
            case TR_StaticRamMethodConst:
            case TR_VirtualRamMethodConst:
            case TR_SpecialRamMethodConst:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getNode()->getSymbolReference(),
                  (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() , (TR_ExternalRelocationTargetKind) getReloKind(), cg()),
                  __FILE__, __LINE__, getNode());
               break;
            case TR_MethodPointer:
               if (getNode() && getNode()->getInlinedSiteIndex() == -1 && 
                  (void *) getSourceImmediate() == cg()->comp()->getCurrentMethod()->resolvedMethodAddress())
                  setReloKind(TR_RamMethod);
               // intentional fall-through
            case TR_RamMethod:
               // intentional fall-through
            case TR_ClassPointer:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                           (uint8_t*)getNode(),
                                                                           (TR_ExternalRelocationTargetKind) _reloKind,
                                                                           cg()),
                  __FILE__, 
                  __LINE__, 
                  getNode());
                  break;
            default:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, 0, (TR_ExternalRelocationTargetKind)getReloKind(), cg()),
                  __FILE__,
                  __LINE__,
                  getNode());
            }
         }

      if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      }

   }


uint8_t* TR::X86ImmInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *immediateCursor = cursor;

   if (getOpCode().hasIntImmediate())
      {
      *(int32_t *)cursor = (int32_t)getSourceImmediate();
      if (getOpCode().isCallImmOp())
         {
         *(int32_t *)cursor -= (int32_t)(intptrj_t)(cursor + 4);
         }
      cursor += 4;
      }
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      {
      *(int8_t *)cursor = (int8_t)getSourceImmediate();
      cursor += 1;
      }
   else
      {
      *(int16_t *)cursor = (int16_t)getSourceImmediate();
      cursor += 2;
      }

   addMetaDataForCodeAddress(immediateCursor);
   return cursor;
   }

uint8_t TR::X86ImmInstruction::getBinaryLengthLowerBound()
   {
   uint8_t len = getOpCode().length(self()->rexBits());
   if (getOpCode().hasIntImmediate())
      len += 4;
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      len += 1;
   else if (getOpCode().hasShortImmediate())
      len += 2;
   return len;
   }

int32_t TR::X86ImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   uint32_t immediateLength = 1;
   if (getOpCode().hasIntImmediate())
      {
      immediateLength = 4;
      }
   else if (getOpCode().hasShortImmediate())
      {
      immediateLength = 2;
      }
   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + immediateLength);

   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86ImmSnippetInstruction:: member functions

void
TR::X86ImmSnippetInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (getOpCode().hasIntImmediate())
      {
      if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      }

   }


uint8_t* TR::X86ImmSnippetInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *immediateCursor = cursor;

   if (getOpCode().hasIntImmediate())
      {
      *(int32_t *)cursor = (int32_t)getSourceImmediate();

      if (getUnresolvedSnippet() != NULL)
         {
         getUnresolvedSnippet()->setAddressOfDataReference(cursor);
         }

      if (getOpCode().isCallImmOp())
         {
         *(int32_t *)cursor -= (int32_t)(intptrj_t)(cursor + 4);
         }
      cursor += 4;
      }
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      {
      *(int8_t *)cursor = (int8_t)getSourceImmediate();
      cursor += 1;
      }
   else
      {
      *(int16_t *)cursor = (int16_t)getSourceImmediate();
      cursor += 2;
      }

   addMetaDataForCodeAddress(immediateCursor);
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86ImmSymInstruction:: member functions

int32_t TR::X86ImmSymInstruction::estimateBinaryLength(int32_t currentEstimate)
   {

   currentEstimate = TR::X86ImmInstruction::estimateBinaryLength(currentEstimate);

   return currentEstimate;
   }


void
TR::X86ImmSymInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   TR::Compilation *comp = cg()->comp();

   if (getOpCode().hasIntImmediate())
      {
      TR::Symbol *sym = getSymbolReference()->getSymbol();

      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (getOpCode().isCallImmOp() || getOpCode().isBranchOp())
         {
         TR::MethodSymbol *methodSym = sym->getMethodSymbol();
         TR::ResolvedMethodSymbol *resolvedMethodSym = sym->getResolvedMethodSymbol();
         TR_ResolvedMethod *resolvedMethod = resolvedMethodSym ? resolvedMethodSym->getResolvedMethod() : 0;
         TR::LabelSymbol *labelSym = sym->getLabelSymbol();

         if ( !(resolvedMethod && resolvedMethod->isSameMethod(comp->getCurrentMethod()) && !comp->isDLT()) )
            {
            if (labelSym)
               {
               cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, labelSym));
               }
            else
               {
               if (methodSym && methodSym->isHelper())
                  {
                  cg()->addProjectSpecializedRelocation(cursor, (uint8_t *)getSymbolReference(), NULL, TR_HelperAddress,
                                                                        __FILE__, __LINE__, getNode());
                  }
               else if (methodSym && methodSym->isJNI() && getNode() && getNode()->isPreparedForDirectJNI())
                  {
                  static const int methodJNITypes = 4;
                  static const int reloTypes[methodJNITypes] = {TR_JNIVirtualTargetAddress, 0 /*Interfaces*/, TR_JNIStaticTargetAddress, TR_JNISpecialTargetAddress};
                  int rType = methodSym->getMethodKind()-1;  //method kinds are 1-based
                  TR_ASSERT(reloTypes[rType], "There shouldn't be direct JNI interface calls!");

                  cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getNode()->getSymbolReference(),
                        getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                        (TR_ExternalRelocationTargetKind) reloTypes [rType], cg()),
                        __FILE__, __LINE__, getNode());
                  }
               else
                  {
                  cg()->addProjectSpecializedRelocation(cursor, (uint8_t *)getSymbolReference(), NULL, TR_RelativeMethodAddress,
                                         __FILE__, __LINE__, getNode());
                  }
               }
            }
         }
      else if (getOpCodeValue() == DDImm4)
         {
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                (uint8_t *)(uintptrj_t)getSourceImmediate(),
                                                                getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                               TR_ConstantPool,
                                                               cg()),
                             __FILE__, __LINE__, getNode());
         }
      else if (getOpCodeValue() == PUSHImm4)
         {
         TR::Symbol *symbol = getSymbolReference()->getSymbol();
         if (symbol->isConst())
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t *)getSymbolReference()->getOwningMethod(comp)->constantPool(),
                                                                  getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                  TR_ConstantPool,
                                                                  cg()),
                                   __FILE__, __LINE__, getNode());
            }
         else if (symbol->isClassObject())
            {
            if (cg()->needClassAndMethodPointerRelocations())
               {
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                (uint8_t *)getSymbolReference(),
                                                                getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                TR_ClassAddress,
                                                                cg()), __FILE__,__LINE__, getNode());
               }
            }
         else if (symbol->isMethod())
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t *)getSymbolReference(),
                                                                  getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                  TR_MethodObject,
                                                                  cg()),
                                   __FILE__,__LINE__, getNode());
            }
         else
            {
            TR::StaticSymbol *staticSym = sym->getStaticSymbol();
            if (staticSym && staticSym->isCompiledMethod())
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, 0, TR_RamMethod, cg()),
                                      __FILE__, __LINE__, getNode());
            else if (staticSym && staticSym->isStartPC())
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                   (uint8_t *) (staticSym->getStaticAddress()), TR_AbsoluteMethodAddress, cg()),
                                      __FILE__, __LINE__, getNode());
            else
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                   (uint8_t *)getSymbolReference(), getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_DataAddress, cg()),
                                       __FILE__, __LINE__, getNode());
            }

         }

      }

   }


uint8_t* TR::X86ImmSymInstruction::generateOperand(uint8_t* cursor)
   {
   TR::Compilation *comp = cg()->comp();

   uint8_t *immediateCursor = cursor;

   if (getOpCode().hasIntImmediate())
      {

      TR::Symbol *sym = getSymbolReference()->getSymbol();

      if (sym->isStatic())
         *(intptrj_t *)cursor = (intptr_t)(sym->getStaticSymbol()->getStaticAddress());
      else
         *(int32_t *)cursor = (int32_t)getSourceImmediate();

      if (getOpCode().isCallImmOp() || getOpCode().isBranchOp())
         {
         TR::MethodSymbol *methodSym = sym->getMethodSymbol();
         TR::ResolvedMethodSymbol *resolvedMethodSym = sym->getResolvedMethodSymbol();
         TR_ResolvedMethod *resolvedMethod = resolvedMethodSym ? resolvedMethodSym->getResolvedMethod() : 0;
         TR::LabelSymbol *labelSym = sym->getLabelSymbol();

         intptrj_t targetAddress = (int32_t)getSourceImmediate();

         if (TR::Compiler->target.is64Bit() && comp->getCodeCacheSwitched() && getOpCodeValue() == CALLImm4)
            {
            TR::SymbolReference *calleeSymRef = NULL;

            if (labelSym==NULL)
               calleeSymRef = getSymbolReference();
            else if (getNode() != NULL)
               calleeSymRef = getNode()->getSymbolReference();

            if (calleeSymRef != NULL)
               {
               if (calleeSymRef->getReferenceNumber()>=TR_AMD64numRuntimeHelpers)
                  cg()->fe()->reserveTrampolineIfNecessary(comp, calleeSymRef, true);
               }
            else
               {
               TR_ASSERT(0, "Missing possible re-reservation for trampolines.\n");
               }
            }

         if (resolvedMethod && resolvedMethod->isSameMethod(comp->getCurrentMethod()) && !comp->isDLT())
            {
            // Compute method's jit entry point
            //
            uint8_t *start = cg()->getCodeStart();
            if (TR::Compiler->target.is64Bit())
               {
               start += TR_LinkageInfo::get(start)->getReservedWord();
               TR_ASSERT(IS_32BIT_RIP(start, cursor+4), "Method start must be within RIP range");
               cg()->fe()->reserveTrampolineIfNecessary(comp, getSymbolReference(), true);
               }

            targetAddress = (intptrj_t)start;
            }
         else
            {
            if (!labelSym)
               {
               // TODO:
               //
               // The following is only necessary on 64-bit because of the present way in which CALL
               // instructions are created.  On 32-bit the target is always reachable from the call
               // site, so the actual 32-bit target address is encoded in the immediate field of
               // the instruction.  There is no need to ask the symbol reference for the actual address
               // because all the information is already present in the instruction.
               //
               // On 64-bit, the target address may not fit in a 32-bit address.  The target symbol
               // must be queried to determine the actual target address and then whether it is with
               // the 32-bit displacement range.  Hence, 64-bit relies on a valid target symbol to find
               // the actual target address.
               //
               // Not all CALL instructions have symbol references that describe the actual target.
               // Profiled targets in static IPICs are an example.  The handshake between instruction
               // selection and binary encoding is dubious, and the safe thing to do at this stage is
               // to revert back to the original logic in this method on 32-bit.  Such CALL instructions
               // do not appear on the 64-bit path.
               //
               // This logic/handshake will be cleaned up in a future release.
               //
               if (TR::Compiler->target.is64Bit())
                  {
                  // Obtain the actual target of this call instruction.
                  //
                  // Symbols created for JNI methods have their method address pointing to the RAM method.
                  // Otherwise, the symbol method address points at the actual target address.
                  //
                  TR::Node *symNode = getNode();
                  if (methodSym && methodSym->isJNI() && symNode && symNode->isPreparedForDirectJNI())
                     targetAddress = (uintptrj_t)resolvedMethod->startAddressForJNIMethod(comp);
                  else
                     targetAddress = (intptrj_t)getSymbolReference()->getMethodAddress();
                  }

               bool forceTrampolineUse = false;

               if (methodSym && methodSym->isHelper())
                  {
                  if (!IS_32BIT_RIP(targetAddress, cursor+4) || forceTrampolineUse)
                     {
                     // TODO:AMD64: Consider AOT ramifications
                     targetAddress = cg()->fe()->indexedTrampolineLookup(getSymbolReference()->getReferenceNumber(), (void *)cursor);
                     TR_ASSERT(IS_32BIT_RIP(targetAddress, cursor+4), "Local helper trampoline must be reachable directly.\n");
                     }
                  }
               else if (methodSym && methodSym->isJNI() && getNode() && getNode()->isPreparedForDirectJNI())
                  {
                  if (!IS_32BIT_RIP(targetAddress, cursor+4) || forceTrampolineUse)
                     {
                     TR_ASSERT(0, "We seem to never generate CALLIMM instructions for JNI on 64Bit!!!");
                     }

                  }
               else
                  {
                  // we need to reserve trampoline in all cases, since methods can get recompiled
                  // and we might need one in the future.
                  if (TR::Compiler->target.is64Bit())
                     cg()->fe()->reserveTrampolineIfNecessary(comp, getSymbolReference(), true);

                  if (!IS_32BIT_RIP(targetAddress, cursor+4) || forceTrampolineUse)
                     {
                     targetAddress = cg()->fe()->methodTrampolineLookup(comp, getSymbolReference(), (void *)cursor);

                     TR_ASSERT(IS_32BIT_RIP(targetAddress, cursor+4), "Local method trampoline must be reachable directly.\n");
                     }
                  }
               }
            }

         // Compute relative target displacement.
         //
         *(int32_t *)cursor = (int32_t)(targetAddress - (intptrj_t)(cursor + 4));
         }
      else if (getOpCodeValue() == PUSHImm4)
         {
         TR::Symbol *symbol = getSymbolReference()->getSymbol();
         if (!symbol->isConst() && symbol->isClassObject())
            {
            if (cg()->needClassAndMethodPointerRelocations())
               {
               if (sym->isStatic())
                  {
                  *(intptrj_t *)cursor = (intptrj_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(cg()->comp(), (TR_OpaqueClassBlock*)sym->getStaticSymbol()->getStaticAddress());
                  }
               else
                  {
                  *(int32_t *)cursor = (int32_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(cg()->comp(), (TR_OpaqueClassBlock*)getSourceImmediate());
                  }
               }
            }
         }
      cursor += 4;
      }
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      {
      *(int8_t *)cursor = (int8_t)getSourceImmediate();
      cursor += 1;
      }
   else
      {
      *(int16_t *)cursor = (int16_t)getSourceImmediate();
      cursor += 2;
      }

   addMetaDataForCodeAddress(immediateCursor);
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86RegInstruction:: member functions

uint8_t* TR::X86RegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }
   return cursor;
   }

uint8_t TR::X86RegInstruction::getBinaryLengthLowerBound()
   {
   TR_X86OpCode  &opCode = getOpCode();
   return opCode.length(self()->rexBits());
   }

int32_t TR::X86RegInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   TR_X86OpCode  &opCode = getOpCode();
   setEstimatedBinaryLength(opCode.length(self()->rexBits()) + rexRepeatCount());
   return currentEstimate + getEstimatedBinaryLength();
   }

OMR::X86::EnlargementResult
TR::X86RegInstruction::enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement)
   {
   static char *disableRexExpansion = feGetEnv("TR_DisableREXInstructionExpansion");
   if (disableRexExpansion || TR::comp()->getOption(TR_DisableZealousCodegenOpts))
      return OMR::X86::EnlargementResult(0, 0);

   if (getOpCode().info().supportsAVX() && getOpCode().info().allowsAVX())
      return OMR::X86::EnlargementResult(0, 0); // REX expansion isn't allowed for AVX instructions

   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement) || requestedEnlargementSize < 1)
      return OMR::X86::EnlargementResult(0, 0);

   int32_t enlargementSize = std::min(requestedEnlargementSize, maxEnlargementSize);

   TR::Compilation *comp = TR::comp();
   if (  TR::Compiler->target.is64Bit()
      && !getOpCode().info().hasMandatoryPrefix()
      && performTransformation(comp, "O^O Enlarging instruction %p by %d bytes by repeating the REX prefix\n", this, enlargementSize))
      {
      setRexRepeatCount(enlargementSize);
      setEstimatedBinaryLength(getEstimatedBinaryLength() + enlargementSize);
      return OMR::X86::EnlargementResult(enlargementSize, enlargementSize);
      }

   return OMR::X86::EnlargementResult(0, 0);
   }


// -----------------------------------------------------------------------------
// TR::X86RegRegInstruction:: member functions

uint8_t* TR::X86RegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }
   if (getOpCode().hasSourceRegisterIgnored() == 0)
      {
      applySourceRegisterToModRMByte(modRM);
      }
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86RegImmInstruction:: member functions

void
TR::X86RegImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (getOpCode().hasIntImmediate())
      {
      // TODO: PIC registration code only works for 32-bit platforms as static PIC address is
      // a 32 bit quantity
      //
      bool staticPIC = false;
      if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
         {
         TR_ASSERT(getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticPIC = true;
         }

      // HCR register HCR PIC sites in TR::X86RegImmInstruction::generateBinaryEncoding
      bool staticHCRPIC = false;
      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
         {
         TR_ASSERT(getOpCode().hasIntImmediate(), "StaticHCRPIC: class pointer cannot be smaller than 4 bytes");
         staticHCRPIC = true;
         }

      bool staticMethodPIC = false;
      if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
         staticMethodPIC = true;

      if (staticPIC)
         {
         cg()->jitAdd32BitPicToPatchOnClassUnload(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticHCRPIC)
         {
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *)getSourceImmediate(), TR_HCR, cg()), __FILE__,__LINE__, getNode());
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticMethodPIC)
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) getSourceImmediateAsAddress(), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAdd32BitPicToPatchOnClassUnload(classPointer, (void *) cursor);
         }

      switch (_reloKind)
         {
         case TR_HEAP_BASE:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                    (uint8_t*)TR_HeapBase,
                                                      TR_GlobalValue,
                                                      cg()),
                          __FILE__, __LINE__, getNode());
            break;

         case TR_HEAP_TOP:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                      (uint8_t*)TR_HeapTop,
                                                      TR_GlobalValue,
                                                      cg()),
                          __FILE__, __LINE__, getNode());
            break;
         case TR_HEAP_BASE_FOR_BARRIER_RANGE:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                      (uint8_t*)TR_HeapBaseForBarrierRange0,
                                                      TR_GlobalValue,
                                                      cg()),
                          __FILE__, __LINE__, getNode());
            break;
         case TR_HEAP_SIZE_FOR_BARRIER_RANGE:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                      (uint8_t*)TR_HeapSizeForBarrierRange0,
                                                      TR_GlobalValue,
                                                      cg()),
                          __FILE__, __LINE__, getNode());
            break;
         case TR_ACTIVE_CARD_TABLE_BASE:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                      (uint8_t*)TR_ActiveCardTableBase,
                                                      TR_GlobalValue,
                                                      cg()),
                          __FILE__, __LINE__, getNode());
            break;

         case TR_MethodPointer:
            if (getNode() && getNode()->getInlinedSiteIndex() == -1 &&
               (void *) getSourceImmediate() == cg()->comp()->getCurrentMethod()->resolvedMethodAddress())
               setReloKind(TR_RamMethod);
            // intentional fall-through
         case TR_RamMethod:
            // intentional fall-through
         case TR_ClassPointer:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                              (uint8_t*)getNode(),
                                                              (TR_ExternalRelocationTargetKind) _reloKind,
                                                              cg()),
                                   __FILE__, __LINE__, getNode());
            break;

         default:
            break;
         }

      }

   }

uint8_t* TR::X86RegImmInstruction::generateOperand(uint8_t* cursor)
   {
   TR::Compilation *comp = cg()->comp();

   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }

   uint8_t *immediateCursor = cursor;

   if (getOpCode().hasIntImmediate())
      {
      *(int32_t *)cursor = (int32_t)getSourceImmediate();
      cursor += 4;
      }
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      {
      *(int8_t *)cursor = (int8_t)getSourceImmediate();
      cursor += 1;
      }
   else
      {
      *(int16_t *)cursor = (int16_t)getSourceImmediate();
      cursor += 2;
      }

   addMetaDataForCodeAddress(immediateCursor);
   return cursor;
   }

uint8_t TR::X86RegImmInstruction::getBinaryLengthLowerBound()
   {
   uint8_t len = getOpCode().length(self()->rexBits());

   if (getOpCode().hasIntImmediate()) len += 4;
   else if (getOpCode().hasShortImmediate()) len += 2;
   else len += 1;
   return len;
   }

int32_t TR::X86RegImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   uint32_t immediateLength = 1;
   if (getOpCode().hasIntImmediate())
      {
      immediateLength = 4;
      }
   else if (getOpCode().hasShortImmediate())
      {
      immediateLength = 2;
      }

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + rexRepeatCount() + immediateLength);
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86RegImmSymInstruction:: member functions

void
TR::X86RegImmSymInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
      {
      cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
      }

   TR::Symbol *symbol = getSymbolReference()->getSymbol();
   TR_RelocationRecordInformation *recordInfo;
   switch (getReloKind())
      {
      case TR_ConstantPool:
         TR_ASSERT(symbol->isConst(), "assertion failure");
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                   (uint8_t *)getSymbolReference()->getOwningMethod(comp)->constantPool(),
                                                                                   getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                   (TR_ExternalRelocationTargetKind)getReloKind(),
                                                                                   cg()),
                                __FILE__,
                                __LINE__,
                                getNode());
         break;
      case TR_ClassAddress:
      case TR_ClassObject:
         {
         if (cg()->needClassAndMethodPointerRelocations())
            {
                //udeshs: this should be temporary??
            TR_ASSERT(!(getSymbolReference()->isUnresolved() && !symbol->isClassObject()), "expecting a resolved symbol for this instruction class!\n");

            *(int32_t *)cursor = (int32_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(cg()->comp(), (TR_OpaqueClassBlock*)getSourceImmediate());
             cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                   (uint8_t *)getSymbolReference(),
                                                                                   getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                   (TR_ExternalRelocationTargetKind)getReloKind(),
                                                                                   cg()),
                                __FILE__,
                                __LINE__,
                                getNode());
            }
         }
         break;
      case TR_MethodObject:
      case TR_DataAddress:
         TR_ASSERT(!(getSymbolReference()->isUnresolved() && !symbol->isClassObject()), "expecting a resolved symbol for this instruction class!\n");

         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                   (uint8_t *)getSymbolReference(),
                                                                                   getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                   (TR_ExternalRelocationTargetKind)getReloKind(),
                                                                                   cg()),
                                __FILE__,
                                __LINE__,
                                getNode());
         break;
      case TR_MethodPointer:
         if (getNode() && getNode()->getInlinedSiteIndex() == -1 &&
            (void *) getSourceImmediate() == cg()->comp()->getCurrentMethod()->resolvedMethodAddress())
            setReloKind(TR_RamMethod);
         // intentional fall-through
      case TR_ClassPointer:
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                           (uint8_t*)getNode(),
                                                           (TR_ExternalRelocationTargetKind)getReloKind(),
                                                           cg()),
                                __FILE__, __LINE__, getNode());
         break;

      default:
         TR_ASSERT(0, "invalid relocation kind for TR::X86RegImmSymInstruction");
      }

   }


uint8_t* TR::X86RegImmSymInstruction::generateOperand(uint8_t* cursor)
   {
   TR::Compilation *comp = cg()->comp();

   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }

   TR_ASSERT(getOpCode().hasIntImmediate(), "expecting a 4 byte address for this instruction class!\n");
   TR_ASSERT(getSymbolReference(), "expecting a symbol reference for this instruction class!\n");

   *(int32_t *)cursor = (int32_t)getSourceImmediate();

   addMetaDataForCodeAddress(cursor);

   cursor += 4;
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86RegRegImmInstruction:: member functions

void TR::X86RegRegImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (getOpCode().hasIntImmediate())
      {
      if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      }
   }


uint8_t* TR::X86RegRegImmInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }
   if (getOpCode().hasSourceRegisterIgnored() == 0)
      {
      applySourceRegisterToModRMByte(modRM);
      }

   uint8_t *immediateCursor = cursor;
   if (getOpCode().hasIntImmediate())
      {
      *(int32_t *)cursor = (int32_t)getSourceImmediate();
      cursor += 4;
      }
   else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
      {
      *(int8_t *)cursor = (int8_t)getSourceImmediate();
      cursor += 1;
      }
   else
      {
      *(int16_t *)cursor = (int16_t)getSourceImmediate();
      cursor += 2;
      }

   addMetaDataForCodeAddress(immediateCursor);
   return cursor;
   }


uint8_t TR::X86RegRegImmInstruction::getBinaryLengthLowerBound()
   {
   uint8_t len = getOpCode().length(self()->rexBits());

   if (getOpCode().hasIntImmediate()) len += 4;
   else if (getOpCode().hasShortImmediate()) len += 2;
   else len += 1;
   return len;
   }

int32_t TR::X86RegRegImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   uint32_t immediateLength = 1;
   if (getOpCode().hasIntImmediate())
      {
      immediateLength = 4;
      }
   else if (getOpCode().hasShortImmediate())
      {
      immediateLength = 2;
      }
   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + immediateLength);
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86MemInstruction:: member functions

bool TR::X86MemInstruction::needsLockPrefix()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);
   return getOpCode().needsLockPrefix() || (barrier & LockPrefix);
   }

uint8_t* TR::X86MemInstruction::generateOperand(uint8_t* cursor)
   {
   return getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   }

uint8_t TR::X86MemInstruction::getBinaryLengthLowerBound()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   uint8_t length = 0;
   if (getOpCode().needsLockPrefix() || (barrier & LockPrefix))
      length++;

   length += getMemoryReference()->getBinaryLengthLowerBound(cg());

   if (barrier & NeedsExplicitBarrier)
      length += getMemoryBarrierBinaryLengthLowerBound(barrier, cg());

   return getOpCode().length(self()->rexBits()) + length;
   }

int32_t TR::X86MemInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);
   int32_t length = 0;
   if (getOpCode().needsLockPrefix() || (barrier & LockPrefix))
      length++;

   length += getMemoryReference()->estimateBinaryLength(cg());

   if (barrier & NeedsExplicitBarrier)
      length += estimateMemoryBarrierBinaryLength(barrier, cg());

   int32_t patchBoundaryPadding = (TR::Compiler->target.isSMP() && getMemoryReference()->getSymbolReference().isUnresolved()) ? 1 : 0;

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + length + patchBoundaryPadding);

   return currentEstimate + getEstimatedBinaryLength();
   }

OMR::X86::EnlargementResult
TR::X86MemInstruction::enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement)
   {
   TR::Compilation *comp = TR::comp();
   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement) || requestedEnlargementSize < 1)
      return OMR::X86::EnlargementResult(0, 0);

   OMR::X86::EnlargementResult result = getMemoryReference()->enlarge(cg(), requestedEnlargementSize,
                                                          maxEnlargementSize, allowPartialEnlargement);
   if (result.getEncodingGrowth() > 0)
      setEstimatedBinaryLength(getEstimatedBinaryLength() + result.getEncodingGrowth());

   return result;
   }

// -----------------------------------------------------------------------------
// TR::X86MemImmInstruction:: member functions

void
TR::X86MemImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {

   if (getOpCode().hasIntImmediate())
      {
      TR::Compilation *comp = cg()->comp();

      // TODO: PIC registration code only works for 32-bit platforms as static PIC address is
      // a 32 bit quantity
      //
      bool staticPIC = false;
      if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
         {
         TR_ASSERT(getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticPIC = true;
         }

      // HCR register HCR pic sites in TR::X86MemImmInstruction::generateBinaryEncoding
      bool staticHCRPIC = false;
      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
         {
         TR_ASSERT(getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticHCRPIC = true;
         }

      bool staticMethodPIC = false;
      if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
         staticMethodPIC = true;

      if (staticPIC)
         {
         cg()->jitAdd32BitPicToPatchOnClassUnload(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      if (staticHCRPIC)
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      if (staticMethodPIC)
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) getSourceImmediateAsAddress(), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAdd32BitPicToPatchOnClassUnload(classPointer, (void *) cursor);
         }

      if (_reloKind == TR_ClassAddress && cg()->needClassAndMethodPointerRelocations())
         {
         TR_ASSERT(getNode(), "node expected to be non-NULL here");
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                       (uint8_t *)getNode()->getSymbolReference(),
                                                       (uint8_t *)getNode()->getInlinedSiteIndex(),
                                                       TR_ClassAddress,
                                                       cg()),
                        __FILE__,__LINE__, getNode());
         }

      }

   }


uint8_t* TR::X86MemImmInstruction::generateOperand(uint8_t* cursor)
   {
   TR::Compilation *comp = cg()->comp();

   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());

   if (cursor)
      {
      uint8_t *immediateCursor = cursor;

      if (getOpCode().hasIntImmediate())
         {
         *(int32_t *)cursor = (int32_t)getSourceImmediate();
         cursor += 4;
         }
      else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
         {
         *(int8_t *)cursor = (int8_t)getSourceImmediate();
         cursor += 1;
         // If this memory reference is about my count for recompile,
         // then it's the cmp instruction that I need to patch
         if (comp->getOption(TR_EnableGCRPatching))
            {
            TR::Node *node = this->getNode();
            if (node && (node->getOpCodeValue() == TR::ificmpeq || node->getOpCodeValue() == TR::ificmpne) /*node->getNumChildren() > 0*/)
               {
               if (node->getFirstChild()->getOpCodeValue() == TR::iload)
                  {
                  TR::SymbolReference *symref = node->getFirstChild()->getSymbolReference();
                  if (symref)
                     {
                     TR::Symbol *symbol = symref->getSymbol();
                     if (symbol && symbol->isCountForRecompile())
                        {
                        comp->getSymRefTab()->findOrCreateGCRPatchPointSymbolRef()->getSymbol()->getStaticSymbol()->setStaticAddress(cursor-1);
                        }
                     }
                  }
               }
            }
         }
      else
         {
         *(int16_t *)cursor = (int16_t)getSourceImmediate();
         cursor += 2;
         }

      addMetaDataForCodeAddress(immediateCursor);
      }

   return cursor;
   }

uint8_t TR::X86MemImmInstruction::getBinaryLengthLowerBound()
   {
   int32_t length = getMemoryReference()->getBinaryLengthLowerBound(cg());

   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   if (barrier & NeedsExplicitBarrier)
      length += getMemoryBarrierBinaryLengthLowerBound(barrier, cg());

   length += getOpCode().length(self()->rexBits());

   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length += 1;

   return length;
   }

int32_t TR::X86MemImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t length = getMemoryReference()->estimateBinaryLength(cg());

   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += estimateMemoryBarrierBinaryLength(barrier, cg());

   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length++;

   uint32_t patchBoundaryPadding = (TR::Compiler->target.isSMP() && getMemoryReference()->getSymbolReference().isUnresolved()) ? 1 : 0;

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + length + patchBoundaryPadding);

   return currentEstimate + getEstimatedBinaryLength();
   }

// -----------------------------------------------------------------------------
// TR::X86MemImmSymInstruction:: member functions

void
TR::X86MemImmSymInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
      {
      cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
      }

   TR::Symbol *symbol = getSymbolReference()->getSymbol();

   TR_ASSERT(!(getSymbolReference()->isUnresolved() && !symbol->isClassObject()),
      "expecting a resolved symbol for this instruction class!\n");

   if (symbol->isConst())
      {
      cg()->addAOTRelocation(
            new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t *)getSymbolReference()->getOwningMethod(comp)->constantPool(),
                                                                   getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                   TR_ConstantPool,
                                                                   cg()),
                 __FILE__, __LINE__, getNode()
            );

      }
   else if (symbol->isClassObject())
      {
      TR_ASSERT(getNode(), "No node where expected!");
      if (cg()->needClassAndMethodPointerRelocations())
         {
         *(int32_t *)cursor = (int32_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(cg()->comp(), (TR_OpaqueClassBlock*)getSourceImmediate());
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getSymbolReference(), getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_ClassAddress, cg()),
                                                                                      __FILE__, __LINE__, getNode());

         }
      }
   else if (symbol->isMethod())
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getSymbolReference(), getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_MethodObject, cg()),
                              __FILE__, __LINE__, getNode());
      }
   else
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getSymbolReference(), getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_DataAddress, cg()),
                              __FILE__, __LINE__, getNode());
      }
   }


uint8_t* TR::X86MemImmSymInstruction::generateOperand(uint8_t* cursor)
   {
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   if (cursor)
      {
      TR_ASSERT(getOpCode().hasIntImmediate(), "expecting a 4 byte address for this instruction class!\n");
      TR_ASSERT(getSymbolReference(), "expecting a symbol reference for this instruction class!\n");

      *(int32_t *)cursor = (int32_t)getSourceImmediate();

      addMetaDataForCodeAddress(cursor);

      cursor += 4;
      }
   return cursor;
   }

// -----------------------------------------------------------------------------
// TR::X86MemRegInstruction:: member functions

uint8_t* TR::X86MemRegInstruction::generateOperand(uint8_t* cursor)
   {
   if (getOpCode().hasSourceRegisterIgnored() == 0)
      {
      toRealRegister(getSourceRegister())->setRegisterFieldInModRM(cursor - 1);
      }
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::X86MemRegImmInstruction:: member functions

void TR::X86MemRegImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   if (getOpCode().hasIntImmediate())
      {
      if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      }
   }


uint8_t* TR::X86MemRegImmInstruction::generateOperand(uint8_t* cursor)
   {
   toRealRegister(getSourceRegister())->setRegisterFieldInModRM(cursor - 1);
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   if (cursor)
      {
      uint8_t *immediateCursor = cursor;

      if (getOpCode().hasIntImmediate())
         {
         *(int32_t *)cursor = (int32_t)getSourceImmediate();
         cursor += 4;
         }
      else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
         {
         *(int8_t *)cursor = (int8_t)getSourceImmediate();
         cursor += 1;
         }
      else
         {
         *(int16_t *)cursor = (int16_t)getSourceImmediate();
         cursor += 2;
         }

      addMetaDataForCodeAddress(immediateCursor);
      }
   return cursor;
   }

uint8_t TR::X86MemRegImmInstruction::getBinaryLengthLowerBound()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   int32_t length = getMemoryReference()->getBinaryLengthLowerBound(cg());

   if (barrier & NeedsExplicitBarrier)
      length += getMemoryBarrierBinaryLengthLowerBound(barrier, cg());

   length += getOpCode().length(self()->rexBits());
   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length += 1;

   return length;
   }

int32_t TR::X86MemRegImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t length = getMemoryReference()->estimateBinaryLength(cg());

   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += estimateMemoryBarrierBinaryLength(barrier, cg());

   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length++;

   int32_t patchBoundaryPadding = (TR::Compiler->target.isSMP() && getMemoryReference()->getSymbolReference().isUnresolved()) ? 1 : 0;

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + length + patchBoundaryPadding);
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86RegMemInstruction:: member functions

bool TR::X86RegMemInstruction::needsLockPrefix()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);
   return getOpCode().needsLockPrefix() || (barrier & LockPrefix);
   }

uint8_t* TR::X86RegMemInstruction::generateOperand(uint8_t* cursor)
   {
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      toRealRegister(getTargetRegister())->setRegisterFieldInModRM(cursor - 1);
      }
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   return cursor;
   }

uint8_t TR::X86RegMemInstruction::getBinaryLengthLowerBound()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   uint8_t length = getMemoryReference()->getBinaryLengthLowerBound(cg());

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += getMemoryBarrierBinaryLengthLowerBound(barrier, cg());

   return getOpCode().length(self()->rexBits()) + length;
   }

int32_t TR::X86RegMemInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   int32_t length = getMemoryReference()->estimateBinaryLength(cg());

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += estimateMemoryBarrierBinaryLength(barrier, cg());

   int32_t patchBoundaryPadding = (TR::Compiler->target.isSMP() && getMemoryReference()->getSymbolReference().isUnresolved()) ? 1 : 0;

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + rexRepeatCount() + length + patchBoundaryPadding);
   return currentEstimate + getEstimatedBinaryLength();
   }

OMR::X86::EnlargementResult
TR::X86RegMemInstruction::enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement)
   {
   TR::Compilation *comp = TR::comp();
   if ((maxEnlargementSize < requestedEnlargementSize && !allowPartialEnlargement) || requestedEnlargementSize < 1)
      return OMR::X86::EnlargementResult(0, 0);

   OMR::X86::EnlargementResult result = getMemoryReference()->enlarge(cg(), requestedEnlargementSize,
                                                          maxEnlargementSize, allowPartialEnlargement);
   if (result.getEncodingGrowth() > 0)
      setEstimatedBinaryLength(getEstimatedBinaryLength() + result.getEncodingGrowth());
   return result;
   }


// -----------------------------------------------------------------------------
// TR::X86RegMemImmInstruction:: member functions

void TR::X86RegMemImmInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   if (getOpCode().hasIntImmediate())
      {
      if (std::find(cg()->comp()->getStaticHCRPICSites()->begin(), cg()->comp()->getStaticHCRPICSites()->end(), this) != cg()->comp()->getStaticHCRPICSites()->end())
         {
         cg()->jitAdd32BitPicToPatchOnClassRedefinition(((void *) getSourceImmediateAsAddress()), (void *) cursor);
         }
      }
   }

uint8_t* TR::X86RegMemImmInstruction::generateOperand(uint8_t* cursor)
   {
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      toRealRegister(getTargetRegister())->setRegisterFieldInModRM(cursor - 1);
      }
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   if (cursor)
      {
      uint8_t *immediateCursor = cursor;

      if (getOpCode().hasIntImmediate())
         {
         *(int32_t *)cursor = (int32_t)getSourceImmediate();
         cursor += 4;
         }
      else if (getOpCode().hasByteImmediate() || getOpCode().hasSignExtendImmediate())
         {
         *(int8_t *)cursor = (int8_t)getSourceImmediate();
         cursor += 1;
         }
      else
         {
         *(int16_t *)cursor = (int16_t)getSourceImmediate();
         cursor += 2;
         }

      addMetaDataForCodeAddress(immediateCursor);
      }
   return cursor;
   }

uint8_t TR::X86RegMemImmInstruction::getBinaryLengthLowerBound()
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   uint8_t length = getMemoryReference()->getBinaryLengthLowerBound(cg());

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += getMemoryBarrierBinaryLengthLowerBound(barrier, cg());

   length += getOpCode().length(self()->rexBits());

   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length++;

   return length;
   }

int32_t TR::X86RegMemImmInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   int32_t barrier = memoryBarrierRequired(getOpCode(), getMemoryReference(), cg(), false);

   int32_t length = getMemoryReference()->estimateBinaryLength(cg());

   if (barrier & LockPrefix)
      length++;

   if (barrier & NeedsExplicitBarrier)
      length += estimateMemoryBarrierBinaryLength(barrier, cg());

   if (getOpCode().hasIntImmediate())
      length += 4;
   else if (getOpCode().hasShortImmediate())
      length += 2;
   else
      length++;

   int32_t patchBoundaryPadding = (TR::Compiler->target.isSMP() && getMemoryReference()->getSymbolReference().isUnresolved()) ? 1 : 0;

   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + length + patchBoundaryPadding);

   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::X86FPRegInstruction:: member functions

uint8_t* TR::X86FPRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;
   applyTargetRegisterToOpCode(opCode);
   return cursor;
   }

// TR::X86FPRegRegInstruction:: member functions

uint8_t* TR::X86FPRegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;
   applyRegistersToOpCode(opCode, cg()->machine());
   return cursor;
   }

// TR::X86FPST0ST1RegRegInstruction:: member functions

uint8_t* TR::X86FPST0ST1RegRegInstruction::generateOperand(uint8_t* cursor)
   {
   return cursor;
   }


// TR::X86FPArithmeticRegRegInstruction:: member functions

uint8_t* TR::X86FPArithmeticRegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;

   TR::Machine *machine = cg()->machine();
   applyRegistersToOpCode(opCode, machine);
   if (getOpCode().hasDirectionBit())
      {
      applyDirectionBitToOpCode(opCode, machine);
      }

   if (getOpCode().modifiesTarget())
      {
      opCode = cursor - 2;
      applyDestinationBitToOpCode(opCode, machine);
      }

   return cursor;
   }


// TR::X86FPST0STiRegRegInstruction:: member functions

uint8_t* TR::X86FPST0STiRegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;
   applySourceRegisterToOpCode(opCode, cg()->machine());
   return cursor;
   }


// TR::X86FPSTiST0RegRegInstruction:: member functions

uint8_t* TR::X86FPSTiST0RegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;
   applyTargetRegisterToOpCode(opCode, cg()->machine());
   return cursor;
   }


// TR::X86FPCompareRegRegInstruction:: member functions

uint8_t* TR::X86FPCompareRegRegInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *opCode = cursor - 1;
   applyRegistersToOpCode(opCode, cg()->machine());
   return cursor;
   }


// TR::X86FPRegMemInstruction:: member functions

uint8_t* TR::X86FPRegMemInstruction::generateOperand(uint8_t* cursor)
   {
   return getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   }

uint8_t TR::X86FPRegMemInstruction::getBinaryLengthLowerBound()
   {
   return getOpCode().length(self()->rexBits());
   }


// TR::X86FPMemRegInstruction:: member functions

uint8_t* TR::X86FPMemRegInstruction::generateOperand(uint8_t* cursor)
   {
   cursor = getMemoryReference()->generateBinaryEncoding(cursor - 1, this, cg());
   if (cursor)
      {
      setBinaryLength(cursor - getBinaryEncoding());
      cg()->addAccumulatedInstructionLengthError(getEstimatedBinaryLength() - getBinaryLength());
      }
   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::AMD64RegImm64Instruction:: member functions

void
TR::AMD64RegImm64Instruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();
   bool staticPIC = false;
   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), this) != comp->getStaticPICSites()->end())
      staticPIC = true;
   bool staticHCRPIC = false;
   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), this) != comp->getStaticHCRPICSites()->end())
      {
      staticHCRPIC = true;
      }
   bool staticMethodPIC = false;
   if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), this) != comp->getStaticMethodPICSites()->end())
      staticMethodPIC = true;

   TR::SymbolReference *methodSymRef = getNode()->getOpCode().hasSymbolReference()?getNode()->getSymbolReference():NULL;

#ifdef J9_PROJECT_SPECIFIC
   if (comp->fej9()->helpersNeedRelocation())
      {
      if (getNode()->getOpCode().hasSymbolReference() &&
          methodSymRef &&
            (methodSymRef->getReferenceNumber()==TR_referenceArrayCopy ||
#if defined(LINUX) || defined(OSX)  // AOT_JIT_GAP
             methodSymRef->getReferenceNumber()==TR_AMD64getTimeOfDay
#else
             methodSymRef->getReferenceNumber()==TR_AMD64GetSystemTimeAsFileTime
#endif
            )
         )
         {// the reference number is set in j9x86evaluator.cpp/VMarrayStoreCheckArrayCopyEvaluator
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                         (uint8_t *)methodSymRef,
                                                         TR_AbsoluteHelperAddress,
                                                         cg()),
                             __FILE__, __LINE__, getNode());
         }
      else if (((getNode()->getOpCodeValue() == TR::aconst) && getNode()->isMethodPointerConstant() && needsAOTRelocation()) || staticHCRPIC)
         {
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                         NULL,
                                                         TR_RamMethod,
                                                         cg()),
                             __FILE__, __LINE__, getNode());
         }
      else
         {
         switch (_reloKind)
            {
            case TR_HEAP_BASE_FOR_BARRIER_RANGE:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                 (uint8_t*)TR_HeapBaseForBarrierRange0,
                                                                 TR_GlobalValue,
                                                                 cg()),
                                      __FILE__, __LINE__, getNode());
               break;

            case TR_HEAP_SIZE_FOR_BARRIER_RANGE:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                               (uint8_t*)TR_HeapSizeForBarrierRange0,
                                                               TR_GlobalValue,
                                                               cg()),
                                   __FILE__, __LINE__, getNode());
               break;
            case TR_ACTIVE_CARD_TABLE_BASE:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                               (uint8_t*)TR_ActiveCardTableBase,
                                                               TR_GlobalValue,
                                                               cg()),
                                   __FILE__, __LINE__, getNode());
               break;
            case TR_ClassAddress:
               TR_ASSERT(getNode(), "node assumed to be non-NULL here");
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                (uint8_t *)methodSymRef,
                                                                (uint8_t *)getNode()->getInlinedSiteIndex(),
                                                                TR_ClassAddress,
                                                                cg()),
                                 __FILE__,__LINE__, getNode());
               break;

            case TR_MethodPointer:
               if (getNode() && getNode()->getInlinedSiteIndex() == -1 &&
                  (void *) getSourceImmediate() == cg()->comp()->getCurrentMethod()->resolvedMethodAddress())
                  setReloKind(TR_RamMethod);
               // intentional fall-through
            case TR_RamMethod:
               // intentional fall-through
            case TR_ClassPointer:
               cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                 (uint8_t*)getNode(),
                                                                 (TR_ExternalRelocationTargetKind) _reloKind,
                                                                 cg()),
                                      __FILE__, __LINE__, getNode());
               break;
            case TR_JNIStaticTargetAddress:
            case TR_JNISpecialTargetAddress:
            case TR_JNIVirtualTargetAddress:
            case TR_StaticRamMethodConst:
            case TR_VirtualRamMethodConst:
            case TR_SpecialRamMethodConst:

                cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getNode()->getSymbolReference(), getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,  (TR_ExternalRelocationTargetKind) _reloKind, cg()),  __FILE__,__LINE__, getNode());
               break;

            }
         }
      }
#endif

   if (staticPIC)
      {
      cg()->jitAddPicToPatchOnClassUnload(((void *) getSourceImmediate()), (void *) cursor);
      }

   if (staticHCRPIC)
      {
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, (uint8_t *)getSourceImmediate(), TR_HCR, cg()), __FILE__,__LINE__, getNode());
      cg()->jitAddPicToPatchOnClassRedefinition(((void *) getSourceImmediate()), (void *) cursor);
      }

   if (staticMethodPIC)
      {
      void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) getSourceImmediate(), comp->getCurrentMethod())->classOfMethod();

      cg()->jitAddPicToPatchOnClassUnload(classPointer, (void *) cursor);
      }
   }


uint8_t* TR::AMD64RegImm64Instruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *modRM = cursor - 1;
   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }
   TR_ASSERT(getOpCode().hasLongImmediate(), "Imm64 instructions must have long immediates");
   *(uint64_t *)cursor = getSourceImmediate();

   addMetaDataForCodeAddress(cursor);

   cursor += 8;
   return cursor;
   }


uint8_t TR::AMD64RegImm64Instruction::getBinaryLengthLowerBound()
   {
   return getOpCode().length(self()->rexBits()) + rexRepeatCount() + 8;
   }


int32_t TR::AMD64RegImm64Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + 8);
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::AMD64RegImm64SymInstruction:: member functions
//

void
TR::AMD64RegImm64SymInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   if (getSymbolReference()->getSymbol()->isLabel())
      {
      // Assumes a 64-bit absolute relocation (i.e., not relative).
      //
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, getSymbolReference()->getSymbol()->castToLabelSymbol()));

      switch (getReloKind())
         {
         case TR_AbsoluteMethodAddress:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
            break;
         default:
            break;
         }

      }
   else
      {
      TR_RelocationRecordInformation * recordInfo;
      switch (getReloKind())
         {
         case TR_ConstantPool:
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                      (uint8_t *)getSymbolReference()->getOwningMethod(comp)->constantPool(),
                                                                                       getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                       (TR_ExternalRelocationTargetKind) getReloKind(),
                                                                                       cg()),
                                   __FILE__,
                                   __LINE__,
                                   getNode());
            break;

         case TR_DataAddress:
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                      (uint8_t *) getSymbolReference(),
                                                                                      (uint8_t *)getNode() ? (uint8_t *) getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                      (TR_ExternalRelocationTargetKind) getReloKind(),
                                                                                      cg()),
                                                                                      __FILE__,
                                                                                      __LINE__,
                                                                                      getNode());

            break;
            }
         case TR_NativeMethodAbsolute:
            {
            if (cg()->comp()->getOption(TR_EnableObjectFileGeneration))
               {
               TR_ResolvedMethod *target = getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
               cg()->addStaticRelocation(TR::StaticRelocation(cursor, target->externalName(cg()->trMemory()), TR::StaticRelocationSize::word64, TR::StaticRelocationType::Absolute));
               }
            break;
            }
         default:
            ;
         }

      }

   }


uint8_t* TR::AMD64RegImm64SymInstruction::generateOperand(uint8_t* cursor)
   {
   uint8_t *modRM = cursor - 1;

   if (getOpCode().hasTargetRegisterIgnored() == 0)
      {
      applyTargetRegisterToModRMByte(modRM);
      }

   TR_ASSERT(getOpCode().hasLongImmediate(), "Imm64 instructions must have long immediates");
   TR_ASSERT(getSymbolReference(), "expecting a symbol reference for this instruction class");
   *(uint64_t *)cursor = getSourceImmediate();

   addMetaDataForCodeAddress(cursor);

   cursor += 8;

   return cursor;
   }


// -----------------------------------------------------------------------------
// TR::AMD64Imm64Instruction:: member functions
//

void
TR::AMD64Imm64Instruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   if (needsAOTRelocation())
      {
      cg()->addProjectSpecializedRelocation(cursor, 0, NULL, TR_BodyInfoAddress,
                             __FILE__,
                             __LINE__,
                             getNode());
      }
   }


uint8_t* TR::AMD64Imm64Instruction::generateOperand(uint8_t* cursor)
   {
   // Always assume this is an 8-byte immediate.
   //
   *(int64_t *)cursor = (int64_t)getSourceImmediate();

   addMetaDataForCodeAddress(cursor);
   cursor += 8;

   return cursor;
   }

uint8_t TR::AMD64Imm64Instruction::getBinaryLengthLowerBound()
   {
   uint8_t len = getOpCode().length(self()->rexBits()) + 8;
   return len;
   }

int32_t TR::AMD64Imm64Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(getOpCode().length(self()->rexBits()) + 8);
   return currentEstimate + getEstimatedBinaryLength();
   }


// -----------------------------------------------------------------------------
// TR::AMD64Imm64SymInstruction:: member functions
//
int32_t TR::AMD64Imm64SymInstruction::estimateBinaryLength(int32_t currentEstimate)
   {
   currentEstimate = TR::AMD64Imm64Instruction::estimateBinaryLength(currentEstimate);
   return currentEstimate;
   }

void
TR::AMD64Imm64SymInstruction::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   if (getOpCodeValue() == DQImm64)
      {
      cg()->addProjectSpecializedRelocation(cursor, (uint8_t *)(uint64_t)getSourceImmediate(), getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1, TR_Thunks,
                             __FILE__,
                             __LINE__,
                             getNode());
      }
   }


uint8_t* TR::AMD64Imm64SymInstruction::generateOperand(uint8_t* cursor)
   {
   TR_ASSERT(getOpCodeValue() == DQImm64, "TR::AMD64Imm64SymInstruction expected to be DQImm64 only");

   *(int64_t *)cursor = (int64_t)getSourceImmediate();
   addMetaDataForCodeAddress(cursor);
   cursor += 8;

   return cursor;
   }
