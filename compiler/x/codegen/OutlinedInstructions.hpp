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

#ifndef OUTLINEDINSTRUCTIONS_INCL
#define OUTLINEDINSTRUCTIONS_INCL

#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"
#include "x/codegen/X86Ops.hpp"

class TR_RegisterAssignerState;
namespace TR { class X86VFPSaveInstruction; }
namespace OMR { class RegisterUsage; }
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
template <typename ListKind> class List;

class TR_OutlinedInstructions
   {
   friend class TR_OutlinedInstructionsGenerator;

   TR::LabelSymbol      *_entryLabel;
   TR::LabelSymbol      *_restartLabel;
   TR::Instruction      *_firstInstruction;
   TR::Instruction      *_appendInstruction;
   TR_X86OpCodes        _targetRegMovOpcode;

   TR::Block            *_block;
   TR::CodeGenerator    *_cg;

   TR::Node             *_callNode;
   TR::Register         *_targetReg;

   TR::RegisterDependencyConditions *_postDependencyMergeList;

   // Non-linear register assigner fields.
   //
   TR::list<OMR::RegisterUsage*> *_outlinedPathRegisterUsageList;
   TR::list<OMR::RegisterUsage*> *_mainlinePathRegisterUsageList;
   TR_RegisterAssignerState      *_registerAssignerStateAtMerge;

   bool                 _hasBeenRegisterAssigned;

   TR::Compilation *comp() { return TR::comp(); }
   TR::CodeGenerator *cg() { return _cg; }

   public:

   TR_ALLOC(TR_Memory::OutlinedCode)

   // For (almost) arbitrary simple instructions
   //
   // NOTE: currently, the exception tables are not set up properly for
   // TR_OutlinedInstructions created with this constructor.  Therefore, be
   // careful that no instructions you put in here can throw or cause GC.  In
   // particular, NO CALL INSTRUCTIONS.  (Use the other constructor for calls.)
   //
   // NOTE: you don't want to do tree-evaluation here, because that will cause
   // problems if some of the trees evaluated are commoned.  Essentially, only
   // do in a TR_OutlinedInstructions what you could have done in a normal
   // in-line internal control flow region.
   //
   // NOTE: use this constructor directly is deprecated, for new development,
   // use TR_OutlinedInstructionsGenerator instead.
   // TR_OutlinedInstructionsGenerator will setup all necessary information for
   // gc map and exception table.
   //
   TR_OutlinedInstructions(TR::LabelSymbol *entryLabel, TR::CodeGenerator *cg);

   void swapInstructionListsWithCompilation();

   // For calls
   //
   TR_OutlinedInstructions(TR::Node *callNode, TR::ILOpCodes callOp, TR::Register *targetReg, TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR::CodeGenerator *cg);

   public:

   TR::LabelSymbol *getEntryLabel()         {return _entryLabel;}
   void setEntryLabel(TR::LabelSymbol *sym) {_entryLabel = sym;}

   TR::LabelSymbol *getRestartLabel()         {return _restartLabel;}
   void setRestartLabel(TR::LabelSymbol *sym) {_restartLabel = sym;}

   TR::Instruction *getFirstInstruction()         {return _firstInstruction;}
   void setFirstInstruction(TR::Instruction *ins) {_firstInstruction = ins;}

   TR::Instruction *getAppendInstruction()         {return _appendInstruction;}
   void setAppendInstruction(TR::Instruction *ins) {_appendInstruction = ins;}

   void setPostDependencyMergeList(TR::RegisterDependencyConditions *deps) {_postDependencyMergeList = deps;}
   TR::RegisterDependencyConditions *getPostDependencyMergeList() {return _postDependencyMergeList;}

   TR::Block *getBlock()       {return _block;}
   void setBlock(TR::Block *b) {_block = b;}

   TR::list<OMR::RegisterUsage*> *getOutlinedPathRegisterUsageList() { return _outlinedPathRegisterUsageList; }
   void setOutlinedPathRegisterUsageList(TR::list<OMR::RegisterUsage*> *rul) { _outlinedPathRegisterUsageList = rul; }

   TR::list<OMR::RegisterUsage*> *getMainlinePathRegisterUsageList() { return _mainlinePathRegisterUsageList; }
   void setMainlinePathRegisterUsageList(TR::list<OMR::RegisterUsage*> *rul) { _mainlinePathRegisterUsageList = rul; }

   TR_RegisterAssignerState *getRegisterAssignerStateAtMerge() { return _registerAssignerStateAtMerge; }
   void setRegisterAssignerStateAtMerge(TR_RegisterAssignerState *ras) { _registerAssignerStateAtMerge = ras; }

   bool hasBeenRegisterAssigned()          {return _hasBeenRegisterAssigned;}
   void setHasBeenRegisterAssigned(bool r) {_hasBeenRegisterAssigned = r;}

   void assignRegisters(TR_RegisterKinds kindsToBeAssigned, TR::X86VFPSaveInstruction  *vfpSaveInstruction);
   void assignRegistersOnOutlinedPath(TR_RegisterKinds kindsToBeAssigned, TR::X86VFPSaveInstruction *vfpSaveInstruction);

   OMR::RegisterUsage *findInRegisterUsageList(TR::list<OMR::RegisterUsage*> *rul, TR::Register *virtReg);

   TR::Node *createOutlinedCallNode(TR::Node *callNode, TR::ILOpCodes callOp);

   TR::Node *getCallNode()       {return _callNode;}
   void setCallNode(TR::Node *n) {_callNode = n;}

   void generateOutlinedInstructionsDispatch();

   TR::RegisterDependencyConditions  *formEvaluatedArgumentDepList();

   };

/**
   @class TR_OutlinedInstructionsGenerator
   @brief This class is used to switch to outlined code generation in the manner of the RAII idiom.

   The main use of this class is to generate instruction in the outlined code region, for example, cold paths codes.
*/
class TR_OutlinedInstructionsGenerator
   {
   public:
   /**
      @brief Switch to outlined code generation.

      @param entryLabel: the entry label of generated outlined code.
      @param node      : the node of which generated outlined code belongs to.
      @param cg        : the code generator.
   */
   TR_OutlinedInstructionsGenerator(TR::LabelSymbol* entryLabel, TR::Node* node, TR::CodeGenerator* cg);
   /**
      @brief Switch back to mainline code generation.
   */
   ~TR_OutlinedInstructionsGenerator();

   /**
    * @brief Performs post outlined instruction generation cleanup
    *
    * This function *must* be called after an outline instruction sequence
    * is generated.
    *
    */
   void endOutlinedInstructionSequence();

   private:
   TR_OutlinedInstructions* _oi;
   bool _hasEnded;
   };

#endif
