/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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
#include "z/codegen/SystemLinkagezOS.hpp"
#include "OMR/Bytes.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

#define  GPREGINDEX(i)   (i-TR::RealRegister::FirstGPR)

TR::S390zOSSystemLinkage::S390zOSSystemLinkage(TR::CodeGenerator* cg)
   :
      TR::SystemLinkage(cg, TR_SystemXPLink),
      _entryPointMarkerLabel(NULL),
      _stackPointerUpdateLabel(NULL),
      _ppa1Snippet(NULL),
      _ppa2Snippet(NULL)
   {
   setProperties(FirstParmAtFixedOffset);
   setProperty(SmallIntParmsAlignedRight);
   setProperty(SplitLongParm);
   setProperty(SkipGPRsForFloatParms);

   if (cg->comp()->target().is64Bit())
      {
      setProperty(NeedsWidening);
      }

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

   if (cg->getSupportsVectorRegisters())
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

   setIntegerReturnRegister(TR::RealRegister::GPR3);
   setLongLowReturnRegister(TR::RealRegister::GPR3);
   setLongHighReturnRegister(TR::RealRegister::GPR2);
   setLongReturnRegister(TR::RealRegister::GPR3);
   setFloatReturnRegister(TR::RealRegister::FPR0);
   setDoubleReturnRegister(TR::RealRegister::FPR0);
   setLongDoubleReturnRegister0(TR::RealRegister::FPR0);
   setLongDoubleReturnRegister2(TR::RealRegister::FPR2);
   setLongDoubleReturnRegister4(TR::RealRegister::FPR4);
   setLongDoubleReturnRegister6(TR::RealRegister::FPR6);
   setStackPointerRegister(TR::RealRegister::GPR4);
   setNormalStackPointerRegister(TR::RealRegister::GPR4);
   setAlternateStackPointerRegister(TR::RealRegister::GPR9);
   setEntryPointRegister(TR::RealRegister::GPR6);
   setLitPoolRegister(TR::RealRegister::GPR8);
   setReturnAddressRegister (TR::RealRegister::GPR7);

   setIntegerArgumentRegister(0, TR::RealRegister::GPR1);
   setIntegerArgumentRegister(1, TR::RealRegister::GPR2);
   setIntegerArgumentRegister(2, TR::RealRegister::GPR3);
   setNumIntegerArgumentRegisters(3);

   setFloatArgumentRegister(0, TR::RealRegister::FPR0);
   setFloatArgumentRegister(1, TR::RealRegister::FPR2);
   setFloatArgumentRegister(2, TR::RealRegister::FPR4);
   setFloatArgumentRegister(3, TR::RealRegister::FPR6);
   setNumFloatArgumentRegisters(4);

   if (cg->getSupportsVectorRegisters())
      {
      int32_t index = 0;

      setVectorArgumentRegister(index++, TR::RealRegister::VRF24);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF25);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF26);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF27);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF28);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF29);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF30);
      setVectorArgumentRegister(index++, TR::RealRegister::VRF31);
      setNumVectorArgumentRegisters(index);

      setVectorReturnRegister(TR::RealRegister::VRF24);
      }

   if (cg->comp()->target().is64Bit())
      {
      setOffsetToFirstParm(XPLINK_STACK_FRAME_BIAS + 128);
      }
   else
      {
      setOffsetToFirstParm(XPLINK_STACK_FRAME_BIAS + 64);
      }

   setOffsetToRegSaveArea(2048);
   setOffsetToLongDispSlot(0);
   setOffsetToFirstLocal(0);
   setOutgoingParmAreaBeginOffset(getOffsetToFirstParm() - XPLINK_STACK_FRAME_BIAS);
   setOutgoingParmAreaEndOffset(0);
   setStackFrameSize(0);
   setNumberOfDependencyGPRegisters(32);
   setLargestOutgoingArgumentAreaSize(0);
   }

void
TR::S390zOSSystemLinkage::createEpilogue(TR::Instruction * cursor)
   {
   TR::Delimiter delimiter (comp(), comp()->getOption(TR_TraceCG), "Epilogue");

   TR::Node* node = cursor->getNext()->getNode();

   cursor = generateS390LabelInstruction(cg(), TR::InstOpCode::LABEL, node, generateLabelSymbol(cg()), cursor);

   cursor = fillFPRsInEpilogue(node, cursor);
   cursor = fillGPRsInEpilogue(node, cursor);

   if (comp()->target().is64Bit())
      {
      cursor = generateRXInstruction(cg(), InstOpCode::BC, node, getRealRegister(TR::RealRegister::GPR15), generateS390MemoryReference(getReturnAddressRealRegister(), 2, cg()), cursor);
      }
   else
      {
      cursor = generateRXInstruction(cg(), InstOpCode::BC, node, getRealRegister(TR::RealRegister::GPR15), generateS390MemoryReference(getReturnAddressRealRegister(), 4, cg()), cursor);
      }
   }

