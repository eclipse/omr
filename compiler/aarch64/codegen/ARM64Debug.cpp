/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include <stdint.h>
#include <string.h>

#include "ras/Debug.hpp"

#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64HelperCallSnippet.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64OutOfLineCodeSection.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "env/IO.hpp"
#include "il/Block.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "aarch64/codegen/CallSnippet.hpp"
#include "aarch64/codegen/StackCheckFailureSnippet.hpp"

namespace TR {
class ARM64ForceRecompilationSnippet;
}
#endif

static const char *ARM64ConditionNames[]
    = { "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv" };

static const char *ARM64ShiftCodeNames[] = { "lsl", "lsr", "asr", "???" };

static const char *ARM64ExtendCodeNames[] = { "uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx" };

static const char *opCodeToNameMap[] = {
    "assocreg",
    "bad",
    "dd",
    "fence",
    "label",
    "proc",
    "retn",
    "vgnop",
    "cbzw",
    "cbnzw",
    "cbzx",
    "cbnzx",
    "tbz",
    "tbnz",
    "b_cond",
    "brkarm64",
    "dsb",
    "dmb",
    "yield",
    "br",
    "blr",
    "ret",
    "b",
    "bl",
    "stxrb",
    "stlxrb",
    "ldxrb",
    "ldaxrb",
    "stlrb",
    "ldarb",
    "stxrh",
    "stlxrh",
    "ldxrh",
    "ldaxrh",
    "stlrh",
    "ldarh",
    "stxrw",
    "stlxrw",
    "stxpw",
    "stlxpw",
    "ldxrw",
    "ldaxrw",
    "ldxpw",
    "ldaxpw",
    "stlrw",
    "ldarw",
    "stxrx",
    "stlxrx",
    "stxpx",
    "stlxpx",
    "ldxrx",
    "ldaxrx",
    "ldxpx",
    "ldaxpx",
    "stlrx",
    "ldarx",
    "ldrw",
    "vldrs",
    "ldrx",
    "vldrd",
    "ldrsw",
    "vldrq",
    "prfm",
    "stnpw",
    "ldnpw",
    "vstnps",
    "vldnps",
    "vstnpd",
    "vldnpd",
    "stnpx",
    "ldnpx",
    "vstnpq",
    "vldnpq",
    "stppostw",
    "ldppostw",
    "vstpposts",
    "vldpposts",
    "ldpswpost",
    "vstppostd",
    "vldppostd",
    "stppostx",
    "ldppostx",
    "vstppostq",
    "vldppostq",
    "stpoffw",
    "ldpoffw",
    "vstpoffs",
    "vldpoffs",
    "ldpswoff",
    "vstpoffd",
    "vldpoffd",
    "stpoffx",
    "ldpoffx",
    "vstpoffq",
    "vldpoffq",
    "stpprew",
    "ldpprew",
    "vstppres",
    "vldppres",
    "ldpswpre",
    "vstppred",
    "vldppred",
    "stpprex",
    "ldpprex",
    "vstppreq",
    "vldppreq",
    "sturb",
    "ldurb",
    "ldursbx",
    "ldursbw",
    "vsturb",
    "vldurb",
    "vsturq",
    "vldurq",
    "sturh",
    "ldurh",
    "ldurshx",
    "ldurshw",
    "vsturh",
    "vldurh",
    "sturw",
    "ldurw",
    "ldursw",
    "vsturs",
    "vldurs",
    "sturx",
    "ldurx",
    "prfum",
    "vsturd",
    "vldurd",
    "strbpost",
    "ldrbpost",
    "ldrsbpostx",
    "ldrsbpostw",
    "vstrpostb",
    "vldrpostb",
    "vstrpostq",
    "vldrpostq",
    "strhpost",
    "ldrhpost",
    "ldrshpostx",
    "ldrshpostw",
    "vstrposth",
    "vldrposth",
    "strpostw",
    "ldrpostw",
    "ldrswpost",
    "vstrposts",
    "vldrposts",
    "strpostx",
    "ldrpostx",
    "vstrpostd",
    "vldrpostd",
    "sttrb",
    "ldtrb",
    "ldtrsbx",
    "ldtrsbw",
    "sttrh",
    "ldtrh",
    "ldtrshx",
    "ldtrshw",
    "sttrw",
    "ldtrw",
    "ldtrsw",
    "sttrx",
    "ldtrx",
    "strbpre",
    "ldrbpre",
    "ldrsbprex",
    "ldrsbprew",
    "vstrpreb",
    "vldrpreb",
    "vstrpreq",
    "vldrpreq",
    "strhpre",
    "ldrhpre",
    "ldrshprex",
    "ldrshprew",
    "vstrpreh",
    "vldrpreh",
    "strprew",
    "ldrprew",
    "ldrswpre",
    "vstrpres",
    "vldrpres",
    "strprex",
    "ldrprex",
    "vstrpred",
    "vldrpred",
    "strboff",
    "ldrboff",
    "ldrsboffx",
    "ldrsboffw",
    "vstroffb",
    "vldroffb",
    "vstroffq",
    "vldroffq",
    "strhoff",
    "ldrhoff",
    "ldrshoffx",
    "ldrshoffw",
    "vstroffh",
    "vldroffh",
    "stroffw",
    "ldroffw",
    "ldrswoff",
    "vstroffs",
    "vldroffs",
    "stroffx",
    "ldroffx",
    "vstroffd",
    "vldroffd",
    "prfmoff",
    "strbimm",
    "ldrbimm",
    "ldrsbimmx",
    "ldrsbimmw",
    "vstrimmb",
    "vldrimmb",
    "vstrimmq",
    "vldrimmq",
    "strhimm",
    "ldrhimm",
    "ldrshimmx",
    "ldrshimmw",
    "vstrimmh",
    "vldrimmh",
    "strimmw",
    "ldrimmw",
    "ldrswimm",
    "vstrimms",
    "vldrimms",
    "strimmx",
    "ldrimmx",
    "vstrimmd",
    "vldrimmd",
    "prfmimm",
    "ldaddx",
    "ldaddax",
    "ldaddlx",
    "ldaddalx",
    "ldaddw",
    "ldaddaw",
    "ldaddlw",
    "ldaddalw",
    "ldaddb",
    "ldaddab",
    "ldaddlb",
    "ldaddalb",
    "ldaddh",
    "ldaddah",
    "ldaddlh",
    "ldaddalh",
    "ldclrx",
    "ldclrax",
    "ldclrlx",
    "ldclralx",
    "ldclrw",
    "ldclraw",
    "ldclrlw",
    "ldclralw",
    "ldclrb",
    "ldclrab",
    "ldclrlb",
    "ldclralb",
    "ldclrh",
    "ldclrah",
    "ldclrlh",
    "ldclralh",
    "ldeorx",
    "ldeorax",
    "ldeorlx",
    "ldeoralx",
    "ldeorw",
    "ldeoraw",
    "ldeorlw",
    "ldeoralw",
    "ldeorb",
    "ldeorab",
    "ldeorlb",
    "ldeoralb",
    "ldeorh",
    "ldeorah",
    "ldeorlh",
    "ldeoralh",
    "ldsetx",
    "ldsetax",
    "ldsetlx",
    "ldsetalx",
    "ldsetw",
    "ldsetaw",
    "ldsetlw",
    "ldsetalw",
    "ldsetb",
    "ldsetab",
    "ldsetlb",
    "ldsetalb",
    "ldseth",
    "ldsetah",
    "ldsetlh",
    "ldsetalh",
    "ldsmaxx",
    "ldsmaxax",
    "ldsmaxlx",
    "ldsmaxalx",
    "ldsmaxw",
    "ldsmaxaw",
    "ldsmaxlw",
    "ldsmaxalw",
    "ldsmaxb",
    "ldsmaxab",
    "ldsmaxlb",
    "ldsmaxalb",
    "ldsmaxh",
    "ldsmaxah",
    "ldsmaxlh",
    "ldsmaxalh",
    "ldsminx",
    "ldsminax",
    "ldsminlx",
    "ldsminalx",
    "ldsminw",
    "ldsminaw",
    "ldsminlw",
    "ldsminalw",
    "ldsminb",
    "ldsminab",
    "ldsminlb",
    "ldsminalb",
    "ldsminh",
    "ldsminah",
    "ldsminlh",
    "ldsminalh",
    "ldumaxx",
    "ldumaxax",
    "ldumaxlx",
    "ldumaxalx",
    "ldumaxw",
    "ldumaxaw",
    "ldumaxlw",
    "ldumaxalw",
    "ldumaxb",
    "ldumaxab",
    "ldumaxlb",
    "ldumaxalb",
    "ldumaxh",
    "ldumaxah",
    "ldumaxlh",
    "ldumaxalh",
    "lduminx",
    "lduminax",
    "lduminlx",
    "lduminalx",
    "lduminw",
    "lduminaw",
    "lduminlw",
    "lduminalw",
    "lduminb",
    "lduminab",
    "lduminlb",
    "lduminalb",
    "lduminh",
    "lduminah",
    "lduminlh",
    "lduminalh",
    "staddx",
    "staddlx",
    "staddw",
    "staddlw",
    "staddb",
    "staddlb",
    "staddh",
    "staddlh",
    "stclrx",
    "stclrlx",
    "stclrw",
    "stclrlw",
    "stclrb",
    "stclrlb",
    "stclrh",
    "stclrlh",
    "steorx",
    "steorlx",
    "steorw",
    "steorlw",
    "steorb",
    "steorlb",
    "steorh",
    "steorlh",
    "stsetx",
    "stsetlx",
    "stsetw",
    "stsetlw",
    "stsetb",
    "stsetlb",
    "stseth",
    "stsetlh",
    "stsmaxx",
    "stsmaxlx",
    "stsmaxw",
    "stsmaxlw",
    "stsmaxb",
    "stsmaxlb",
    "stsmaxh",
    "stsmaxlh",
    "stsminx",
    "stsminlx",
    "stsminw",
    "stsminlw",
    "stsminb",
    "stsminlb",
    "stsminh",
    "stsminlh",
    "stumaxx",
    "stumaxlx",
    "stumaxw",
    "stumaxlw",
    "stumaxb",
    "stumaxlb",
    "stumaxh",
    "stumaxlh",
    "stuminx",
    "stuminlx",
    "stuminw",
    "stuminlw",
    "stuminb",
    "stuminlb",
    "stuminh",
    "stuminlh",
    "swpx",
    "swpax",
    "swplx",
    "swpalx",
    "swpw",
    "swpaw",
    "swplw",
    "swpalw",
    "swpb",
    "swpab",
    "swplb",
    "swpalb",
    "swph",
    "swpah",
    "swplh",
    "swpalh",
    "casx",
    "casax",
    "caslx",
    "casalx",
    "casw",
    "casaw",
    "caslw",
    "casalw",
    "casb",
    "casab",
    "caslb",
    "casalb",
    "cash",
    "casah",
    "caslh",
    "casalh",
    "adr",
    "adrp",
    "addimmw",
    "addsimmw",
    "subimmw",
    "subsimmw",
    "addimmx",
    "addsimmx",
    "subimmx",
    "subsimmx",
    "andimmw",
    "orrimmw",
    "eorimmw",
    "andsimmw",
    "andimmx",
    "orrimmx",
    "eorimmx",
    "andsimmx",
    "movnw",
    "movzw",
    "movkw",
    "movnx",
    "movzx",
    "movkx",
    "sbfmw",
    "bfmw",
    "ubfmw",
    "sbfmx",
    "bfmx",
    "ubfmx",
    "extrw",
    "extrx",
    "andw",
    "bicw",
    "orrw",
    "ornw",
    "eorw",
    "eonw",
    "andsw",
    "bicsw",
    "andx",
    "bicx",
    "orrx",
    "ornx",
    "eorx",
    "eonx",
    "andsx",
    "bicsx",
    "addw",
    "addsw",
    "subw",
    "subsw",
    "addx",
    "addsx",
    "subx",
    "subsx",
    "addextw",
    "addsextw",
    "subextw",
    "subsextw",
    "addextx",
    "addsextx",
    "subextx",
    "subsextx",
    "adcw",
    "adcsw",
    "sbcw",
    "sbcsw",
    "adcx",
    "adcsx",
    "sbcx",
    "sbcsx",
    "ccmnw",
    "ccmnx",
    "ccmpw",
    "ccmpx",
    "ccmnimmw",
    "ccmnimmx",
    "ccmpimmw",
    "ccmpimmx",
    "cselw",
    "csincw",
    "csinvw",
    "csnegw",
    "cselx",
    "csincx",
    "csinvx",
    "csnegx",
    "maddw",
    "maddx",
    "smaddl",
    "umaddl",
    "msubw",
    "msubx",
    "smsubl",
    "umsubl",
    "smulh",
    "umulh",
    "fmaddd",
    "fmadds",
    "crc32x",
    "crc32cx",
    "crc32b",
    "crc32cb",
    "crc32h",
    "crc32ch",
    "crc32w",
    "crc32cw",
    "udivw",
    "udivx",
    "sdivw",
    "sdivx",
    "lslvw",
    "lslvx",
    "lsrvw",
    "lsrvx",
    "asrvw",
    "asrvx",
    "rorvw",
    "rorvx",
    "rbitw",
    "rbitx",
    "clzw",
    "clzx",
    "clsw",
    "clsx",
    "revw",
    "revx",
    "rev16w",
    "rev16x",
    "rev32",
    "fmov_stow",
    "fmov_wtos",
    "fmov_dtox",
    "fmov_xtod",
    "fcvt_stod",
    "fcvt_dtos",
    "fcvtzs_stow",
    "fcvtzs_dtow",
    "fcvtzs_stox",
    "fcvtzs_dtox",
    "scvtf_wtos",
    "scvtf_wtod",
    "scvtf_xtos",
    "scvtf_xtod",
    "vfcvtzs_stow4s",
    "vfcvtzs_dtox2d",
    "vscvtf_wtos4s",
    "vscvtf_xtod2d",
    "fmovimms",
    "fmovimmd",
    "fcmps",
    "fcmps_zero",
    "fcmpd",
    "fcmpd_zero",
    "fcsels",
    "fcseld",
    "fmovs",
    "fmovd",
    "fabss",
    "fabsd",
    "fnegs",
    "fnegd",
    "fsqrts",
    "fsqrtd",
    "fadds",
    "faddd",
    "fsubs",
    "fsubd",
    "fmuls",
    "fmuld",
    "fdivs",
    "fdivd",
    "fmaxs",
    "fmaxd",
    "fmins",
    "fmind",
    "vmovi16b",
    "vmovi8h",
    "vmovi2s",
    "vmovi4s",
    "vmovi4s_one",
    "movid",
    "vmovi2d",
    "vfmov4s",
    "vfmov2d",
    "vmvni8h",
    "vmvni4s",
    "vmvni4s_one",
    "vbicimm8h",
    "vbicimm4s",
    "vorrimm8h",
    "vorrimm4s",
    "vshl16b",
    "vshl8h",
    "vshl4s",
    "vshl2d",
    "vsshll_8h",
    "vsshll_4s",
    "vsshll_2d",
    "vsshll2_8h",
    "vsshll2_4s",
    "vsshll2_2d",
    "vushll_8h",
    "vushll_4s",
    "vushll_2d",
    "vushll2_8h",
    "vushll2_4s",
    "vushll2_2d",
    "vsli16b",
    "vsli8h",
    "vsli4s",
    "vsli2d",
    "vsshr16b",
    "vsshr8h",
    "vsshr4s",
    "vsshr2d",
    "vshrn_8b",
    "vshrn_4h",
    "vshrn_2s",
    "vshrn2_16b",
    "vshrn2_8h",
    "vshrn2_4s",
    "vushr16b",
    "vushr8h",
    "vushr4s",
    "vushr2d",
    "vsri16b",
    "vsri8h",
    "vsri4s",
    "vsri2d",
    "vshll_8h",
    "vshll_4s",
    "vshll_2d",
    "vshll2_8h",
    "vshll2_4s",
    "vshll2_2d",
    "vcmeq16b",
    "vcmeq8b",
    "vcmeq8h",
    "vcmeq4h",
    "vcmeq4s",
    "vcmeq2s",
    "vcmeq2d",
    "vcmeq16b_zero",
    "vcmeq8b_zero",
    "vcmeq8h_zero",
    "vcmeq4h_zero",
    "vcmeq4s_zero",
    "vcmeq2s_zero",
    "vcmeq2d_zero",
    "vcmhs16b",
    "vcmhs8h",
    "vcmhs4s",
    "vcmhs2d",
    "vcmge16b",
    "vcmge8h",
    "vcmge4s",
    "vcmge2d",
    "vcmge16b_zero",
    "vcmge8b_zero",
    "vcmge8h_zero",
    "vcmge4h_zero",
    "vcmge4s_zero",
    "vcmge2s_zero",
    "vcmge2d_zero",
    "vcmhi16b",
    "vcmhi8h",
    "vcmhi4s",
    "vcmhi2d",
    "vcmgt16b",
    "vcmgt8h",
    "vcmgt4s",
    "vcmgt2d",
    "vcmgt16b_zero",
    "vcmgt8b_zero",
    "vcmgt8h_zero",
    "vcmgt4h_zero",
    "vcmgt4s_zero",
    "vcmgt2s_zero",
    "vcmgt2d_zero",
    "vcmle16b_zero",
    "vcmle8b_zero",
    "vcmle8h_zero",
    "vcmle4h_zero",
    "vcmle4s_zero",
    "vcmle2s_zero",
    "vcmle2d_zero",
    "vcmlt16b_zero",
    "vcmlt8b_zero",
    "vcmlt8h_zero",
    "vcmlt4h_zero",
    "vcmlt4s_zero",
    "vcmlt2s_zero",
    "vcmlt2d_zero",
    "vcmtst16b",
    "vcmtst8h",
    "vcmtst4s",
    "vcmtst2d",
    "vfcmeq4s",
    "vfcmeq2d",
    "vfcmeq4s_zero",
    "vfcmeq2d_zero",
    "vfcmge4s",
    "vfcmge2d",
    "vfcmge4s_zero",
    "vfcmge2d_zero",
    "vfcmgt4s",
    "vfcmgt2d",
    "vfcmgt4s_zero",
    "vfcmgt2d_zero",
    "vfcmle4s_zero",
    "vfcmle2d_zero",
    "vfcmlt4s_zero",
    "vfcmlt2d_zero",
    "vfacge4s",
    "vfacge2d",
    "vfacgt4s",
    "vfacgt2d",
    "vadd16b",
    "vadd8h",
    "vadd4s",
    "vadd2d",
    "vfadd4s",
    "vfadd2d",
    "vsub16b",
    "vsub8h",
    "vsub4s",
    "vsub2d",
    "vfsub4s",
    "vfsub2d",
    "vmul16b",
    "vmul8h",
    "vmul4s",
    "vfmul4s",
    "vfmul2d",
    "vfdiv4s",
    "vfdiv2d",
    "vand16b",
    "vbic16b",
    "vorr16b",
    "veor16b",
    "vmla16b",
    "vmla8h",
    "vmla4s",
    "vmls16b",
    "vmls8h",
    "vmls4s",
    "vfmla4s",
    "vfmla2d",
    "vsmin16b",
    "vsmin8h",
    "vsmin4s",
    "vfmin4s",
    "vfmin2d",
    "vsmax16b",
    "vsmax8h",
    "vsmax4s",
    "vfmax4s",
    "vfmax2d",
    "vsshl16b",
    "vsshl8h",
    "vsshl4s",
    "vsshl2d",
    "vushl16b",
    "vushl8h",
    "vushl4s",
    "vushl2d",
    "vbif16b",
    "vbit16b",
    "vbsl16b",
    "vzip1_16b",
    "vzip1_8h",
    "vzip1_4s",
    "vzip1_2d",
    "vzip2_16b",
    "vzip2_8h",
    "vzip2_4s",
    "vzip2_2d",
    "vuzp1_16b",
    "vuzp1_8h",
    "vuzp1_4s",
    "vuzp1_2d",
    "vuzp2_16b",
    "vuzp2_8h",
    "vuzp2_4s",
    "vuzp2_2d",
    "vtrn1_8b",
    "vtrn1_16b",
    "vtrn2_8b",
    "vtrn2_16b",
    "vext16b",
    "vneg16b",
    "vneg8h",
    "vneg4s",
    "vneg2d",
    "vfneg4s",
    "vfneg2d",
    "vnot16b",
    "vfsqrt4s",
    "vfsqrt2d",
    "vabs16b",
    "vabs8h",
    "vabs4s",
    "vabs2d",
    "vfabs4s",
    "vfabs2d",
    "vrbit16b",
    "vrev16_16b",
    "vrev32_16b",
    "vrev32_8h",
    "vrev64_16b",
    "vrev64_8h",
    "vrev64_4s",
    "vxtn_8b",
    "vxtn_4h",
    "vxtn_2s",
    "vxtn2_16b",
    "vxtn2_8h",
    "vxtn2_4s",
    "vcls16b",
    "vcls8h",
    "vcls4s",
    "vclz16b",
    "vclz8h",
    "vclz4s",
    "vcnt8b",
    "vcnt16b",
    "vsaddlp16b",
    "vsaddlp8h",
    "vsaddlp4s",
    "vuaddlp16b",
    "vuaddlp8h",
    "vuaddlp4s",
    "vdup16b",
    "vdup8h",
    "vdup4s",
    "vdup2d",
    "vdupe16b",
    "vdupe8h",
    "vdupe4s",
    "vdupe2d",
    "smovwb",
    "smovwh",
    "smovxb",
    "smovxh",
    "smovxs",
    "umovwb",
    "umovwh",
    "umovws",
    "umovxd",
    "vinsb",
    "vinsh",
    "vinss",
    "vinsd",
    "vinseb",
    "vinseh",
    "vinses",
    "vinsed",
    "fmulelem_4s",
    "fmulelem_2d",
    "vfmulelem_4s",
    "vfmulelem_2d",
    "vumlal_8h",
    "vumlal_4s",
    "vumlal_2d",
    "vumlal2_8h",
    "vumlal2_4s",
    "vumlal2_2d",
    "vumlsl_8h",
    "vumlsl_4s",
    "vumlsl_2d",
    "vumlsl2_8h",
    "vumlsl2_4s",
    "vumlsl2_2d",
    "vumull_8h",
    "vumull_4s",
    "vumull_2d",
    "vumull2_8h",
    "vumull2_4s",
    "vumull2_2d",
    "vaddv8b",
    "vaddv16b",
    "vaddv8h",
    "vaddv4s",
    "vfmaxnmv4s",
    "vfmaxv4s",
    "vfminnmv4s",
    "vfminv4s",
    "vsaddlv8h",
    "vsaddlv4s",
    "vsaddlv2d",
    "vsmaxv16b",
    "vsmaxv8h",
    "vsmaxv4s",
    "vsminv16b",
    "vsminv8h",
    "vsminv4s",
    "vuaddlv8h",
    "vuaddlv4s",
    "vuaddlv2d",
    "vumaxv16b",
    "vumaxv8h",
    "vumaxv4s",
    "vuminv16b",
    "vuminv8h",
    "vuminv4s",
    "vaddp16b",
    "vaddp8h",
    "vaddp4s",
    "vaddp2d",
    "addp2d",
    "vfaddp4s",
    "vfaddp2d",
    "faddp2s",
    "faddp2d",
    "vfmaxnmp4s",
    "vfmaxnmp2d",
    "fmaxnmp2s",
    "fmaxnmp2d",
    "vfmaxp4s",
    "vfmaxp2d",
    "fmaxp2s",
    "fmaxp2d",
    "vfminnmp4s",
    "vfminnmp2d",
    "fminnmp2s",
    "fminnmp2d",
    "vfminp4s",
    "vfminp2d",
    "fminp2s",
    "fminp2d",
    "vsmaxp16b",
    "vsmaxp8h",
    "vsmaxp4s",
    "vsminp16b",
    "vsminp8h",
    "vsminp4s",
    "vumaxp16b",
    "vumaxp8h",
    "vumaxp4s",
    "vuminp16b",
    "vuminp8h",
    "vuminp4s",
    "nop",
};

