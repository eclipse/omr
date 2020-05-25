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

#pragma csect(CODE,"TRLINKAGEBase#C")
#pragma csect(STATIC,"TRLINKAGEBase#S")
#pragma csect(TEST,"TRLINKAGEBase#T")

// See also S390Linkage2.cpp which contains more S390 Linkage
// implementations (primarily System Linkage and derived classes).

#include "codegen/Linkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/SystemLinkagezOS.hpp"


#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

extern TR::Instruction* generateS390ImmToRegister(TR::CodeGenerator * cg, TR::Node * node, TR::Register * targetRegister,
   intptr_t value, TR::Instruction * cursor);
extern bool storeHelperImmediateInstruction(TR::Node * valueChild, TR::CodeGenerator * cg, bool isReversed, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::MemoryReference * mf);

////////////////////////////////////////////////////////////////////////////////
// TR::S390Linkage member functions
////////////////////////////////////////////////////////////////////////////////

OMR::Z::Linkage::Linkage(TR::CodeGenerator * codeGen)
   : OMR::Linkage(codeGen),
      _explicitLinkageType(TR_S390LinkageDefault), _linkageType(TR_None), _stackSizeCheckNeeded(true), _raContextSaveNeeded(true),
      _integerReturnRegister(TR::RealRegister::NoReg),
      _floatReturnRegister(TR::RealRegister::NoReg),
      _doubleReturnRegister(TR::RealRegister::NoReg),
      _longLowReturnRegister(TR::RealRegister::NoReg),
      _longHighReturnRegister(TR::RealRegister::NoReg),
      _longReturnRegister(TR::RealRegister::NoReg),
      _stackPointerRegister(TR::RealRegister::NoReg),
      _entryPointRegister(TR::RealRegister::NoReg),
      _litPoolRegister(TR::RealRegister::NoReg),
      _staticBaseRegister(TR::RealRegister::NoReg),
      _privateStaticBaseRegister(TR::RealRegister::NoReg),
      _returnAddrRegister(TR::RealRegister::NoReg),
      _raContextRestoreNeeded(true),
      _firstSaved(TR::RealRegister::NoReg),
      _lastSaved(TR::RealRegister::NoReg),
      _lastPrologueInstr(NULL),
      _firstPrologueInstr(NULL)
   {
   int32_t i;
   self()->setProperties(0);
   for (i=0; i<TR::RealRegister::NumRegisters;i++)
      {
      self()->setRegisterFlags(REGNUM(i),0);
      }
   }


/**
 * Get or create the TR::Linkage object that corresponds to the given linkage
 * convention.
 * Even though this method is common, its implementation is machine-specific.
 */
OMR::Z::Linkage::Linkage(TR::CodeGenerator * codeGen,TR_S390LinkageConventions elc, TR_LinkageConventions lc)
   : OMR::Linkage(codeGen),
      _explicitLinkageType(elc), _linkageType(lc), _stackSizeCheckNeeded(true), _raContextSaveNeeded(true),
      _integerReturnRegister(TR::RealRegister::NoReg),
      _floatReturnRegister(TR::RealRegister::NoReg),
      _doubleReturnRegister(TR::RealRegister::NoReg),
      _longLowReturnRegister(TR::RealRegister::NoReg),
      _longHighReturnRegister(TR::RealRegister::NoReg),
      _longReturnRegister(TR::RealRegister::NoReg),
      _stackPointerRegister(TR::RealRegister::NoReg),
      _entryPointRegister(TR::RealRegister::NoReg),
      _litPoolRegister(TR::RealRegister::NoReg),
      _staticBaseRegister(TR::RealRegister::NoReg),
      _privateStaticBaseRegister(TR::RealRegister::NoReg),
      _returnAddrRegister(TR::RealRegister::NoReg),
      _raContextRestoreNeeded(true),
      _firstSaved(TR::RealRegister::NoReg),
      _lastSaved(TR::RealRegister::NoReg),
      _lastPrologueInstr(NULL),
      _firstPrologueInstr(NULL)
   {
   int32_t i;
   self()->setProperties(0);
   for (i=0; i<TR::RealRegister::NumRegisters;i++)
      {
      self()->setRegisterFlags(REGNUM(i),0);
      }
   }


void
OMR::Z::Linkage::markPreservedRegsInDep(TR::RegisterDependencyConditions * deps)
   {
   for (int32_t curReg = TR::RealRegister::FirstGPR; curReg <= TR::RealRegister::LastFPR; curReg++)
      {
      if (self()->getPreserved(REGNUM(curReg)) &&
          deps->searchPostConditionRegister(REGNUM(curReg)))
         {
         self()->getRealRegister(REGNUM(curReg))->setModified(true);
         }
      }

  // and the RAReg
  if (deps && deps->searchPostConditionRegister(self()->cg()->getReturnAddressRegister()))
     {
     self()->getRealRegister(self()->getReturnAddressRegister())->setModified(true);
     }
  }

void
OMR::Z::Linkage::markPreservedRegsInBlock(int32_t blockNum)
   {
   // kill RAReg
   self()->getRealRegister(self()->getReturnAddressRegister())->setModified(true);

   // catch blocks kill all preserved regs
   for (int32_t curReg = TR::RealRegister::FirstGPR; curReg <= TR::RealRegister::LastFPR; curReg++)
      {
      if (self()->getPreserved(REGNUM(curReg)))
         {
         self()->getRealRegister(REGNUM(curReg))->setModified(true);
         }
      }
   }

TR::Instruction *
OMR::Z::Linkage::loadUpArguments(TR::Instruction * cursor)
   {
   return NULL;
   }


void
OMR::Z::Linkage::removeOSCOnSavedArgument(TR::Instruction* instr, TR::Register* sReg, int32_t stackOffset)
   {
   bool          done=false;
   int32_t       windowSize=0;
   const int32_t maxWindowSize=10;

   // Start looking at the top of instruction stream body
   //
   TR::Instruction * current = instr->getNext();

   static char *disable = feGetEnv("TR_EnableRemoveOSC");
   if (!disable)
      return;

   if (self()->comp()->getOption(TR_TraceCG))
      traceMsg(self()->comp(), "%p SP OFFSET %d, SREG %p\n",instr, stackOffset, sReg);

   while ( (current != NULL) &&
            !current->isLabel()    &&
            !current->isCall()     &&
            !current->isBranchOp() &&
            windowSize < maxWindowSize && !done )
      {
      if (self()->comp()->getOption(TR_TraceCG))
         traceMsg(self()->comp(), "%p inspecting\n", current);

      if (current->isLoad())
         {
         TR::Register            *tReg    = ((TR::S390RXInstruction*)current)->getRegisterOperand(1);
         TR::MemoryReference *mr      = current->getMemoryReference();
         TR::SymbolReference     *symRef  = mr->getSymbolReference();
         TR::Symbol               *sym     = symRef->getSymbol();
         int32_t                 disp    = -1;

         //  Make sure the sym is a param
         //
         if (sym && sym->isRegisterMappedSymbol() && sym->getKind()==TR::Symbol::IsParameter)
            {
            disp = sym->castToRegisterMappedSymbol()->getOffset() + mr->getOffset();
            }

         TR::Instruction         *newInst = NULL;

         if (self()->comp()->getOption(TR_TraceCG))
            traceMsg(self()->comp(), "%p is load, mrOffset %i\n",current, disp);

         // Find a memRef that matches the store
         //
         if (disp == stackOffset)
            {
            TR::Instruction* prev = current->getPrev();
            TR::InstOpCode::Mnemonic opCode = current->getOpCodeValue();
            bool mustReplace = true;

            if (opCode == TR::InstOpCode::L)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::L %s,%d(SP) to TR::InstOpCode::LR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }

               mustReplace = false;   // If sReg = treg, just delete the redundent load
               }
            else if (opCode == TR::InstOpCode::LG)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LGR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LG %s,%d(SP) to TR::InstOpCode::LGR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }

               mustReplace = false;   // If sReg = treg, just delete the redundent load
               }
            else if (opCode == TR::InstOpCode::LE)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LER, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LE %s,%d(SP) to TR::InstOpCode::LER %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }

               mustReplace = false;   // If sReg = treg, just delete the redundent load
               }
            else if (opCode == TR::InstOpCode::LD)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LDR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LD %s,%d(SP) to TR::InstOpCode::LDR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }

               mustReplace = false;   // If sReg = treg, just delete the redundent load
               }
            else if (opCode == TR::InstOpCode::LT)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LTR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LT %s,%d(SP) to TR::InstOpCode::LTR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }
               }
            else if (opCode == TR::InstOpCode::LTG)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LTGR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LTG %s,%d(SP) to TR::InstOpCode::LTGR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }
               }
            else if (opCode == TR::InstOpCode::LGF)
               {
               newInst = generateRRInstruction(self()->cg(), TR::InstOpCode::LGFR, current->getNode(), tReg, sReg, prev);
               if (self()->comp()->getOption(TR_TraceCG))
                  {
                  traceMsg(self()->comp(), "\nProlog peeking changing TR::InstOpCode::LGF %s,%d(SP) to TR::InstOpCode::LGFR %s,%s : [%p] => [%p]\n",
                        tReg->getRegisterName(self()->comp()), stackOffset,
                        tReg->getRegisterName(self()->comp()), sReg->getRegisterName(self()->comp()), current, newInst);
                  }
               }

            // Replace if we have a new instruction
            //
            if (newInst &&
                performTransformation(self()->comp(), "O^O : Prolog peeking removing [%p]\n", current) )
               {
               self()->cg()->replaceInst(current, newInst);
               self()->cg()->deleteInst(current);

               if(!mustReplace && tReg == sReg)
                 {
                 self()->cg()->deleteInst(newInst);
                 if (self()->comp()->getOption(TR_TraceCG))
                    traceMsg(self()->comp(), "deleting instruction as sreg = treg\n");
                 }

               done = true;
               }
            }
         else if (current->matchesTargetRegister(sReg))
            {
            done = true;
            }
         }
      else if (current->matchesTargetRegister(sReg))
         {
         done = true;
         }

      current=current->getNext();
      windowSize++;
      }

   return;
   }



/**
 * This routine is called to save the arguments on stack when we generate
 * the prolog code.
 */
