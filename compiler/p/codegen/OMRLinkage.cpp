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

#include "codegen/Linkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "runtime/Runtime.hpp"

class TR_OpaqueClassBlock;
namespace TR { class AutomaticSymbol; }
namespace TR { class Register; }

void OMR::Power::Linkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   }

void OMR::Power::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   }

void OMR::Power::Linkage::initPPCRealRegisterLinkage()
   {
   }

void OMR::Power::Linkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   }

bool OMR::Power::Linkage::hasToBeOnStack(TR::ParameterSymbol *parm)
   {
   return(false);
   }

TR::MemoryReference *OMR::Power::Linkage::getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t length)
   {
   TR::Machine *machine = self()->machine();
   const TR::PPCLinkageProperties& properties = self()->getProperties();

   TR::MemoryReference *result = new (self()->trHeapMemory()) TR::MemoryReference(machine->getRealRegister(properties.getNormalStackPointerRegister()),
                                                                              argSize+self()->getOffsetToFirstParm(), length, self()->cg());
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode;
   return(result);
   }


// Default values:
// fsd=false
// saveOnly=false
TR::Instruction *OMR::Power::Linkage::saveArguments(TR::Instruction *cursor, bool fsd, bool saveOnly)
   {
   return self()->saveArguments(cursor, fsd, saveOnly, self()->comp()->getJittedMethodSymbol()->getParameterList());
   }

