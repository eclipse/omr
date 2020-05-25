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

#include "codegen/Linkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/StackMemoryRegion.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/CompilerEnv.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Assert.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86SystemLinkage.hpp"

#ifdef DEBUG
static uint32_t accumOrigSize = 0;
static uint32_t accumMappedSize = 0;
#endif


OMR::X86::Linkage::Linkage(TR::CodeGenerator *cg) :
      OMR::Linkage(cg),
   _minimumFirstInstructionSize(0)
   {
   // Initialize the movOp table based on preferred load instructions for this target.
   //
   TR_X86OpCodes op = cg->getXMMDoubleLoadOpCode() ? cg->getXMMDoubleLoadOpCode() : MOVSDRegMem;
   _movOpcodes[RegMem][Float8] = op;
   }


void OMR::X86::Linkage::mapCompactedStack(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::AutomaticSymbol>  automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   const TR::X86LinkageProperties&    linkage           = self()->getProperties();
   int32_t                           offsetToFirstParm = linkage.getOffsetToFirstParm();
   uint32_t                          stackIndex        = linkage.getOffsetToFirstLocal();
   TR::GCStackAtlas                  *atlas             = self()->cg()->getStackAtlas();
   int32_t                           i;

   uint8_t pointerSize  = linkage.getPointerSize();
   uint8_t pointerShift = linkage.getPointerShift();
   uint8_t parmSlotSize  = linkage.getParmSlotSize();
   uint8_t parmSlotShift = linkage.getParmSlotShift();
   uint8_t scalarTempShift = parmSlotShift;

#ifdef DEBUG
   uint32_t origSize = 0;
   bool     isFirst  = false;
#endif

   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

   int32_t *colourToOffsetMap =
      (int32_t *) self()->trMemory()->allocateStackMemory(self()->cg()->getLocalsIG()->getNumberOfColoursUsedToColour() * sizeof(int32_t));

   uint32_t *colourToSizeMap =
      (uint32_t *) self()->trMemory()->allocateStackMemory(self()->cg()->getLocalsIG()->getNumberOfColoursUsedToColour() * sizeof(uint32_t));

   for (i=0; i<self()->cg()->getLocalsIG()->getNumberOfColoursUsedToColour(); i++)
      {
      colourToOffsetMap[i] = -1;
      colourToSizeMap[i] = 0;
      }

   // Find maximum allocation size for each shared local.
   //
   TR_IGNode    *igNode;
   uint32_t      size;
   IGNodeColour  colour;
   for (i=0; i<self()->cg()->getLocalsIG()->getNumNodes(); i++)
      {
      igNode = self()->cg()->getLocalsIG()->getNodeTable(i);
      colour = igNode->getColour();

      TR_ASSERT(colour != UNCOLOURED, "uncoloured local %p (igNode=%p) found in locals IG\n",
              igNode->getEntity(), igNode);

      if ((colour != UNCOLOURED) &&
          ((size = ((TR::Symbol *)(igNode->getEntity()))->castToAutoSymbol()->getRoundedSize()) > colourToSizeMap[colour]))
         {
         colourToSizeMap[colour] = size;
         }
      }

   // Map all garbage collected references together so we can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   //
   int32_t lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   // Map local references to get the right amount of stack reserved
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         TR_IGNode *igNode = self()->cg()->getLocalsIG()->getIGNodeForEntity(localCursor);
         bool performSharing = true;
         if (igNode)
            {
            IGNodeColour colour = igNode->getColour();

            if (localCursor->isInternalPointer() || localCursor->isPinningArrayPointer() || localCursor->holdsMonitoredObject())
               {
               // Regardless of colouring on the local, map an internal
               // pointer or a pinning array local. These kinds of locals
               // do not participate in the compaction of locals phase and
               // are handled specially (basically the slots are not shared for
               // these autos).
               //
               self()->mapSingleAutomatic(localCursor, stackIndex);
               }
            else if (colourToOffsetMap[colour] == -1)
               {
               self()->mapSingleAutomatic(localCursor, stackIndex);
               colourToOffsetMap[colour] = localCursor->getOffset();
#ifdef DEBUG
               isFirst = true;
#endif
               }
            else
               {
               performSharing = performTransformation(self()->comp(), "O^O COMPACT LOCALS: Sharing slot for local %p\n", localCursor);

               if (performSharing)
                  localCursor->setOffset(colourToOffsetMap[colour]);
               else
                  {
                  self()->mapSingleAutomatic(localCursor, stackIndex);
                  colourToOffsetMap[colour] = localCursor->getOffset();
#ifdef DEBUG
                  isFirst = true;
#endif
                  }
               }

#ifdef DEBUG
            if (debug("reportCL"))
               {
               diagnostic("%s ref local %p (colour=%d): %s\n", isFirst ? "First" : "Shared", localCursor, colour, self()->comp()->signature());
               isFirst = false;
               }
#endif
            }
         else
            self()->mapSingleAutomatic(localCursor, stackIndex);

#ifdef DEBUG
         origSize += localCursor->getRoundedSize();
#endif
         }
      }

   self()->alignLocalObjectWithCollectedFields(stackIndex);

   // map local references again to set the stack position correct according to
   // the gc map index.
   // first map collected local references
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() >= 0)
         {
         int32_t newOffset = stackIndex + pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex);

         if (self()->cg()->comp()->getOption(TR_TraceRA))
            traceMsg(self()->comp(), "\nmapCompactedStack: changing %s (GC index %d) offset from %d to %d",
               self()->comp()->getDebug()->getName(localCursor), localCursor->getGCMapIndex(), localCursor->getOffset(), newOffset);

         localCursor->setOffset(newOffset);

         TR_ASSERT((localCursor->getOffset() <= 0), "Local %p (GC index %d) offset cannot be positive (stackIndex = %d)\n", localCursor, localCursor->getGCMapIndex(), stackIndex);

         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset());
            }
         }

   method->setObjectTempSlots((lowGCOffset-stackIndex) >> pointerShift);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() < 0)
         {
         TR_IGNode *igNode = self()->cg()->getLocalsIG()->getIGNodeForEntity(localCursor);
         if (igNode)
            {
            IGNodeColour colour = igNode->getColour();

            if (colourToOffsetMap[colour] == -1)
               {
               self()->mapSingleAutomatic(localCursor, colourToSizeMap[colour], stackIndex);
               colourToOffsetMap[colour] = localCursor->getOffset();
#ifdef DEBUG
               isFirst = true;
#endif
               }
            else
               {
               localCursor->setOffset(colourToOffsetMap[colour]);
               }

#ifdef DEBUG
            if (debug("reportCL"))
               {
               diagnostic("%s local %p (colour=%d): %s\n",
                           isFirst ? "First" : "Shared",
                           localCursor, colour,
                           self()->comp()->signature());
               isFirst = false;
               }
#endif
            }
         else
            {
            self()->mapSingleAutomatic(localCursor, stackIndex);
            }

#ifdef DEBUG
         origSize += localCursor->getRoundedSize();
#endif
         }

   // Ensure the frame will always be a multiple of the Java pointer size.
   //
   if (!self()->cg()->mapAutosTo8ByteSlots())
      {
      if (stackIndex % TR::Compiler->om.sizeofReferenceAddress())
         stackIndex -= 4;

      TR_ASSERT((stackIndex % TR::Compiler->om.sizeofReferenceAddress()) == 0,
             "size of scalar temp area not a multiple of Java pointer size");
      }

   method->setScalarTempSlots((lowGCOffset-stackIndex) >> scalarTempShift);

   if (self()->comp()->getMethodSymbol()->getLinkageConvention() != TR_System)
      self()->mapIncomingParms(method);
   else
      TR::toX86SystemLinkage(self()->cg()->getLinkage())->mapIncomingParms(method, stackIndex);

   method->setLocalMappingCursor(stackIndex);

   atlas->setLocalBaseOffset(lowGCOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + offsetToFirstParm);

   } // scope of the stack memory region

   if (debug("reportCL"))
      {
      automaticIterator.reset();
      diagnostic("SYMBOL OFFSETS\n");
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         diagnostic("Local %p, offset=%d\n", localCursor, localCursor->getOffset());
         }

