/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef ARMOPS_INCL
#define ARMOPS_INCL

typedef enum  {
   ARMOp_bad,              //Illegal Opcode
   ARMOp_add,              // Add
   ARMOp_add_r,            // Add with record
   ARMOp_adc,              // Add with carry
   ARMOp_adc_r,            // Add with carry with record
   ARMOp_and,              // AND
   ARMOp_and_r,            // AND with record
   ARMOp_b,                // Unconditional branch
   ARMOp_bl,               // Branch and link
   ARMOp_bic,              // Bit clear (NAND)
   ARMOp_bic_r,            // Bit clear (NAND) with record
   ARMOp_cmp,              // Compare (rN - op2)
   ARMOp_cmn,              // Compare negated (rN + op2)
   ARMOp_eor,              // XOR
   ARMOp_eor_r,            // XOR with record
   ARMOp_ldfs,             // Load single word from memory to coprocessor register
   ARMOp_ldfd,             // Load double word from memory to coprocessor register
   ARMOp_ldm,              // Load multiple words
   ARMOp_ldmia,            // Load multiple words, increment after
   ARMOp_ldr,              // Load dword
   ARMOp_ldrb,             // Load byte and zero extend
   ARMOp_ldrsb,            // Load byte and sign extend
   ARMOp_ldrh,             // Load word and zero extend
   ARMOp_ldrsh,            // Load word and sign extend
   ARMOp_mul,              // Multiply
   ARMOp_mul_r,            // Multiply with record
   ARMOp_mla,              // Multiply and accumulate
   ARMOp_mla_r,            // Multiply and accumulate with record
   ARMOp_smull,            // Multiply Long Signed
   ARMOp_smull_r,          // Multiply Long Signed with record
   ARMOp_umull,            // Multiply Long Unsigned
   ARMOp_umull_r,          // Multiply Long Unsigned with record
   ARMOp_smlal,            // Multiply Long Signed and accumulate
   ARMOp_smlal_r,          // Multiply Long Signed and accumulate with record
   ARMOp_umlal,            // Multiply Long Unsigned and accumulate
   ARMOp_umlal_r,          // Multiply Long Unsigned and accumulate with record
   ARMOp_mov,              // Move
   ARMOp_mov_r,            // Move with record
   ARMOp_mvn,              // Move negated
   ARMOp_mvn_r,            // Move negated with record
   ARMOp_orr,              // OR
   ARMOp_orr_r,            // OR with record
   ARMOp_teq,              // Test (XOR)
   ARMOp_tst,              // Test (AND)
   ARMOp_sub,              // Subtract
   ARMOp_sub_r,            // Subtract with record
   ARMOp_sbc,              // Subtract with carry
   ARMOp_sbc_r,            // Subtract with carry with record
   ARMOp_rsb,              // Reverse Subtract
   ARMOp_rsb_r,            // Reverse Subtract with record
   ARMOp_rsc,              // Reverse Subtract with carry
   ARMOp_rsc_r,            // Reverse Subtract with carry with record
   ARMOp_stfs,             // Store single word from coprocessor register to memory
   ARMOp_stfd,             // Store double word from coprocessor register to memory
   ARMOp_str,              // Store dword
   ARMOp_strb,             // Store byte and zero extend
   ARMOp_strh,             // Store word and sign extend
   ARMOp_stm,              // Store multiple words
   ARMOp_stmdb,            // Store multiple words, decrement before
   ARMOp_swp,              // Swap dword
   ARMOp_fence,            // Fence
   ARMOp_ret,              // Return
   ARMOp_wrtbar,           // Write barrier directive
   ARMOp_proc,             // Entry to the method
   ARMOp_dd,               // define word
   ARMOp_dmb_v6,           // Data memory barrier
   ARMOp_dmb,              // Data memory barrier on ARMv7A
   ARMOp_dmb_st,           // Data write memory barrier on ARMv7A
   ARMOp_ldrex,            // Load exclusive
   ARMOp_strex,            // Store exclusive(conditional)
   ARMOp_iflong,           // compare and branch long
   ARMOp_setblong,         // compare long and set boolean
   ARMOp_setbool,          // compare and set boolean (int or simple float)
   ARMOp_setbflt,          // compare float and set boolean (complex float)
   ARMOp_lcmp,             // compare long and set result -1,0,1
   ARMOp_flcmpl,           // compare float and set result -1(less,unordered),0(equal),1(greater)
   ARMOp_flcmpg,           // compare float and set result -1(less),0(equal),1(greater,unordered)
   ARMOp_idiv,             // integer divide
   ARMOp_irem,             // integer remainder
   ARMOp_label,            // Destination of a jump
   ARMOp_vgdnop,           // A virtual guard encoded as a NOP instruction
   ARMOp_nop,              // NOP
// VFP instructions starts here
   ARMOp_fabsd,            // Absolute value double
   ARMOp_fabss,            // Absolute value single
   ARMOp_faddd,            // Add double
   ARMOp_fadds,            // Add float
   ARMOp_fcmpd,            // Compare double
   ARMOp_fcmps,            // Compare float
   ARMOp_fcmpzd,           // Compare double to 0.0
   ARMOp_fcmpzs,           // Compare float to 0.0
   ARMOp_fcpyd,            // Copy/Move double
   ARMOp_fcpys,            // Copy/Move float
   ARMOp_fcvtds,           // Convert float to double
   ARMOp_fcvtsd,           // Convert double to float
   ARMOp_fdivd,            // Divide double
   ARMOp_fdivs,            // Divide float
   ARMOp_fldd,             // VFP load double (coprocessor 11) = LDC cp11
   ARMOp_fldmd,            // VFP load multiple double
   ARMOp_fldms,            // VFP load multiple float
   ARMOp_flds,             // VFP load float (coprocessor 10) = LDC cp10
   ARMOp_fmacd,            // Multiply and accumulate double
   ARMOp_fmacs,            // Multiply and accumulate single
   ARMOp_fmdrr,            // Move to double from two registers
   ARMOp_fmrrd,            // Move from double to two registers
   ARMOp_fmrrs,            // Move from single(2) to two registers
   ARMOp_fmrs,             // Move from single to register
   ARMOp_fmrx,             // Move from system register to register
   ARMOp_fmscd,            // Multiply and subtract double
   ARMOp_fmscs,            // Multiply and subtract single
   ARMOp_fmsr,             // Move from register to single
   ARMOp_fmsrr,            // Move from two registers to single(2)
   ARMOp_fmstat,           // Move floating point flags back
   ARMOp_fmuld,            // Multiply double
   ARMOp_fmuls,            // Multiple float
   ARMOp_fmxr,             // Move to system register to register
   ARMOp_fnegd,            // Negate double
   ARMOp_fnegs,            // Negate float
   ARMOp_fnmacd,           // Negated multiply and accumulate double
   ARMOp_fnmacs,           // Negated multiply and accumulate single
   ARMOp_fnmscd,           // Negated multiply and subtract double
   ARMOp_fnmscs,           // Negated multiply and subtract single
   ARMOp_fnmuld,           // Negated multiply double
   ARMOp_fnmuls,           // Negated multiply single
   ARMOp_fsitod,           // Convert signed integer to double
   ARMOp_fsitos,           // Convert signed integer to float
   ARMOp_fsqrtd,           // Square root double
   ARMOp_fsqrts,           // Square root single
   ARMOp_fstd,             // VFP store double (coprocessor 11) = STC cp11
   ARMOp_fstmd,            // VFP store multiple double
   ARMOp_fstms,            // VFP store multiple float
   ARMOp_fsts,             // VFP store float (coprocessor 10) = STC cp10
   ARMOp_fsubd,            // Subtract double
   ARMOp_fsubs,            // Subtract float
   ARMOp_ftosizd,          // Convert double to signed integer (Round towards zero)
   ARMOp_ftosizs,          // Convert float to signed integer (Round towards zero)
   ARMOp_ftouizd,          // Convert double to unsigned integer (Round towards zero)
   ARMOp_ftouizs,          // Convert float to unsigned integer (Round towards zero)
   ARMOp_fuitod,           // Convert unsigned integer to double
   ARMOp_fuitos,           // Convert unsigned integer to float
// Last VFP instructions
   ARMLastOp = ARMOp_fuitos,
   ARMNumOpCodes = ARMLastOp + 1
} TR_ARMOpCodes;
#include "codegen/ARMOps2.hpp"

#endif
