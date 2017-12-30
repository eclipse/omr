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

// See also S390Linkage.cpp which contains more S390 Linkage
// implementations (primarily Private Linkage and base class).

#include <algorithm>                                // for std::max
#include <stddef.h>                                 // for NULL, size_t
#include <stdint.h>                                 // for int32_t, etc
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                     // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/Instruction.hpp"                  // for Instruction
#include "codegen/Linkage.hpp"                      // for REGNUM, etc
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/Snippet.hpp"
#include "codegen/SystemLinkage.hpp"                // for SystemLinkage
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                  // for Compilation
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"                         // for Allocator, etc
#include "env/jittypes.h"                           // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes::acall, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node
#include "il/Node_inlines.hpp"                      // for Node::getChild, etc
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"               // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"            // for ParameterSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/List.hpp"                           // for ListIterator, etc
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "ras/Delimiter.hpp"                        // for Delimiter
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/TRSystemLinkage.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

#define  GPREGINDEX(i)   (i-TR::RealRegister::FirstGPR)

////////////////////////////////////////////////////////////////////////////////
// TR::S390zOSSystemLinkage for J9
////////////////////////////////////////////////////////////////////////////////

TR::SystemLinkage *
TR::S390SystemLinkage::self()
   {
   return static_cast<TR::SystemLinkage *>(this);
   }

void
TR::S390SystemLinkage::initS390RealRegisterLinkage()
   {
   int32_t icount = 0, ret_count=0;

   TR::RealRegister * spReal  = getStackPointerRealRegister();
   spReal->setState(TR::RealRegister::Locked);
   spReal->setAssignedRegister(spReal);
   spReal->setHasBeenAssignedInMethod(true);

   if (cg()->supportsHighWordFacility() && !comp()->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
      {
      TR::RealRegister * tempHigh = toRealRegister(spReal)->getHighWordRegister();
      tempHigh->setState(TR::RealRegister::Locked);
      tempHigh->setAssignedRegister(tempHigh);
      tempHigh->setHasBeenAssignedInMethod(true);
      }

#if 0
   TR::RealRegister * gpr0Real  = machine()->getS390RealRegister(TR::RealRegister::GPR0);
   gpr0Real->setState(TR::RealRegister::Locked);
   gpr0Real->setAssignedRegister(gpr0Real);
   gpr0Real->setHasBeenAssignedInMethod(true);
#endif

#if 0
// May lock this if GOT needed
   TR::RealRegister * gpr12Real  = cg()->machine()->getS390RealRegister(TR::RealRegister::GPR12);
   gpr12Real->setState(TR::RealRegister::Locked);
   gpr12Real->setAssignedRegister(gpr12Real);
   gpr12Real->setHasBeenAssignedInMethod(true);
#endif

   // additional (forced) restricted regs.
   // Similar code in TR_S390Machine::initializeGlobalRegisterTable() for GRA tables

   for (icount = TR::RealRegister::FirstGPR; icount < TR::RealRegister::LastGPR; ++icount)
      {
      TR::RealRegister * regReal = cg()->machine()->getS390RealRegister(icount);
      if (cg()->machine()->isRestrictedReg(regReal->getRegisterNumber()))
         {
         regReal->setState(TR::RealRegister::Locked);
         regReal->setAssignedRegister(regReal);
         regReal->setHasBeenAssignedInMethod(true);
         if (cg()->supportsHighWordFacility() && !comp()->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
            {
            TR::RealRegister * tempHigh = toRealRegister(regReal)->getHighWordRegister();
            tempHigh->setState(TR::RealRegister::Locked);
            tempHigh->setAssignedRegister(tempHigh);
            tempHigh->setHasBeenAssignedInMethod(true);
            }
         }
      }

   // set register weight
   for (icount = TR::RealRegister::FirstGPR; icount >= TR::RealRegister::LastAssignableGPR; icount++)
      {
      int32_t weight;
      if (getIntegerReturn((TR::RealRegister::RegNum) icount))
         {
         weight = ++ret_count;
         }
      else if (getPreserved((TR::RealRegister::RegNum) icount))
         {
         weight = 0xf000 + icount;
         }
      else
         {
         weight = icount;
         }
      cg()->machine()->getS390RealRegister((TR::RealRegister::RegNum) icount)->setWeight(weight);
      }
   }


// Non-Java use
void
TR::S390SystemLinkage::initParamOffset(TR::ResolvedMethodSymbol * method, int32_t stackIndex, List<TR::ParameterSymbol> *parameterList)
{
   ListIterator<TR::ParameterSymbol> parameterIterator((parameterList != NULL)? parameterList : &method->getParameterList());
   parameterIterator.reset();
   TR::ParameterSymbol * parmCursor = parameterIterator.getFirst();
   int8_t gprSize = cg()->machine()->getGPRSize();

   int32_t paramNum = -1;
   setIncomingParmAreaBeginOffset(stackIndex);

   while (parmCursor != NULL)
      {
      paramNum++;

      int8_t lri = parmCursor->getLinkageRegisterIndex();
      int32_t parmSize = parmCursor->getSize();
      if (lri == -1)
         {
         parmCursor->setParameterOffset(stackIndex);
         stackIndex += std::max<int32_t>(parmCursor->getSize(), cg()->machine()->getGPRSize());
         }
      else
         { // linkage parm registers reserved in caller save area
         TR::RealRegister::RegNum argRegNum;
         if (parmCursor->getType().isFloatingPoint())
             argRegNum = getFloatArgumentRegister(lri);
         else
             argRegNum = getIntegerArgumentRegister(lri);
         parmCursor->setParameterOffset(getStackFrameSize() + getRegisterSaveOffset(REGNUM(argRegNum)));
         }

      if (isSmallIntParmsAlignedRight() && parmCursor->getType().isIntegral() && (gprSize > parmCursor->getSize())) // WCode motivated
         {
         // linkage related
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + gprSize - parmCursor->getSize());
         }

      parmCursor = parameterIterator.getNext();
      }
   setIncomingParmAreaEndOffset(stackIndex);
}

/**
 * Non-Java use
 */
TR::SymbolReference *
TR::S390SystemLinkage::createAutoMarkerSymbol(TR_S390AutoMarkers markerType)
   {
   char *name = NULL;
   switch (markerType)
      {
      case TR_AutoMarker_EndOfParameterBlock:
         name = "end of parameter block";
         break;
      default:
         TR_ASSERT(false, "invalid auto marker symbol type");
         break;
      }

   TR::AutomaticSymbol *sym = TR::AutomaticSymbol::createMarker(trHeapMemory(),name);
   TR::SymbolReference *symRef = new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), sym);

   TR_Array<TR::SymbolReference*> *symbols = _autoMarkerSymbols;
   if (symbols == NULL)
      {
      symbols = new (trHeapMemory()) TR_Array<TR::SymbolReference*>(trMemory(), TR_AutoMarker_NumAutoMarkers, true);
      setAutoMarkerSymbols(symbols);
      }
   (*symbols)[markerType] = symRef;
   return symRef;
   }

/**
 * General utility
 * Perform a save or a restore operation of preserved FPRs from automatic
 * The operation depends on supplied opcode
 */
TR::Instruction *
TR::S390SystemLinkage::getputFPRs(TR::InstOpCode::Mnemonic opcode, TR::Instruction * cursor, TR::Node *node, TR::RealRegister *spReg)
   {
   int16_t FPRSaveMask;
   int32_t offset, firstSaved, lastSaved, beginSaveOffset;
   TR::MemoryReference *rsa;
   int32_t i,j, fprSize;

   if (spReg == NULL)
      {
      spReg = getNormalStackPointerRealRegister();
      }

   FPRSaveMask = getFPRSaveMask();
   beginSaveOffset = getFPRSaveAreaEndOffset(); // end offset is really begin offset -- see mapStack()

   firstSaved = TR::Linkage::getFirstMaskedBit(FPRSaveMask);
   lastSaved = TR::Linkage::getLastMaskedBit(FPRSaveMask);

   fprSize = cg()->machine()->getFPRSize();
   j = 0;
   for (i = firstSaved; i <= lastSaved; ++i)
      {
      if (FPRSaveMask & (1 << (i)))
         {
         offset = beginSaveOffset + (j*fprSize);
         rsa = generateS390MemoryReference(spReg, offset, cg());
         cursor = generateRXInstruction(cg(), opcode,
               node, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstFPR)), rsa, cursor);
         j++;
         }
      }

   return cursor;
   }

/**
 * General utility
 * Perform a save or a restore operation of preserved ARs from automatic
 * The operation depends on supplied opcode
 */
TR::Instruction *
TR::S390SystemLinkage::getputARs(TR::InstOpCode::Mnemonic opcode, TR::Instruction * cursor, TR::Node *node, TR::RealRegister *spReg)
   {
   int16_t ARSaveMask;
   int32_t offset, firstSaved, lastSaved, beginSaveOffset;
   TR::MemoryReference *rsa;
   int32_t i,j, arSize;

   if (spReg == NULL)
      {
      spReg = getNormalStackPointerRealRegister();
      }

   ARSaveMask = getARSaveMask();
   beginSaveOffset = getARSaveAreaEndOffset(); // end offset is really begin offset -- see mapStack()

   lastSaved = TR::Linkage::getLastMaskedBit(ARSaveMask);
   firstSaved = TR::Linkage::getFirstMaskedBit(ARSaveMask);
   arSize = cg()->machine()->getGPRSize();

   j = 0;
   for (i = firstSaved; i <= lastSaved; ++i)
      {
      if (ARSaveMask & (1 << (i)))
         {
         offset =  beginSaveOffset + (j*arSize);
         rsa = generateS390MemoryReference(spReg, offset, cg());
         cursor = generateRSInstruction(cg(), opcode,
               node, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)),
               getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)), rsa, cursor);
         j++;
         }
      }

   return cursor;
   }

TR::Instruction *
TR::S390SystemLinkage::getputHPRs(TR::InstOpCode::Mnemonic opcode, TR::Instruction * cursor, TR::Node *node, TR::RealRegister *spReg)
   {
   int16_t HPRSaveMask;
   int32_t offset, firstSaved, lastSaved, beginSaveOffset;
   TR::MemoryReference *rsa;
   int32_t i, j, hprSize;

   if (spReg == NULL)
      {
      spReg = getNormalStackPointerRealRegister();
      }

   HPRSaveMask = getHPRSaveMask();
   beginSaveOffset = getHPRSaveAreaEndOffset(); // following suit, see other getput functions, map stack

   lastSaved = TR::Linkage::getLastMaskedBit(HPRSaveMask);
   firstSaved = TR::Linkage::getFirstMaskedBit(HPRSaveMask);
   hprSize = cg()->machine()->getHPRSize();

   j = 0;
   uint8_t k = firstSaved; // dispel warning
   bool previousWasOne = false;
   offset = beginSaveOffset; // dispel warning
   for(i = firstSaved; i <= lastSaved; ++i)
      {
      if(HPRSaveMask & (1 << i))
         {
         if(!previousWasOne)
            {
            offset = beginSaveOffset + (j*hprSize);
            k = i;
            }
         j++;
         }
      if(previousWasOne && (i == lastSaved || !(HPRSaveMask & (1 << i))))
         {
         rsa = generateS390MemoryReference(spReg, offset, cg());
         int shift = (i == lastSaved) && (HPRSaveMask & (1 << i)) ? 0 : 1;
         cursor = generateRSInstruction(cg(), opcode,
               node, getS390RealRegister(REGNUM(k+TR::RealRegister::FirstGPR)),
               getS390RealRegister(REGNUM(i-shift+TR::RealRegister::FirstGPR)), rsa, cursor);
         }
      previousWasOne = HPRSaveMask & (1 << i);
      }

   return cursor;
   }

/**
 * General utility
 * Returns
 *  *** if checkTempNeeded parm is non-NULL ***
 *
 *   checkTempNeeded parm is set to whether temporary register is needed
 *   to produce the instruction sequence.
 *   This is used by the caller to determine if it should allocate a temporary
 *   register to provided on a subsequent call to this routine. The caller
 *   could always provide a temporary register - but that will have adverse
 *   affects on performance if the register is not needed.
 *
 *  *** if checkTempNeeded parm is NULL ***
 *   instruction  sequence
 */
