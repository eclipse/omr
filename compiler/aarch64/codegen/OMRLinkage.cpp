/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stddef.h>
#include <stdint.h>

#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/MemoryReference.hpp"
#include "compile/Compilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"

#define OPT_DETAILS "O^O COMPACT LOCALS: "

typedef bool (*localSizeTestMethod) (uint32_t size);
static bool isLocalSize4Byte(uint32_t size)
   {
   return (size == 4);
   }
static bool isLocalSize8Byte(uint32_t size)
   {
   return (size == 8);
   }
static bool isLocalSizeLargerThan8Byte(uint32_t size)
   {
   return (size > 8);
   }

void OMR::ARM64::Linkage::alignLocalReferences(uint32_t &stackIndex)
   {
   // do nothing
   }

void OMR::ARM64::Linkage::mapCompactedStack(TR::ResolvedMethodSymbol *method)
   {
   const TR::ARM64LinkageProperties& linkageProperties = getProperties();
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor;
   TR::CodeGenerator *cg = self()->cg();
   TR::Compilation *comp = self()->comp();

   TR::GCStackAtlas *atlas = cg->getStackAtlas();

   { // scope of the stack memory region starts here
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   const IGNodeColour numberOfColors = cg->getLocalsIG()->getNumberOfColoursUsedToColour();

   int32_t *colourToOffsetMap = reinterpret_cast<int32_t *>(trMemory()->allocateStackMemory(numberOfColors * sizeof(int32_t)));

   uint32_t *colourToSizeMap = reinterpret_cast<uint32_t *>(trMemory()->allocateStackMemory(numberOfColors * sizeof(uint32_t)));

   for (IGNodeColour i = 0; i < numberOfColors; i++)
      {
      colourToOffsetMap[i] = -1;
      colourToSizeMap[i] = 0;
      }

   // Find maximum allocation size for each shared local.
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      TR_IGNode *igNode = cg->getLocalsIG()->getIGNodeForEntity(localCursor);
      if (igNode != NULL) // if the local doesn't have an interference graph node, we will just map it without attempt to compact, so we can ignore it
         {
         IGNodeColour colour = igNode->getColour();

         TR_ASSERT_FATAL(colour != UNCOLOURED, "uncoloured local %p (igNode=%p) found in locals IG\n",
                 localCursor, igNode);

         if (!(localCursor->isInternalPointer() || localCursor->isPinningArrayPointer() || localCursor->holdsMonitoredObject()))
            {
            uint32_t size = localCursor->getRoundedSize();
            if (size > colourToSizeMap[colour])
               {
               colourToSizeMap[colour] = size;
               }
            }
         }
      }

   // Map all garbage collected references together so we can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   int32_t firstLocalOffset = linkageProperties.getOffsetToFirstLocal();
   uint32_t stackIndex = firstLocalOffset;
   int32_t lowGCOffset = stackIndex;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();
#ifdef DEBUG
   uint32_t origSize = 0;
   bool     isFirst  = false;
#endif
   // Here we map the garbage collected references onto the stack
   // This stage is reversed later on, since in CodeGenGC we actually set all of the GC offsets
   // so effectively the local stack compaction of collected references happens there
   // but we must perform this stage to determine the size of the stack that contains object temp slots
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         TR_IGNode *igNode = cg->getLocalsIG()->getIGNodeForEntity(localCursor);
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
               mapSingleAutomatic(localCursor, stackIndex);
               }
            else if (colourToOffsetMap[colour] == -1)
               {
               mapSingleAutomatic(localCursor, stackIndex);
               colourToOffsetMap[colour] = localCursor->getOffset();
#ifdef DEBUG
               isFirst = true;
#endif
               }
            else
               {
               performSharing = performTransformation(comp, "%sSharing slot for local %p\n", OPT_DETAILS, localCursor);

               if (performSharing)
                  localCursor->setOffset(colourToOffsetMap[colour]);
               else
                  {
                  mapSingleAutomatic(localCursor, stackIndex);
                  colourToOffsetMap[colour] = localCursor->getOffset();
#ifdef DEBUG
                  isFirst = true;
#endif
                  }
               }

