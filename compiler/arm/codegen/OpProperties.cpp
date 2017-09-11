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

#include "codegen/ARMOps.hpp"

const uint32_t TR_ARMOpCode::properties[ARMNumOpCodes] =
{
// bad
0,

// add
ARMOpProp_HasRecordForm,

// add_r
ARMOpProp_IsRecordForm,

// adc
ARMOpProp_HasRecordForm,

// adc_r
ARMOpProp_IsRecordForm,

// and
ARMOpProp_HasRecordForm,

// and_r
ARMOpProp_IsRecordForm,

// b
ARMOpProp_BranchOp,

// bl
ARMOpProp_BranchOp,

// bic
ARMOpProp_HasRecordForm,

// bic_r
ARMOpProp_IsRecordForm,

// cmp
ARMOpProp_IsRecordForm,

// cmn
ARMOpProp_IsRecordForm,

// eor
ARMOpProp_HasRecordForm,

// eor_r
ARMOpProp_IsRecordForm,

// ldfs
0,

// ldfd
0,

// ldm
0,

// ldmia
0,

// ldr
0,

// ldrb
0,

// ldrsb
ARMOpProp_Arch4,

// ldrh
ARMOpProp_Arch4,

// ldrsh
ARMOpProp_Arch4,

// mul
ARMOpProp_HasRecordForm,

// mul_r
ARMOpProp_IsRecordForm,

// mla
ARMOpProp_HasRecordForm,

// mla_r
ARMOpProp_IsRecordForm,

// smull
ARMOpProp_HasRecordForm,

// smull_r
ARMOpProp_IsRecordForm,

// umull
ARMOpProp_HasRecordForm,

// umull_r
ARMOpProp_IsRecordForm,

// smlal
ARMOpProp_HasRecordForm,

// smlal_r
ARMOpProp_IsRecordForm,

// umlal
ARMOpProp_HasRecordForm,

// umlal_r
ARMOpProp_IsRecordForm,

// mov
ARMOpProp_HasRecordForm,

// mov_r
ARMOpProp_IsRecordForm,

// mvn
ARMOpProp_HasRecordForm,

// mvn_r
ARMOpProp_IsRecordForm,

// orr
ARMOpProp_HasRecordForm,

// orr_r
ARMOpProp_IsRecordForm,

// teq
ARMOpProp_IsRecordForm,

// tst
ARMOpProp_IsRecordForm,

// sub
ARMOpProp_HasRecordForm,

// sub_r
ARMOpProp_IsRecordForm,

// sbc
ARMOpProp_HasRecordForm,

// sbc_r
ARMOpProp_IsRecordForm,

// rsb
ARMOpProp_HasRecordForm,

// rsb_r
ARMOpProp_IsRecordForm,

// rsc
ARMOpProp_HasRecordForm,

// rsc_r
ARMOpProp_IsRecordForm,

// stfs
0,

// stfd
0,

// str
0,

// strb
0,

// strh
ARMOpProp_Arch4,

// stm
0,

// stmdb
0,

// swp
0,

// fence
0,

// ret
0,

// wrtbar
0,

// proc
0,

// dd
0,

// dmb_v6
0,

// dmb
0,

// dmb_st
0,

// ldrex
0,

// strex
0,

// iflong
0,

// setblong
0,

// setbool
0,

// setbflt
0,

// lcmp
0,

// flcmpl
0,

// flcmpg
0,

// idiv
0,

// irem
0,

// Label
ARMOpProp_Label,

// vgdnop
0,

// nop
0,

// fabsd (vabs<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fabss (vabs<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// faddd (vadd<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fadds (vadd<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fcmpd (vcmp<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fcmps (vcmp<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fcmpzd (vcmp<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fcmpzs (vcmp<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fcpyd (vmov<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fcpys (vmov<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fcvtds (vcvt<c>.f64.f32)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fcvtsd (vcvt<c>.f32.f64)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fdivd (vdiv<c>.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fdivs (vdiv<c>.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fldd  (vldr)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fldmd (vldm)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fldms (vldm)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// flds (vldr)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmacd (vmla.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fmacs (vmla.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmdrr (vmov)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fmrrd (vmov)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fmrrs (vmov)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmrs (vmov)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmrx (vmrs)
ARMOpProp_VFP,

// fmscd (vmls.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fmscs (vmls.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmsr (vmov)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmsrr (vmov)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmstat (vmrs APSR_nzcv, FPSCR)
ARMOpProp_IsRecordForm,

// fmuld (vmul.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fmuls (vmul.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fmxr (vmsr)
ARMOpProp_VFP,

// fnegd (vneg.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fnegs (vneg.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fnmacd (vnmla.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fnmacs (vnmla.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fnmscd (vnmls.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fnmscs (vnmls.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fnmuld (vnmul.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fnmuls (vnmul.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fsitod (vcvt.f64.s32)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fsitos (vcvt.f32.s32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fsqrtd (vsqrt.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fsqrts (vsqrt.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fstd (vstr)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fstmd (vstm)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fstms (vstm)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fsts (vstr)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// fsubd (vsub.f64)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fsubs (vsub.f32)
ARMOpProp_SingleFP |
ARMOpProp_VFP,

// ftosizd (vcvt.s32.f64)
ARMOpProp_VFP,

// ftosizs (vcvt.s32.f32)
ARMOpProp_VFP,

// ftouizd (vcvt.u32.f64)
ARMOpProp_VFP,

// ftouizs (vcvt.u32.f32)
ARMOpProp_VFP,

// fuitod (vcvt.f64,u32)
ARMOpProp_DoubleFP |
ARMOpProp_VFP,

// fuitos (vcvt.f32.u32)
ARMOpProp_SingleFP |
ARMOpProp_VFP

};