void *
OMR::Z::Linkage::saveArguments(void * cursor, bool genBinary, bool InPreProlog, int32_t frameOffset, List<TR::ParameterSymbol> *parameterList
   )
   {
   TR::RealRegister * stackPtr = self()->getNormalStackPointerRealRegister();
   const bool enableVectorLinkage = self()->cg()->getSupportsVectorRegisters();

   // for XPLink or FASTLINK, there are cases where the "normal" stack pointer is
   // not appropriate for where to save arguments in registers.
   bool useAlternateStackPointer = false;
   if (self()->isXPLinkLinkageType())
      {
      useAlternateStackPointer = true;
      }

   if (useAlternateStackPointer)
      {
      stackPtr = self()->getStackPointerRealRegister();
      }

   TR::ResolvedMethodSymbol * bodySymbol = self()->comp()->getJittedMethodSymbol();

   ListIterator<TR::ParameterSymbol> paramIterator((parameterList!=NULL) ? parameterList : &(bodySymbol->getParameterList()));

   TR::ParameterSymbol * paramCursor;

   TR::Node * firstNode = self()->comp()->getStartTree()->getNode();

   TR_BitVector globalAllocatedRegisters;
   TR_BitVector freeScratchable;

   int32_t busyMoves[5][32];
   int32_t busyIndex = 0, lastFreeIntArgIndex = 0, lastFreeFloatArgIndex = 0, lastFreeVectorArgIndex = 0, i1;

   uint32_t binLocalRegs = 0x1<<14;   // Binary pattern representing reg14 as free for local alloc

   int8_t gprSize = self()->machine()->getGPRSize();

   bool unconditionalSave  = false;

   // If we use preexistence or FSD or HCR, then we could be reverting back to the
   // interpreter by creating prePrologue snippets.  In such cases, we need
   // to pass any register arguments back onto the stack, even if they are
   // read-only.
   // For FSD, we need to store all parameters back onto the stack.
   unconditionalSave |= (InPreProlog && self()->comp()->cg()->mustGenerateSwitchToInterpreterPrePrologue());

   unconditionalSave |= (self()->isForceSaveIncomingParameters() != 0);

   int32_t lastReg = TR::RealRegister::LastFPR;

   // Keep a list of available scratch register
   //  -> reset means assigned
   //  -> set means free
   // Keep a list of global registers
   //
   if (enableVectorLinkage)
      {
      freeScratchable.init(TR::RealRegister::LastVRF + 1, self()->trMemory());
      globalAllocatedRegisters.init(TR::RealRegister::LastVRF + 1, self()->trMemory());
      }
   else
      {
      freeScratchable.init(TR::RealRegister::LastFPR + 1, self()->trMemory());
      globalAllocatedRegisters.init(TR::RealRegister::LastFPR + 1, self()->trMemory());
      }

   for (i1 = TR::RealRegister::FirstGPR; i1 <= lastReg; i1++)
      {
      // TODO: remove hard-coded GPR6, valid on zOS for non-XPLINK?
      bool gpr6Test = (i1 >= TR::RealRegister::GPR6);
      if ((gpr6Test || !self()->getPreserved(REGNUM(i1))) &&
         !self()->getIntegerArgument(REGNUM(i1)) &&
         !self()->getFloatArgument(REGNUM(i1)))
         {
         freeScratchable.set(i1);
         }
      }

   for (paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      int32_t ai = paramCursor->getAssignedGlobalRegisterIndex();
      int32_t ai_l = paramCursor->getAssignedLowGlobalRegisterIndex();  //  low reg of a pair

      // Contruct list of globally allocated registers
      //
      if (ai > 0)
         {
         globalAllocatedRegisters.set(ai);
         }
      if (ai_l > 0)
         {
         globalAllocatedRegisters.set(ai_l);
         }
      }

   if (self()->isSkipGPRsForFloatParms())
      { // set GPRs skipped for FPRs to free
      int32_t numIntArgs = -1, r;
      for (paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
         {
         TR::DataType type = paramCursor->getType();
         if (type.isFloatingPoint())
            {
            int32_t count = 0;
            if (type.getDataType() == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                || type.getDataType() == TR::DecimalDouble
#endif
                )
                count = (self()->cg()->comp()->target().is64Bit()) ? 1 : 2;
#ifdef J9_PROJECT_SPECIFIC
            else if (type.isLongDouble())
                count = (self()->cg()->comp()->target().is64Bit()) ? 2 : 4;
#endif
            else
                count = 1;

            for (int32_t i = 0; i < count; i++)
               {
               numIntArgs++;
               if (numIntArgs < self()->getNumIntegerArgumentRegisters())
                  {
                  r = self()->getIntegerArgumentRegister(numIntArgs);
                  freeScratchable.set(r); // GPR is free (due to hole)
                  }
               }
            }
         else
            {
            numIntArgs += (type.isInt64() && self()->cg()->comp()->target().is32Bit()) ? 2 : 1;
            }
         }
      }

   int32_t parmNum = 0;
   for (paramCursor = paramIterator.getFirst(); paramCursor != NULL; paramCursor = paramIterator.getNext())
      {
      parmNum++;
      int32_t lri = paramCursor->getLinkageRegisterIndex();                // linkage register
      int32_t ai = paramCursor->getAssignedGlobalRegisterIndex();          // global reg number
      TR::DataType dtype = paramCursor->getDataType();
      int32_t offset = paramCursor->getParameterOffset() - frameOffset;
      TR::SymbolReference * paramSymRef = NULL;
      int32_t namelen = 0;
      const char * param_name = NULL;

      param_name = paramCursor->getTypeSignature(namelen);

      // Treat GPR6 special on zLinux.  It is a linkage register but also preserved.
      // If it has no global reg number, locate it on local save area instead of register save area
      if (self()->cg()->comp()->target().isLinux() &&
          (lri > 0 && ai < 0 && (self()->getPreserved(REGNUM(lri + 3)) || dtype.isVector())))
          paramCursor->setParmHasToBeOnStack();

      if (self()->comp()->getOption(TR_TraceCG))
         {
         traceMsg(self()->comp(), "save argument: %s, lri: %d, ai: %d, offset: %d, isReferenced: %d, hasToBeOnStack: %d\n", param_name, lri, ai, offset, paramCursor->isReferencedParameter(),self()->hasToBeOnStack(paramCursor));
         }

      TR::InstOpCode::Mnemonic storeOpCode, loadOpCode;
      uint32_t opcodeMask = 0;
      bool secondStore = false;

      int32_t slotSize = paramCursor->getSize();

      if (self()->isSmallIntParmsAlignedRight())
         {
         // On both Linux and z/OS system linkages we spill both the clean and dirty bits of the incoming arguments to
         // the stack if needed. The `initParamOffset` API will ensure the parameters within the method are initialized
         // with offsets which point only to the clean data (no dirty bits loaded).
         slotSize = gprSize;
         }

      if (slotSize < 4)
         slotSize = 4;

      switch (slotSize)
         {
         case 1:
            storeOpCode = TR::InstOpCode::STC;
            loadOpCode = TR::InstOpCode::IC;
            break;
         case 2:
            storeOpCode = TR::InstOpCode::STH;
            loadOpCode = TR::InstOpCode::ICM;  // mask 3
            opcodeMask = 0x3;
            break;
         case 3:
            storeOpCode = TR::InstOpCode::STCM;  // mask 7
            loadOpCode = TR::InstOpCode::ICM;   // mask 7
            opcodeMask = 0x7;
            break;
         case 4:
            storeOpCode = TR::InstOpCode::ST;
            loadOpCode = TR::InstOpCode::L;
            break;
         case 8:
            if (self()->cg()->comp()->target().is64Bit())
               {
               storeOpCode = TR::InstOpCode::STG;
               loadOpCode = TR::InstOpCode::LG;
               }
            else
               {
               storeOpCode = TR::InstOpCode::ST;
               secondStore = true;
               loadOpCode = TR::InstOpCode::L;
               }
            break;
         case 16:
            storeOpCode = TR::InstOpCode::VST;
            loadOpCode = TR::InstOpCode::VL;
            break;
         }

      if (((self()->isSmallIntParmsAlignedRight() && paramCursor->getType().isIntegral()) ||
           (self()->isPadFloatParms() &&  paramCursor->getType().isFloatingPoint())) && (gprSize > paramCursor->getSize()))
         {
         offset = offset & ~(gprSize-1);
         }

      // Reset "stack" pointer - which could change below
      // with special register usage
      stackPtr = self()->getNormalStackPointerRealRegister();
      if (useAlternateStackPointer) stackPtr = self()->getStackPointerRealRegister();

      // +ve lri means a linkage register is being used
      //
      if (lri >= 0)
         {
         TR::DataType type = paramCursor->getType();
         TR::RealRegister::RegNum regNum;

         // Long argument is completely in regs
         //
         bool fullLong = (type.isInt64() && lri < self()->getNumIntegerArgumentRegisters() - 1);

         if (type.isFloatingPoint()) // float/double/long double
            {
            regNum = self()->getFloatArgumentRegister(lri);
            lastFreeFloatArgIndex = lri + 1;
#ifdef J9_PROJECT_SPECIFIC
            if (dtype == TR::DecimalLongDouble)
               {
               TR_ASSERT(  ((lri%2) == 0) , "LongDouble must be passed in legal FP reg pair!");
               TR_ASSERT(  (lri < (self()->getNumFloatArgumentRegisters() - 1)),"long double must be passed in FP reg pair in full");
               lastFreeFloatArgIndex++;
               }
#endif
            }
         else if(self()->cg()->comp()->target().isLinux() && dtype == TR::Aggregate)
           {
           TR_ASSERT( paramCursor->getSize()<=8, "Only aggregates of size 8 bytes or less are passed in registers");
           regNum = self()->getIntegerArgumentRegister(lri);
           lastFreeIntArgIndex = lri + 1;
           if(self()->cg()->comp()->target().is32Bit() && paramCursor->getSize() > 4)
             {
             fullLong = true;   // On 31 bit this larger aggregate will be treated like a 64 bit long
             lastFreeIntArgIndex++;
             }
           }
         else if (enableVectorLinkage && dtype.isVector()) // vector type
            {
            regNum = self()->getVectorArgumentRegister(lri);
            lastFreeVectorArgIndex = lri + 1;
            }
         else
            {
               regNum = self()->getIntegerArgumentRegister(lri);

            lastFreeIntArgIndex = lri + 1;
            if (fullLong && self()->cg()->comp()->target().is32Bit())
               {
               lastFreeIntArgIndex++;
               }
            }

         if (self()->comp()->getOption(TR_TraceCG))
         {
         traceMsg(self()->comp(), "save argument: %s, regNum: %d, \n", param_name, regNum);
         }
         // XPLINK(STOREARGS)
         if ( !paramCursor->isReferencedParameter() && !paramCursor->isParmHasToBeOnStack() &&
             (!unconditionalSave || (unconditionalSave && dtype != TR::Address)) )
            {
            freeScratchable.set(regNum);
            if (fullLong && self()->cg()->comp()->target().is32Bit())
               {
               freeScratchable.set(regNum + 1);
               }
#ifdef J9_PROJECT_SPECIFIC
            if (type.isLongDouble())
               freeScratchable.set(regNum + 2);  // 2nd of even-even FP pair
#endif

            continue;
            }

         bool skipLong = false;

         // Negative ai means the register is not a global, so store the argument
         // off to the stack
         //
         // If a parameter is not referenced in the method (i.e. not used),
         // then we do not have to store it back onto the stack if either:
         //     1) We are not reverting back to interpreter.
         //         (For more details, see unconditionalSave comment above.)
         //     2) We are reverting back to interpreter, and the parameter's
         //        type is not an address.
         //     3) FSD, all parms have to be on stack, so that if method
         //        has breakpoint set, all values, including unreferenced parms
         //        will be updated by GC.
         //
         // For static zLinux, if you are a leaf routine, you cannot store anything on to the stack.
         // This query here is actually not ideal.  originally isReferencedParameter (which actually means isReferencedParameterAtIlGenTime) was replaced with isParmHasToBeOnStack, which would make sense.
         // However, this broke some java modes (OSR,HCR).  Likely, those modes should rely on 'unconditionalSave' to ensure we always store out to the stack for whenever we need all variables on the stack.
         // This investigation will be a future todo .

         if ((ai < 0 && paramCursor->isReferencedParameter()) || self()->hasToBeOnStack(paramCursor) || unconditionalSave)
            {
            bool regIsUsed = false;
            switch (dtype)
               {
               case TR::Int8:
               case TR::Int16:
               case TR::Int32:
               case TR::Address:
               case TR::Int64:
               case TR::Aggregate:     // Should only happen on zLinux
                  {
                  if (genBinary)
                     {
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(storeOpCode, (uint8_t *) cursor,
                                          self()->getRealRegister(regNum), offset, self()->cg());
                     }
                  else
                     {
                     TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset, self()->cg(), param_name);

                     if (storeOpCode == TR::InstOpCode::STCM)
                        cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::STCM, firstNode, self()->getRealRegister(regNum),
                                    opcodeMask, mr, (TR::Instruction *) cursor);

                     else
                        cursor = generateRXInstruction(self()->cg(), storeOpCode, firstNode, self()->getRealRegister(regNum),
                                             mr, (TR::Instruction *) cursor);



                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                     }
                  }

                  if (secondStore &&
                      fullLong &&
                      self()->cg()->comp()->target().is32Bit())
                     {
                     if (genBinary)
                        {
                        cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::ST, (uint8_t *) cursor, self()->getRealRegister(regNum),
                                             offset, self()->cg());
                        }
                     else
                        {
                        TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset + 4, self()->cg(), param_name);
                        cursor =  generateRXInstruction(self()->cg(), TR::InstOpCode::ST, firstNode, self()->getRealRegister(REGNUM(regNum + 1)),
                                             mr, (TR::Instruction *) cursor);
                        ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                        if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                           self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                        }

                     // argument stored, so regnum is free to be used as a scratch reg
                     //
                     if (ai < 0)
                        {
                        freeScratchable.set(regNum + 1);
                        }
                     }
                  break;
               case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
               case TR::DecimalFloat:
#endif
                  if (genBinary)
                     {
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::STE, (uint8_t *) cursor, self()->getRealRegister(regNum),
                                          offset, self()->cg());
                     }
                  else
                     {
                     TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset, self()->cg(), param_name);
                     cursor =  generateRXInstruction(self()->cg(), TR::InstOpCode::STE, firstNode, self()->getRealRegister(regNum),
                                          mr, (TR::Instruction *) cursor);
                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                     }
                  break;
               case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
               case TR::DecimalDouble:
#endif
                  if (genBinary)
                     {
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::STD, (uint8_t *) cursor, self()->getRealRegister(regNum),
                                          offset, self()->cg());
                     }
                  else
                     {
                     TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset, self()->cg(), param_name);
                     cursor = (void *) generateRXInstruction(self()->cg(), TR::InstOpCode::STD, firstNode, self()->getRealRegister(regNum),
                                          mr, (TR::Instruction *) cursor);
                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                     }
                  break;
#ifdef J9_PROJECT_SPECIFIC
               case TR::DecimalLongDouble:
                  if (genBinary)
                     {
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::STD, (uint8_t *) cursor, self()->getRealRegister(regNum),
                                          offset, self()->cg());
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::STD, (uint8_t *) cursor, self()->getRealRegister(REGNUM(regNum+2)),
                                          offset+8, self()->cg());
                     }
                  else
                     {
                     TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset, self()->cg(), param_name);
                     TR::MemoryReference * hiMR = generateS390MemoryReference(*mr, 8, self()->cg());

                     cursor = (void *) generateRXInstruction(self()->cg(), TR::InstOpCode::STD, firstNode, self()->getRealRegister(regNum),
                                          mr, (TR::Instruction *) cursor);
                     cursor = (void *) generateRXInstruction(self()->cg(), TR::InstOpCode::STD, firstNode, self()->getRealRegister(REGNUM(regNum+2)),
                                          hiMR, (TR::Instruction *) cursor);

                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(REGNUM(regNum+2)))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(REGNUM(regNum+2)), offset+8);
                     }
                  break;