TR::Instruction *OMR::Power::Linkage::saveArguments(TR::Instruction *cursor, bool fsd, bool saveOnly,
                                             List<TR::ParameterSymbol> &parmList)
   {
   #define  REAL_REGISTER(ri)  machine->getRealRegister(ri)
   #define  REGNUM(ri)         ((TR::RealRegister::RegNum)(ri))
   const TR::PPCLinkageProperties& properties = self()->getProperties();
   TR::Machine *machine = self()->machine();
   TR::RealRegister      *stackPtr   = self()->cg()->getStackPointerRegister();
   TR::ResolvedMethodSymbol    *bodySymbol = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>   paramIterator(&parmList);
   TR::ParameterSymbol      *paramCursor;
   TR::Node                 *firstNode = self()->comp()->getStartTree()->getNode();
   TR_BitVector             freeScratchable;
   int32_t                  busyMoves[3][64];
   int32_t                  busyIndex = 0, i1;

   // the freeScratchable structure will not be used when saveOnly == true
   // no additional conditions were added with the intention of keeping the code easier to read
   // and not full of if conditions

   freeScratchable.init(TR::RealRegister::LastFPR + 1, self()->trMemory());

   // first, consider all argument registers free
   for (i1=TR::RealRegister::FirstGPR; i1<=TR::RealRegister::LastFPR; i1++)
      {
      if (!properties.getReserved(REGNUM(i1)))
         {
         freeScratchable.set(i1);
         }
      }
   // second, go through all parameters and reset registers that are actually used
   for (paramCursor=paramIterator.getFirst(); paramCursor!=NULL; paramCursor=paramIterator.getNext())
      {
      int32_t lri = paramCursor->getLinkageRegisterIndex();

      if (lri >= 0)
         {
         TR::DataType type = paramCursor->getType();
         TR::RealRegister::RegNum regNum;
         bool twoRegs = (self()->comp()->target().is32Bit() && type.isInt64() && lri < properties.getNumIntArgRegs()-1);

         if (!type.isFloatingPoint())
            {
            regNum = properties.getIntegerArgumentRegister(lri);
            }
         else
            {
            regNum = properties.getFloatArgumentRegister(lri);
            }

         if (paramCursor->isReferencedParameter())
            {
            freeScratchable.reset(regNum);
            if (twoRegs)
               freeScratchable.reset(regNum+1);
            }
         }
      }

   for (paramCursor=paramIterator.getFirst(); paramCursor!=NULL; paramCursor=paramIterator.getNext())
      {
      int32_t lri = paramCursor->getLinkageRegisterIndex();
      int32_t ai  = paramCursor->getAssignedGlobalRegisterIndex();
      int32_t offset = paramCursor->getParameterOffset();
      TR::DataType type = paramCursor->getType();
      int32_t dtype = type.getDataType();

      // TODO: Is there an accurate assume to insert here ?
      if (lri >= 0)
         {
         if (!paramCursor->isReferencedParameter() && !paramCursor->isParmHasToBeOnStack()) continue;

         TR::RealRegister::RegNum regNum;
         bool twoRegs = (self()->comp()->target().is32Bit() && type.isInt64() && lri < properties.getNumIntArgRegs()-1);

         if (type.isFloatingPoint())
            regNum = properties.getFloatArgumentRegister(lri);
         else
            regNum = properties.getIntegerArgumentRegister(lri);

         // Do not save arguments to the stack if in Full Speed Debug and saveOnly is not set.
         // If not in Full Speed Debug, the arguments will be saved.
         if (((ai<0 || self()->hasToBeOnStack(paramCursor)) && !fsd) || (fsd && saveOnly))
            {
            TR::InstOpCode::Mnemonic op = TR::InstOpCode::bad;
            int32_t length = 0;

            switch (dtype)
               {
               case TR::Int8:
               case TR::Int16:
                  if (properties.getSmallIntParmsAlignedRight())
                     offset &= ~3;

               case TR::Int32:
                  op = TR::InstOpCode::stw;
                  length = 4;
                  break;

               case TR::Address:
               case TR::Int64:
                  op = TR::InstOpCode::Op_st;
                  length = TR::Compiler->om.sizeofReferenceAddress();
                  break;

               case TR::Float:
                  op = TR::InstOpCode::stfs;
                  length = 4;
                  break;

               case TR::Double:
                  op = TR::InstOpCode::stfd;
                  length = 8;
                  break;

               default:
                  TR_ASSERT(false, "assertion failure");
                  break;
               }

            cursor = generateMemSrc1Instruction(self()->cg(), op, firstNode,
                        new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, length, self()->cg()),
                        REAL_REGISTER(regNum),
                        cursor);

            if (twoRegs)
               {
               cursor = generateMemSrc1Instruction(self()->cg(), TR::InstOpCode::stw, firstNode,
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset+4, 4, self()->cg()),
                           REAL_REGISTER(REGNUM(regNum+1)),
                           cursor);
               if (ai<0)
                  freeScratchable.set(regNum+1);
               }

            if (ai<0)
               freeScratchable.set(regNum);
            }

         // Global register is allocated to this argument.

         // Don't process if in Full Speed Debug and saveOnly is set
         if (ai>=0 && (!fsd || !saveOnly))
            {
            if (regNum != ai)      // Equal assignment: do nothing
               {
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateTrg1Src1Instruction(self()->cg(),
                              (type.isFloatingPoint()) ? TR::InstOpCode::fmr:TR::InstOpCode::mr,
                              firstNode, REAL_REGISTER(REGNUM(ai)), REAL_REGISTER(regNum), cursor);
                  freeScratchable.reset(ai);
                  freeScratchable.set(regNum);
                  }
               else    // The status of target global register is unclear (i.e. it is a arg reg)
                  {
                  busyMoves[0][busyIndex] = regNum;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 0;
                  busyIndex++;
                  }
               }

            if (self()->comp()->target().is32Bit() && type.isInt64())
               {
               int32_t aiLow = paramCursor->getAssignedLowGlobalRegisterIndex();

               if (!twoRegs)    // Low part needs to come from memory
                  {
                  offset += 4;   // We are dealing with the low part

                  if (freeScratchable.isSet(aiLow))
                     {
                     cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, REAL_REGISTER(REGNUM(aiLow)),
                                 new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
                     freeScratchable.reset(aiLow);
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = offset;
                     busyMoves[1][busyIndex] = aiLow;
                     busyMoves[2][busyIndex] = 1;
                     busyIndex++;
                     }
                  }
               else if (regNum+1 != aiLow)  // Low part needs to be moved
                  {
                  if (freeScratchable.isSet(aiLow))
                     {
                     cursor = generateTrg1Src1Instruction(self()->cg(), TR::InstOpCode::mr,
                              firstNode, REAL_REGISTER(REGNUM(aiLow)),
                              REAL_REGISTER(REGNUM(regNum+1)), cursor);
                     freeScratchable.reset(aiLow);
                     freeScratchable.set(regNum+1);
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = regNum+1;
                     busyMoves[1][busyIndex] = aiLow;
                     busyMoves[2][busyIndex] = 0;
                     busyIndex++;
                     }
                  }
               }
            }
         }

      // Don't process if in Full Speed Debug and saveOnly is set
      else if (ai >= 0 && (!fsd || !saveOnly))     // lri<0: arg needs to come from memory
         {
         switch (dtype)
            {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, REAL_REGISTER(REGNUM(ai)),
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
                  freeScratchable.reset(ai);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  busyMoves[2][busyIndex] = 1;
                  busyIndex++;
                  }
               break;
            case TR::Address:
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(),TR::InstOpCode::Op_load, firstNode, REAL_REGISTER(REGNUM(ai)),
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, TR::Compiler->om.sizeofReferenceAddress(), self()->cg()), cursor);
                  freeScratchable.reset(ai);
                  }
               else
                  {
                  busyMoves[0][busyIndex] = offset;
                  busyMoves[1][busyIndex] = ai;
                  if (self()->comp()->target().is64Bit())
                     busyMoves[2][busyIndex] = 2;
                  else
                     busyMoves[2][busyIndex] = 1;
                  busyIndex++;
                  }
               break;
            case TR::Int64:
               if (self()->comp()->target().is64Bit())
                  {
                  if (freeScratchable.isSet(ai))
                     {
                     cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::ld, firstNode, REAL_REGISTER(REGNUM(ai)),
                              new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 8, self()->cg()), cursor);
                     freeScratchable.reset(ai);
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = offset;
                     busyMoves[1][busyIndex] = ai;
                     busyMoves[2][busyIndex] = 2;
                     busyIndex++;
                     }
                  }
               else // 32-bit
                  {
                  if (freeScratchable.isSet(ai))
                     {
                     cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, REAL_REGISTER(REGNUM(ai)),
                              new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
                     freeScratchable.reset(ai);
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = offset;
                     busyMoves[1][busyIndex] = ai;
                     busyMoves[2][busyIndex] = 1;
                     busyIndex++;
                     }

                  ai = paramCursor->getAssignedLowGlobalRegisterIndex();
                  if (freeScratchable.isSet(ai))
                     {
                     cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, REAL_REGISTER(REGNUM(ai)),
                              new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset+4, 4, self()->cg()), cursor);
                     freeScratchable.reset(ai);
                     }
                  else
                     {
                     busyMoves[0][busyIndex] = offset+4;
                     busyMoves[1][busyIndex] = ai;
                     busyMoves[2][busyIndex] = 1;
                     busyIndex++;
                     }
                  }
               break;
            case TR::Float:
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lfs, firstNode, REAL_REGISTER(REGNUM(ai)),
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
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
               if (freeScratchable.isSet(ai))
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lfd, firstNode, REAL_REGISTER(REGNUM(ai)),
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 8, self()->cg()), cursor);
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
            default:
               break;
            }
         }
      }

   if (!fsd || !saveOnly)
      {
      bool     freeMore = true;
      int32_t  numMoves = busyIndex;

      while (freeMore && numMoves>0)
         {
         freeMore = false;
         for (i1=0; i1<busyIndex; i1++)
            {
            int32_t source = busyMoves[0][i1];
            int32_t target = busyMoves[1][i1];
            if (!(target<0) && freeScratchable.isSet(target))
               {
               TR::InstOpCode::Mnemonic op = TR::InstOpCode::bad;
               int32_t length = 0;

               switch(busyMoves[2][i1])
                  {
                  case 0:
                     cursor = generateTrg1Src1Instruction(self()->cg(), (source<=TR::RealRegister::LastGPR)?TR::InstOpCode::mr:TR::InstOpCode::fmr,
                              firstNode, REAL_REGISTER(REGNUM(target)), REAL_REGISTER(REGNUM(source)), cursor);
                     freeScratchable.set(source);
                     break;
                  case 1:
                     op = TR::InstOpCode::lwz;
                     length = 4;
                     break;
                  case 2:
                     op = TR::InstOpCode::ld;
                     length = 8;
                     break;
                  case 3:
                     op = TR::InstOpCode::lfs;
                     length = 4;
                     break;
                  case 4:
                     op = TR::InstOpCode::lfd;
                     length = 8;
                     break;
                  }

               if (busyMoves[2][i1] != 0)
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(), op, firstNode,
                              REAL_REGISTER(REGNUM(target)),
                              new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, source, length, self()->cg()),
                              cursor);
                  }

               freeScratchable.reset(target);
               freeMore = true;
               busyMoves[0][i1] = busyMoves[1][i1] = -1;
               numMoves--;
               }
            }
         }

      TR_ASSERT(numMoves<=0, "Circular argument register dependency can and should be avoided.");
      }

   return(cursor);
   }