#ifdef DEBUG
            if (debug("reportCL"))
               {
               diagnostic("%s ref local %p (colour=%d): %s\n", isFirst ? "First" : "Shared", localCursor, colour, comp->signature());
               isFirst = false;
               }
#endif
            }
         else
            {
            mapSingleAutomatic(localCursor, stackIndex);
            }

#ifdef DEBUG
         origSize += localCursor->getRoundedSize();
#endif
         }
      }

   alignLocalReferences(stackIndex);

   const uint8_t pointerSize = TR::Compiler->om.sizeofReferenceAddress();

   // Here is where we reverse the previous stage
   // We map local references again to set the stack position correct according to
   // the GC map index, which is set in CodeGenGC
   //
   automaticIterator.reset();
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         int32_t newOffset = stackIndex + pointerSize * (localCursor->getGCMapIndex() - firstLocalGCIndex);
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "\nmapCompactedStack: changing %s (GC index %d) offset from %d to %d",
               comp->getDebug()->getName(localCursor), localCursor->getGCMapIndex(), localCursor->getOffset(), newOffset);
            }

         localCursor->setOffset(newOffset);

         TR_ASSERT_FATAL((localCursor->getOffset() <= 0), "Local %p (GC index %d) offset cannot be positive (stackIndex = %d)\n", localCursor, localCursor->getGCMapIndex(), stackIndex);

         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
            }
         }
      }

   method->setObjectTempSlots((lowGCOffset - stackIndex) / pointerSize);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   localSizeTestMethod testMethods[3] = {&isLocalSize4Byte, &isLocalSize8Byte, &isLocalSizeLargerThan8Byte};
   uint32_t alignments[3] = {4, 8, 16};

   // First, map 4-byte autos, then 8-byte autos, finally others (vectors and stack allocated objects).
   for (int32_t i = 0; i < 3; i++)
      {
      // Ensure the frame is properly aligned.
      //
      uint32_t roundup = (alignments[i] - 1);
#ifdef DEBUG
      origSize == (origSize + roundup) & (~roundup);
#endif
      stackIndex &= ~roundup;

      automaticIterator.reset();
      for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
         {
         if (localCursor->getGCMapIndex() < 0)
            {
            TR_IGNode *igNode = self()->cg()->getLocalsIG()->getIGNodeForEntity(localCursor);
            if (igNode)
               {
               IGNodeColour colour = igNode->getColour();

               if (testMethods[i](colourToSizeMap[colour]))
                  {
                  if (colourToOffsetMap[colour] == -1)
                     {
                     stackIndex &= ~roundup;
                     mapSingleAutomatic(localCursor, colourToSizeMap[colour], stackIndex);
                     colourToOffsetMap[colour] = localCursor->getOffset();
   #ifdef DEBUG
                     isFirst = true;
   #endif
                     }
                  else  // share local with already mapped stack slot
                     {
                     if (comp->getOption(TR_TraceCG))
                        traceMsg(comp, "%sSharing slot for local %p (colour = %d)\n", OPT_DETAILS, localCursor, colour);

                     localCursor->setOffset(colourToOffsetMap[colour]);
                     }

   #ifdef DEBUG
                  origSize += localCursor->getRoundedSize();

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
               }
            else if (testMethods[i](localCursor->getRoundedSize()))
               {
   #ifdef DEBUG
               origSize += localCursor->getRoundedSize();
               if(debug("reportCL"))
                  diagnostic("No ig node exists for local %p, mapping regularly\n", localCursor);
   #endif
               stackIndex &= ~roundup;
               mapSingleAutomatic(localCursor, stackIndex);
               }
            }
         }
      }

   method->setScalarTempSlots((lowGCOffset - stackIndex) / pointerSize);
   method->setLocalMappingCursor(stackIndex);

   mapIncomingParms(method);

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + getOffsetToFirstParm() - firstLocalOffset);
   } // scope of the stack memory region ends here

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

void OMR::ARM64::Linkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   /* do nothing */
   }

void OMR::ARM64::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   mapSingleAutomatic(p, p->getRoundedSize(), stackIndex);
   }

void OMR::ARM64::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t size, uint32_t &stackIndex)
   {
   p->setOffset(stackIndex -= size);
   }

void OMR::ARM64::Linkage::mapIncomingParms(TR::ResolvedMethodSymbol *method)
   {
   /* do nothing */
   }