#endif
               case TR::VectorInt8:
               case TR::VectorInt16:
               case TR::VectorInt32:
               case TR::VectorInt64:
               case TR::VectorDouble:
                  if (genBinary)
                     {
                     cursor =  (void *) TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::VST, (uint8_t *) cursor, self()->getRealRegister(regNum),
                                          offset, self()->cg());
                     }
                  else
                     {
                     TR::MemoryReference* mr = generateS390MemoryReference(stackPtr, offset, self()->cg(), param_name);
                     cursor =  generateRXInstruction(self()->cg(), TR::InstOpCode::VST, firstNode, self()->getRealRegister(regNum),
                                          mr, (TR::Instruction *) cursor);
                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     if (!InPreProlog && !globalAllocatedRegisters.isSet(regNum))
                        self()->removeOSCOnSavedArgument((TR::Instruction *)cursor, self()->getRealRegister(regNum), offset);
                     }
                  break;

               } //switch(dtype)

            // argument stored, so regnum is free to be used as a scratch reg
            //
            if (ai < 0 )
               {
               freeScratchable.set(regNum);
#ifdef J9_PROJECT_SPECIFIC
               if (dtype == TR::DecimalLongDouble)
                  freeScratchable.set(regNum+2);
#endif
               }
            }

         // Global register is allocated to this argument, try and move arg to global
         //
         if (ai>=0 && !InPreProlog)
            {
            // Argument regNum is not the same as global reg so add in some register moves to fix up
            // Or we have long reg on 31bir, so we need to build the 64bit register from the two
            // arguments (reg + reg) or (reg + mem)
            // also need to take care of longdouble/complex types which takes 2 or more slots
            if (regNum != ai || (dtype == TR::Int64 && self()->cg()->comp()->target().is32Bit()))
               {
               //  Global register is available as scratch reg, so make the move
               //
               if (freeScratchable.isSet(ai))
                  {
                  if (dtype == TR::Float
                      || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                      || dtype == TR::DecimalFloat
                      || dtype == TR::DecimalDouble
#endif
                      )
                     {
                     cursor = generateRRInstruction(self()->cg(),  TR::InstOpCode::LDR, firstNode, self()->getRealRegister(REGNUM(ai)),
                                    self()->getRealRegister(regNum), (TR::Instruction *) cursor);

                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     freeScratchable.reset(ai);
                     freeScratchable.set(regNum);
                     }
#ifdef J9_PROJECT_SPECIFIC
                  else if (dtype == TR::DecimalLongDouble)
                     {
                     int32_t ai_l = paramCursor->getAssignedLowGlobalRegisterIndex();  //  low reg of a pair
                     TR_ASSERT( (ai_l == (ai+2)),"global RA incorrect for long double params");
                     cursor = generateRRInstruction(self()->cg(),  TR::InstOpCode::LXR, firstNode, self()->cg()->allocateFPRegisterPair(self()->getRealRegister(REGNUM(ai_l)), self()->getRealRegister(REGNUM(ai))),
                        self()->cg()->allocateFPRegisterPair(self()->getRealRegister(REGNUM(regNum+2)),self()->getRealRegister(REGNUM(regNum))), (TR::Instruction *) cursor);

                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     freeScratchable.reset(ai);
                     freeScratchable.reset(ai_l);
                     freeScratchable.set(regNum);
                     freeScratchable.set(regNum+2);
                     }
#endif
                  else
                     {
                     if (dtype == TR::Int64 && self()->cg()->comp()->target().is32Bit())
                        {
                        cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::SLLG, firstNode, self()->getRealRegister(REGNUM(ai)),
                                    self()->getRealRegister(regNum), 32, (TR::Instruction *) cursor);

                        if (regNum != ai)
                           {
                           freeScratchable.set(regNum);
                           }
                        freeScratchable.reset(ai);

                        // Figure out if the low word is passed in a reg or in memory
                        if (fullLong)
                           {
                           cursor = generateRRInstruction(self()->cg(), TR::InstOpCode::LR, firstNode, self()->getRealRegister(REGNUM(ai)),
                                    self()->getRealRegister(REGNUM(regNum + 1)), (TR::Instruction *) cursor);

                           freeScratchable.set(regNum + 1);
                           }
                        else
                           {
                           cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::L, firstNode, self()->getRealRegister(REGNUM(ai)),
                                    generateS390MemoryReference(stackPtr, offset + 4, self()->cg()), (TR::Instruction *) cursor);
                           }

                        // Don't do anything for the low word later on as it was handled here
                        //
                        skipLong = true;
                        }
                     else
                        {
                        cursor = generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), firstNode, self()->getRealRegister(REGNUM(ai)),
                           self()->getRealRegister(regNum), (TR::Instruction *) cursor);

                        freeScratchable.reset(ai);
                        freeScratchable.set(regNum);
                        }

                     ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                     }
                  }
               // Global register is not available, remember this for later
               //
               else
                  {
                  // Deal with the fact that the global register is not available
                  // We have to handle the high and low word.  We use two entries
                  // in the busyMoves to represent high and low word.
                  //
                  if (dtype == TR::Int64 && self()->cg()->comp()->target().is32Bit())
                     {
                     if (fullLong)
                        {
                        busyMoves[0][busyIndex] = regNum;
                        busyMoves[1][busyIndex] = ai;
                        busyMoves[2][busyIndex] = 6;
                        busyIndex++;

                        busyMoves[0][busyIndex] = regNum+1;
                        busyMoves[1][busyIndex] = ai;
                        busyMoves[2][busyIndex] = 0;
                        busyIndex++;
                        }
                     else
                        {
                        busyMoves[0][busyIndex] = regNum;
                        busyMoves[1][busyIndex] = ai;
                        busyMoves[2][busyIndex] = 7;
                        busyIndex++;

                        busyMoves[0][busyIndex] = offset + 4;
                        busyMoves[1][busyIndex] = ai;
                        busyMoves[2][busyIndex] = 0;
                        busyIndex++;
                        }
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = regNum;
                     busyMoves[1][busyIndex] = ai;
                     if (dtype == TR::Float
                         || dtype == TR::Double
#ifdef J9_PROJECT_SPECIFIC
                         || dtype == TR::DecimalFloat
                         || dtype == TR::DecimalDouble
#endif
                         )
                        {
                        busyMoves[2][busyIndex] = 5;
                        }
                     else
                        {
                        busyMoves[2][busyIndex] = 0;
                        }
                     busyIndex++;
                     }
                  }  // if ai is free scrachable or else..
               } // if regNum !=ai
            } // if ai >=0

         int32_t ai_l = 0;
         if (!skipLong)
            ai_l = paramCursor->getAssignedLowGlobalRegisterIndex();
         if (ai_l>=0 && !InPreProlog && !skipLong)
            {
            // Some linkages allow for the low half of the argument to be passed
            // on the stack while the high work is passed in regs
            //
            // cases that needs reg pairs:
            //  - 64bit int in 32bit mode
            //  - complex:  can pass real part in FP reg (or FP reg pair for complex long double) and imag part on stack
            //      - complex float / double
            //      - complex long double
#ifdef J9_PROJECT_SPECIFIC
            if (dtype == TR::DecimalLongDouble )  // if long int lower word pass in stack ...
               {
               TR_ASSERT(  ai_l == (ai+2), "global RA incorrect for long double param");
               // should be handled by the high word part case already.
               }
            else
#endif
                 if (!fullLong)  // if long int lower word pass in stack ...
               {
               if (freeScratchable.isSet(ai_l))
                  {
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(), firstNode, self()->getRealRegister(REGNUM(ai_l)),
                             generateS390MemoryReference(stackPtr, offset + 4, self()->cg()), (TR::Instruction *) cursor);

                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  freeScratchable.reset(ai_l);
                  }
               // Global register is not available, remember this for later
               //
               else
                  {
                  busyMoves[0][busyIndex] = offset + 4;
                  busyMoves[1][busyIndex] = ai_l;
                  busyMoves[2][busyIndex] = 1;
                  busyMoves[3][busyIndex] = TR::InstOpCode::getLoadOpCode();
                  busyMoves[4][busyIndex] = 0;
                  busyIndex++;
                  }

               }
            // Argument regNum is not the same as global reg
            // so add in some register moves to fix up
            //
#ifdef J9_PROJECT_SPECIFIC
            else if (dtype != TR::DecimalLongDouble && (regNum+1) != ai_l)
               {
               //  Global register is available as scratch reg, so make the move
               //
               if (freeScratchable.isSet(ai_l))
                  {
                  cursor = generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), firstNode,
                                   self()->getRealRegister(REGNUM(ai_l)),
                                   self()->getRealRegister(REGNUM(regNum + 1)), (TR::Instruction *) cursor);

                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  freeScratchable.reset(ai_l);
                  freeScratchable.set(regNum + 1);
                  }
               // Global register is not available, remember this for later
               //
               else
                  {
                  busyMoves[0][busyIndex] = regNum + 1;
                  busyMoves[1][busyIndex] = ai_l;
                  busyMoves[2][busyIndex] = 0;
                  busyIndex++;
                  }
               }
#endif
            } // if ai_l >=0
         }  // if lri >= 0
      // lri<0: arg was passed in memory, but ai >= 0 means arg was assigned a global reg.
      //
      else if (ai>=0 && !InPreProlog)
         {
         int32_t ai_l = paramCursor->getAssignedLowGlobalRegisterIndex();
         switch (dtype)
            {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
            case TR::Address:
            case TR::Int64:
            case TR::Aggregate:     // Should only happen on zLinux
               if (freeScratchable.isSet(ai))
                  {
                  TR::MemoryReference * mr = generateS390MemoryReference(stackPtr, offset, self()->cg());

                  if (loadOpCode == TR::InstOpCode::ICM)
                     cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::ICM, firstNode, self()->getRealRegister(REGNUM(ai)), opcodeMask, mr, (TR::Instruction *) cursor);
                  else
                     cursor = generateRXInstruction(self()->cg(), loadOpCode, firstNode, self()->getRealRegister(REGNUM(ai)),
                             mr, (TR::Instruction *) cursor);


                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  freeScratchable.reset(ai);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 1;
                  busyMoves[3][busyIndex] = loadOpCode;
                  busyMoves[4][busyIndex] = opcodeMask;
                  busyIndex++;
                  }
               break;
            case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
            case TR::DecimalFloat:
#endif
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LE, firstNode, self()->getRealRegister(REGNUM(ai)),
                              generateS390MemoryReference(stackPtr, offset, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);

                  freeScratchable.reset(ai);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 3;
                  busyIndex++;
                  }
               break;
            case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
            case TR::DecimalDouble:
#endif
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(ai)),
                              generateS390MemoryReference(stackPtr, offset, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);

                  freeScratchable.reset(ai);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 4;
                  busyIndex++;
                  }
               break;
#ifdef J9_PROJECT_SPECIFIC
            case TR::DecimalLongDouble:
               TR_ASSERT( (ai_l == (ai+2) ),"global RA incorrect for long double param");
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(ai)),
                              generateS390MemoryReference(stackPtr, offset, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(ai_l)),
                              generateS390MemoryReference(stackPtr, offset+8, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);

                  freeScratchable.reset(ai);
                  freeScratchable.reset(ai_l);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 9;  // long double mem to reg copy
                  busyIndex++;
                  }
               break;