TR::Instruction *
TR::S390SystemLinkage::addImmediateToRealRegister(TR::RealRegister *targetReg, int32_t value, TR::RealRegister *tempReg, TR::Node *node, TR::Instruction *cursor, bool *checkTempNeeded)
   {
   bool smallPositiveValue = (value<MAXDISP && value>=0);
   bool largeValue = (value<MIN_IMMEDIATE_VAL || value>MAX_IMMEDIATE_VAL);

   if (checkTempNeeded)
      *checkTempNeeded = false; // assume no reg needed - change this below

   if (smallPositiveValue)
      {
      if (!checkTempNeeded)
         cursor = generateRXInstruction(cg(), TR::InstOpCode::LA, node, targetReg, generateS390MemoryReference(targetReg,value,cg()),cursor);
      }
   else if (largeValue)
      {
      // TODO: could reduce number of cases dependent on temporary register
      //  For example, could generate sequence of AHI's for suitably small large value
      if (checkTempNeeded)
         *checkTempNeeded = true;
      else if (!tempReg)
         TR_ASSERT( 0,"temporary register needed for add to register");
      else
         {
         cursor = generateS390ImmToRegister(cg(), node, tempReg, (intptr_t)(value), cursor);
         cursor = generateRRInstruction(cg(), TR::InstOpCode::getAddRegOpCode(), node, targetReg, tempReg, cursor);
         }
      }

   if (!checkTempNeeded)
      cursor = generateRXYInstruction(cg(), TR::InstOpCode::LAY, node, targetReg, generateS390MemoryReference(targetReg,value,cg()), cursor);

   return cursor;
   }

/**
 * General utility
 * Reverse the bit ordering of the save mask
 */
uint16_t
TR::S390SystemLinkage::flipBitsRegisterSaveMask(uint16_t mask)
   {
   TR_ASSERT(NUM_S390_GPR == NUM_S390_FPR, "need special flip bits for FPR");
   uint16_t i, outputMask;

   outputMask = 0;
   for (i = 0; i < NUM_S390_GPR; i++)
       {
       if (mask & (1 << i))
          {
          outputMask |= 1 << (NUM_S390_GPR-1-i);
          }
       }
   return outputMask;
   }


TR::S390zOSSystemLinkage::S390zOSSystemLinkage(TR::CodeGenerator * codeGen)
   : TR::ZOSBaseSystemLinkageConnector(codeGen, TR_SystemXPLink)
   {
   // linkage properties
   setProperties(FirstParmAtFixedOffset);
   setProperty(SkipGPRsForFloatParms);
   if (TR::Compiler->target.is64Bit())
      {
      setProperty(NeedsWidening);
      }
   else
      {
      setProperty(FloatParmDescriptors);
      }
   setNumSpecialArgumentRegisters(0);  // could be changed by ilgen

   setRegisterFlag(TR::RealRegister::GPR4, Preserved);

   setRegisterFlag(TR::RealRegister::GPR8, Preserved);
   setRegisterFlag(TR::RealRegister::GPR9, Preserved);
   setRegisterFlag(TR::RealRegister::GPR10, Preserved);
   setRegisterFlag(TR::RealRegister::GPR11, Preserved);
   setRegisterFlag(TR::RealRegister::GPR12, Preserved);
   setRegisterFlag(TR::RealRegister::GPR13, Preserved);
   setRegisterFlag(TR::RealRegister::GPR14, Preserved);
   setRegisterFlag(TR::RealRegister::GPR15, Preserved);

   setRegisterFlag(TR::RealRegister::FPR8, Preserved);
   setRegisterFlag(TR::RealRegister::FPR9, Preserved);
   setRegisterFlag(TR::RealRegister::FPR10, Preserved);
   setRegisterFlag(TR::RealRegister::FPR11, Preserved);
   setRegisterFlag(TR::RealRegister::FPR12, Preserved);
   setRegisterFlag(TR::RealRegister::FPR13, Preserved);
   setRegisterFlag(TR::RealRegister::FPR14, Preserved);
   setRegisterFlag(TR::RealRegister::FPR15, Preserved);

   static const bool enableVectorLinkage = codeGen->getSupportsVectorRegisters();

   if (enableVectorLinkage)
      {
      setRegisterFlag(TR::RealRegister::VRF16, Preserved);
      setRegisterFlag(TR::RealRegister::VRF17, Preserved);
      setRegisterFlag(TR::RealRegister::VRF18, Preserved);
      setRegisterFlag(TR::RealRegister::VRF19, Preserved);
      setRegisterFlag(TR::RealRegister::VRF20, Preserved);
      setRegisterFlag(TR::RealRegister::VRF21, Preserved);
      setRegisterFlag(TR::RealRegister::VRF22, Preserved);
      setRegisterFlag(TR::RealRegister::VRF23, Preserved);
      }

   setIntegerReturnRegister (TR::RealRegister::GPR3 );
   setLongLowReturnRegister (TR::RealRegister::GPR3 );
   setLongHighReturnRegister(TR::RealRegister::GPR2 );
   setLongReturnRegister    (TR::RealRegister::GPR3 );
   setFloatReturnRegister   (TR::RealRegister::FPR0 );
   setDoubleReturnRegister  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister0  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister2  (TR::RealRegister::FPR2 );
   setLongDoubleReturnRegister4  (TR::RealRegister::FPR4 );
   setLongDoubleReturnRegister6  (TR::RealRegister::FPR6 );
   if (enableVectorLinkage) setVectorReturnRegister  (TR::RealRegister::VRF24);
   setStackPointerRegister  (TR::RealRegister::GPR4 );
   setNormalStackPointerRegister  (TR::RealRegister::GPR4 );
   setAlternateStackPointerRegister  (TR::RealRegister::GPR9 );
   setEntryPointRegister    (TR::RealRegister::GPR6);
   setLitPoolRegister       (TR::RealRegister::GPR8 );
   setExtCodeBaseRegister   (TR::RealRegister::GPR7  );
   setReturnAddressRegister (TR::RealRegister::GPR7 );

   setEnvironmentPointerRegister (TR::RealRegister::GPR5  );
   setCAAPointerRegister   (TR::RealRegister::GPR12  );
   setDebugHooksRegister(TR::Compiler->target.is64Bit() ? TR::RealRegister::GPR10 : TR::RealRegister::GPR12);

   setIntegerArgumentRegister(0, TR::RealRegister::GPR1);
   setIntegerArgumentRegister(1, TR::RealRegister::GPR2);
   setIntegerArgumentRegister(2, TR::RealRegister::GPR3);
   setNumIntegerArgumentRegisters(3);

   setFloatArgumentRegister(0, TR::RealRegister::FPR0);
   setFloatArgumentRegister(1, TR::RealRegister::FPR2);
   setFloatArgumentRegister(2, TR::RealRegister::FPR4);
   setFloatArgumentRegister(3, TR::RealRegister::FPR6);
   setNumFloatArgumentRegisters(4);

   if (enableVectorLinkage)
      {
      int vecIndex = 0;
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF25);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF26);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF27);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF28);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF29);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF30);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF31);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF24);
      setNumVectorArgumentRegisters(vecIndex);
      }

   setOffsetToFirstParm   ((TR::Compiler->target.is64Bit()) ? 2176 : 2112);
   setOffsetToRegSaveArea   (2048);
   setNumberOfDependencyGPRegisters(32);
   setFrameType(TR_XPLinkUnknownFrame);
   setLargestOutgoingArgumentAreaSize(0);
   setAutoMarkerSymbols(0);
   setOffsetToLongDispSlot(0);
   }


/**
 * Non-Java use.
 * Calculate XPLink call descriptor for given method body (excl. entry offset).
 * This value is present in PPA1 block and also used for determining register
 * linkage for incoming floating point parameters.
 */
uint32_t
TR::S390zOSSystemLinkage::calculateInterfaceMappingFlags(TR::ResolvedMethodSymbol *method)
   {
   uint32_t callDescValue;

   if (TR::Compiler->target.is64Bit())
      {
      return 0;
      }

   // Linkage field  (0 means XPLink)
   // Bits 0-2 inclusive
   //
   callDescValue = 0x00000000;

   //
   // Return value adjust field
   // Bits 3-7 inclusive
   //

   TR::DataType dataType = TR::NoType;
   int32_t aggregateLength = 0;
      TR_ASSERT( 0, "dont know how to get datatype of non-WCode method");
   uint32_t rva = calculateReturnValueAdjustFlag(dataType, aggregateLength);
   callDescValue |= rva << 24;

   //
   // Float parameter description fields
   // Bits 8-31 inclusive
   //

   uint32_t parmDescriptorFields = 0;

   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   parameterIterator.reset();
   TR::ParameterSymbol * parmCursor = parameterIterator.getFirst();
   TR::Symbol *funcSymbol = method;

   int32_t parmCount = 1;

   int32_t floatParmNum = 0;
   uint32_t parmAreaOffset = 0;
   uint32_t lastFloatParmAreaOffset= 0;

   bool done = false;
   while ((parmCursor != NULL) && !done)
      {
      TR::Symbol *parmSymbol = parmCursor;
      TR::DataType dataType = parmSymbol->getDataType();
      int32_t argSize = parmSymbol->getSize();

      done = updateFloatParmDescriptorFlags(&parmDescriptorFields, funcSymbol, parmCount, argSize, dataType, &floatParmNum, &lastFloatParmAreaOffset, &parmAreaOffset);

      parmCursor = parameterIterator.getNext();
      parmCount++;
      }

   callDescValue |= parmDescriptorFields;
   return callDescValue;
   }

/**
 * XPLink utility
 * Calculate "Return value adjust" component of XPLink call descriptor
 */
uint32_t
TR::S390zOSSystemLinkage::calculateReturnValueAdjustFlag(TR::DataType dataType, int32_t aggregateLength)
   {
   // 5 bit values for "return value adjust" field of XPLink descriptor
   #define XPLINK_RVA_RETURN_VOID_OR_UNUSED    0x00
   #define XPLINK_RVA_RETURN_INT32_OR_LESS     0x01
   #define XPLINK_RVA_RETURN_INT64             0x02
   #define XPLINK_RVA_RETURN_FAR_POINTER       0x04
   #define XPLINK_RVA_RETURN_FLOAT4            0x08
   #define XPLINK_RVA_RETURN_FLOAT8            0x09
   #define XPLINK_RVA_RETURN_FLOAT16           0x0A
   #define XPLINK_RVA_RETURN_COMPLEX4          0x0C
   #define XPLINK_RVA_RETURN_COMPLEX8          0x0D
   #define XPLINK_RVA_RETURN_COMPLEX16         0x0E
   #define XPLINK_RVA_RETURN_AGGREGATE         0x10 // lower bits have meaning

   uint32_t rva = 0;

   switch (dataType)
      {
      case TR::NoType:
           rva = XPLINK_RVA_RETURN_VOID_OR_UNUSED;
           break;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Address:
           rva = XPLINK_RVA_RETURN_INT32_OR_LESS;
           break;
      case TR::Int64:
           rva = XPLINK_RVA_RETURN_INT64;
           break;
      case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:
#endif
           rva = XPLINK_RVA_RETURN_FLOAT4;
           break;
      case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalDouble:
#endif
           rva = XPLINK_RVA_RETURN_FLOAT8;
           break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalLongDouble:
           rva = XPLINK_RVA_RETURN_FLOAT16;
           break;
      case TR::PackedDecimal:
      case TR::ZonedDecimal:
      case TR::ZonedDecimalSignLeadingEmbedded:
      case TR::ZonedDecimalSignLeadingSeparate:
      case TR::ZonedDecimalSignTrailingSeparate:
      case TR::UnicodeDecimal:
      case TR::UnicodeDecimalSignLeading:
      case TR::UnicodeDecimalSignTrailing:
#endif
      case TR::Aggregate:
           TR_ASSERT( aggregateLength != 0, "aggregate length is zero");
           rva = XPLINK_RVA_RETURN_AGGREGATE;
           if (isAggregateReturnedInIntRegisters(aggregateLength))
              {
              rva = rva + aggregateLength;
              }
           break;
      default:
           TR_ASSERT( 0, "unknown datatype for parm descriptor calculation");
           break;
      }

   return rva;
   }

/**
 * XPLink utility
 * Utility return to help float parameter fields of XPLink call descriptor
 * This routine is called once for each "parameter" unless the method returns
 * false - meaning float parameter fields of the call descriptor are now fully
 * determined.
 */