void TR::S390zOSSystemLinkage::createPrologue(TR::Instruction* cursor)
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
   setStackFrameSize((((getOffsetToFirstParm() + argSize + localSize) + (32 - 1)) & ~(32 - 1)) - XPLINK_STACK_FRAME_BIAS);

   int32_t stackFrameSize = getStackFrameSize();

   TR_ASSERT_FATAL((stackFrameSize & 31) == 0, "Misaligned stack frame size (%d) detected", stackFrameSize);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "Initial stackFrameSize = %d\n Offset to first parameter = %d\n Argument size = %d\n Local size = %d\n", stackFrameSize, getOffsetToFirstParm(), argSize, localSize);
      }

   // Now that we know the stack frame size, map the stack backwards
   mapStack(bodySymbol, stackFrameSize + XPLINK_STACK_FRAME_BIAS);

   TR::CodeGenerator* cg = self()->cg();

   _xplinkFunctionDescriptorSnippet = new (self()->trHeapMemory()) XPLINKFunctionDescriptorSnippet(cg);
   _ppa1Snippet = new (self()->trHeapMemory()) TR::PPA1Snippet(cg, this);
   _ppa2Snippet = new (self()->trHeapMemory()) TR::PPA2Snippet(cg, this);

   cg->addSnippet(_xplinkFunctionDescriptorSnippet);
   cg->addSnippet(_ppa1Snippet);
   cg->addSnippet(_ppa2Snippet);

   TR::Node* node = cursor->getNode();

   cursor = cursor->getPrev();

   // Emit the Entry Point Marker
   _entryPointMarkerLabel = generateLabelSymbol(cg);
   cursor = generateS390LabelInstruction(cg, InstOpCode::LABEL, node, _entryPointMarkerLabel, cursor);

   // "C.E.E.1."
   cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, 0x00C300C5, cursor);
   cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, 0x00C500F1, cursor);
   cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, 0x00000000, cursor);

   cg->addRelocation(new (cg->trHeapMemory()) InstructionLabelRelative32BitRelocation(cursor, -8, _ppa1Snippet->getSnippetLabel(), 1));

   // DSA size is the frame size aligned to 32-bytes which means it's least significant 5 bits are zero and are used to
   // represent the flags which are always 0 for OMR as we do not support leaf frames or direct calls to alloca()
   cursor = generateDataConstantInstruction(cg, TR::InstOpCode::DC, node, stackFrameSize, cursor);

   cursor = cursor->getNext();

   setFirstPrologueInstruction(cursor);

   cursor = spillGPRsInPrologue(node, cursor);
   cursor = spillFPRsInPrologue(node, cursor);

   cursor = reinterpret_cast<TR::Instruction*>(saveArguments(cursor, false));

   setLastPrologueInstruction(cursor);
   }

void
TR::S390zOSSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method)
   {
   self()->setParameterLinkageRegisterIndex(method, method->getParameterList());
   }

void
TR::S390zOSSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol * method, List<TR::ParameterSymbol> &parmList)
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
            
            // On 64-bit XPLINK floating point arguments leave "holes" in the GPR linkage registers, but not vice versa
            numGPRArgs++;
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
            
            // On 64-bit XPLINK floating point arguments leave "holes" in the GPR linkage registers, but not vice versa
            numGPRArgs++;
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
      int32_t minimalArgAreaSize = ((comp()->target().is64Bit()) ? 32 : 16);
      argAreaSize = (argAreaSize < minimalArgAreaSize) ? minimalArgAreaSize : argAreaSize;
      }
   return argAreaSize;
   }

/**
 * TR::S390zOSSystemLinkage::callNativeFunction - call System routine
 * return value will be return value from system routine copied to private linkage return reg
 */
