/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#ifndef OMR_Z_PEEPHOLE_INCL
#define OMR_Z_PEEPHOLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_PEEPHOLE_CONNECTOR
#define OMR_PEEPHOLE_CONNECTOR
namespace OMR { namespace Z { class Peephole; } }
namespace OMR { typedef OMR::Z::Peephole PeepholeConnector; }
#else
#error OMR::Z::Peephole expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRPeephole.hpp"

#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE Peephole : public OMR::Peephole
   {
   public:

   Peephole(TR::Compilation* comp);

   virtual bool performOnInstruction(TR::Instruction* cursor);

   private:

   /** \brief
    *     Tries to reduce L[' '|FH|G] R,MR1  ST[' '|FH|G] R,MR2 sequences to MVC MR2, MR1
    *     to save a register and instruction.
    *
    *  \param storeOpCode
    *     The store op code that matches the load.
    *
    *  \param size
    *     The number of bits being moved.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryLoadStoreReduction(TR::InstOpCode::Mnemonic storeOpCode, uint16_t size);

   /** \brief
    *     Tries to fold a load register instruction (\c LR or \c LGR) into a subsequent three-operand instruction if
    *     possible. For example:
    *
    *     <code>
    *     LR GPR2,GPR3
    *     LR GPR5,GPR6
    *     AHI GPR2,5
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LR GPR5,GPR6
    *     AHIK GPR2,GPR3,5
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToFoldLoadRegisterIntoSubsequentInstruction();

   /** \brief
    *     Tries to forward a branch target if the branch instruction transfers control to another unconditional
    *     branch instruction (i.e. a trampoline). For example:
    *
    *     <code>
    *     CIJ GPR2,0,B'1000',<LABEL_1>
    *     ...
    *     LABEL_1:
    *     BRC B'1000',<LABEL_999>
    *     ...
    *     LABEL_999:
    *     ...
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     CIJ GPR2,0,B'1000',<LABEL_999>
    *     ...
    *     LABEL_1:
    *     BRC B'1000',<LABEL_999>
    *     ...
    *     LABEL_999:
    *     ...
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToForwardBranchTarget();

   /** \brief
    *     Tries to reduce a 64-bit shift instruction to a 32-bit shift instruction based on the IL node associated
    *     with the 64-bit shift. For example if the node which generated the 64-bit shift operation is a \c TR::ishl
    *     then the 64-bit shfit:
    *
    *     <code>
    *     SLAK GPR15,GPR15,0(GPR3)
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     SLA GPR15,0(GPR3)
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduce64BitShiftTo32BitShift();

   /** \brief
    *     Tries to reduce AGIs (Address Generation Interlock) which occur when an instruction requires a register in
    *     its operand address calculation but the register is unavailable because it is written to by a preceeding
    *     instruction which has not been completed yet. This causes a pipeline stall which we would like to avoid. In
    *     some cases however the AGI can be avoided by register renaming. For example, the following:
    *
    *     <code>
    *     LTGR GPR3,GPR8
    *     AGRK GPR2,GPR11,GPR12
    *     LOCGR GPR11,16(GPR3),B'1000'
    *     </code>
    *
    *     can be register renamed to:
    *
    *     <code>
    *     LTGR GPR3,GPR8
    *     AGRK GPR2,GPR11,GPR12
    *     LOCGR GPR11,16(GPR8),B'1000'
    *     </code>
    *
    *     which avoids the AGI because \c GPR3 is no longer used within the memory reference.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceAGI();

   /** \brief
    *     Tries to reduce a compare logical (\c CLR) insturction followed by a branch to a compare and branch
    *     instruction (\c CLRJ) For example:
    *
    *     <code>
    *     CLR GPR1,GPR2
    *     BRC B'1000',<LABEL>
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     CLRJ GPR1,GPR2,<LABEL>,B'1000'
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceCLRToCLRJ();

   /** \brief
    *     Tries to reduce a simple branch conditional load of an immediate to a load immediate on condition branch-
    *     less sequence. For example:
    *
    *     <code>
    *     CRJ GPR2,GPR3,B'1010',<LABEL>
    *     LHI GPR4,55
    *     LABEL:
    *     ...
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     CR GPR2,GPR3
    *     LOCHI GPR4,55,B'1010'
    *     ...
    *     </code>
    *
    *  \param compareMnemonic
    *     The compare mnemonic which will replace the compare and branch instruction and feed the condition code into
    *     the conditional load.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceCRJLHIToLOCHI(TR::InstOpCode::Mnemonic compareMnemonic);

   /** \brief
    *     Tries to reduce a load instruction (\c L) to an insert character under mask (\c ICM) instruction. This can
    *     be done if following the load we have a load and test or a compare against certain immediates. For example:
    *
    *     <code>
    *     L GPR2,8(GPR5)
    *     LTR GPR2,GPR2
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     ICM GPR2,8(GPR5),B'1111'
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLToICM();

   /** \brief
    *     Tries to reduce a load instruction (\c L, \c LG, \c LLGF) followed by an \c NILL to its equivalent load
    *     and zero rightmost byte equivalent instruction. For example:
    *
    *     <code>
    *     LG GPR2,8(GPR5)
    *     NILL GPR2,0xFF00
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LZRG GPR2,8(GPR5)
    *     </code>
    *
    *  \param loadAndZeroRightMostByteMnemonic
    *     The load and zero rightmost byte mnemonic to replace the original load with.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLToLZRF(TR::InstOpCode::Mnemonic loadAndZeroRightMostByteMnemonic);

   /** \brief
    *     Tries to reduce a load register instruction (\c LGR or \c LTGR) followed by a sign extension to \c LGFR.
    *     For example:
    *
    *     <code>
    *     LGR GPR2,GPR3
    *     LGFR GPR2,GPR2
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LGFR GPR2,GPR3
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLGRToLGFR();

   /** \brief
    *     Tries to reduce a load halfword immedaite instruction (\c LHI) to an XOR \c XR.
    *     For example:
    *
    *     <code>
    *     LHI GPR2,0
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     XR GPR2,GPR2
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLHIToXR();

   /** \brief
    *     Tries to reduce a load logical character instruction (\c LLC) followed by a zero extension to \c LLGC.
    *     For example:
    *
    *     <code>
    *     LLC GPR2,8(GPR3,GPR5)
    *     LGFR GPR2,GPR2
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LLGC GPR2,8(GPR3,GPR5)
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLLCToLLGC();

   /** \brief
    *     Tries to reduce a load register instruction (\c LR or \c LGR) and a future compare (\c CHI) against the
    *     target register to \c LTR or \c LTGR. For example:
    *
    *     <code>
    *     LR GPR2,GPR3
    *     IC GPR4,16(GPR2,GPR3)
    *     CHI GPR2,0
    *     BRC B'1000',<LABEL>
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LTR GPR2,GPR3
    *     IC GPR4,16(GPR2,GPR3)
    *     BRC B'1000',<LABEL>
    *     </code>
    *
    *     since none of the instructions between the \c LR and \c CHI set the condition code or define the source or
    *     target registers.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLRCHIToLTR();

   /** \brief
    *     Tries to reduce a load and test register instruction (\c LTR or \c LTGR) to a compare halfword immediate if
    *     the target register of the load is used in a future memory reference. This is an attempt to reduce the AGI
    *     incurred by using the target register. For example:
    *
    *     <code>
    *     LTR GPR2,GPR2
    *     BRC B'1000',<LABEL>
    *     AHI GPR3,6
    *     L GPR4,8(GPR2,GPR3)
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     CHI GPR2,0
    *     BRC B'1000',<LABEL>
    *     AHI GPR3,6
    *     L GPR4,8(GPR2,GPR3)
    *     </code>
    *
    *     which will avoid the AGI incurred by the memory refrence on the \c L instruction.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToReduceLTRToCHI();

   /** \brief
    *     Tries to remove duplicate (\c LR, \c LGR, \c LDR, \c LER) instructions which have the same source and
    *     target registers.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveDuplicateLR();

   /** \brief
    *     Tries to use a peephole window to look for and remove duplicate load [and test] register instructions
    *     which when executed would result in a NOP.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveDuplicateLoadRegister();

   /** \brief
    *     Tries to remove duplicate NILF instructions which target the same register and use the same immediate or
    *     redundant NILF instructions where the second NILF operation would not change the value of the target register.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveDuplicateNILF();

   /** \brief
    *     Tries to remove duplicate NILH instructions which target the same register and use the same immediate.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveDuplicateNILH();

   /** \brief
    *     Tries to remove redundant compare and trap instructions which can never be executed because a previous
    *     compare and trap instruction would have triggered a trap already.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveRedundantCompareAndTrap();

   /** \brief
    *     Tries to remove redundant \c LA instructions whose evaluation would result in a NOP.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveRedundantLA();

   /** \brief
    *     Tries to remove redundant shift instructions which can be fused into a single shift. For example:
    *
    *     <code>
    *     SRL GPR2,8(GPR2)
    *     SRL GPR2,4(GPR2)
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     SRL GPR2,12(GPR2)
    *     </code>
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveRedundantShift();

   /** \brief
    *     Tries to remove redundant \c LR or \c LGR instructions which are followed by a load and test instruction
    *     acting on the same source and target registers.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveRedundantLR();

   /** \brief
    *     Tries to remove redundant \c LTR or \c LTGR instructions if we can reuse the condition code from a
    *     previous arithmetic operation. For example:
    *
    *     <code>
    *     SLR GPR2,GPR3
    *     LTR GPR2,GPR2
    *     BRC B'1000',<LABEL>
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     SLR GPR2,GPR3
    *     BRC B'1010',<LABEL>
    *     </code>
    *
    *     Note the modified mask value of the \c BRC instruction.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool tryToRemoveRedundantLTR();

   /** \brief
    *     Tries to remove redundant 32 to 64 bit extensions with \c LGFR or \c LLGFR on register
    *     values originating from 32 bit loads if the 32 bit load instruction can be replaced with
    *     an equivalent extending 32 bit load. For example:
    *
    *     <code>
    *     L    R1,N(R2,R3)
    *     LGFR R1,R1
    *     </code>
    *
    *     can be reduced to:
    *
    *     <code>
    *     LGF  R1,N(R2,R3)
    *     </code>
    *
    *  \param isSigned
    *     true if operating on an LGFR instruction; false if LLGFR
    *
    *  \return
    *     true if the reduction was successful; false otherwise
    */
   bool tryToRemoveRedundant32To64BitExtend(bool isSigned);

   private:

   /// The instruction cursor currently being processed by the peephole optimization
   TR::Instruction* cursor;
   };

}

}

#endif