bool
TR::S390zOSSystemLinkage::updateFloatParmDescriptorFlags(uint32_t *parmDescriptorFields, TR::Symbol *funcSymbol, int32_t parmCount, int32_t argSize, TR::DataType dataType, int32_t *floatParmNum, uint32_t *lastFloatParmAreaOffset, uint32_t *parmAreaOffset)
   {
   uint32_t gprSize = cg()->machine()->getGPRSize();

   // Note: complex type is attempted to be handled although other code needs
   // to change in 390 codegen to support complex
   //
   // PERFORMANCE TODO: it is desirable to use the defined "parameter count" of
   // the function symbol to help determine if we have an unprototyped argument
   // of a call (site) to a vararg function.  Currently we overcompensate for
   // outgoing float parms to vararg functions and always shadow in FPR and
   // and stack/gprs as with an unprotoyped call - see pushArg(). Precise
   // information can help remove such compensation. Changes to fix this would
   // involve: this function, pushArg() and buildArgs().

   int32_t numFPRsNeeded = 0;
   switch (dataType)
      {
      case TR::Float:
      case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:
      case TR::DecimalDouble:
#endif
         numFPRsNeeded = 1;
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalLongDouble:
         break;
#endif
      }

   if (numFPRsNeeded != 0)
      {
      uint32_t unitSize = argSize / numFPRsNeeded;
      uint32_t wordsToPreviousParm = ((*parmAreaOffset) - (*lastFloatParmAreaOffset))/gprSize;
      if (wordsToPreviousParm > 0xF)
         { // to big for descriptor. Will pass in stack
         return true; // done
         }
      uint32_t val = wordsToPreviousParm + ((unitSize == 4) ? 0x10 : 0x20);

      (*parmDescriptorFields) |= shiftFloatParmDescriptorFlag(val, (*floatParmNum));

      (*floatParmNum)++;
      if ((*floatParmNum) >= getNumFloatArgumentRegisters())
         {
         return true; // done
         }

      if (numFPRsNeeded == 2)
         {
         (*parmDescriptorFields) |= shiftFloatParmDescriptorFlag(0x33, (*floatParmNum));  // 0x33 == bits 110001
         (*floatParmNum)++;
         if ((*floatParmNum) >= getNumFloatArgumentRegisters())
            {
            return true; // done
            }
         }
      }
   (*parmAreaOffset) += argSize < gprSize ? gprSize : argSize;
   if (numFPRsNeeded != 0)
      (*lastFloatParmAreaOffset) = (*parmAreaOffset);

   return false;
   }

TR::Register *
TR::S390zOSSystemLinkage::loadEnvironmentIntoRegister(TR::Node * callNode, TR::RegisterDependencyConditions * deps, bool loadIntoEnvironmentRegister)
   {
   // Important note: this should only be called in pre-prologue phase for routines we know are
   // not leaf routines.

   TR::Register *envReg;
   if (loadIntoEnvironmentRegister)
      {
      TR::Register * systemEnvironmentRegister  = deps->searchPostConditionRegister(getEnvironmentPointerRegister()); // gpr 5
      envReg = systemEnvironmentRegister;
      }
   else
      {
      envReg = cg()->allocateRegister();
      }


   return envReg;
   }

/**
 * WCode use
 */
int32_t
TR::S390zOSSystemLinkage::getOutgoingParameterBlockSize()
   {
   //
   // Calculate size of outgoing argument area
   //
   int32_t argAreaSize = getLargestOutgoingArgumentAreaSize();
   if (argAreaSize)
      {
      // xplink spec has minimal size
      // we assume yes to be safe
      int32_t minimalArgAreaSize = ((TR::Compiler->target.is64Bit()) ? 32 : 16);
      argAreaSize = (argAreaSize < minimalArgAreaSize) ? minimalArgAreaSize : argAreaSize;
      }
   return argAreaSize;
   }

////////////////////////////////////////////////////////////////////////////////
// TR::S390zLinuxSystemLinkage for J9
////////////////////////////////////////////////////////////////////////////////
TR::S390zLinuxSystemLinkage::S390zLinuxSystemLinkage(TR::CodeGenerator * codeGen)
   : TR::SystemLinkage(codeGen,TR_SystemLinux)
   {
   // linkage properties
   setProperties(FirstParmAtFixedOffset);
   if (TR::Compiler->target.is64Bit())
      {
      setProperty(NeedsWidening);
      setProperty(PadFloatParms);
      }

   setRegisterFlag(TR::RealRegister::GPR6, Preserved);
   setRegisterFlag(TR::RealRegister::GPR6, IntegerArgumentAddToPost);        // I only need to do this for r6 because r6 is the only preserved linkage register on zlinux.
   setRegisterFlag(TR::RealRegister::GPR7, Preserved);
   setRegisterFlag(TR::RealRegister::GPR8, Preserved);
   setRegisterFlag(TR::RealRegister::GPR9, Preserved);
   setRegisterFlag(TR::RealRegister::GPR10, Preserved);
   setRegisterFlag(TR::RealRegister::GPR11, Preserved);
   setRegisterFlag(TR::RealRegister::GPR12, Preserved);
   setRegisterFlag(TR::RealRegister::GPR13, Preserved);
   setRegisterFlag(TR::RealRegister::GPR14, Preserved);      // Setting the return address register as preserved so that way we do not always have to emit stores in prologues / epilogues
                                                                // This possibly assumes that we don't shuffle the return address register around elsewhere
   setRegisterFlag(TR::RealRegister::GPR15, Preserved);

   // all FP registers are not preserved for zTPF
      {
      if (TR::Compiler->target.is64Bit())
         {
         setRegisterFlag(TR::RealRegister::FPR8, Preserved);
         setRegisterFlag(TR::RealRegister::FPR9, Preserved);
         setRegisterFlag(TR::RealRegister::FPR10, Preserved);
         setRegisterFlag(TR::RealRegister::FPR11, Preserved);
         setRegisterFlag(TR::RealRegister::FPR12, Preserved);
         setRegisterFlag(TR::RealRegister::FPR13, Preserved);
         setRegisterFlag(TR::RealRegister::FPR14, Preserved);
         setRegisterFlag(TR::RealRegister::FPR15, Preserved);
         }
      else
         {
         setRegisterFlag(TR::RealRegister::FPR4, Preserved);
         setRegisterFlag(TR::RealRegister::FPR6, Preserved);
         }
      }

   setIntegerArgumentRegister(0, TR::RealRegister::GPR2);
   setIntegerArgumentRegister(1, TR::RealRegister::GPR3);

   if (!comp()->getCurrentMethod()->isJ9())
      {
      setProperty(AggregatesPassedInParmRegs);
      setProperty(AggregatesPassedOnParmStack);
      // This property could be controlled by wcode INFO gcclongdouble
      setProperty(LongDoubleReturnedOnStorage);
      setProperty(LongDoublePassedOnStorage);
      // This property could be controlled by wcode INFO gcccomplex
      setProperty(ComplexReturnedOnStorage);
      setProperty(ComplexPassedOnStorage);
      setIntegerArgumentRegister(2, TR::RealRegister::GPR4);
      setIntegerArgumentRegister(3, TR::RealRegister::GPR5);
      setIntegerArgumentRegister(4, TR::RealRegister::GPR6);
      }
   else
      {
      // For JIT to native transition, We will set up param4 and parm4 in GPR8 and GPR9
      // during param evaluation and will move them to GPR5 and GPR6 just before going
      // into native code. This is needed for correct evaluation of parms as GPR5 is
      // java stack pointer and GPR6 may be Literal pool pointer
      setIntegerArgumentRegister(2, TR::RealRegister::GPR8);
      setIntegerArgumentRegister(3, TR::RealRegister::GPR9);
      setIntegerArgumentRegister(4, TR::RealRegister::GPR10);
      }

   setNumIntegerArgumentRegisters(5);

   setFloatArgumentRegister(0, TR::RealRegister::FPR0);
   setFloatArgumentRegister(1, TR::RealRegister::FPR2);
   setNumFloatArgumentRegisters(2);

   static const bool enableVectorLinkage = codeGen->getSupportsVectorRegisters();

   if (enableVectorLinkage)
      {
      int vecIndex = 0;
      // The vector registers used for parameter passing are allocated in the following order to enable
      // future exension which might operate on vector register pairs.
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF24);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF26);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF28);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF30);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF25);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF27);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF29);
      setVectorArgumentRegister(vecIndex++, TR::RealRegister::VRF31);
      setNumVectorArgumentRegisters(vecIndex);
      }

   if (TR::Compiler->target.is64Bit())
      {
      setFloatArgumentRegister(2, TR::RealRegister::FPR4);
      setFloatArgumentRegister(3, TR::RealRegister::FPR6);
      setNumFloatArgumentRegisters(4);
      }

   setIntegerReturnRegister (TR::RealRegister::GPR2 );
   setLongLowReturnRegister (TR::RealRegister::GPR3 );
   setLongHighReturnRegister(TR::RealRegister::GPR2 );
   setLongReturnRegister    (TR::RealRegister::GPR2 );
   setFloatReturnRegister   (TR::RealRegister::FPR0 );
   setDoubleReturnRegister  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister0  (TR::RealRegister::FPR0 );
   setLongDoubleReturnRegister2  (TR::RealRegister::FPR2 );
   setLongDoubleReturnRegister4  (TR::RealRegister::FPR4 );
   setLongDoubleReturnRegister6  (TR::RealRegister::FPR6 );
   if (enableVectorLinkage) setVectorReturnRegister  (TR::RealRegister::VRF24 );
   setStackPointerRegister  (TR::RealRegister::GPR15 );
   setNormalStackPointerRegister  (TR::RealRegister::GPR15 );
   setAlternateStackPointerRegister  (TR::RealRegister::GPR11 );
   setEntryPointRegister    (TR::RealRegister::GPR14 );
   setLitPoolRegister       (TR::RealRegister::GPR13 );
   setExtCodeBaseRegister   (TR::RealRegister::GPR7  );
   setReturnAddressRegister (TR::RealRegister::GPR14 );

   setGOTPointerRegister   (TR::RealRegister::GPR12  );

#if defined(OMRZTPF)
/*
 * z/TPF os is a 64 bit target
 */
   setOffsetToRegSaveArea     (16);    // z/TPF is a 64 bit target only
   setGPRSaveAreaBeginOffset  (16);    //x'10'  see ICST_R2 in tpf/icstk.h
   setGPRSaveAreaEndOffset    (160);   //x'a0'  see ICST_PAT in tpf/icstk.h
   setOffsetToFirstParm       (448);   //x'1c0' see ICST_PAR in tpf/icstk.h
#else
   setOffsetToRegSaveArea   ((TR::Compiler->target.is64Bit()) ? 16 : 8);
   setGPRSaveAreaBeginOffset  ((TR::Compiler->target.is64Bit()) ? 48 : 24);
   setGPRSaveAreaEndOffset  ((TR::Compiler->target.is64Bit()) ? 128 : 64 );
   setOffsetToFirstParm   ((TR::Compiler->target.is64Bit()) ? 160 : 96);
#endif /* defined(OMRZTPF) */
   setOffsetToFirstLocal  (0);
   setOffsetToLongDispSlot(TR::Compiler->target.is64Bit() ? 8 : 4);
   setOutgoingParmAreaBeginOffset(getOffsetToFirstParm());
   setOutgoingParmAreaEndOffset(0);
   setStackFrameSize(0);
   setVarArgOffsetInParmArea(getOutgoingParmAreaBeginOffset());
   setVarArgRegSaveAreaOffset(0);

   setNumberOfDependencyGPRegisters(32);
   setLargestOutgoingArgumentAreaSize(0);
   }

/**
 * Front-end customization of callNativeFunction
 */
void
TR::S390SystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   TR_ASSERT(0,"Different system types should have their own implementation.");
   }

/**
 * Call System routine
 * @return return value will be return value from system routine copied to private linkage return reg
 */
TR::Register *
TR::S390SystemLinkage::callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   return 0;
   }

/**
 * Front-end customization of callNativeFunction
 */
void
TR::S390zOSSystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
 #if defined(PYTHON) || defined(OMR_RUBY) || defined(JITTEST)
   TR_ASSERT(0,"Python/Ruby/Test should have their own front-end customization.");
 #endif

   }

/**
 * TR::S390zOSSystemLinkage::callNativeFunction - call System routine
 * return value will be return value from system routine copied to private linkage return reg
 */