TR::Register *
TR::S390zOSSystemLinkage::callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
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
         if(comp()->target().is64Bit() && returnRegister && !returnRegister->is64BitReg())
            {
            returnRegister->setIs64BitReg(true);    //in 64bit target, force return 64bit address register,
            }                                           //until it's known that dispatch functions can return non-64bit addresses.
         break;
      case TR::icall:
      case TR::icalli:
         retReg = deps->searchPostConditionRegister(getIntegerReturnRegister());
         returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
         break;
      case TR::lcalli:
      case TR::lcall:
         {
         if (comp()->target().is64Bit())
            {
            retReg = deps->searchPostConditionRegister(getIntegerReturnRegister());
            returnRegister = deps->searchPostConditionRegister(getIntegerReturnRegister());
            }
         else
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

TR::RealRegister::RegNum
TR::S390zOSSystemLinkage::getENVPointerRegister()
   {
   return TR::RealRegister::GPR5;
   }

TR::RealRegister::RegNum
TR::S390zOSSystemLinkage::getCAAPointerRegister()
   {
   return comp()->target().is64Bit() ?
      TR::RealRegister::NoReg :
      TR::RealRegister::GPR12;
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

void
TR::S390zOSSystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
    // WCode specific
    //
    // There are 4 cases for outgoing branch sequences
    //   case 1)            pure OS linkage call using entry point
    //                      BASR R14,R15
    //   case 2) (indirect) Call vi function pointer
    //                      LM R5,R6,disp(regfp)
    //                      BASR R7,R6
    //          where:
    //             a) regfp is a register containing pointer to function descriptor
    //                (most likely placed in R6)
    //             b) disp is offset into function descriptor - 16/0 (31/64 bit respectively)
    //   case 3) (direct) Call to (static or global) function defined in the compilation unit:
    //                      BRASL R7,func
    //
    //   case 4) (direct) Call to external function referenced function
    //                      LM R5,R6,disp(regenv)
    //                      BASR R7,R6
    //         where:
    //             a) disp is an offset in the environment (aka ADA) containing the
    //                function descriptor body (i.e. not pointer to function descriptor)
    TR_XPLinkCallTypes callType;
    
    TR::Register* aeReg = deps->searchPostConditionRegister(getENVPointerRegister());
    TR::Register* epReg = deps->searchPostConditionRegister(getEntryPointRegister());
    TR::Register* raReg = deps->searchPostConditionRegister(getReturnAddressRegister());

    TR::RegisterDependencyConditions* preDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(deps->getPreConditions(), NULL, deps->getAddCursorForPre(), 0, cg());
    TR::RegisterDependencyConditions* postDeps = new (trHeapMemory()) TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), cg());

    if (callNode->getOpCode().isIndirect())
       {
       TR::Register* targetAddress = cg()->evaluate(callNode->getFirstChild());

       generateRSInstruction(cg(), TR::InstOpCode::getLoadMultipleOpCode(), callNode, aeReg, epReg, generateS390MemoryReference(targetAddress, 0, cg()));
       generateRRInstruction(cg(), InstOpCode::BASR, callNode, raReg, epReg, preDeps);
       callType = TR_XPLinkCallType_BASR;
       }
    else
       {
       TR::SymbolReference* callSymRef = callNode->getSymbolReference();
       TR::Symbol* callSymbol = callSymRef->getSymbol();

       if (comp()->isRecursiveMethodTarget(callSymbol))
          {
          // No need to load the environment or the entry point for recursive calls because these values are not used
          // within OMR compiled methods
          TR::Instruction* callInstr = new (cg()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, raReg, callSymbol, callSymRef, cg());
          callInstr->setDependencyConditions(preDeps);
          }
       else
          {
          struct FunctionDescriptor
             {
             void* environment;
             void* func;
             };

          FunctionDescriptor* fd = reinterpret_cast<FunctionDescriptor*>(callSymbol->castToMethodSymbol()->getMethodAddress());

          genLoadAddressConstant(cg(), callNode, reinterpret_cast<uintptr_t>(fd), epReg);
          generateRSInstruction(cg(), TR::InstOpCode::getLoadMultipleOpCode(), callNode, aeReg, epReg, generateS390MemoryReference(epReg, 0, cg()));

          TR::Instruction* callInstr = new (cg()->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::BRASL, callNode, raReg, fd->func, callSymRef, cg());
          callInstr->setDependencyConditions(preDeps);
          }

       callType = TR_XPLinkCallType_BRASL7;
       }

    auto cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, callNode, returnFromJNICallLabel);

    genCallNOPAndDescriptor(cursor, callNode, callNode, callType);

    // Append post-dependencies after NOP
    TR::LabelSymbol* depsLabel = generateLabelSymbol(cg());
    generateS390LabelInstruction(cg(), InstOpCode::LABEL, callNode, depsLabel, postDeps);
}
TR::LabelSymbol*
TR::S390zOSSystemLinkage::getEntryPointMarkerLabel() const
   {
   return _entryPointMarkerLabel;
   }

TR::LabelSymbol*
TR::S390zOSSystemLinkage::getStackPointerUpdateLabel() const
   {
   return _stackPointerUpdateLabel;
   }

TR::PPA1Snippet*
TR::S390zOSSystemLinkage::getPPA1Snippet() const
   {
   return _ppa1Snippet;
   }

TR::PPA2Snippet*
TR::S390zOSSystemLinkage::getPPA2Snippet() const
   {
   return _ppa2Snippet;
   }

TR::Instruction *
TR::S390zOSSystemLinkage::genCallNOPAndDescriptor(TR::Instruction* cursor, TR::Node* node, TR::Node* callNode, TR_XPLinkCallTypes callType)
   {
   if (comp()->target().is32Bit())
      {
      // The XPLINK Call Descriptor is created only on 31-bit targets when:
      // 
      // 1. The call site is so far removed from the Entry Point Marker of the function that its offset cannot be contained
      // in the space available in the call NOP descriptor following the call site.
      // 
      // 2. The call contains a return value or parameters that are passed in registers or in ways incompatible with non-
      // XPLINK code.
      // 
      // The XPLINK Call Descriptor has the following format:
      // 
      //                                        0x01                               0x02                               0x03
      // 0x00 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
      //      | Signed offset, in bytes, to Entry Point Marker (if it exists)                                                                             |
      // 0x04 +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
      //      | Linkage and Return Value Adjust  | Parameter Adjust                                                                                       |
      //      +----------------------------------+----------------------------------+----------------------------------+----------------------------------+
      //
      // We generate the XPLINK call descriptor inline right after the call instead of the literal pool because some
      // z/OS 31-bit programs tend to be rather large, and the distance between the call location and the literal pool
      // may exceed the number of bits we have to encode the offset in the NOP descriptor.

      TR::LabelSymbol* xplinkCallDescriptorBeginLabel = generateLabelSymbol(cg());
      TR::LabelSymbol* xplinkCallDescriptorEndLabel = generateLabelSymbol(cg());

      uint32_t nopDescriptor = 0x47000000 | (static_cast<uint32_t>(callType) << 16);
      cursor = generateDataConstantInstruction(cg(), TR::InstOpCode::DC, node, nopDescriptor, cursor);

      cg()->addRelocation(new (cg()->trHeapMemory()) XPLINKCallDescriptorRelocation(cursor, xplinkCallDescriptorBeginLabel));

      cursor = generateS390BranchInstruction(cg(), InstOpCode::BRC, InstOpCode::COND_BRC, node, xplinkCallDescriptorEndLabel, cursor);
      cursor = generateAlignmentNopInstruction(cg(), node, 8, cursor);
      cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, xplinkCallDescriptorBeginLabel, cursor);
      cursor = generateDataConstantInstruction(cg(), TR::InstOpCode::DC, node, 0x00000000, cursor);

      uint32_t callDescriptorValue = generateCallDescriptorValue(callNode);
      cursor = generateDataConstantInstruction(cg(), TR::InstOpCode::DC, node, callDescriptorValue, cursor);
      cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, xplinkCallDescriptorEndLabel, cursor);
      }
   else
      {
      // TODO: Once we support generic TR::InstOpCode::DC of any size we need to modify this line to use it, similarly
      // to what we do above for the 31-bit case.
      uint16_t nopDescriptor = 0x1800 | static_cast<uint16_t>(callType);
      cursor = new (cg()->trHeapMemory()) TR::S390Imm2Instruction(TR::InstOpCode::DC2, node, nopDescriptor, cursor, cg());
      }

   return cursor;
   }