#endif
             default:
               TR_ASSERT(0, "saveArguments: unhandled parameter type");
            } //switch(dtype)
         }  // if lri >= 0 else if lri <0 ...
      }  // For loop parmCursor

   // Reset "stack" pointer - which could have changed
   // with special register usage.
   stackPtr = self()->getNormalStackPointerRealRegister();
   if (useAlternateStackPointer) stackPtr = self()->getStackPointerRealRegister();

   for (i1=lastFreeIntArgIndex; i1<self()->getNumIntegerArgumentRegisters(); i1++)
      freeScratchable.set(self()->getIntegerArgumentRegister(i1));

   for (i1=lastFreeFloatArgIndex; i1<self()->getNumFloatArgumentRegisters(); i1++)
      freeScratchable.set(self()->getFloatArgumentRegister(i1));

   if (enableVectorLinkage)
      {
      for (i1=lastFreeVectorArgIndex; i1<self()->getNumVectorArgumentRegisters(); i1++)
         freeScratchable.set(self()->getVectorArgumentRegister(i1));
      }

   bool     freeMore = true;
   int32_t  numMoves = busyIndex;

   while (freeMore && numMoves>0)
      {
      freeMore = false;
      for (i1=0; i1<busyIndex; i1++)
         {
         int32_t source = busyMoves[0][i1];
         int32_t target = busyMoves[1][i1];
         if (!(target<0) && (freeScratchable.isSet(target) ||
              (source==target && busyMoves[2][i1]==6) || (source==target && busyMoves[2][i1]==7)))
            {
            switch(busyMoves[2][i1])
               {
               case 0: // Reg 2 Reg
                  {
                  cursor = generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), firstNode, self()->getRealRegister(REGNUM(target)),
                     self()->getRealRegister(REGNUM(source)), (TR::Instruction *) cursor);

                  freeScratchable.set(source);
                  break;
                  }
               case 1: // Int or Addr  Reg from Stack Slot
                  {
                  TR::MemoryReference *mr = generateS390MemoryReference(stackPtr, source, self()->cg());

                  if ((TR::InstOpCode::Mnemonic) busyMoves[3][i1] == TR::InstOpCode::ICM)
                     cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::ICM, firstNode, self()->getRealRegister(REGNUM(target)), busyMoves[4][i1], mr, (TR::Instruction *) cursor);

                  else if ((TR::InstOpCode::Mnemonic) busyMoves[3][i1] == TR::InstOpCode::LFH)
                     cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LFH, firstNode, self()->getRealRegister(REGNUM(target)), mr, (TR::Instruction *) cursor);

                  else
                     cursor = generateRXInstruction(self()->cg(), (TR::InstOpCode::Mnemonic) busyMoves[3][i1], firstNode, self()->getRealRegister(REGNUM(target)), mr, (TR::Instruction *) cursor);

                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  break;
                  }
               case 3: // Floats
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LE, firstNode, self()->getRealRegister(REGNUM(target)),
                              generateS390MemoryReference(stackPtr, source, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  break;
               case 4:  // Doubles
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(target)),
                              generateS390MemoryReference(stackPtr, source, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  break;
               case 5:  // Floats and Doubles
                  cursor = generateRRInstruction(self()->cg(),  TR::InstOpCode::LDR, firstNode, self()->getRealRegister(REGNUM(target)),
                              self()->getRealRegister(REGNUM(source)), (TR::Instruction *) cursor);
                  freeScratchable.set(source);
                  break;
               case 6: // 64bit reg on 31bit
                  cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::SLLG, firstNode, self()->getRealRegister(REGNUM(target)),
                                    self()->getRealRegister(REGNUM(source)), 32, (TR::Instruction *) cursor);
                  busyMoves[0][i1] = busyMoves[1][i1] = -1;
                  numMoves--;
                  if (source != target)
                     {
                     freeScratchable.set(source);
                     }

                  i1++;
                  source = busyMoves[0][i1];
                  target = busyMoves[1][i1];
                  cursor = generateRRInstruction(self()->cg(),  TR::InstOpCode::LR, firstNode, self()->getRealRegister(REGNUM(target)),
                              self()->getRealRegister(REGNUM(source)), (TR::Instruction *) cursor);
                  freeScratchable.set(source);
                  break;
               case 7: // 64bit reg on 31bit
                  cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::SLLG, firstNode, self()->getRealRegister(REGNUM(target)),
                                    self()->getRealRegister(REGNUM(source)), 32, (TR::Instruction *) cursor);
                  busyMoves[0][i1] = busyMoves[1][i1] = -1;
                  numMoves--;
                  if (source != target)
                     {
                     freeScratchable.set(source);
                     }

                  i1++;
                  source = busyMoves[0][i1];
                  target = busyMoves[1][i1];
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::L, firstNode, self()->getRealRegister(REGNUM(target)),
                              generateS390MemoryReference(stackPtr, source, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  break;
               case 9:  // LongDouble
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(target)),
                              generateS390MemoryReference(stackPtr, source, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, firstNode, self()->getRealRegister(REGNUM(target+2)),
                              generateS390MemoryReference(stackPtr, source+8, self()->cg()), (TR::Instruction *) cursor);
                  ((TR::Instruction*)cursor)->setBinLocalFreeRegs(binLocalRegs);
                  freeScratchable.reset(target+2);
                  busyMoves[0][i1] = busyMoves[1][i1] = -1;
                  numMoves--;
                  break;
               case 10:  // LongDouble
                  cursor = generateRRInstruction(self()->cg(),  TR::InstOpCode::LXR, firstNode,
                              self()->cg()->allocateFPRegisterPair(self()->getRealRegister(REGNUM(target+2)),self()->getRealRegister(REGNUM(target))),
                              self()->cg()->allocateFPRegisterPair(self()->getRealRegister(REGNUM(source+2)),self()->getRealRegister(REGNUM(source))),
                              (TR::Instruction *) cursor);
                  freeScratchable.set(source);
                  freeScratchable.set(source+2);
                  freeScratchable.reset(target+2);
                  busyMoves[0][i1] = busyMoves[1][i1] = -1;
                  numMoves--;
                  break;
               }

            freeScratchable.reset(target);
            freeMore = true;
            busyMoves[0][i1] = busyMoves[1][i1] = -1;
            numMoves--;
            }
         }
      }

   TR_ASSERT( numMoves<=0,"Circular argument register dependency can and should be avoided.");

   return cursor;
   }

TR::InstOpCode::Mnemonic
OMR::Z::Linkage::getOpCodeForLinkage(TR::Node * child, bool isStore, bool isRegReg)
   {
       bool isWiden = self()->isNeedsWidening() ;
       switch (child->getDataType())
         {
         case TR::Int64:
            if (self()->cg()->comp()->target().is32Bit())
               return (isStore)? TR::InstOpCode::STM : (isRegReg)? TR::InstOpCode::LR : TR::InstOpCode::LM;
            else
               return (isStore)? TR::InstOpCode::STG : (isRegReg)? TR::InstOpCode::LGR : TR::InstOpCode::LG;
            break;
         case TR::Address:
              return (isStore)? TR::InstOpCode::getStoreOpCode() : (isRegReg)? TR::InstOpCode::getLoadRegOpCode() : TR::InstOpCode::getLoadOpCode();
            break;
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (self()->cg()->comp()->target().is32Bit())
              return (isStore)? TR::InstOpCode::ST : (isRegReg)? TR::InstOpCode::LR : TR::InstOpCode::L;
            else
              return (isStore)? (isWiden)? TR::InstOpCode::STG : TR::InstOpCode::ST : (isRegReg)? (isWiden)? TR::InstOpCode::LGFR : TR::InstOpCode::LGR : (isWiden)? TR::InstOpCode::LGF: TR::InstOpCode::L;
         case TR::Aggregate: // can get here for aggregates with register size
            if (self()->cg()->comp()->target().is32Bit())
              return (isStore)? TR::InstOpCode::ST : (isRegReg)? TR::InstOpCode::LR : TR::InstOpCode::L;
            else
              return (isStore)? TR::InstOpCode::STG : (isRegReg)? TR::InstOpCode::LGR : TR::InstOpCode::LG;
         case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalFloat:
#endif
              return (isStore)? TR::InstOpCode::STE : (isRegReg)? TR::InstOpCode::LER : TR::InstOpCode::LE;
         case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalDouble:
#endif
              return (isStore)? TR::InstOpCode::STD : (isRegReg)? TR::InstOpCode::LDR : TR::InstOpCode::LD;
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalLongDouble:
              return (isStore)? TR::InstOpCode::STD : (isRegReg)? TR::InstOpCode::LXR : TR::InstOpCode::LD;
#endif
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
         case TR::VectorDouble:
              return (isStore)? TR::InstOpCode::VST : (isRegReg)? TR::InstOpCode::VLR : TR::InstOpCode::VL;
         }
      return TR::InstOpCode::NOP;
   }

TR::InstOpCode::Mnemonic
OMR::Z::Linkage::getRegCopyOpCodeForLinkage(TR::Node * child)
   {
   return self()->getOpCodeForLinkage(child, false,true);
   }

TR::InstOpCode::Mnemonic
OMR::Z::Linkage::getStoreOpCodeForLinkage(TR::Node * child)
   {
   return self()->getOpCodeForLinkage(child, true,false);
   }

TR::InstOpCode::Mnemonic
OMR::Z::Linkage::getLoadOpCodeForLinkage(TR::Node * child)
   {
   return self()->getOpCodeForLinkage(child, false,false);
   }

TR::Register *
OMR::Z::Linkage::copyArgRegister(TR::Node * callNode, TR::Node * child, TR::Register * argRegister)
   {
   TR::Register * tempRegister=NULL;
   TR::InstOpCode::Mnemonic copyOpCode = self()->getRegCopyOpCodeForLinkage(child);
   TR::Instruction * cursor = NULL;

   TR_Debug * debugObj = self()->cg()->getDebug();
   char * REG_PARAM = "LR=Reg_param";
   if (self()->cg()->comp()->target().is32Bit() && child->getDataType() == TR::Int64 && !argRegister->getRegisterPair())
      {
      TR::Register * tempRegH = self()->cg()->allocateRegister();
      TR::Register * tempRegL = self()->cg()->allocateRegister();

      cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::SRLG,
          callNode, tempRegH, argRegister, 32);
      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RSInstruction(cursor), REG_PARAM);
         }

      cursor = generateRRInstruction(self()->cg(), copyOpCode,
        callNode, tempRegL, argRegister);
      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RRInstruction(cursor), REG_PARAM);
         }

      argRegister = self()->cg()->allocateConsecutiveRegisterPair(tempRegL, tempRegH);
      self()->cg()->stopUsingRegister(argRegister);
      }
   else if (!self()->cg()->canClobberNodesRegister(child, 0))
      {
      if (argRegister->containsCollectedReference())
         {
         tempRegister = self()->cg()->allocateCollectedReferenceRegister();
         }
      else if (argRegister->getKind() == TR_FPR)
         {
         TR_ASSERT(argRegister->getRegisterPair() == NULL, "No longer should have regpair arg here");
         if ( argRegister->getRegisterPair() == NULL )
            tempRegister = self()->cg()->allocateRegister(TR_FPR);
         }
      else
         {
         tempRegister = self()->cg()->allocateRegister();
         }

         cursor = generateRRInstruction(self()->cg(), copyOpCode, callNode, tempRegister, argRegister);

      if ((argRegister->getKind() == TR_FPR) && ( argRegister->getRegisterPair() != NULL ))
            self()->cg()->stopUsingRegister(tempRegister);

      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RRInstruction(cursor), REG_PARAM);
         }
      self()->cg()->stopUsingRegister(argRegister);
      argRegister = tempRegister;
      }
   else if (copyOpCode == TR::InstOpCode::LGFR)
      {
      tempRegister = self()->cg()->allocateRegister();
      cursor = generateRRInstruction(self()->cg(), copyOpCode, callNode, tempRegister, argRegister);
      if (debugObj)
         {
         debugObj->addInstructionComment(toS390RRInstruction(cursor), REG_PARAM);
         }

      self()->cg()->stopUsingRegister(argRegister);
      argRegister = tempRegister;
      }

   return argRegister;
   }


TR::Register *
OMR::Z::Linkage::pushLongArg32(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs, int32_t numFloatArgs, int32_t * stackOffsetPtr,TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister)
   {
   int8_t argSize = 8;

   argRegister = self()->cg()->evaluate(child);
   self()->cg()->decReferenceCount(child);

   argRegister = self()->copyArgRegister(callNode, child, argRegister);

   bool isStorePair = self()->isAllParmsOnStack();
   bool isStoreOnlyLow = isStorePair;
   TR::Register * argRegisterHigh=argRegister->getRegisterPair()->getHighOrder();
   TR::Register * argRegisterLow =argRegister->getRegisterPair()->getLowOrder();

   TR::RealRegister::RegNum argRegNum = TR::RealRegister::NoReg;

   switch (callNode->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
         isStorePair = true;
         break;
#endif
      default:
         {
         argRegNum = self()->getLongHighArgumentRegister(numIntegerArgs);
         if (argRegNum != TR::RealRegister::NoReg)
            {
            dependencies->addPreCondition(argRegisterHigh, argRegNum);
            argRegNum = self()->getIntegerArgumentRegister(numIntegerArgs+1);
            if (argRegNum != TR::RealRegister::NoReg)
               {
               dependencies->addPreCondition(argRegisterLow, argRegNum);
               }
            else
               {
               isStoreOnlyLow = true;
               }
            }
         else
            {
            argRegNum = self()->getIntegerArgumentRegister(numIntegerArgs);
            if (argRegNum != TR::RealRegister::NoReg)
               {
               dependencies->addPreCondition(argRegisterHigh, argRegNum);
               }

            isStorePair = true;
            }
         }
         break;
      }

   if (!self()->isFirstParmAtFixedOffset())
      {
      *stackOffsetPtr -= (isStoreOnlyLow)? 4 : 8;
      }
   else if (self()->isFirstParmAtFixedOffset() && isStoreOnlyLow)
      {
      *stackOffsetPtr += 4;
      }

   if (isStorePair || isStoreOnlyLow)
      {
      TR::Register *stackRegister = self()->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
      TR::InstOpCode::Mnemonic storeOp = isStorePair? TR::InstOpCode::STM : TR::InstOpCode::ST;
      TR::Register * storeReg = isStorePair? argRegister : argRegisterLow;
      self()->storeArgumentOnStack(callNode, storeOp, storeReg, stackOffsetPtr, stackRegister);

      if (self()->isFirstParmAtFixedOffset())
         {
         *stackOffsetPtr += (isStoreOnlyLow)? 4 : 8;
         }
      else if (isStoreOnlyLow)
         {
         *stackOffsetPtr -= 4;
         }
      }
   else if (self()->isXPLinkLinkageType() || self()->isFastLinkLinkageType()) // call specific
      { // reserved space for this
      *stackOffsetPtr += argSize;
      }

   return argRegister;
   }

