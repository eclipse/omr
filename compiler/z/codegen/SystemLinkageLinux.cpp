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

// See also S390Linkage.cpp which contains more S390 Linkage
// implementations (primarily Private Linkage and base class).

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "ras/Delimiter.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/SystemLinkageLinux.hpp"
#include "OMR/Bytes.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

TR::S390zLinuxSystemLinkage::S390zLinuxSystemLinkage(TR::CodeGenerator* cg)
   :
      TR::SystemLinkage(cg, TR_SystemLinux)
   {
   setProperties(FirstParmAtFixedOffset);
   setProperty(SmallIntParmsAlignedRight);

   if (cg->comp()->target().is64Bit())
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

   if (cg->comp()->target().is64Bit())
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

   if (cg->comp()->target().is64Bit())
      {
      setFloatArgumentRegister(2, TR::RealRegister::FPR4);
      setFloatArgumentRegister(3, TR::RealRegister::FPR6);
      setNumFloatArgumentRegisters(4);
      }
   else
      {
      setNumFloatArgumentRegisters(2);
      }

   if (cg->getSupportsVectorRegisters())
      {
      int32_t index = 0;

      // The vector registers used for parameter passing are allocated in the following order to enable future exension
      // which might operate on vector register pairs
      setVectorArgumentRegister(index++, TR::RealRegister::VRF24);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF26);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF28);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF30);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF25);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF27);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF29);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF31);
      setNumVectorArgumentRegisters(index);

      setVectorReturnRegister(TR::RealRegister::VRF24);
      }

   setIntegerReturnRegister(TR::RealRegister::GPR2);
   setLongLowReturnRegister(TR::RealRegister::GPR3);
   setLongHighReturnRegister(TR::RealRegister::GPR2);
   setLongReturnRegister(TR::RealRegister::GPR2);
   setFloatReturnRegister(TR::RealRegister::FPR0);
   setDoubleReturnRegister(TR::RealRegister::FPR0);
   setLongDoubleReturnRegister0(TR::RealRegister::FPR0);
   setLongDoubleReturnRegister2(TR::RealRegister::FPR2);
   setLongDoubleReturnRegister4(TR::RealRegister::FPR4);
   setLongDoubleReturnRegister6(TR::RealRegister::FPR6);
   setStackPointerRegister(TR::RealRegister::GPR15);
   setNormalStackPointerRegister(TR::RealRegister::GPR15);
   setAlternateStackPointerRegister(TR::RealRegister::GPR11);
   setEntryPointRegister(TR::RealRegister::GPR14);
   setLitPoolRegister(TR::RealRegister::GPR13);
   setReturnAddressRegister(TR::RealRegister::GPR14);

#if defined(OMRZTPF)
   // z/TPF OS is a 64-bit only target
   setOffsetToRegSaveArea(16);

   // x'10'  see ICST_R2 in tpf/icstk.h
   setGPRSaveAreaBeginOffset(16);

   // x'a0'  see ICST_PAT in tpf/icstk.h
   setGPRSaveAreaEndOffset(160);

   // x'1c0' see ICST_PAR in tpf/icstk.h
   setOffsetToFirstParm(448);
#else
   if (cg->comp()->target().is64Bit())
      {
      setOffsetToRegSaveArea(16);
      setGPRSaveAreaBeginOffset(48);
      setGPRSaveAreaEndOffset(128);
      setOffsetToFirstParm(160);
      setOffsetToLongDispSlot(8);
      }
   else
      {
      setOffsetToRegSaveArea(8);
      setGPRSaveAreaBeginOffset(24);
      setGPRSaveAreaEndOffset(64);
      setOffsetToFirstParm(96);
      setOffsetToLongDispSlot(4);
      }
#endif /* defined(OMRZTPF) */

   setOffsetToFirstLocal(0);
   setOutgoingParmAreaBeginOffset(getOffsetToFirstParm());
   setOutgoingParmAreaEndOffset(0);
   setStackFrameSize(0);
   setNumberOfDependencyGPRegisters(32);
   setLargestOutgoingArgumentAreaSize(0);
   }

void TR::S390zLinuxSystemLinkage::createEpilogue(TR::Instruction * cursor)
   {
   TR::Delimiter delimiter (comp(), comp()->getOption(TR_TraceCG), "Epilogue");

   TR::Node* node = cursor->getNext()->getNode();

   cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, node, generateLabelSymbol(cg()), cursor);

   cursor = fillFPRsInEpilogue(node, cursor);
   cursor = fillGPRsInEpilogue(node, cursor);

   cursor = generateS390BranchInstruction(cg(), TR::InstOpCode::BCR, node, TR::InstOpCode::COND_BCR, getReturnAddressRealRegister(), cursor);
   }