uint32_t
TR::S390zOSSystemLinkage::generateCallDescriptorValue(TR::Node* callNode)
   {
   uint32_t result = 0;

   if (cg()->comp()->target().is32Bit())
      {
      uint32_t returnValueAdjust = 0;

      // 5 bit values for Return Value Adjust field of XPLLINK descriptor
      enum ReturnValueAdjust
         {
         XPLINK_RVA_RETURN_VOID_OR_UNUSED  = 0x00,
         XPLINK_RVA_RETURN_INT32_OR_LESS   = 0x01,
         XPLINK_RVA_RETURN_INT64           = 0x02,
         XPLINK_RVA_RETURN_FAR_POINTER     = 0x04,
         XPLINK_RVA_RETURN_FLOAT4          = 0x08,
         XPLINK_RVA_RETURN_FLOAT8          = 0x09,
         XPLINK_RVA_RETURN_FLOAT16         = 0x0A,
         XPLINK_RVA_RETURN_COMPLEX4        = 0x0C,
         XPLINK_RVA_RETURN_COMPLEX8        = 0x0D,
         XPLINK_RVA_RETURN_COMPLEX16       = 0x0E,
         XPLINK_RVA_RETURN_AGGREGATE       = 0x10,
         };

      TR::DataType dataType = callNode->getDataType();

      switch (dataType)
         {
         case TR::NoType:
            returnValueAdjust = XPLINK_RVA_RETURN_VOID_OR_UNUSED;
            break;

         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            returnValueAdjust = XPLINK_RVA_RETURN_INT32_OR_LESS;
            break;

         case TR::Int64:
            returnValueAdjust = XPLINK_RVA_RETURN_INT64;
            break;

         case TR::Float:
            returnValueAdjust = XPLINK_RVA_RETURN_FLOAT4;
            break;

         case TR::Double:
            returnValueAdjust = XPLINK_RVA_RETURN_FLOAT8;
            break;

         default:
            TR_ASSERT_FATAL(false, "Unknown datatype (%s) for call node (%p)", dataType.toString(), callNode);
            break;
         }

      result |= returnValueAdjust << 24;

      //
      // Float parameter description fields
      // Bits 8-31 inclusive
      //

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
            parmAreaOffset += sizeof(uintptr_t);

         // For FastJNI, certain calls do not have to pass in receiver object.
         if (cg()->fej9()->jniDoNotPassReceiver(resolvedMethod))
            parmAreaOffset -= sizeof(uintptr_t);
         }
#endif

      uint32_t parmDescriptorFields = 0;

      TR::Symbol *funcSymbol = callNode->getSymbolReference()->getSymbol();

      uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
      int32_t to = callNode->getNumChildren() - 1;
      int32_t parmCount = 1;

      int32_t floatParmNum = 0;
      uint32_t gprSize = cg()->machine()->getGPRSize();

      uint32_t lastFloatParmAreaOffset = 0;

      bool done = false;
      for (int32_t i = firstArgumentChild; (i <= to) && !done; i++, parmCount++)
         {
         TR::Node *child = callNode->getChild(i);
         TR::DataType dataType = child->getDataType();
         TR::SymbolReference *parmSymRef = child->getOpCode().hasSymbolReference() ? child->getSymbolReference() : NULL;
         int32_t argSize = 0;

         if (parmSymRef == NULL)
            argSize = child->getSize();
         else
            argSize = parmSymRef->getSymbol()->getSize();


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
            uint32_t wordsToPreviousParm = (parmAreaOffset - lastFloatParmAreaOffset) / gprSize;
            if (wordsToPreviousParm > 0xF)
               { // to big for descriptor. Will pass in stack
               done = true; // done
               }
            uint32_t val = wordsToPreviousParm + ((unitSize == 4) ? 0x10 : 0x20);

            parmDescriptorFields |= val << (6 * (3 - floatParmNum));

            floatParmNum++;

            if (floatParmNum >= self()->getNumFloatArgumentRegisters())
               {
               done = true;
               }
            }
         parmAreaOffset += argSize < gprSize ? gprSize : argSize;

         if (numFPRsNeeded != 0)
            {
            lastFloatParmAreaOffset = parmAreaOffset;
            }
         }

      result |= parmDescriptorFields;
      }

   return result;
   }