TR::Register *
TR::S390zOSSystemLinkage::callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {

   /*****************************/
   /***Front-end customization***/
   /*****************************/
   generateInstructionsForCall(callNode, deps, targetAddress, methodAddressReg,
         javaLitOffsetReg, returnFromJNICallLabel, jniCallDataSnippet, isJNIGCPoint);

   TR::CodeGenerator * codeGen = cg();

   TR::Register * retReg = NULL;
   TR::Register * returnRegister = NULL;
   // set dependency on return register
   TR::Register * lowReg = NULL, * highReg = NULL;

   TR::Register * Real_highReg = NULL, * Real_lowReg = NULL, * Img_highReg = NULL, * Img_lowReg = NULL, * Real = NULL, * Imaginary = NULL;
   switch (callNode->getOpCodeValue())
      {
      case TR::acall:
      case TR::acalli:
         retReg = deps->searchPostConditionRegister(getIntegerReturnRegister());
         returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
         if(TR::Compiler->target.is64Bit() && returnRegister && !returnRegister->is64BitReg())
            {
            returnRegister->setIs64BitReg(true);    //in 64bit target, force return 64bit address register,
            }                                           //until it's known that dispatch functions can return non-64bit addresses.
         break;
      case TR::icall:
      case TR::iucall:
      case TR::icalli:
      case TR::iucalli:
         retReg = deps->searchPostConditionRegister(getIntegerReturnRegister());
         returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcalli:
      case TR::lucalli:
      case TR::lcall:
      case TR::lucall:
         {
         if (codeGen->use64BitRegsOn32Bit())
            {
            TR::Instruction *cursor = NULL;
            lowReg = deps->searchPostConditionRegister(getLongLowReturnRegister());
            highReg = deps->searchPostConditionRegister(getLongHighReturnRegister());

            generateRSInstruction(codeGen, TR::InstOpCode::SLLG, callNode, highReg, highReg, 32);
            cursor =
               generateRRInstruction(codeGen, TR::InstOpCode::LR, callNode, highReg, lowReg);

            codeGen->stopUsingRegister(lowReg);
            retReg = highReg;
            returnRegister = retReg;
            }
         else if (TR::Compiler->target.is64Bit())
            {
            retReg = deps->searchPostConditionRegister(getIntegerReturnRegister());
            returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
            }
         else
            {
            lowReg = deps->searchPostConditionRegister(getLongLowReturnRegister());
            highReg = deps->searchPostConditionRegister(getLongHighReturnRegister());

            retReg = codeGen->allocateConsecutiveRegisterPair(lowReg, highReg);
            returnRegister = retReg;
            }
         }
         break;
      case TR::fcalli:
      case TR::dcalli:
      case TR::fcall:
      case TR::dcall:
#if defined(SUPPORT_DFP) && defined(J9_PROJECT_SPECIFIC)
      case TR::dfcalli:
      case TR::ddcalli:
      case TR::dfcall:
      case TR::ddcall:
#endif
         retReg = deps->searchPostConditionRegister(getFloatReturnRegister());
         returnRegister = retReg;
         break;
#if defined(SUPPORT_DFP) && defined(J9_PROJECT_SPECIFIC)
      case TR::decall:
      case TR::decalli:
         highReg = deps->searchPostConditionRegister(getLongDoubleReturnRegister0());
         lowReg = deps->searchPostConditionRegister(getLongDoubleReturnRegister2());
         retReg = codeGen->allocateFPRegisterPair(lowReg, highReg);
         returnRegister = retReg;
         break;
#endif
      case TR::calli:
      case TR::call:
         retReg = NULL;
         returnRegister = retReg;
         break;
      default:
         retReg = NULL;
         returnRegister = retReg;
         TR_ASSERT(0, "Unknown direct call Opcode %d.", callNode->getOpCodeValue());
      }


   if (returnRegister != retReg)
      {
      generateRRInstruction(codeGen, TR::InstOpCode::getLoadRegOpCode(), callNode, returnRegister, retReg);
      }

   return returnRegister;
   }

/**
 * Front-end customization of callNativeFunction
 */
void
TR::S390zLinuxSystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   TR::CodeGenerator * codeGen = cg();

   TR::RegisterDependencyConditions * postDeps = new (trHeapMemory())
      TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), cg());

   TR::Register * systemReturnAddressRegister =
      deps->searchPostConditionRegister(getReturnAddressRegister());

   //omr todo: this should be placed in Test/Python/Ruby
   //zLinux SystemLinkage
#if defined(PYTHON) || defined(OMR_RUBY) || defined(JITTEST)
      {

      if (callNode->getOpCode().isIndirect())
         {
         TR_ASSERT(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isComputed(), "system linkage only supports computed indirect call for now %p\n", callNode);
         // get the address of the function descriptor
         TR::Register *targetReg = codeGen->evaluate(callNode->getFirstChild());
         generateRRInstruction(codeGen, TR::InstOpCode::BASR, callNode, systemReturnAddressRegister, targetReg, deps);
         }
      else
         {
         TR::SymbolReference *callSymRef = callNode->getSymbolReference();
         TR::Symbol *callSymbol = callSymRef->getSymbol();
         TR::Register * fpReg = systemReturnAddressRegister;
         TR::Instruction * callInstr = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, fpReg, callSymbol, callSymRef, codeGen);
         callInstr->setDependencyConditions(deps);
         }
      }
#endif

   }

/**
 * Call System routine
 * @return return value will be return reg
 */
TR::Register *
TR::S390zLinuxSystemLinkage::callNativeFunction(TR::Node * callNode,
   TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
   TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
   TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   /********************************/
   /***Generate call instructions***/
   /********************************/
   generateInstructionsForCall(callNode, deps, targetAddress, methodAddressReg,
         javaLitOffsetReg, returnFromJNICallLabel, jniCallDataSnippet,isJNIGCPoint);

   TR::Register * returnRegister = NULL;
   // set dependency on return register
   TR::Register * lowReg = NULL, * highReg = NULL;
   TR::Register * Real_highReg = NULL, * Real_lowReg = NULL, * Img_highReg = NULL, * Img_lowReg = NULL, * Real = NULL, * Imaginary = NULL;
   switch (callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::icalli:
      case TR::iucall:
      case TR::iucalli:
      case TR::acall:
      case TR::acalli:
         returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::lcalli:
      case TR::lucall:
      case TR::lucalli:
         {
         if (cg()->use64BitRegsOn32Bit())
            {
            TR::Instruction *cursor = NULL;
            lowReg = deps->searchPostConditionRegister(getLongLowReturnRegister());
            highReg = deps->searchPostConditionRegister(getLongHighReturnRegister());

            generateRSInstruction(cg(), TR::InstOpCode::SLLG, callNode, highReg, highReg, 32);
            cursor =
               generateRRInstruction(cg(), TR::InstOpCode::LR, callNode, highReg, lowReg);

/*
            TR::RegisterDependencyConditions * deps =
               new (cg()->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg());
            deps->addPostCondition(lowReg, getLongLowReturnRegister());
            deps->addPostCondition(highReg, getLongHighReturnRegister());
            cursor->setDependencyConditions(deps);
*/

            cg()->stopUsingRegister(lowReg);
            returnRegister = highReg;
            }
         else if (TR::Compiler->target.is64Bit())
            {
            returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
            }
         else
            {
            lowReg = deps->searchPostConditionRegister(getLongLowReturnRegister());
            highReg = deps->searchPostConditionRegister(getLongHighReturnRegister());
            returnRegister = cg()->allocateConsecutiveRegisterPair(lowReg, highReg);
            }
         }
         break;
      case TR::fcall:
      case TR::fcalli:
      case TR::dcall:
      case TR::dcalli:
#if defined(SUPPORT_DFP) && defined(J9_PROJECT_SPECIFIC)
      case TR::dfcall:
      case TR::dfcalli:
      case TR::ddcall:
      case TR::ddcalli:
#endif
         returnRegister = deps->searchPostConditionRegister(getFloatReturnRegister());
         break;
#if defined(SUPPORT_DFP) && defined(J9_PROJECT_SPECIFIC)
      case TR::decall:
      case TR::decalli:
         highReg = deps->searchPostConditionRegister(getLongDoubleReturnRegister0());
         lowReg = deps->searchPostConditionRegister(getLongDoubleReturnRegister2());
         returnRegister = cg()->allocateFPRegisterPair(lowReg, highReg);
         break;
#endif
      case TR::call:
      case TR::calli:
         returnRegister = NULL;
         break;
      case TR::vcall:
      case TR::vcalli:
         returnRegister = deps->searchPostConditionRegister(getVectorReturnRegister());
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call Opcode %d.", callNode->getOpCodeValue());
      }

   return returnRegister;
   }

/**
 * @todo (cleanup) should remove this method and use base class version.
 */
void
TR::S390zLinuxSystemLinkage::initParamOffset(TR::ResolvedMethodSymbol * method, int32_t stackIndex, List<TR::ParameterSymbol> *parameterList)
{
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   parameterIterator.reset();
   TR::ParameterSymbol * parmCursor = parameterIterator.getFirst();
   int32_t numIntegerArgs=0, numFloatArgs=0, numVectorArgs=0;
   int32_t indexInArgRegistersArray = 0;
   TR::RealRegister::RegNum argRegNum = TR::RealRegister::NoReg;
   int8_t gprSize = cg()->machine()->getGPRSize();
   int32_t parmInLocalAreaOffset = getParmOffsetInLocalArea();

   setIncomingParmAreaBeginOffset(stackIndex);
   while (parmCursor != NULL)
      {
      int32_t previousIndex=parmCursor->getLinkageRegisterIndex();
      parmCursor->setLinkageRegisterIndex(-1);
      TR::DataType originalDataType=parmCursor->getDataType();

      if ( parmCursor->getDataType()==TR::Aggregate)
         {
         TR_ASSERT(0, "should not reach here due to folding of getDataTypeOfAggregateMember()");
         TR::DataType dt = TR::NoType;
         if(dt != TR::NoType) parmCursor->setDataType(dt);
         else if (parmCursor->getSize() > 8) parmCursor->setDataType(TR::Address);
         else if (parmCursor->getSize() == 8) parmCursor->setDataType(TR::Int64);
         else if (parmCursor->getSize() == 4) parmCursor->setDataType(TR::Int32);
         else if (parmCursor->getSize() == 2) parmCursor->setDataType(TR::Int16);
         else if (parmCursor->getSize() == 1) parmCursor->setDataType(TR::Int8);
         else parmCursor->setDataType(TR::Address);
         }

      switch (parmCursor->getDataType())
         {
         case TR::Int64:
            if(TR::Compiler->target.is32Bit())
               {
               //make sure that we can fit the entire value in registers, not half in registers half on the stack
               if((numIntegerArgs + 2) <= getNumIntegerArgumentRegisters())
                  {
                  indexInArgRegistersArray = numIntegerArgs;
                  argRegNum = getIntegerArgumentRegister(indexInArgRegistersArray);
                  }
               else
                  argRegNum = TR::RealRegister::NoReg;
               numIntegerArgs += 2;
               break;
               }
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            indexInArgRegistersArray = numIntegerArgs;
            argRegNum = getIntegerArgumentRegister(indexInArgRegistersArray);
            numIntegerArgs ++;
            break;
         case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalFloat:
#endif
            indexInArgRegistersArray = numFloatArgs;
            argRegNum = getFloatArgumentRegister(indexInArgRegistersArray);
            numFloatArgs ++;
               if (isSkipGPRsForFloatParms())
                  {
                  if (numIntegerArgs < getNumIntegerArgumentRegisters())
                     {
                     numIntegerArgs ++;
                     }
                  }
             break;
         case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalDouble:
#endif
            indexInArgRegistersArray = numFloatArgs;
            argRegNum = getFloatArgumentRegister(indexInArgRegistersArray);
            numFloatArgs ++;
               if (isSkipGPRsForFloatParms())
                  {
                  if (numIntegerArgs < getNumIntegerArgumentRegisters())
                     {
                     numIntegerArgs += (TR::Compiler->target.is64Bit()) ? 1 : 2;
                     }
                  }
            break;
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
         case TR::VectorDouble:
            indexInArgRegistersArray = numVectorArgs;
            argRegNum = getVectorArgumentRegister(indexInArgRegistersArray);
            numVectorArgs ++;
            break;
         default: break;
         }
      if (argRegNum == TR::RealRegister::NoReg)
         {
         if ((isPadFloatParms() &&  parmCursor->getType().isFloatingPoint()) && (gprSize > parmCursor->getSize()))
            parmCursor->setParameterOffset(stackIndex + gprSize - parmCursor->getSize());
          else
            parmCursor->setParameterOffset(stackIndex);
         stackIndex += std::max<int32_t>(parmCursor->getSize(),cg()->machine()->getGPRSize());
         }
      else
         {
         // Check where to locate the linkage register 6 which is preserved on zLiux
          if ((getIntegerArgumentAddToPost(argRegNum) !=0 && parmCursor->isParmHasToBeOnStack()) ||
              parmCursor->getType().isVector())

            {
            TR_ASSERT( getParmOffsetInLocalArea() >=0, "ParmOffsetInLocalArea is negative");

            // on local save area
            parmCursor->setParameterOffset(parmInLocalAreaOffset);

            if (parmCursor->getType().isVector())
               parmInLocalAreaOffset += cg()->machine()->getVRFSize();
            else
               parmInLocalAreaOffset += cg()->machine()->getGPRSize();
            }
         else
            parmCursor->setParameterOffset(getStackFrameSize() + getRegisterSaveOffset(REGNUM(argRegNum)));

         parmCursor->setLinkageRegisterIndex(indexInArgRegistersArray);
         }

      if ( isSmallIntParmsAlignedRight() && parmCursor->getType().isIntegral() && (gprSize > parmCursor->getSize())) // WCode motivated
         {
         // linkage related
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + gprSize - parmCursor->getSize());
         }

      if (TR::Compiler->target.is64Bit() && parmCursor->getType().isAddress() && (parmCursor->getSize()==4))
         {
         // This is a 31-bit pointer parameter. It's real location is +4 bytes from the start of the
         // 8-byte parm slot. Since ptr31 parms are passed as 64-bit values, the prologue code has the
         // spill the full 8-bytes into the parm slot. However, the rest of the code will load/store
         // this parm as a 4-byte value, hence we have to add +4 here.
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + 4);
         }

      parmCursor = parameterIterator.getNext();
      }

   setVarArgOffsetInParmArea(stackIndex);
   setIncomingParmAreaEndOffset(stackIndex);
   setNumUsedArgumentGPRs(numIntegerArgs);
   setNumUsedArgumentFPRs(numFloatArgs);
   setNumUsedArgumentVFRs(numVectorArgs);
}