/**
 * Normally arguments for outgoing call are set via stack register except
 * for C++ FASTLINK where they are placed in callee frame via the NAB
 */
TR::Register *
OMR::Z::Linkage::getStackRegisterForOutgoingArguments(TR::Node *n, TR::RegisterDependencyConditions *dependencies)
   {
   TR::Register *sreg = NULL;

   TR::Register * stackRegister = dependencies->searchPreConditionRegister(self()->getNormalStackPointerRegister());
   if (stackRegister == NULL || (!self()->cg()->supportsJITFreeSystemStackPointer()))
      {
      stackRegister = self()->getNormalStackPointerRealRegister();
      }

   if (!self()->isFastLinkLinkageType())
      { // normal case
      sreg = stackRegister;
      }
   return sreg;
   }

TR::Register *
OMR::Z::Linkage::pushArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs, int32_t numFloatArgs, int32_t * stackOffsetPtr,TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister)
   {
   TR::DataType argType = child->getType();
   TR::DataType argDataType = argType.getDataType();
   int8_t regSize = (self()->cg()->comp()->target().is64Bit())? 8:4;
   int8_t argSize = 0;

   if (argType.isInt64()
       || argDataType == TR::Double
#ifdef J9_PROJECT_SPECIFIC
       || argDataType == TR::DecimalDouble
#endif
       )
      {
      argSize += self()->isTwoStackSlotsForLongAndDouble()? 2*regSize : 8;
      }
#ifdef J9_PROJECT_SPECIFIC
   else if (argType.isLongDouble())
       argSize += 16;
#endif
   else
       argSize += regSize;

   if (self()->cg()->comp()->target().is32Bit() && argType.isInt64())
      return self()->pushLongArg32(callNode, child, numIntegerArgs,  numFloatArgs, stackOffsetPtr, dependencies, argRegister);

   bool isStoreArg = self()->isAllParmsOnStack();
   bool isFloatShadowedToIntRegs = false;

   TR::RealRegister::RegNum argRegNum=TR::RealRegister::NoReg, argRegNum2=TR::RealRegister::NoReg;
   switch (callNode->getSymbol()->castToMethodSymbol()->getMandatoryRecognizedMethod())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
         isStoreArg = true;
         break;
#endif
      default:
         {
         // Determine the real register to store the argument.
         if (argType.isFloatingPoint())
            {
            argRegNum = self()->getFloatArgumentRegister(numFloatArgs);
#ifdef J9_PROJECT_SPECIFIC
            if (argType.isLongDouble())
               {
               argRegNum2 = self()->getFloatArgumentRegister(numFloatArgs+1);
               }
#endif
            }
         else
            argRegNum = self()->getIntegerArgumentRegister(numIntegerArgs);
         }
         break;
      }

   if ( (argRegNum == TR::RealRegister::NoReg)
#ifdef J9_PROJECT_SPECIFIC
        || (argType.isLongDouble() && argRegNum2 == TR::RealRegister::NoReg)
#endif
      )
      isStoreArg = true;
   else
      {
      if (argRegister == NULL)
         {
         argRegister = self()->cg()->evaluate(child);
         self()->cg()->decReferenceCount(child);
         argRegister = self()->copyArgRegister(callNode, child, argRegister);
         }

#ifdef J9_PROJECT_SPECIFIC
      // Set up register dependencies for the parameter registers.
      if (argType.isLongDouble())
         {
         TR::Register *argRegisterHigh = argRegister->getRegisterPair()->getHighOrder();
         TR::Register *argRegisterLow = argRegister->getRegisterPair()->getLowOrder();
         dependencies->addPreCondition(argRegisterHigh, argRegNum);
         dependencies->addPreCondition(argRegisterLow, argRegNum2);
         }
      else
#endif
         {
         dependencies->addPreCondition(argRegister, argRegNum);
         // See comment related to IntegerArgumentAddToPost in S390Linkage.hpp
         if (self()->getIntegerArgumentAddToPost(argRegNum) != 0)
            {
            dependencies->addPostCondition(argRegister, argRegNum);
            }
         }
      }

   if (argType.isFloatingPoint())
      {
      }

   if (!self()->isFirstParmAtFixedOffset())
      {
      *stackOffsetPtr -= argSize;
      }

   if (isStoreArg)
      {
      TR::InstOpCode::Mnemonic storeOp = self()->getStoreOpCodeForLinkage(child);

      // Try to generate MVHI / MVGHI first.
      if (argRegister == NULL && child->getOpCode().isLoadConst() &&
            (storeOp == TR::InstOpCode::ST || storeOp == TR::InstOpCode::STG))
         {
         TR::Register *stackRegister = self()->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
         if (storeHelperImmediateInstruction(child, self()->cg(), false, (storeOp == TR::InstOpCode::ST)?InstOpCode::MVHI:InstOpCode::MVGHI, callNode, generateS390MemoryReference(stackRegister, *stackOffsetPtr, self()->cg()) ))
            {
            argRegister = self()->cg()->evaluate(child);
            // no need to call copyArgRegister if dependence is not created
            }
         self()->cg()->decReferenceCount(child);
         }
      else if (argRegister == NULL)
         {
         argRegister = self()->cg()->evaluate(child);
         self()->cg()->decReferenceCount(child);
         // no need to call copyArgRegister if dependence is not created
         }

      if (argRegister != NULL)
         {
         TR::Register *stackRegister = self()->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
#ifdef J9_PROJECT_SPECIFIC
         if (child->getDataType()== TR::DecimalLongDouble)
            self()->storeLongDoubleArgumentOnStack(callNode, child->getDataType(), storeOp, argRegister, stackOffsetPtr, stackRegister);
         else
#endif
            self()->storeArgumentOnStack(callNode, storeOp, argRegister, stackOffsetPtr, stackRegister);
         }

      if (self()->isFirstParmAtFixedOffset())
         {
         *stackOffsetPtr +=  argSize;
         }
      }
   else if (self()->isXPLinkLinkageType() || self()->isFastLinkLinkageType()) // call specific
      {
      *stackOffsetPtr += argSize;
      }
   if (isFloatShadowedToIntRegs)
      {
      TR::Register *stackRegister = self()->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
      self()->loadIntArgumentsFromStack(callNode, dependencies, argDataType, (*stackOffsetPtr)-argSize, argSize, numIntegerArgs, stackRegister);
      }

   return argRegister;
   }


TR::Register *
OMR::Z::Linkage::pushJNIReferenceArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs, int32_t numFloatArgs, int32_t * stackOffsetPtr,TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister)
   {
   TR::Register * pushRegister = NULL;
   bool isNull = false;
   bool isCheckForNull = false;

   if (child->getOpCodeValue() == TR::loadaddr)
      {
      TR::SymbolReference * symRef = child->getSymbolReference();
      TR::StaticSymbol * sym = symRef->getSymbol()->getStaticSymbol();

      if (sym)
         {
         if (!sym->isAddressOfClassObject())
            isCheckForNull = true;
         }
      else
         {
         if (child->pointsToNull())
            isNull = true;
         else if (!child->pointsToNonNull())
            isCheckForNull = true;
         }
      }

    if (isNull)
       {
       pushRegister = self()->cg()->allocateRegister();
       generateRRInstruction(self()->cg(), TR::InstOpCode::getXORRegOpCode(), child, pushRegister, pushRegister);
       }
    else if (isCheckForNull)
       {
       TR::Register * addrReg = self()->cg()->evaluate(child);
       TR::Register * whatReg = self()->cg()->allocateCollectedReferenceRegister();
       TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(self()->cg());
       TR::LabelSymbol * nonNullLabel = generateLabelSymbol(self()->cg());

       generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(), child, whatReg,
          generateS390MemoryReference(addrReg, (int32_t) 0, self()->cg()));

       self()->cg()->decReferenceCount(child);
       if (!self()->cg()->canClobberNodesRegister(child, 0))
          {
          pushRegister = self()->cg()->allocateRegister();
          generateRRInstruction(self()->cg(), TR::InstOpCode::getLoadRegOpCode(), child, pushRegister, addrReg);
          }
       else
          {
          pushRegister = addrReg;
          }
       generateRIInstruction(self()->cg(), TR::InstOpCode::getCmpHalfWordImmOpCode(), child, whatReg, 0);

       generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, child, cFlowRegionStart);
       cFlowRegionStart->setStartInternalControlFlow();
       generateS390BranchInstruction(self()->cg(), TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, child, nonNullLabel);
       generateRRInstruction(self()->cg(), TR::InstOpCode::getXORRegOpCode(), child, pushRegister, pushRegister);

       TR::RegisterDependencyConditions * conditions = new (self()->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, self()->cg());
       conditions->addPostCondition(addrReg, TR::RealRegister::AssignAny);
       conditions->addPostCondition(whatReg, TR::RealRegister::AssignAny);

       generateS390LabelInstruction(self()->cg(), TR::InstOpCode::LABEL, child, nonNullLabel, conditions);
       nonNullLabel->setEndInternalControlFlow();

       self()->cg()->stopUsingRegister(whatReg);
       }

    return self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, stackOffsetPtr, dependencies, pushRegister);
    }

TR::Register *
OMR::Z::Linkage::pushVectorArg(TR::Node * callNode, TR::Node * child, int32_t numVectorArgs, int32_t pindex, int32_t * stackOffsetPtr, TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister)
   {
   TR::DataType argType = child->getType();

   bool isStoreArg = self()->isAllParmsOnStack();

   int32_t argSize = 16;

   TR::RealRegister::RegNum argRegNum=TR::RealRegister::NoReg;

      {
      argRegNum = self()->getVectorArgumentRegister(numVectorArgs);
      if (argRegNum == TR::RealRegister::NoReg)
         isStoreArg = true;
      }

   if (argRegister == NULL)
         {
         argRegister = self()->cg()->evaluate(child);
         self()->cg()->decReferenceCount(child);
         // TODO     argRegister = copyArgRegister(callNode, child, argRegister);
         }

   dependencies->addPreCondition(argRegister, argRegNum);

   if (!self()->isFirstParmAtFixedOffset())
      {
      *stackOffsetPtr -= argSize;
      }

   if (isStoreArg)
      {
      TR::InstOpCode::Mnemonic storeOp = self()->getStoreOpCodeForLinkage(child);

      TR::Register *stackRegister = self()->getStackRegisterForOutgoingArguments(callNode, dependencies);  // delay (possibly) creating this till needed
      self()->storeArgumentOnStack(callNode, storeOp, argRegister, stackOffsetPtr, stackRegister);

      if (self()->isFirstParmAtFixedOffset())
         {
         *stackOffsetPtr +=  argSize;
         }
      }

   return argRegister;
   }

TR::Register *
OMR::Z::Linkage::pushAggregateArg(TR::Node * callNode, TR::Node * child, int32_t numIntegerArgs,
       int32_t numFloatArgs, int32_t * stackOffsetPtr,
       TR::RegisterDependencyConditions * dependencies, TR::Register * argRegister)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR_ASSERT(false, "TR_EnableNewOType should be enabled");
#endif
   return argRegister; // somewhat meaningless
   }

/**
 * Load up integer argument registers with values saved in the stack at their
 * canonical positions
 */
void
OMR::Z::Linkage::loadIntArgumentsFromStack(TR::Node *callNode, TR::RegisterDependencyConditions *dependencies, TR::DataType argType, int32_t stackOffset, int32_t argSize, int32_t numIntegerArgs, TR::Register* stackRegister)
   {
   TR::RealRegister::RegNum argRegNum;
   int8_t gprSize = self()->machine()->getGPRSize();


   argRegNum = self()->getIntegerArgumentRegister(numIntegerArgs);
   if (argRegNum != TR::RealRegister::NoReg)
      {
      int32_t regsNeeded = argSize / gprSize;
      for (int32_t i = 0; i < regsNeeded; i++)
         {
         TR::Register *argRegister = self()->cg()->allocateRegister();
         dependencies->addPreCondition(argRegister, argRegNum);
         TR::MemoryReference * argMemRef = generateS390MemoryReference(stackRegister, stackOffset , self()->cg());
         TR::InstOpCode::Mnemonic loadOp = TR::InstOpCode::getLoadOpCode();
#ifdef J9_PROJECT_SPECIFIC
         if (argType == TR::DecimalFloat && self()->cg()->comp()->target().is64Bit()) // special case for DecFloat on 64bit
            loadOp = TR::InstOpCode::LLGF;
#endif

         generateRXInstruction(self()->cg(), loadOp, callNode, argRegister, argMemRef);

         stackOffset += gprSize;
         numIntegerArgs ++;
         argRegNum = self()->getIntegerArgumentRegister(numIntegerArgs);
         if (argRegNum == TR::RealRegister::NoReg)
            break;
         }
      }
   }

