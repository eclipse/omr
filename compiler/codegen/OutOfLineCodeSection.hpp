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

#ifndef OUTOFLINECODESECTION_INCL
#define OUTOFLINECODESECTION_INCL

#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "il/ILOpCodes.hpp"

namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }

/** \brief
 *
 *  Represents a sequence of instructions which will be emitted outside of the main-line instruction sequence with
 *  the register allocator favoring the allocation of registers that appear in the main-line instruction sequence.
 *
 *  \details
 *
 *  Out of line (OOL) code sections (paths) are typically used to bias internal control flow edges during register
 *  allocation. The local register allocator (RA) is unaware of control flow regions within a basic block and will
 *  assign the registers within the respective block in a linear pass.
 *
 *  Control flow within a basic block can be introduced by code generator evaluators; so called internal control
 *  flow (ICF) regions. Typically an ICF will look like a diamond:
 *
 *  \code
 *   ...
 *    |
 *  +-+-+     +---+
 *  | 3 + --> + 1 |
 *  +-+-+     +-+-+
 *    |         |     \
 *   ...       ...    | ICF region
 *    |         |     /
 *  +-+-+     +-+-+
 *  | 4 + <-- + 2 |
 *  +-+-+     +---+
 *    |
 *   ...
 *  \endcode
 *
 *  Block 3 represents a branch instruction which branches to block 1 representing a label. On both sides of the
 *  diamond ... represent some sequence of non-branch instructions. Block 2 represents a branch which branches to
 *  block 4 representing the merge label. Note that the sequence of instructions in each block are encoded linearly
 *  and the diamond diagram is drawn for the sake of example.
 *
 *  Because RA is unaware of ICF regions any virtual registers used inside of the ICF region must be attached as
 *  post register dependencies on the ICF merge label to ensure that no register shuffles or spills can occur
 *  inside of the ICF region. This must be done to ensure that the register allocation will not shuffle or spill
 *  any value on one side of the diamond but not the other.
 *
 *  If one side of the diamond is not often executed we could be unnecessarily introducing register shuffling or
 *  register spilling on the merge label to satisfy all post register dependencies, i.e. the union of all registers
 *  used on either side of the diamond. The OOL code section is used to improve the register allocation of the fast
 *  path by sacrificing performance of the slow path.
 *
 *  We define the fast path of the diamond to be the instructions most often executed and the slow path of the
 *  diamond to be the instructions least often executed. In our example above instructions between block 3 and block
 *  4 represent the fast path and instructions between block 1 and block 2 represent the slow path instructions.
 *
 *  Instructions in the slow path will be emitted inside of an OOL code section and the register allocation proceeds
 *  to register allocate the entire region as follows (assuming a backwards register allocation pass):
 *
 *  1. Register allocate block 4
 *  2. Register allocate fast path instructions
 *  3. Register allocate block 2
 *  4. Register allocate slow path instructions
 *  5. Register allocate block 1
 *  6. Register allocate block 3
 *
 *  The register allocator takes a snapshot of the register states, where the register state refers to all register
 *  associations, assignments, and states between live virtual and real registers at blocks 4 and 3 and restores the
 *  register state when register allocating blocks 2 and 1 respectively. The act of restoring the register state is
 *  achieved by creating register dependencies at the respective locations so as to satisfy the exact register state
 *  in the fast path of the ICF diamond. The register allocator also ensures that any spilled registers in the fast
 *  path are properly spilled in the slow path.
 *
 *  The net effect of using an OOL code section for the slow path of an ICF diamond is that the register allocator
 *  will register allocate the fast path of the ICF diamond as if no ICF region exists. In addition to the benefits
 *  achieved through register allocation OOL code sections will typically be binary encoded outside of the main-line
 *  instruction sequence and be placed towards the end of the method so as to improve i-cache coherence.
 */
class TR_OutOfLineCodeSection
   {

protected:

   TR::LabelSymbol       *_entryLabel;
   TR::LabelSymbol       *_restartLabel;
   TR::Instruction      *_firstInstruction;
   TR::Instruction      *_appendInstruction;

   TR::Block            *_block;
   TR::CodeGenerator    *_cg;

   TR::Node             *_callNode;
   TR::Register         *_targetReg;

   bool                 _hasBeenRegisterAssigned;

   void evaluateNodesWithFutureUses(TR::Node *node);

public:

   TR_ALLOC(TR_Memory::OutlinedCode)

   TR_OutOfLineCodeSection(TR::LabelSymbol *entryLabel, TR::CodeGenerator *cg);

   TR_OutOfLineCodeSection(TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR::CodeGenerator *cg);

   void swapInstructionListsWithCompilation();

   // For calls
   //
   TR_OutOfLineCodeSection(TR::Node *callNode, TR::ILOpCodes callOp, TR::Register *targetReg, TR::LabelSymbol *entryLabel, TR::LabelSymbol *restartLabel, TR::CodeGenerator *cg);

   TR::LabelSymbol *getEntryLabel()         { return _entryLabel; }
   void setEntryLabel(TR::LabelSymbol *sym) { _entryLabel = sym; }

   TR::LabelSymbol *getRestartLabel()         { return _restartLabel; }
   void setRestartLabel(TR::LabelSymbol *sym) { _restartLabel = sym; }

   TR::Instruction *getFirstInstruction()         { return _firstInstruction; }
   void setFirstInstruction(TR::Instruction *ins) { _firstInstruction = ins; }

   TR::Instruction *getAppendInstruction()         { return _appendInstruction; }
   void setAppendInstruction(TR::Instruction *ins) { _appendInstruction = ins; }

   TR::Block *getBlock()       { return _block; }
   void setBlock(TR::Block *b) { _block = b; }

   bool hasBeenRegisterAssigned()          { return _hasBeenRegisterAssigned; }
   void setHasBeenRegisterAssigned(bool r) { _hasBeenRegisterAssigned = r; }

   TR::Node *createOutOfLineCallNode(TR::Node *callNode, TR::ILOpCodes callOp);

   TR::Node *getCallNode()       { return _callNode; }
   void setCallNode(TR::Node *n) { _callNode = n; }

   void preEvaluatePersistentHelperArguments();
   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned) {}

   };

#endif