void TR::S390zLinuxSystemLinkage::createPrologue(TR::Instruction* cursor)
   {
   TR::Delimiter delimiter (comp(), comp()->getOption(TR_TraceCG), "Prologue");

   int32_t argSize = getOutgoingParameterBlockSize();
   setOutgoingParmAreaEndOffset(getOutgoingParmAreaBeginOffset() + argSize);

   TR::ResolvedMethodSymbol* bodySymbol = comp()->getJittedMethodSymbol();

   // Calculate size of locals determined in prior call to `mapStack`. We make the size a multiple of the strictest
   // alignment symbol so that backwards mapping of auto symbols will follow the alignment.
   //
   // TODO: We should be using OMR::align here once mapStack is fixed so we don't pass negative offsets
   size_t localSize = ((-1 * static_cast<int32_t>(bodySymbol->getLocalMappingCursor())) + (8 - 1)) & ~(8 - 1);
   setStackFrameSize(((getOffsetToFirstParm() + argSize + localSize) + (8 - 1)) & ~(8 - 1));

   int32_t stackFrameSize = getStackFrameSize();

   TR_ASSERT_FATAL((stackFrameSize & 7) == 0, "Misaligned stack frame size (%d) detected", stackFrameSize);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "Initial stackFrameSize = %d\n Offset to first parameter = %d\n Argument size = %d\n Local size = %d\n", stackFrameSize, getOffsetToFirstParm(), argSize, localSize);
      }

   // Now that we know the stack frame size, map the stack backwards
   mapStack(bodySymbol, stackFrameSize);

   setFirstPrologueInstruction(cursor);

   TR::Node* node = comp()->getStartTree()->getNode();

   cursor = spillGPRsInPrologue(node, cursor);
   cursor = spillFPRsInPrologue(node, cursor);

#if defined(JITTEST)
   // Literal Pool for Test compiler
   TR::Snippet* firstSnippet = cg()->getFirstSnippet();
   if ( cg()->isLiteralPoolOnDemandOn() != true && firstSnippet != NULL )
      cursor = new (trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, node, getLitPoolRealRegister(), firstSnippet, cursor, cg());
#endif

   TR::RealRegister* spReg = getNormalStackPointerRealRegister();

   // Check for large stack
   const bool largeStack = stackFrameSize < MIN_IMMEDIATE_VAL || stackFrameSize > MAX_IMMEDIATE_VAL;

   if (largeStack)
      {
      TR::RealRegister* tempReg = getRealRegister(TR::RealRegister::GPR0);

      cursor = generateS390ImmToRegister(cg(), node, tempReg, (intptr_t)(stackFrameSize * -1), cursor);
      cursor = generateRRInstruction(cg(), TR::InstOpCode::getAddRegOpCode(), node, spReg, tempReg, cursor);
      }
   else
      {
      cursor = generateRXInstruction(cg(), TR::InstOpCode::LA, node, spReg, generateS390MemoryReference(spReg,(stackFrameSize) * -1, cg()),cursor);
      }

   cursor = reinterpret_cast<TR::Instruction*>(saveArguments(cursor, false));

   setLastPrologueInstruction(cursor);
   }

void
TR::S390zLinuxSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method)
   {
   self()->setParameterLinkageRegisterIndex(method, method->getParameterList());
   }

void
TR::S390zLinuxSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method, List<TR::ParameterSymbol> &parmList)
   {
   int32_t numGPRArgs = 0;
   int32_t numFPRArgs = 0;
   int32_t numVRFArgs = 0;

   int32_t maxGPRArgs = self()->getNumIntegerArgumentRegisters();
   int32_t maxFPRArgs = self()->getNumFloatArgumentRegisters();
   int32_t maxVRFArgs = self()->getNumVectorArgumentRegisters();

   ListIterator<TR::ParameterSymbol> paramIterator(&parmList);
   for (TR::ParameterSymbol* paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      int32_t lri = -1;

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Int64:
         case TR::Address:
            {
            if (numGPRArgs < maxGPRArgs)
               {
               lri = numGPRArgs;
               }

            numGPRArgs++;
            break;
            }

         case TR::Float:
         case TR::Double:
            {
            if (numFPRArgs < self()->getNumFloatArgumentRegisters())
               {
               lri = numFPRArgs;
               }
            
            numFPRArgs++;
            break;
            }

         case TR::Aggregate:
            {
            TR_ASSERT_FATAL(false, "Support for aggregates is currently not implemented");
            break;
            }

         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
         case TR::VectorDouble:
            {
            if (numVRFArgs < self()->getNumVectorArgumentRegisters())
               {
               lri = numVRFArgs;
               }

            numVRFArgs++;
            break;
            }

         default:
            {
            TR_ASSERT_FATAL(false, "Unknown data type %s", paramCursor->getDataType().toString());
            break;
            }
         }

      paramCursor->setLinkageRegisterIndex(lri);
      }
   }