#ifdef DEBUG
      diagnostic("\n**** Mapped locals size: %d (orig map size=%d, shared size=%d)  %s\n",
                  (linkage.getOffsetToFirstLocal() - stackIndex),
                  origSize,
                  origSize - (linkage.getOffsetToFirstLocal() - stackIndex),
                  self()->comp()->signature());

      accumMappedSize += (linkage.getOffsetToFirstLocal() - stackIndex);
      accumOrigSize += origSize;

      diagnostic("\n**** Accumulated totals: mapped size=%d, shared size=%d, original size=%d after %s\n",
                  accumMappedSize, accumOrigSize-accumMappedSize, accumOrigSize,
                  self()->comp()->signature());
#endif
      }
   }


void OMR::X86::Linkage::mapStack(TR::ResolvedMethodSymbol *method)
   {

   if (self()->cg()->getLocalsIG() && self()->cg()->getSupportsCompactedLocals())
      {
      self()->mapCompactedStack(method);
      return;
      }

   ListIterator<TR::AutomaticSymbol>  automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol               *localCursor       = automaticIterator.getFirst();
   const TR::X86LinkageProperties&    linkage           = self()->getProperties();
   int32_t                           offsetToFirstParm = linkage.getOffsetToFirstParm();
   uint32_t                          stackIndex        = linkage.getOffsetToFirstLocal();
   TR::GCStackAtlas                  *atlas             = self()->cg()->getStackAtlas();

   uint8_t pointerSize  = linkage.getPointerSize();
   uint8_t pointerShift = linkage.getPointerShift();
   uint8_t parmSlotSize  = linkage.getParmSlotSize();
   uint8_t parmSlotShift = linkage.getParmSlotShift();
   uint8_t scalarTempShift = parmSlotShift;

   // map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.

   int32_t lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   stackIndex -= (atlas->getNumberOfSlotsMapped() - atlas->getNumberOfParmSlotsMapped()) << pointerShift;

   self()->alignLocalObjectWithCollectedFields(stackIndex);

   // Map local references again to set the stack position correct according to
   // the GC map index.
   // first map collected local references
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      if (localCursor->getGCMapIndex() >= 0)
         {
         localCursor->setOffset(stackIndex + pointerSize*(localCursor->getGCMapIndex()-firstLocalGCIndex));

         TR_ASSERT((localCursor->getOffset() <= 0), "Local %p (GC index %d) offset cannot be positive (stackIndex = %d)\n", localCursor, localCursor->getGCMapIndex(), stackIndex);

         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset());
            }
         }


   method->setObjectTempSlots((lowGCOffset-stackIndex) >> pointerShift);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   // the sorting below is done to reduce the number of
   // misaligned stack references
   //
   static char *sortAutosBySize = feGetEnv("TR_noSortAutosBySize");
   if (!sortAutosBySize)
      {
      // do 8byte autos first
      //
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         if (localCursor->getGCMapIndex() < 0 &&
               localCursor->getSize() == 8)
            self()->mapSingleAutomatic(localCursor, stackIndex);

      // do 4byte autos or others next
      //
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         if (localCursor->getGCMapIndex() < 0 &&
               localCursor->getSize() != 8)
            self()->mapSingleAutomatic(localCursor, stackIndex);
      }
   else
      {
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         if (localCursor->getGCMapIndex() < 0)
            self()->mapSingleAutomatic(localCursor, stackIndex);
      }

   // Ensure the frame will always be a multiple of the Java pointer size.
   //
   if (!self()->cg()->mapAutosTo8ByteSlots())
      {
      if (stackIndex % TR::Compiler->om.sizeofReferenceAddress())
         stackIndex -= 4;

      TR_ASSERT((stackIndex % TR::Compiler->om.sizeofReferenceAddress()) == 0,
             "size of scalar temp area not a multiple of Java pointer size");
      }

   method->setScalarTempSlots((lowGCOffset-stackIndex) >> scalarTempShift);

   if (self()->comp()->getMethodSymbol()->getLinkageConvention() != TR_System)
      self()->mapIncomingParms(method);
   else
      TR::toX86SystemLinkage(self()->cg()->getLinkage())->mapIncomingParms(method, stackIndex);

   method->setLocalMappingCursor(stackIndex);

   atlas->setLocalBaseOffset(lowGCOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + offsetToFirstParm);