/**
 * Do not kill special regs (java stack ptr, system stack ptr, and method metadata reg)
 */
void
OMR::Z::Linkage::doNotKillSpecialRegsForBuildArgs (TR::Linkage *linkage, bool isFastJNI, int64_t &killMask)
   {
   int32_t i;
   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastFPR; i++)
      {
      if (linkage->getPreserved(REGNUM(i)))
         killMask &= ~(0x1L << REGINDEX(i));
      }
   }


/**
 * Build arguments for System routines
 */
int32_t
OMR::Z::Linkage::buildArgs(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
   bool isFastJNI, int64_t killMask, TR::Register* &vftReg, bool PassReceiver)
   {

   TR::SystemLinkage * systemLinkage = (TR::SystemLinkage *) self()->cg()->getLinkage(TR_System);

   int8_t gprSize = self()->machine()->getGPRSize();
   TR::Register * tempRegister;
   int32_t argIndex = 0, i, from, to, step, numChildren;
   int32_t argSize = 0;
   int32_t stackOffset= 0;
   uint32_t numIntegerArgs = 0;
   uint32_t numFloatArgs = 0;
   uint32_t numVectorArgs = 0;
   TR::Node * child;
   void * smark;
   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
   TR::DataType resType = callNode->getType();
   TR::DataType resDataType = resType.getDataType();

   const bool enableVectorLinkage = self()->cg()->getSupportsVectorRegisters();


   //Not kill special registers
   self()->doNotKillSpecialRegsForBuildArgs(self(), isFastJNI, killMask);

   // For the generated classObject argument, we didn't use them in the dispatch sequence.
   // Simply evaluating them would be enough. Care must be taken when we begin to use them,
   // in order not to spill in the dispatch sequence.
   for (i = 0; i < firstArgumentChild; i++)
      {
      TR::Node * child = callNode->getChild(i);
      vftReg = self()->cg()->evaluate(child);
      self()->cg()->decReferenceCount(child);
      }

   // Skip the first receiver argument if instructed.
   if (!PassReceiver)
      {
      // Force evaluation of child if necessary
      TR::Node *receiverChild = callNode->getChild(firstArgumentChild);
      if (receiverChild->getReferenceCount() > 1)
         {
         self()->cg()->evaluate(receiverChild);
         self()->cg()->decReferenceCount(receiverChild);
         }
      else
         {
         self()->cg()->recursivelyDecReferenceCount(receiverChild);
         }

      firstArgumentChild++;
      }

   numChildren = callNode->getNumChildren() - 1;
   if((callNode->getNumChildren() >= 1) && (callNode->getChild(numChildren)->getOpCodeValue() == TR::GlRegDeps))
       numChildren--;

   // setup helper routine arguments in reverse order
   bool rightToLeft = self()->isParmsInReverseOrder() &&
      //we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
      !callNode->getSymbolReference()->isOSRInductionHelper();
   if (rightToLeft)
      {
      from = numChildren;
      to = firstArgumentChild;
      step = -1;
      }
   else
      {
      from = firstArgumentChild;
      to = numChildren;
      step = 1;
      }

   //Add special argument register dependency
   self()->addSpecialRegDepsForBuildArgs(callNode, dependencies, from, step);

   for (i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      child = callNode->getChild(i);
      TR::DataType argType = child->getType();
      TR::DataType argDataType = argType.getDataType();

      if (argType.isInt64()
          || argDataType == TR::Double
#ifdef J9_PROJECT_SPECIFIC
          || argDataType == TR::DecimalDouble
#endif
          )
         {
         argSize += self()->isTwoStackSlotsForLongAndDouble()? 2*gprSize : 8;
         }
#ifdef J9_PROJECT_SPECIFIC
      else if (argType.isLongDouble())
         argSize += 16;
#endif
      else if (enableVectorLinkage && argDataType.isVector())
         argSize += 128;
      else
         argSize += gprSize;
      }

   stackOffset = self()->isFirstParmAtFixedOffset() ? self()->getOffsetToFirstParm() : argSize;

   //store env register
   stackOffset = self()->storeExtraEnvRegForBuildArgs(callNode, self(), dependencies, isFastJNI, stackOffset, gprSize, numIntegerArgs);

   // Is indirect dispatch
   // For now use GPR1/6 ... we should try and generalize to any reg
   //

   //Push args onto stack
   for (i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      TR::MemoryReference * mref = NULL;
      TR::Register * argRegister = NULL;
      child = callNode->getChild(i);

      switch (child->getDataType())
         {
         case TR::Address:
            if (isFastJNI && child->getDataType() == TR::Address)
               {
               argRegister = self()->pushJNIReferenceArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);
               numIntegerArgs++;
               break;
               }
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            argRegister = self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);
            numIntegerArgs++;
            break;
         case TR::Int64:
            if (self()->cg()->comp()->target().is32Bit())
               {
               argRegister = self()->pushLongArg32(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);
               numIntegerArgs += 2;
               }
            else
               {
               argRegister = self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);
               numIntegerArgs += 1;
               }
            break;
         case TR::Float:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalFloat:
#endif
               {
               argRegister = self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);

               numFloatArgs++;

               if (self()->isSkipGPRsForFloatParms())
                  {
                  numIntegerArgs++;
                  }
               break;
               }
         case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalDouble:
#endif
            argRegister = self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);

            numFloatArgs++;

            if (self()->isSkipGPRsForFloatParms())
               {
               numIntegerArgs += (self()->cg()->comp()->target().is64Bit()) ? 1 : 2;
               }
            break;
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalLongDouble:
            if (numFloatArgs%2 == 1)
               { // need to skip fp arg 'hole' for long double arg
               numFloatArgs++;
               if (self()->isSkipGPRsForFloatParms() && numIntegerArgs < self()->getNumIntegerArgumentRegisters())
                  numIntegerArgs++;
               }

            argRegister = self()->pushArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);

            numFloatArgs = numFloatArgs + 2;  // long double takes up 2 fp registers

            if (self()->isSkipGPRsForFloatParms())
               {
               if (numIntegerArgs < self()->getNumIntegerArgumentRegisters())
                  {
                  numIntegerArgs += (self()->cg()->comp()->target().is64Bit()) ? 2 : 4;
                  }
               }
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
            if (self()->isAggregatesPassedInParmRegs() || self()->isAggregatesPassedOnParmStack())
               {
               argRegister = self()->pushAggregateArg(callNode, child, numIntegerArgs, numFloatArgs, &stackOffset, dependencies);

               if (self()->isAggregatesPassedInParmRegs())
                  {
                  size_t slot = self()->machine()->getGPRSize();
                  size_t childSize = child->getSize();
                  size_t size = (childSize + slot - 1) & (~(slot - 1));
                  size_t slots = size / slot;
                  numIntegerArgs += slots;
                  }
               else
                  numIntegerArgs ++;
               }
            break;
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
         case TR::VectorDouble:
            argRegister = self()->pushVectorArg(callNode, child, numVectorArgs, i, &stackOffset, dependencies);
            numVectorArgs++;
            break;
         }

         if (self()->isFastLinkLinkageType())
            {
            if ((numFloatArgs == 1) || (numIntegerArgs >= self()->getNumIntegerArgumentRegisters()))
               {
               // force fastlink ABI condition of only one float parameter for fastlink parameter and it must be within first slots
               numFloatArgs = self()->getNumFloatArgumentRegisters();   // no more float args possible now
               }
            }
      }

   //Setup return register dependency
   TR::Register * resultReg=NULL;
   TR::Register * javaResultReg=NULL;
   switch(resDataType)
     {
         case TR::NoType:
            break;
         case TR::Float:
         case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalFloat:
         case TR::DecimalDouble:
#endif
            resultReg = self()->cg()->allocateRegister(TR_FPR);
            self()->cg()->setRealRegisterAssociation(resultReg, self()->getFloatReturnRegister());
            dependencies->addPostCondition(resultReg, self()->getFloatReturnRegister(),DefinesDependentRegister);
            killMask &= (~(0x1L << REGINDEX(self()->getFloatReturnRegister())));
            break;
#ifdef J9_PROJECT_SPECIFIC
         case TR::DecimalLongDouble:
            {
            TR::Register * lowReg = self()->cg()->allocateRegister(TR_FPR);
            TR::Register * highReg = self()->cg()->allocateRegister(TR_FPR);
            self()->cg()->setRealRegisterAssociation(highReg, self()->getLongDoubleReturnRegister0());
            dependencies->addPostCondition(highReg, self()->getLongDoubleReturnRegister0(),DefinesDependentRegister);
            self()->cg()->setRealRegisterAssociation(lowReg, self()->getLongDoubleReturnRegister2());
            dependencies->addPostCondition(lowReg, self()->getLongDoubleReturnRegister2(),DefinesDependentRegister);
            killMask &= (~(0x1L << REGINDEX(self()->getLongDoubleReturnRegister0())));
            killMask &= (~(0x1L << REGINDEX(self()->getLongDoubleReturnRegister2())));
            break;
            }
#endif
         case TR::Int64:
            if (self()->cg()->comp()->target().is32Bit())
               {
               //In this case, private and system linkage use same regs for return value
               TR::Register * resultRegLow = self()->cg()->allocateRegister();
               TR::Register * resultRegHigh = self()->cg()->allocateRegister();

               self()->cg()->setRealRegisterAssociation(resultRegLow, self()->getLongLowReturnRegister());
               dependencies->addPostCondition(resultRegLow, self()->getLongLowReturnRegister(),DefinesDependentRegister);
               killMask &= (~(0x1L << REGINDEX(self()->getLongLowReturnRegister())));

               self()->cg()->setRealRegisterAssociation(resultRegHigh, self()->getLongHighReturnRegister());
               dependencies->addPostCondition(resultRegHigh, self()->getLongHighReturnRegister(),DefinesDependentRegister);
               killMask &= (~(0x1L << REGINDEX(self()->getLongHighReturnRegister())));
               break;
               }
         case TR::Address:
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            resultReg = (resType.isAddress())? self()->cg()->allocateCollectedReferenceRegister() : self()->cg()->allocateRegister();
            self()->cg()->setRealRegisterAssociation(resultReg, self()->getIntegerReturnRegister());
            dependencies->addPostCondition(resultReg, self()->getIntegerReturnRegister(),DefinesDependentRegister);
            killMask &= (~(0x1L << REGINDEX(self()->getIntegerReturnRegister())));
            //add extra return register dependency
            killMask = self()->addFECustomizedReturnRegDependency(killMask, self(), resType, dependencies);
            break;
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
         case TR::VectorDouble:
            resultReg = self()->cg()->allocateRegister(TR_VRF);
            self()->cg()->setRealRegisterAssociation(resultReg, self()->getVectorReturnRegister());
            dependencies->addPostCondition(resultReg, self()->getVectorReturnRegister(),DefinesDependentRegister);
            killMask &= (~(0x1L << REGINDEX(self()->getVectorReturnRegister())));
            break;
      }


   // In zLinux, GPR6 may be used for param passing, in this case, we need to kill it
   // so kill GPR6 if used in preDeps and is not in postDeps
  if (self()->cg()->comp()->target().isLinux())
     {
     TR::Register * gpr6Reg= dependencies->searchPreConditionRegister(TR::RealRegister::GPR6);
     if (gpr6Reg)
        {
        gpr6Reg= dependencies->searchPostConditionRegister(TR::RealRegister::GPR6);
        }
     }

   // We need to kill all non-preserved regs on return from a call out.
   // We do this by creating a dummy post dependency condition on all
   // regs that are not preserved, not used for parameter passing in the
   // pre-cond, and not used as return regs in the post-cond,
   // or any other regs that are in the post conditions already
   TR::Register * dummyReg;
   TR::RealRegister::RegNum last = TR::RealRegister::LastFPR;


   for (i = TR::RealRegister::FirstGPR; i <= last ; i++)
      {
      if ((killMask & (0x1L << REGINDEX(i))))
         {
         dummyReg = NULL;
         self()->killAndAssignRegister(killMask, dependencies, &dummyReg, REGNUM(i), self()->cg(), true, true );
         }
      }

   // spill all overlapped vector registers if vector linkage is enabled
   if (enableVectorLinkage)
      {
      for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; i++)
         {
         if (!self()->getPreserved(REGNUM(i)))
            {
            dummyReg = NULL;
            self()->killAndAssignRegister(killMask,dependencies,&dummyReg,REGNUM(i),self()->cg(),true,true);
            }
         }
      }

   // spill all high regs
   //
   if (self()->cg()->comp()->target().is32Bit())
      {
      TR::Register *reg = self()->cg()->allocateRegister();

      dependencies->addPostCondition(reg, TR::RealRegister::KillVolHighRegs);
      self()->cg()->stopUsingRegister(reg);
      }

   // Adjust largest outgoing argument area
   int32_t argumentAreaSize;
   if (!self()->isFirstParmAtFixedOffset())
      {
      argumentAreaSize = argSize - stackOffset;
      }
   else
      {
      argumentAreaSize = stackOffset - self()->getOffsetToFirstParm();
      }
   if (self()->getLargestOutgoingArgumentAreaSize() < argumentAreaSize)
      self()->setLargestOutgoingArgumentAreaSize(argumentAreaSize);


   return argSize;
   }