/**
 * @brief Flush or load the arguments onto or from the stack
 * @param[in] flush : sets the operating mode of the function(e.g. flush arguemnts if bool flush is true; Load arguments otherwise)
 * @param[in] cursor: preceding instruction
 */
TR::Instruction *OMR::Power::Linkage::flushOrLoadUpArguments(TR::Instruction *cursor, bool flush)
   {
   if (!flush && !self()->cg()->buildInterpreterEntryPoint())
      // would be better to use a different linkage for this purpose
      return cursor;

   TR::Machine *machine = self()->machine();
   TR::RealRegister      *stackPtr   = self()->cg()->getStackPointerRegister();
   TR::ResolvedMethodSymbol      *bodySymbol = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol>   paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   TR::Node                 *firstNode = self()->comp()->getStartTree()->getNode();
   int32_t                  numIntArgs = 0, numFloatArgs = 0;
   const TR::PPCLinkageProperties& properties = self()->getProperties();

   while ( (paramCursor!=NULL) &&
           ( (numIntArgs < properties.getNumIntArgRegs()) ||
             (numFloatArgs < properties.getNumFloatArgRegs()) ) )
      {
      TR::RealRegister     *argRegister;
      int32_t                 offset = paramCursor->getParameterOffset();

      // If parm is referenced or required to be on stack (i.e. FSD), we have to flush or load.
      bool hasToStoreOrLoadStack = paramCursor->isReferencedParameter() || paramCursor->isParmHasToBeOnStack();

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (hasToStoreOrLoadStack &&
                  numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               if (flush)
                  {
                  cursor = generateMemSrc1Instruction(self()->cg(), TR::InstOpCode::stw, firstNode,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()),
                     argRegister, cursor);
                  }
               else
                  {
                  cursor = cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, argRegister,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
                  }
               }
            numIntArgs++;
            break;
         case TR::Address:
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               if (flush)
                  {
                  cursor = generateMemSrc1Instruction(self()->cg(),TR::InstOpCode::Op_st, firstNode,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, TR::Compiler->om.sizeofReferenceAddress(), self()->cg()),
                     argRegister, cursor);
                  }
               else
                  {
                  cursor = generateTrg1MemInstruction(self()->cg(),TR::InstOpCode::Op_load, firstNode, argRegister,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, TR::Compiler->om.sizeofReferenceAddress(), self()->cg()), cursor);
                  }
               }
            numIntArgs++;
            break;
         case TR::Int64:
            if (hasToStoreOrLoadStack &&
                  numIntArgs<properties.getNumIntArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs));
               if (self()->comp()->target().is64Bit())
                  {
                     if (flush)
                        {
                        cursor = generateMemSrc1Instruction(self()->cg(),TR::InstOpCode::Op_st, firstNode,
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 8, self()->cg()),
                           argRegister, cursor);
                        }
                     else
                        {
                        cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::ld, firstNode, argRegister,
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 8, self()->cg()), cursor);
                        }
                  }
               else
                  {
                  if (flush)
                     {
                     cursor = generateMemSrc1Instruction(self()->cg(), TR::InstOpCode::stw, firstNode,
                        new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()),
                        argRegister, cursor);
                     }
                  else
                     {
                     cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, argRegister,
                        new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, 4, self()->cg()), cursor);
                     }

                  if (numIntArgs < properties.getNumIntArgRegs()-1)
                     {
                     argRegister = machine->getRealRegister(properties.getIntegerArgumentRegister(numIntArgs+1));
                     if (flush)
                        {
                        cursor = generateMemSrc1Instruction(self()->cg(), TR::InstOpCode::stw, firstNode,
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset+4, 4, self()->cg()),
                           argRegister, cursor);
                        }
                     else
                        {
                        cursor = generateTrg1MemInstruction(self()->cg(), TR::InstOpCode::lwz, firstNode, argRegister,
                           new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset+4, 4, self()->cg()), cursor);
                        }
                     }
                  }
               }
            if (self()->comp()->target().is64Bit())
               numIntArgs++;
            else
               numIntArgs+=2;
            break;

         case TR::Float:
         case TR::Double:
            if (hasToStoreOrLoadStack &&
                  numFloatArgs<properties.getNumFloatArgRegs())
               {
               argRegister = machine->getRealRegister(properties.getFloatArgumentRegister(numFloatArgs));

               TR::InstOpCode::Mnemonic op;
               int32_t length;

               if (paramCursor->getDataType() == TR::Float)
                  {
                  op = flush ? TR::InstOpCode::stfs : TR::InstOpCode::lfs; length = 4;
                  }
               else
                  {
                  op = flush ? TR::InstOpCode::stfd : TR::InstOpCode::lfd; length = 8;
                  }

               cursor = flush ? generateMemSrc1Instruction(self()->cg(), op, firstNode,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, length, self()->cg()),
                     argRegister, cursor):
                     generateTrg1MemInstruction(self()->cg(), op, firstNode, argRegister,
                     new (self()->trHeapMemory()) TR::MemoryReference(stackPtr, offset, length, self()->cg()), cursor);
               }
            numFloatArgs++;
            break;
         }
      paramCursor = paramIterator.getNext();
      }
   return(cursor);
   }