void
TR::S390zLinuxSystemLinkage::generateInstructionsForCall(TR::Node* callNode, TR::RegisterDependencyConditions* deps, intptr_t targetAddress, TR::Register* methodAddressReg, TR::Register* javaLitOffsetReg, TR::LabelSymbol* returnFromJNICallLabel, TR::S390JNICallDataSnippet* jniCallDataSnippet, bool isJNIGCPoint)
   {
   TR::CodeGenerator * codeGen = cg();

   TR::RegisterDependencyConditions * postDeps = new (trHeapMemory())
      TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), cg());

   TR::Register * systemReturnAddressRegister =
      deps->searchPostConditionRegister(getReturnAddressRegister());

   //omr todo: this should be placed in the Test compiler
   //zLinux SystemLinkage
#if defined(JITTEST)
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
   TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
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
      case TR::acall:
      case TR::acalli:
         returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::lcalli:
         {
         if (cg()->comp()->target().is64Bit())
            {
            returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
            }
         else
            {
            lowReg = deps->searchPostConditionRegister(getLongLowReturnRegister());
            highReg = deps->searchPostConditionRegister(getLongHighReturnRegister());

            generateRSInstruction(cg(), TR::InstOpCode::SLLG, callNode, highReg, highReg, 32);
            generateRRInstruction(cg(), TR::InstOpCode::LR, callNode, highReg, lowReg);

            cg()->stopUsingRegister(lowReg);
            returnRegister = highReg;
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
   int32_t parmInLocalAreaOffset = 0;

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
            if(cg()->comp()->target().is32Bit())
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
                     numIntegerArgs += (cg()->comp()->target().is64Bit()) ? 1 : 2;
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

      if (cg()->comp()->target().is64Bit() && parmCursor->getType().isAddress() && (parmCursor->getSize()==4))
         {
         // This is a 31-bit pointer parameter. It's real location is +4 bytes from the start of the
         // 8-byte parm slot. Since ptr31 parms are passed as 64-bit values, the prologue code has the
         // spill the full 8-bytes into the parm slot. However, the rest of the code will load/store
         // this parm as a 4-byte value, hence we have to add +4 here.
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + 4);
         }

      parmCursor = parameterIterator.getNext();
      }

   setIncomingParmAreaEndOffset(stackIndex);
   setNumUsedArgumentGPRs(numIntegerArgs);
   setNumUsedArgumentFPRs(numFloatArgs);
   setNumUsedArgumentVFRs(numVectorArgs);
}

int32_t
TR::S390zLinuxSystemLinkage::getRegisterSaveOffset(TR::RealRegister::RegNum srcReg)
   {
   int32_t gpr2Offset = cg()->comp()->target().is64Bit() ? 16 : 8;
   int32_t fpr0Offset = cg()->comp()->target().is64Bit() ? 128 : 64;
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

TR::Instruction*
TR::S390zLinuxSystemLinkage::fillGPRsInEpilogue(TR::Node* node, TR::Instruction* cursor)
   {
   int16_t GPRSaveMask = 0;
   bool    hasRestoreSP = false;
   int32_t offset = 0;
   int32_t stackFrameSize = getStackFrameSize();
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();

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
            firstReg = static_cast<TR::RealRegister::RegNum>(i);

         if ((getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "\tand Assigned. ");
            findStartReg = false;
            }

         if (!findStartReg && ( !(getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod() || (i == TR::RealRegister::LastGPR && firstReg != TR::RealRegister::LastGPR)) )
            {
            // LastGPR is the stack pointer on zLinux which always be saved and restored
            if (i == TR::RealRegister::LastGPR)
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
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, getRealRegister(REGNUM(firstReg )), retAddrMemRef, cursor);
            else
               cursor = generateRSInstruction(cg(),  TR::InstOpCode::getLoadMultipleOpCode(), node, getRealRegister(REGNUM(firstReg)),  getRealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


            findStartReg = true;
            }

         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\n");
         }
      }

    // LastGPR is the stack pointer on zLinux which always be saved and restored
    if (!hasRestoreSP)
       {
       lastReg = static_cast<TR::RealRegister::RegNum>(TR::RealRegister::LastGPR);
       offset =  getRegisterSaveOffset(REGNUM(lastReg));
       TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset + stackFrameSize, cg());
       cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, getRealRegister(REGNUM(lastReg )), retAddrMemRef, cursor);
       }
   return cursor;
   }