void
OMR::Z::Linkage::replaceCallWithJumpInstruction(TR::Instruction *callInstruction)
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

TR::Instruction *
OMR::Z::Linkage::storeArgumentOnStack(TR::Node * callNode, TR::InstOpCode::Mnemonic opCode, TR::Register * argReg, int32_t *stackOffsetPtr, TR::Register* stackRegister)
   {
   TR::Instruction * cursor;
   int32_t padding = (self()->isPadFloatParms() && opCode == TR::InstOpCode::STE) ? 4 : 0;
   int32_t storeOffset = *stackOffsetPtr;

   TR::MemoryReference * argMemRef = generateS390MemoryReference(
       stackRegister, storeOffset +padding, self()->cg());

   if (opCode == TR::InstOpCode::STM)
      {
      cursor = generateRSInstruction(self()->cg(), opCode, callNode, argReg, argMemRef);
      }
   else if (opCode == TR::InstOpCode::VST)
      {
      cursor = generateVRXInstruction(self()->cg(), opCode, callNode, argReg, argMemRef);
      }
   else
      {
      cursor = generateRXInstruction(self()->cg(), opCode, callNode, argReg, argMemRef);
      }

   self()->cg()->stopUsingRegister(argReg);

   return cursor;
   }

/**
 * Handles LongDouble type params
 */
TR::Instruction *
OMR::Z::Linkage::storeLongDoubleArgumentOnStack(TR::Node * callNode, TR::DataType argType, TR::InstOpCode::Mnemonic opCode, TR::Register * argReg, int32_t *stackOffsetPtr, TR::Register* stackRegister)
   {
   TR::Instruction * cursor;
   int32_t size = (opCode==InstOpCode::STE) ? 4 : 8;
   int32_t storeOffset = *stackOffsetPtr;

    TR::MemoryReference * argMemRef = generateS390MemoryReference(stackRegister, storeOffset, self()->cg());
    TR::MemoryReference * argMemRef2 = generateS390MemoryReference(*argMemRef, size, self()->cg());
    generateRXInstruction(self()->cg(), opCode, callNode, argReg->getHighOrder(), argMemRef);
    argMemRef->stopUsingMemRefRegister(self()->cg());
    cursor = generateRXInstruction(self()->cg(), opCode, callNode, argReg->getLowOrder(), argMemRef2);
    argMemRef2->stopUsingMemRefRegister(self()->cg());
   self()->cg()->stopUsingRegister(argReg);

   return cursor;

   }

int64_t
OMR::Z::Linkage::killAndAssignRegister(int64_t killMask, TR::RegisterDependencyConditions * deps,
   TR::Register ** virtualRegPtr, TR::RealRegister::RegNum regNum,
   TR::CodeGenerator * codeGen, bool isAllocate, bool isDummy)
   {
   TR::Register * depVirReg = deps->searchPostConditionRegister(regNum);
   if (depVirReg)
      {
      if (*virtualRegPtr) codeGen->stopUsingRegister(*virtualRegPtr);
      *virtualRegPtr = depVirReg;
      return killMask;
      }
   else
      {
      if (isAllocate)
         {
         if (regNum <= TR::RealRegister::LastGPR)
            *virtualRegPtr = self()->cg()->allocateRegister();
         else if (regNum <= TR::RealRegister::LastFPR)
            *virtualRegPtr = self()->cg()->allocateRegister(TR_FPR);
         else
            *virtualRegPtr = self()->cg()->allocateRegister(TR_VRF);
         }

      // edTODO vrf linkage : killAndAssignRegister unimplemented

      deps->addPostCondition(*virtualRegPtr, regNum, DefinesDependentRegister);
      if (isAllocate)
         codeGen->stopUsingRegister(*virtualRegPtr);

      if (isDummy)
         {
         (*virtualRegPtr)->setPlaceholderReg();
         if (self()->cg()->comp()->target().is64Bit() && regNum <= TR::RealRegister::LastGPR)
            {
            (*virtualRegPtr)->setIs64BitReg(true);
            }
         }
      return killMask & (~(0x1L << REGINDEX(regNum)));
      }
   }

int64_t
OMR::Z::Linkage::killAndAssignRegister(int64_t killMask, TR::RegisterDependencyConditions * deps,
   TR::Register ** virtualRegPtr, TR::RealRegister* realReg,
   TR::CodeGenerator * codeGen, bool isAllocate, bool isDummy)
   {
   return self()->killAndAssignRegister(killMask, deps, virtualRegPtr, realReg->getRegisterNumber(), codeGen, isAllocate, isDummy );
   }


void OMR::Z::Linkage::generateDispatchReturnLable(TR::Node * callNode, TR::CodeGenerator * codeGen, TR::RegisterDependencyConditions * &deps,
      TR::Register * javaReturnRegister, bool hasGlRegDeps, TR::Node *GlobalRegDeps)
   {
   TR::LabelSymbol * endOfDirectToJNILabel = generateLabelSymbol(self()->cg());
   TR::RegisterDependencyConditions * postDeps = new (self()->trHeapMemory())
               TR::RegisterDependencyConditions(NULL, deps->getPostConditions(), 0, deps->getAddCursorForPost(), self()->cg());

   generateS390LabelInstruction(codeGen, TR::InstOpCode::LABEL, callNode, endOfDirectToJNILabel, postDeps);

#ifdef J9_PROJECT_SPECIFIC
   if (codeGen->getSupportsRuntimeInstrumentation())
      TR::TreeEvaluator::generateRuntimeInstrumentationOnOffSequence(codeGen, TR::InstOpCode::RION, callNode);
#endif

   callNode->setRegister(javaReturnRegister);

   #if TODO // for live register - to do later
   self()->cg()->freeAndResetTransientLongs();
   #endif

   if (javaReturnRegister)
      {
      deps->stopUsingDepRegs(self()->cg(), javaReturnRegister->getRegisterPair() == NULL ? javaReturnRegister :
         javaReturnRegister->getHighOrder(), javaReturnRegister->getLowOrder());
      }
   else
      {
      deps->stopUsingDepRegs(self()->cg());
      }


   if(hasGlRegDeps)
      {
      self()->cg()->decReferenceCount(GlobalRegDeps);
      }
   }


void
OMR::Z::Linkage::setupRegisterDepForLinkage(TR::Node * callNode, TR_DispatchType dispatchType,
   TR::RegisterDependencyConditions * &deps, int64_t & killMask, TR::SystemLinkage * systemLinkage,
   TR::Node * &GlobalRegDeps, bool &hasGlRegDeps, TR::Register ** methodAddressReg, TR::Register * &JavaLitOffsetReg)
   {
   // Extra dependency for killing volatile high registers (see KillVolHighRegs)
   int32_t numDeps = systemLinkage->getNumberOfDependencyGPRegisters() + 1;

   if (self()->cg()->getSupportsVectorRegisters())
      numDeps += 32; //VRFs need to be spilled

   // Remove DEPEND instruction and merge glRegDeps to call deps
   // *Speculatively* increase numDeps for dependencies from glRegDeps
   // which is added right before callNativeFunction.
   // GlobalRegDeps should not add any more children after here.

   hasGlRegDeps = (callNode->getNumChildren() >= 1) &&
            (callNode->getChild(callNode->getNumChildren()-1)->getOpCodeValue() == TR::GlRegDeps);


   if(hasGlRegDeps)
      {
      GlobalRegDeps = callNode->getChild(callNode->getNumChildren()-1);
      numDeps += GlobalRegDeps->getNumChildren();
      }

   if (!deps)
      deps = generateRegisterDependencyConditions(numDeps, numDeps, self()->cg());


   self()->comp()->setHasNativeCall();


   if (!self()->cg()->comp()->target().isZOS())
      {
      TR::Register * parm3VirtualRegister = NULL;
      killMask = self()->killAndAssignRegister(killMask, deps, &parm3VirtualRegister, TR::RealRegister::GPR4, self()->cg(), true);
      }



   if (dispatchType == TR_SystemDispatch)
     {
     if (!self()->cg()->comp()->target().isZOS())
       killMask = self()->killAndAssignRegister(killMask, deps, methodAddressReg, TR::RealRegister::GPR14, self()->cg(), true);
     }
   }


void
OMR::Z::Linkage::setupBuildArgForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::RegisterDependencyConditions * deps, bool isFastJNI,
      bool isPassReceiver, int64_t & killMask, TR::Node * GlobalRegDeps, bool hasGlRegDeps, TR::SystemLinkage * systemLinkage)
   {

   TR::RegisterDependencyConditions *glRegDeps;


   TR::Register * systemReturnAddressVirtualRegister = NULL;
   TR::RealRegister::RegNum systemReturnAddressRegister = systemLinkage->getReturnAddressRegister();

   killMask = self()->killAndAssignRegister(killMask, deps, &systemReturnAddressVirtualRegister,
      systemReturnAddressRegister, self()->cg(), true);



   //Evaluate and load arguments to native function
   TR::Register *vftReg = NULL;
   int32_t argSize = systemLinkage->buildArgs(callNode, deps, isFastJNI, killMask, vftReg, isPassReceiver);


   if(hasGlRegDeps)
      {
      self()->cg()->evaluate(GlobalRegDeps);
      glRegDeps = generateRegisterDependencyConditions(self()->cg(), GlobalRegDeps, 32);
      TR::RegisterDependency *regDep;

      //Add preconditions
      for(int i = 0; i < glRegDeps->getAddCursorForPre(); i++)
         {
         regDep = glRegDeps->getPreConditions()->getRegisterDependency(i);
         deps->addPreConditionIfNotAlreadyInserted(regDep->getRegister(), regDep->getRealRegister());
         }

      //Add postconditions
      for(int i = 0; i < glRegDeps->getAddCursorForPost(); i++)
         {
         regDep = glRegDeps->getPostConditions()->getRegisterDependency(i);
         deps->addPostConditionIfNotAlreadyInserted(regDep->getRegister(), regDep->getRealRegister());
         }
      }
   }


void
OMR::Z::Linkage::performCallNativeFunctionForLinkage(TR::Node * callNode, TR_DispatchType dispatchType, TR::Register * &javaReturnRegister, TR::SystemLinkage * systemLinkage,
      TR::RegisterDependencyConditions * &deps, TR::Register * javaLitOffsetReg, TR::Register * methodAddressReg, bool isJNIGCPoint)
   {
   TR::Snippet * callDataSnippet = NULL;
   TR::LabelSymbol * returnFromJNICallLabel = generateLabelSymbol(self()->cg());
   intptr_t targetAddress = (intptr_t) 0;

   if (dispatchType == TR_SystemDispatch)
     {
     TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
     TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
     targetAddress = (intptr_t) methodSymbol->getMethodAddress();
     }

   javaReturnRegister = systemLinkage->callNativeFunction(
        callNode, deps, targetAddress, methodAddressReg, javaLitOffsetReg, returnFromJNICallLabel,
        callDataSnippet, isJNIGCPoint);
   }


TR::Register *
OMR::Z::Linkage::buildNativeDispatch(TR::Node * callNode,
      TR_DispatchType dispatchType,
      TR::RegisterDependencyConditions * &deps,
      bool isFastJNI,
      bool isPassReceiver,
      bool isJNIGCPoint)
   {
   if (self()->comp()->getOption(TR_TraceCG))
      traceMsg(self()->comp(), "\nbuildNativeDispatch\n");

   bool hasGlRegDeps;
   int64_t killMask = -1;
   TR::Node *GlobalRegDeps;

   TR::Register * methodAddressReg = NULL;
   TR::Register * javaLitOffsetReg = NULL;
   TR::Register * javaReturnRegister = NULL;

   TR::SystemLinkage * systemLinkage = (TR::SystemLinkage *) self()->cg()->getLinkage(TR_System);


   /*********************************/
   /* Register Dependencies         */
   /*********************************/
   // omr: extracted out code that deals with register dependencies into it's own method
   // in order to sink j9 specific customizations out with virutal dispatch.
   // todo: can be cleaned up better (i.e, get rid of some j9 only params) once other related linkage methods are cleaned up
   self()->setupRegisterDepForLinkage(callNode, dispatchType, deps, killMask, systemLinkage, GlobalRegDeps, hasGlRegDeps, &methodAddressReg, javaLitOffsetReg);


   /*********************************/
   /* Build Arguments               */
   /*********************************/
   self()->setupBuildArgForLinkage(callNode, dispatchType, deps, isFastJNI, isPassReceiver, killMask, GlobalRegDeps, hasGlRegDeps, systemLinkage);

   // omr todo: this should be cleaned up with TR::S390PrivateLinkage::setupBuildArgForLinkage and JNI Dispatch
   //if (dispatchType == TR_JNIDispatch)  return NULL;


   /*********************************/
   /*  Call Native Function         */
   /*********************************/
   self()->performCallNativeFunctionForLinkage(callNode, dispatchType, javaReturnRegister, systemLinkage, deps, javaLitOffsetReg, methodAddressReg, isJNIGCPoint);


   /*********************************/
   /* Generate Return Labels etc.   */
   /*********************************/
   self()->generateDispatchReturnLable(callNode, self()->cg(), deps, javaReturnRegister, hasGlRegDeps, GlobalRegDeps);


   // omr todo: need to move wcode stuff to a better place


   return javaReturnRegister;
   }


