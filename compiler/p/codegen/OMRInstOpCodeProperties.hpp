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

/*
 * This file will be included within a static table.  Only comments and
 * definitions are permitted.
 */


   {
   /* .mnemonic    = */ OMR::InstOpCode::bad,
   /* .name        = */ "bad",
   /* .description =    "Illegal Opcode", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::brd, */
   /* .name        =    "brd", */
   /* .description =    "Byte-Reverse Doubleword", */
   /* .opcode      =    0x7C0001B6, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::brh, */
   /* .name        =    "brh", */
   /* .description =    "Byte-Reverse Halfword", */
   /* .opcode      =    0x7C000176, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::brw, */
   /* .name        =    "brw", */
   /* .description =    "Byte-Reverse Word", */
   /* .opcode      =    0x7C000136, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::add,
   /* .name        = */ "add",
   /* .description =    "Add", */
   /* .opcode      = */ 0x7C000214,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::add_r,
   /* .name        = */ "add.",
   /* .description =    "Add Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::add].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::add].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::add].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::add].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addc,
   /* .name        = */ "addc",
   /* .description =    "Add carrying", */
   /* .opcode      = */ 0x7C000014,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addc_r,
   /* .name        = */ "addc.",
   /* .description =    "Add carrying Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addc].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addc].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addc].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addc].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addco,
   /* .name        = */ "addco",
   /* .description =    "Add carrying setting overflow", */
   /* .opcode      = */ 0x7C000414,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addco_r,
   /* .name        = */ "addco.",
   /* .description =    "Add carrying setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addco].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addco].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addco].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addco].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::adde,
   /* .name        = */ "adde",
   /* .description =    "Add extended", */
   /* .opcode      = */ 0x7C000114,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::adde_r,
   /* .name        = */ "adde.",
   /* .description =    "Add extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::adde].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::adde].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::adde].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::adde].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addeo,
   /* .name        = */ "addeo",
   /* .description =    "Add extended setting overflow", */
   /* .opcode      = */ 0x7C000514,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addeo_r,
   /* .name        = */ "addeo.",
   /* .description =    "Add extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addi,
   /* .name        = */ "addi",
   /* .description =    "Add immediate", */
   /* .opcode      = */ 0x38000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addic,
   /* .name        = */ "addic",
   /* .description =    "Add immediate carrying", */
   /* .opcode      = */ 0x30000000,
   /* .format      = */ FORMAT_RT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addic_r,
   /* .name        = */ "addic.",
   /* .description =    "Add immediate carrying Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].opcode + 0x04000000,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addi2,
   /* .name        = */ "addi",
   /* .description =    "Add imm (carry bit set only if record form)", */
   /* .opcode      = */ 0x38000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addi2_r,
   /* .name        = */ "addic.",
   /* .description =    "Add imm (carry bit set only if record form) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].opcode + 0x04000000,
   // Note: Normally, addic is not meant to be used as a memory instruction, since its RA field is
   //       not subject to the normal rule about gr0. However, it can be used under safely under
   //       certain controlled conditions that ensure that no code expected the gr0 = 0 behaviour
   //       will be triggered.
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addic].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addis,
   /* .name        = */ "addis",
   /* .description =    "Add immediate shifted", */
   /* .opcode      = */ 0x3C000000,
   /* .format      = */ FORMAT_RT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addme,
   /* .name        = */ "addme",
   /* .description =    "Add to minus one extended", */
   /* .opcode      = */ 0x7C0001D4,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addme_r,
   /* .name        = */ "addme.",
   /* .description =    "Add to minus one extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addme].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addme].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addme].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addme].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addmeo,
   /* .name        = */ "addmeo",
   /* .description =    "Add to minus one extended setting overflow", */
   /* .opcode      = */ 0x7C0005D4,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addmeo_r,
   /* .name        = */ "addmeo.",
   /* .description =    "Add to minus one extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addmeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addmeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addmeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addmeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addo,
   /* .name        = */ "addo",
   /* .description =    "Add setting overflow", */
   /* .opcode      = */ 0x7C000614,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addo_r,
   /* .name        = */ "addo.",
   /* .description =    "Add setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addpcis,
   /* .name        = */ "addpcis",
   /* .description =    "Add PC Immediate Shifted", */
   /* .opcode      = */ 0x4C000004,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addze,
   /* .name        = */ "addze",
   /* .description =    "Add to zero extended", */
   /* .opcode      = */ 0x7C000194,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addze_r,
   /* .name        = */ "addze.",
   /* .description =    "Add to zero extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addze].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addze].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addze].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addze].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addzeo,
   /* .name        = */ "addzeo",
   /* .description =    "Add to zero extended setting overflow", */
   /* .opcode      = */ 0x7C000594,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addzeo_r,
   /* .name        = */ "addzeo.",
   /* .description =    "Add to zero extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addzeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addzeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addzeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addzeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::AND,
   /* .name        = */ "and",
   /* .description =    "AND", */
   /* .opcode      = */ 0x7C000038,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::and_r,
   /* .name        = */ "and.",
   /* .description =    "AND Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::AND].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::AND].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::AND].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::AND].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::andc,
   /* .name        = */ "andc",
   /* .description =    "AND with complement", */
   /* .opcode      = */ 0x7C000078,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::andc_r,
   /* .name        = */ "andc.",
   /* .description =    "AND with complement Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::andc].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::andc].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::andc].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::andc].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::andcxor, */
   /* .name        =    "andcxor", */
   /* .description =    "AND with Complement & XOR", */
   /* .opcode      =    0x10000037, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::addex,
   /* .name        = */ "addex",
   /* .description =    "Add Extended using alternate carry bits", */
   /* .opcode      = */ 0x7C000154,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::addex_r,
   /* .name        = */ "addex.",
   /* .description =    "Add Extended using alternate carry bits Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addex].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addex].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addex].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::addex].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::andi_r,
   /* .name        = */ "andi.",
   /* .description =    "AND immediate", */
   /* .opcode      = */ 0x70000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::andis_r,
   /* .name        = */ "andis.",
   /* .description =    "AND immediate shifted", */
   /* .opcode      = */ 0x74000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::b,
   /* .name        = */ "b",
   /* .description =    "Unconditional branch", */
   /* .opcode      = */ 0x48000000,
   /* .format      = */ FORMAT_I_FORM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bctr,
   /* .name        = */ "bctr",
   /* .description =    "Branch to count register", */
   /* .opcode      = */ 0x4E800420,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bctrl,
   /* .name        = */ "bctrl",
   /* .description =    "Branch to count register and link", */
   /* .opcode      = */ 0x4E800421,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bdnz,
   /* .name        = */ "bdnz",
   /* .description =    "Branch if CTR!=0 after decrementing it", */
   /* .opcode      = */ 0x42000000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr |
                        PPCOpProp_SetsCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bdz,
   /* .name        = */ "bdz",
   /* .description =    "Branch if CTR==0 after decrementing it", */
   /* .opcode      = */ 0x42400000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr |
                        PPCOpProp_SetsCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::beq,
   /* .name        = */ "beq",
   /* .description =    "Branch if equal", */
   /* .opcode      = */ 0x41820000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::beql,
   /* .name        = */ "beql",
   /* .description =    "Branch and link if equal", */
   /* .opcode      = */ 0x41820001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bfctr,
   /* .name        = */ "bfctr",
   /* .description =    "Branch false to count register", */
   /* .opcode      = */ 0x4C800020,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bge,
   /* .name        = */ "bge",
   /* .description =    "Branch if greater than or equal", */
   /* .opcode      = */ 0x40800000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bgel,
   /* .name        = */ "bgel",
   /* .description =    "Branch and link if greater than or equal", */
   /* .opcode      = */ 0x40800001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bgt,
   /* .name        = */ "bgt",
   /* .description =    "Branch if greater than", */
   /* .opcode      = */ 0x41810000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bgtl,
   /* .name        = */ "bgtl",
   /* .description =    "Branch and link if greater than", */
   /* .opcode      = */ 0x41810001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bl,
   /* .name        = */ "bl",
   /* .description =    "Branch and link", */
   /* .opcode      = */ 0x48000001,
   /* .format      = */ FORMAT_I_FORM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ble,
   /* .name        = */ "ble",
   /* .description =    "Branch if less than or equal", */
   /* .opcode      = */ 0x40810000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::blel,
   /* .name        = */ "blel",
   /* .description =    "Branch and link if less than or equal", */
   /* .opcode      = */ 0x40810001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::blt,
   /* .name        = */ "blt",
   /* .description =    "Branch if less than", */
   /* .opcode      = */ 0x41800000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bltl,
   /* .name        = */ "bltl",
   /* .description =    "Branch and link if less than", */
   /* .opcode      = */ 0x41800001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::blr,
   /* .name        = */ "blr",
   /* .description =    "Branch to link register", */
   /* .opcode      = */ 0x4E800020,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::blrl,
   /* .name        = */ "blrl",
   /* .description =    "Branch to link register and link", */
   /* .opcode      = */ 0x4E800021,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bne,
   /* .name        = */ "bne",
   /* .description =    "Branch if not equal", */
   /* .opcode      = */ 0x40820000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bnel,
   /* .name        = */ "bnel",
   /* .description =    "Branch and link if not equal", */
   /* .opcode      = */ 0x40820001,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bnun,
   /* .name        = */ "bnu",
   /* .description =    "Branch if not unordered", */
   /* .opcode      = */ 0x40830000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::btctr,
   /* .name        = */ "btctr",
   /* .description =    "Branch true to count register", */
   /* .opcode      = */ 0x4D800020,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp |
                        PPCOpProp_UsesCtr,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bun,
   /* .name        = */ "bun",
   /* .description =    "Branch if unordered", */
   /* .opcode      = */ 0x41830000,
   /* .format      = */ FORMAT_B_FORM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::beqlr,
   /* .name        = */ "beqlr",
   /* .description =    "Branch to link register if equal", */
   /* .opcode      = */ 0x4D820020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bgelr,
   /* .name        = */ "bgelr",
   /* .description =    "Branch to link register if greater than or equal", */
   /* .opcode      = */ 0x4C800020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bgtlr,
   /* .name        = */ "bgtlr",
   /* .description =    "Branch to link register if greater than", */
   /* .opcode      = */ 0x4D810020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::blelr,
   /* .name        = */ "blelr",
   /* .description =    "Branch to link register if less than or equal", */
   /* .opcode      = */ 0x4C810020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bltlr,
   /* .name        = */ "bltlr",
   /* .description =    "Branch to link register if less than", */
   /* .opcode      = */ 0x4D800020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::bnelr,
   /* .name        = */ "bnelr",
   /* .description =    "Branch to link register if not equal", */
   /* .opcode      = */ 0x4C820020,
   /* .format      = */ FORMAT_XL_FORM_BRANCH,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmp4,
   /* .name        = */ "cmpw",
   /* .description =    "Compare word algebraic", */
   /* .opcode      = */ 0x7C000000,
   /* .format      = */ FORMAT_BF_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmp8,
   /* .name        = */ "cmpd",
   /* .description =    "Compare dword algebraic", */
   /* .opcode      = */ 0x7C200000,
   /* .format      = */ FORMAT_BF_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DWord |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpi4,
   /* .name        = */ "cmpwi",
   /* .description =    "Compare word immediate algebraic", */
   /* .opcode      = */ 0x2C000000,
   /* .format      = */ FORMAT_BF_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpi8,
   /* .name        = */ "cmpdi",
   /* .description =    "Compare dword immediate algebraic", */
   /* .opcode      = */ 0x2C200000,
   /* .format      = */ FORMAT_BF_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DWord |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpl4,
   /* .name        = */ "cmplw",
   /* .description =    "Compare word logical", */
   /* .opcode      = */ 0x7C000040,
   /* .format      = */ FORMAT_BF_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpl8,
   /* .name        = */ "cmpld",
   /* .description =    "Compare dword logical", */
   /* .opcode      = */ 0x7C200040,
   /* .format      = */ FORMAT_BF_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DWord |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpli4,
   /* .name        = */ "cmplwi",
   /* .description =    "Compare word immediate logical", */
   /* .opcode      = */ 0x28000000,
   /* .format      = */ FORMAT_BF_RA_UI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpli8,
   /* .name        = */ "cmpldi",
   /* .description =    "Compare dword immediate logical", */
   /* .opcode      = */ 0x28200000,
   /* .format      = */ FORMAT_BF_RA_UI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DWord |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmprb,
   /* .name        = */ "cmprb",
   /* .description =    "Compare Ranged Byte", */
   /* .opcode      = */ 0x7C000180,
   /* .format      = */ FORMAT_BF_RA_RB_L,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cmpeqb,
   /* .name        = */ "cmpeqb",
   /* .description =    "Compare Equal Byte", */
   /* .opcode      = */ 0x7C0001C0,
   /* .format      = */ FORMAT_BF_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cntlzd,
   /* .name        = */ "cntlzd",
   /* .description =    "Count leading zeros dword", */
   /* .opcode      = */ 0x7C000074,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cntlzd_r,
   /* .name        = */ "cntlzd.",
   /* .description =    "Count leading zeros dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cntlzw,
   /* .name        = */ "cntlzw",
   /* .description =    "Count leading zeros word", */
   /* .opcode      = */ 0x7C000034,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cntlzw_r,
   /* .name        = */ "cntlzw.",
   /* .description =    "Count leading zeros word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cntlzw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::cnttzw, */
   /* .name        =    "cnttzw", */
   /* .description =    "Count Trailing Zeros Word", */
   /* .opcode      =    0x7C000434, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::cnttzw_r, */
   /* .name        =    "cnttzw.", */
   /* .description =    "Count Trailing Zeros Word Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzw].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzw].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzw].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::cnttzd, */
   /* .name        =    "cnttzd", */
   /* .description =    "Count Trailing Zeros Dword", */
   /* .opcode      =    0x7C000474, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::cnttzd_r, */
   /* .name        =    "cnttzd.", */
   /* .description =    "Count Trailing Zeros Dword Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzd].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzd].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzd].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::cnttzd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::crand,
   /* .name        = */ "crand",
   /* .description =    "Condition register AND", */
   /* .opcode      = */ 0x4C000202,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::crandc,
   /* .name        = */ "crandc",
   /* .description =    "Condition register AND with complement", */
   /* .opcode      = */ 0x4C000102,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::creqv,
   /* .name        = */ "creqv",
   /* .description =    "Condition register equivalent", */
   /* .opcode      = */ 0x4C000242,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::crnand,
   /* .name        = */ "crnand",
   /* .description =    "Condition register NAND", */
   /* .opcode      = */ 0x4C0001C2,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::crnor,
   /* .name        = */ "crnor",
   /* .description =    "Condition register NOR", */
   /* .opcode      = */ 0x4C000042,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::cror,
   /* .name        = */ "cror",
   /* .description =    "Condition register OR", */
   /* .opcode      = */ 0x4C000382,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::crorc,
   /* .name        = */ "crorc",
   /* .description =    "Condition register OR with complement", */
   /* .opcode      = */ 0x4C000342,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::crxor,
   /* .name        = */ "crxor",
   /* .description =    "Condition register XOR", */
   /* .opcode      = */ 0x4C000182,
   /* .format      = */ FORMAT_BT_BA_BB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_CRLogical |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dadd,
   /* .name        = */ "dadd",
   /* .description =    "Add (DFP64)", */
   /* .opcode      = */ 0xEC000004,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dadd_r,
   /* .name        = */ "dadd.",
   /* .description =    "Add (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dadd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dadd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dadd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dadd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::darn,
   /* .name        = */ "darn",
   /* .description =    "Deliver a random number", */
   /* .opcode      = */ 0x7C0005E6,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcbt,
   /* .name        = */ "dcbt",
   /* .description =    "Data cache block touch", */
   /* .opcode      = */ 0x7C00022C,
   /* .format      = */ FORMAT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcbtst,
   /* .name        = */ "dcbtst",
   /* .description =    "Data cache block touch for store", */
   /* .opcode      = */ 0x7C0001EC,
   /* .format      = */ FORMAT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcbtstt,
   /* .name        = */ "dcbtstt",
   /* .description =    "Data cache block touch for store - transient", */
   /* .opcode      = */ 0x7E0001EC,
   /* .format      = */ FORMAT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcbtt,
   /* .name        = */ "dcbtt",
   /* .description =    "Data cache block touch - transient", */
   /* .opcode      = */ 0x7E00022C,
   /* .format      = */ FORMAT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcbz,
   /* .name        = */ "dcbz",
   /* .description =    "Data cache block zero", */
   /* .opcode      = */ 0x7C0007EC,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcffix,
   /* .name        = */ "dcffix",
   /* .description =    "Convert From Fixed (DFP64)", */
   /* .opcode      = */ 0xEC000644,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcffix_r,
   /* .name        = */ "dcffix.",
   /* .description =    "Convert From Fixed (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffix].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffix].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffix].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffix].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcffixq,
   /* .name        = */ "dcffixq",
   /* .description =    "Convert From Fixed (DFP128)", */
   /* .opcode      = */ 0xFC000644,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcffixq_r,
   /* .name        = */ "dcffixq.",
   /* .description =    "Convert From Fixed (DFP128) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffixq].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffixq].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffixq].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dcffixq].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dcmpu,
   /* .name        = */ "dcmpu",
   /* .description =    "Unordered Compare (DFP64)", */
   /* .opcode      = */ 0xEC000504,
   /* .format      = */ FORMAT_BF_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dctfix,
   /* .name        = */ "dctfix",
   /* .description =    "Convert to Fixed (DFP64)", */
   /* .opcode      = */ 0xEC000244,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dctfix_r,
   /* .name        = */ "dctfix.",
   /* .description =    "Convert to Fixed (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dctfix].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dctfix].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dctfix].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dctfix].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ddedpd,
   /* .name        = */ "ddedpd",
   /* .description =    "Decode DPD to BCD (DFP64)", */
   /* .opcode      = */ 0xEC000284,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ddedpd_r,
   /* .name        = */ "ddedpd.",
   /* .description =    "Decode DPD to BCD (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddedpd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddedpd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddedpd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddedpd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ddiv,
   /* .name        = */ "ddiv",
   /* .description =    "Divide (DFP64)", */
   /* .opcode      = */ 0xEC000444,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ddiv_r,
   /* .name        = */ "ddiv.",
   /* .description =    "Divide (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddiv].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddiv].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddiv].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::ddiv].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::denbcdu,
   /* .name        = */ "denbcdu",
   /* .description =    "Encode Unsigned BCD to DFP (DFP64)", */
   /* .opcode      = */ 0xEC000684,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::denbcdu_r,
   /* .name        = */ "denbcdu.",
   /* .description =    "Encode Unsigned BCD to DFP (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::denbcdu].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::denbcdu].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::denbcdu].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::denbcdu].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divd,
   /* .name        = */ "divd",
   /* .description =    "Divide dword", */
   /* .opcode      = */ 0x7C0003D2,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divd_r,
   /* .name        = */ "divd.",
   /* .description =    "Divide dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::diex,
   /* .name        = */ "diex",
   /* .description =    "Insert Biased Exponent (DFP64)", */
   /* .opcode      = */ 0xEC0006C4,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::diex_r,
   /* .name        = */ "diex.",
   /* .description =    "Insert Biased Exponent (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::diex].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::diex].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::diex].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::diex].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divdo,
   /* .name        = */ "divdo",
   /* .description =    "Divide dword setting overflow", */
   /* .opcode      = */ 0x7C0007D2,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divdo_r,
   /* .name        = */ "divdo.",
   /* .description =    "Divide dword setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divdu,
   /* .name        = */ "divdu",
   /* .description =    "Divide dword unsigned", */
   /* .opcode      = */ 0x7C000392,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divdu_r,
   /* .name        = */ "divdu.",
   /* .description =    "Divide dword unsigned Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdu].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdu].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdu].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divdu].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divduo,
   /* .name        = */ "divduo",
   /* .description =    "Divide dword unsigned setting overflow", */
   /* .opcode      = */ 0x7C000792,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divduo_r,
   /* .name        = */ "divduo.",
   /* .description =    "Divide dword unsigned setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divduo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divduo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divduo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divduo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divw,
   /* .name        = */ "divw",
   /* .description =    "Divide word", */
   /* .opcode      = */ 0x7C0003D6,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divw_r,
   /* .name        = */ "divw.",
   /* .description =    "Divide word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwo,
   /* .name        = */ "divwo",
   /* .description =    "Divide word setting overflow", */
   /* .opcode      = */ 0x7C0007D6,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwo_r,
   /* .name        = */ "divwo.",
   /* .description =    "Divide word setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwu,
   /* .name        = */ "divwu",
   /* .description =    "Divide word unsigned", */
   /* .opcode      = */ 0x7C000396,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwu_r,
   /* .name        = */ "divwu.",
   /* .description =    "Divide word unsigned Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwu].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwu].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwu].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwu].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwuo,
   /* .name        = */ "divwuo",
   /* .description =    "Divide word unsigned setting overflow", */
   /* .opcode      = */ 0x7C000796,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::divwuo_r,
   /* .name        = */ "divwuo.",
   /* .description =    "Divide word unsigned setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwuo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwuo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwuo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::divwuo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dmul,
   /* .name        = */ "dmul",
   /* .description =    "Multiply (DFP64)", */
   /* .opcode      = */ 0xEC000044,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dmul_r,
   /* .name        = */ "dmul.",
   /* .description =    "Multiply (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dmul].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dmul].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dmul].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dmul].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dqua,
   /* .name        = */ "dqua",
   /* .description =    "Quantize (DFP64)", */
   /* .opcode      = */ 0xEC000006,
   /* .format      = */ FORMAT_FRT_FRA_FRB_RMC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dqua_r,
   /* .name        = */ "dqua.",
   /* .description =    "Quantize (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dqua].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dqua].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dqua].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dqua].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::drrnd,
   /* .name        = */ "drrnd",
   /* .description =    "Reround (DFP64)", */
   /* .opcode      = */ 0xEC000046,
   /* .format      = */ FORMAT_FRT_FRA_FRB_RMC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::drrnd_r,
   /* .name        = */ "drrnd.",
   /* .description =    "Reround (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drrnd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drrnd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drrnd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drrnd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::drdpq,
   /* .name        = */ "drdpq",
   /* .description =    "Round To DFP64 (DFP128)", */
   /* .opcode      = */ 0xFC000604,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::drdpq_r,
   /* .name        = */ "drdpq.",
   /* .description =    "Round To DFP64 (DFP128) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drdpq].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drdpq].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drdpq].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::drdpq].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dsub,
   /* .name        = */ "dsub",
   /* .description =    "Subtract (DFP64)", */
   /* .opcode      = */ 0xEC000404,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dsub_r,
   /* .name        = */ "dsub.",
   /* .description =    "Subtract (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dsub].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dsub].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dsub].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dsub].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dtstdc,
   /* .name        = */ "dtstdc",
   /* .description =    "Test Data Class (DFP64)", */
   /* .opcode      = */ 0xEC000184,
   /* .format      = */ FORMAT_BF_FRA_DM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dtstdg,
   /* .name        = */ "dtstdg",
   /* .description =    "Test Data Group (DFP64)", */
   /* .opcode      = */ 0xEC0001C4,
   /* .format      = */ FORMAT_BF_FRA_DM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::dtstsfi, */
   /* .name        =    "dtstsfi", */
   /* .description =    "DFP Test Significance Immediate", */
   /* .opcode      =    0xEC000546, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::dtstsfiq, */
   /* .name        =    "dtstsfiq", */
   /* .description =    "DFP Test Significance Immediate Quad", */
   /* .opcode      =    0xFC000546, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::dxex,
   /* .name        = */ "dxex",
   /* .description =    "Extract Biased Exponent (DFP64)", */
   /* .opcode      = */ 0xEC0002C4,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dxex_r,
   /* .name        = */ "dxex.",
   /* .description =    "Extract Biased Exponent (DFP64) Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dxex].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dxex].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dxex].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::dxex].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::eieio,
   /* .name        = */ "eieio",
   /* .description =    "Enforce in order execution of I/O", */
   /* .opcode      = */ 0x7C0006AC,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::eqv,
   /* .name        = */ "eqv",
   /* .description =    "Equivalent", */
   /* .opcode      = */ 0x7C000238,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::eqv_r,
   /* .name        = */ "eqv.",
   /* .description =    "Equivalent Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::eqv].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::eqv].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::eqv].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::eqv].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsb,
   /* .name        = */ "extsb",
   /* .description =    "Extend sign byte", */
   /* .opcode      = */ 0x7C000774,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsb_r,
   /* .name        = */ "extsb.",
   /* .description =    "Extend sign byte Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsb].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsb].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsb].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsb].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsh,
   /* .name        = */ "extsh",
   /* .description =    "Extend sign half word", */
   /* .opcode      = */ 0x7C000734,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsh_r,
   /* .name        = */ "extsh.",
   /* .description =    "Extend sign half word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsh].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsh].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsh].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsw,
   /* .name        = */ "extsw",
   /* .description =    "Extend sign word", */
   /* .opcode      = */ 0x7C0007B4,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extsw_r,
   /* .name        = */ "extsw.",
   /* .description =    "Extend sign word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extsw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extswsli,
   /* .name        = */ "extswsli",
   /* .description =    "Extend Sign Word & Shift Left Immediate", */
   /* .opcode      = */ 0x7C0006F4,
   /* .format      = */ FORMAT_RA_RS_SH6,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::extswsli_r,
   /* .name        = */ "extswsli.",
   /* .description =    "Extend Sign Word & Shift Left Immediate Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extswsli].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extswsli].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extswsli].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::extswsli].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fabs,
   /* .name        = */ "fabs",
   /* .description =    "Floating absolute value", */
   /* .opcode      = */ 0xFC000210,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fadd,
   /* .name        = */ "fadd",
   /* .description =    "Floating add", */
   /* .opcode      = */ 0xFC00002A,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fadds,
   /* .name        = */ "fadds",
   /* .description =    "Floating add single", */
   /* .opcode      = */ 0xEC00002A,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcfid,
   /* .name        = */ "fcfid",
   /* .description =    "Floating convert from integer dword", */
   /* .opcode      = */ 0xFC00069C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcfidu,
   /* .name        = */ "fcfidu",
   /* .description =    "Floating convert from integer dword unsigned", */
   /* .opcode      = */ 0xFC00079C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcfids,
   /* .name        = */ "fcfids",
   /* .description =    "Floating convert from integer dword single", */
   /* .opcode      = */ 0xEC00069C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcfidus,
   /* .name        = */ "fcfidus",
   /* .description =    "Floating convert from integer dword unsigned single", */
   /* .opcode      = */ 0xEC00079C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcmpo,
   /* .name        = */ "fcmpo",
   /* .description =    "Floating compare ordered", */
   /* .opcode      = */ 0xFC000040,
   /* .format      = */ FORMAT_BF_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcmpu,
   /* .name        = */ "fcmpu",
   /* .description =    "Floating compare unordered", */
   /* .opcode      = */ 0xFC000000,
   /* .format      = */ FORMAT_BF_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fcpsgn,
   /* .name        = */ "fcpsgn",
   /* .description =    "Floating copy sign", */
   /* .opcode      = */ 0xFC000010,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fctid,
   /* .name        = */ "fctid",
   /* .description =    "Floating convert to integer dword", */
   /* .opcode      = */ 0xFC00065C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fctidz,
   /* .name        = */ "fctidz",
   /* .description =    "Floating convert to integer dword round toward zero", */
   /* .opcode      = */ 0xFC00065E,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fctiw,
   /* .name        = */ "fctiw",
   /* .description =    "Floating convert to integer word", */
   /* .opcode      = */ 0xFC00001C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios2,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fctiwz,
   /* .name        = */ "fctiwz",
   /* .description =    "Floating convert to integer word round toward zero", */
   /* .opcode      = */ 0xFC00001E,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios2,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fdiv,
   /* .name        = */ "fdiv",
   /* .description =    "Floating divide double", */
   /* .opcode      = */ 0xFC000024,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fdivs,
   /* .name        = */ "fdivs",
   /* .description =    "Floating divide single", */
   /* .opcode      = */ 0xEC000024,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmadd,
   /* .name        = */ "fmadd",
   /* .description =    "Floating multiply add double", */
   /* .opcode      = */ 0xFC00003A,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmadds,
   /* .name        = */ "fmadds",
   /* .description =    "Floating multiply add single", */
   /* .opcode      = */ 0xEC00003A,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmr,
   /* .name        = */ "fmr",
   /* .description =    "Floating move register", */
   /* .opcode      = */ 0xFC000090,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_IsRegCopy |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmsub,
   /* .name        = */ "fmsub",
   /* .description =    "Floating multiply subtract double", */
   /* .opcode      = */ 0xFC000038,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmsubs,
   /* .name        = */ "fmsubs",
   /* .description =    "Floating multiply subtract single", */
   /* .opcode      = */ 0xEC000038,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmul,
   /* .name        = */ "fmul",
   /* .description =    "Floating multiply double", */
   /* .opcode      = */ 0xFC000032,
   /* .format      = */ FORMAT_FRT_FRA_FRC,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmuls,
   /* .name        = */ "fmuls",
   /* .description =    "Floating multiply single", */
   /* .opcode      = */ 0xEC000032,
   /* .format      = */ FORMAT_FRT_FRA_FRC,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fnabs,
   /* .name        = */ "fnabs",
   /* .description =    "Floating negative absolute value", */
   /* .opcode      = */ 0xFC000110,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fneg,
   /* .name        = */ "fneg",
   /* .description =    "Floating negate", */
   /* .opcode      = */ 0xFC000050,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fnmadd,
   /* .name        = */ "fnmadd",
   /* .description =    "Floating negative multiply add double", */
   /* .opcode      = */ 0xFC00003E,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fnmadds,
   /* .name        = */ "fnmadds",
   /* .description =    "Floating negative multiply add single", */
   /* .opcode      = */ 0xEC00003E,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fnmsub,
   /* .name        = */ "fnmsub",
   /* .description =    "Floating negative multiply subtract double", */
   /* .opcode      = */ 0xFC00003C,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fnmsubs,
   /* .name        = */ "fnmsubs",
   /* .description =    "Floating negative multiply subtract single", */
   /* .opcode      = */ 0xEC00003C,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fres,
   /* .name        = */ "fres",
   /* .description =    "Floating reciprocal estimate single", */
   /* .opcode      = */ 0xFC000030,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::frim,
   /* .name        = */ "frim",
   /* .description =    "Floating round to minus (floor) double", */
   /* .opcode      = */ 0xFC0003D0,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCgr,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::frin,
   /* .name        = */ "frin",
   /* .description =    "Floating round to nearest double", */
   /* .opcode      = */ 0xFC000310,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCgr,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::frip,
   /* .name        = */ "frip",
   /* .description =    "Floating round to plus (ceil) double", */
   /* .opcode      = */ 0xFC000390,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCgr,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::frsp,
   /* .name        = */ "frsp",
   /* .description =    "Floating round to single", */
   /* .opcode      = */ 0xFC000018,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::frsqrte,
   /* .name        = */ "frsqrte",
   /* .description =    "Floating reciprocal square root estimate", */
   /* .opcode      = */ 0xFC000034,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fsel,
   /* .name        = */ "fsel",
   /* .description =    "Floating select", */
   /* .opcode      = */ 0xFC00002E,
   /* .format      = */ FORMAT_FRT_FRA_FRC_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fsqrt,
   /* .name        = */ "fsqrt",
   /* .description =    "Floating square root double", */
   /* .opcode      = */ 0xFC00002C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios2,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fsqrts,
   /* .name        = */ "fsqrts",
   /* .description =    "Floating square root single", */
   /* .opcode      = */ 0xEC00002C,
   /* .format      = */ FORMAT_FRT_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fsub,
   /* .name        = */ "fsub",
   /* .description =    "Floating subtract double", */
   /* .opcode      = */ 0xFC000028,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fsubs,
   /* .name        = */ "fsubs",
   /* .description =    "Floating subtract single", */
   /* .opcode      = */ 0xEC000028,
   /* .format      = */ FORMAT_FRT_FRA_FRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::isync,
   /* .name        = */ "isync",
   /* .description =    "Instruction synchronize", */
   /* .opcode      = */ 0x4C00012C,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsSync,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lbz,
   /* .name        = */ "lbz",
   /* .description =    "Load byte and zero extend", */
   /* .opcode      = */ 0x88000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lbzu,
   /* .name        = */ "lbzu",
   /* .description =    "Load byte and zero extend with update", */
   /* .opcode      = */ 0x8C000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lbzux,
   /* .name        = */ "lbzux",
   /* .description =    "Load byte and zero extend with update indexed", */
   /* .opcode      = */ 0x7C0000EE,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lbzx,
   /* .name        = */ "lbzx",
   /* .description =    "Load byte and zero extend indexed", */
   /* .opcode      = */ 0x7C0000AE,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ld,
   /* .name        = */ "ld",
   /* .description =    "Load dword", */
   /* .opcode      = */ 0xE8000000,
   /* .format      = */ FORMAT_RT_DS_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_OffsetRequiresWordAlignment,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldarx,
   /* .name        = */ "ldarx",
   /* .description =    "Load dword and reserve indexed", */
   /* .opcode      = */ 0x7C0000A8,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldu,
   /* .name        = */ "ldu",
   /* .description =    "Load dword with update", */
   /* .opcode      = */ 0xE8000001,
   /* .format      = */ FORMAT_RT_DS_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_OffsetRequiresWordAlignment,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldux,
   /* .name        = */ "ldux",
   /* .description =    "Load dword with update indexed", */
   /* .opcode      = */ 0x7C00006A,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldx,
   /* .name        = */ "ldx",
   /* .description =    "Load dword indexed", */
   /* .opcode      = */ 0x7C00002A,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfd,
   /* .name        = */ "lfd",
   /* .description =    "Load floating point double", */
   /* .opcode      = */ 0xC8000000,
   /* .format      = */ FORMAT_FRT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfdp,
   /* .name        = */ "lfdp",
   /* .description =    "Load floating point double pair", */
   /* .opcode      = */ 0xE4000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfdu,
   /* .name        = */ "lfdu",
   /* .description =    "Load floating point double with update", */
   /* .opcode      = */ 0xCC000000,
   /* .format      = */ FORMAT_FRT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfdux,
   /* .name        = */ "lfdux",
   /* .description =    "Load floating point double with update indexed", */
   /* .opcode      = */ 0x7C0004EE,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfdx,
   /* .name        = */ "lfdx",
   /* .description =    "Load floating point double indexed", */
   /* .opcode      = */ 0x7C0004AE,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfdpx,
   /* .name        = */ "lfdpx",
   /* .description =    "Load floating point double pair indexed", */
   /* .opcode      = */ 0x7C00062E,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfiwax,
   /* .name        = */ "lfiwax",
   /* .description =    "Load floating point as integer word algebraic", */
   /* .opcode      = */ 0x7C0006AE,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfiwzx,
   /* .name        = */ "lfiwzx",
   /* .description =    "Load floating point as integer word and zero indexed", */
   /* .opcode      = */ 0x7C0006EE,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfs,
   /* .name        = */ "lfs",
   /* .description =    "Load floating short", */
   /* .opcode      = */ 0xC0000000,
   /* .format      = */ FORMAT_FRT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfsu,
   /* .name        = */ "lfsu",
   /* .description =    "Load floating short with update", */
   /* .opcode      = */ 0xC4000000,
   /* .format      = */ FORMAT_FRT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfsux,
   /* .name        = */ "lfsux",
   /* .description =    "Load floating short with update indexed", */
   /* .opcode      = */ 0x7C00046E,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lfsx,
   /* .name        = */ "lfsx",
   /* .description =    "Load floating short indexed", */
   /* .opcode      = */ 0x7C00042E,
   /* .format      = */ FORMAT_FRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lha,
   /* .name        = */ "lha",
   /* .description =    "Load half word algebraic", */
   /* .opcode      = */ 0xA8000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhau,
   /* .name        = */ "lhau",
   /* .description =    "Load half word algebraic", */
   /* .opcode      = */ 0xAC000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhaux,
   /* .name        = */ "lhaux",
   /* .description =    "Load half word algebraic", */
   /* .opcode      = */ 0x7C0002EE,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhax,
   /* .name        = */ "lhax",
   /* .description =    "Load half word algebraic", */
   /* .opcode      = */ 0x7C0002AE,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhbrx,
   /* .name        = */ "lhbrx",
   /* .description =    "Load half word byte reversed indexed", */
   /* .opcode      = */ 0x7C00062C,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhz,
   /* .name        = */ "lhz",
   /* .description =    "Load half word and zero extend", */
   /* .opcode      = */ 0xA0000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhzu,
   /* .name        = */ "lhzu",
   /* .description =    "Load half word and zero extend with update", */
   /* .opcode      = */ 0xA4000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhzux,
   /* .name        = */ "lhzux",
   /* .description =    "Load half word and zero extend with update indexed", */
   /* .opcode      = */ 0x7C00026E,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lhzx,
   /* .name        = */ "lhzx",
   /* .description =    "Load half word and zero extend indexed", */
   /* .opcode      = */ 0x7C00022E,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::li,
   /* .name        = */ "li",
   /* .description =    "Load immediate", */
   /* .opcode      = */ 0x38000000,
   /* .format      = */ FORMAT_RT_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lis,
   /* .name        = */ "lis",
   /* .description =    "Load immediate shifted", */
   /* .opcode      = */ 0x3C000000,
   /* .format      = */ FORMAT_RT_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lmw,
   /* .name        = */ "lmw",
   /* .description =    "Load multiple word", */
   /* .opcode      = */ 0xB8000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lswi,
   /* .name        = */ "lswi",
   /* .description =    "Load string word immediate", */
   /* .opcode      = */ 0x7C0004AA,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lswx,
   /* .name        = */ "lswx",
   /* .description =    "Load string word indexed", */
   /* .opcode      = */ 0x7C00042A,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwa,
   /* .name        = */ "lwa",
   /* .description =    "Load word algebraic", */
   /* .opcode      = */ 0xE8000002,
   /* .format      = */ FORMAT_RT_DS_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_OffsetRequiresWordAlignment,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwarx,
   /* .name        = */ "lwarx",
   /* .description =    "Load word and reserve indexed", */
   /* .opcode      = */ 0x7C000028,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwaux,
   /* .name        = */ "lwaux",
   /* .description =    "Load word algebraic with update indexed", */
   /* .opcode      = */ 0x7C0002EA,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwax,
   /* .name        = */ "lwax",
   /* .description =    "Load word algebraic indexed", */
   /* .opcode      = */ 0x7C0002AA,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwbrx,
   /* .name        = */ "lwbrx",
   /* .description =    "Load word byte reverse indexed", */
   /* .opcode      = */ 0x7C00042C,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldbrx,
   /* .name        = */ "ldbrx",
   /* .description =    "Load doubleword byte reverse indexed", */
   /* .opcode      = */ 0x7C000428,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwsync,
   /* .name        = */ "lwsync",
   /* .description =    "Lightweight Synchronize", */
   /* .opcode      = */ 0x7C2004AC,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_IsSync,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwz,
   /* .name        = */ "lwz",
   /* .description =    "Load word and zero extend", */
   /* .opcode      = */ 0x80000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwzu,
   /* .name        = */ "lwzu",
   /* .description =    "Load word and zero extend with update", */
   /* .opcode      = */ 0x84000000,
   /* .format      = */ FORMAT_RT_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwzux,
   /* .name        = */ "lwzux",
   /* .description =    "Load word and zero with update indexed", */
   /* .opcode      = */ 0x7C00006E,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lwzx,
   /* .name        = */ "lwzx",
   /* .description =    "Load word and zero extend indexed", */
   /* .opcode      = */ 0x7C00002E,
   /* .format      = */ FORMAT_RT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_AltFormat,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::maddhd, */
   /* .name        =    "maddhd", */
   /* .description =    "Multiply-Add High Dword", */
   /* .opcode      =    0x10000030, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::maddhdu, */
   /* .name        =    "maddhdu", */
   /* .description =    "Multiply-Add High Dword Unsigned", */
   /* .opcode      =    0x10000031, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::maddld,
   /* .name        = */ "maddld",
   /* .description =    "Multiply-Add Low Dword", */
   /* .opcode      = */ 0x10000033,
   /* .format      = */ FORMAT_RT_RA_RB_RC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mcrf,
   /* .name        = */ "mcrf",
   /* .description =    "Move condition register field", */
   /* .opcode      = */ 0x4C000000,
   /* .format      = */ FORMAT_BF_BFA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mcrfs,
   /* .name        = */ "mcrfs",
   /* .description =    "Move to condition register field from FPSCR", */
   /* .opcode      = */ 0xFC000080,
   /* .format      = */ FORMAT_BF_BFAI,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_ReadsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mcrxr,
   /* .name        = */ "mcrxr",
   /* .description =    "Move to condition register field from XER", */
   /* .opcode      = */ 0x7C000400,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mcrxrx, */
   /* .name        =    "mcrxrx", */
   /* .description =    "Move XER to CR Extended", */
   /* .opcode      =    0x7C000480, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfcr,
   /* .name        = */ "mfcr",
   /* .description =    "Move from condition register to gpr", */
   /* .opcode      = */ 0x7C000026,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfocrf,
   /* .name        = */ "mfocrf",
   /* .description =    "Move from one condition register field to gpr (XFX-form)", */
   /* .opcode      = */ 0x7C100026,
   /* .format      = */ FORMAT_RT_FXM1,
   /* .minimumALS  = */ TR_Processor::TR_PPCgr,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfctr,
   /* .name        = */ "mfctr",
   /* .description =    "Move from count register to gpr", */
   /* .opcode      = */ 0x7C0902A6,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_UsesCtr |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mffgpr,
   /* .name        = */ "mffgpr",
   /* .description =    "Move Floating-Point From GPR", */
   /* .opcode      = */ 0x7C0004BE,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mffs,
   /* .name        = */ "mffs",
   /* .description =    "Move from FPSCR to gpr", */
   /* .opcode      = */ 0xFC00048E,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mflr,
   /* .name        = */ "mflr",
   /* .description =    "Move from link register to gpr", */
   /* .opcode      = */ 0x7C0802A6,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfmsr,
   /* .name        = */ "mfmsr",
   /* .description =    "Move from machine state register", */
   /* .opcode      = */ 0x7C0000A6,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfspr,
   /* .name        = */ "mfspr",
   /* .description =    "Move from special purpose register to gpr", */
   /* .opcode      = */ 0x7C0002A6,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mftexasr,
   /* .name        = */ "mftexasr",
   /* .description =    "Move from transaction exception and summary register to gpr", */
   /* .opcode      = */ 0x7C0222A6,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mftexasru,
   /* .name        = */ "mftexasru",
   /* .description =    "Move from upper 32 bits of transaction exception and summary register to gpr", */
   /* .opcode      = */ 0x7C0322A6,
   /* .format      = */ FORMAT_RT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mftgpr,
   /* .name        = */ "mftgpr",
   /* .description =    "Move Floating-Point To GPR", */
   /* .opcode      = */ 0x7C0005BE,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_SingleFP |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mfvsrld, */
   /* .name        =    "mfvsrld", */
   /* .description =    "Move from VSR lower Dword", */
   /* .opcode      =    0x7C000266, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::modud,
   /* .name        = */ "modud",
   /* .description =    "Modulo unsigned DWord", */
   /* .opcode      = */ 0x7C000212,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::modsd,
   /* .name        = */ "modsd",
   /* .description =    "Modulo signed DWord", */
   /* .opcode      = */ 0x7C000612,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::moduw,
   /* .name        = */ "moduw",
   /* .description =    "Modulo unsigned word", */
   /* .opcode      = */ 0x7C000216,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::modsw,
   /* .name        = */ "modsw",
   /* .description =    "Modulo signed word", */
   /* .opcode      = */ 0x7C000616,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mr,
   /* .name        = */ "ori",
   /* .description =    "Register copy", */
   /* .opcode      = */ 0x60000000,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_IsRegCopy |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::msgsync, */
   /* .name        =    "msgsync", */
   /* .description =    "Message synchronize", */
   /* .opcode      =    0x7C0006EC, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsSync, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtocrf,
   /* .name        = */ "mtocrf",
   /* .description =    "Move to one condition register field from gpr", */
   /* .opcode      = */ 0x7C100120,
   /* .format      = */ FORMAT_RS_FXM1,
   /* .minimumALS  = */ TR_Processor::TR_PPCgp,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtcr,
   /* .name        = */ "mtcr",
   /* .description =    "Move to condition register from gpr", */
   /* .opcode      = */ 0x7C0FF120,
   /* .format      = */ FORMAT_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtctr,
   /* .name        = */ "mtctr",
   /* .description =    "Move to count register from gpr", */
   /* .opcode      = */ 0x7C0903A6,
   /* .format      = */ FORMAT_RS,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SetsCtr |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsb0,
   /* .name        = */ "mtfsb0",
   /* .description =    "Move to FPSCR bit 0", */
   /* .opcode      = */ 0xFC00008C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsb1,
   /* .name        = */ "mtfsb1",
   /* .description =    "Move to FPSCR bit 1", */
   /* .opcode      = */ 0xFC00004C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfl,
   /* .name        = */ "mtfsfl",
   /* .description =    "Move to FPSCR fields L=1", */
   /* .opcode      = */ 0xFE00058E,
   /* .format      = */ FORMAT_MTFSF,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfl_r,
   /* .name        = */ "mtfsfl.",
   /* .description =    "Move to FPSCR fields L=1 Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfl].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfl].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfl].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfl].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsf,
   /* .name        = */ "mtfsf",
   /* .description =    "Move to FPSCR fields L=0/W=0", */
   /* .opcode      = */ 0xFC00058E,
   /* .format      = */ FORMAT_MTFSF,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsf_r,
   /* .name        = */ "mtfsf.",
   /* .description =    "Move to FPSCR fields L=0/W=0 Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsf].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsf].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsf].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsf].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfw,
   /* .name        = */ "mtfsfw",
   /* .description =    "Move to FPSCR fields L=0/W=1", */
   /* .opcode      = */ 0xFC01058E,
   /* .format      = */ FORMAT_MTFSF,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfw_r,
   /* .name        = */ "mtfsfw.",
   /* .description =    "Move to FPSCR fields L=0/W=1 Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfi,
   /* .name        = */ "mtfsfi",
   /* .description =    "Move to FPSCR field immediate", */
   /* .opcode      = */ 0xFC00010C,
   /* .format      = */ FORMAT_MTFSFI,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsFPSCR |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtfsfi_r,
   /* .name        = */ "mtfsfi.",
   /* .description =    "Move to FPSCR field immediate Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfi].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfi].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfi].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mtfsfi].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtlr,
   /* .name        = */ "mtlr",
   /* .description =    "Move to link register from gpr", */
   /* .opcode      = */ 0x7C0803A6,
   /* .format      = */ FORMAT_RS,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtmsr,
   /* .name        = */ "mtmsr",
   /* .description =    "Move to machine state register", */
   /* .opcode      = */ 0x7C000124,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtspr,
   /* .name        = */ "mtspr",
   /* .description =    "Move to special purpose register", */
   /* .opcode      = */ 0x7C0003A6,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfvsrd,
   /* .name        = */ "mfvsrd",
   /* .description =    "Move From VSR Doubleword", */
   /* .opcode      = */ 0x7C000066,
   /* .format      = */ FORMAT_RA_XS,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mfvsrwz,
   /* .name        = */ "mfvsrwz",
   /* .description =    "Move From VSR Word and Zero", */
   /* .opcode      = */ 0x7C0000E6,
   /* .format      = */ FORMAT_RA_XS,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtvsrd,
   /* .name        = */ "mtvsrd",
   /* .description =    "Move To VSR Doubleword", */
   /* .opcode      = */ 0x7C000166,
   /* .format      = */ FORMAT_XT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtvsrwz,
   /* .name        = */ "mtvsrwz",
   /* .description =    "Move To VSR Word and Zero", */
   /* .opcode      = */ 0x7C0001E6,
   /* .format      = */ FORMAT_XT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mtvsrwa,
   /* .name        = */ "mtvsrwa",
   /* .description =    "Move To VSR Word and Zero Sign-Extend", */
   /* .opcode      = */ 0x7C0001A6,
   /* .format      = */ FORMAT_XT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mtvsrdd, */
   /* .name        =    "mtvsrdd", */
   /* .description =    "Move to VSR double Dword", */
   /* .opcode      =    0x7C000366, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mtvsrws, */
   /* .name        =    "mtvsrws", */
   /* .description =    "Move to VSR word & wplat", */
   /* .opcode      =    0x7C000326, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhd,
   /* .name        = */ "mulhd",
   /* .description =    "Multiply high dword", */
   /* .opcode      = */ 0x7C000092,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhd_r,
   /* .name        = */ "mulhd.",
   /* .description =    "Multiply high dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhdu,
   /* .name        = */ "mulhdu",
   /* .description =    "Multiply high dword unsigned", */
   /* .opcode      = */ 0x7C000012,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhdu_r,
   /* .name        = */ "mulhdu.",
   /* .description =    "Multiply high dword unsigned Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhdu].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhdu].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhdu].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhdu].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhw,
   /* .name        = */ "mulhw",
   /* .description =    "Multiply high word", */
   /* .opcode      = */ 0x7C000096,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhw_r,
   /* .name        = */ "mulhw.",
   /* .description =    "Multiply high word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhwu,
   /* .name        = */ "mulhwu",
   /* .description =    "Multiply high word unsigned", */
   /* .opcode      = */ 0x7C000016,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulhwu_r,
   /* .name        = */ "mulhwu.",
   /* .description =    "Multiply high word unsigned Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhwu].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhwu].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhwu].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulhwu].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulld,
   /* .name        = */ "mulld",
   /* .description =    "Multiply low dword", */
   /* .opcode      = */ 0x7C0001D2,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulld_r,
   /* .name        = */ "mulld.",
   /* .description =    "Multiply low dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulld].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulld].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulld].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulld].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulldo,
   /* .name        = */ "mulldo",
   /* .description =    "Multiply low dword setting overflow", */
   /* .opcode      = */ 0x7C0005D2,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulldo_r,
   /* .name        = */ "mulldo.",
   /* .description =    "Multiply low dword setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulldo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulldo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulldo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mulldo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mulli,
   /* .name        = */ "mulli",
   /* .description =    "Multiply low immediate", */
   /* .opcode      = */ 0x1C000000,
   /* .format      = */ FORMAT_RT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mullw,
   /* .name        = */ "mullw",
   /* .description =    "Multiply low word", */
   /* .opcode      = */ 0x7C0001D6,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mullw_r,
   /* .name        = */ "mullw.",
   /* .description =    "Multiply low word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mullwo,
   /* .name        = */ "mullwo",
   /* .description =    "Multiply low word setting overflow", */
   /* .opcode      = */ 0x7C0005D6,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::mullwo_r,
   /* .name        = */ "mullwo.",
   /* .description =    "Multiply low word setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullwo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullwo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullwo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::mullwo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nand,
   /* .name        = */ "nand",
   /* .description =    "NAND", */
   /* .opcode      = */ 0x7C0003B8,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nand_r,
   /* .name        = */ "nand.",
   /* .description =    "NAND Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nand].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nand].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nand].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nand].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::neg,
   /* .name        = */ "neg",
   /* .description =    "Negate", */
   /* .opcode      = */ 0x7C0000D0,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::neg_r,
   /* .name        = */ "neg.",
   /* .description =    "Negate Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::neg].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::neg].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::neg].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::neg].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nego,
   /* .name        = */ "nego",
   /* .description =    "Negate setting overflow", */
   /* .opcode      = */ 0x7C0004D0,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nego_r,
   /* .name        = */ "nego.",
   /* .description =    "Negate setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nego].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nego].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nego].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nego].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nor,
   /* .name        = */ "nor",
   /* .description =    "NOR", */
   /* .opcode      = */ 0x7C0000F8,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nor_r,
   /* .name        = */ "nor.",
   /* .description =    "NOR Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nor].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nor].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nor].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::nor].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::OR,
   /* .name        = */ "or",
   /* .description =    "OR", */
   /* .opcode      = */ 0x7C000378,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::or_r,
   /* .name        = */ "or.",
   /* .description =    "OR Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::OR].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::OR].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::OR].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::OR].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::orc,
   /* .name        = */ "orc",
   /* .description =    "OR with complement", */
   /* .opcode      = */ 0x7C000338,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::orc_r,
   /* .name        = */ "orc.",
   /* .description =    "OR with complement Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::orc].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::orc].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::orc].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::orc].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ori,
   /* .name        = */ "ori",
   /* .description =    "OR immediate", */
   /* .opcode      = */ 0x60000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::oris,
   /* .name        = */ "oris",
   /* .description =    "OR immediate shifted", */
   /* .opcode      = */ 0x64000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::popcntd,
   /* .name        = */ "popcntd",
   /* .description =    "Population count dword", */
   /* .opcode      = */ 0x7C0003F4,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::popcntw,
   /* .name        = */ "popcntw",
   /* .description =    "Population count word", */
   /* .opcode      = */ 0x7C0002F4,
   /* .format      = */ FORMAT_RA_RS,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rfi,
   /* .name        = */ "rfi",
   /* .description =    "Return from interrupt", */
   /* .opcode      = */ 0x4C000064,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldcl,
   /* .name        = */ "rldcl",
   /* .description =    "Rotate left dword then clear left", */
   /* .opcode      = */ 0x78000010,
   /* .format      = */ FORMAT_RLDCL,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldcl_r,
   /* .name        = */ "rldcl.",
   /* .description =    "Rotate left dword then clear left Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcl].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcl].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcl].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcl].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldcr,
   /* .name        = */ "rldcr",
   /* .description =    "Rotate left dword then clear right", */
   /* .opcode      = */ 0x78000012,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_UseMaskEnd |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldcr_r,
   /* .name        = */ "rldcr.",
   /* .description =    "Rotate left dword then clear right Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcr].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcr].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcr].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldcr].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldic,
   /* .name        = */ "rldic",
   /* .description =    "Rotate left dword immediate then clear", */
   /* .opcode      = */ 0x78000008,
   /* .format      = */ FORMAT_RLDIC,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldic_r,
   /* .name        = */ "rldic.",
   /* .description =    "Rotate left dword immediate then clear Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldic].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldic].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldic].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldic].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldicl,
   /* .name        = */ "rldicl",
   /* .description =    "Rotate left dword immediate then clear left", */
   /* .opcode      = */ 0x78000000,
   /* .format      = */ FORMAT_RLDICL,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldicl_r,
   /* .name        = */ "rldicl.",
   /* .description =    "Rotate left dword immediate then clear left Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicl].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicl].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicl].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicl].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldicr,
   /* .name        = */ "rldicr",
   /* .description =    "Rotate left dword immediate then clear right", */
   /* .opcode      = */ 0x78000004,
   /* .format      = */ FORMAT_RLDICR,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_UseMaskEnd |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldicr_r,
   /* .name        = */ "rldicr.",
   /* .description =    "Rotate left dword immediate then clear right Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicr].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicr].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicr].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldicr].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldimi,
   /* .name        = */ "rldimi",
   /* .description =    "Rotate left dword immediate then mask insert", */
   /* .opcode      = */ 0x7800000C,
   /* .format      = */ FORMAT_RLDIC,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rldimi_r,
   /* .name        = */ "rldimi.",
   /* .description =    "Rotate left dword immediate then mask insert Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldimi].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldimi].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldimi].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rldimi].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwimi,
   /* .name        = */ "rlwimi",
   /* .description =    "Rotate left word immediate then mask insert", */
   /* .opcode      = */ 0x50000000,
   /* .format      = */ FORMAT_RLWINM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwimi_r,
   /* .name        = */ "rlwimi.",
   /* .description =    "Rotate left word immediate then mask insert Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwimi].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwimi].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwimi].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwimi].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwinm,
   /* .name        = */ "rlwinm",
   /* .description =    "Rotate left word immediate then AND with mask", */
   /* .opcode      = */ 0x54000000,
   /* .format      = */ FORMAT_RLWINM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwinm_r,
   /* .name        = */ "rlwinm.",
   /* .description =    "Rotate left word immediate then AND with mask Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwinm].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwinm].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwinm].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwinm].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwnm,
   /* .name        = */ "rlwnm",
   /* .description =    "Rotate left word then AND with mask", */
   /* .opcode      = */ 0x5C000000,
   /* .format      = */ FORMAT_RLWNM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::rlwnm_r,
   /* .name        = */ "rlwnm.",
   /* .description =    "Rotate left word then AND with mask Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwnm].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwnm].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwnm].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::rlwnm].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::setb,
   /* .name        = */ "setb",
   /* .description =    "Set Boolean", */
   /* .opcode      = */ 0x7C000100,
   /* .format      = */ FORMAT_RT_BFA,
   /* .minimumALS  = */ TR_Processor::TR_PPCp9,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::slbsync, */
   /* .name        =    "slbsync", */
   /* .description =    "SLB Synchronize", */
   /* .opcode      =    0x7C0002A4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsSync, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::sld,
   /* .name        = */ "sld",
   /* .description =    "Shift left dword", */
   /* .opcode      = */ 0x7C000036,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sld_r,
   /* .name        = */ "sld.",
   /* .description =    "Shift left dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sld].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sld].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sld].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sld].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::slw,
   /* .name        = */ "slw",
   /* .description =    "Shift left word", */
   /* .opcode      = */ 0x7C000030,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::slw_r,
   /* .name        = */ "slw.",
   /* .description =    "Shift left word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::slw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::slw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::slw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::slw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srad,
   /* .name        = */ "srad",
   /* .description =    "Shift right algebraic dword", */
   /* .opcode      = */ 0x7C000634,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srad_r,
   /* .name        = */ "srad.",
   /* .description =    "Shift right algebraic dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srad].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srad].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srad].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srad].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sradi,
   /* .name        = */ "sradi",
   /* .description =    "Shift right algebraic dword immediate", */
   /* .opcode      = */ 0x7C000674,
   /* .format      = */ FORMAT_RA_RS_SH6,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sradi_r,
   /* .name        = */ "sradi.",
   /* .description =    "Shift right algebraic dword immediate Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sradi].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sradi].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sradi].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sradi].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sraw,
   /* .name        = */ "sraw",
   /* .description =    "Shift right algebraic word", */
   /* .opcode      = */ 0x7C000630,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sraw_r,
   /* .name        = */ "sraw.",
   /* .description =    "Shift right algebraic word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sraw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sraw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sraw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::sraw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srawi,
   /* .name        = */ "srawi",
   /* .description =    "Shift right algebraic word immediate", */
   /* .opcode      = */ 0x7C000670,
   /* .format      = */ FORMAT_RA_RS_SH5,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srawi_r,
   /* .name        = */ "srawi.",
   /* .description =    "Shift right algebraic word immediate Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srawi].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srawi].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srawi].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srawi].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srd,
   /* .name        = */ "srd",
   /* .description =    "Shift right dword", */
   /* .opcode      = */ 0x7C000436,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_DWord |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srd_r,
   /* .name        = */ "srd.",
   /* .description =    "Shift right dword Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srd].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srd].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srd].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srw,
   /* .name        = */ "srw",
   /* .description =    "Shift right word", */
   /* .opcode      = */ 0x7C000430,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::srw_r,
   /* .name        = */ "srw.",
   /* .description =    "Shift right word Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srw].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::srw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stb,
   /* .name        = */ "stb",
   /* .description =    "Store byte", */
   /* .opcode      = */ 0x98000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stbu,
   /* .name        = */ "stbu",
   /* .description =    "Store byte with update", */
   /* .opcode      = */ 0x9C000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stbux,
   /* .name        = */ "stbux",
   /* .description =    "Store byte with update indexed", */
   /* .opcode      = */ 0x7C0001EE,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stbx,
   /* .name        = */ "stbx",
   /* .description =    "Store byte indexed", */
   /* .opcode      = */ 0x7C0001AE,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::std,
   /* .name        = */ "std",
   /* .description =    "Store dword", */
   /* .opcode      = */ 0xF8000000,
   /* .format      = */ FORMAT_RS_DS_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_OffsetRequiresWordAlignment,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stdcx_r,
   /* .name        = */ "stdcx.",
   /* .description =    "Store word conditional indexed", */
   /* .opcode      = */ 0x7C0001AD,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stdu,
   /* .name        = */ "stdu",
   /* .description =    "Store dword with update", */
   /* .opcode      = */ 0xF8000001,
   /* .format      = */ FORMAT_RS_DS_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore |
                        PPCOpProp_OffsetRequiresWordAlignment,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stdux,
   /* .name        = */ "stdux",
   /* .description =    "Store dword with update indexed", */
   /* .opcode      = */ 0x7C00016A,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stdx,
   /* .name        = */ "stdx",
   /* .description =    "Store dword indexed", */
   /* .opcode      = */ 0x7C00012A,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfd,
   /* .name        = */ "stfd",
   /* .description =    "Store float double", */
   /* .opcode      = */ 0xD8000000,
   /* .format      = */ FORMAT_FRS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfdp,
   /* .name        = */ "stfdp",
   /* .description =    "Store float double pair", */
   /* .opcode      = */ 0xF4000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfdu,
   /* .name        = */ "stfdu",
   /* .description =    "Store float double with update", */
   /* .opcode      = */ 0xDC000000,
   /* .format      = */ FORMAT_FRS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfdux,
   /* .name        = */ "stfdux",
   /* .description =    "Store float double with update indexed", */
   /* .opcode      = */ 0x7C0005EE,
   /* .format      = */ FORMAT_FRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfdx,
   /* .name        = */ "stfdx",
   /* .description =    "Store float double indexed", */
   /* .opcode      = */ 0x7C0005AE,
   /* .format      = */ FORMAT_FRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfdpx,
   /* .name        = */ "stfdpx",
   /* .description =    "Store float double pair indexed", */
   /* .opcode      = */ 0x7C00072E,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfiwx,
   /* .name        = */ "stfiwx",
   /* .description =    "Store float as integer word indexed", */
   /* .opcode      = */ 0x7C0007AE,
   /* .format      = */ FORMAT_FRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfs,
   /* .name        = */ "stfs",
   /* .description =    "Store float single", */
   /* .opcode      = */ 0xD0000000,
   /* .format      = */ FORMAT_FRS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfsu,
   /* .name        = */ "stfsu",
   /* .description =    "Store float single with update", */
   /* .opcode      = */ 0xD4000000,
   /* .format      = */ FORMAT_FRS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_SingleFP |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfsux,
   /* .name        = */ "stfsux",
   /* .description =    "Store float single with update indexed", */
   /* .opcode      = */ 0x7C00056E,
   /* .format      = */ FORMAT_FRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_SingleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stfsx,
   /* .name        = */ "stfsx",
   /* .description =    "Store float single indexed", */
   /* .opcode      = */ 0x7C00052E,
   /* .format      = */ FORMAT_FRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SingleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sth,
   /* .name        = */ "sth",
   /* .description =    "Store half word", */
   /* .opcode      = */ 0xB0000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sthbrx,
   /* .name        = */ "sthbrx",
   /* .description =    "Store half word byte reversed indexed", */
   /* .opcode      = */ 0x7C00072C,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sthu,
   /* .name        = */ "sthu",
   /* .description =    "Store half word with update", */
   /* .opcode      = */ 0xB4000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sthux,
   /* .name        = */ "sthux",
   /* .description =    "Store half word with update indexed", */
   /* .opcode      = */ 0x7C00036E,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sthx,
   /* .name        = */ "sthx",
   /* .description =    "Store half word indexed", */
   /* .opcode      = */ 0x7C00032E,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stmw,
   /* .name        = */ "stmw",
   /* .description =    "Store multiple word", */
   /* .opcode      = */ 0xBC000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stop, */
   /* .name        =    "stop", */
   /* .description =    "The thread is placed into power-saving mode and execution is stopped", */
   /* .opcode      =    0x4C0002E4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_None, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::stswi,
   /* .name        = */ "stswi",
   /* .description =    "Store string word immediate", */
   /* .opcode      = */ 0x7C0005AA,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stswx,
   /* .name        = */ "stswx",
   /* .description =    "Store string word indexed", */
   /* .opcode      = */ 0x7C00052A,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stw,
   /* .name        = */ "stw",
   /* .description =    "Store word", */
   /* .opcode      = */ 0x90000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stwbrx,
   /* .name        = */ "stwbrx",
   /* .description =    "Store word byte reverse indexed", */
   /* .opcode      = */ 0x7C00052C,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stdbrx,
   /* .name        = */ "stdbrx",
   /* .description =    "Store doubleword byte reverse indexed", */
   /* .opcode      = */ 0x7C000528,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stwcx_r,
   /* .name        = */ "stwcx.",
   /* .description =    "Store word conditional indexed", */
   /* .opcode      = */ 0x7C00012D,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stwu,
   /* .name        = */ "stwu",
   /* .description =    "Store word with update", */
   /* .opcode      = */ 0x94000000,
   /* .format      = */ FORMAT_RS_D16_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stwux,
   /* .name        = */ "stwux",
   /* .description =    "Store word with update indexed", */
   /* .opcode      = */ 0x7C00016E,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_UpdateForm |
                        PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stwx,
   /* .name        = */ "stwx",
   /* .description =    "Store word indexed", */
   /* .opcode      = */ 0x7C00012E,
   /* .format      = */ FORMAT_RS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subf,
   /* .name        = */ "subf",
   /* .description =    "Subtract from", */
   /* .opcode      = */ 0x7C000050,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCpwr630,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subf_r,
   /* .name        = */ "subf.",
   /* .description =    "Subtract from Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subf].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subf].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subf].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subf].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfc,
   /* .name        = */ "subfc",
   /* .description =    "Subtract from carrying", */
   /* .opcode      = */ 0x7C000010,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfc_r,
   /* .name        = */ "subfc.",
   /* .description =    "Subtract from carrying Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfc].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfc].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfc].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfc].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfco,
   /* .name        = */ "subfco",
   /* .description =    "Subtract from carrying setting overflow", */
   /* .opcode      = */ 0x7C000410,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfco_r,
   /* .name        = */ "subfco.",
   /* .description =    "Subtract from carrying setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfco].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfco].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfco].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfco].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfe,
   /* .name        = */ "subfe",
   /* .description =    "Subtract from extended", */
   /* .opcode      = */ 0x7C000110,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfe_r,
   /* .name        = */ "subfe.",
   /* .description =    "Subtract from extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfe].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfe].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfe].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfe].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfeo,
   /* .name        = */ "subfeo",
   /* .description =    "Subtract from extended setting overflow", */
   /* .opcode      = */ 0x7C000510,
   /* .format      = */ FORMAT_RT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfeo_r,
   /* .name        = */ "subfeo.",
   /* .description =    "Subtract from extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfic,
   /* .name        = */ "subfic",
   /* .description =    "Subtract from immediate carrying", */
   /* .opcode      = */ 0x20000000,
   /* .format      = */ FORMAT_RT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfme,
   /* .name        = */ "subfme",
   /* .description =    "Subtract from minus one extended", */
   /* .opcode      = */ 0x7C0001D0,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfme_r,
   /* .name        = */ "subfme.",
   /* .description =    "Subtract from minus one extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfme].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfme].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfme].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfme].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfmeo,
   /* .name        = */ "subfmeo",
   /* .description =    "Subtract from minus one extended setting overflow", */
   /* .opcode      = */ 0x7C0005D0,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfmeo_r,
   /* .name        = */ "subfmeo.",
   /* .description =    "Subtract from minus one extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfmeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfmeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfmeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfmeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfze,
   /* .name        = */ "subfze",
   /* .description =    "Subtract from zero extended", */
   /* .opcode      = */ 0x7C000190,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfze_r,
   /* .name        = */ "subfze.",
   /* .description =    "Subtract from zero extended Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfze].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfze].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfze].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfze].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfzeo,
   /* .name        = */ "subfzeo",
   /* .description =    "Subtract from zero extended setting overflow", */
   /* .opcode      = */ 0x7C000590,
   /* .format      = */ FORMAT_RT_RA,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_SetsOverflowFlag |
                        PPCOpProp_ReadsCarryFlag |
                        PPCOpProp_SetsCarryFlag |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::subfzeo_r,
   /* .name        = */ "subfzeo.",
   /* .description =    "Subtract from zero extended setting overflow Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfzeo].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfzeo].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfzeo].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::subfzeo].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::sync,
   /* .name        = */ "sync",
   /* .description =    "Synchronize", */
   /* .opcode      = */ 0x7C0004AC,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_IsSync,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabort_r,
   /* .name        = */ "tabort.",
   /* .description =    "Transactional Memory abort", */
   /* .opcode      = */ 0x7C00071D,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdeq_r,
   /* .name        = */ "tabortdeq.",
   /* .description =    "Transactional Memory abort dword if equal", */
   /* .opcode      = */ 0x7C80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdeqi_r,
   /* .name        = */ "tabortdeqi.",
   /* .description =    "Transactional Memory abort dword if equal to immediate value", */
   /* .opcode      = */ 0x7C8006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdge_r,
   /* .name        = */ "tabortdge.",
   /* .description =    "Transactional Memory abort dword if greater than or equal to", */
   /* .opcode      = */ 0x7D80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdgei_r,
   /* .name        = */ "tabortdgei.",
   /* .description =    "Transactional Memory abort dword if greater than or equal to immediate", */
   /* .opcode      = */ 0x7D8006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdgt_r,
   /* .name        = */ "tabortdgt.",
   /* .description =    "Transactional Memory abort dword if greater than", */
   /* .opcode      = */ 0x7D00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdgti_r,
   /* .name        = */ "tabortdgti.",
   /* .description =    "Transactional Memory abort dword if greater than immediate", */
   /* .opcode      = */ 0x7D0006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdle_r,
   /* .name        = */ "tabortdle.",
   /* .description =    "Transactional Memory abort dword if less than or equal to", */
   /* .opcode      = */ 0x7E80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlei_r,
   /* .name        = */ "tabortdlei.",
   /* .description =    "Transactional Memory abort dword if less than or equal to immediate", */
   /* .opcode      = */ 0x7E8006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlge_r,
   /* .name        = */ "tabortdlge.",
   /* .description =    "Transactional Memory abort dword if logically greater than or equal to", */
   /* .opcode      = */ 0x7CA0065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlgei_r,
   /* .name        = */ "tabortdlgei.",
   /* .description =    "Transactional Memory abort dword if logically greater than or equal to immediate", */
   /* .opcode      = */ 0x7CA006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlgt_r,
   /* .name        = */ "tabortdlgt.",
   /* .description =    "Transactional Memory abort dword if logically greater than", */
   /* .opcode      = */ 0x7C20065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlgti_r,
   /* .name        = */ "tabortdlgti.",
   /* .description =    "Transactional Memory abort dword if logically greater than immediate", */
   /* .opcode      = */ 0x7C2006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlle_r,
   /* .name        = */ "tabortdlle.",
   /* .description =    "Transactional Memory abort dword if logically less than or equal to", */
   /* .opcode      = */ 0x7CC0065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdllei_r,
   /* .name        = */ "tabortdllei.",
   /* .description =    "Transactional Memory abort dword if logically less than or equal to immediate", */
   /* .opcode      = */ 0x7CC006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdllt_r,
   /* .name        = */ "tabortdllt.",
   /* .description =    "Transactional Memory abort dword if logically less than", */
   /* .opcode      = */ 0x7C40065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdllti_r,
   /* .name        = */ "tabortdllti.",
   /* .description =    "Transactional Memory abort dword if logically less than immediate", */
   /* .opcode      = */ 0x7C4006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlt_r,
   /* .name        = */ "tabortdlt.",
   /* .description =    "Transactional Memory abort dword if less than", */
   /* .opcode      = */ 0x7E00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdlti_r,
   /* .name        = */ "tabortdlti.",
   /* .description =    "Transactional Memory abort dword if less than immediate", */
   /* .opcode      = */ 0x7E0006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdneq_r,
   /* .name        = */ "tabortdneq.",
   /* .description =    "Transactional Memory abort dword if not equal to", */
   /* .opcode      = */ 0x7F00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortdneqi_r,
   /* .name        = */ "tabortdneqi.",
   /* .description =    "Transactional Memory abort dword if not equal to immediate", */
   /* .opcode      = */ 0x7F0006DD,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortweq_r,
   /* .name        = */ "tabortweq.",
   /* .description =    "Transactional Memory abort if equal", */
   /* .opcode      = */ 0x7C80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortweqi_r,
   /* .name        = */ "tabortweqi.",
   /* .description =    "Transactional Memory abort if equal to immediate value", */
   /* .opcode      = */ 0x7C80069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwge_r,
   /* .name        = */ "tabortwge.",
   /* .description =    "Transactional Memory abort if greater than or equal to", */
   /* .opcode      = */ 0x7D80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwgei_r,
   /* .name        = */ "tabortwgei.",
   /* .description =    "Transactional Memory abort if greater than or equal to immediate", */
   /* .opcode      = */ 0x7D80069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwgt_r,
   /* .name        = */ "tabortwgt.",
   /* .description =    "Transactional Memory abort if greater than", */
   /* .opcode      = */ 0x7D00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwgti_r,
   /* .name        = */ "tabortwgti.",
   /* .description =    "Transactional Memory abort if greater than immediate", */
   /* .opcode      = */ 0x7D00069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwle_r,
   /* .name        = */ "tabortwle.",
   /* .description =    "Transactional Memory abort if less than or equal to", */
   /* .opcode      = */ 0x7E80065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlei_r,
   /* .name        = */ "tabortwlei.",
   /* .description =    "Transactional Memory abort if less than or equal to immediate", */
   /* .opcode      = */ 0x7E80069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlge_r,
   /* .name        = */ "tabortwlge.",
   /* .description =    "Transactional Memory abort if logically greater than or equal to", */
   /* .opcode      = */ 0x7CA0065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlgei_r,
   /* .name        = */ "tabortwlgei.",
   /* .description =    "Transactional Memory abort if logically greater than or equal to immediate", */
   /* .opcode      = */ 0x7CA0069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlgt_r,
   /* .name        = */ "tabortwlgt.",
   /* .description =    "Transactional Memory abort if logically greater than", */
   /* .opcode      = */ 0x7C20065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlgti_r,
   /* .name        = */ "tabortwlgti.",
   /* .description =    "Transactional Memory abort if logically greater than immediate", */
   /* .opcode      = */ 0x7C20069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlle_r,
   /* .name        = */ "tabortwlle.",
   /* .description =    "Transactional Memory abort if logically less than or equal to", */
   /* .opcode      = */ 0x7CC0065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwllei_r,
   /* .name        = */ "tabortwllei.",
   /* .description =    "Transactional Memory abort if logically less than or equal to immediate", */
   /* .opcode      = */ 0x7CC0069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwllt_r,
   /* .name        = */ "tabortwllt.",
   /* .description =    "Transactional Memory abort if logically less than", */
   /* .opcode      = */ 0x7C40065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwllti_r,
   /* .name        = */ "tabortwllti.",
   /* .description =    "Transactional Memory abort if logically less than immediate", */
   /* .opcode      = */ 0x7C40069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlt_r,
   /* .name        = */ "tabortwlt.",
   /* .description =    "Transactional Memory abort if less than", */
   /* .opcode      = */ 0x7E00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwlti_r,
   /* .name        = */ "tabortwlti.",
   /* .description =    "Transactional Memory abort if less than immediate", */
   /* .opcode      = */ 0x7E00069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwneq_r,
   /* .name        = */ "tabortwneq.",
   /* .description =    "Transactional Memory abort if not equal to", */
   /* .opcode      = */ 0x7F00065D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tabortwneqi_r,
   /* .name        = */ "tabortwneqi.",
   /* .description =    "Transactional Memory abort if not equal to immediate", */
   /* .opcode      = */ 0x7F00069D,
   /* .format      = */ FORMAT_RA_SI5,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_TMAbort |
                        PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tbegin_r,
   /* .name        = */ "tbegin.",
   /* .description =    "Begin transaction", */
   /* .opcode      = */ 0x7C00051D,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tbeginro_r,
   /* .name        = */ "tbeginro.",
   /* .description =    "Begin roll-back only transaction", */
   /* .opcode      = */ 0x7C20051D,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tend_r,
   /* .name        = */ "tend.",
   /* .description =    "End transaction (supports nesting)", */
   /* .opcode      = */ 0x7C00055D,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tendall_r,
   /* .name        = */ "tendall.",
   /* .description =    "End transaction", */
   /* .opcode      = */ 0x7E00055D,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdeq,
   /* .name        = */ "tdeq",
   /* .description =    "Trap dword if equal", */
   /* .opcode      = */ 0x7C800088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdeqi,
   /* .name        = */ "tdeqi",
   /* .description =    "Trap dword if equal to immediate value", */
   /* .opcode      = */ 0x08800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdge,
   /* .name        = */ "tdge",
   /* .description =    "Trap dword if greater than or equal to", */
   /* .opcode      = */ 0x7D800088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdgei,
   /* .name        = */ "tdgei",
   /* .description =    "Trap dword if greater than or equal to immediate value", */
   /* .opcode      = */ 0x09800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdgt,
   /* .name        = */ "tdgt",
   /* .description =    "Trap dword if greater than", */
   /* .opcode      = */ 0x7D000088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdgti,
   /* .name        = */ "tdgti",
   /* .description =    "Trap dword if greater than immediate value", */
   /* .opcode      = */ 0x09000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdle,
   /* .name        = */ "tdle",
   /* .description =    "Trap dword if less than or equal to", */
   /* .opcode      = */ 0x7E800088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlei,
   /* .name        = */ "tdlei",
   /* .description =    "Trap dword if less than or equal to immediate value", */
   /* .opcode      = */ 0x0A800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlge,
   /* .name        = */ "tdlge",
   /* .description =    "Trap dword if logically greater than or equal to", */
   /* .opcode      = */ 0x7CA00088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlgei,
   /* .name        = */ "tdlgei",
   /* .description =    "Trap dword if logically greater than or equal to immediate value", */
   /* .opcode      = */ 0x08A00000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlgt,
   /* .name        = */ "tdlgt",
   /* .description =    "Trap dword if logically greater than", */
   /* .opcode      = */ 0x7C200088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlgti,
   /* .name        = */ "tdlgti",
   /* .description =    "Trap dword if logically greater than immediate value", */
   /* .opcode      = */ 0x08200000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlle,
   /* .name        = */ "tdlle",
   /* .description =    "Trap dword if logically less than or equal to", */
   /* .opcode      = */ 0x7CC00088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdllei,
   /* .name        = */ "tdllei",
   /* .description =    "Trap dword if logically less than or equal to immediate value", */
   /* .opcode      = */ 0x08C00000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdllt,
   /* .name        = */ "tdllt",
   /* .description =    "Trap dword if logically less than", */
   /* .opcode      = */ 0x7C400088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdllti,
   /* .name        = */ "tdllti",
   /* .description =    "Trap dword if logically less than immediate value", */
   /* .opcode      = */ 0x08400000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlt,
   /* .name        = */ "tdlt",
   /* .description =    "Trap dword if less than", */
   /* .opcode      = */ 0x7E000088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdlti,
   /* .name        = */ "tdlti",
   /* .description =    "Trap dword if less than immediate value", */
   /* .opcode      = */ 0x0A000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdneq,
   /* .name        = */ "tdneq",
   /* .description =    "Trap dword if not equal", */
   /* .opcode      = */ 0x7F000088,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tdneqi,
   /* .name        = */ "tdneqi",
   /* .description =    "Trap dword if not equal to immediate value", */
   /* .opcode      = */ 0x0B000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap |
                        PPCOpProp_DWord,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::trap,
   /* .name        = */ "trap",
   /* .description =    "Unconditional trap", */
   /* .opcode      = */ 0x7FE00008,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tweq,
   /* .name        = */ "tweq",
   /* .description =    "Trap word if equal", */
   /* .opcode      = */ 0x7C800008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::tweqi,
   /* .name        = */ "tweqi",
   /* .description =    "Trap word if equal to immediate value", */
   /* .opcode      = */ 0x0C800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twge,
   /* .name        = */ "twge",
   /* .description =    "Trap word if greater than or equal to", */
   /* .opcode      = */ 0x7D800008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twgei,
   /* .name        = */ "twgei",
   /* .description =    "Trap word if greater than or equal to immediate value", */
   /* .opcode      = */ 0x0D800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twgt,
   /* .name        = */ "twgt",
   /* .description =    "Trap word if greater than", */
   /* .opcode      = */ 0x7D000008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twgti,
   /* .name        = */ "twgti",
   /* .description =    "Trap word if greater than immediate value", */
   /* .opcode      = */ 0x0D000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twle,
   /* .name        = */ "twle",
   /* .description =    "Trap word if less than or equal to", */
   /* .opcode      = */ 0x7E800008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlei,
   /* .name        = */ "twlei",
   /* .description =    "Trap word if less than or equal to immediate value", */
   /* .opcode      = */ 0x0E800000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlge,
   /* .name        = */ "twlge",
   /* .description =    "Trap word if logically greater than or equal to", */
   /* .opcode      = */ 0x7CA00008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlgei,
   /* .name        = */ "twlgei",
   /* .description =    "Trap word if logically greater than or equal to immediate value", */
   /* .opcode      = */ 0x0CA00000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlgt,
   /* .name        = */ "twlgt",
   /* .description =    "Trap word if logically greater than", */
   /* .opcode      = */ 0x7C200008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlgti,
   /* .name        = */ "twlgti",
   /* .description =    "Trap word if logically greater than immediate value", */
   /* .opcode      = */ 0x0C200000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlle,
   /* .name        = */ "twlle",
   /* .description =    "Trap word if logically less than or equal to", */
   /* .opcode      = */ 0x7CC00008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twllei,
   /* .name        = */ "twllei",
   /* .description =    "Trap word if logically less than or equal to immediate value", */
   /* .opcode      = */ 0x0CC00000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twllt,
   /* .name        = */ "twllt",
   /* .description =    "Trap word if logically less than", */
   /* .opcode      = */ 0x7C400008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twllti,
   /* .name        = */ "twllti",
   /* .description =    "Trap word if logically less than immediate value", */
   /* .opcode      = */ 0x0C400000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlt,
   /* .name        = */ "twlt",
   /* .description =    "Trap word if less than", */
   /* .opcode      = */ 0x7E000008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twlti,
   /* .name        = */ "twlti",
   /* .description =    "Trap word if less than immediate value", */
   /* .opcode      = */ 0x0E000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twneq,
   /* .name        = */ "twneq",
   /* .description =    "Trap word if not equal", */
   /* .opcode      = */ 0x7F000008,
   /* .format      = */ FORMAT_RA_RB,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::twneqi,
   /* .name        = */ "twneqi",
   /* .description =    "Trap word if not equal to immediate value", */
   /* .opcode      = */ 0x0F000000,
   /* .format      = */ FORMAT_RA_SI16,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_Trap,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::XOR,
   /* .name        = */ "xor",
   /* .description =    "XOR", */
   /* .opcode      = */ 0x7C000278,
   /* .format      = */ FORMAT_RA_RS_RB,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xor_r,
   /* .name        = */ "xor.",
   /* .description =    "XOR Rc=1", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::XOR].opcode + 1,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::XOR].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::XOR].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::XOR].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xor3, */
   /* .name        =    "xor3", */
   /* .description =    "XOR 3-way", */
   /* .opcode      =    0x10000036, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xori,
   /* .name        = */ "xori",
   /* .description =    "XOR immediate", */
   /* .opcode      = */ 0x68000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xoris,
   /* .name        = */ "xoris",
   /* .description =    "XOR immediate shifted", */
   /* .opcode      = */ 0x6C000000,
   /* .format      = */ FORMAT_RA_RS_UI16,
   /* .minimumALS  = */ TR_Processor::TR_PPCrios1,
   /* .properties  = */ PPCOpProp_AltFormatx |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::nop,
   /* .name        = */ "nop",
   /* .description =    "NoOp (ori)", */
   /* .opcode      = */ 0x60000000,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::genop,
   /* .name        = */ "genop",
   /* .description =    "Group Ending NoOp (ori)", */
   /* .opcode      = */ 0x60000000,
   /* .format      = */ FORMAT_DIRECT,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fence,
   /* .name        = */ "fence",
   /* .description =    "Fence", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_NONE,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::shdfence,
   /* .name        = */ "shdfence",
   /* .description =    "Scheduling Fence", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ret,
   /* .name        = */ "ret",
   /* .description =    "Return", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_NONE,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::rxor, */
   /* .name        =    "rxor", */
   /* .description =    "Rotate & XOR", */
   /* .opcode      =    0x7C0002F8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_AltFormatx | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::wrtbar,
   /* .name        = */ "wrtbar",
   /* .description =    "Write barrier directive", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::proc,
   /* .name        = */ "proc",
   /* .description =    "Entry to the method", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_NONE,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::dd,
   /* .name        = */ ".long",
   /* .description =    "define word", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_DD,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::assocreg,
   /* .name        = */ "assocreg",
   /* .description =    "Associate real registers with Virtual registers.", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_NONE,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vgdnop,
   /* .name        = */ "vgdnop",
   /* .description =    "Virtual Guard NOP instruction", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::probenop,
   /* .name        = */ "nop",
   /* .description =    "Probe NOP (for RI)", */
   /* .opcode      = */ 0x7C000038,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::iflong,
   /* .name        = */ "iflong",
   /* .description =    "compare and branch long", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_BranchOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::setblong,
   /* .name        = */ "setblong",
   /* .description =    "compare long and set boolean", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::setbool,
   /* .name        = */ "setbool",
   /* .description =    "compare and set boolean (int or simple float)", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::setbflt,
   /* .name        = */ "setbflt",
   /* .description =    "compare float and set boolean (complex float)", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lcmp,
   /* .name        = */ "lcmp",
   /* .description =    "compare long and set result -1,0,1", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::flcmpl,
   /* .name        = */ "flcmpl",
   /* .description =    "compare float and set result -1(less,unordered),0(equal),1(greater)", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::flcmpg,
   /* .name        = */ "flcmpg",
   /* .description =    "compare float and set result -1(less),0(equal),1(greater,unordered)", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::idiv,
   /* .name        = */ "idiv",
   /* .description =    "integer divide", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::ldiv,
   /* .name        = */ "ldiv",
   /* .description =    "long divide for 64 bit target", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::irem,
   /* .name        = */ "irem",
   /* .description =    "integer remainder", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lrem,
   /* .name        = */ "lrem",
   /* .description =    "long remainder for 64 bit target", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::d2i,
   /* .name        = */ "d2i",
   /* .description =    "converts from double to integer", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::d2l,
   /* .name        = */ "d2l",
   /* .description =    "converts from double to long", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::iselect,
   /* .name        = */ "iselect",
   /* .description =    "select evaluator", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdcpsgn_r, */
   /* .name        =    "bcdcpsgn.", */
   /* .description =    "Decimal copySign & record", */
   /* .opcode      =    0x10000341, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdcfn_r, */
   /* .name        =    "bcdcfn.", */
   /* .description =    "Decimal convert from national & record", */
   /* .opcode      =    0x10070581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdcfsq_r, */
   /* .name        =    "bcdcfsq.", */
   /* .description =    "Decimal convert from signed qword & record", */
   /* .opcode      =    0x10020581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdcfz_r, */
   /* .name        =    "bcdcfz.", */
   /* .description =    "Decimal convert from zoned & record", */
   /* .opcode      =    0x10060581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdctn_r, */
   /* .name        =    "bcdctn.", */
   /* .description =    "Decimal convert to national & record", */
   /* .opcode      =    0x10050581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdctsq_r, */
   /* .name        =    "bcdctsq.", */
   /* .description =    "Decimal convert to signed qword & record", */
   /* .opcode      =    0x10000581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdctz_r, */
   /* .name        =    "bcdctz.", */
   /* .description =    "Decimal convert to zoned & record", */
   /* .opcode      =    0x10040581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcds_r, */
   /* .name        =    "bcds.", */
   /* .description =    "Decimal shift & record", */
   /* .opcode      =    0x100004C1, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdsetsgn_r, */
   /* .name        =    "bcdsetsgn.", */
   /* .description =    "Decimal set Sign & record", */
   /* .opcode      =    0x101F0581, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdsr_r, */
   /* .name        =    "bcdsr.", */
   /* .description =    "Decimal shift & round & record", */
   /* .opcode      =    0x100005C1, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdtrunc_r, */
   /* .name        =    "bcdtrunc.", */
   /* .description =    "Decimal truncate & record", */
   /* .opcode      =    0x10000501, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdus_r, */
   /* .name        =    "bcdus.", */
   /* .description =    "Decimal unsigned shift & record", */
   /* .opcode      =    0x10000481, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::bcdutrunc_r, */
   /* .name        =    "bcdutrunc.", */
   /* .description =    "Decimal unsigned truncate & record", */
   /* .opcode      =    0x10000541, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRecordForm | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvsl,
   /* .name        = */ "lvsl",
   /* .description =    "Load vector for shift left", */
   /* .opcode      = */ 0x7C00000C,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvsr,
   /* .name        = */ "lvsr",
   /* .description =    "Load vector for shift right", */
   /* .opcode      = */ 0x7C00004C,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvx,
   /* .name        = */ "lvx",
   /* .description =    "Load vector indexed", */
   /* .opcode      = */ 0x7C0000CE,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvebx,
   /* .name        = */ "lvebx",
   /* .description =    "Load vector element byte indexed", */
   /* .opcode      = */ 0x7C00000E,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvehx,
   /* .name        = */ "lvehx",
   /* .description =    "Load vector element halfword indexed", */
   /* .opcode      = */ 0x7C00004E,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lvewx,
   /* .name        = */ "lvewx",
   /* .description =    "Load vector element word indexed", */
   /* .opcode      = */ 0x7C00008E,
   /* .format      = */ FORMAT_VRT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVMX,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mtvscr, */
   /* .name        =    "mtvscr", */
   /* .description =    "Move To VSCR", */
   /* .opcode      =    0x10000644, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormatx, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::mfvscr, */
   /* .name        =    "mfvscr", */
   /* .description =    "Move From VSCR", */
   /* .opcode      =    0x10000604, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::stvx,
   /* .name        = */ "stvx",
   /* .description =    "store vector indexed", */
   /* .opcode      = */ 0x7C0001CE,
   /* .format      = */ FORMAT_VRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stvebx,
   /* .name        = */ "stvebx",
   /* .description =    "store vector element byte indexed", */
   /* .opcode      = */ 0x7C00010E,
   /* .format      = */ FORMAT_VRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stvehx,
   /* .name        = */ "stvehx",
   /* .description =    "store vector element halfword indexed", */
   /* .opcode      = */ 0x7C00014E,
   /* .format      = */ FORMAT_VRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVMX,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stvewx,
   /* .name        = */ "stvewx",
   /* .description =    "store vector element word indexed", */
   /* .opcode      = */ 0x7C00018E,
   /* .format      = */ FORMAT_VRS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVMX,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vabsdub, */
   /* .name        =    "vabsdub", */
   /* .description =    "vector absolute difference unsigned byte", */
   /* .opcode      =    0x10000403, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vabsduh, */
   /* .name        =    "vabsduh", */
   /* .description =    "vector absolute difference unsigned hword", */
   /* .opcode      =    0x10000443, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vabsduw, */
   /* .name        =    "vabsduw", */
   /* .description =    "vector absolute difference unsigned word", */
   /* .opcode      =    0x10000483, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vand,
   /* .name        = */ "vand",
   /* .description =    "vector logical and", */
   /* .opcode      = */ 0x10000404,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vandc,
   /* .name        = */ "vandc",
   /* .description =    "vector logical and with complement", */
   /* .opcode      = */ 0x10000444,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vclzlsbb, */
   /* .name        =    "vclzlsbb", */
   /* .description =    "vector count leading zero least-significant bits byte", */
   /* .opcode      =    0x10000602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctzlsbb, */
   /* .name        =    "vctzlsbb", */
   /* .description =    "vector count trailing zero least-significant bits byte", */
   /* .opcode      =    0x10010602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctzb, */
   /* .name        =    "vctzb", */
   /* .description =    "vector count trailing zeros Byte", */
   /* .opcode      =    0x101C0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctzh, */
   /* .name        =    "vctzh", */
   /* .description =    "vector count trailing zeros Hword", */
   /* .opcode      =    0x101D0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctzw, */
   /* .name        =    "vctzw", */
   /* .description =    "vector count trailing zeros Word", */
   /* .opcode      =    0x101E0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctzd, */
   /* .name        =    "vctzd", */
   /* .description =    "vector count trailing zeros Dword", */
   /* .opcode      =    0x101F0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextractub, */
   /* .name        =    "vextractub", */
   /* .description =    "vector extract unsigned Byte", */
   /* .opcode      =    0x1000020D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextractuh, */
   /* .name        =    "vextractuh", */
   /* .description =    "vector extract unsigned Hword", */
   /* .opcode      =    0x1000024D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextractuw, */
   /* .name        =    "vextractuw", */
   /* .description =    "vector extract unsigned Word", */
   /* .opcode      =    0x1000028D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextractd, */
   /* .name        =    "vextractd", */
   /* .description =    "vector extract Dword", */
   /* .opcode      =    0x100002CD, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextsb2w, */
   /* .name        =    "vextsb2w", */
   /* .description =    "vector extend sign byte to Word", */
   /* .opcode      =    0x10100602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextsh2w, */
   /* .name        =    "vextsh2w", */
   /* .description =    "vector extend sign hword to Word", */
   /* .opcode      =    0x10110602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextsb2d, */
   /* .name        =    "vextsb2d", */
   /* .description =    "vector extend sign byte to Dword", */
   /* .opcode      =    0x10180602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextsh2d, */
   /* .name        =    "vextsh2d", */
   /* .description =    "vector extend sign hword to Dword", */
   /* .opcode      =    0x10190602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextsw2d, */
   /* .name        =    "vextsw2d", */
   /* .description =    "vector extend sign word to Dword", */
   /* .opcode      =    0x101A0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextublx, */
   /* .name        =    "vextublx", */
   /* .description =    "vector extract unsigned byte left-indexed", */
   /* .opcode      =    0x1000060D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextuhlx, */
   /* .name        =    "vextuhlx", */
   /* .description =    "vector extract unsigned hword left-indexed", */
   /* .opcode      =    0x1000064D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextuwlx, */
   /* .name        =    "vextuwlx", */
   /* .description =    "vector extract unsigned word left-indexed", */
   /* .opcode      =    0x1000068D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextubrx, */
   /* .name        =    "vextubrx", */
   /* .description =    "vector extract unsigned byte right-indexed", */
   /* .opcode      =    0x1000070D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextuhrx, */
   /* .name        =    "vextuhrx", */
   /* .description =    "vector extract unsigned hword right-indexed", */
   /* .opcode      =    0x1000074D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vextuwrx, */
   /* .name        =    "vextuwrx", */
   /* .description =    "vector extract unsigned word right-indexed", */
   /* .opcode      =    0x1000078D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vgbbd, */
   /* .name        =    "vgbbd", */
   /* .description =    "Vector Gather Bits by Byte by Dword", */
   /* .opcode      =    0x1000050C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vinsertb, */
   /* .name        =    "vinsertb", */
   /* .description =    "vector insert Byte", */
   /* .opcode      =    0x1000030D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vinserth, */
   /* .name        =    "vinserth", */
   /* .description =    "vector insert Hword", */
   /* .opcode      =    0x1000034D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vinsertw, */
   /* .name        =    "vinsertw", */
   /* .description =    "vector insert Word", */
   /* .opcode      =    0x1000038D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vinsertd, */
   /* .name        =    "vinsertd", */
   /* .description =    "vector insert Dword", */
   /* .opcode      =    0x100003CD, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmaxud, */
   /* .name        =    "vmaxud", */
   /* .description =    "Vector Maximum Unsigned Dword", */
   /* .opcode      =    0x100000C2, */
   /* .format      =    FORMAT_VRT_VRA_VRB, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vminsd, */
   /* .name        =    "vminsd", */
   /* .description =    "Vector Minimum Signed Dword", */
   /* .opcode      =    0x100003C2, */
   /* .format      =    FORMAT_VRT_VRA_VRB, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmul10cuq, */
   /* .name        =    "vmul10cuq", */
   /* .description =    "vector multiply-by-10 & write carry unsigned qword", */
   /* .opcode      =    0x10000001, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmul10ecuq, */
   /* .name        =    "vmul10ecuq", */
   /* .description =    "vector multiply-by-10 extended & write carry unsigned qword", */
   /* .opcode      =    0x10000041, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmul10uq, */
   /* .name        =    "vmul10uq", */
   /* .description =    "vector multiply-by-10 unsigned qword", */
   /* .opcode      =    0x10000201, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmul10euq, */
   /* .name        =    "vmul10euq", */
   /* .description =    "vector multiply-by-10 extended unsigned qword", */
   /* .opcode      =    0x10000241, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmulesw, */
   /* .name        =    "vmulesw", */
   /* .description =    "Vector Multiply Even Signed Word", */
   /* .opcode      =    0x10000388, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmuleuw, */
   /* .name        =    "vmuleuw", */
   /* .description =    "Vector Multiply Even Unsigned Word", */
   /* .opcode      =    0x10000288, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmulosw, */
   /* .name        =    "vmulosw", */
   /* .description =    "Vector Multiply Odd Signed Word", */
   /* .opcode      =    0x10000188, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmulouw, */
   /* .name        =    "vmulouw", */
   /* .description =    "Vector Multiply Odd Unsigned Word", */
   /* .opcode      =    0x10000088, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vnegw, */
   /* .name        =    "vnegw", */
   /* .description =    "vector negate word", */
   /* .opcode      =    0x10060602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vnegd, */
   /* .name        =    "vnegd", */
   /* .description =    "vector negate DWord", */
   /* .opcode      =    0x10070602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vprtybw, */
   /* .name        =    "vprtybw", */
   /* .description =    "vector parity byte word", */
   /* .opcode      =    0x10080602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vprtybd, */
   /* .name        =    "vprtybd", */
   /* .description =    "vector parity byte DWord", */
   /* .opcode      =    0x10090602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vprtybq, */
   /* .name        =    "vprtybq", */
   /* .description =    "vector parity byte qword", */
   /* .opcode      =    0x100A0602, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmr, */
   /* .name        =    "vor", */
   /* .description =    "vector move register", */
   /* .opcode      =    0x10000484, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_IsRegCopy, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vor,
   /* .name        = */ "vor",
   /* .description =    "vector or", */
   /* .opcode      = */ 0x10000484,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vorc, */
   /* .name        =    "vorc", */
   /* .description =    "Vector Logical OR with Complement", */
   /* .opcode      =    0x10000544, */
   /* .format      =    FORMAT_VRT_VRA_VRB, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vxor,
   /* .name        = */ "vxor",
   /* .description =    "vector xor", */
   /* .opcode      = */ 0x100004C4,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vnand, */
   /* .name        =    "vnand", */
   /* .description =    "Vector Logical NAND", */
   /* .opcode      =    0x10000584, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vncipher, */
   /* .name        =    "vncipher", */
   /* .description =    "Vector AES Inverse Cipher", */
   /* .opcode      =    0x10000548, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vncipherlast, */
   /* .name        =    "vncipherlast", */
   /* .description =    "Vector AES Inverse Cipher Last", */
   /* .opcode      =    0x10000549, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vnor,
   /* .name        = */ "vnor",
   /* .description =    "vector nor", */
   /* .opcode      = */ 0x10000504,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vperm,
   /* .name        = */ "vperm",
   /* .description =    "vector permute", */
   /* .opcode      = */ 0x1000002B,
   /* .format      = */ FORMAT_VRT_VRA_VRB_VRC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpermr, */
   /* .name        =    "vpermr", */
   /* .description =    "vector permute right-indexed", */
   /* .opcode      =    0x1000003B, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vbpermd, */
   /* .name        =    "vbpermd", */
   /* .description =    "vector bit permute DWord", */
   /* .opcode      =    0x100005CC, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vbpermq,
   /* .name        = */ "vbpermq",
   /* .description =    "Vector Bit Permute Qword", */
   /* .opcode      = */ 0x1000054C,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vrld, */
   /* .name        =    "vrld", */
   /* .description =    "Vector Rotate Left Dword", */
   /* .opcode      =    0x100000C4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsbox, */
   /* .name        =    "vsbox", */
   /* .description =    "Vector AES SubBytes", */
   /* .opcode      =    0x100005C8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsel,
   /* .name        = */ "vsel",
   /* .description =    "vector conditional select", */
   /* .opcode      = */ 0x1000002A,
   /* .format      = */ FORMAT_VRT_VRA_VRB_VRC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsld, */
   /* .name        =    "vsld", */
   /* .description =    "Vector Shift Left Dword", */
   /* .opcode      =    0x100005C4, */
   /* .format      =    FORMAT_VRT_VRA_VRB, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsrad, */
   /* .name        =    "vsrad", */
   /* .description =    "Vector Shift Right Algebraic Dword", */
   /* .opcode      =    0x100003C4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsrd, */
   /* .name        =    "vsrd", */
   /* .description =    "Vector Shift Right Dword", */
   /* .opcode      =    0x100006C4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsubcuq, */
   /* .name        =    "vsubcuq", */
   /* .description =    "Vector Subtract & write Carry Unsigned Qword", */
   /* .opcode      =    0x10000540, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsubecuq, */
   /* .name        =    "vsubecuq", */
   /* .description =    "Vector Subtract Extended & write Carry Unsigned Qword", */
   /* .opcode      =    0x1000003F, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsubeuqm, */
   /* .name        =    "vsubeuqm", */
   /* .description =    "Vector Subtract Extended Unsigned Qword Modulo", */
   /* .opcode      =    0x1000003E, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsubuqm, */
   /* .name        =    "vsubuqm", */
   /* .description =    "Vector Subtract Unsigned Qword Modulo", */
   /* .opcode      =    0x10000500, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsumsws,
   /* .name        = */ "vsumsws",
   /* .description =    "vector sum across signed word saturate", */
   /* .opcode      = */ 0x10000788,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsum2sws,
   /* .name        = */ "vsum2sws",
   /* .description =    "vector sum across partial signed word saturate", */
   /* .opcode      = */ 0x10000688,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsum4sbs,
   /* .name        = */ "vsum4sbs",
   /* .description =    "vector sum across partial signed byte saturate", */
   /* .opcode      = */ 0x10000708,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsum4shs,
   /* .name        = */ "vsum4shs",
   /* .description =    "vector sum across partial signed halfword saturate", */
   /* .opcode      = */ 0x10000648,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsum4ubs,
   /* .name        = */ "vsum4ubs",
   /* .description =    "vector sum across partial unsigned byte saturate", */
   /* .opcode      = */ 0x10000608,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vspltb,
   /* .name        = */ "vspltb",
   /* .description =    "vector splat byte", */
   /* .opcode      = */ 0x1000020C,
   /* .format      = */ FORMAT_VRT_VRB_UIM4,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsplth,
   /* .name        = */ "vsplth",
   /* .description =    "vector splat halfword", */
   /* .opcode      = */ 0x1000024C,
   /* .format      = */ FORMAT_VRT_VRB_UIM3,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vspltw,
   /* .name        = */ "vspltw",
   /* .description =    "vector splat word", */
   /* .opcode      = */ 0x1000028C,
   /* .format      = */ FORMAT_VRT_VRB_UIM2,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vspltisb,
   /* .name        = */ "vspltisb",
   /* .description =    "vector splat immediate signed byte", */
   /* .opcode      = */ 0x1000030C,
   /* .format      = */ FORMAT_VRT_SIM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vspltish,
   /* .name        = */ "vspltish",
   /* .description =    "vector splat immediate signed halfword", */
   /* .opcode      = */ 0x1000034C,
   /* .format      = */ FORMAT_VRT_SIM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vspltisw,
   /* .name        = */ "vspltisw",
   /* .description =    "vector splat immediate signed word", */
   /* .opcode      = */ 0x1000038C,
   /* .format      = */ FORMAT_VRT_SIM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsl,
   /* .name        = */ "vsl",
   /* .description =    "vector shift left", */
   /* .opcode      = */ 0x100001C4,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vslb,
   /* .name        = */ "vslb",
   /* .description =    "vector shift left byte", */
   /* .opcode      = */ 0x10000104,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsldoi,
   /* .name        = */ "vsldoi",
   /* .description =    "vector shift left double by octet immediate", */
   /* .opcode      = */ 0x1000002C,
   /* .format      = */ FORMAT_VRT_VRA_VRB_SHB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vslh,
   /* .name        = */ "vslh",
   /* .description =    "vector shift left halfword", */
   /* .opcode      = */ 0x10000144,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vslo,
   /* .name        = */ "vslo",
   /* .description =    "vector shift left by octet", */
   /* .opcode      = */ 0x1000040C,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vslw,
   /* .name        = */ "vslw",
   /* .description =    "vector shift left word", */
   /* .opcode      = */ 0x10000184,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsr,
   /* .name        = */ "vsr",
   /* .description =    "vector shift right", */
   /* .opcode      = */ 0x100002C4,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsrab,
   /* .name        = */ "vsrab",
   /* .description =    "vector shift right algebraic byte", */
   /* .opcode      = */ 0x10000304,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsrah,
   /* .name        = */ "vsrah",
   /* .description =    "vector shift right algebraic halfword", */
   /* .opcode      = */ 0x10000344,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsraw,
   /* .name        = */ "vsraw",
   /* .description =    "vector shift right algebraic word", */
   /* .opcode      = */ 0x10000384,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsrb,
   /* .name        = */ "vsrb",
   /* .description =    "vector shift right byte", */
   /* .opcode      = */ 0x10000204,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsrh,
   /* .name        = */ "vsrh",
   /* .description =    "vector shift right halfword", */
   /* .opcode      = */ 0x10000244,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsro,
   /* .name        = */ "vsro",
   /* .description =    "vector shift right by octet", */
   /* .opcode      = */ 0x1000044C,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsrw,
   /* .name        = */ "vsrw",
   /* .description =    "vector shift right word", */
   /* .opcode      = */ 0x10000284,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsrv, */
   /* .name        =    "vsrv", */
   /* .description =    "vector shift right variable", */
   /* .opcode      =    0x10000704, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vslv, */
   /* .name        =    "vslv", */
   /* .description =    "vector shift left variable", */
   /* .opcode      =    0x10000744, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vrlb,
   /* .name        = */ "vrlb",
   /* .description =    "vector rotate left byte", */
   /* .opcode      = */ 0x10000004,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vrlh,
   /* .name        = */ "vrlh",
   /* .description =    "vector rotate left halfword", */
   /* .opcode      = */ 0x10000044,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vrlw,
   /* .name        = */ "vrlw",
   /* .description =    "vector rotate left word", */
   /* .opcode      = */ 0x10000084,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsRotateOrShift |
                        PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vrlwmi, */
   /* .name        =    "vrlwmi", */
   /* .description =    "vector rotate left word then mask insert", */
   /* .opcode      =    0x10000085, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vrldmi, */
   /* .name        =    "vrldmi", */
   /* .description =    "vector rotate left DWord then mask insert", */
   /* .opcode      =    0x100000C5, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vrlwnm, */
   /* .name        =    "vrlwnm", */
   /* .description =    "vector rotate left word then and with mask", */
   /* .opcode      =    0x10000185, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vrldnm, */
   /* .name        =    "vrldnm", */
   /* .description =    "vector rotate left DWord then and with mask", */
   /* .opcode      =    0x100001C5, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsRotateOrShift | */
   /*                   PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vaddcuw, */
   /* .name        =    "vaddcuw", */
   /* .description =    "Vector Add & Write Carry-Out Unsigned Word", */
   /* .opcode      =    0x10000180, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vaddcuq, */
   /* .name        =    "vaddcuq", */
   /* .description =    "Vector Add & write Carry Unsigned Qword", */
   /* .opcode      =    0x10000140, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vaddecuq, */
   /* .name        =    "vaddecuq", */
   /* .description =    "Vector Add Extended & write Carry Unsigned Qword", */
   /* .opcode      =    0x1000003D, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vaddeuqm, */
   /* .name        =    "vaddeuqm", */
   /* .description =    "Vector Add Extended Unsigned Qword Modulo", */
   /* .opcode      =    0x1000003C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddsbs,
   /* .name        = */ "vaddsbs",
   /* .description =    "vector add signed byte saturate", */
   /* .opcode      = */ 0x10000300,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddshs,
   /* .name        = */ "vaddshs",
   /* .description =    "vector add signed halfword saturate", */
   /* .opcode      = */ 0x10000340,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddsws,
   /* .name        = */ "vaddsws",
   /* .description =    "vector add signed word saturate", */
   /* .opcode      = */ 0x10000380,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddubm,
   /* .name        = */ "vaddubm",
   /* .description =    "vector add unsigned byte modulo", */
   /* .opcode      = */ 0x10000000,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddubs,
   /* .name        = */ "vaddubs",
   /* .description =    "vector add unsigned byte saturate", */
   /* .opcode      = */ 0x10000200,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vaddudm,
   /* .name        = */ "vaddudm",
   /* .description =    "Vector Add Unsigned Dword Modulo", */
   /* .opcode      = */ 0x100000C0,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vadduhm,
   /* .name        = */ "vadduhm",
   /* .description =    "vector add unsigned halfword modulo", */
   /* .opcode      = */ 0x10000040,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vadduhs,
   /* .name        = */ "vadduhs",
   /* .description =    "vector add unsigned halfword saturate", */
   /* .opcode      = */ 0x10000240,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vadduwm,
   /* .name        = */ "vadduwm",
   /* .description =    "vector add unsigned word modulo", */
   /* .opcode      = */ 0x10000080,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vadduws,
   /* .name        = */ "vadduws",
   /* .description =    "vector add unsigned word saturate", */
   /* .opcode      = */ 0x10000280,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vadduqm, */
   /* .name        =    "vadduqm", */
   /* .description =    "Vector Add Unsigned Qword Modulo", */
   /* .opcode      =    0x10000100, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavgsb, */
   /* .name        =    "vavgsb", */
   /* .description =    "Vector Average Signed Byte", */
   /* .opcode      =    0x10000502, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavgsh, */
   /* .name        =    "vavgsh", */
   /* .description =    "Vector Average Signed Hword", */
   /* .opcode      =    0x10000542, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavgsw, */
   /* .name        =    "vavgsw", */
   /* .description =    "Vector Average Signed Word", */
   /* .opcode      =    0x10000582, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavgub, */
   /* .name        =    "vavgub", */
   /* .description =    "Vector Average Unsigned Byte", */
   /* .opcode      =    0x10000402, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavguh, */
   /* .name        =    "vavguh", */
   /* .description =    "Vector Average Unsigned Hword", */
   /* .opcode      =    0x10000442, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vavguw, */
   /* .name        =    "vavguw", */
   /* .description =    "Vector Average Unsigned Word", */
   /* .opcode      =    0x10000482, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vsubcuw, */
   /* .name        =    "vsubcuw", */
   /* .description =    "Vector Subtract & Write Carry-Out Unsigned Word", */
   /* .opcode      =    0x10000580, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_SetsCarryFlag, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubsbs,
   /* .name        = */ "vsubsbs",
   /* .description =    "vector subtract signed byte saturate", */
   /* .opcode      = */ 0x10000700,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubshs,
   /* .name        = */ "vsubshs",
   /* .description =    "vector subtract signed halfword saturate", */
   /* .opcode      = */ 0x10000740,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubsws,
   /* .name        = */ "vsubsws",
   /* .description =    "vector subtract signed word saturate", */
   /* .opcode      = */ 0x10000780,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsububm,
   /* .name        = */ "vsububm",
   /* .description =    "vector subtract unsigned byte modulo", */
   /* .opcode      = */ 0x10000400,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsububs,
   /* .name        = */ "vsububs",
   /* .description =    "vector subtract unsigned byte saturate", */
   /* .opcode      = */ 0x10000600,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubudm,
   /* .name        = */ "vsubudm",
   /* .description =    "vector subtract unsigned Dword modulo", */
   /* .opcode      = */ 0x100004C0,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubuhm,
   /* .name        = */ "vsubuhm",
   /* .description =    "vector subtract unsigned halfword modulo", */
   /* .opcode      = */ 0x10000440,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubuhs,
   /* .name        = */ "vsubuhs",
   /* .description =    "vector subtract unsigned halfword saturate", */
   /* .opcode      = */ 0x10000640,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubuwm,
   /* .name        = */ "vsubuwm",
   /* .description =    "vector subtract unsigned word modulo", */
   /* .opcode      = */ 0x10000480,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vsubuws,
   /* .name        = */ "vsubuws",
   /* .description =    "vector subtract unsigned word saturate", */
   /* .opcode      = */ 0x10000680,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmhaddshs, */
   /* .name        =    "vmhaddshs", */
   /* .description =    "Vector Multiply-High-Add Signed Hword Saturate", */
   /* .opcode      =    0x10000020, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmhraddshs, */
   /* .name        =    "vmhraddshs", */
   /* .description =    "Vector Multiply-High-Round-Add Signed Hword Saturate", */
   /* .opcode      =    0x10000021, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmladduhm, */
   /* .name        =    "vmladduhm", */
   /* .description =    "Vector Multiply-Low-Add Unsigned Hword Modulo", */
   /* .opcode      =    0x10000022, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsummbm, */
   /* .name        =    "vmsummbm", */
   /* .description =    "Vector Multiply-Sum Mixed Byte Modulo", */
   /* .opcode      =    0x10000025, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsumshm, */
   /* .name        =    "vmsumshm", */
   /* .description =    "Vector Multiply-Sum Signed Hword Modulo", */
   /* .opcode      =    0x10000028, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsumshs, */
   /* .name        =    "vmsumshs", */
   /* .description =    "Vector Multiply-Sum Signed Hword Saturate", */
   /* .opcode      =    0x10000029, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsumubm, */
   /* .name        =    "vmsumubm", */
   /* .description =    "Vector Multiply-Sum Unsigned Byte Modulo", */
   /* .opcode      =    0x10000024, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsumuhs, */
   /* .name        =    "vmsumuhs", */
   /* .description =    "Vector Multiply-Sum Unsigned Hword Saturate", */
   /* .opcode      =    0x10000027, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmulesb, */
   /* .name        =    "vmulesb", */
   /* .description =    "Vector Multiply Even Signed Byte", */
   /* .opcode      =    0x10000308, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmuleub, */
   /* .name        =    "vmuleub", */
   /* .description =    "Vector Multiply Even Unsigned Byte", */
   /* .opcode      =    0x10000208, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmuleuh, */
   /* .name        =    "vmuleuh", */
   /* .description =    "Vector Multiply Even Unsigned Hword", */
   /* .opcode      =    0x10000248, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmulosb, */
   /* .name        =    "vmulosb", */
   /* .description =    "Vector Multiply Odd Signed Byte", */
   /* .opcode      =    0x10000108, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmuloub, */
   /* .name        =    "vmuloub", */
   /* .description =    "Vector Multiply Odd Unsigned Byte", */
   /* .opcode      =    0x10000008, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmulesh,
   /* .name        = */ "vmulesh",
   /* .description =    "vector multiply even signed halfword", */
   /* .opcode      = */ 0x10000348,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmulosh,
   /* .name        = */ "vmulosh",
   /* .description =    "vector multiply odd signed halfword", */
   /* .opcode      = */ 0x10000148,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmulouh,
   /* .name        = */ "vmulouh",
   /* .description =    "vector multiply odd unsigned halfword", */
   /* .opcode      = */ 0x10000048,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmuluwm,
   /* .name        = */ "vmuluwm",
   /* .description =    "vector multiply unsigned word modulo", */
   /* .opcode      = */ 0x10000089,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminsb,
   /* .name        = */ "vminsb",
   /* .description =    "vector minimum signed byte", */
   /* .opcode      = */ 0x10000302,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminsh,
   /* .name        = */ "vminsh",
   /* .description =    "vector minimum signed halfword", */
   /* .opcode      = */ 0x10000342,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminsw,
   /* .name        = */ "vminsw",
   /* .description =    "vector minimum signed word", */
   /* .opcode      = */ 0x10000382,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminub,
   /* .name        = */ "vminub",
   /* .description =    "vector minimum unsigned byte", */
   /* .opcode      = */ 0x10000202,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminuh,
   /* .name        = */ "vminuh",
   /* .description =    "vector minimum unsigned halfword", */
   /* .opcode      = */ 0x10000242,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vminuw,
   /* .name        = */ "vminuw",
   /* .description =    "vector minimum unsigned word", */
   /* .opcode      = */ 0x10000282,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxsb,
   /* .name        = */ "vmaxsb",
   /* .description =    "vector maximum signed byte", */
   /* .opcode      = */ 0x10000102,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxsh,
   /* .name        = */ "vmaxsh",
   /* .description =    "vector maximum signed halfword", */
   /* .opcode      = */ 0x10000142,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxsw,
   /* .name        = */ "vmaxsw",
   /* .description =    "vector maximum signed word", */
   /* .opcode      = */ 0x10000182,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxub,
   /* .name        = */ "vmaxub",
   /* .description =    "vector maximum unsigned byte", */
   /* .opcode      = */ 0x10000002,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxuh,
   /* .name        = */ "vmaxuh",
   /* .description =    "vector maximum unsigned halfword", */
   /* .opcode      = */ 0x10000042,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmaxuw,
   /* .name        = */ "vmaxuw",
   /* .description =    "vector maximum unsigned word", */
   /* .opcode      = */ 0x10000082,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vmsumudm, */
   /* .name        =    "vmsumudm", */
   /* .description =    "Vector Multiply-Sum Unsigned Dword", */
   /* .opcode      =    0x7C000154, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmsumuhm,
   /* .name        = */ "vmsumuhm",
   /* .description =    "vector multiply-sum unsigned halfword word modulo", */
   /* .opcode      = */ 0x10000026,
   /* .format      = */ FORMAT_VRT_VRA_VRB_VRC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcipher, */
   /* .name        =    "vcipher", */
   /* .description =    "Vector AES Cipher", */
   /* .opcode      =    0x10000508, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcipherlast, */
   /* .name        =    "vcipherlast", */
   /* .description =    "Vector AES Cipher Last", */
   /* .opcode      =    0x10000509, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vclzb, */
   /* .name        =    "vclzb", */
   /* .description =    "Vector Count Leading Zeros Byte", */
   /* .opcode      =    0x10000702, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vclzd, */
   /* .name        =    "vclzd", */
   /* .description =    "Vector Count Leading Zeros Dword", */
   /* .opcode      =    0x100007C2, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vclzh, */
   /* .name        =    "vclzh", */
   /* .description =    "Vector Count Leading Zeros Hword", */
   /* .opcode      =    0x10000742, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vclzw, */
   /* .name        =    "vclzw", */
   /* .description =    "Vector Count Leading Zeros Word", */
   /* .opcode      =    0x10000782, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpbfp, */
   /* .name        =    "vcmpbfp", */
   /* .description =    "Vector Compare Bounds Floating-Point", */
   /* .opcode      =    0x100003C6, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_HasRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpbfp_r, */
   /* .name        =    "vcmpbfp.", */
   /* .description =    "Vector Compare Bounds Floating-Point Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpbfp].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpbfp].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpbfp].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpbfp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpeqfp, */
   /* .name        =    "vcmpeqfp", */
   /* .description =    "Vector Compare Equal To Floating-Point", */
   /* .opcode      =    0x100000C6, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_HasRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpeqfp_r, */
   /* .name        =    "vcmpeqfp.", */
   /* .description =    "Vector Compare Equal To Floating-Point Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpeqfp].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpeqfp].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpeqfp].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpeqfp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequb,
   /* .name        = */ "vcmpequb",
   /* .description =    "vector compare equal unsigned byte", */
   /* .opcode      = */ 0x10000006,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequb_r,
   /* .name        = */ "vcmpequb.",
   /* .description =    "vector compare equal unsigned byte with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequb].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequb].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequb].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequb].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpequd, */
   /* .name        =    "vcmpequd", */
   /* .description =    "Vector Compare Equal Unsigned Dword", */
   /* .opcode      =    0x100000C7, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_HasRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpequd_r, */
   /* .name        =    "vcmpequd.", */
   /* .description =    "Vector Compare Equal Unsigned Dword Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequd].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequd].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequd].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequh,
   /* .name        = */ "vcmpequh",
   /* .description =    "vector compare equal unsigned halfword", */
   /* .opcode      = */ 0x10000046,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequh_r,
   /* .name        = */ "vcmpequh.",
   /* .description =    "vector compare equal unsigned halfword with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequh].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequh].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequh].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequw,
   /* .name        = */ "vcmpequw",
   /* .description =    "vector compare equal unsigned word", */
   /* .opcode      = */ 0x10000086,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpequw_r,
   /* .name        = */ "vcmpequw.",
   /* .description =    "vector compare equal unsigned word with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequw].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpequw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsb,
   /* .name        = */ "vcmpgtsb",
   /* .description =    "vector compare greater than signed byte", */
   /* .opcode      = */ 0x10000306,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsb_r,
   /* .name        = */ "vcmpgtsb.",
   /* .description =    "vector compare greater than signed byte with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsb].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsb].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsb].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsb].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpgtsd, */
   /* .name        =    "vcmpgtsd", */
   /* .description =    "Vector Compare Greater Than Signed Dword", */
   /* .opcode      =    0x100003C7, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_HasRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpgtsd_r, */
   /* .name        =    "vcmpgtsd.", */
   /* .description =    "Vector Compare Greater Than Signed Dword Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsd].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsd].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsd].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsd].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsh,
   /* .name        = */ "vcmpgtsh",
   /* .description =    "vector compare greater than signed halfword", */
   /* .opcode      = */ 0x10000346,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsh_r,
   /* .name        = */ "vcmpgtsh.",
   /* .description =    "vector compare greater than signed halfword with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsh].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsh].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsh].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsw,
   /* .name        = */ "vcmpgtsw",
   /* .description =    "vector compare greater than signed word", */
   /* .opcode      = */ 0x10000386,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtsw_r,
   /* .name        = */ "vcmpgtsw.",
   /* .description =    "vector compare greater than signed word with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsw].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtsw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtub,
   /* .name        = */ "vcmpgtub",
   /* .description =    "vector compare greater than unsigned byte", */
   /* .opcode      = */ 0x10000206,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtub_r,
   /* .name        = */ "vcmpgtub.",
   /* .description =    "vector compare greater than unsigned byte with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtub].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtub].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtub].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtub].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtuh,
   /* .name        = */ "vcmpgtuh",
   /* .description =    "vector compare greater than unsigned halfword", */
   /* .opcode      = */ 0x10000246,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtuh_r,
   /* .name        = */ "vcmpgtuh.",
   /* .description =    "vector compare greater than unsigned halfword with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuh].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuh].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuh].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtuw,
   /* .name        = */ "vcmpgtuw",
   /* .description =    "vector compare greater than unsigned word", */
   /* .opcode      = */ 0x10000286,
   /* .format      = */ FORMAT_VRT_VRA_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_HasRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vcmpgtuw_r,
   /* .name        = */ "vcmpgtuw.",
   /* .description =    "vector compare greater than unsigned word with record", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuw].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuw].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuw].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpgtuw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpneb, */
   /* .name        =    "vcmpneb", */
   /* .description =    "vector compare not equal Byte", */
   /* .opcode      =    0x10000007, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpneb_r, */
   /* .name        =    "vcmpneb.", */
   /* .description =    "vector compare not equal Byte Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneb].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneb].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneb].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneb].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpneh, */
   /* .name        =    "vcmpneh", */
   /* .description =    "vector compare not equal Hword", */
   /* .opcode      =    0x10000047, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpneh_r, */
   /* .name        =    "vcmpneh.", */
   /* .description =    "vector compare not equal Hword Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneh].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneh].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneh].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpneh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnew, */
   /* .name        =    "vcmpnew", */
   /* .description =    "vector compare not equal Word", */
   /* .opcode      =    0x10000087, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnew_r, */
   /* .name        =    "vcmpnew.", */
   /* .description =    "vector compare not equal Word Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnew].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnew].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnew].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnew].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezb, */
   /* .name        =    "vcmpnezb", */
   /* .description =    "vector compare not equal or zero Byte", */
   /* .opcode      =    0x10000107, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezb_r, */
   /* .name        =    "vcmpnezb.", */
   /* .description =    "vector compare not equal or zero Byte Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezb].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezb].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezb].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezb].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezh, */
   /* .name        =    "vcmpnezh", */
   /* .description =    "vector compare not equal or zero Hword", */
   /* .opcode      =    0x10000147, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezh_r, */
   /* .name        =    "vcmpnezh.", */
   /* .description =    "vector compare not equal or zero Hword Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezh].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezh].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezh].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezh].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezw, */
   /* .name        =    "vcmpnezw", */
   /* .description =    "vector compare not equal or zero Word", */
   /* .opcode      =    0x10000187, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vcmpnezw_r, */
   /* .name        =    "vcmpnezw.", */
   /* .description =    "vector compare not equal or zero Word Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezw].opcode + 1, */
   /* .format      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezw].format, */
   /* .minimumALS  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezw].minimumALS, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::vcmpnezw].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vctsxs, */
   /* .name        =    "vctsxs", */
   /* .description =    "Vector Convert To Signed Word Saturate", */
   /* .opcode      =    0x100003CA, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::veqv, */
   /* .name        =    "veqv", */
   /* .description =    "Vector Logical Equivalence", */
   /* .opcode      =    0x10000684, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vupkhsb,
   /* .name        = */ "vupkhsb",
   /* .description =    "vector unpack high signed byte", */
   /* .opcode      = */ 0x1000020E,
   /* .format      = */ FORMAT_VRT_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vupkhsh,
   /* .name        = */ "vupkhsh",
   /* .description =    "vector unpack high signed halfword", */
   /* .opcode      = */ 0x1000024E,
   /* .format      = */ FORMAT_VRT_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vupklsb,
   /* .name        = */ "vupklsb",
   /* .description =    "vector unpack low signed byte", */
   /* .opcode      = */ 0x1000028E,
   /* .format      = */ FORMAT_VRT_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vupklsh,
   /* .name        = */ "vupklsh",
   /* .description =    "vector unpack low signed halfword", */
   /* .opcode      = */ 0x100002CE,
   /* .format      = */ FORMAT_VRT_VRB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vupklsw, */
   /* .name        =    "vupklsw", */
   /* .description =    "Vector Unpack Low Signed Word", */
   /* .opcode      =    0x100006CE, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vupklpx, */
   /* .name        =    "vupklpx", */
   /* .description =    "Vector Unpack Low Pixel", */
   /* .opcode      =    0x100003CE, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vupkhsw, */
   /* .name        =    "vupkhsw", */
   /* .description =    "Vector Unpack High Signed Word", */
   /* .opcode      =    0x1000064E, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpksdss, */
   /* .name        =    "vpksdss", */
   /* .description =    "Vector Pack Signed Dword Signed Saturate", */
   /* .opcode      =    0x100005CE, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpksdus, */
   /* .name        =    "vpksdus", */
   /* .description =    "Vector Pack Signed Dword Unsigned Saturate", */
   /* .opcode      =    0x1000054E, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vpkuhum,
   /* .name        = */ "vpkuhum",
   /* .description =    "vector pack unsigned half word unsigned modulo", */
   /* .opcode      = */ 0x1000000E,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vpkuwum,
   /* .name        = */ "vpkuwum",
   /* .description =    "vector pack unsigned word unsigned modulo", */
   /* .opcode      = */ 0x1000004E,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpkuwus, */
   /* .name        =    "vpkuwus", */
   /* .description =    "Vector Pack Unsigned Word Unsigned Saturate", */
   /* .opcode      =    0x100000CE, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp6, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpmsumb, */
   /* .name        =    "vpmsumb", */
   /* .description =    "Vector Polynomial Multiply-Sum Byte", */
   /* .opcode      =    0x10000408, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpmsumh, */
   /* .name        =    "vpmsumh", */
   /* .description =    "Vector Polynomial Multiply-Sum Hword", */
   /* .opcode      =    0x10000448, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpmsumw, */
   /* .name        =    "vpmsumw", */
   /* .description =    "Vector Polynomial Multiply-Sum Word", */
   /* .opcode      =    0x10000488, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpopcntb, */
   /* .name        =    "vpopcntb", */
   /* .description =    "Vector Population Count Byte", */
   /* .opcode      =    0x10000703, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpopcntd, */
   /* .name        =    "vpopcntd", */
   /* .description =    "Vector Population Count Dword", */
   /* .opcode      =    0x100007C3, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpopcnth, */
   /* .name        =    "vpopcnth", */
   /* .description =    "Vector Population Count Hword", */
   /* .opcode      =    0x10000743, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::vpopcntw, */
   /* .name        =    "vpopcntw", */
   /* .description =    "Vector Population Count Word", */
   /* .opcode      =    0x10000783, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp8, */
   /* .properties  =    PPCOpProp_IsVMX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrghb,
   /* .name        = */ "vmrghb",
   /* .description =    "vector merge high byte", */
   /* .opcode      = */ 0x1000000C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrghh,
   /* .name        = */ "vmrghh",
   /* .description =    "vector merge high half word", */
   /* .opcode      = */ 0x1000004C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrghw,
   /* .name        = */ "vmrghw",
   /* .description =    "vector merge high word", */
   /* .opcode      = */ 0x1000008C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrglb,
   /* .name        = */ "vmrglb",
   /* .description =    "vector merge low byte", */
   /* .opcode      = */ 0x1000010C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrglh,
   /* .name        = */ "vmrglh",
   /* .description =    "vector merge low half word", */
   /* .opcode      = */ 0x1000014C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrglw,
   /* .name        = */ "vmrglw",
   /* .description =    "vector merge low word", */
   /* .opcode      = */ 0x1000018C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp6,
   /* .properties  = */ PPCOpProp_IsVMX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxsd, */
   /* .name        =    "lxsd", */
   /* .description =    "Load VSX scalar Dword", */
   /* .opcode      =    0xE4000002, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_DWord | */
   /*                   PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxsdx,
   /* .name        = */ "lxsdx",
   /* .description =    "Load VSX Scalar Doubleword Indexed", */
   /* .opcode      = */ 0x7C000498,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsLoad |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxssp, */
   /* .name        =    "lxssp", */
   /* .description =    "Load VSX scalar single", */
   /* .opcode      =    0xE4000003, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxsibzx, */
   /* .name        =    "lxsibzx", */
   /* .description =    "Load VSX scalar as integer byte & zero indexed", */
   /* .opcode      =    0x7C00061A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxsihzx, */
   /* .name        =    "lxsihzx", */
   /* .description =    "Load VSX scalar as integer Hword & zero indexed", */
   /* .opcode      =    0x7C00065A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxv, */
   /* .name        =    "lxv", */
   /* .description =    "Load VSX vector", */
   /* .opcode      =    0xF4000001, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvb16x, */
   /* .name        =    "lxvb16x", */
   /* .description =    "Load VSX vector byte*16 indexed", */
   /* .opcode      =    0x7C0006D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvh8x, */
   /* .name        =    "lxvh8x", */
   /* .description =    "Load VSX vector Hword*8 indexed", */
   /* .opcode      =    0x7C000658, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvl, */
   /* .name        =    "lxvl", */
   /* .description =    "Load VSX vector with length", */
   /* .opcode      =    0x7C00021A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvll, */
   /* .name        =    "lxvll", */
   /* .description =    "Load VSX vector left-justified with length", */
   /* .opcode      =    0x7C00025A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvwsx, */
   /* .name        =    "lxvwsx", */
   /* .description =    "Load VSX vector word & splat indexed", */
   /* .opcode      =    0x7C0002D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::lxvx, */
   /* .name        =    "lxvx", */
   /* .description =    "Load VSX vector indexed", */
   /* .opcode      =    0x7C000218, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsLoad | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxsd, */
   /* .name        =    "stxsd", */
   /* .description =    "Store VSX Scalar Dword", */
   /* .opcode      =    0xF4000002, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::stxsdx,
   /* .name        = */ "stxsdx",
   /* .description =    "Store VSX Scalar Doubleword Indexed", */
   /* .opcode      = */ 0x7C000598,
   /* .format      = */ FORMAT_XS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsStore |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxvd2x,
   /* .name        = */ "lxvd2x",
   /* .description =    "Load VSX Vector Doubleword*2 Indexed", */
   /* .opcode      = */ 0x7C000698,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxvdsx,
   /* .name        = */ "lxvdsx",
   /* .description =    "Load VSX Vector Doubleword & Splat Indexed", */
   /* .opcode      = */ 0x7C000298,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxvw4x,
   /* .name        = */ "lxvw4x",
   /* .description =    "Load VSX Vector Word*4 Indexed", */
   /* .opcode      = */ 0x7C000618,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsLoad |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stxvd2x,
   /* .name        = */ "stxvd2x",
   /* .description =    "store VSX Vector Doubleword*2 Indexed", */
   /* .opcode      = */ 0x7C000798,
   /* .format      = */ FORMAT_XS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stxvw4x,
   /* .name        = */ "stxvw4x",
   /* .description =    "store VSX Vector Word*4 Indexed", */
   /* .opcode      = */ 0x7C000718,
   /* .format      = */ FORMAT_XS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsStore |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxssp, */
   /* .name        =    "stxssp", */
   /* .description =    "Store VSX Scalar SP", */
   /* .opcode      =    0xF4000003, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_IsStore, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsabsdp,
   /* .name        = */ "xsabsdp",
   /* .description =    "VSX Scalar Absolute Value Double-Precision", */
   /* .opcode      = */ 0xF0000564,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsadddp,
   /* .name        = */ "xsadddp",
   /* .description =    "VSX Scalar Add Double-Precision", */
   /* .opcode      = */ 0xF0000100,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscmpodp,
   /* .name        = */ "xscmpodp",
   /* .description =    "VSX Scalar Compare Ordered Double-Precision", */
   /* .opcode      = */ 0xF0000158,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscmpudp,
   /* .name        = */ "xscmpudp",
   /* .description =    "VSX Scalar Compare Unordered Double-Precision", */
   /* .opcode      = */ 0xF0000118,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_CompareOp |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxsibx, */
   /* .name        =    "stxsibx", */
   /* .description =    "Store VSX scalar as integer byte indexed", */
   /* .opcode      =    0x7C00071A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxsihx, */
   /* .name        =    "stxsihx", */
   /* .description =    "Store VSX scalar as integer Hword indexed", */
   /* .opcode      =    0x7C00075A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxv, */
   /* .name        =    "stxv", */
   /* .description =    "Store VSX vector", */
   /* .opcode      =    0xF4000005, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxvb16x, */
   /* .name        =    "stxvb16x", */
   /* .description =    "Store VSX vector byte*16 indexed", */
   /* .opcode      =    0x7C0007D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxvh8x, */
   /* .name        =    "stxvh8x", */
   /* .description =    "Store VSX vector Hword*8 indexed", */
   /* .opcode      =    0x7C000758, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxvl, */
   /* .name        =    "stxvl", */
   /* .description =    "Store VSX vector with length", */
   /* .opcode      =    0x7C00031A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxvll, */
   /* .name        =    "stxvll", */
   /* .description =    "Store VSX vector left-justified with length", */
   /* .opcode      =    0x7C00035A, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::stxvx, */
   /* .name        =    "stxvx", */
   /* .description =    "Store VSX vector indexed", */
   /* .opcode      =    0x7C000318, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsStore | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsabsqp, */
   /* .name        =    "xsabsqp", */
   /* .description =    "VSX scalar absolute QP", */
   /* .opcode      =    0xFC000648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsaddqp, */
   /* .name        =    "xsaddqp", */
   /* .description =    "VSX scalar add QP", */
   /* .opcode      =    0xFC000008, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpexpqp, */
   /* .name        =    "xscmpexpqp", */
   /* .description =    "VSX scalar compare exponents QP", */
   /* .opcode      =    0xFC000148, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpoqp, */
   /* .name        =    "xscmpoqp", */
   /* .description =    "VSX scalar compare ordered QP", */
   /* .opcode      =    0xFC000108, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpuqp, */
   /* .name        =    "xscmpuqp", */
   /* .description =    "VSX Scalar Compare Unordered QP", */
   /* .opcode      =    0xFC000508, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_CompareOp | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscpsgndp,
   /* .name        = */ "xscpsgndp",
   /* .description =    "VSX Scalar Copy Sign Double-Precision , For VSR Copy register", */
   /* .opcode      = */ 0xF0000580,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_IsRegCopy |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscpsgnqp, */
   /* .name        =    "xscpsgnqp", */
   /* .description =    "VSX Scalar Copy Sign QP", */
   /* .opcode      =    0xFC0000C8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvqpdp, */
   /* .name        =    "xscvqpdp", */
   /* .description =    "VSX Scalar Convert QP to DP", */
   /* .opcode      =    0xFC140688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpsxds,
   /* .name        = */ "xscvdpsxds",
   /* .description =    "VSX Scalar Convert Double-Precision to Signed Integer Doubleword Saturate", */
   /* .opcode      = */ 0xF0000560,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpsxws,
   /* .name        = */ "xscvdpsxws",
   /* .description =    "VSX Scalar Convert Double-Precision to Signed Integer Word Saturate", */
   /* .opcode      = */ 0xF0000160,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvdpqp, */
   /* .name        =    "xscvdpqp", */
   /* .description =    "VSX Scalar Convert DP to QP", */
   /* .opcode      =    0xFC160688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvqpsdz, */
   /* .name        =    "xscvqpsdz", */
   /* .description =    "VSX Scalar Convert QP to Signed Dword truncate", */
   /* .opcode      =    0xFC190688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvqpswz, */
   /* .name        =    "xscvqpswz", */
   /* .description =    "VSX Scalar Convert QP to Signed Word truncate", */
   /* .opcode      =    0xFC090688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvqpudz, */
   /* .name        =    "xscvqpudz", */
   /* .description =    "VSX Scalar Convert QP to Unsigned Dword truncate", */
   /* .opcode      =    0xFC110688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvqpuwz, */
   /* .name        =    "xscvqpuwz", */
   /* .description =    "VSX Scalar Convert QP to Unsigned Word truncate", */
   /* .opcode      =    0xFC010688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvsdqp, */
   /* .name        =    "xscvsdqp", */
   /* .description =    "VSX Scalar Convert Signed Dword to QP", */
   /* .opcode      =    0xFC0A0688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvudqp, */
   /* .name        =    "xscvudqp", */
   /* .description =    "VSX Scalar Convert Unsigned Dword to QP", */
   /* .opcode      =    0xFC020688, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsdivqp, */
   /* .name        =    "xsdivqp", */
   /* .description =    "VSX Scalar Divide QP", */
   /* .opcode      =    0xFC000448, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsdivdp,
   /* .name        = */ "xsdivdp",
   /* .description =    "VSX Scalar Divide Double-Precision", */
   /* .opcode      = */ 0xF00001C0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsiexpqp, */
   /* .name        =    "xsiexpqp", */
   /* .description =    "VSX Scalar Insert Exponent QP", */
   /* .opcode      =    0xFC0006C8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmaddqp, */
   /* .name        =    "xsmaddqp", */
   /* .description =    "VSX Scalar Multiply-Add QP", */
   /* .opcode      =    0xFC000308, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmsubqp, */
   /* .name        =    "xsmsubqp", */
   /* .description =    "VSX Scalar Multiply-Subtract QP", */
   /* .opcode      =    0xFC000348, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmulqp, */
   /* .name        =    "xsmulqp", */
   /* .description =    "VSX Scalar Multiply QP", */
   /* .opcode      =    0xFC000048, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmaxcdp, */
   /* .name        =    "xsmaxcdp", */
   /* .description =    "VSX Scalar Maximum Type-C Double-Precision", */
   /* .opcode      =    0xF0000400, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmaxjdp, */
   /* .name        =    "xsmaxjdp", */
   /* .description =    "VSX Scalar Maximum Type-J Double-Precision", */
   /* .opcode      =    0xF0000480, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmaddadp,
   /* .name        = */ "xsmaddadp",
   /* .description =    "VSX Scalar Multiply-Add Type A Double-Precision", */
   /* .opcode      = */ 0xF0000108,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmaddmdp,
   /* .name        = */ "xsmaddmdp",
   /* .description =    "VSX Scalar Multiply-Add Type M Double-Precision", */
   /* .opcode      = */ 0xF0000148,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsmincdp, */
   /* .name        =    "xsmincdp", */
   /* .description =    "VSX Scalar Minimum Type-C Double-Precision", */
   /* .opcode      =    0xF0000440, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsminjdp, */
   /* .name        =    "xsminjdp", */
   /* .description =    "VSX Scalar Minimum Type-J Double-Precision", */
   /* .opcode      =    0xF00004C0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmsubadp,
   /* .name        = */ "xsmsubadp",
   /* .description =    "VSX Scalar Multiply-Subtract Type A Double-Precision", */
   /* .opcode      = */ 0xF0000188,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmsubmdp,
   /* .name        = */ "xsmsubmdp",
   /* .description =    "VSX Scalar Multiply-Subtract Type M Double-Precision", */
   /* .opcode      = */ 0xF00001C8,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmuldp,
   /* .name        = */ "xsmuldp",
   /* .description =    "VSX Scalar Multiply Double-Precision", */
   /* .opcode      = */ 0xF0000180,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnabsdp,
   /* .name        = */ "xsnabsdp",
   /* .description =    "VSX Scalar Negative Absolute Value Double-Precision", */
   /* .opcode      = */ 0xF00005A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsnmaddqp, */
   /* .name        =    "xsnmaddqp", */
   /* .description =    "VSX Scalar Negative Multiply-Add QP", */
   /* .opcode      =    0xFC000388, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsnmsubqp, */
   /* .name        =    "xsnmsubqp", */
   /* .description =    "VSX Scalar Negative Multiply-Subtract QP", */
   /* .opcode      =    0xFC0003C8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnegdp,
   /* .name        = */ "xsnegdp",
   /* .description =    "VSX Scalar Negate Double-Precision", */
   /* .opcode      = */ 0xF00005E4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmaddadp,
   /* .name        = */ "xsnmaddadp",
   /* .description =    "VSX Scalar Negative Multiply-Add Type A Double-Precision", */
   /* .opcode      = */ 0xF0000508,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmaddmdp,
   /* .name        = */ "xsnmaddmdp",
   /* .description =    "VSX Scalar Negative Multiply-Add Type M Double-Precision", */
   /* .opcode      = */ 0xF0000548,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmsubadp,
   /* .name        = */ "xsnmsubadp",
   /* .description =    "VSX Scalar Negative Multiply-Subtract Type A Double-Precision", */
   /* .opcode      = */ 0xF0000588,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmsubmdp,
   /* .name        = */ "xsnmsubmdp",
   /* .description =    "VSX Scalar Negative Multiply-Subtract Type M Double-Precision", */
   /* .opcode      = */ 0xF00005C8,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsnabsqp, */
   /* .name        =    "xsnabsqp", */
   /* .description =    "VSX Scalar Negative Absolute QP", */
   /* .opcode      =    0xFC080648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsnegqp, */
   /* .name        =    "xsnegqp", */
   /* .description =    "VSX Scalar Negate QP", */
   /* .opcode      =    0xFC100648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrdpic,
   /* .name        = */ "xsrdpic",
   /* .description =    "VSX Scalar Round to Double-Precision exact using Current rounding mode", */
   /* .opcode      = */ 0xF00001AC,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsredp,
   /* .name        = */ "xsredp",
   /* .description =    "VSX Scalar Reciprocal Estimate Double-Precision", */
   /* .opcode      = */ 0xF0000168,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrsqrtedp,
   /* .name        = */ "xsrsqrtedp",
   /* .description =    "VSX Scalar Reciprocal Square Root Estimate Double-Precision", */
   /* .opcode      = */ 0xF0000128,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xssqrtqp, */
   /* .name        =    "xssqrtqp", */
   /* .description =    "VSX Scalar Square Root QP", */
   /* .opcode      =    0xFC1B0648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xssubqp, */
   /* .name        =    "xssubqp", */
   /* .description =    "VSX Scalar Subtract QP", */
   /* .opcode      =    0xFC000408, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xststdcqp, */
   /* .name        =    "xststdcqp", */
   /* .description =    "VSX Scalar Test Data Class QP", */
   /* .opcode      =    0xFC000588, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsxsigqp, */
   /* .name        =    "xsxsigqp", */
   /* .description =    "VSX Scalar Extract Significand QP", */
   /* .opcode      =    0xFC120648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsxexpqp, */
   /* .name        =    "xsxexpqp", */
   /* .description =    "VSX Scalar Extract Exponent QP", */
   /* .opcode      =    0xFC020648, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxextractuw, */
   /* .name        =    "xxextractuw", */
   /* .description =    "VSX Vector Extract Unsigned Word", */
   /* .opcode      =    0xF0000294, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxinsertw, */
   /* .name        =    "xxinsertw", */
   /* .description =    "VSX Vector Insert Word", */
   /* .opcode      =    0xF00002D4, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlor,
   /* .name        = */ "xxlor",
   /* .description =    "VSX Logical OR", */
   /* .opcode      = */ 0xF0000490,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlnor,
   /* .name        = */ "xxlnor",
   /* .description =    "VSX Logical NOR", */
   /* .opcode      = */ 0xF0000510,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxsel,
   /* .name        = */ "xxsel",
   /* .description =    "VSX Select", */
   /* .opcode      = */ 0xF0000030,
   /* .format      = */ FORMAT_XT_XA_XB_XC,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxsldwi,
   /* .name        = */ "xxsldwi",
   /* .description =    "VSX Shift Left Double by Word Immediate", */
   /* .opcode      = */ 0xF0000010,
   /* .format      = */ FORMAT_XT_XA_XB_SHW,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxspltw,
   /* .name        = */ "xxspltw",
   /* .description =    "VSX Splat Word", */
   /* .opcode      = */ 0xF0000290,
   /* .format      = */ FORMAT_XT_XB_UIM2,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxspltib, */
   /* .name        =    "xxspltib", */
   /* .description =    "VSX Vector Splat Immediate Byte", */
   /* .opcode      =    0xF00002D0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxperm, */
   /* .name        =    "xxperm", */
   /* .description =    "VSX Vector Permute", */
   /* .opcode      =    0xF00000D0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxpermdi,
   /* .name        = */ "xxpermdi",
   /* .description =    "VSX Permute Doubleword Immediate", */
   /* .opcode      = */ 0xF0000050,
   /* .format      = */ FORMAT_XT_XA_XB_DM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxpermr, */
   /* .name        =    "xxpermr", */
   /* .description =    "VSX Vector Permute Right-indexed", */
   /* .opcode      =    0xF00001D0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xssqrtdp,
   /* .name        = */ "xssqrtdp",
   /* .description =    "VSX Scalar Square Root Double-Precision", */
   /* .opcode      = */ 0xF000012C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xssubdp,
   /* .name        = */ "xssubdp",
   /* .description =    "VSX Scalar Subtract Double-Precision", */
   /* .opcode      = */ 0xF0000140,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpeqdp, */
   /* .name        =    "xscmpeqdp", */
   /* .description =    "VSX Scalar Compare Equal Double-Precision", */
   /* .opcode      =    0xF0000018, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpgtdp, */
   /* .name        =    "xscmpgtdp", */
   /* .description =    "VSX Scalar Compare Greater Than Double-Precision", */
   /* .opcode      =    0xF0000058, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpgedp, */
   /* .name        =    "xscmpgedp", */
   /* .description =    "VSX Scalar Compare Greater Than or Equal Double-Precision", */
   /* .opcode      =    0xF0000098, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpnedp, */
   /* .name        =    "xscmpnedp", */
   /* .description =    "VSX Scalar Compare Not Equal Double-Precision", */
   /* .opcode      =    0xF00000D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscmpexpdp, */
   /* .name        =    "xscmpexpdp", */
   /* .description =    "VSX Scalar Compare Exponents DP", */
   /* .opcode      =    0xF00001D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvsxddp,
   /* .name        = */ "xscvsxddp",
   /* .description =    "VSX Scalar Convert Signed Integer Doubleword to Double-Precision", */
   /* .opcode      = */ 0xF00005E0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvdphp, */
   /* .name        =    "xscvdphp", */
   /* .description =    "VSX Scalar Convert DP to HP", */
   /* .opcode      =    0xF011056C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_DoubleFP | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpsp,
   /* .name        = */ "xscvdpsp",
   /* .description =    "VSX Scalar Convert From Double-Precision to Single-Precision", */
   /* .opcode      = */ 0xF0000424,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_AltFormat |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xscvhpdp, */
   /* .name        =    "xscvhpdp", */
   /* .description =    "VSX Scalar Convert HP to DP", */
   /* .opcode      =    0xF010056C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsiexpdp, */
   /* .name        =    "xsiexpdp", */
   /* .description =    "VSX Scalar Insert Exponent DP", */
   /* .opcode      =    0xF000072C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xststdcdp, */
   /* .name        =    "xststdcdp", */
   /* .description =    "VSX Scalar Test Data Class DP", */
   /* .opcode      =    0xF00005A8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_DoubleFP | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xststdcsp, */
   /* .name        =    "xststdcsp", */
   /* .description =    "VSX Scalar Test Data Class SP", */
   /* .opcode      =    0xF00004A8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SingleFP | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsxexpdp, */
   /* .name        =    "xsxexpdp", */
   /* .description =    "VSX Scalar Extract Exponent DP", */
   /* .opcode      =    0xF000056C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xsxsigdp, */
   /* .name        =    "xsxsigdp", */
   /* .description =    "VSX Scalar Extract Significand DP", */
   /* .opcode      =    0xF001056C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvadddp,
   /* .name        = */ "xvadddp",
   /* .description =    "VSX Vector Add Double-Precision", */
   /* .opcode      = */ 0xF0000300,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcmpnesp, */
   /* .name        =    "xvcmpnesp", */
   /* .description =    "VSX Vector Compare Not Equal Single-Precision", */
   /* .opcode      =    0xF00002D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcmpnesp_r, */
   /* .name        =    "xvcmpnesp.", */
   /* .description =    "VSX Vector Compare Not Equal Single-Precision Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpnesp].opcode + 1, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpnesp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcmpnedp, */
   /* .name        =    "xvcmpnedp", */
   /* .description =    "VSX Vector Compare Not Equal Double-Precision", */
   /* .opcode      =    0xF00003D8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    PPCOpProp_HasRecordForm | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_CompareOp, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcmpnedp_r, */
   /* .name        =    "xvcmpnedp.", */
   /* .description =    "VSX Vector Compare Not Equal Double-Precision Rc=1", */
   /* .opcode      =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpnedp].opcode + 1, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_DefaultPPCProcessor, */
   /* .properties  =    OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpnedp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmindp,
   /* .name        = */ "xvmindp",
   /* .description =    "VSX Vector Min Double-Precision", */
   /* .opcode      = */ 0xF0000740,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaxdp,
   /* .name        = */ "xvmaxdp",
   /* .description =    "VSX Vector Max Double-Precision", */
   /* .opcode      = */ 0xF0000700,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpeqdp,
   /* .name        = */ "xvcmpeqdp",
   /* .description =    "VSX Vector Compare Equal To Double-Precision", */
   /* .opcode      = */ 0xF0000318,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpeqdp_r,
   /* .name        = */ "xvcmpeqdp.",
   /* .description =    "VSX Vector Compare Equal To Double-Precision", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqdp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqdp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqdp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqdp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgedp,
   /* .name        = */ "xvcmpgedp",
   /* .description =    "VSX Vector Compare Greater Than Or Equal To Double-Precision", */
   /* .opcode      = */ 0xF0000398,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgedp_r,
   /* .name        = */ "xvcmpgedp.",
   /* .description =    "VSX Vector Compare Greater Than Or Equal To Double-Precision", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgedp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgedp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgedp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgedp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgtdp,
   /* .name        = */ "xvcmpgtdp",
   /* .description =    "VSX Vector Compare Greater Than Double-Precision", */
   /* .opcode      = */ 0xF0000358,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgtdp_r,
   /* .name        = */ "xvcmpgtdp.",
   /* .description =    "VSX Vector Compare Greater Than Double-Precision", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtdp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtdp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtdp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtdp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcvhpsp, */
   /* .name        =    "xvcvhpsp", */
   /* .description =    "VSX Vector Convert HP to SP", */
   /* .opcode      =    0xF018076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvcvsphp, */
   /* .name        =    "xvcvsphp", */
   /* .description =    "VSX Vector Convert SP to HP", */
   /* .opcode      =    0xF019076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_AltFormat | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvdivdp,
   /* .name        = */ "xvdivdp",
   /* .description =    "VSX Vector Divide Double-Precision", */
   /* .opcode      = */ 0xF00003C0,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xviexpsp, */
   /* .name        =    "xviexpsp", */
   /* .description =    "VSX Vector Insert Exponent SP", */
   /* .opcode      =    0xF00006C0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xviexpdp, */
   /* .name        =    "xviexpdp", */
   /* .description =    "VSX Vector Insert Exponent DP", */
   /* .opcode      =    0xF00007C0, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmuldp,
   /* .name        = */ "xvmuldp",
   /* .description =    "VSX Vector Multiply Double-Precision", */
   /* .opcode      = */ 0xF0000380,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnegdp,
   /* .name        = */ "xvnegdp",
   /* .description =    "VSX Vector Negate Double-Precision", */
   /* .opcode      = */ 0xF00007E4,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmsubadp,
   /* .name        = */ "xvnmsubadp",
   /* .description =    "VSX Vector Negative Multiply-Subtract Type-A Double-Precision", */
   /* .opcode      = */ 0xF0000788,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmsubmdp,
   /* .name        = */ "xvnmsubmdp",
   /* .description =    "VSX Vector Negative Multiply-Subtract Type-M Double-Precision", */
   /* .opcode      = */ 0xF00007C8,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvsubdp,
   /* .name        = */ "xvsubdp",
   /* .description =    "VSX Vector Subtract Double-Precision", */
   /* .opcode      = */ 0xF0000340,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaddadp,
   /* .name        = */ "xvmaddadp",
   /* .description =    "VSX Vector Multiply-Add Type-A Double-Precision", */
   /* .opcode      = */ 0xF0000308,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaddmdp,
   /* .name        = */ "xvmaddmdp",
   /* .description =    "VSX Vector Multiply-Add Type-M Double-Precision", */
   /* .opcode      = */ 0xF0000348,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmsubadp,
   /* .name        = */ "xvmsubadp",
   /* .description =    "VSX Vector Multiply-Sub Type-A Double-Precision", */
   /* .opcode      = */ 0xF0000388,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmsubmdp,
   /* .name        = */ "xvmsubmdp",
   /* .description =    "VSX Vector Multiply-Sub Type-M Double-Precision", */
   /* .opcode      = */ 0xF00003C8,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_UsesTarget,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvsqrtdp,
   /* .name        = */ "xvsqrtdp",
   /* .description =    "VSX Vector Square Root Double-Precision", */
   /* .opcode      = */ 0xF000032C,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_DoubleFP |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvtstdcsp, */
   /* .name        =    "xvtstdcsp", */
   /* .description =    "VSX Vector Test Data Class SP", */
   /* .opcode      =    0xF00006A8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_SingleFP | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvtstdcdp, */
   /* .name        =    "xvtstdcdp", */
   /* .description =    "VSX Vector Test Data Class DP", */
   /* .opcode      =    0xF00007A8, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_DoubleFP | */
   /*                   PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvxexpdp, */
   /* .name        =    "xvxexpdp", */
   /* .description =    "VSX Vector Extract Exponent DP", */
   /* .opcode      =    0xF000076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvxexpsp, */
   /* .name        =    "xvxexpsp", */
   /* .description =    "VSX Vector Extract Exponent SP", */
   /* .opcode      =    0xF008076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvxsigdp, */
   /* .name        =    "xvxsigdp", */
   /* .description =    "VSX Vector Extract Significand DP", */
   /* .opcode      =    0xF001076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xvxsigsp, */
   /* .name        =    "xvxsigsp", */
   /* .description =    "VSX Vector Extract Significand SP", */
   /* .opcode      =    0xF009076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxbrd, */
   /* .name        =    "xxbrd", */
   /* .description =    "VSX Vector Byte-Reverse Dword", */
   /* .opcode      =    0xF017076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxbrh, */
   /* .name        =    "xxbrh", */
   /* .description =    "VSX Vector Byte-Reverse Hword", */
   /* .opcode      =    0xF007076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxbrw, */
   /* .name        =    "xxbrw", */
   /* .description =    "VSX Vector Byte-Reverse Word", */
   /* .opcode      =    0xF00F076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   /* { */
   /* .mnemonic    =    OMR::InstOpCode::xxbrq, */
   /* .name        =    "xxbrq", */
   /* .description =    "VSX Vector Byte-Reverse Qword", */
   /* .opcode      =    0xF01F076C, */
   /* .format      =    FORMAT_UNKNOWN, */
   /* .minimumALS  =    TR_Processor::TR_PPCp9, */
   /* .properties  =    PPCOpProp_IsVSX | */
   /*                   PPCOpProp_SyncSideEffectFree | */
   /*                   PPCOpProp_AltFormat, */
   /* }, */

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmrgew,
   /* .name        = */ "fmrgew",
   /* .description =    "Merge Even Word", */
   /* .opcode      = */ 0xFC00078C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::fmrgow,
   /* .name        = */ "fmrgow",
   /* .description =    "Merge Odd Word", */
   /* .opcode      = */ 0xFC00068C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxsiwax,
   /* .name        = */ "lxsiwax",
   /* .description =    "VSX Scalar as Integer Word Algebraic Indexed", */
   /* .opcode      = */ 0x7C000098,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxsiwzx,
   /* .name        = */ "lxsiwzx",
   /* .description =    "VSX Scalar as Integer Word and Zero Indexed", */
   /* .opcode      = */ 0x7C000018,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::lxsspx,
   /* .name        = */ "lxsspx",
   /* .description =    "VSX Scalar Single-Precision Indexed", */
   /* .opcode      = */ 0x7C000418,
   /* .format      = */ FORMAT_XT_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_IsLoad,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stxsiwx,
   /* .name        = */ "stxsiwx",
   /* .description =    "VSX Scalar as Integer Word Indexed", */
   /* .opcode      = */ 0x7C000118,
   /* .format      = */ FORMAT_XS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::stxsspx,
   /* .name        = */ "stxsspx",
   /* .description =    "VSR Scalar Word Indexed", */
   /* .opcode      = */ 0x7C000518,
   /* .format      = */ FORMAT_XS_RA_RB_MEM,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_IsStore,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsaddsp,
   /* .name        = */ "xsaddsp",
   /* .description =    "Scalar Add Single-Precision", */
   /* .opcode      = */ 0xF0000000,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpspn,
   /* .name        = */ "xscvdpspn",
   /* .description =    "Scalar Convert Double-Precision to Single-Precision format Non-signalling", */
   /* .opcode      = */ 0xF000042C,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpuxds,
   /* .name        = */ "xscvdpuxds",
   /* .description =    "Scalar Convert Double-Precision to Unsigned Fixed-Point Doubleword Saturate", */
   /* .opcode      = */ 0xF0000520,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvdpuxws,
   /* .name        = */ "xscvdpuxws",
   /* .description =    "Scalar Convert Double-Precision to Unsigned Fixed-Point Word Saturate", */
   /* .opcode      = */ 0xF0000120,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvspdp,
   /* .name        = */ "xscvspdp",
   /* .description =    "Scalar Convert Single-Precision to Double-Precision (p=1)", */
   /* .opcode      = */ 0xF0000524,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvspdpn,
   /* .name        = */ "xscvspdpn",
   /* .description =    "Convert Single-Precision to Double-Precision format Non-signalling", */
   /* .opcode      = */ 0xF000052C,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvsxdsp,
   /* .name        = */ "xscvsxdsp",
   /* .description =    "Scalar Convert Signed Fixed-Point Doubleword to Single-Precision", */
   /* .opcode      = */ 0xF00004E0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvuxddp,
   /* .name        = */ "xscvuxddp",
   /* .description =    "Scalar Convert Unsigned Fixed-Point Doubleword to Double-Precision", */
   /* .opcode      = */ 0xF00005A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xscvuxdsp,
   /* .name        = */ "xscvuxdsp",
   /* .description =    "Scalar Convert Unsigned Fixed-Point Doubleword to Single-Precision", */
   /* .opcode      = */ 0xF00004A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsdivsp,
   /* .name        = */ "xsdivsp",
   /* .description =    "Scalar Divide Single-Precision", */
   /* .opcode      = */ 0xF00000C0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmaddasp,
   /* .name        = */ "xsmaddasp",
   /* .description =    "Scalar Multiply-Add Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000008,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmaddmsp,
   /* .name        = */ "xsmaddmsp",
   /* .description =    "Scalar Multiply-Add Type-M Single-Precision", */
   /* .opcode      = */ 0xF0000048,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmaxdp,
   /* .name        = */ "xsmaxdp",
   /* .description =    "Scalar Maximum Double-Precision", */
   /* .opcode      = */ 0xF0000500,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmindp,
   /* .name        = */ "xsmindp",
   /* .description =    "Scalar Minimum Double-Precision", */
   /* .opcode      = */ 0xF0000540,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmsubasp,
   /* .name        = */ "xsmsubasp",
   /* .description =    "Scalar Multiply-Subtract Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000088,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmsubmsp,
   /* .name        = */ "xsmsubmsp",
   /* .description =    "Scalar Multiply-Subtract Type-M Single-Precision", */
   /* .opcode      = */ 0xF00000C8,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsmulsp,
   /* .name        = */ "xsmulsp",
   /* .description =    "Scalar Multiply Single-Precision", */
   /* .opcode      = */ 0xF0000080,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmaddasp,
   /* .name        = */ "xsnmaddasp",
   /* .description =    "Scalar Negative Multiply-Add Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000408,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmaddmsp,
   /* .name        = */ "xsnmaddmsp",
   /* .description =    "Scalar Negative Multiply-Add Type-M Single-Precision", */
   /* .opcode      = */ 0xF0000448,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmsubasp,
   /* .name        = */ "xsnmsubasp",
   /* .description =    "Scalar Negative Multiply-Subtract Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000488,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsnmsubmsp,
   /* .name        = */ "xsnmsubmsp",
   /* .description =    "Scalar Negative Multiply-Subtract Type-M Single-Precision", */
   /* .opcode      = */ 0xF00004C8,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrdpi,
   /* .name        = */ "xsrdpi",
   /* .description =    "Scalar Round to Double-Precision Integer", */
   /* .opcode      = */ 0xF0000124,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrdpim,
   /* .name        = */ "xsrdpim",
   /* .description =    "Scalar Round to Double-Precision Integer toward -Infinity", */
   /* .opcode      = */ 0xF00001E4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrdpip,
   /* .name        = */ "xsrdpip",
   /* .description =    "Scalar Round to Double-Precision Integer toward +Infinity", */
   /* .opcode      = */ 0xF00001A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrdpiz,
   /* .name        = */ "xsrdpiz",
   /* .description =    "Scalar Round to Double-Precision Integer toward Zero", */
   /* .opcode      = */ 0xF0000164,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsresp,
   /* .name        = */ "xsresp",
   /* .description =    "Scalar Reciprocal Estimate Single-Precision", */
   /* .opcode      = */ 0xF0000068,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrsp,
   /* .name        = */ "xsrsp",
   /* .description =    "Scalar Round to Single-Precision", */
   /* .opcode      = */ 0xF0000464,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xsrsqrtesp,
   /* .name        = */ "xsrsqrtesp",
   /* .description =    "Scalar Reciprocal Square Root Estimate Single-Precision", */
   /* .opcode      = */ 0xF0000028,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xssqrtsp,
   /* .name        = */ "xssqrtsp",
   /* .description =    "Scalar Square Root Single-Precision", */
   /* .opcode      = */ 0xF000002C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xssubsp,
   /* .name        = */ "xssubsp",
   /* .description =    "Scalar Subtract Single-Precision", */
   /* .opcode      = */ 0xF0000040,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxland,
   /* .name        = */ "xxland",
   /* .description =    "Logical AND", */
   /* .opcode      = */ 0xF0000410,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlandc,
   /* .name        = */ "xxlandc",
   /* .description =    "Logical AND with Complement", */
   /* .opcode      = */ 0xF0000450,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxleqv,
   /* .name        = */ "xxleqv",
   /* .description =    "Logical Equivalence", */
   /* .opcode      = */ 0xF00005D0,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlnand,
   /* .name        = */ "xxlnand",
   /* .description =    "Logical NAND", */
   /* .opcode      = */ 0xF0000590,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlorc,
   /* .name        = */ "xxlorc",
   /* .description =    "Logical OR with Complement", */
   /* .opcode      = */ 0xF0000550,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxlxor,
   /* .name        = */ "xxlxor",
   /* .description =    "Logical XOR", */
   /* .opcode      = */ 0xF00004D0,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxmrghw,
   /* .name        = */ "xxmrghw",
   /* .description =    "Merge High Word", */
   /* .opcode      = */ 0xF0000090,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xxmrglw,
   /* .name        = */ "xxmrglw",
   /* .description =    "Merge Low Word", */
   /* .opcode      = */ 0xF0000190,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrgew,
   /* .name        = */ "vmrgew",
   /* .description =    "Merge Even Word", */
   /* .opcode      = */ 0x1000078C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::vmrgow,
   /* .name        = */ "vmrgow",
   /* .description =    "Merge Odd Word", */
   /* .opcode      = */ 0x1000068C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp8,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvabsdp,
   /* .name        = */ "xvabsdp",
   /* .description =    "Vector Absolute Value Double-Precision", */
   /* .opcode      = */ 0xF0000764,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvabssp,
   /* .name        = */ "xvabssp",
   /* .description =    "Vector Absolute Value Single-Precision", */
   /* .opcode      = */ 0xF0000664,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvaddsp,
   /* .name        = */ "xvaddsp",
   /* .description =    "Vector Add Single-Precision", */
   /* .opcode      = */ 0xF0000200,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpeqsp,
   /* .name        = */ "xvcmpeqsp",
   /* .description =    "Vector Compare Equal To Single-Precision", */
   /* .opcode      = */ 0xF0000218,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpeqsp_r,
   /* .name        = */ "xvcmpeqsp.",
   /* .description =    "Vector Compare Equal To Single-Precision & record CR6", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqsp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqsp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqsp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpeqsp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgesp,
   /* .name        = */ "xvcmpgesp",
   /* .description =    "Vector Compare Greater Than or Equal To Single-Precision", */
   /* .opcode      = */ 0xF0000298,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgesp_r,
   /* .name        = */ "xvcmpgesp.",
   /* .description =    "Vector Compare Greater Than or Equal To Single-Precision & record CR6", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgesp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgesp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgesp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgesp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgtsp,
   /* .name        = */ "xvcmpgtsp",
   /* .description =    "Vector Compare Greater Than Single-Precision", */
   /* .opcode      = */ 0xF0000258,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_HasRecordForm |
                        PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_CompareOp,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcmpgtsp_r,
   /* .name        = */ "xvcmpgtsp.",
   /* .description =    "Vector Compare Greater Than Single-Precision & record CR6", */
   /* .opcode      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtsp].opcode | 0x400,
   /* .format      = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtsp].format,
   /* .minimumALS  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtsp].minimumALS,
   /* .properties  = */ OMR::Power::InstOpCode::metadata[OMR::InstOpCode::xvcmpgtsp].properties & ~PPCOpProp_HasRecordForm | PPCOpProp_IsRecordForm,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcpsgndp,
   /* .name        = */ "xvcpsgndp",
   /* .description =    "Vector Copy Sign Double-Precision", */
   /* .opcode      = */ 0xF0000780,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcpsgnsp,
   /* .name        = */ "xvcpsgnsp",
   /* .description =    "Vector Copy Sign Single-Precision", */
   /* .opcode      = */ 0xF0000680,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvdpsp,
   /* .name        = */ "xvcvdpsp",
   /* .description =    "Vector Convert Double-Precision to Single-Precision", */
   /* .opcode      = */ 0xF0000624,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvdpsxds,
   /* .name        = */ "xvcvdpsxds",
   /* .description =    "Vector Convert Double-Precision to Signed Fixed-Point Doubleword Saturate", */
   /* .opcode      = */ 0xF0000760,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvdpsxws,
   /* .name        = */ "xvcvdpsxws",
   /* .description =    "Vector Convert Double-Precision to Signed Fixed-Point Word Saturate", */
   /* .opcode      = */ 0xF0000360,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvdpuxds,
   /* .name        = */ "xvcvdpuxds",
   /* .description =    "Vector Convert Double-Precision to Unsigned Fixed-Point Doubleword Saturate", */
   /* .opcode      = */ 0xF0000720,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvdpuxws,
   /* .name        = */ "xvcvdpuxws",
   /* .description =    "Vector Convert Double-Precision to Unsigned Fixed-Point Word Saturate", */
   /* .opcode      = */ 0xF0000320,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvspdp,
   /* .name        = */ "xvcvspdp",
   /* .description =    "Vector Convert Single-Precision to Double-Precision", */
   /* .opcode      = */ 0xF0000724,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvspsxds,
   /* .name        = */ "xvcvspsxds",
   /* .description =    "Vector Convert Single-Precision to Signed Fixed-Point Doubleword Saturate", */
   /* .opcode      = */ 0xF0000660,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvspsxws,
   /* .name        = */ "xvcvspsxws",
   /* .description =    "Vector Convert Single-Precision to Signed Fixed-Point Word Saturate", */
   /* .opcode      = */ 0xF0000260,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvspuxds,
   /* .name        = */ "xvcvspuxds",
   /* .description =    "Vector Convert Single-Precision to Unsigned Fixed-Point Doubleword Saturate", */
   /* .opcode      = */ 0xF0000620,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvspuxws,
   /* .name        = */ "xvcvspuxws",
   /* .description =    "Vector Convert Single-Precision to Unsigned Fixed-Point Word Saturate", */
   /* .opcode      = */ 0xF0000220,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvsxddp,
   /* .name        = */ "xvcvsxddp",
   /* .description =    "Vector Convert Signed Fixed-Point Doubleword to Double-Precision", */
   /* .opcode      = */ 0xF00007E0,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvsxdsp,
   /* .name        = */ "xvcvsxdsp",
   /* .description =    "Vector Convert Signed Fixed-Point Doubleword to Single-Precision", */
   /* .opcode      = */ 0xF00006E0,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvsxwdp,
   /* .name        = */ "xvcvsxwdp",
   /* .description =    "Vector Convert Signed Fixed-Point Word to Double-Precision", */
   /* .opcode      = */ 0xF00003E0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvsxwsp,
   /* .name        = */ "xvcvsxwsp",
   /* .description =    "Vector Convert Signed Fixed-Point Word to Single-Precision", */
   /* .opcode      = */ 0xF00002E0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvuxddp,
   /* .name        = */ "xvcvuxddp",
   /* .description =    "Vector Convert Unsigned Fixed-Point Doubleword to Double-Precision", */
   /* .opcode      = */ 0xF00007A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvuxdsp,
   /* .name        = */ "xvcvuxdsp",
   /* .description =    "Vector Convert Unsigned Fixed-Point Doubleword to Single-Precision", */
   /* .opcode      = */ 0xF00006A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvuxwdp,
   /* .name        = */ "xvcvuxwdp",
   /* .description =    "Vector Convert Unsigned Fixed-Point Word to Double-Precision", */
   /* .opcode      = */ 0xF00003A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvcvuxwsp,
   /* .name        = */ "xvcvuxwsp",
   /* .description =    "Vector Convert Unsigned Fixed-Point Word to Single-Precision", */
   /* .opcode      = */ 0xF00002A0,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvdivsp,
   /* .name        = */ "xvdivsp",
   /* .description =    "Vector Divide Single-Precision", */
   /* .opcode      = */ 0xF00002C0,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaddasp,
   /* .name        = */ "xvmaddasp",
   /* .description =    "Vector Multiply-Add Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000208,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaddmsp,
   /* .name        = */ "xvmaddmsp",
   /* .description =    "Vector Multiply-Add Type-M Single-Precision", */
   /* .opcode      = */ 0xF0000248,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmaxsp,
   /* .name        = */ "xvmaxsp",
   /* .description =    "Vector Maximum Single-Precision", */
   /* .opcode      = */ 0xF0000600,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvminsp,
   /* .name        = */ "xvminsp",
   /* .description =    "Vector Minimum Single-Precision", */
   /* .opcode      = */ 0xF0000640,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmsubasp,
   /* .name        = */ "xvmsubasp",
   /* .description =    "Vector Multiply-Subtract Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000288,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmsubmsp,
   /* .name        = */ "xvmsubmsp",
   /* .description =    "Vector Multiply-Subtract Type-M Single-Precision", */
   /* .opcode      = */ 0xF00002C8,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvmulsp,
   /* .name        = */ "xvmulsp",
   /* .description =    "Vector Multiply Single-Precision", */
   /* .opcode      = */ 0xF0000280,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnabsdp,
   /* .name        = */ "xvnabsdp",
   /* .description =    "Vector Negative Absolute Value Double-Precision", */
   /* .opcode      = */ 0xF00007A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnabssp,
   /* .name        = */ "xvnabssp",
   /* .description =    "Vector Negative Absolute Value Single-Precision", */
   /* .opcode      = */ 0xF00006A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnegsp,
   /* .name        = */ "xvnegsp",
   /* .description =    "Vector Negate Single-Precision", */
   /* .opcode      = */ 0xF00006E4,
   /* .format      = */ FORMAT_XT_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmaddadp,
   /* .name        = */ "xvnmaddadp",
   /* .description =    "Vector Negative Multiply-Add Type-A Double-Precision", */
   /* .opcode      = */ 0xF0000708,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmaddasp,
   /* .name        = */ "xvnmaddasp",
   /* .description =    "Vector Negative Multiply-Add Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000608,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmaddmdp,
   /* .name        = */ "xvnmaddmdp",
   /* .description =    "Vector Negative Multiply-Add Type-M Double-Precision", */
   /* .opcode      = */ 0xF0000748,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmaddmsp,
   /* .name        = */ "xvnmaddmsp",
   /* .description =    "Vector Negative Multiply-Add Type-M Single-Precision", */
   /* .opcode      = */ 0xF0000648,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmsubasp,
   /* .name        = */ "xvnmsubasp",
   /* .description =    "Vector Negative Multiply-Subtract Type-A Single-Precision", */
   /* .opcode      = */ 0xF0000688,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvnmsubmsp,
   /* .name        = */ "xvnmsubmsp",
   /* .description =    "Vector Negative Multiply-Subtract Type-M Single-Precision", */
   /* .opcode      = */ 0xF00006C8,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrdpi,
   /* .name        = */ "xvrdpi",
   /* .description =    "Vector Round to Double-Precision Integer", */
   /* .opcode      = */ 0xF0000324,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrdpic,
   /* .name        = */ "xvrdpic",
   /* .description =    "Vector Round to Double-Precision Integer using Current rounding mode", */
   /* .opcode      = */ 0xF00003AC,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrdpim,
   /* .name        = */ "xvrdpim",
   /* .description =    "Vector Round to Double-Precision Integer toward -Infinity", */
   /* .opcode      = */ 0xF00003E4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrdpip,
   /* .name        = */ "xvrdpip",
   /* .description =    "Vector Round to Double-Precision Integer toward +Infinity", */
   /* .opcode      = */ 0xF00003A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrdpiz,
   /* .name        = */ "xvrdpiz",
   /* .description =    "Vector Round to Double-Precision Integer toward Zero", */
   /* .opcode      = */ 0xF0000364,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvredp,
   /* .name        = */ "xvredp",
   /* .description =    "Vector Reciprocal Estimate Double-Precision", */
   /* .opcode      = */ 0xF0000368,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvresp,
   /* .name        = */ "xvresp",
   /* .description =    "Vector Reciprocal Estimate Single-Precision", */
   /* .opcode      = */ 0xF0000268,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrspi,
   /* .name        = */ "xvrspi",
   /* .description =    "Vector Round to Single-Precision Integer", */
   /* .opcode      = */ 0xF0000224,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrspic,
   /* .name        = */ "xvrspic",
   /* .description =    "Vector Round to Single-Precision Integer using Current rounding mode", */
   /* .opcode      = */ 0xF00002AC,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrspim,
   /* .name        = */ "xvrspim",
   /* .description =    "Vector Round to Single-Precision Integer toward -Infinity", */
   /* .opcode      = */ 0xF00002E4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrspip,
   /* .name        = */ "xvrspip",
   /* .description =    "Vector Round to Single-Precision Integer toward +Infinity", */
   /* .opcode      = */ 0xF00002A4,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrspiz,
   /* .name        = */ "xvrspiz",
   /* .description =    "Vector Round to Single-Precision Integer toward Zero", */
   /* .opcode      = */ 0xF0000264,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrsqrtedp,
   /* .name        = */ "xvrsqrtedp",
   /* .description =    "Vector Reciprocal Square Root Estimate Double-Precision", */
   /* .opcode      = */ 0xF0000328,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_DoubleFP |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvrsqrtesp,
   /* .name        = */ "xvrsqrtesp",
   /* .description =    "Vector Reciprocal Square Root Estimate Single-Precision", */
   /* .opcode      = */ 0xF0000228,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvsqrtsp,
   /* .name        = */ "xvsqrtsp",
   /* .description =    "Vector Square Root Single-Precision", */
   /* .opcode      = */ 0xF000022C,
   /* .format      = */ FORMAT_UNKNOWN,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree |
                        PPCOpProp_AltFormat,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::xvsubsp,
   /* .name        = */ "xvsubsp",
   /* .description =    "Vector Subtract Single-Precision", */
   /* .opcode      = */ 0xF0000240,
   /* .format      = */ FORMAT_XT_XA_XB,
   /* .minimumALS  = */ TR_Processor::TR_PPCp7,
   /* .properties  = */ PPCOpProp_IsVSX |
                        PPCOpProp_SyncSideEffectFree,
   },

   {
   /* .mnemonic    = */ OMR::InstOpCode::label,
   /* .name        = */ "label",
   /* .description =    "Destination of a jump", */
   /* .opcode      = */ 0x00000000,
   /* .format      = */ FORMAT_NONE,
   /* .minimumALS  = */ TR_Processor::TR_DefaultPPCProcessor,
   /* .properties  = */ PPCOpProp_None,
   },