void OMR::ARM64::Linkage::initARM64RealRegisterLinkage()
   {
   /* do nothing */
   }

void OMR::ARM64::Linkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   /* do nothing */
   }

TR::MemoryReference *OMR::ARM64::Linkage::getOutgoingArgumentMemRef(TR::Register *baseReg, int32_t offset, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::ARM64MemoryArgument &memArg)
   {
   const TR::ARM64LinkageProperties& properties = self()->getProperties();

   TR::MemoryReference *result = TR::MemoryReference::createWithDisplacement(cg(), baseReg, offset);
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode;

   return result;
   }

TR::Instruction *OMR::ARM64::Linkage::saveParametersToStack(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   return cursor;
   }

TR::Instruction *OMR::ARM64::Linkage::loadStackParametersToLinkageRegisters(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   return cursor;
   }

TR::Register *OMR::ARM64::Linkage::pushIntegerWordArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      pushRegister = cg->allocateRegister();
      loadConstant32(cg, child, child->getInt(), pushRegister);
      child->setRegister(pushRegister);
      }
   else
      {
      pushRegister = cg->evaluate(child);
      }
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::ARM64::Linkage::pushAddressArg(TR::Node *child)
   {
   TR_ASSERT(child->getDataType() == TR::Address, "assumption violated");
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::ARM64::Linkage::pushLongArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      pushRegister = cg->allocateRegister();
      loadConstant64(cg, child, child->getLongInt(), pushRegister);
      child->setRegister(pushRegister);
      }
   else
      {
      pushRegister = cg->evaluate(child);
      }
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::ARM64::Linkage::pushFloatArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::ARM64::Linkage::pushDoubleArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

int32_t
OMR::ARM64::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
   {
   switch (kind)
      {
      case TR_GPR:
         return self()->getProperties().getNumIntArgRegs();
      case TR_FPR:
         return self()->getProperties().getNumFloatArgRegs();
      default:
         return 0;
      }
   }

/**
 * @brief Returns opcode for loading parameters with the given type.
 *
 * @param[in] type : the type of parameters
 *
 * @return The opcode used to load parameters with the given type
 */
static inline TR::InstOpCode::Mnemonic getOpCodeForParmLoads(TR::DataTypes type)
   {
   switch (type)
      {
      case TR::Double:
         return TR::InstOpCode::vldrimmd;
      case TR::Float:
         return TR::InstOpCode::vldrimms;
      case TR::Int64:
      case TR::Address:
         return TR::InstOpCode::ldrimmx;
      default:
         return TR::InstOpCode::ldrimmw;
      }
   }

/**
 * @brief Returns opcode for storing parameters with the given type.
 *
 * @param[in] type : the type of parameters
 *
 * @return The opcode used to store parameters with the given type
 */
static inline TR::InstOpCode::Mnemonic getOpCodeForParmStores(TR::DataTypes type)
   {
   switch (type)
      {
      case TR::Double:
         return TR::InstOpCode::vstrimmd;
      case TR::Float:
         return TR::InstOpCode::vstrimms;
      case TR::Int64:
      case TR::Address:
         return TR::InstOpCode::strimmx;
      default:
         return TR::InstOpCode::strimmw;
      }
   }

/**
 * @brief Returns true if the global register is assigned to the parameter.
 *
 * @param[in] parm : the parameter
 *
 * @return true if the global register is assigned to the parameter
 */
static inline bool isGlobalRegisterAssigned(TR::ParameterSymbol *parm)
   {
   return parm->getAssignedGlobalRegisterIndex() != -1;
   }