#ifdef DEBUG
   if (debug("reportCL"))
      {
      diagnostic("\n**** Mapped locals size: %d  %s\n",
                  linkage.getOffsetToFirstLocal()-stackIndex, self()->comp()->signature());

      accumMappedSize += (linkage.getOffsetToFirstLocal()-stackIndex);
      accumOrigSize += (linkage.getOffsetToFirstLocal()-stackIndex);

      diagnostic("\n**** Accumulated totals: mapped size=%d, shared size=%d, original size=%d after %s\n",
                  accumMappedSize, accumOrigSize-accumMappedSize, accumOrigSize,
                  self()->comp()->signature());
      }
#endif

   }

void OMR::X86::Linkage::mapIncomingParms(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   TR::ParameterSymbol              *parmCursor   = parameterIterator.getFirst();
   int32_t                          offsetToFirstParm = self()->getProperties().getOffsetToFirstParm();
   if (self()->getProperties().passArgsRightToLeft())
      {
      int32_t currentOffset = offsetToFirstParm;
      for (; parmCursor; parmCursor = parameterIterator.getNext())
         {
         if (!debug("amd64unimplemented"))
         TR_ASSERT(self()->comp()->target().is32Bit(), "Right-to-left not yet implemented on AMD64");
         parmCursor->setParameterOffset(currentOffset);
         currentOffset += parmCursor->getRoundedSize();
         }
      }
   else
      {
      TR_ASSERT(self()->comp()->target().is32Bit(), "This code is IA32-specific");
      // TODO:AMD64: This code deosn't need to support 8-byte slots; put it back the way it was
      uint8_t parmSlotShift = self()->getProperties().getParmSlotShift();
      int32_t topOfParameterArea = (method->getNumParameterSlots() << parmSlotShift) + offsetToFirstParm;
      for (; parmCursor; parmCursor = parameterIterator.getNext())
         {
         // Incoming parm offsets are zero-based, positive, and based on 4-byte slots
         int32_t parmSlotNumber = parmCursor->getParameterOffset() >> 2;
         int32_t parmSlotCount  = parmCursor->getRoundedSize() >> 2;
         // Convert to negative (ie. left-to-right), based on actual slot size
         int32_t actualOffset   = (-parmSlotNumber - parmSlotCount) << parmSlotShift;
         // Set offsets relative to top of the incoming parameter area
         parmCursor->setParameterOffset(topOfParameterArea + actualOffset);
         }
      }

   }