TR::Instruction *
TR::S390zOSSystemLinkage::addImmediateToRealRegister(TR::RealRegister *targetReg, int32_t value, TR::RealRegister *tempReg, TR::Node *node, TR::Instruction *cursor, bool *checkTempNeeded)
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
   else
      {
      if (!checkTempNeeded)
         cursor = generateRXInstruction(cg(), TR::InstOpCode::LAY, node, targetReg, generateS390MemoryReference(targetReg,value,cg()), cursor);
      }

   return cursor;
   }

TR::Instruction*
TR::S390zOSSystemLinkage::fillGPRsInEpilogue(TR::Node* node, TR::Instruction* cursor)
   {
   int16_t GPRSaveMask;
   int8_t gprSize = cg()->machine()->getGPRSize();
   TR::Node * currentNode = cursor->getNode();
   int32_t offset;

   int32_t stackFrameSize, offsetToFirstSavedReg;
   int32_t  firstSaved, lastSaved, firstPossibleSaved;
   TR::MemoryReference *rsa;

   int32_t blockNumber = -1;

   TR::RealRegister *spReg = getNormalStackPointerRealRegister(); // normal sp reg used in prol/epil
   stackFrameSize = getStackFrameSize();

   GPRSaveMask = getGPRSaveMask(); // restore mask is subset of save mask
   GPRSaveMask  &= ~(1 << GPREGINDEX(getEntryPointRegister())); // entry point register not restored

   firstSaved = TR::Linkage::getFirstMaskedBit(GPRSaveMask);
   firstPossibleSaved = 4; // GPR4 is first reg in save area
   lastSaved = TR::Linkage::getLastMaskedBit(GPRSaveMask);

   if (firstSaved >= 0)
      {
      offsetToFirstSavedReg = (firstSaved-firstPossibleSaved)*gprSize; // relative to start of save area
      offset = XPLINK_STACK_FRAME_BIAS + offsetToFirstSavedReg;
      rsa = generateS390MemoryReference(spReg, offset, cg());

      if (firstSaved == lastSaved)
         {
            cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), currentNode,
                                           getRealRegister(REGNUM(firstSaved + TR::RealRegister::FirstGPR)), rsa, cursor);
         }
      else if (firstSaved < lastSaved)
         {
         int8_t numNeededToRestore = -1;

            cursor = restorePreservedRegs(REGNUM(firstSaved + TR::RealRegister::FirstGPR),
                                         REGNUM(lastSaved + TR::RealRegister::FirstGPR),
                                         blockNumber, cursor, currentNode, spReg, rsa, getNormalStackPointerRegister());
         }
      }

   if (!(GPRSaveMask &(1 << GPREGINDEX(getNormalStackPointerRegister()))))
      { // GPR4 not restored by previous and needs restoring
      cursor = addImmediateToRealRegister(spReg, stackFrameSize,  NULL, currentNode, cursor);
      TR_ASSERT( cursor != NULL, "xplink retore code - should not need temp register");
      }

   return cursor;
   }