static uint32_t getSFbit(uint32_t enc) { return (enc >> 31); /* bit 31 */ }

static bool useGPR(uint32_t enc) { return ((enc >> 25) & 7) == 5; /* bits 25-27 */ }

static uint32_t getFType(uint32_t enc) { return (enc >> 22) & 3; /* bits 22-23 */ }

static uint32_t getVRbit(uint32_t enc) { return (enc >> 26) & 1; /* bit 26 */ }

static uint32_t getLoadStoreSize(uint32_t enc) { return (enc >> 30); /* bits 30-31 */ }

static TR_RegisterSizes getRegisterSizeFromFType(uint32_t ftype)
{
    switch (ftype) {
        case 0:
            return TR_FloatReg;
        case 1:
            return TR_DoubleReg;
        default:
            return TR_VectorReg128;
    }
}

const char *TR_Debug::getOpCodeName(OP *opCode) { return opCodeToNameMap[opCode->getMnemonic()]; }

void TR_Debug::printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *mr)
{
    TR::Symbol *symbol = mr->getSymbolReference()->getSymbol();
    if (symbol == NULL && mr->getSymbolReference()->getOffset() == 0)
        return;

    log->prints("\t\t# SymRef");
    print(log, mr->getSymbolReference());
}

void TR_Debug::print(OMR::Logger *log, TR::Instruction *instr)
{
    switch (instr->getKind()) {
        case OMR::Instruction::IsImm:
            print(log, (TR::ARM64ImmInstruction *)instr);
            break;
        case OMR::Instruction::IsSynchronization:
            print(log, (TR::ARM64SynchronizationInstruction *)instr);
            break;
        case OMR::Instruction::IsException:
            print(log, (TR::ARM64ImmInstruction *)instr); // printing handled by superclass
            break;
        case OMR::Instruction::IsRelocatableImm:
            print(log, (TR::ARM64RelocatableImmInstruction *)instr);
            break;
        case OMR::Instruction::IsImmSym:
            print(log, (TR::ARM64ImmSymInstruction *)instr);
            break;
        case OMR::Instruction::IsLabel:
            print(log, (TR::ARM64LabelInstruction *)instr);
            break;
        case OMR::Instruction::IsBranch:
            print(log, (TR::ARM64BranchInstruction *)instr);
            break;
        case OMR::Instruction::IsConditionalBranch:
            print(log, (TR::ARM64ConditionalBranchInstruction *)instr);
            break;
        case OMR::Instruction::IsCompareBranch:
            print(log, (TR::ARM64CompareBranchInstruction *)instr);
            break;
        case OMR::Instruction::IsTestBitBranch:
            print(log, (TR::ARM64TestBitBranchInstruction *)instr);
            break;
#ifdef J9_PROJECT_SPECIFIC
        case OMR::Instruction::IsVirtualGuardNOP:
            print(log, (TR::ARM64VirtualGuardNOPInstruction *)instr);
            break;
#endif
        case OMR::Instruction::IsRegBranch:
            print(log, (TR::ARM64RegBranchInstruction *)instr);
            break;
        case OMR::Instruction::IsAdmin:
            print(log, (TR::ARM64AdminInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1:
            print(log, (TR::ARM64Trg1Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Cond:
            print(log, (TR::ARM64Trg1CondInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Imm:
            print(log, (TR::ARM64Trg1ImmInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1ImmShifted:
            print(log, (TR::ARM64Trg1ImmShiftedInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1ImmSym:
            print(log, (TR::ARM64Trg1ImmSymInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1ZeroSrc1:
            print(log, (TR::ARM64Trg1ZeroSrc1Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1ZeroImm:
            print(log, (TR::ARM64Trg1ZeroImmInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src1:
            print(log, (TR::ARM64Trg1Src1Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src1Imm:
            print(log, (TR::ARM64Trg1Src1ImmInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2:
            print(log, (TR::ARM64Trg1Src2Instruction *)instr);
            break;
        case OMR::Instruction::IsCondTrg1Src2:
            print(log, (TR::ARM64CondTrg1Src2Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2Imm:
            print(log, (TR::ARM64Trg1Src2ImmInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2Shifted:
            print(log, (TR::ARM64Trg1Src2ShiftedInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2Extended:
            print(log, (TR::ARM64Trg1Src2ExtendedInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2IndexedElement:
            print(log, (TR::ARM64Trg1Src2IndexedElementInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2Zero:
            print(log, (TR::ARM64Trg1Src2ZeroInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src3:
            print(log, (TR::ARM64Trg1Src3Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Mem:
            print(log, (TR::ARM64Trg1MemInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg2Mem:
            print(log, (TR::ARM64Trg2MemInstruction *)instr);
            break;
        case OMR::Instruction::IsMem:
            print(log, (TR::ARM64MemInstruction *)instr);
            break;
        case OMR::Instruction::IsMemImm:
            print(log, (TR::ARM64MemImmInstruction *)instr);
            break;
        case OMR::Instruction::IsMemSrc1:
            print(log, (TR::ARM64MemSrc1Instruction *)instr);
            break;
        case OMR::Instruction::IsMemSrc2:
            print(log, (TR::ARM64MemSrc2Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1MemSrc1:
            print(log, (TR::ARM64Trg1MemSrc1Instruction *)instr);
            break;
        case OMR::Instruction::IsSrc1:
            print(log, (TR::ARM64Src1Instruction *)instr);
            break;
        case OMR::Instruction::IsZeroSrc1Imm:
            print(log, (TR::ARM64ZeroSrc1ImmInstruction *)instr);
            break;
        case OMR::Instruction::IsSrc2:
            print(log, (TR::ARM64Src2Instruction *)instr);
            break;
        case OMR::Instruction::IsZeroSrc2:
            print(log, (TR::ARM64ZeroSrc2Instruction *)instr);
            break;
        case OMR::Instruction::IsSrc1ImmCond:
            print(log, (TR::ARM64Src1ImmCondInstruction *)instr);
            break;
        case OMR::Instruction::IsSrc2Cond:
            print(log, (TR::ARM64Src2CondInstruction *)instr);
            break;
        default:
            TR_ASSERT(false, "unexpected instruction kind");
            // fall through
        case OMR::Instruction::IsNotExtended: {
            printPrefix(log, instr);
            log->prints(getOpCodeName(&instr->getOpCode()));
            log->flush();
        }
    }
}

void TR_Debug::printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr)
{
    while (tabStops-- > 0)
        log->printc('\t');

    dumpInstructionComments(log, instr);
}

void TR_Debug::printPrefix(OMR::Logger *log, TR::Instruction *instr)
{
    printPrefix(log, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
    TR::Node *node = instr->getNode();
    log->printf("%d \t", node ? node->getByteCodeIndex() : 0);
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64ImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t0x%08x", getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64RelocatableImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t" POINTER_PRINTF_FORMAT "\t; %s", getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate(),
        TR::ExternalRelocation::getName(instr->getReloKind()));
    TR::SymbolReference *sr = instr->getSymbolReference();
    if (sr)
        log->printf(" \"%s\"", getName(sr));
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64ImmSymInstruction *instr)
{
    printPrefix(log, instr);

    TR::Symbol *target = instr->getSymbolReference()->getSymbol();
    const char *name = target ? getName(instr->getSymbolReference()) : 0;
    if (name)
        log->printf("%s \t" POINTER_PRINTF_FORMAT "\t\t; Direct Call \"%s\"", getOpCodeName(&instr->getOpCode()),
            instr->getAddrImmediate(), name);
    else
        log->printf("%s \t" POINTER_PRINTF_FORMAT, getOpCodeName(&instr->getOpCode()), instr->getAddrImmediate());

    printInstructionComment(log, 1, instr);

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64LabelInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    print(log, label);
    log->printc(':');
    if (label->isStartInternalControlFlow())
        log->prints("\t; (Start of internal control flow)");
    else if (label->isEndInternalControlFlow())
        log->prints("\t; (End of internal control flow)");
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64BranchInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    log->prints("b \t");
    print(log, label);
    if (snippet) {
        log->printf(" (%s)", getName(snippet));
    }
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::print(OMR::Logger *log, TR::ARM64VirtualGuardNOPInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s Site:" POINTER_PRINTF_FORMAT ", ", getOpCodeName(&instr->getOpCode()), instr->getSite());
    print(log, instr->getLabelSymbol());
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}
#endif

void TR_Debug::print(OMR::Logger *log, TR::ARM64ConditionalBranchInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    log->printf("b.%s \t", ARM64ConditionNames[instr->getConditionCode()]);
    print(log, label);
    if (snippet) {
        log->printf(" (%s)", getName(snippet));
    }
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64CompareBranchInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, label);
    if (snippet) {
        log->printf(" (%s)", getName(snippet));
    }
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64TestBitBranchInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getSource1Register(), TR_DoubleWordReg);
    log->prints(", ");
    log->printf("#%d, ", instr->getBitPos());
    print(log, label);
    if (snippet) {
        log->printf(" (%s)", getName(snippet));
    }
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64RegBranchInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getTargetRegister(), TR_DoubleWordReg);
    printInstructionComment(log, 1, instr);
    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64AdminInstruction *instr)
{
    if (instr->getOpCodeValue() == OP::assocreg) {
        printAssocRegDirective(log, instr);
        return;
    }

    printPrefix(log, instr);
    log->printf("%s ", getOpCodeName(&instr->getOpCode()));

    TR::Node *node = instr->getNode();
    if (node) {
        if (node->getOpCodeValue() == TR::BBStart) {
            log->printf(" (BBStart (block_%d))", node->getBlock()->getNumber());
        } else if (node->getOpCodeValue() == TR::BBEnd) {
            log->printf(" (BBEnd (block_%d))", node->getBlock()->getNumber());
        }
    }

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getTargetRegister(), TR_WordReg);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1CondInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;

    if (op == OP::csincx || (op == OP::csincw)) {
        // cset alias
        log->printf("cset%c \t", (sf == 1) ? 'x' : 'w');
        print(log, instr->getTargetRegister(), regSize);
        log->printf(", %s", ARM64ConditionNames[cc_invert(instr->getConditionCode())]);
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), regSize);
        log->printf(", xzr, xzr, %s", ARM64ConditionNames[instr->getConditionCode()]);
    }
    log->flush();
}

/*
 * Convert 8-bit modified immediate constant used in fmovimm instructions
 * to double value
 */
static double getDoubleFromImm8(uint32_t imm8)
{
    uint32_t isNegative = imm8 & 0x80;
    uint32_t lower4 = imm8 & 0xf;
    uint32_t upper3 = (imm8 >> 4) & 0x7;
    uint32_t shift = (upper3 <= 3) ? (upper3 + 4) : (upper3 - 4);

    double x = (lower4 + 16) / 16.0;
    double y = (1 << shift) / 8.0;
    double r = x * y;

    return isNegative ? -r : r;
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1ImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    if ((op == OP::adr) || (op == OP::adrp)) {
        int64_t offset = static_cast<int64_t>(static_cast<int32_t>(instr->getSourceImmediate()));
        if (instr->getOpCodeValue() == OP::adrp) {
            offset *= 4096;
        }
        print(log, instr->getTargetRegister(), TR_DoubleWordReg);
        log->printf(", " POINTER_PRINTF_FORMAT, (instr->getBinaryEncoding() + offset));
    } else if (op == OP::fmovimms || op == OP::fmovimmd || op == OP::vfmov4s || op == OP::vfmov2d) {
        uint32_t imm = instr->getSourceImmediate() & 0xFF;
        TR_RegisterSizes regSize = TR_VectorReg128; // for vfmov4s and vfmov2d
        if (op == OP::fmovimms) {
            regSize = TR_FloatReg;
        } else if (op == OP::fmovimmd) {
            regSize = TR_DoubleReg;
        }
        print(log, instr->getTargetRegister(), regSize);
        log->printf(", 0x%02x (%lf)", imm, getDoubleFromImm8(imm));
    } else if (op == OP::movzx || op == OP::movzw || op == OP::movnx || op == OP::movnw || op == OP::movkx
        || op == OP::movkw) {
        uint32_t imm = instr->getSourceImmediate() & 0xFFFF;
        uint32_t shift = (instr->getSourceImmediate() & 0x30000) >> 12;
        uint32_t enc = OP::getOpCodeBinaryEncoding(op);
        uint32_t sf = getSFbit(enc);
        TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
        print(log, instr->getTargetRegister(), regSize);
        log->printf(", 0x%04x", imm);
        if (shift != 0) {
            log->printf(", LSL #%d", shift);
        }
    } else if (op == OP::vmovi2d) {
        uint8_t imm8 = instr->getSourceImmediate() & 0xFF;
        uint64_t imm = 0;
        for (int i = 0; i < 8; i++) {
            if ((imm8 & (1 << i)) != 0) {
                imm |= static_cast<uint64_t>(0xff) << (i * 8);
            }
        }
        print(log, instr->getTargetRegister(), TR_DoubleReg);
        log->printf(", 0x%08llx", imm);
    } else {
        uint32_t imm = instr->getSourceImmediate() & 0xFF;
        print(log, instr->getTargetRegister(), TR_DoubleWordReg);
        log->printf(", 0x%02x", imm);
    }
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1ImmShiftedInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getTargetRegister(), TR_WordReg);

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t imm = instr->getSourceImmediate() & 0xFF;
    uint32_t shift = instr->getShiftAmount();
    log->printf(", 0x%02x", imm);
    if (shift != 0) {
        if ((op == OP::vmovi4s_one) || (op == OP::vmvni4s_one)) {
            log->printf(", MSL #%d", shift);
        } else {
            log->printf(", LSL #%d", shift);
        }
    }
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1ImmSymInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getTargetRegister(), TR_DoubleWordReg);
    log->prints(", ");

    auto sym = instr->getSymbol();
    if ((sym != NULL) && sym->isLabel()) {
        auto label = sym->getLabelSymbol();
        print(log, label);
        TR::Snippet *snippet = label->getSnippet();
        if (snippet) {
            log->printf("(%s)", getName(snippet));
        }
    } else {
        int64_t offset = static_cast<int64_t>(static_cast<int32_t>(instr->getSourceImmediate()));
        if ((instr->getOpCodeValue() != OP::adr) && (instr->getOpCodeValue() != OP::adrp)) {
            // ldr literal variants
            offset *= 4;
        }
        if (instr->getOpCodeValue() == OP::adrp) {
            offset *= 4096;
        }
        log->printf(POINTER_PRINTF_FORMAT, (instr->getBinaryEncoding() + offset));
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src1Instruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    if (op >= OP::fcvt_stod && op <= OP::vscvtf_xtod2d) {
        TR_RegisterSizes regSize1 = TR_UnknownSizeReg; // trg register size
        TR_RegisterSizes regSize2 = TR_UnknownSizeReg; // src register size
        uint32_t sf = getSFbit(enc);
        uint32_t ftype = getFType(enc);
        if (op == OP::fcvt_stod) {
            regSize1 = TR_DoubleReg;
            regSize2 = TR_FloatReg;
        } else if (op == OP::fcvt_dtos) {
            regSize1 = TR_FloatReg;
            regSize2 = TR_DoubleReg;
        } else if (op >= OP::fcvtzs_stow && op <= OP::fcvtzs_dtox) {
            regSize1 = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
            regSize2 = getRegisterSizeFromFType(ftype);
        } else if (op >= OP::scvtf_wtos && op <= OP::scvtf_xtod) {
            regSize1 = getRegisterSizeFromFType(ftype);
            regSize2 = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
        } else if (op >= OP::vfcvtzs_stow4s && op <= OP::vscvtf_xtod2d) {
            regSize1 = TR_VectorReg128;
            regSize2 = TR_VectorReg128;
        }

        print(log, instr->getTargetRegister(), regSize1);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize2);
    } else {
        TR_RegisterSizes regSize = TR_UnknownSizeReg;
        if (useGPR(enc)) {
            uint32_t sf = getSFbit(enc);
            regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
        } else {
            regSize = getRegisterSizeFromFType(getFType(enc));
        }

        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);

        if (op >= OP::vshll_8h && op <= OP::vshll2_2d) {
            uint32_t size = (enc >> 22) & 0x3; /* bits 22-23 */
            uint32_t shiftAmount = 8 << size;
            log->printf(", %d", shiftAmount);
        }
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1ZeroSrc1Instruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    char suffix = (sf == 1) ? 'x' : 'w';
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;

    if (op == OP::orrx || op == OP::orrw) {
        // mov alias
        log->printf("mov%c \t", suffix);
    } else if (op == OP::ornx || op == OP::ornw) {
        // mvn alias
        log->printf("mvn%c \t", suffix);
    } else if (op == OP::subx || op == OP::subw) {
        // neg alias
        log->printf("neg%c \t", suffix);
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    }

    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");
    print(log, instr->getSource1Register(), regSize);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src1ImmInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    bool done = false;
    if (op == OP::subsimmx || op == OP::subsimmw || op == OP::addsimmx || op == OP::addsimmw) {
        done = true;
        TR::Register *r = instr->getTargetRegister();
        if (r && r->getRealRegister() && toRealRegister(r)->getRegisterNumber() == TR::RealRegister::xzr) {
            // cmp/cmn alias
            char *mnemonic = NULL;
            switch (op) {
                case OP::subsimmx:
                    mnemonic = "cmpimmx";
                    break;
                case OP::subsimmw:
                    mnemonic = "cmpimmw";
                    break;
                case OP::addsimmx:
                    mnemonic = "cmnimmx";
                    break;
                case OP::addsimmw:
                    mnemonic = "cmnimmw";
                    break;
                default:
                    TR_ASSERT_FATAL(0, "unexpected operation");
                    break;
            }
            log->printf("%s \t", mnemonic);
            print(log, instr->getSource1Register(), regSize);
            log->printf(", %d", instr->getSourceImmediate());
        } else {
            log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
            print(log, instr->getTargetRegister(), regSize);
            log->prints(", ");
            print(log, instr->getSource1Register(), regSize);
            log->printf(", %d", instr->getSourceImmediate());
        }
        if (instr->getNbit()) {
            log->printf(", LSL #%d", 12);
        }
    } else if ((op == OP::subimmx || op == OP::subimmw || op == OP::addimmx || op == OP::addimmw)) {
        done = true;
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->printf(", %d", instr->getSourceImmediate());
        if (instr->getNbit()) {
            log->printf(", LSL #%d", 12);
        }
    } else if (op == OP::sbfmx || op == OP::sbfmw) {
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        if (op == OP::sbfmx) {
            if (imms == 63) {
                // asr alias
                done = true;
                log->prints("asrx \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", immr);
            } else if ((immr == 0) && ((imms == 7) || (imms == 15) || (imms == 31))) {
                // sxtb, sxth or sxtw (signed extend byte|half word|word) alias
                done = true;
                log->printf("sxt%cx \t", (imms == 7) ? 'b' : ((imms == 15) ? 'h' : 'w'));
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
            }
        } else if ((op == OP::sbfmw) && ((immr & (1 << 6)) == 0) && ((imms & (1 << 6)) == 0)) {
            if (imms == 31) {
                // asr alias
                done = true;
                log->prints("asrw \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", immr);
            } else if ((immr == 0) && ((imms == 7) || (imms == 15))) {
                // sxtb or sxth (signed extend byte|half word) alias
                done = true;
                log->printf("sxt%cw \t", (imms == 7) ? 'b' : 'h');
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
            }
        }
        if (!done) {
            done = true;
            log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
            print(log, instr->getTargetRegister(), regSize);
            log->prints(", ");
            print(log, instr->getSource1Register(), regSize);
            log->printf(", %d, %d", immr, imms);
        }
    } else if (op == OP::ubfmx || op == OP::ubfmw) {
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        if (op == OP::ubfmx) {
            if (imms == 63) {
                // lsr alias
                done = true;
                log->prints("lsrx \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", immr);
            } else if (imms + 1 == immr) {
                // lsl alias
                done = true;
                log->prints("lslx \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", 63 - imms);
            } else if (imms < immr) {
                // ubfiz alias
                done = true;
                log->prints("ubfizx \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", 64 - immr, imms + 1);
            } else {
                // ubfx alias
                done = true;
                log->prints("ubfxx \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", immr, imms + 1 - immr);
            }
        } else if ((op == OP::ubfmw) && ((immr & (1 << 6)) == 0) && ((imms & (1 << 6)) == 0)) {
            if (imms == 31) {
                // lsr alias
                done = true;
                log->prints("lsrw \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", immr);
            } else if (imms + 1 == immr) {
                // lsl alias
                done = true;
                log->prints("lslw \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d", 31 - imms);
            } else if ((immr == 0) && ((imms == 7) || (imms == 15))) {
                // uxtb or uxth (unsigned extend byte|half word) alias
                done = true;
                log->printf("uxt%cx \t", (imms == 7) ? 'b' : 'h');
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
            } else if (imms < immr) {
                // ubfiz alias
                done = true;
                log->prints("ubfizw \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", 32 - immr, imms + 1);
            } else {
                // ubfx alias
                done = true;
                log->prints("ubfxw \t");
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", immr, imms + 1 - immr);
            }
        }
        if (!done) {
            done = true;
            log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
            print(log, instr->getTargetRegister(), regSize);
            log->prints(", ");
            print(log, instr->getSource1Register(), regSize);
            log->printf(", %d, %d", immr, imms);
        }
    } else if (op == OP::bfmx || op == OP::bfmw) {
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        if ((op == OP::bfmx) || (((immr & (1 << 6)) == 0) && ((imms & (1 << 6)) == 0))) {
            char suffix = (sf == 1) ? 'x' : 'w';
            if (imms < immr) {
                // bfi alias
                done = true;
                log->printf("bfi%c \t", suffix);
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", 64 - immr, imms + 1);
            } else {
                // bfxil alias
                done = true;
                log->printf("bfxil%c \t", suffix);
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", %d, %d", immr, imms + 1 - immr);
            }
        }
        if (!done) {
            done = true;
            log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
            print(log, instr->getTargetRegister(), regSize);
            log->prints(", ");
            print(log, instr->getSource1Register(), regSize);
            log->printf(", %d, %d", immr, imms);
        }
    } else if (op == OP::andimmx || op == OP::andimmw || op == OP::andsimmx || op == OP::andsimmw || op == OP::orrimmx
        || op == OP::orrimmw || op == OP::eorimmx || op == OP::eorimmw) {
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        auto n = instr->getNbit();
        if (op == OP::andimmx || op == OP::andsimmx || op == OP::orrimmx || op == OP::eorimmx) {
            uint64_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", 0x%llx", immediate);
            }
        } else {
            uint32_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
                print(log, instr->getTargetRegister(), regSize);
                log->prints(", ");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", 0x%lx", immediate);
            }
        }
    } else if ((op >= OP::vshl16b) && (op <= OP::vsri2d)) {
        done = true;
        bool isShiftLeft = (op <= OP::vsli2d);
        uint32_t immh = (enc >> 19) & 0xf; /* bits 19-22 */
        uint32_t elementSize = 8 << (31 - leadingZeroes(immh));
        uint32_t imm = instr->getSourceImmediate();
        uint32_t shiftAmount = isShiftLeft ? (imm - elementSize) : (elementSize * 2 - imm);
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), TR_VectorReg128);
        log->prints(", ");
        print(log, instr->getSource1Register(), TR_VectorReg128);
        log->printf(", %d", shiftAmount);
    } else if ((op >= OP::vdupe16b) && (op <= OP::umovxd)) {
        done = true;
        const uint32_t elementSizeShift = (op <= OP::vdupe2d)
            ? (op - OP::vdupe16b)
            : ((op <= OP::smovwh) ? (op - OP::smovwb) : ((op <= OP::smovxh) ? (op - OP::smovxb) : (op - OP::umovwb)));

        const uint32_t imm5 = instr->getSourceImmediate() & 0x1f;
        const uint32_t index = imm5 >> (elementSizeShift + 1);
        if (op <= OP::vdupe2d) {
            regSize = TR_VectorReg128;
        } else {
            // SMOV or UMOV
            regSize = ((enc >> 30) & 1) ? TR_DoubleWordReg : TR_WordReg;
        }
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), TR_VectorReg128);
        log->printf(".[%d]", index);
    } else if ((op >= OP::vinswb) && (op <= OP::vinsxd)) {
        done = true;
        const uint32_t elementSizeShift = op - OP::vinswb;
        const uint32_t imm5 = instr->getSourceImmediate() & 0x1f;
        const uint32_t index = imm5 >> (elementSizeShift + 1);

        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), TR_VectorReg128);
        log->printf(".[%d]", index);
        log->prints(", ");
        print(log, instr->getSource1Register(), ((imm5 % 8) == 0) ? TR_DoubleWordReg : TR_WordReg);
    } else if ((op >= OP::vinseb) && (op <= OP::vinsed)) {
        done = true;
        const uint32_t elementSizeShift = op - OP::vinseb;
        const uint32_t imm5 = (instr->getSourceImmediate() >> 5) & 0x1f;
        const uint32_t imm4 = instr->getSourceImmediate() & 0xf;
        const uint32_t dstIndex = imm5 >> (elementSizeShift + 1);
        const uint32_t srcIndex = imm4 >> elementSizeShift;

        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), TR_VectorReg128);
        log->printf(".[%d]", dstIndex);
        log->prints(", ");
        print(log, instr->getSource1Register(), TR_VectorReg128);
        log->printf(".[%d]", srcIndex);
    }

    if (!done) {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->printf(", %d", instr->getSourceImmediate());
    }

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1ZeroImmInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    bool done = false;
    if (op == OP::orrimmx || op == OP::orrimmw) {
        // mov (bitmask immediate) alias
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        auto n = instr->getNbit();
        if (op == OP::orrimmx) {
            uint64_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->prints("movx \t");
                print(log, instr->getTargetRegister(), TR_DoubleWordReg);
                log->printf(", 0x%llx", immediate);
            }
        } else {
            uint32_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->prints("movw \t");
                print(log, instr->getTargetRegister(), TR_WordReg);
                log->printf(", 0x%lx", immediate);
            }
        }
    }
    if (!done) {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), TR_WordReg);
        log->printf(", %d", instr->getSourceImmediate());
    }

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64ZeroSrc1ImmInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    char suffix = (sf == 1) ? 'x' : 'w';
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    bool done = false;
    if (op == OP::subsimmx || op == OP::subsimmw) {
        // cmp alias
        done = true;
        log->printf("cmpimm%c \t", suffix);
        print(log, instr->getSource1Register(), regSize);
        log->printf(", %d", instr->getSourceImmediate());
        if (instr->getNbit()) {
            log->printf(", LSL #%d", 12);
        }
    } else if (op == OP::addsimmx || op == OP::addsimmw) {
        // cmn alias
        done = true;
        log->printf("cmnimm%c \t", suffix);
        print(log, instr->getSource1Register(), regSize);
        log->printf(", %d", instr->getSourceImmediate());
        if (instr->getNbit()) {
            log->printf(", LSL #%d", 12);
        }
    } else if (op == OP::andsimmx || op == OP::andsimmw) {
        // tst alias
        uint32_t imm12 = instr->getSourceImmediate();
        auto immr = imm12 >> 6;
        auto imms = imm12 & 0x3f;
        auto n = instr->getNbit();
        if (op == OP::andsimmx) {
            uint64_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->prints("tstimmx \t");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", 0x%llx", immediate);
            }
        } else {
            uint32_t immediate;
            if (decodeBitMasks(n, immr, imms, immediate)) {
                done = true;
                log->prints("tstimmw \t");
                print(log, instr->getSource1Register(), regSize);
                log->printf(", 0x%lx", immediate);
            }
        }
    }

    if (!done) {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getSource1Register(), regSize);
        log->printf(", %d", instr->getSourceImmediate());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2Instruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    uint32_t sf = getSFbit(enc);
    if (useGPR(enc)) {
        regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    } else {
        regSize = getRegisterSizeFromFType(getFType(enc));
    }

    bool done = false;
    if (op == OP::orrx || op == OP::orrw) {
        TR::Register *r = instr->getSource1Register();
        if (r && r->getRealRegister() && toRealRegister(r)->getRegisterNumber() == TR::RealRegister::xzr) {
            // mov alias
            done = true;
            char suffix = (sf == 1) ? 'x' : 'w';
            log->printf("mov%c \t", suffix);
            print(log, instr->getTargetRegister(), regSize);
            log->prints(", ");
            print(log, instr->getSource2Register(), regSize);
        }
    } else if (op == OP::subsx || op == OP::subsw) {
        TR::Register *r = instr->getTargetRegister();
        if (r && r->getRealRegister() && toRealRegister(r)->getRegisterNumber() == TR::RealRegister::xzr) {
            // cmp alias
            done = true;
            char suffix = (sf == 1) ? 'x' : 'w';
            log->printf("cmp%c \t", suffix);
            print(log, instr->getSource1Register(), regSize);
            log->prints(", ");
            print(log, instr->getSource2Register(), regSize);
        }
    }

    if (!done) {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->prints(", ");
        print(log, instr->getSource2Register(), regSize);
    }

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64ZeroSrc2Instruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    char suffix = (sf == 1) ? 'x' : 'w';
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    if (op == OP::subsx || op == OP::subsw) {
        // cmp alias
        log->printf("cmp%c \t", suffix);
    } else if (op == OP::addsx || op == OP::addsw) {
        // cmn alias
        log->printf("cmn%c \t", suffix);
    } else if (op == OP::andsx || op == OP::andsw) {
        // tst alias
        log->printf("tst%c \t", suffix);
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    }
    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Src1ImmCondInstruction *instr)
{
    printPrefix(log, instr);

    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getSource1Register(), TR_WordReg);
    log->printf(", %d", instr->getSourceImmediate());
    log->printf(", %d", instr->getConditionFlags());
    log->printf(", %s", ARM64ConditionNames[instr->getConditionCode()]);

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Src2CondInstruction *instr)
{
    printPrefix(log, instr);

    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getSource1Register(), TR_WordReg);
    log->prints(", ");
    print(log, instr->getSource2Register(), TR_WordReg);
    log->printf(", %d", instr->getConditionFlags());
    log->printf(", %s", ARM64ConditionNames[instr->getConditionCode()]);

    log->flush();
}

static const char *getBarrierLimitationName(OP::AArch64BarrierLimitation lim)
{
    switch (lim) {
        case OP::sy:
            return "sy";
        case OP::st:
            return "st";
        case OP::ld:
            return "ld";
        case OP::ish:
            return "ish";
        case OP::ishst:
            return "ishst";
        case OP::ishld:
            return "ishld";
        case OP::nsh:
            return "nsh";
        case OP::nshst:
            return "nshst";
        case OP::nshld:
            return "nshld";
        case OP::osh:
            return "osh";
        case OP::oshst:
            return "oshst";
        case OP::oshld:
            return "oshld";

        default:
            return "???";
    }
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64SynchronizationInstruction *instr)
{
    printPrefix(log, instr);
    const char *lim = getBarrierLimitationName(static_cast<OP::AArch64BarrierLimitation>(instr->getSourceImmediate()));
    log->printf("%s \t%s", getOpCodeName(&instr->getOpCode()), lim);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64CondTrg1Src2Instruction *instr)
{
    printPrefix(log, instr);
    TR::Register *r1 = instr->getSource1Register();
    TR::Register *r2 = instr->getSource2Register();
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    if ((r1 == r2) && ((op == OP::csincx) || (op == OP::csincw))) {
        log->printf("cinc%c \t", (sf == 1) ? 'x' : 'w');
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, r1, regSize);
        log->printf(", %s", ARM64ConditionNames[cc_invert(instr->getConditionCode())]);
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->prints(", ");
        print(log, instr->getSource2Register(), regSize);
        log->printf(", %s", ARM64ConditionNames[instr->getConditionCode()]);
    }

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2ImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getTargetRegister(), TR_WordReg);
    log->prints(", ");
    print(log, instr->getSource1Register(), TR_WordReg);
    log->prints(", ");
    print(log, instr->getSource2Register(), TR_WordReg);
    log->printf(", %d", instr->getSourceImmediate());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2ShiftedInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    if ((op == OP::extrw || op == OP::extrx) && (instr->getSource1Register() == instr->getSource2Register())) {
        // ror alias
        log->printf("ror%c \t", (op == OP::extrx) ? 'x' : 'w');
        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->printf(", #%d", instr->getShiftAmount());
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

        print(log, instr->getTargetRegister(), regSize);
        log->prints(", ");
        print(log, instr->getSource1Register(), regSize);
        log->prints(", ");
        print(log, instr->getSource2Register(), regSize);
        log->printf(" %s %d", ARM64ShiftCodeNames[instr->getShiftType()], instr->getShiftAmount());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2ExtendedInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");
    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);
    log->printf(" %s %d", ARM64ExtendCodeNames[instr->getExtendType()], instr->getShiftAmount());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2IndexedElementInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getTargetRegister(), TR_VectorReg128);
    log->prints(", ");
    print(log, instr->getSource1Register(), TR_VectorReg128);
    log->prints(", ");
    print(log, instr->getSource2Register(), TR_VectorReg128);

    log->printf(".[%d]", instr->getIndex());
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src2ZeroInstruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t sf = getSFbit(enc);
    TR_RegisterSizes regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    if (op == OP::maddx || op == OP::maddw) {
        // mul alias
        log->printf("mul%c \t", (sf == 1) ? 'x' : 'w');
    } else {
        log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    }
    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");
    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1Src3Instruction *instr)
{
    printPrefix(log, instr);
    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    if (useGPR(enc)) {
        uint32_t sf = getSFbit(enc);
        regSize = (sf == 1) ? TR_DoubleWordReg : TR_WordReg;
    } else {
        regSize = getRegisterSizeFromFType(getFType(enc));
    }
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");
    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource3Register(), regSize);

    if (instr->getDependencyConditions())
        print(log, instr->getDependencyConditions());

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1MemInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t vr = getVRbit(enc);
    uint32_t size = getLoadStoreSize(enc);
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    if (vr == 0) { // GPR
        switch (size) {
            case 2:
                regSize = TR_WordReg;
                break;
            case 3:
                regSize = TR_DoubleWordReg;
                break;
        }
    } else { // FP/SIMD
        if (size == 3) {
            regSize = TR_DoubleReg;
        } else {
            uint32_t opc = (enc >> 22) & 3; /* bits 22-23 */
            regSize = (size == 0 && opc == 3) ? TR_VectorReg128 : TR_FloatReg;
        }
    }

    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");

    print(log, instr->getMemoryReference());
    TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference()->getSymbol();
    if (symbol && symbol->isSpillTempAuto()) {
        log->printf("\t\t; spilled for %s", getName(instr->getNode()->getOpCode()));
    }
    if (instr->getSnippetForGC() != NULL) {
        log->printf("\t\t; Backpatched branch to Unresolved Data %s",
            getName(instr->getSnippetForGC()->getSnippetLabel()));
    }

    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg2MemInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t vr = getVRbit(enc);
    uint32_t opc = (enc >> 30); /* bits 30-31 */
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    if (vr == 0) { // GPR
        switch (opc) {
            case 0:
            case 1:
                regSize = TR_WordReg;
                break;
            case 2:
                regSize = TR_DoubleWordReg;
                break;
        }
    } else {
        switch (opc) {
            case 0:
                regSize = TR_FloatReg;
                break;
            case 1:
                regSize = TR_DoubleReg;
                break;
            case 2:
                regSize = TR_VectorReg128;
                break;
        }
    }

    print(log, instr->getTargetRegister(), regSize);
    log->prints(", ");
    print(log, instr->getTarget2Register(), regSize);
    log->prints(", ");

    print(log, instr->getMemoryReference());

    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64MemInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    print(log, instr->getMemoryReference());
    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64MemImmInstruction *instr)
{
    OP::Mnemonic op = instr->getOpCodeValue();
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));
    if ((op == OP::prfmoff || op == OP::prfmimm)) {
        uint32_t immediate = instr->getImmediate();
        uint32_t typeValue = (immediate >> 3) & 0x3;
        uint32_t targetValue = (immediate >> 1) & 0x3;
        if ((typeValue != 3) && (targetValue != 3)) {
            ARM64PrefetchType type = static_cast<ARM64PrefetchType>(typeValue);
            ARM64PrefetchTarget target = static_cast<ARM64PrefetchTarget>(targetValue);
            ARM64PrefetchPolicy policy = static_cast<ARM64PrefetchPolicy>(immediate & 0x1);
            log->printf("%s%s%s, ",
                (type == ARM64PrefetchType::LOAD) ? "pld" : ((type == ARM64PrefetchType::INSTRUCTION) ? "pli" : "pst"),
                (target == ARM64PrefetchTarget::L1) ? "l1" : ((target == ARM64PrefetchTarget::L2) ? "l2" : "l3"),
                (policy == ARM64PrefetchPolicy::KEEP) ? "keep" : "strm");
        } else {
            log->printf("#%d, ", instr->getImmediate());
        }
    } else {
        log->printf("#%d, ", instr->getImmediate());
    }
    print(log, instr->getMemoryReference());
    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64MemSrc1Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t vr = getVRbit(enc);
    uint32_t size = getLoadStoreSize(enc);
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    if (vr == 0) { // GPR
        switch (size) {
            case 2:
                regSize = TR_WordReg;
                break;
            case 3:
                regSize = TR_DoubleWordReg;
                break;
        }
    } else { // FP/SIMD
        if (size == 3) {
            regSize = TR_DoubleReg;
        } else {
            uint32_t opc = (enc >> 22) & 3; /* bits 22-23 */
            regSize = (size == 0 && opc == 2) ? TR_VectorReg128 : TR_FloatReg;
        }
    }

    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getMemoryReference());

    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64MemSrc2Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    uint32_t vr = getVRbit(enc);
    uint32_t opc = (enc >> 30); /* bits 30-31 */
    TR_RegisterSizes regSize = TR_UnknownSizeReg;
    if (vr == 0) { // GPR
        switch (opc) {
            case 0:
                regSize = TR_WordReg;
                break;
            case 2:
                regSize = TR_DoubleWordReg;
                break;
        }
    } else { // FP/SIMD
        switch (opc) {
            case 0:
                regSize = TR_FloatReg;
                break;
            case 1:
                regSize = TR_DoubleReg;
                break;
            case 2:
                regSize = TR_VectorReg128;
                break;
        }
    }

    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);
    log->prints(", ");
    print(log, instr->getMemoryReference());

    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Trg1MemSrc1Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    print(log, instr->getTargetRegister(), TR_WordReg);
    log->prints(", ");
    print(log, instr->getSource1Register(), TR_WordReg);
    log->prints(", ");
    print(log, instr->getMemoryReference());

    printMemoryReferenceComment(log, instr->getMemoryReference());
    printInstructionComment(log, 1, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Src1Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    TR_RegisterSizes regSize = getRegisterSizeFromFType(getFType(enc));

    print(log, instr->getSource1Register(), regSize);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARM64Src2Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s \t", getOpCodeName(&instr->getOpCode()));

    OP::Mnemonic op = instr->getOpCodeValue();
    uint32_t enc = OP::getOpCodeBinaryEncoding(op);
    TR_RegisterSizes regSize = getRegisterSizeFromFType(getFType(enc));

    print(log, instr->getSource1Register(), regSize);
    log->prints(", ");
    print(log, instr->getSource2Register(), regSize);

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::RegisterDependency *dep)
{
    log->printc('[');
    print(log, dep->getRegister(), TR_DoubleWordReg);
    log->prints(" : ");
    log->printf("%s] ", getARM64RegisterName(dep->getRealRegister()));
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::RegisterDependencyConditions *conditions)
{
    if (conditions) {
        int i;
        log->prints("\n PRE: ");
        for (i = 0; i < conditions->getAddCursorForPre(); i++) {
            print(log, conditions->getPreConditions()->getRegisterDependency(i));
        }
        log->prints("\nPOST: ");
        for (i = 0; i < conditions->getAddCursorForPost(); i++) {
            print(log, conditions->getPostConditions()->getRegisterDependency(i));
        }
        log->flush();
    }
}

void TR_Debug::printAssocRegDirective(OMR::Logger *log, TR::Instruction *instr)
{
    TR::RegisterDependencyGroup *depGroup = instr->getDependencyConditions()->getPostConditions();

    printPrefix(log, instr);
    log->prints(getOpCodeName(&instr->getOpCode()));
    log->flush();

    auto numPostConditions = instr->getDependencyConditions()->getAddCursorForPost();
    for (int i = 0; i < numPostConditions; i++) {
        TR::RegisterDependency *dependency = depGroup->getRegisterDependency(i);
        TR::Register *virtReg = dependency->getRegister();

        if (virtReg) {
            print(log, dependency);
        }
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::MemoryReference *mr)
{
    log->printc('[');

    if (mr->getBaseRegister() != NULL) {
        print(log, mr->getBaseRegister(), TR_DoubleWordReg);
        log->prints(", ");
    }

    if (mr->getIndexRegister() != NULL) {
        TR::ARM64ExtendCode extendCode = mr->getIndexExtendCode();
        uint8_t scale = mr->getScale();

        TR_RegisterSizes indexSize
            = (extendCode == TR::ARM64ExtendCode::EXT_UXTW || extendCode == TR::ARM64ExtendCode::EXT_SXTW)
            ? TR_WordReg
            : TR_DoubleWordReg;
        print(log, mr->getIndexRegister(), indexSize);

        if (extendCode != TR::ARM64ExtendCode::EXT_LSL) {
            log->printf(", %s", ARM64ExtendCodeNames[extendCode]);
            if (scale != 0) {
                log->printf(" %d", scale);
            }
        } else if (scale != 0) {
            log->printf(", lsl %d", scale);
        } else {
            // print nothing for EXT_LSL with 0 scale (default)
        }
    } else
        log->printf("%d", mr->getOffset(true));

    log->printc(']');
}

void TR_Debug::printARM64GCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *map)
{
    TR::Machine *machine = _cg->machine();

    log->prints("    registers: {");
    for (int i = 0; i < 32; i++) {
        if (map->getMap() & (1 << i))
            log->printf("%s ",
                getName(machine->getRealRegister((TR::RealRegister::RegNum)(i + TR::RealRegister::FirstGPR))));
    }

    log->prints("}\n");
}

void TR_Debug::print(OMR::Logger *log, TR::RealRegister *reg, TR_RegisterSizes size)
{
    log->prints(getName(reg, size));
}

static const char *getRegisterName(TR::RealRegister::RegNum num, bool is64bit)
{
    switch (num) {
        case TR::RealRegister::NoReg:
            return "NoReg";
        case TR::RealRegister::SpilledReg:
            return "SpilledReg";
        case TR::RealRegister::KillVectorRegs:
            return "KillVectorRegs";
        case TR::RealRegister::x0:
            return (is64bit ? "x0" : "w0");
        case TR::RealRegister::x1:
            return (is64bit ? "x1" : "w1");
        case TR::RealRegister::x2:
            return (is64bit ? "x2" : "w2");
        case TR::RealRegister::x3:
            return (is64bit ? "x3" : "w3");
        case TR::RealRegister::x4:
            return (is64bit ? "x4" : "w4");
        case TR::RealRegister::x5:
            return (is64bit ? "x5" : "w5");
        case TR::RealRegister::x6:
            return (is64bit ? "x6" : "w6");
        case TR::RealRegister::x7:
            return (is64bit ? "x7" : "w7");
        case TR::RealRegister::x8:
            return (is64bit ? "x8" : "w8");
        case TR::RealRegister::x9:
            return (is64bit ? "x9" : "w9");
        case TR::RealRegister::x10:
            return (is64bit ? "x10" : "w10");
        case TR::RealRegister::x11:
            return (is64bit ? "x11" : "w11");
        case TR::RealRegister::x12:
            return (is64bit ? "x12" : "w12");
        case TR::RealRegister::x13:
            return (is64bit ? "x13" : "w13");
        case TR::RealRegister::x14:
            return (is64bit ? "x14" : "w14");
        case TR::RealRegister::x15:
            return (is64bit ? "x15" : "w15");
        case TR::RealRegister::x16:
            return (is64bit ? "x16" : "w16");
        case TR::RealRegister::x17:
            return (is64bit ? "x17" : "w17");
        case TR::RealRegister::x18:
            return (is64bit ? "x18" : "w18");
        case TR::RealRegister::x19:
            return (is64bit ? "x19" : "w19");
        case TR::RealRegister::x20:
            return (is64bit ? "x20" : "w20");
        case TR::RealRegister::x21:
            return (is64bit ? "x21" : "w21");
        case TR::RealRegister::x22:
            return (is64bit ? "x22" : "w22");
        case TR::RealRegister::x23:
            return (is64bit ? "x23" : "w23");
        case TR::RealRegister::x24:
            return (is64bit ? "x24" : "w24");
        case TR::RealRegister::x25:
            return (is64bit ? "x25" : "w25");
        case TR::RealRegister::x26:
            return (is64bit ? "x26" : "w26");
        case TR::RealRegister::x27:
            return (is64bit ? "x27" : "w27");
        case TR::RealRegister::x28:
            return (is64bit ? "x28" : "w28");
        case TR::RealRegister::x29:
            return (is64bit ? "x29" : "w29");
        case TR::RealRegister::x30:
            return "lr";
        case TR::RealRegister::sp:
            return "sp";
        case TR::RealRegister::xzr:
            return (is64bit ? "xzr" : "wzr");

        case TR::RealRegister::v0:
            return (is64bit ? "d0" : "s0");
        case TR::RealRegister::v1:
            return (is64bit ? "d1" : "s1");
        case TR::RealRegister::v2:
            return (is64bit ? "d2" : "s2");
        case TR::RealRegister::v3:
            return (is64bit ? "d3" : "s3");
        case TR::RealRegister::v4:
            return (is64bit ? "d4" : "s4");
        case TR::RealRegister::v5:
            return (is64bit ? "d5" : "s5");
        case TR::RealRegister::v6:
            return (is64bit ? "d6" : "s6");
        case TR::RealRegister::v7:
            return (is64bit ? "d7" : "s7");
        case TR::RealRegister::v8:
            return (is64bit ? "d8" : "s8");
        case TR::RealRegister::v9:
            return (is64bit ? "d9" : "s9");
        case TR::RealRegister::v10:
            return (is64bit ? "d10" : "s10");
        case TR::RealRegister::v11:
            return (is64bit ? "d11" : "s11");
        case TR::RealRegister::v12:
            return (is64bit ? "d12" : "s12");
        case TR::RealRegister::v13:
            return (is64bit ? "d13" : "s13");
        case TR::RealRegister::v14:
            return (is64bit ? "d14" : "s14");
        case TR::RealRegister::v15:
            return (is64bit ? "d15" : "s15");
        case TR::RealRegister::v16:
            return (is64bit ? "d16" : "s16");
        case TR::RealRegister::v17:
            return (is64bit ? "d17" : "s17");
        case TR::RealRegister::v18:
            return (is64bit ? "d18" : "s18");
        case TR::RealRegister::v19:
            return (is64bit ? "d19" : "s19");
        case TR::RealRegister::v20:
            return (is64bit ? "d20" : "s20");
        case TR::RealRegister::v21:
            return (is64bit ? "d21" : "s21");
        case TR::RealRegister::v22:
            return (is64bit ? "d22" : "s22");
        case TR::RealRegister::v23:
            return (is64bit ? "d23" : "s23");
        case TR::RealRegister::v24:
            return (is64bit ? "d24" : "s24");
        case TR::RealRegister::v25:
            return (is64bit ? "d25" : "s25");
        case TR::RealRegister::v26:
            return (is64bit ? "d26" : "s26");
        case TR::RealRegister::v27:
            return (is64bit ? "d27" : "s27");
        case TR::RealRegister::v28:
            return (is64bit ? "d28" : "s28");
        case TR::RealRegister::v29:
            return (is64bit ? "d29" : "s29");
        case TR::RealRegister::v30:
            return (is64bit ? "d30" : "s30");
        case TR::RealRegister::v31:
            return (is64bit ? "d31" : "s31");

        default:
            return "???";
    }
}

static const char *getRegisterName(TR::RealRegister::RegNum num, TR_RegisterSizes size)
{
    if (num >= TR::RealRegister::v0 && num <= TR::RealRegister::v31 && size != TR_FloatReg && size != TR_DoubleReg) {
        switch (num) {
            case TR::RealRegister::v0:
                return "v0";
            case TR::RealRegister::v1:
                return "v1";
            case TR::RealRegister::v2:
                return "v2";
            case TR::RealRegister::v3:
                return "v3";
            case TR::RealRegister::v4:
                return "v4";
            case TR::RealRegister::v5:
                return "v5";
            case TR::RealRegister::v6:
                return "v6";
            case TR::RealRegister::v7:
                return "v7";
            case TR::RealRegister::v8:
                return "v8";
            case TR::RealRegister::v9:
                return "v9";
            case TR::RealRegister::v10:
                return "v10";
            case TR::RealRegister::v11:
                return "v11";
            case TR::RealRegister::v12:
                return "v12";
            case TR::RealRegister::v13:
                return "v13";
            case TR::RealRegister::v14:
                return "v14";
            case TR::RealRegister::v15:
                return "v15";
            case TR::RealRegister::v16:
                return "v16";
            case TR::RealRegister::v17:
                return "v17";
            case TR::RealRegister::v18:
                return "v18";
            case TR::RealRegister::v19:
                return "v19";
            case TR::RealRegister::v20:
                return "v20";
            case TR::RealRegister::v21:
                return "v21";
            case TR::RealRegister::v22:
                return "v22";
            case TR::RealRegister::v23:
                return "v23";
            case TR::RealRegister::v24:
                return "v24";
            case TR::RealRegister::v25:
                return "v25";
            case TR::RealRegister::v26:
                return "v26";
            case TR::RealRegister::v27:
                return "v27";
            case TR::RealRegister::v28:
                return "v28";
            case TR::RealRegister::v29:
                return "v29";
            case TR::RealRegister::v30:
                return "v30";
            case TR::RealRegister::v31:
                return "v31";

            default:
                break; // do nothing here
        }
    }

    return getRegisterName(num, (size == TR_DoubleWordReg || size == TR_DoubleReg));
}

const char *TR_Debug::getName(TR::RealRegister *reg, TR_RegisterSizes size)
{
    return getRegisterName(reg->getRegisterNumber(), size);
}

const char *TR_Debug::getName(uint32_t regNum, TR_RegisterSizes size)
{
    return getRegisterName((TR::RealRegister::RegNum)regNum, size);
}

const char *TR_Debug::getARM64RegisterName(uint32_t regNum, bool is64bit)
{
    return getRegisterName((TR::RealRegister::RegNum)regNum, is64bit);
}

void TR_Debug::printARM64OOLSequences(OMR::Logger *log)
{
    auto oiIterator = _cg->getARM64OutOfLineCodeSectionList().begin();

    while (oiIterator != _cg->getARM64OutOfLineCodeSectionList().end()) {
        log->prints("\n------------ start out-of-line instructions\n");
        TR::Instruction *instr = (*oiIterator)->getFirstInstruction();

        do {
            print(log, instr);
            instr = instr->getNext();
        } while (instr != (*oiIterator)->getAppendInstruction());

        if ((*oiIterator)->getAppendInstruction()) {
            print(log, (*oiIterator)->getAppendInstruction());
        }
        log->prints("\n------------ end out-of-line instructions\n");

        ++oiIterator;
    }
}

const char *TR_Debug::getNamea64(TR::Snippet *snippet)
{
    switch (snippet->getKind()) {
        case TR::Snippet::IsCall:
            return "Call Snippet";
            break;
        case TR::Snippet::IsUnresolvedCall:
            return "Unresolved Call Snippet";
            break;
        case TR::Snippet::IsVirtualUnresolved:
            return "Unresolved Virtual Call Snippet";
            break;
        case TR::Snippet::IsInterfaceCall:
            return "Interface Call Snippet";
            break;
        case TR::Snippet::IsStackCheckFailure:
            return "Stack Check Failure Snippet";
            break;
        case TR::Snippet::IsForceRecompilation:
            return "Force Recompilation Snippet";
            break;
        case TR::Snippet::IsUnresolvedData:
            return "Unresolved Data Snippet";
            break;
        case TR::Snippet::IsConstantData:
            return "Constant Data Snippet";
            break;
        case TR::Snippet::IsRecompilation:
            return "Recompilation Snippet";
            break;
        case TR::Snippet::IsHelperCall:
            return "Helper Call Snippet";
            break;
        case TR::Snippet::IsMonitorEnter:
            return "MonitorEnter Inc Counter";
            break;
        case TR::Snippet::IsMonitorExit:
            return "MonitorExit Dec Counter";
            break;
        case TR::Snippet::IsHeapAlloc:
            return "Heap Alloc Snippet";
            break;
        default:
            return "<unknown snippet>";
    }
}

void TR_Debug::printa64(OMR::Logger *log, TR::Snippet *snippet)
{
    switch (snippet->getKind()) {
#ifdef J9_PROJECT_SPECIFIC
        case TR::Snippet::IsCall:
            print(log, (TR::ARM64CallSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedCall:
            print(log, (TR::ARM64UnresolvedCallSnippet *)snippet);
            break;
        case TR::Snippet::IsVirtualUnresolved:
            print(log, (TR::ARM64VirtualUnresolvedSnippet *)snippet);
            break;
        case TR::Snippet::IsInterfaceCall:
            print(log, (TR::ARM64InterfaceCallSnippet *)snippet);
            break;
        case TR::Snippet::IsStackCheckFailure:
            print(log, (TR::ARM64StackCheckFailureSnippet *)snippet);
            break;
        case TR::Snippet::IsForceRecompilation:
            print(log, (TR::ARM64ForceRecompilationSnippet *)snippet);
            break;
        case TR::Snippet::IsRecompilation:
            print(log, (TR::ARM64RecompilationSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsHelperCall:
            snippet->print(log, this);
            break;
        case TR::Snippet::IsUnresolvedData:
            print(log, (TR::UnresolvedDataSnippet *)snippet);
            break;
        case TR::Snippet::IsConstantData:
            print(log, (TR::ARM64ConstantDataSnippet *)snippet);
            break;

        case TR::Snippet::IsMonitorExit:
        case TR::Snippet::IsMonitorEnter:
        case TR::Snippet::IsHeapAlloc:
            snippet->print(log, this);
            break;
        default:
            TR_ASSERT(0, "unexpected snippet kind");
    }
}

// This function assumes that if a trampoline is required then it must be to a helper function.
// Use this API only for inquiring about branches to helpers.
bool TR_Debug::isBranchToTrampoline(TR::SymbolReference *symRef, uint8_t *cursor, int32_t &distance)
{
    uintptr_t target = (uintptr_t)symRef->getMethodAddress();
    bool requiresTrampoline = false;

    if (_cg->directCallRequiresTrampoline(target, (intptr_t)cursor)) {
        target = TR::CodeCacheManager::instance()->findHelperTrampoline(symRef->getReferenceNumber(), (void *)cursor);
        requiresTrampoline = true;
    }

    distance = (int32_t)(target - (intptr_t)cursor);
    return requiresTrampoline;
}