#if 0
void
TR::S390zLinuxSystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex)
   {
   size_t align = 1;
   size_t size = p->getSize();

   if ((size & (size - 1)) == 0  && size <= 8) // if size is power of 2 and small
      {
      align = size;
      }
   else if (size > 8)
      {
      align = 8;
      }

   align = (align == 0) ? 1 : align;
   if (true)
      {
      // we map in a backwards fashion
      //TODO: not sure what to do here
      intptr_t roundup = align - 1;
      stackIndex = (stackIndex-size) & (~roundup);
      p->setOffset(stackIndex);
      }
   else
      {
      // we map in a backwards fashion
      intptr_t roundup = align - 1;
      stackIndex = (stackIndex - size) & (~roundup);
      p->setOffset(stackIndex);
      // we have some stacks that are getting very close
      TR_ASSERT( (-stackIndex + size) >= (-stackIndex), "Stack index growing too big");
      }

   setStrictestAutoSymbolAlignment(align); // API affected if (align > current strictest)
   }
#endif

void
TR::S390SystemLinkage::mapStack(TR::ResolvedMethodSymbol * method)
  	{
		{
     	mapStack(method,0);
  		}
  	}

/**
 * This function will intitialize all the offsets from frame pointer
 * i.e. stackPointer value before buying stack for method being jitted
 * The offsets will be fixed up (at the end /in createPrologue) when the stackFrameSize is known
 */
void
TR::S390SystemLinkage::mapStack(TR::ResolvedMethodSymbol * method, uint32_t stackIndex)
   {
   int32_t i;

// ====== COMPLEXITY ALERT:  (TODO: fix this nonsense: map stack positively)
// This method is called twice - once with a 0 value of stackIndex and secondly with the stack size.
// This adds complexities:
//     -  alignment: ensuring things are aligned properly
//     -  item layout is done in a reverse order : those things mapped first here are at higher addresses
//        end offsets are really begin offsets and vice versa.
// ====== COMPLEXITY ALERT

   setStrictestAutoSymbolAlignment(TR::Compiler->target.is64Bit() ? 8 : 8, true); // initial setting

   setStackSizeCheckNeeded(true);


   // process incomming parameters
   if (!isParmBlockRegister() && !isZLinuxLinkageType()) // ! OS (Type 1 linkage) && ! zLinux Linkage
       {
       initParamOffset(method,getOutgoingParmAreaBeginOffset()+ stackIndex);
       }

   // Now map the locals
   //
   setLocalsAreaBeginOffset(stackIndex);
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol * localCursor = automaticIterator.getFirst();
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (!TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   // align double - but there is more to do to align the stack in general as double.
   while (localCursor != NULL)
      {
      if (TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }


   // Force the stack size to be increased by...
   stackIndex -= (comp()->getOptions()->get390StackBufferSize()/4)*4;
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   if (true)
      {
      //Save used Preserved FPRs
      setFPRSaveAreaBeginOffset(stackIndex);
      int16_t FPRSaveMask = 0;
      for (i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; ++i)
         {
         if (getPreserved(REGNUM(i)) && ((getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod()))
            {
            FPRSaveMask |= 1 << (i - TR::RealRegister::FirstFPR);
            stackIndex -= cg()->machine()->getFPRSize();
            }
         }

      setFPRSaveMask(FPRSaveMask);

      if (FPRSaveMask != 0 && isOSLinkageType())
         {
         // PPA1 offset in multiple of 16 bytes - so align
         setStrictestAutoSymbolAlignment(16);
         #define DELTA_ALIGN(x, align) ((x & (align-1)) ? (align -((x)&(align-1))) : 0)
         stackIndex -= DELTA_ALIGN(stackIndex, 16);
         }
      setFPRSaveAreaEndOffset(stackIndex);

      //Save used Preserved ARs
      if(!TR::Compiler->target.isLinux() ) //zLinux linkage doesn't deal with ARs
         {
         setARSaveAreaBeginOffset(stackIndex);
         int16_t ARSaveMask = 0;
         for (i = TR::RealRegister::FirstAR; i <= TR::RealRegister::LastAR; ++i)
           {
           if (getPreserved(REGNUM(i)) && (getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
              {
              ARSaveMask |= 1 << (i - TR::RealRegister::FirstAR);
              stackIndex -= cg()->machine()->getGPRSize();
              }
           }
         setARSaveMask(ARSaveMask);
         setARSaveAreaEndOffset(stackIndex);
         }

      }

   //  Map slot for Long Disp
   if ( !comp()->getOption(TR_DisableLongDispStackSlot) )
      {
      if(TR::Compiler->target.isLinux())
         {
         //zLinux has a special reserved slot in the linkage convention
         setOffsetToLongDispSlot(TR::Compiler->target.is64Bit() ? 8 : 4);
         }
      else
         {
         //setOffsetToLongDispSlot(stackIndex -= cg()->sizeOfJavaPointer());
         setOffsetToLongDispSlot(stackIndex -= 16);
         }
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "\n\nOffsetToLongDispSlot = %d\n", getOffsetToLongDispSlot());
      }
   else
      {
      setOffsetToLongDispSlot(0);
      }

   if (isParmBlockRegister()) // OS Type 1 linkage
       {
       // map areas for storing registers in lower stack addresses
       initParamOffset(method,stackIndex);
       stackIndex -= (getIncomingParmAreaBeginOffset()-getIncomingParmAreaEndOffset());
       }
   else if (isZLinuxLinkageType())
       //initParamOffset(method, stackIndex);
       initParamOffset(method,getOutgoingParmAreaBeginOffset()+ stackIndex);

   method->setLocalMappingCursor(stackIndex);

   stackIndex -= (stackIndex & 0x4) ? 4 : 0;
   setLocalsAreaEndOffset(stackIndex);
   }

void TR::S390SystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex)
   {
   size_t align = 1;
   size_t size = p->getSize();

   if ((size & (size - 1)) == 0  && size <= 8) // if size is power of 2 and small
      {
      align = size;
      }
   else if (size > 8)
      {
      align = 8;
      }

   align = (align == 0) ? 1 : align;

   // we map in a backwards fashion
   int32_t roundup = align - 1;
   stackIndex = (stackIndex-size) & (~roundup);
   p->setOffset(stackIndex);

   setStrictestAutoSymbolAlignment(align); // API affected if (align > current strictest)
   }

bool TR::S390SystemLinkage::hasToBeOnStack(TR::ParameterSymbol * parm)
   {
   return parm->getAllocatedIndex() >=  0 &&  parm->isParmHasToBeOnStack();
   }


TR::Instruction*
TR::S390SystemLinkage::saveGPRsInPrologue2(TR::Instruction * cursor)
   {
   int16_t GPRSaveMask = 0;
   int32_t offset = 0;
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::Node * firstNode = comp()->getStartTree()->getNode();

   TR::RealRegister::RegNum lastReg, firstReg;
   int32_t i;
   bool findStartReg = true;

   for( i = lastReg = firstReg = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR-2; ++i )      //special case for r14, the return value
      {
      traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);
      if(getPreserved(REGNUM(i)))
         {
         traceMsg(comp(), "\tIt is Preserved\n");

         if (findStartReg)
            {
            traceMsg(comp(), "\tSetting firstReg to %d\n",(i- TR::RealRegister::FirstGPR));
            firstReg = static_cast<TR::RealRegister::RegNum>(i);
            }

         if ((getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            traceMsg(comp(), "\t It is Assigned. Putting in to GPRSaveMask\n");
            GPRSaveMask |= 1 << (i - TR::RealRegister::FirstGPR);
            findStartReg = false;
            lastReg = static_cast<TR::RealRegister::RegNum>(i);
            }

         if ((getS390RealRegister(REGNUM(i)))->getModified())
            {
            traceMsg(comp(), "\tIt is also Modified\n");
            }
         traceMsg(comp(), "\n");
         }
      }

   //Original Strategy --- use 1 store multiple operation that goes from 1st to last preserved reg.  We will preserve regs in between even if they aren't assigned.

   traceMsg(comp(), "\tlastReg is %d .  firstReg = %d \n",lastReg,firstReg);
   offset =  getRegisterSaveOffset(REGNUM(firstReg));
   traceMsg(comp(),"\tstackFrameSize = %d offset = %d\n",getStackFrameSize(),offset);

   TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());

   if (lastReg - firstReg == 0)
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getS390RealRegister(REGNUM(firstReg )), retAddrMemRef, cursor);
   else if (lastReg > firstReg)
      cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(), firstNode, getS390RealRegister(REGNUM(firstReg)),  getS390RealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


   //r14 is a special case.  It is the return value register.  Since we will have to save/restore it often, we will try to reduce the range above by just emiting its own save/restore
   // should also experiment with changing preferred ordering in pickregister.
   traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);

   if(getPreserved(REGNUM(i)) && (getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
      {
      traceMsg(comp(), "\t It is Assigned. Putting in to GPRSaveMask\n");
      GPRSaveMask |= 1 << (i - TR::RealRegister::FirstGPR);
      offset =  getRegisterSaveOffset(REGNUM(i));
      TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getS390RealRegister(REGNUM(i)), retAddrMemRef, cursor);
      }

   //r15 (stack pointer) might need to be put in to the register mask as well.  Other code will deal with how we save/restore it.

   ++i;
   traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);
   if(getPreserved(REGNUM(i)) && (getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
      {
      traceMsg(comp(), "\t It is Assigned. Putting in to GPRSaveMask\n");
      GPRSaveMask |= 1 << (i - TR::RealRegister::FirstGPR);
      }

   setGPRSaveMask(GPRSaveMask);
   return cursor;
   }

TR::Instruction*
TR::S390SystemLinkage::saveGPRsInPrologue(TR::Instruction * cursor)
   {
   int16_t GPRSaveMask = 0;
   bool hasSavedSP = false;
   int32_t offset = 0;
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::Node * firstNode = comp()->getStartTree()->getNode();

   TR::RealRegister::RegNum lastReg, firstReg;
   int32_t i;
   bool findStartReg = true;

   for( i = lastReg = firstReg = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i )
      {
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);
      if(getPreserved(REGNUM(i)))
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\tIt is Preserved\n");

         if (findStartReg)
            {
            //traceMsg(comp(), "\tSetting firstReg to %d\n",(i- TR::RealRegister::FirstGPR));
            firstReg = static_cast<TR::RealRegister::RegNum>(i);
            }

         if ((getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "\t It is Assigned. Putting in to GPRSaveMask\n");
            GPRSaveMask |= 1 << (i - TR::RealRegister::FirstGPR);
            findStartReg = false;
            }

         if (!findStartReg && ( !(getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod() || (i == TR::RealRegister::LastGPR && firstReg != TR::RealRegister::LastGPR)) )
            {
            // LastGPR is the stack pointer on zLinux which always be saved and restored
            if (i == TR::RealRegister::LastGPR && !getIsLeafRoutine())
               {
               lastReg = static_cast<TR::RealRegister::RegNum>(i);
               hasSavedSP = true;
               }
            else
               lastReg = static_cast<TR::RealRegister::RegNum>(i-1);

            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "\tGenerating preserve stores from %d  to %d \n",(lastReg - TR::RealRegister::FirstGPR),firstReg - TR::RealRegister::FirstGPR);

            offset =  getRegisterSaveOffset(REGNUM(firstReg));
            //traceMsg(comp(),"\tstackFrameSize = %d offset = %d\n",getStackFrameSize(),offset);

            TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());

            if (lastReg - firstReg == 0)
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getS390RealRegister(REGNUM(firstReg)), retAddrMemRef, cursor);
            else
               cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(), firstNode, getS390RealRegister(REGNUM(firstReg)),  getS390RealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


            findStartReg = true;
            }
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\n");
         }
      }


   // LastGPR is the stack pointer on zLinux which always be saved and restored
   if (!hasSavedSP && !getIsLeafRoutine())
      {
       lastReg = static_cast<TR::RealRegister::RegNum>(TR::RealRegister::LastGPR);
       offset =  getRegisterSaveOffset(REGNUM(lastReg));
       TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
       cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), firstNode, getS390RealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);
      }

   setGPRSaveMask(GPRSaveMask);
   return cursor;
   }