TR::Instruction*
TR::S390zLinuxSystemLinkage::fillFPRsInEpilogue(TR::Node* node, TR::Instruction* cursor)
   {
   TR::RealRegister* spReg = getNormalStackPointerRealRegister();
   int32_t offset = getFPRSaveAreaEndOffset();
   int16_t FPRSaveMask = getFPRSaveMask();

   for (int32_t i = TR::Linkage::getFirstMaskedBit(FPRSaveMask); i <= TR::Linkage::getLastMaskedBit(FPRSaveMask); ++i)
     {
      if (FPRSaveMask & (1 << (i)))
         {
         TR::MemoryReference* fillMemRef = generateS390MemoryReference(spReg, offset, cg());

         cursor = generateRXInstruction(cg(), TR::InstOpCode::LD, node, getRealRegister(REGNUM(i + TR::RealRegister::FirstFPR)), fillMemRef, cursor);

         offset += cg()->machine()->getFPRSize();
         }
      }

   return cursor;
   }

TR::Instruction*
TR::S390zLinuxSystemLinkage::spillGPRsInPrologue(TR::Node* node, TR::Instruction* cursor)
   {
   int16_t GPRSaveMask = 0;
   bool hasSavedSP = false;
   int32_t offset = 0;
   TR::RealRegister * spReg = getNormalStackPointerRealRegister();

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

         if ((getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
            {
            if (comp()->getOption(TR_TraceCG))
               traceMsg(comp(), "\t It is Assigned. Putting in to GPRSaveMask\n");
            GPRSaveMask |= 1 << (i - TR::RealRegister::FirstGPR);
            findStartReg = false;
            }

         if (!findStartReg && ( !(getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod() || (i == TR::RealRegister::LastGPR && firstReg != TR::RealRegister::LastGPR)) )
            {
            // LastGPR is the stack pointer on zLinux which always be saved and restored
            if (i == TR::RealRegister::LastGPR)
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
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, getRealRegister(REGNUM(firstReg)), retAddrMemRef, cursor);
            else
               cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(), node, getRealRegister(REGNUM(firstReg)),  getRealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);


            findStartReg = true;
            }
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "\n");
         }
      }


   // LastGPR is the stack pointer on zLinux which always be saved and restored
   if (!hasSavedSP)
      {
       lastReg = static_cast<TR::RealRegister::RegNum>(TR::RealRegister::LastGPR);
       offset =  getRegisterSaveOffset(REGNUM(lastReg));
       TR::MemoryReference *retAddrMemRef = generateS390MemoryReference(spReg, offset, cg());
       cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, getRealRegister(REGNUM(lastReg)), retAddrMemRef, cursor);
      }

   setGPRSaveMask(GPRSaveMask);
   return cursor;
   }

TR::Instruction*
TR::S390zLinuxSystemLinkage::spillFPRsInPrologue(TR::Node* node, TR::Instruction* cursor)
   {
   TR::RealRegister* spReg = getNormalStackPointerRealRegister();
   int32_t offset = getFPRSaveAreaEndOffset();
   int16_t FPRSaveMask = getFPRSaveMask();

   for (int32_t i = TR::Linkage::getFirstMaskedBit(FPRSaveMask); i <= TR::Linkage::getLastMaskedBit(FPRSaveMask); ++i)
      {
      if (FPRSaveMask & (1 << (i)))
         {
         TR::MemoryReference* spillMemRef = generateS390MemoryReference(spReg, offset, cg());

         cursor = generateRXInstruction(cg(), TR::InstOpCode::STD, node, getRealRegister(REGNUM(i + TR::RealRegister::FirstFPR)), spillMemRef, cursor);

         offset += cg()->machine()->getFPRSize();
         }
      }

   return cursor;
   }