void OMR::X86::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p,
                                        uint32_t           &stackIndex)
   {
   return self()->mapSingleAutomatic(p, p->getRoundedSize(), stackIndex);
   }


void OMR::X86::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p,
                                        uint32_t            size,
                                        uint32_t           &stackIndex)
   {
   // Assumes 'size' is rounded to nearest multiple of 4-bytes.
   //
   if (self()->cg()->mapAutosTo8ByteSlots() && p->getDataType() != TR::Address)
      size *= 2;

   stackIndex -= size;
   // align stack-allocated objects that don't have GC map index > 0
   // and are referenced through a register
   if (p->isLocalObject() && self()->comp()->target().is64Bit())
      {
      if (p->getGCMapIndex() == -1)
         self()->alignLocalObjectWithoutCollectedFields(stackIndex);

      // In above code, localObjectAlignment is larger than 8
      // if it has been executed, stackIndex is already aligned to 8
      if (stackIndex % 8)
         stackIndex -= 4;
      }

   p->setOffset(stackIndex);


   if (self()->cg()->comp()->getOption(TR_TraceRA))
      traceMsg(self()->cg()->comp(), "\nmapSingleAutomatic(%s, %d) = %d", self()->cg()->getDebug()->getName(p), size, stackIndex);
   }


void OMR::X86::Linkage::stopUsingKilledRegisters(TR::RegisterDependencyConditions  *deps, TR::Register *returnRegister)
   {
   // Stop using the killed registers that are not going to persist
   //
   TR::Register *firstReturnRegister  = returnRegister;
   TR::Register *secondReturnRegister = NULL;
   if (returnRegister)
      {
      TR::RegisterPair *regPair = returnRegister->getRegisterPair();
      if (regPair)
         {
         firstReturnRegister = regPair->getLowOrder();
         secondReturnRegister = regPair->getHighOrder();
         }
      }

   TR::Register *vmThreadReg = self()->cg()->getVMThreadRegister();
   for (int32_t i = deps->getNumPostConditions()-1; i >= 0; --i)
      {
      if (deps->getPostConditions()->getRegisterDependency(i)->getRealRegister() != TR::RealRegister::NoReg)
         {
         TR::Register *reg = deps->getPostConditions()->getRegisterDependency(i)->getRegister();
         if (reg                         &&
             reg != firstReturnRegister  &&
             reg != secondReturnRegister &&
             reg != vmThreadReg)
            self()->cg()->stopUsingRegister(reg);
         }
      }
   }