TR::Instruction*
TR::S390zOSSystemLinkage::fillFPRsInEpilogue(TR::Node* node, TR::Instruction* cursor)
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
TR::S390zOSSystemLinkage::spillGPRsInPrologue(TR::Node* node, TR::Instruction* cursor)
   {
   enum TR_XPLinkFrameType frameType;
   int16_t GPRSaveMask;
   TR::LabelSymbol *stmLabel;

   int8_t gprSize = cg()->machine()->getGPRSize();
   int32_t stackFrameSize, offsetToRegSaveArea, offsetToFirstSavedReg, gpr3ParmOffset;
   int32_t  firstSaved, lastSaved, firstPossibleSaved;

   // As defined by XPLINK specification
   int32_t intermediateThreshold = comp()->target().is64Bit() ? 1024 * 1024 : 4096;

   TR::RealRegister *spReg = getNormalStackPointerRealRegister(); // normal sp reg used in prol/epil
   stackFrameSize = getStackFrameSize();
   offsetToRegSaveArea = getOffsetToRegSaveArea();

   // This delta is slightly pessimistic and could be a "tad" more. But, taking
   // into account the "tad" complicates this code more than its worth.
   // I.E. small set of cases could be forced to being intermediate vs. small
   // frame types because of this "tad".
   int32_t delta = XPLINK_STACK_FRAME_BIAS - stackFrameSize;

   if (stackFrameSize > intermediateThreshold)
      // guard page option mandates explicit checking
      frameType = TR_XPLinkStackCheckFrame;
   else if (delta >=0)
      // delta > 0 implies stack offset for STM will be positive and <2K
      frameType = TR_XPLinkSmallFrame;
   else if (stackFrameSize < intermediateThreshold)
      frameType = TR_XPLinkIntermediateFrame;
   else
      frameType = TR_XPLinkStackCheckFrame;

   TR::MemoryReference *rsa, *bosRef, *extenderRef, *gpr3ParmRef;
   int32_t rsaOffset, bosOffset, extenderOffset;

   //
   // Determine minimal Preserved GPRs. Notes:
   //   1) an option can force the save of the stack pointer register
   //   2) XPLink noleaf:  routines mandates the save of the entry point register (r6) & return register (r7)
   //               leaf:  routines cannot modify r6, r7
   //
   GPRSaveMask = 0;
   TR::RealRegister::RegNum lastUsedReg = TR::RealRegister::NoReg;
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
     {
     if (getPreserved(REGNUM(i)))
        {
        if (getRealRegister(REGNUM(i))->getHasBeenAssignedInMethod())
           {
           GPRSaveMask |= 1 << GPREGINDEX(i);
           }
        }
     }

   GPRSaveMask  |= 1 << GPREGINDEX(getReturnAddressRegister());
   GPRSaveMask  |= 1 << GPREGINDEX(getEntryPointRegister());

    // Determine if we need a temp register in either buying or releasing
    // the stack frame. If so, we will save the back chain so epilogue code
    // can just restore it from the frame and not require a temp register.
    // We still need a temp register for prologue code though.
    bool needAddTempReg, saveTempReg;
    addImmediateToRealRegister(NULL, stackFrameSize, NULL, NULL, NULL, &needAddTempReg);

   // For 64 bit stack checking code - need a temp reg for LAA access
   if (comp()->target().is64Bit() && (frameType == TR_XPLinkStackCheckFrame))
      needAddTempReg = true;

   if (needAddTempReg)
      GPRSaveMask  |= 1 << GPREGINDEX(getNormalStackPointerRegister());
   else
      GPRSaveMask  &= ~(1 << GPREGINDEX(getNormalStackPointerRegister()));


   // CAA is either locked or restored on RET - so remove it from mask
   // But not in 64 bit code
   if(!comp()->target().is64Bit())
      {
      GPRSaveMask  &= ~(1 << GPREGINDEX(getCAAPointerRegister()));
      }

   if ((frameType == TR_XPLinkStackCheckFrame)
      || ((frameType == TR_XPLinkIntermediateFrame) && needAddTempReg))
      { // GPR4 is saved but indirectly via GPR0 in stack check or medium case - so don't save GPR4 via STM
      firstSaved = TR::Linkage::getFirstMaskedBit(GPRSaveMask&~(1 << GPREGINDEX(getNormalStackPointerRegister())));
      }
   else
      firstSaved = TR::Linkage::getFirstMaskedBit(GPRSaveMask);

   setGPRSaveMask(GPRSaveMask);

   if (comp()->getOption(TR_TraceCG))
      {
      traceMsg(comp(), "GPRSaveMask: Register context %x\n", GPRSaveMask&0xffff);
      }

   firstPossibleSaved = GPREGINDEX(TR::RealRegister::GPR4); // GPR4 is first reg in save area
   lastSaved = TR::Linkage::getLastMaskedBit(GPRSaveMask);
   offsetToFirstSavedReg = (firstSaved-firstPossibleSaved)*gprSize; // relative to start of save area

   // variables for stack extension code - xplink spec mandates these particular registers
   TR::RealRegister * gpr0Real = getRealRegister(TR::RealRegister::GPR0);
   TR::RealRegister * gpr3Real = getRealRegister(TR::RealRegister::GPR3);
   TR::RealRegister * caaReal  = getRealRegister(getCAAPointerRegister());  // 31 bit oly
   gpr3ParmOffset = getOffsetToFirstParm() + 2 * gprSize;

   _firstSaved = REGNUM(firstSaved + TR::RealRegister::FirstGPR);
   _lastSaved  = REGNUM(lastSaved  + TR::RealRegister::FirstGPR);

   switch (frameType)
      {
      case TR_XPLinkSmallFrame:
          // STM rx,ry,offset(R4)
          rsaOffset =  (offsetToRegSaveArea - stackFrameSize) + offsetToFirstSavedReg;
          rsa = generateS390MemoryReference(spReg, rsaOffset, cg());
          cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(),
                  node, getRealRegister(REGNUM(firstSaved + TR::RealRegister::FirstGPR)),
                  getRealRegister(REGNUM(lastSaved + TR::RealRegister::FirstGPR)), rsa, cursor);

          _stackPointerUpdateLabel = generateLabelSymbol(cg());
          cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, _stackPointerUpdateLabel, cursor);

          // AHI R4,-stackFrameSize
          cursor = addImmediateToRealRegister(spReg, (stackFrameSize) * -1, NULL, node, cursor);
          break;

      case TR_XPLinkIntermediateFrame:
          if (needAddTempReg)
             {
             // (needAddTempReg) LR R0,R4
             // (saveTempReg|needAddTempReg) ST R3,D(R4)   <-- save temp register
             // R4<- R4 - stackSize
             // LR R3,R0
             // (saveTempReg) L  R3,D(R3)   <-- load temp register
             saveTempReg = true;  // PERFORMANCE NOTE: TODO: set to false if GPR3 is not used as a parameter

             if (saveTempReg || needAddTempReg)
                {
                cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr0Real, spReg, cursor); // LR R0,R4
                }
             if (saveTempReg)
                {
                gpr3ParmRef = generateS390MemoryReference(spReg, gpr3ParmOffset, cg()); // used for temporary register
                cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, gpr3Real, gpr3ParmRef, cursor); // ST R3,parmRef(R4)
                }

             _stackPointerUpdateLabel = generateLabelSymbol(cg());
             cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, _stackPointerUpdateLabel, cursor);

             cursor = addImmediateToRealRegister(spReg, (stackFrameSize) * -1, gpr3Real, node, cursor);  // R4 <- R4 - stack size

             if (saveTempReg)
                {
                cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr3Real, gpr0Real, cursor);
                gpr3ParmRef = generateS390MemoryReference(gpr3Real, gpr3ParmOffset, cg());
                cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, gpr3Real, gpr3ParmRef, cursor);
                }
             }
          else
             { // no temp reg needed
             // (needAddTempReg)    LR R0,R4
             // AHI R4,-stackFrameSize
             // STM rx,ry,offset(R4)
             // (needAddTempReg)    ST R0,D(R4)

             if (needAddTempReg) // LR R0,R4
                cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node, gpr0Real, spReg, cursor);

             _stackPointerUpdateLabel = generateLabelSymbol(cg());
             cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, _stackPointerUpdateLabel, cursor);

             cursor = addImmediateToRealRegister(spReg, (stackFrameSize) * -1, NULL, node, cursor);
             }
          rsaOffset =  offsetToRegSaveArea + offsetToFirstSavedReg;
          rsa = generateS390MemoryReference(spReg, rsaOffset, cg());
          cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(),
                  node, getRealRegister(REGNUM(firstSaved + TR::RealRegister::FirstGPR)),
                  getRealRegister(REGNUM(lastSaved + TR::RealRegister::FirstGPR)), rsa, cursor);
          if (needAddTempReg)
             { // ST R0,2048(R4)
             rsaOffset = offsetToRegSaveArea + 0; // GPR4 saved at start of RSA
             rsa = generateS390MemoryReference(spReg, rsaOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, gpr0Real, rsa, cursor);
             }
          break;

      case TR_XPLinkStackCheckFrame:
          // TODO: move the code to exend stack to "cold" path
          // Code:
          //   important note: saveTempReg -> needAddTempReg
          //
          //     (saveTempReg)      ST R3,parmOffsetR3(GPR4)
          //     (needAddTempReg)    LR R0,R4
          //     AHI R4, -stackFrameSize  [this varies based on stackFrameSize]
          //     [31 bit] C  R4,BOS(R12)           R12=CAA
          //     [64 bit] LLGT  R3,1268            R3=control block - low mem
          //     [64 bit] C  R4,BOS(R3)            R3=ditto
          //     BL stmLabel
          //     /* extension logic */
          //     (!needAddTempReg)    LR R0,R3   /* could be smarter here */
          //     L  R3,extender(R12)
          //     BASR R3,R3
          //     NOP x                    [xplink noop]
          //     (!needAddTempReg)   LR R3,R0   /* could be smarter here */
          //
          //   stmLabel:
          //     STM rx,ry,offset(R4)
          //     (needAddTempReg)    ST R0,2048(,R4)
          //     (saveTempReg)      LR R3,R0   /* could be smarter here */
          //     (saveTempReg)      L  R3,parmOffsetR3(GPR3)
          //
          stmLabel = generateLabelSymbol(cg());
          saveTempReg = needAddTempReg;  // TODO: and with check that R3 is used as parm

          //------------------------------
          // Stack check - prologue part 1
          //------------------------------

          if (saveTempReg)
             {
             gpr3ParmRef = generateS390MemoryReference(spReg, gpr3ParmOffset, cg()); // used for temporary register
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, gpr3Real, gpr3ParmRef, cursor); // ST R3,parmRef(R4)
             }
          if (needAddTempReg) // LR R0,R4
              cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr0Real, spReg, cursor);
          cursor = addImmediateToRealRegister(spReg, (stackFrameSize) * -1, gpr3Real, node, cursor); // AHI ...

          if (comp()->target().is64Bit())
             {
             TR::MemoryReference *laaRef;
             bosOffset = 64; // 64=offset in LAA to BOS
             int32_t laaLowMemAddress = 1208; // LAA address
             laaRef = generateS390MemoryReference(gpr0Real, laaLowMemAddress, cg());
             cursor = generateRXInstruction(cg(), InstOpCode::LLGT, node, gpr3Real, laaRef, cursor);
             bosRef = generateS390MemoryReference(gpr3Real, bosOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getCmpOpCode(), node, spReg, bosRef, cursor); // C R4,bos(tempreg)
             }
          else
             {
             bosOffset = 868; // 868=offset in CAA to BOS
             bosRef = generateS390MemoryReference(caaReal, bosOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getCmpOpCode(), node, spReg, bosRef, cursor); // C R4,bos(R12)
             }
          cursor = generateS390BranchInstruction(cg(), InstOpCode::BRC, InstOpCode::COND_BNM, node, stmLabel, cursor); // BNM stmLabel

          //------------------------------
          // Stack check - extension routine logic
          //------------------------------

          // This logic should be moved out of line eventually
          if (!needAddTempReg)
             {
             cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr0Real, gpr3Real, cursor); // LR R0,R3
             }

          if (comp()->target().is64Bit())
             {
             extenderOffset = 72;  // offset of extender routine in LAA
             extenderRef = generateS390MemoryReference(gpr3Real, extenderOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, gpr3Real, extenderRef, cursor); //  L R3,extender(R12)
             }
          else
             {
             extenderOffset = 872;  // offset of extender routine in CAA
             extenderRef = generateS390MemoryReference(caaReal, extenderOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, gpr3Real, extenderRef, cursor); //  L R3,extender(R12)
             }
          cursor = generateRRInstruction(cg(), InstOpCode::BASR, node, gpr3Real, gpr3Real, cursor);
          cursor = genCallNOPAndDescriptor(cursor, node, NULL, TR_XPLinkCallType_BASR33);

          if (!needAddTempReg)
             { // LR R3,R0
             cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr3Real, gpr0Real, cursor);
             }

          //------------------------------
          // Stack check - prologue part 2
          //------------------------------

          // STM rx,ry,disp(R4)
          cursor = generateS390LabelInstruction(cg(), InstOpCode::LABEL, node, stmLabel, cursor);

          rsaOffset =  offsetToRegSaveArea + offsetToFirstSavedReg;
          rsa = generateS390MemoryReference(spReg, rsaOffset, cg());
          cursor = generateRSInstruction(cg(),  TR::InstOpCode::getStoreMultipleOpCode(),
                  node, getRealRegister(REGNUM(firstSaved + TR::RealRegister::FirstGPR)),
                  getRealRegister(REGNUM(lastSaved + TR::RealRegister::FirstGPR)), rsa, cursor);
          if (needAddTempReg)
             { // ST R0,2048(R4)
             rsaOffset = offsetToRegSaveArea + 0; // GPR4 saved at start of RSA
             rsa = generateS390MemoryReference(spReg, rsaOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getStoreOpCode(), node, gpr0Real, rsa, cursor);
             }
          if (saveTempReg)
             {
             cursor = generateRRInstruction(cg(), TR::InstOpCode::getLoadRegOpCode(), node,  gpr3Real, gpr0Real, cursor);
             gpr3ParmRef = generateS390MemoryReference(gpr3Real, gpr3ParmOffset, cg());
             cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), node, gpr3Real, gpr3ParmRef, cursor);
             }

          break;

      default:
          TR_ASSERT( 0,"invalid xplink frame type");
          break;
      }

   return cursor;
   }