TR::Linkage::FrameType
TR::S390zLinuxSystemLinkage::checkLeafRoutine(int32_t stackFrameSize, TR::Instruction **callInstruction)
   {
   FrameType ft = standardFrame;


   if(comp()->getOption(TR_DisableLeafRoutineDetection))
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(),"Leaf Routine Detection Disabled\n");
      return ft;
      }

   if(isForceSaveIncomingParameters())
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(),"Force Save Incoming Parameters enabled\n");
      return ft;
      }

   int32_t zeroArgsAndLocalsStackFrameSize = (((getOffsetToFirstParm() + 7)>>3)<<3);

   if(comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "stackFrameSize = %d  default 0 arg and 0 local stack frame size = %d\n",stackFrameSize,zeroArgsAndLocalsStackFrameSize);

   // a) frame size < 256 and b) no alloca()calls
   if (stackFrameSize != zeroArgsAndLocalsStackFrameSize)
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "LeafRoutine: stackFrameSize != zeroArgsAndLocalStackFrameSize\n");
      return ft;
      }

   // b) no alloca
   if (getNotifiedOfAlloca())
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "LeafRoutine: Alloca's present\n");
      return ft;
      }

   // c) not var arg
   TR::MethodSymbol *methodSymbol = comp()->getMethodSymbol();


   //e) ensure stack pointer isn't used.
   TR::RealRegister * spReg = getStackPointerRealRegister();

   if(spReg->isUsedInMemRef() || getARSaveMask() || getHPRSaveMask())       //don't need to check gprmask or hpr mask as these are saved in caller stack.
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "LeafRoutine: stack pointer isUsedInMemRef = %d  or arsavemask = %d  or hprsavemask = %d is set to 1\n",spReg->isUsedInMemRef(),getARSaveMask(),getHPRSaveMask());

      return ft;
      }


   // g) no calls


   // For no calls we check if method contains possible branch instruction
   // and for small sized methods - so some leafs may skip by
   //

   #define ZLINUX_SYSTEMLINK_LEAF_INSTRUCTION_CHECK_THRESHOLD 150
   TR::Instruction *cursor = comp()->cg()->getFirstInstruction();
   int32_t numToCheck = ZLINUX_SYSTEMLINK_LEAF_INSTRUCTION_CHECK_THRESHOLD;
   bool regs[TR::RealRegister::NumRegisters] = { };



   bool hasCalls = findPossibleCallInstruction(cursor, numToCheck,callInstruction,regs,TR::RealRegister::NumRegisters);


   if (!hasCalls && cursor == NULL)
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "callInstruction = %p cursor = %p\n",*callInstruction,cursor);

      if (callInstruction && *callInstruction && checkPreservedRegisterUsage(regs,TR::RealRegister::NumRegisters))
         {
         ft = noStackForwardingFrame;
         }
      else if (callInstruction && *callInstruction) // register usage doesn't allow a framewithout a stack.
         {
         ft = standardFrame;
         }
      else if (checkPreservedRegisterUsage(regs,TR::RealRegister::NumRegisters))
         {
         ft = noStackLeafFrame;
         }
      else
         {
         ft = StackLeafFrame;
         }
      }

   return ft;
   }
//returns true if there are no preserved registers used in the method
bool OMR::Z::Linkage::checkPreservedRegisterUsage(bool *regs, int32_t regsSize)
   {
   int32_t i;
   bool retval = true;

   if(self()->comp()->getOption(TR_TraceCG))
      traceMsg(self()->comp(), "LeafRoutine: Checking Register Usage.  Preserved Registers that are assigned:");

   for( i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR-2; ++i )      //special case for r14, the return value
      {
      //TR_ASSERT(regs[i] == (getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod(), "Mismatch between register assigned information!!\n");

      //traceMsg(comp(), "regs[%d] = %d getHasBeenAssignedInMethod = %d\n",i,regs[i],(getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod());

      if (self()->getPreserved(REGNUM(i)) && (self()->getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
         {
         if(self()->comp()->getOption(TR_TraceCG))
            traceMsg(self()->comp(), "\t%d",i-TR::RealRegister::FirstGPR);

         retval = false;
         //return false;        // fast path, using retval for debugging purposes
         }
      }

   //check r14 usage -- don't check if it has been assigned because it could be used in a call. the regs array passed in will take this in to account.
   if(regs[TR::RealRegister::GPR14])
      {
      if(self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(), "\t%d (r14)",TR::RealRegister::GPR14-TR::RealRegister::FirstGPR);

      retval = false;
      // return false;
      }

   if(self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(), "\n");

   return retval;
   }


////////////////////////////////////////////////////////////////////////////////
// TR::390SystemLinkage::createPrologue() - create prolog for System linkage
////////////////////////////////////////////////////////////////////////////////
void TR::S390SystemLinkage::createPrologue(TR::Instruction * cursor)
   {
   TR::Delimiter delimiter (comp(), comp()->getOption(TR_TraceCG), "Prologue");
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::RealRegister * lpReg = getLitPoolRealRegister();
   TR::Node * firstNode = comp()->getStartTree()->getNode();
   int32_t i=0;
   int32_t firstLocalOffset = getOffsetToFirstLocal();
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   int32_t argSize = getOutgoingParameterBlockSize();
   TR::Snippet * firstSnippet = NULL;

   int16_t FPRSaveMask = 0;
   int16_t ARSaveMask = 0;

   int32_t localSize; // Auto + Spill size

   localSize =  -1 * (int32_t) (bodySymbol->getLocalMappingCursor());

   setOutgoingParmAreaEndOffset(getOutgoingParmAreaBeginOffset() + argSize);

   setStackFrameSize(((getOffsetToFirstParm() +  argSize + localSize + 7)>>3)<<3);
   int32_t stackFrameSize = getStackFrameSize();

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "initial stackFrameSize: stack size %d = offset to first parm %d + arg size %d + localSize %d, rounded up to 8\n", stackFrameSize, getOffsetToFirstParm(), argSize, localSize);

   TR_ASSERT( ((int32_t) stackFrameSize % 8 == 0), "misaligned stack detected");

   setVarArgRegSaveAreaOffset(getStackFrameSize());

   //  We assume frame size is less than 32k
   //TR_ASSERT(stackFrameSize<=MAX_IMMEDIATE_VAL,
   //      "TR::S390SystemLinkage::createPrologue -- Frame size (0x%x) greater than 0x7FFF\n",stackFrameSize);

   //Now that stackFrame size is known, map stack
   mapStack(bodySymbol, stackFrameSize);

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "final stackFrameSize: stack size %d\n", stackFrameSize);

   setFirstPrologueInstruction(cursor);


   //check if leaf method

   TR::Instruction *callInstruction = 0;
   FrameType ft = checkLeafRoutine(stackFrameSize, &callInstruction);

   if ( ( ft == noStackLeafFrame || ft == StackLeafFrame || ft == noStackForwardingFrame ) &&
         performTransformation(comp(), "O^O Generating method as leaf of type %d\n",ft))
      {
      setFrameType(ft);

      if(ft == noStackForwardingFrame)
         replaceCallWithJumpInstruction(callInstruction);
      }



   if(! (getFrameType() == noStackForwardingFrame || getFrameType() == noStackLeafFrame))
      {
      if(comp()->getOption(TR_LinkagePreserveStrategy2))
         cursor = saveGPRsInPrologue2(cursor);
      else
         cursor = saveGPRsInPrologue(cursor);
      }



   int16_t GPRSaveMask = getGPRSaveMask();

#if defined(PYTHON) || defined(OMR_RUBY) || defined(JITTEST)
   // Literal Pool for Ruby and Python and test
   firstSnippet = cg()->getFirstSnippet();
   if ( cg()->isLiteralPoolOnDemandOn() != true && firstSnippet != NULL )
      cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, firstNode, lpReg, firstSnippet, cursor, cg());
#endif


   if(! (getFrameType() == noStackForwardingFrame || getFrameType() == noStackLeafFrame || getFrameType() == StackLeafFrame))
      {
      // Check for large stack
      bool largeStack = (stackFrameSize < MIN_IMMEDIATE_VAL || stackFrameSize > MAX_IMMEDIATE_VAL);

      TR::RealRegister * tempReg = getS390RealRegister(TR::RealRegister::GPR0);
      TR::RealRegister * backChainReg = getS390RealRegister(TR::RealRegister::GPR1);


      if (!largeStack)
         {
         // Adjust stack pointer with LA (reduce AGI delay)
         cursor = generateRXYInstruction(cg(), TR::InstOpCode::LAY, firstNode, spReg, generateS390MemoryReference(spReg,(stackFrameSize) * -1, cg()),cursor);
         }
      else
         {
         // adjust stack frame pointer
         if (largeStack)
            {
            cursor = generateS390ImmToRegister(cg(), firstNode, tempReg, (intptr_t)(stackFrameSize * -1), cursor);
            cursor = generateRRInstruction(cg(), TR::InstOpCode::getAddRegOpCode(), firstNode, spReg, tempReg, cursor);
            }
         else
            {
            cursor = generateRIInstruction(cg(), TR::InstOpCode::getAddHalfWordImmOpCode(), firstNode, spReg, (stackFrameSize) * -1, cursor);
            }
         }
      }
   else
      {
      if(comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "Not emitting stack save because method is leaf Routine\n");
      }

   //Save preserved FPRs
   FPRSaveMask = getFPRSaveMask();
   int32_t firstSaved = TR::Linkage::getFirstMaskedBit(FPRSaveMask);
   int32_t lastSaved = TR::Linkage::getLastMaskedBit(FPRSaveMask);
   int32_t offset = 0;
   TR::MemoryReference * retAddrMemRef ;

   if (TR::Compiler->target.isLinux())
      offset = getFPRSaveAreaEndOffset();
   else
      offset = getFPRSaveAreaBeginOffset() + 16*cg()->machine()->getGPRSize();

   for (i = firstSaved; i <= lastSaved; ++i)
      {
      if (FPRSaveMask & (1 << (i)))
         {
         retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
         offset += cg()->machine()->getFPRSize();
         cursor = generateRXInstruction(cg(), TR::InstOpCode::STD,
               firstNode, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstFPR)), retAddrMemRef, cursor);
         }
      }

   //Save preserved ARs
   ARSaveMask = getARSaveMask();
   firstSaved = TR::Linkage::getFirstMaskedBit(ARSaveMask);
   lastSaved = TR::Linkage::getLastMaskedBit(ARSaveMask);
   offset =  getARSaveAreaBeginOffset();
   retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
   for (i = firstSaved; i <= lastSaved; ++i)
      {
      if (ARSaveMask & (1 << (i)))
         {
         offset += cg()->machine()->getARSize();
         cursor = generateRSInstruction(cg(), TR::InstOpCode::STAM,
               firstNode, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)),
               getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)), retAddrMemRef, cursor);
         }
      }

   if (_notifiedOfalloca)
      {
      cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), firstNode,
         getS390RealRegister(getAlternateStackPointerRegister()), spReg, cursor);
      }

   cursor = (TR::Instruction *) saveArguments(cursor, false);

   setLastPrologueInstruction(cursor);
   }


