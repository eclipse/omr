/*******************************************************************************
 * Copyright IBM Corp. and others 2019
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"

void TR::RVLinkageProperties::initialize()
   {
   /*
    * Note: following code depends on zero initialization of all members!
    */

   _numAllocatableIntegerRegisters = TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1;
   _numAllocatableFloatRegisters = TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1;

   _firstIntegerArgumentRegister = 0;
   _numIntegerArgumentRegisters = 0;
   _firstIntegerReturnRegister = 0;
   uint8_t numIntegerReturnRegisters = 0;

   for (auto regNum = TR::RealRegister::FirstGPR; regNum <= TR::RealRegister::LastGPR; regNum++)
      {
      if (_registerFlags[regNum] & RV_Reserved)
         {
         _numAllocatableIntegerRegisters--;
         }
      if (_registerFlags[regNum] & IntegerArgument)
         {
         _argumentRegisters[_firstIntegerArgumentRegister + _numIntegerArgumentRegisters++] = regNum;
         }
      if (_registerFlags[regNum] & IntegerReturn)
         {
         _returnRegisters[_firstIntegerReturnRegister + numIntegerReturnRegisters++] = regNum;
         }
      }

   _firstFloatArgumentRegister = _firstIntegerArgumentRegister + _numIntegerArgumentRegisters;
   _numFloatArgumentRegisters = 0;
   _firstFloatReturnRegister = _firstIntegerArgumentRegister + numIntegerReturnRegisters;
   uint8_t numFloatReturnRegisters = 0;

   for (auto regNum = TR::RealRegister::FirstFPR; regNum <= TR::RealRegister::LastFPR; regNum++)
         {
         if (_registerFlags[regNum] & RV_Reserved)
            {
            _numAllocatableFloatRegisters--;
            }
         if (_registerFlags[regNum] & FloatArgument)
            {
            _argumentRegisters[_firstFloatArgumentRegister + _numFloatArgumentRegisters++] = regNum;
            }
         if (_registerFlags[regNum] & FloatReturn)
            {
            _returnRegisters[_firstFloatReturnRegister + numFloatReturnRegisters++] = regNum;
            }
         }

   // TODO: following is safe default, can probably be lower. Q: how is this number computed?
   _numberOfDependencyRegisters =   (TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1)
                                  + (TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1);

   }

void OMR::RV::Linkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   /* do nothing */
   }

void OMR::RV::Linkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   /* do nothing */
   }

void OMR::RV::Linkage::initRVRealRegisterLinkage()
   {
   /* do nothing */
   }

void OMR::RV::Linkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   /* do nothing */
   }

TR::MemoryReference *OMR::RV::Linkage::getOutgoingArgumentMemRef(TR::Register *argMemReg, int32_t offset, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::RVMemoryArgument &memArg)
   {
   TR::Machine *machine = cg()->machine();
   const TR::RVLinkageProperties& properties = self()->getProperties();

   TR::MemoryReference *result = TR::MemoryReference::createWithDisplacement(cg(), argMemReg, offset); // post-increment
   memArg.argRegister = argReg;
   memArg.argMemory = result;
   memArg.opCode = opCode; // opCode must be post-index form

   return result;
   }

TR::Instruction *OMR::RV::Linkage::saveArguments(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();

   return cursor;
   }

TR::Instruction *OMR::RV::Linkage::loadUpArguments(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();

   return cursor;
   }

TR::Instruction *OMR::RV::Linkage::flushArguments(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();

   return cursor;
   }

TR::Register *OMR::RV::Linkage::pushIntegerWordArg(TR::Node *child)
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

TR::Register *OMR::RV::Linkage::pushAddressArg(TR::Node *child)
   {
   TR_ASSERT(child->getDataType() == TR::Address, "assumption violated");
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::RV::Linkage::pushLongArg(TR::Node *child)
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

TR::Register *OMR::RV::Linkage::pushFloatArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

TR::Register *OMR::RV::Linkage::pushDoubleArg(TR::Node *child)
   {
   TR::CodeGenerator *cg = self()->cg();
   TR::Register *pushRegister = cg->evaluate(child);
   cg->decReferenceCount(child);
   return pushRegister;
   }

int32_t
OMR::RV::Linkage::numArgumentRegisters(TR_RegisterKinds kind)
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
         return TR::InstOpCode::_fld;
      case TR::Float:
         return TR::InstOpCode::_flw;
      case TR::Int64:
      case TR::Address:
         return TR::InstOpCode::_ld;
      default:
         return TR::InstOpCode::_lw;
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
         return TR::InstOpCode::_fsd;
      case TR::Float:
         return TR::InstOpCode::_fsw;
      case TR::Int64:
      case TR::Address:
         return TR::InstOpCode::_sd;
      default:
         return TR::InstOpCode::_sw;
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


TR::Instruction *
OMR::RV::Linkage::copyParametersToHomeLocation(TR::Instruction *cursor, bool parmsHaveBeenStored)
   {
   TR::Machine *machine = cg()->machine();
   const TR::RVLinkageProperties& properties = getProperties();
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
   // the required register-to-register moves.
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
            loadCursor = generateLOAD(getOpCodeForParmLoads(paramType), NULL, machine->getRealRegister(ai), stackMR, cg(), loadCursor);
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
               cursor = generateSTORE(getOpCodeForParmStores(paramType), NULL, stackMR, linkageReg, cg(), cursor);
               }
            }

         // Copy to the ai register if necessary
         //
         if (isGlobalRegisterAssigned(paramCursor) && ai != sourceIndex)
            {
            // This parameter needs a register-to-register move.  We don't know yet whether
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

   // Phase 2: Iterate through the parameters again to insert the register-to-register moves.
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

         // Work our way backward along the chain, generating all the necessary moves
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
               cursor = generateRTYPE((paramType == TR::Double) ? TR::InstOpCode::_fsgnj_d : TR::InstOpCode::_fsgnj_s,
                        NULL, trgReg, srcReg, srcReg, cg(), cursor);
               }
            else
               {
               cursor = generateITYPE((paramType == TR::Int64) || (paramType == TR::Address) ? TR::InstOpCode::_addi : TR::InstOpCode::_addiw,
                        NULL, trgReg, srcReg, 0, cg(), cursor);
               }

            // Update movStatus as we go so we don't generate redundant moves
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