TR::Register *OMR::Power::Linkage::pushIntegerWordArg(TR::Node *child)
   {
   TR::Register *pushRegister = NULL;
   TR_ASSERT(child->getDataType() != TR::Address, "assumption violated");
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      pushRegister = self()->cg()->allocateRegister();
      loadConstant(self()->cg(), child, (int32_t)child->get64bitIntegralValue(), pushRegister);
      child->setRegister(pushRegister);
      }
   else
      {
      pushRegister = self()->cg()->evaluate(child);
      }
   self()->cg()->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::Power::Linkage::pushAddressArg(TR::Node *child)
   {
   TR::Register *pushRegister = NULL;
   TR_ASSERT(child->getDataType() == TR::Address, "assumption violated");
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      bool isClass = child->isClassPointerConstant();
      pushRegister = self()->cg()->allocateRegister();
      if (isClass && self()->cg()->wantToPatchClassPointer((TR_OpaqueClassBlock*)child->getAddress(), child))
         loadAddressConstantInSnippet(self()->cg(), child, child->getAddress(), pushRegister, NULL,TR::InstOpCode::Op_load, NULL, NULL);
      else
         {
         if (child->isMethodPointerConstant())
            loadAddressConstant(self()->cg(), self()->cg()->comp()->compileRelocatableCode(), child, child->getAddress(), pushRegister, NULL, false, TR_RamMethodSequence);
         else
            loadAddressConstant(self()->cg(), self()->cg()->comp()->compileRelocatableCode(), child, child->getAddress(), pushRegister);
         }
      child->setRegister(pushRegister);
      }
   else
      {
      pushRegister = self()->cg()->evaluate(child);
      }
   self()->cg()->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::Power::Linkage::pushThis(TR::Node *child)
   {
   TR::Register *tempRegister = self()->cg()->evaluate(child);
   self()->cg()->decReferenceCount(child);
   return tempRegister;
   }