TR::Instruction*
TR::S390SystemLinkage::restoreGPRsInEpilogue2(TR::Instruction *cursor)
   {
   int16_t GPRSaveMask = 0;
   int32_t offset = 0;
   int32_t stackFrameSize = getIsLeafRoutine() ? 0 : getStackFrameSize();
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::Node * nextNode = cursor->getNext()->getNode();

   TR::RealRegister::RegNum lastReg, firstReg;
   int32_t i;
   bool findStartReg = true;

   for( i = lastReg = firstReg = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR-2; ++i )      //special casing r14 (ret value)
      {
      traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);
      if(getPreserved(REGNUM(i)))
         {
         traceMsg(comp(), "\tIt is Preserved\n");

         if (findStartReg)
            firstReg = static_cast<TR::RealRegister::RegNum>(i);

         if ((getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            traceMsg(comp(), "\tand Assigned. ");
            findStartReg = false;
            lastReg = static_cast<TR::RealRegister::RegNum>(i);
            }
         traceMsg(comp(), "\n");
         }
      }

   //Original Strategy --- use 1 load multiple operation that goes from 1st to last preserved reg.  We will preserve regs in between even if they aren't assigned.

   traceMsg(comp(), "\tlastReg is %d .  firstReg = %d \n",lastReg,firstReg);
   offset =  getRegisterSaveOffset(REGNUM(firstReg));
   traceMsg(comp(),"\tstackFrameSize = %d offset = %d\n",getStackFrameSize(),offset);

   TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset + stackFrameSize, cg());

   if (lastReg - firstReg == 0)
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), nextNode, getS390RealRegister(REGNUM(firstReg)), retAddrMemRef, cursor);
   else if (lastReg > firstReg)
      cursor = generateRSInstruction(cg(),  TR::InstOpCode::getLoadMultipleOpCode(), nextNode, getS390RealRegister(REGNUM(firstReg)),  getS390RealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


   //r14 is a special case.  It is the return value register.  Since we will have to save/restore it often, we will try to reduce the range above by just emiting its own save/restore
   // should also experiment with changing preferred ordering in pickregister.
   traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);

   if(getPreserved(REGNUM(i)) && (getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
      {
      traceMsg(comp(), "\t It is Assigned.\n");
      offset =  getRegisterSaveOffset(REGNUM(i));
      TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset + stackFrameSize, cg());
      cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), nextNode, getS390RealRegister(REGNUM(i)), retAddrMemRef, cursor);
      }

   return cursor;
   }

TR::Instruction*
TR::S390SystemLinkage::restoreGPRsInEpilogue(TR::Instruction *cursor)
   {
   int16_t GPRSaveMask = 0;
   bool    hasRestoreSP = false;
   int32_t offset = 0;
   int32_t stackFrameSize = getIsLeafRoutine() ? 0 : getStackFrameSize();
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::Node * nextNode = cursor->getNext()->getNode();

   TR::RealRegister::RegNum lastReg, firstReg;
   int32_t i;
   bool findStartReg = true;

   int32_t blockNumber = cursor->getNext()->getBlockIndex();

   for( i = lastReg = firstReg = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i )
      {
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "Considering Register %d:\n",i- TR::RealRegister::FirstGPR);
      if(getPreserved(REGNUM(i)))
         {
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\tIt is Preserved\n");

         if (findStartReg)
            firstReg = static_cast<TR::RealRegister::RegNum>(i);

         if ((getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "\tand Assigned. ");
            findStartReg = false;
            }

         if (!findStartReg && ( !(getS390RealRegister(REGNUM(i)))->getHasBeenAssignedInMethod() || (i == TR::RealRegister::LastGPR && firstReg != TR::RealRegister::LastGPR)) )
            {
            // LastGPR is the stack pointer on zLinux which always be saved and restored
            if (i == TR::RealRegister::LastGPR && !getIsLeafRoutine())
               {
               hasRestoreSP = true;
               lastReg = static_cast<TR::RealRegister::RegNum>(i);
               }
            else
               lastReg = static_cast<TR::RealRegister::RegNum>(i-1);

            offset =  getRegisterSaveOffset(REGNUM(firstReg));

            //traceMsg(comp(),"\tstackFrameSize = %d offset = %d\n",stackFrameSize,offset);

            TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset + stackFrameSize, cg());

            if (lastReg - firstReg == 0)
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), nextNode, getS390RealRegister(REGNUM(firstReg )), retAddrMemRef, cursor);
            else
               cursor = generateRSInstruction(cg(),  TR::InstOpCode::getLoadMultipleOpCode(), nextNode, getS390RealRegister(REGNUM(firstReg)),  getS390RealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


            findStartReg = true;
            }

         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\n");
         }
      }

    // LastGPR is the stack pointer on zLinux which always be saved and restored
    if (!hasRestoreSP && !getIsLeafRoutine())
       {
       lastReg = static_cast<TR::RealRegister::RegNum>(TR::RealRegister::LastGPR);
       offset =  getRegisterSaveOffset(REGNUM(lastReg));
       TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset + stackFrameSize, cg());
       cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), nextNode, getS390RealRegister(REGNUM(lastReg )), retAddrMemRef, cursor);
       }
   return cursor;
   }

/**
 * Create epilog for System linkage
 */
void TR::S390SystemLinkage::createEpilogue(TR::Instruction * cursor)
   {
   TR::Delimiter delimiter (comp(), comp()->getOption(TR_TraceCG), "Epilogue");

   TR::RealRegister * spReg = getNormalStackPointerRealRegister();
   TR::Node * currentNode = cursor->getNode();
   TR::Node * nextNode = cursor->getNext()->getNode();
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   int32_t i, offset = 0, firstSaved, lastSaved ;
   int16_t GPRSaveMask = getGPRSaveMask();
   int16_t FPRSaveMask = getFPRSaveMask();
   int16_t ARSaveMask = getARSaveMask();
   int32_t stackFrameSize = getStackFrameSize();

   TR::MemoryReference * retAddrMemRef ;

   TR::LabelSymbol * epilogBeginLabel = generateLabelSymbol(cg());

   cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, nextNode, epilogBeginLabel, cursor);


   if (_notifiedOfalloca)
      {
      cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), nextNode,
         spReg, getS390RealRegister(getAlternateStackPointerRegister()), cursor);
      }

   //Restore preserved FPRs
   firstSaved = TR::Linkage::getFirstMaskedBit(FPRSaveMask);
   lastSaved = TR::Linkage::getLastMaskedBit(FPRSaveMask);
   if (TR::Compiler->target.isLinux())
      offset = getFPRSaveAreaEndOffset();
   else
      offset = getFPRSaveAreaBeginOffset() + 16*cg()->machine()->getGPRSize();

   for (i = firstSaved; i <= lastSaved; ++i)
     {
     if (FPRSaveMask & (1 << (i)))
        {
        retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());

        cursor = generateRXInstruction(cg(), TR::InstOpCode::LD,
           nextNode, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstFPR)), retAddrMemRef, cursor);

        offset +=  cg()->machine()->getFPRSize();
        }
     }

   //Restore preserved ARs
   firstSaved = TR::Linkage::getFirstMaskedBit(ARSaveMask);
   lastSaved = TR::Linkage::getLastMaskedBit(ARSaveMask);
   offset =  getARSaveAreaBeginOffset();
   retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
   for (i = firstSaved; i <= lastSaved; ++i)
     {
     if (ARSaveMask & (1 << (i)))
        {
        cursor = generateRSInstruction(cg(), TR::InstOpCode::LAM,
           nextNode, getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)),
           getS390RealRegister(REGNUM(i+TR::RealRegister::FirstAR)), retAddrMemRef, cursor);
        }
     }

   if(! (getFrameType() == noStackForwardingFrame || getFrameType() == noStackLeafFrame))
      {
      // Check for large stack
      bool largeStack = (stackFrameSize < MIN_IMMEDIATE_VAL || stackFrameSize > MAX_IMMEDIATE_VAL);

      TR::RealRegister * tempReg = getS390RealRegister(TR::RealRegister::GPR0);

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "GPRSaveMask: Register context %x\n", GPRSaveMask&0xffff);



      if(comp()->getOption(TR_LinkagePreserveStrategy2))
         cursor = restoreGPRsInEpilogue2(cursor);
      else
         cursor = restoreGPRsInEpilogue(cursor);
      }

   cursor = generateS390RegInstruction(cg(), TR::InstOpCode::BCR, nextNode,
          getS390RealRegister(REGNUM(TR::RealRegister::GPR14)), cursor);
   ((TR::S390RegInstruction *)cursor)->setBranchCondition(TR::InstOpCode::COND_BCR);
   }

void TR::S390SystemLinkage::notifyHasalloca()
   {
   TR::RealRegister::RegNum alternateSP = getAlternateStackPointerRegister();
   TR::RealRegister * alternateStackReg = getS390RealRegister(getAlternateStackPointerRegister());
   alternateStackReg->setState(TR::RealRegister::Locked);
   alternateStackReg->setAssignedRegister(alternateStackReg);
   alternateStackReg->setHasBeenAssignedInMethod(true);
   if (cg()->supportsHighWordFacility() && !comp()->getOption(TR_DisableHighWordRA) && TR::Compiler->target.is64Bit())
      {
      TR::RealRegister * tempHigh = toRealRegister(alternateStackReg)->getHighWordRegister();
      tempHigh->setState(TR::RealRegister::Locked);
      tempHigh->setAssignedRegister(tempHigh);
      tempHigh->setHasBeenAssignedInMethod(true);
      }
   _notifiedOfalloca = true;
   setStackPointerRegister(getAlternateStackPointerRegister());

    int32_t globalAltStackReg = cg()->machine()->findGlobalRegisterIndex(alternateSP);
    if (globalAltStackReg != -1)
       {
       cg()->machine()->lockGlobalRegister(globalAltStackReg);
       }
   }

int32_t
TR::S390zLinuxSystemLinkage::getRegisterSaveOffset(TR::RealRegister::RegNum srcReg)
   {
   int32_t gpr2Offset = TR::Compiler->target.is64Bit() ? 16 : 8;
   int32_t fpr0Offset = TR::Compiler->target.is64Bit() ? 128 : 64;
   switch(srcReg)
      {
      case TR::RealRegister::FPR0: return fpr0Offset;
      case TR::RealRegister::FPR2: return fpr0Offset + cg()->machine()->getFPRSize();
      case TR::RealRegister::FPR4: return fpr0Offset + 2 * cg()->machine()->getFPRSize();
      case TR::RealRegister::FPR6: return fpr0Offset + 3 * cg()->machine()->getFPRSize();
      case TR::RealRegister::FPR1:
      case TR::RealRegister::FPR3:
      case TR::RealRegister::FPR5:
      case TR::RealRegister::FPR7:
      case TR::RealRegister::GPR0:
      case TR::RealRegister::GPR1:
         TR_ASSERT(false,"ERROR: TR::zLinuxSystemLinkage::getRegisterSaveOffset called for volatile reg: %d",srcReg);
         return -1;
      case TR::RealRegister::GPR2: return gpr2Offset;
      default: return gpr2Offset + (srcReg - TR::RealRegister::GPR2) * cg()->machine()->getGPRSize();
      }
   }

int32_t
TR::S390zOSSystemLinkage::getRegisterSaveOffset(TR::RealRegister::RegNum srcReg)
   {
   int32_t offset;
   if ((srcReg >= TR::RealRegister::GPR4) && (srcReg <= TR::RealRegister::GPR15))
      {
      int32_t offset = 2048 + (srcReg - TR::RealRegister::GPR4) * cg()->machine()->getGPRSize();
      return offset;
      }
   else
      {
      TR_ASSERT(false, "ERROR: TR::S390zOSSystemLinkage::getRegisterSaveOffset called for volatile reg: %d\n",srcReg);
      return -1;
      }
   }