TR::Register * OMR::Z::Linkage::buildSystemLinkageDispatch(TR::Node * callNode)
   {
   return self()->buildNativeDispatch(callNode, TR_SystemDispatch);
   }


////////////////////////////////////////////////////////////////////////////////
// TR_S390PrivateLinage:: member functions
////////////////////////////////////////////////////////////////////////////////

void
OMR::Z::Linkage::lockRegister(TR::RealRegister * lpReal)
   {
   // literal pool register
   lpReal->setState(TR::RealRegister::Locked);
   lpReal->setAssignedRegister(lpReal);
   lpReal->setHasBeenAssignedInMethod(true);
   }

void
OMR::Z::Linkage::unlockRegister(TR::RealRegister * lpReal)
   {
   // literal pool register
   lpReal->resetState(TR::RealRegister::Free);
   lpReal->setAssignedRegister(NULL);
   lpReal->setHasBeenAssignedInMethod(false);
   }

bool OMR::Z::Linkage::needsAlignment(TR::DataType dt, TR::CodeGenerator * cg)
   {
   TR::Compilation* comp = cg->comp();
   switch (dt)
      {
      case TR::Double:
      case TR::Int64:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalDouble:
      case TR::DecimalLongDouble:
#endif
      case TR::Aggregate:
         return true;
      case TR::Address:
         return TR::Compiler->om.sizeofReferenceAddress() == 8 ? true : false;
      default:
         break;
      }
   return false;
   }



/**
 * Get first used register between fromreg to toreg
 */
TR::RealRegister::RegNum
OMR::Z::Linkage::getFirstSavedRegister(int32_t fromreg, int32_t toreg)
   {
   TR::RealRegister::RegNum firstUsedReg = TR::RealRegister::NoReg;

   for (int32_t i = fromreg; i <= toreg; ++i)
      {
      if ((self()->getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
         {
         firstUsedReg = REGNUM(i);
         return firstUsedReg;
         }
      }

   return firstUsedReg;
   }

/**
 * Get last used register between fromreg to toreg
 */
TR::RealRegister::RegNum
OMR::Z::Linkage::getLastSavedRegister(int32_t fromreg, int32_t toreg)
   {
   TR::RealRegister::RegNum lastUsedReg = TR::RealRegister::NoReg;

   for (int32_t i = fromreg; i <= toreg; ++i)
      {
      if ((self()->getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod())
         {
         lastUsedReg = REGNUM(i);
         }
      }

   return lastUsedReg;
   }

/**
 * Get last used register between fromreg to toreg
 */
TR::RealRegister::RegNum
OMR::Z::Linkage::getFirstRestoredRegister(int32_t fromreg, int32_t toreg)
   {
   return self()->getFirstSavedRegister(fromreg, toreg);
   }

/**
 * Get last used register between fromreg to toreg
 */
TR::RealRegister::RegNum
OMR::Z::Linkage::getLastRestoredRegister(int32_t fromreg, int32_t toreg)
   {
   return self()->getLastSavedRegister(fromreg, toreg);
   }


#define LMG_THRESHOLD 5

TR::Instruction *
OMR::Z::Linkage::restorePreservedRegs(TR::RealRegister::RegNum firstUsedReg,
   TR::RealRegister::RegNum lastUsedReg, int32_t blockNumber,
   TR::Instruction * cursor, TR::Node * nextNode, TR::RealRegister * spReg, TR::MemoryReference * rsa,
   TR::RealRegister::RegNum spRealReg)
   {
   bool break64 = self()->cg()->comp()->target().is64Bit() && self()->comp()->getOption(TR_Enable39064Epilogue);

   // restoring preserved regs is unavoidable in Java due to GC object moves

   // Update new start and end
   uint8_t newFirstUsedReg = firstUsedReg;
   uint8_t newLastUsedReg = lastUsedReg;
   uint8_t newNumUsedRegs = newLastUsedReg - newFirstUsedReg + 1;

   // can't break 64-bit LOAD MULTIPLE down
   if (break64 && (newNumUsedRegs >= LMG_THRESHOLD))
      {
      break64 = false;
      }

   // if we're restoring SP, then want to stick to LOAD MULTIPLE
   uint8_t curReg = newFirstUsedReg;
   for (uint8_t i = 0; i < newNumUsedRegs; i++)
      {
      if (spRealReg == REGNUM(curReg))
         {
         break64 = false;
         break;
         }
      curReg++;
      }

   //NOTE: Can never re-use memrefs in > 1 instruction
   //NOTE: LMG GPRx,GPRx only loads 4-bytes of data

   // 64-bit optimized case
   if (break64 ||
       newNumUsedRegs == 1)
      {
      curReg = newFirstUsedReg;
      int32_t curDisp = rsa->getOffset();
      for (uint8_t i = 0; i < newNumUsedRegs; i++)
         {
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(),
                   nextNode, self()->getRealRegister(REGNUM(curReg)), rsa, cursor);

         curDisp += self()->machine()->getGPRSize();
         curReg++;

         // generate the next rsa

         if (i != newNumUsedRegs)
            {
            rsa = generateS390MemoryReference(spReg, curDisp, self()->cg());
            }
         }
      }

   // 64-bit non-optimized CASE AND 32-bit shorter LMG case
   else if ((self()->cg()->comp()->target().is64Bit() && !break64) ||
            ((newFirstUsedReg+1) %2 == 0))
      {
      cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::getLoadMultipleOpCode(), nextNode,
                 self()->getRealRegister(REGNUM(newFirstUsedReg)),
                 self()->getRealRegister(REGNUM(newLastUsedReg)), rsa, cursor);
      }
   else //32-bit (try to use bumps)
      {
      //Load Single + Load Multiple
      cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::getLoadOpCode(),
                nextNode, self()->getRealRegister(REGNUM(newFirstUsedReg)), rsa, cursor);

      rsa = generateS390MemoryReference(spReg, rsa->getOffset()+self()->machine()->getGPRSize(), self()->cg());
      cursor = generateRSInstruction(self()->cg(), TR::InstOpCode::getLoadMultipleOpCode(), nextNode,
                    self()->getRealRegister(REGNUM(newFirstUsedReg+1)),
                    self()->getRealRegister(REGNUM(newLastUsedReg)), rsa, cursor);
      }
   return cursor;
   }


int32_t OMR::Z::Linkage::getFirstMaskedBit(int16_t mask, int32_t from , int32_t to)
   {
   for (int32_t i = from; i <= to; i++)
     {
     if (mask & (1 << (i)))
        {
        return i;
        }
     }
   return -1;
   }

int32_t OMR::Z::Linkage::getLastMaskedBit(int16_t mask, int32_t from , int32_t to)
   {
   for (int32_t i = to; i >= from; i--)
     {
     if (mask & (1 << (i)))
        {
        return i;
        }
     }
   return -1;
   }

int32_t
OMR::Z::Linkage::getFirstMaskedBit(int16_t mask)
   {
   return TR::Linkage::getFirstMaskedBit(mask , 0, 15);
   }

int32_t
OMR::Z::Linkage::getLastMaskedBit(int16_t mask)
   {
   return TR::Linkage::getLastMaskedBit(mask , 0, 15);
   }

bool
OMR::Z::Linkage::isOSLinkageType()
   {
   return (self()->getExplicitLinkageType() & TR_SystemOS_MASK) != 0;
   }

bool
OMR::Z::Linkage::isXPLinkLinkageType()
   {
   return self()->getExplicitLinkageType() == TR_SystemXPLink;
   }

bool
OMR::Z::Linkage::isFastLinkLinkageType()
   {
   return self()->getExplicitLinkageType() == TR_SystemFastLink;
   }

bool
OMR::Z::Linkage::isZLinuxLinkageType()
   {
   return self()->getExplicitLinkageType() == TR_SystemLinux;
   }

TR::Register *
OMR::Z::Linkage::buildNativeDispatch(TR::Node * callNode, TR_DispatchType dispatchType)
   {
   TR::RegisterDependencyConditions * deps = NULL; return self()->buildNativeDispatch(callNode, dispatchType, deps, false, true, true);
   }

int32_t
OMR::Z::Linkage::getOutgoingParameterBlockSize()
   {
   return self()->getLargestOutgoingArgumentAreaSize();
   }

int32_t
OMR::Z::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
   {
   switch (kind)
      {
      case TR_GPR:
         return self()->getNumIntegerArgumentRegisters();
      case TR_FPR:
         return self()->getNumFloatArgumentRegisters();
      case TR_VRF:
         return self()->getNumVectorArgumentRegisters();
      default:
         return 0;
      }
   return 0;
   }

void
OMR::Z::Linkage::setIntegerArgumentRegister(uint32_t index, TR::RealRegister::RegNum r)
   {
   _intArgRegisters[index] = r;
   self()->setRegisterFlag(r, IntegerArgument);
   }

/** Get the indexth Long High argument register */
TR::RealRegister::RegNum
OMR::Z::Linkage::getLongHighArgumentRegister(uint32_t index)
   {
   if ((index < _numIntegerArgumentRegisters -1)||
       ((index == _numIntegerArgumentRegisters -1) && self()->isSplitLongParm()))
      return _intArgRegisters[index];
   else
      return TR::RealRegister::NoReg;
   }

/** Set the indexth float argument register */
void
OMR::Z::Linkage::setFloatArgumentRegister(uint32_t index, TR::RealRegister::RegNum r)
   {
   _floatArgRegisters[index] = r;
   self()->setRegisterFlag(r, FloatArgument);
   }

/** Set the indexth vector argument register */
void
OMR::Z::Linkage::setVectorArgumentRegister(uint32_t index, TR::RealRegister::RegNum r)
    {
    _vectorArgRegisters[index] = r;
    self()->setRegisterFlag(r, VectorArgument);
    }

TR::RealRegister::RegNum
OMR::Z::Linkage::setIntegerReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, IntegerReturn); return _integerReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongLowReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, IntegerReturn); return _longLowReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongHighReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, IntegerReturn); return _longHighReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, IntegerReturn); return _longReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setFloatReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _floatReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setDoubleReturnRegister (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _doubleReturnRegister = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongDoubleReturnRegister0 (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _longDoubleReturnRegister0 = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongDoubleReturnRegister2 (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _longDoubleReturnRegister2 = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongDoubleReturnRegister4 (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _longDoubleReturnRegister4 = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setLongDoubleReturnRegister6 (TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, FloatReturn); return _longDoubleReturnRegister6 = r;
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::setVectorReturnRegister(TR::RealRegister::RegNum r)
   {
   self()->setRegisterFlag(r, VectorReturn); return _vectorReturnRegister = r;
   }

TR::RealRegister *
OMR::Z::Linkage::getStackPointerRealRegister()
   {
   return self()->getRealRegister(_stackPointerRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getStackPointerRealRegister(uint32_t num)
   {
   return self()->getRealRegister(_stackPointerRegister);
   }

TR::RealRegister::RegNum
OMR::Z::Linkage::getNormalStackPointerRegister()
   {
   return self()->getStackPointerRegister();
   }

TR::RealRegister *
OMR::Z::Linkage::getNormalStackPointerRealRegister()
   {
   return self()->getStackPointerRealRegister();
   }

TR::RealRegister *
OMR::Z::Linkage::getEntryPointRealRegister()
   {
   return self()->getRealRegister(_entryPointRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getLitPoolRealRegister()
   {
   return self()->getRealRegister(_litPoolRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getStaticBaseRealRegister()
   {
   return self()->getRealRegister(_staticBaseRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getPrivateStaticBaseRealRegister()
   {
   return self()->getRealRegister(_privateStaticBaseRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getReturnAddressRealRegister()
   {
   return self()->getRealRegister(_returnAddrRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getVTableIndexArgumentRegisterRealRegister()
   {
   return self()->getRealRegister(_vtableIndexArgumentRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getJ9MethodArgumentRegisterRealRegister()
   {
   return self()->getRealRegister(_j9methodArgumentRegister);
   }

TR::RealRegister *
OMR::Z::Linkage::getRealRegister(TR::RealRegister::RegNum rNum)
   {
   return self()->machine()->getRealRegister(rNum);
   }