TR::Register *OMR::Power::Linkage::pushLongArg(TR::Node *child)
   {
   TR::Register *pushRegister = NULL;
   if (child->getRegister() == NULL && child->getOpCode().isLoadConst())
      {
      if (self()->comp()->target().is64Bit())
         {
         pushRegister = self()->cg()->allocateRegister();
         loadConstant(self()->cg(), child, child->getLongInt(), pushRegister);
         }
      else
         {
         TR::Register   *lowRegister = self()->cg()->allocateRegister();
         TR::Register   *highRegister = self()->cg()->allocateRegister();
         pushRegister = self()->cg()->allocateRegisterPair(lowRegister, highRegister);
         loadConstant(self()->cg(), child, child->getLongIntLow(), lowRegister);
         loadConstant(self()->cg(), child, child->getLongIntHigh(), highRegister);
         }
      child->setRegister(pushRegister);
      }
   else
      {
      pushRegister = self()->cg()->evaluate(child);
      }
   self()->cg()->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::Power::Linkage::pushFloatArg(TR::Node *child)
   {
   TR::Register *pushRegister = self()->cg()->evaluate(child);
   self()->cg()->decReferenceCount(child);
   return(pushRegister);
   }

TR::Register *OMR::Power::Linkage::pushDoubleArg(TR::Node *child)
   {
   TR::Register *pushRegister = self()->cg()->evaluate(child);
   self()->cg()->decReferenceCount(child);
   return(pushRegister);
   }

int32_t
OMR::Power::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
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

TR_ReturnInfo OMR::Power::Linkage::getReturnInfoFromReturnType(TR::DataType returnType)
   {
   switch (returnType)
      {
      case TR::NoType:
         return TR_VoidReturn;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         return TR_IntReturn;
      case TR::Int64:
         return TR_LongReturn;
      case TR::Address:
         return self()->comp()->target().is64Bit() ? TR_ObjectReturn : TR_IntReturn;
      case TR::Float:
         return TR_FloatReturn;
      case TR::Double:
         return TR_DoubleReturn;
      }

   TR_ASSERT(false, "Unhandled return type");

   return TR_VoidReturn;
   }