/**
 * Find first instruction, within a window of instructions, that may be a possible call banch instruction
 * Window of instructions is determined by starting instruction and count.
 *
 * Input arguments are updated on return and a boolean result is returned.
 *
 * Usage: Predocminantly used for "leaf" detection.
 *
 * @note
 *  1) Some linkages may have different instructions used for branching to callee
 *     In those cases, the code below should include a linkage check or the
 *     particular linkage should have derived version of this routine.
 *  2) A "true" return does not mean the instruction found is a call, it may be a call.
 *     If necessary, The caller of this routine can do further verication base on
 *     the returned instruction.
 */
void
OMR::Z::Linkage::setUsedRegisters(TR::Instruction *instruction, bool *regs, int32_t regsSize)
   {
   if(!regs || regsSize == 0)
      return;

   TR::list<TR::Register *> usedRegs(getTypedAllocator<TR::Register*>(self()->comp()->allocator()));
   if(instruction->getRegisters(usedRegs))
      {
      for(auto cursor = usedRegs.begin(); cursor != usedRegs.end(); ++cursor)
         {
         if ((*cursor) && (*cursor)->getRealRegister())
            {
            TR::RealRegister::RegNum regNum = toRealRegister((*cursor))->getRegisterNumber();

            TR_ASSERT(regNum < regsSize, "Trying to set regNum %d to true when max regsSize = %d\n",regNum,regsSize);

            if(self()->comp()->getOption(TR_TraceCG))
               traceMsg(self()->comp(), "Instruction %p uses reg %d\n",instruction,regNum - TR::RealRegister::FirstGPR );
            regs[regNum] = true;
            }
         }
      }


   }
/**
 * @param cursor Both an input and output parameter
 *        On Input: Instruction to start at
 *        On Output: Instruction found or instruction just outside window (or NULL if no more -- this is the successful indicator that this may be a leaf routine)
 * @param numToCheck Both an input and output parameter
 *        On Input: Max number of instructions to check
 *        On Output: Number of instructions checked
 * @param regs Calculate which registers are used
 * @param regsSize Size of regs array
 * @return Either true for no call found or a single call that can be changed to a jump
 */
bool
OMR::Z::Linkage::findPossibleCallInstruction(
           TR::Instruction * &cursor,
           int32_t& numToCheck,
           TR::Instruction **callInstruction,
           bool *regs, int32_t regsSize)

   {
   bool result = false;

   TR::Instruction *instruction = cursor;

   int32_t n = 0;

   while ((instruction != NULL) && (result == false))
      {
      if (n == numToCheck) break;
      n++;

      TR::InstOpCode::Mnemonic opCode = instruction->getOpCodeValue();
      switch (opCode)
         {
         case TR::InstOpCode::BASSM:
         case TR::InstOpCode::BASR:
         case TR::InstOpCode::BALR:
         case TR::InstOpCode::BAS:   // fastlink uses this
         case TR::InstOpCode::BAL:
         case TR::InstOpCode::BRAS:
         case TR::InstOpCode::BRASL:
            {
            result = true;
            }
            break;
         default:
            {
            // The first call instruction, don't set any of the registers as being
            // used (r14) because it will be transformed if possible
            //
            self()->setUsedRegisters(instruction,regs,regsSize);
            instruction = instruction->getNext();
            }
            break;
         }
      }


   bool canReplaceCallWithJump = false;
   if(callInstruction)
      {
      *callInstruction = instruction;
      canReplaceCallWithJump = true;
      }


   if (instruction)
      instruction = instruction->getNext();

   while (instruction !=NULL && canReplaceCallWithJump)
      {
      if (n == numToCheck) break;
         n++;

      self()->setUsedRegisters(instruction,regs,regsSize);

      TR::InstOpCode::Mnemonic opCode = instruction->getOpCodeValue();
      switch (opCode)
         {
         case TR::InstOpCode::ASSOCREGS:
         case TR::InstOpCode::FENCE:
         case TR::InstOpCode::LABEL:
         case TR::InstOpCode::RET:
            break;
         default:
            canReplaceCallWithJump = false;
            break;
         }
      instruction = instruction->getNext();
      }


   if (result && canReplaceCallWithJump && *callInstruction && n < numToCheck)
      {
      TR::InstOpCode::Mnemonic opCode = (*callInstruction)->getOpCodeValue();

      if(opCode == TR::InstOpCode::BASR)     // don't know which instruction to use for a virtual dispatch (yet).  In theory, BCR.
         {
         canReplaceCallWithJump = false;
         }
      else      // otherwise, we found only 1 call that can be changed to a jump
         {
         result = false;
         }
      }
   else if (callInstruction)
      {
      *callInstruction = NULL;
      }

   cursor = instruction;
   numToCheck = n;
   return result;
   }

void OMR::Z::Linkage::replaceCallWithJumpInstruction(TR::Instruction *callInstruction)
   {
   TR::InstOpCode::Mnemonic opCode = callInstruction->getOpCodeValue();
   TR_ASSERT(opCode == TR::InstOpCode::BASSM || opCode == TR::InstOpCode::BASR || opCode == TR::InstOpCode::BALR || opCode == TR::InstOpCode::BAS || opCode == TR::InstOpCode::BAL|| opCode == TR::InstOpCode::BRAS || opCode == TR::InstOpCode::BRASL, "Wrong opcode type on instruction!\n");

   TR::Node *node = (callInstruction)->getNode();
   TR::SymbolReference *callSymRef = (callInstruction)->getNode()->getSymbolReference();
   TR::Symbol *callSymbol = (callInstruction)->getNode()->getSymbolReference()->getSymbol();

   TR::Instruction *replacementInst =0 ;

   replacementInst = new (self()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::BRCL, node, (uint32_t)0xf, callSymbol, callSymRef, self()->cg());

   if(self()->comp()->getOption(TR_TraceCG))
      traceMsg(self()->comp(), "Replacing instruction %p to a jump %p !\n",callInstruction, replacementInst);

   self()->cg()->replaceInst(callInstruction,replacementInst);


   }


bool OMR::Z::Linkage::canDataTypeBePassedByReference(TR::DataType type) { return false; }

bool TR::S390zLinuxSystemLinkage::canDataTypeBePassedByReference(TR::DataType type)
   {

   if (type == TR::Aggregate)
      return true;

   return false;
   }

bool OMR::Z::Linkage::isSymbolPassedByReference(TR::Symbol *sym) { return false; }

bool TR::S390zLinuxSystemLinkage::isSymbolPassedByReference(TR::Symbol *sym)
   {
   if(!canDataTypeBePassedByReference(sym->getDataType()))
      return false;

   if(sym->getDataType() != TR::Aggregate)     //any non-aggregate symbols that have a data type that can be passed by reference *will* be passed by reference
      return true;

   int32_t gprSize = cg()->machine()->getGPRSize();
   int32_t symSize = sym->getSize();
   TR_ASSERT(0, "should not reach here due to folding of getDataTypeOfAggregateMember()");

   TR::DataType aggrMemberDataType = TR::NoType;
   bool isVectorAggregate = (TR::DataType(aggrMemberDataType).isVector() && cg()->getSupportsVectorRegisters());

   if ((symSize > gprSize) && !(gprSize == 4 && symSize == 8) && !isVectorAggregate)
      return true;

   return false;
   }

bool
TR::Z::ZOSBaseSystemLinkage::isAggregateReturnedInIntRegistersAndMemory(int32_t aggregateLenth)
   {
   return isFastLinkLinkageType() &&
      (aggregateLenth  > (getNumIntegerArgumentRegisters()* cg()->machine()->getGPRSize()));
   }

bool
TR::S390zOSSystemLinkage::isAggregateReturnedInIntRegisters(int32_t aggregateLenth)
   {
   return aggregateLenth  <= (getNumIntegerArgumentRegisters()* cg()->machine()->getGPRSize());
   }


/**
 * General XPLink utility
 *
 * Calculate XPLink call descriptor for given call site (excl. entry offset).
 * "callNode"  can be null which means that this is a special call.
 * The retunred value is used for call descriptor emission and is also used to
 * determine floating point register likage for outgoing parameters.
 */
uint32_t
TR::S390zOSSystemLinkage::calculateCallDescriptorFlags(TR::Node *callNode)
   {
   #define XPLINK_FLOAT_PARM_UNPROTYPED_CALL 0x08
   uint32_t callDescValue;

   if (TR::Compiler->target.is64Bit())
      {
      return 0;
      }

   // Linkage field  (0 means XPLink)
   // Bits 0-2 inclusive
   //
   bool linkageIsXPLink;
   linkageIsXPLink = true;
   callDescValue = 0x00000000;   // xplink (0) value here


   //
   // Return value adjust field
   // Bits 3-7 inclusive
   //

   TR::DataType dataType;
   TR::ILOpCodes opcode;
   int32_t aggregateLength = 0;

   if (callNode == NULL)  // specialized calls no real descriptor needed
      dataType = TR::NoType;
   else
      {
      dataType = callNode->getDataType();
#ifdef J9_PROJECT_SPECIFIC
      if (callNode->getType().isBCD())
         aggregateLength = callNode->getSize();
#endif
      }

   uint32_t rva;
   if (linkageIsXPLink)
      {
      rva = calculateReturnValueAdjustFlag(dataType, aggregateLength);
      }
   else
      {
      // TOBEY generates 0 for non-XPLink cases.
      // We are consistent with TOBEY although not sure that is 100% correct
      rva = 0;
      }
   callDescValue |= rva << 24;

   if (callNode == NULL)
      {
      return callDescValue; // specialized calls - so no float requirements
      }

   //
   // Float parameter description fields
   // Bits 8-31 inclusive
   //

   bool isPrototyped = true; // All Java calls are prototyped.
   uint32_t parmAreaOffset = 0;

#ifdef J9_PROJECT_SPECIFIC
   TR::MethodSymbol* callSymbol = callNode->getSymbol()->castToMethodSymbol();
   if (callSymbol->isJNI() && callNode->isPreparedForDirectJNI())
      {
      TR::ResolvedMethodSymbol * cs = callSymbol->castToResolvedMethodSymbol();
      TR_ResolvedMethod * resolvedMethod = cs->getResolvedMethod();
      // JNI Calls include a JNIEnv* pointer that is not included in list of children nodes.
      // For FastJNI, certain calls do not require us to pass the JNIEnv.
      if (!cg()->fej9()->jniDoNotPassThread(resolvedMethod))
         parmAreaOffset += sizeof(uintptrj_t);

      // For FastJNI, certain calls do not have to pass in receiver object.
      if (cg()->fej9()->jniDoNotPassReceiver(resolvedMethod))
         parmAreaOffset -= sizeof(uintptrj_t);
      }
#endif

   uint32_t parmDescriptorFields = 0;

   if (!isPrototyped)
      {
      parmDescriptorFields = shiftFloatParmDescriptorFlag(XPLINK_FLOAT_PARM_UNPROTYPED_CALL, 0);  // FPR0 field has special value for unprototped call
      callDescValue |= parmDescriptorFields;
      return callDescValue;
      }

   // WCode only logic follows for float parameter description fields

   TR::Symbol *funcSymbol = callNode->getSymbolReference()->getSymbol();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
   int32_t to = callNode->getNumChildren() - 1;
   int32_t parmCount = 1;

   int32_t floatParmNum = 0;

   uint32_t lastFloatParmAreaOffset= 0;

   bool done = false;
   for (int32_t i = firstArgumentChild; (i <= to) && !done; i++, parmCount++)
      {
      TR::Node *child = callNode->getChild(i);
      TR::DataType dataType = child->getDataType();
      TR::SymbolReference *parmSymRef = child->getOpCode().hasSymbolReference() ? child->getSymbolReference() : NULL;
      int32_t argSize = 0;

      if (parmSymRef==NULL)
         argSize = child->getSize();
      else
         argSize = parmSymRef->getSymbol()->getSize();

      done = updateFloatParmDescriptorFlags(&parmDescriptorFields, funcSymbol, parmCount, argSize, dataType, &floatParmNum, &lastFloatParmAreaOffset, &parmAreaOffset);
      }

   callDescValue |= parmDescriptorFields;
   return callDescValue;
   }