TR::Instruction *OMR::ARM64::Linkage::copyParametersToHomeLocation(TR::Instruction *cursor, bool parmsHaveBeenStored)
   {
   TR::Machine *machine = cg()->machine();
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::RealRegister *stackPtr = machine->getRealRegister(properties.getStackPointerRegister());

   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol *paramCursor;
   struct MovStatus {
      TR::RealRegister::RegNum sourceReg;
      TR::RealRegister::RegNum targetReg;
      TR::DataTypes            type;
   };
   const TR::RealRegister::RegNum noReg = TR::RealRegister::NoReg;
   TR_ASSERT(noReg == 0, "noReg must be zero so zero-initializing movStatus will work");
   MovStatus movStatus[TR::RealRegister::NumRegisters] = {{(TR::RealRegister::RegNum)0,(TR::RealRegister::RegNum)0, (TR::DataTypes)0}};

   // We must always do the stores first, then the reg-reg copies, then the
   // loads, so that we never clobber a register we will need later.  However,
   // the logic is simpler if we do the loads and stores in the same loop.
   // Therefore, we maintain a separate instruction cursor for the loads.
   //
   // We defer the initialization of loadCursor until we generate the first
   // load.  Otherwise, if we happen to generate some stores first, then the
   // store cursor would get ahead of the loadCursor, and the instructions
   // would end up in the wrong order despite our efforts.
   //
   TR::Instruction *loadCursor = NULL;

   // Phase 1: generate stores and loads, and collect information about
   // the required RegReg movs.
   //
   // Store to stack all parameters passed in linkage registers
   //
   for (paramCursor = paramIterator.getFirst();
        paramCursor != NULL;
        paramCursor = paramIterator.getNext())
      {
      const int8_t lri = paramCursor->getLinkageRegisterIndex();     // How the parameter enters the method
      const TR::RealRegister::RegNum ai                              // Where method body expects to find it
         = (TR::RealRegister::RegNum)paramCursor->getAssignedGlobalRegisterIndex();
      const int32_t offset = paramCursor->getParameterOffset();      // Location of the parameter's stack slot
      const TR::DataTypes paramType = paramCursor->getType().getDataType();

      // Copy the parameter to wherever it should be
      //
      if (!paramCursor->isParmPassedInRegister()) // It's on the stack
         {
         if (isGlobalRegisterAssigned(paramCursor)) // Method body expects it to be in the ai register
            {
            if (loadCursor == NULL)
               loadCursor = cursor;

            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Loading %d\n", ai);

            // ai := stack
            TR::MemoryReference *stackMR = TR::MemoryReference::createWithDisplacement(cg(), stackPtr, offset);
            loadCursor = generateTrg1MemInstruction(cg(), getOpCodeForParmLoads(paramType), NULL, machine->getRealRegister(ai), stackMR, loadCursor);
            }
         }
      else // It's in a linkage register
         {
         TR::RealRegister::RegNum sourceIndex = paramCursor->getType().isFloatingPoint() ?
                                 properties.getFloatArgumentRegister(lri) : properties.getIntegerArgumentRegister(lri);
         // Copy to the stack if necessary
         //
         if (!isGlobalRegisterAssigned(paramCursor) || hasToBeOnStack(paramCursor))
            {

            if (parmsHaveBeenStored)
               {
               if (debug("traceCopyParametersToHomeLocation"))
                  diagnostic("copyParametersToHomeLocation: Skipping store of %d because parmsHaveBeenStored already\n", sourceIndex);
               }
            else
               {
               if (debug("traceCopyParametersToHomeLocation"))
                  diagnostic("copyParametersToHomeLocation: Storing %d\n", sourceIndex);

               TR::RealRegister *linkageReg = machine->getRealRegister(sourceIndex);
               TR::MemoryReference *stackMR = TR::MemoryReference::createWithDisplacement(cg(), stackPtr, offset);
               cursor = generateMemSrc1Instruction(cg(), getOpCodeForParmStores(paramType), NULL, stackMR, linkageReg, cursor);
               }
            }

         // Copy to the ai register if necessary
         //
         if (isGlobalRegisterAssigned(paramCursor) && ai != sourceIndex)
            {
            // This parameter needs a RegReg move.  We don't know yet whether
            // we need the value in the target register, so for now we just
            // remember that we need to do this and keep going.
            //
            TR_ASSERT(movStatus[ai         ].sourceReg == noReg, "Each target reg must have only one source");
            TR_ASSERT(movStatus[sourceIndex].targetReg == noReg, "Each source reg must have only one target");
            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Planning to move %d to %d\n", sourceIndex, ai);
            movStatus[ai].sourceReg                  = sourceIndex;
            movStatus[sourceIndex].targetReg         = ai;
            movStatus[sourceIndex].type              = paramType;
            }

         if (debug("traceCopyParametersToHomeLocation") && ai == sourceIndex)
            {
            diagnostic("copyParametersToHomeLocation: Parameter #%d already in register %d\n", lri, ai);
            }
         }
      }

   // Phase 2: Iterate through the parameters again to insert the RegReg moves.
   //
   for (paramCursor = paramIterator.getFirst();
       paramCursor != NULL;
       paramCursor = paramIterator.getNext())
      {
      if (!paramCursor->isParmPassedInRegister())
         continue;

      const int8_t lri = paramCursor->getLinkageRegisterIndex();
      const TR::RealRegister::RegNum paramReg = paramCursor->getType().isFloatingPoint() ?
                                 properties.getFloatArgumentRegister(lri) : properties.getIntegerArgumentRegister(lri);

      if (movStatus[paramReg].targetReg == noReg)
         {
         // This parameter does not need to be copied anywhere
         if (debug("traceCopyParametersToHomeLocation"))
            diagnostic("copyParametersToHomeLocation: Not moving %d\n", paramReg);
         }
      else
         {
         if (debug("traceCopyParametersToHomeLocation"))
            diagnostic("copyParametersToHomeLocation: Preparing to move %d\n", paramReg);

         // If a mov's target register is the source for another mov, we need
         // to do that other mov first.  The idea is to find the end point of
         // the chain of movs starting with paramReg and ending with a
         // register whose current value is not needed; then do that chain of
         // movs in reverse order.
         //
         TR::RealRegister::RegNum regCursor;

         // Find the last target in the chain
         //
         regCursor = movStatus[paramReg].targetReg;
         while(movStatus[regCursor].targetReg != noReg)
            {
            // Haven't found the end yet
            regCursor = movStatus[regCursor].targetReg;
            if (regCursor == paramReg)
               {
               comp()->failCompilation<TR::CompilationException>("Can't yet handle cyclic dependencies\n");
               }

            // TODO Use scratch register to break cycles
            //
            // A properly-written pickRegister should never
            // cause cycles to occur in the first place.  However, we may want
            // to consider adding cycle-breaking logic so that (1) pickRegister
            // has more flexibility, and (2) we're more robust against
            // otherwise harmless bugs in pickRegister.
            }

         // Work our way backward along the chain, generating all the necessary movs
         //
         while(movStatus[regCursor].sourceReg != noReg)
            {
            TR::RealRegister::RegNum source = movStatus[regCursor].sourceReg;
            if (debug("traceCopyParametersToHomeLocation"))
               diagnostic("copyParametersToHomeLocation: Moving %d to %d\n", source, regCursor);
            // regCursor := regCursor.sourceReg
            TR::DataTypes paramType = movStatus[source].type;
            TR::Register *trgReg = machine->getRealRegister(regCursor);
            TR::Register *srcReg = machine->getRealRegister(source);
            if ((paramType == TR::Double) || (paramType == TR::Float))
               {
               cursor = generateTrg1Src1Instruction(cg(), (paramType == TR::Double) ? TR::InstOpCode::fmovd : TR::InstOpCode::fmovs,
                        NULL, trgReg, srcReg, cursor);
               }
            else
               {
               // generateMovInstruction cannot be used in the binary encoding phase because it relies on RegDep for assigning xzr for 1st source register.
               cursor = generateTrg1Src2Instruction(cg(), (paramType == TR::Int64) || (paramType == TR::Address) ? TR::InstOpCode::orrx : TR::InstOpCode::orrw,
                        NULL, trgReg, machine->getRealRegister(TR::RealRegister::xzr), srcReg, cursor);
               }

            // Update movStatus as we go so we don't generate redundant movs
            movStatus[regCursor].sourceReg = noReg;
            movStatus[source   ].targetReg = noReg;
            // Continue with the next register in the chain
            regCursor = source;
            }
         }
      }

   // Return the last instruction we inserted, whether or not it was a load.
   //
   return loadCursor? loadCursor : cursor;
   }

bool OMR::ARM64::Linkage::killsVectorRegisters()
   {
   // We need to kill vector registers if there is any live one.
   TR_LiveRegisters *liveRegs = cg()->getLiveRegisters(TR_VRF);
   return (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);
   }