void OMR::X86::Linkage::associatePreservedRegisters(TR::RegisterDependencyConditions  *deps, TR::Register *returnRegister)
   {
   TR::Register *vmThreadReg = self()->cg()->getVMThreadRegister();
   TR_LiveRegisters *liveRegisters = self()->cg()->getLiveRegisters(TR_GPR);

   for (TR_LiveRegisterInfo *liveReg = liveRegisters->getFirstLiveRegister(); liveReg; liveReg = liveReg->getNext())
      {
      TR::Register *virtReg = liveReg->getRegister();

      // Skip registers that already have good associations that we don't want to disturb
      //
      if (virtReg == returnRegister)
         continue;
      if (virtReg == vmThreadReg)
         continue;

      // Search all registers for the best one to associate with virtReg
      //
      TR::RealRegister::RegNum bestReal = TR::RealRegister::NoReg;
      for (int realReg = TR::RealRegister::LastAssignableGPR; realReg >= TR::RealRegister::FirstGPR; realReg--)
         {
         // Can't use locked regs
         //
         if (self()->machine()->getRealRegister((TR::RealRegister::RegNum)realReg)->getState() == TR::RealRegister::Locked)
            continue;

         // Volatile regs are no good for virtuals that live across a call
         //
         if (!self()->getProperties().isPreservedRegister((TR::RealRegister::RegNum)realReg))
            continue;

         if (self()->machine()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)realReg) == virtReg)
            {
            // virtReg is already associated with a preserved realReg, so obviously that's the best choice
            //
            bestReal = (TR::RealRegister::RegNum)realReg;
            break;
            }
         else if (self()->machine()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)realReg) == NULL)
            {
            // realReg isn't associated with anything yet so it's a good choice,
            // but keep looking in case we find a better choice.
            //
            bestReal = (TR::RealRegister::RegNum)realReg;
            }
         }

      if (bestReal == TR::RealRegister::NoReg)
         {
         // There are no more preserved regs to associate, so we're done.
         //
         break;
         }
      else if (self()->machine()->getVirtualAssociatedWithReal(bestReal) == virtReg)
         {
         // virtReg is already associated; do nothing
         }
      else
         {
         self()->machine()->setVirtualAssociatedWithReal(bestReal, virtReg);
         }

      }
   }


// TR_Private linkage properties
//


TR::Register *OMR::X86::Linkage::findReturnRegisterFromDependencies(TR::Node                             *callNode,
                                                               TR::RegisterDependencyConditions  *dependencies,
                                                               TR::MethodSymbol                     *methodSymbol,
                                                               bool                                 isDirect)
   {
   TR::Register *returnRegister = NULL;
   TR_X86RegisterDependencyGroup *postConditions = dependencies->getPostConditions();
   switch (callNode->getDataType())
      {
      case TR::Int32:
      case TR::Address:
         returnRegister = postConditions->getRegisterDependency(0)->getRegister();
         if (isDirect && dependencies->getNumPostConditions() > 1)
            postConditions->getRegisterDependency(1)->getRegister()->setPlaceholderReg();

         break;
      case TR::Int64:
         returnRegister = self()->cg()->allocateRegisterPair(
            postConditions->getRegisterDependency(0)->getRegister(),
            postConditions->getRegisterDependency(1)->getRegister()
            );
         break;
      case TR::Float:
      case TR::Double:
         {
         // FP return registers are normally in dependency 3;
         // dependency 0 for an INL method that preserves all registers.
         //
         int depNum = ((methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()) && methodSymbol->preservesAllRegisters()) ? 0 : 3;
         returnRegister = postConditions->getRegisterDependency(depNum)->getRegister();
         }
         // deliberate fall-through
      default:
         if (isDirect && !methodSymbol->preservesAllRegisters())
            {
            postConditions->getRegisterDependency(0)->getRegister()->setPlaceholderReg();
            postConditions->getRegisterDependency(1)->getRegister()->setPlaceholderReg();
            }
      }
   return returnRegister;
   }