TR::Instruction*
TR::S390zOSSystemLinkage::spillFPRsInPrologue(TR::Node* node, TR::Instruction* cursor)
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

TR::S390zOSSystemLinkage::XPLINKCallDescriptorRelocation::XPLINKCallDescriptorRelocation(TR::Instruction* nop, TR::LabelSymbol* callDescriptor)
   :
      TR::LabelRelocation(NULL, callDescriptor),
      _nop(nop)
   {
   }

uint8_t*
TR::S390zOSSystemLinkage::XPLINKCallDescriptorRelocation::getUpdateLocation()
   {
   uint8_t* updateLocation = TR::LabelRelocation::getUpdateLocation();

   if (updateLocation == NULL && _nop->getBinaryEncoding() != NULL)
      {
      updateLocation = setUpdateLocation(_nop->getBinaryEncoding() + 2);
      }

   return updateLocation;
   }

void
TR::S390zOSSystemLinkage::XPLINKCallDescriptorRelocation::apply(TR::CodeGenerator* cg)
   {
   int64_t offsetToCallDescriptor = static_cast<int64_t>(getLabel()->getCodeLocation() - (_nop->getBinaryEncoding() - 7)) / 8;

   if (offsetToCallDescriptor < std::numeric_limits<int16_t>::min() || offsetToCallDescriptor > std::numeric_limits<int16_t>::max())
      {
      cg->comp()->failCompilation<TR::ExcessiveComplexity>("Unable to encode offset to XPLINK call descriptor");
      }

   uint8_t* p = getUpdateLocation();
   *reinterpret_cast<int16_t*>(p) = static_cast<int16_t>(offsetToCallDescriptor);
   }

TR::S390zOSSystemLinkage::XPLINKFunctionDescriptorSnippet::XPLINKFunctionDescriptorSnippet(TR::CodeGenerator* cg)
   :
      TR::S390ConstantDataSnippet(cg, NULL, NULL, cg->comp()->target().is32Bit() ? 8 : 16)
   {
   if (cg->comp()->target().is32Bit())
      {
      *(reinterpret_cast<uint32_t*>(_value) + 0) = 0;
      *(reinterpret_cast<uint32_t*>(_value) + 1) = 0;
      }
   else
      {
      *(reinterpret_cast<uint64_t*>(_value) + 0) = 0;
      *(reinterpret_cast<uint64_t*>(_value) + 1) = 0;
      }
   }

uint8_t*
TR::S390zOSSystemLinkage::XPLINKFunctionDescriptorSnippet::emitSnippetBody()
   {
   uint8_t* cursor = cg()->getBinaryBufferCursor();

   // TODO: We should not have to do this here. This should be done by the caller.
   getSnippetLabel()->setCodeLocation(cursor);

   // Update the method symbol address to point to the function descriptor
   cg()->comp()->getMethodSymbol()->setMethodAddress(cursor);

   if (cg()->comp()->target().is32Bit())
      {
      // Address of function's environment
      *reinterpret_cast<uint32_t*>(cursor) = 0;
      cursor += sizeof(uint32_t);

      // Function of function
      *reinterpret_cast<uint32_t**>(cursor) = reinterpret_cast<uint32_t*>(cg()->getCodeStart());
      cursor += sizeof(uint32_t);
      }
   else
      {
      // Address of function's environment
      *reinterpret_cast<uint64_t*>(cursor) = 0;
      cursor += sizeof(uint64_t);

      // Function of function
      *reinterpret_cast<uint64_t**>(cursor) = reinterpret_cast<uint64_t*>(cg()->getCodeStart());
      cursor += sizeof(uint64_t);
      }

   return cursor;
   }