// FIXME: This should be replaced by thunk code in the PicBuilder.
//
void OMR::X86::Linkage::coerceFPReturnValueToXMMR(TR::Node                             *callNode,
                                              TR::RegisterDependencyConditions  *dependencies,
                                              TR::MethodSymbol                     *methodSymbol,
                                              TR::Register                         *returnReg)
   {
   TR_ASSERT(returnReg->getKind() == TR_FPR,
          "cannot coerce FP return values into non-XMM registers\n");

   // A call to an FP-returning INL method that preserves all registers
   // expects the return register in dependency 0.
   //
   //   int depNum = ((methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative()) && methodSymbol->preservesAllRegisters()) ? 0 : 3;
   //   TR::Register *fpReg = dependencies->getPostConditions()->getRegisterDependency(depNum)->getRegister();

   TR::Register *fpReg = callNode->getOpCode().isFloat() ? self()->cg()->allocateSinglePrecisionRegister(TR_X87)
                                                        : self()->cg()->allocateRegister(TR_X87);
   fpReg->incTotalUseCount();

   if (callNode->getOpCode().isFloat())
      {
   TR::MemoryReference  *tempMR = self()->machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTPMemReg, callNode, tempMR, fpReg, self()->cg());
      generateRegMemInstruction(MOVSSRegMem, callNode, returnReg, generateX86MemoryReference(*tempMR, 0, self()->cg()), self()->cg());
      }
   else
      {
      TR_ASSERT(callNode->getOpCode().isDouble(),
             "cannot return non-floating-point values in XMM registers\n");
      TR::MemoryReference  *tempMR = self()->machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTPMemReg, callNode, tempMR, fpReg, self()->cg());
      generateRegMemInstruction(self()->cg()->getXMMDoubleLoadOpCode(), callNode, returnReg, generateX86MemoryReference(*tempMR, 0, self()->cg()), self()->cg());
      }

   self()->cg()->stopUsingRegister(fpReg);
   }


uint32_t
OMR::X86::Linkage::getRightToLeft()
   {
   return self()->getProperties().passArgsRightToLeft();
   }


uint8_t *
OMR::X86::Linkage::loadArguments(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea, bool isReturnAddressOnStack)
   {
   return self()->flushArguments(callNode, cursor, calculateSizeOnly, sizeOfFlushArea, isReturnAddressOnStack, true);
   }


uint8_t *
OMR::X86::Linkage::storeArguments(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea)
   {
   return self()->flushArguments(callNode, cursor, calculateSizeOnly, sizeOfFlushArea, true, false);
   }


TR::Instruction *
OMR::X86::Linkage::loadArguments(TR::Instruction *prev, TR::ResolvedMethodSymbol *methodSymbol)
   {
   return self()->flushArguments(prev, methodSymbol, true, true);
   }


TR::Instruction *
OMR::X86::Linkage::storeArguments(TR::Instruction *prev, TR::ResolvedMethodSymbol *methodSymbol)
   {
   return self()->flushArguments(prev, methodSymbol, true, false);
   }


int32_t
OMR::X86::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
   {
   switch (kind)
      {
      case TR_GPR:
         return self()->getProperties().getNumIntegerArgumentRegisters();
      case TR_FPR:
         return self()->getProperties().getNumFloatArgumentRegisters();
      default:
         return 0;
      }
   }


TR_RegisterKinds
OMR::X86::Linkage::movRegisterKind(TR_MovDataTypes mdt)
   {
   return self()->isFloat(mdt)? TR_FPR : TR_GPR;
   }


TR_MovDataTypes
OMR::X86::Linkage::paramMovType(TR::ParameterSymbol *param)
   {
   return self()->movType(param->getDataType());
   }

TR::Environment&
OMR::X86::Linkage::getTargetFromComp()
   {
   return TR::comp()->target();
   }


TR_X86OpCodes OMR::X86::Linkage::_movOpcodes[NumMovOperandTypes][NumMovDataTypes] =
   {
   //    Int4         Int8        Float4         Float8
   {    S4MemReg,    S8MemReg,  MOVSSMemReg,  MOVSDMemReg}, // MemReg
   {    L4RegMem,    L8RegMem,  MOVSSRegMem,  MOVSDRegMem}, // RegMem
   {  MOV4RegReg,  MOV8RegReg,  MOVSSRegReg,  MOVSDRegReg}, // RegReg
   };
