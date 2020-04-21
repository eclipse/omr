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

#ifndef TR_PPCOPSDEFINES_INCL
#define TR_PPCOPSDEFINES_INCL

enum PPCInstructionFormat : uint8_t
{

// Placeholder format for instructions that should not appear in the instruction stream during the
// binary encoding phase.
FORMAT_UNKNOWN,

// Format for pseudoinstructions that don't emit any actual instructions.
FORMAT_NONE,

// Format for instructions whose binary encoding is copied directly into the instruction stream
// without filling in any fields.
FORMAT_DIRECT,

// Format for the dd instruction, which is simply a single word placed into the instruction stream.
FORMAT_DD,

// IMPORTANT: The diagrams for instruction fields below only include fields which are filled in by
// the binary encoder. Some fields are filled in by the instruction opcode itself, as defined in
// OMRInstOpCodeProperties.hpp, but they are omitted as they are generally not important to the
// format of an opcode.

// Note also that the formats of instructions with condition register fields here may not exactly
// match what appears in the Power ISA, as the bottom two bits generally indicate which field in a
// CCR should be used. These bits are often part of the instruction itself, since we generally use
// the extended mnemonics.

// Format for I-Form instructions (see Power ISA):
//
// +------+-------------------------------------------------+-----+
// |      | LI                                              |     |
// | 0    | 6                                               | 30  |
// +------+-------------------------------------------------+-----+
FORMAT_I_FORM,

// Format for B-Form instructions (see Power ISA):
//
// +------+-----+-----+-----+-------------------------------+-----+
// |      | BO  | BI  |     | BD                            |     |
// | 0    | 6   | 11  | 14  | 16                            | 30  |
// +------+-----+-----+-----+-------------------------------+-----+
FORMAT_B_FORM,

// Format for XL-Form conditional branch instructions, with the BI field controlled by the condition
// register and the BO field controlled by the branch hint:
//
// +------+-----+-----+-------------------------------------------+
// |      | BO  | BI  |                                           |
// | 0    | 6   | 11  | 14                                        |
// +------+-----+-----+-------------------------------------------+
FORMAT_XL_FORM_BRANCH,

// Format for the mtfsfi X-form instruction, with the U field controlled by imm1 and the BF and W
// fields controlled by imm2:
//
// +------+-----+-----+----+-----+--------------------------------+
// |      | BF  |     | W  | U   |                                |
// | 0    | 6   | 9   | 15 | 16  | 20                             |
// +------+-----+-----+----+-----+--------------------------------+
FORMAT_MTFSFI,

// Format for the mtfsf XFL-form instruction, with the FRB field encoding the source register and
// the FLM field controlled by the unsigned immediate:
//
// +------+---------+-----+-------+-------------------------------+
// |      | FLM     |     | FRB   |                               |
// | 0    | 7       | 15  | 16    | 21                            |
// +------+---------+-----+-------+-------------------------------+
FORMAT_MTFSF,

// Format for instructions with an RS field encoding the source register:
//
// +------+-------+-----------------------------------------------+
// |      | RS    |                                               |
// | 0    | 6     | 11                                            |
// +------+-------+-----------------------------------------------+
FORMAT_RS,

// Format for instructions with an RA field encoding the source register and a 16-bit SI field
// encoding the signed immediate:
//
// +-----------------+----------+---------------------------------+
// |                 | RA       | SI                              |
// | 0               | 11       | 16                              |
// +-----------------+----------+---------------------------------+
FORMAT_RA_SI16,

// Format for instructions with an RA field encoding the source register and a 5-bit SI field
// encoding the signed immediate:
//
// +-----------------+----------+----------+----------------------+
// |                 | RA       | SI       |                      |
// | 0               | 11       | 16       | 21                   |
// +-----------------+----------+----------+----------------------+
FORMAT_RA_SI5,

// Format for instructions with an RS field encoding the source register and a FXM field encoding an
// unsigned immediate mask:
//
// +------+----------+----+----------+----------------------------+
// |      | RT       |    | FXM      |                            |
// | 0    | 6        | 11 | 12       | 20                         |
// +------+----------+----+----------+----------------------------+
//
// Note: The "1" variant of this format asserts that only one bit in FXM should be set.
FORMAT_RS_FXM,
FORMAT_RS_FXM1,

// Format for instructions with an RT field encoding the target register:
//
// +------+-------+-----------------------------------------------+
// |      | RT    |                                               |
// | 0    | 6     | 11                                            |
// +------+-------+-----------------------------------------------+
FORMAT_RT,

// Format for instructions with an RA field encoding the target register and an RS field encoding
// the only source register:
//
// +------+----------+----------+---------------------------------+
// |      | RS       | RA       |                                 |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_RA_RS,

// Format for instructions with an RT field encoding the target register and an RA field encoding
// the only source register:
//
// +------+----------+----------+---------------------------------+
// |      | RT       | RA       |                                 |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_RT_RA,

// Format for instructions with an FRT field encoding the target FP register and an FRB field
// encoding the only source FP register:
//
// +------+----------+----------+----------+----------------------+
// |      | FRT      |          | FRB      |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_FRT_FRB,

// Format for instructions with a BF field encoding the target CC register and a BFA field encoding
// the only source CC register:
//
// +------+-----+-----+-----+-------------------------------------+
// |      | BF  |     | BFA |                                     |
// | 0    | 6   | 9   | 11  | 14                                  |
// +------+-----+-----+-----+-------------------------------------+
FORMAT_BF_BFA,

// Format for instructions with an RA field encoding the target register and an XS field encoding
// the only VSX source register:
//
// +------+----------+----------+----------------------------+----+
// |      | XS       | RA       |                            | XS |
// | 0    | 6        | 11       | 16                         | 31 |
// +------+----------+----------+----------------------------+----+
FORMAT_RA_XS,

// Format for instructions with an XT field encoding the target VSX target register and an RA field
// encoding the only source register:
//
// +------+----------+----------+----------------------------+----+
// |      | XT       | RA       |                            | XT |
// | 0    | 6        | 11       | 16                         | 31 |
// +------+----------+----------+----------------------------+----+
FORMAT_XT_RA,

// Format for instructions with an RT field encoding the target register and an BFA field encoding
// the only source CC register:
//
// +------+----------+-----+--------------------------------------+
// |      | RT       | BFA |                                      |
// | 0    | 6        | 11  | 14                                   |
// +------+----------+-----+--------------------------------------+
FORMAT_RT_BFA,

// Format for instructions with an VRT field encoding the target vector register and a VRB field
// encoding the only source vector register:
//
// +------+----------+----------+----------+----------------------+
// |      | VRT      |          | VRB      |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_VRT_VRB,

// Format for instructions with an XT field encoding the target VSX register and an XB field
// encoding the only source VSX register:
//
// +------+----------+----------+----------+------------+----+----+
// |      | XT       |          | XB       |            | XB | XT |
// | 0    | 6        | 11       | 16       | 21         | 30 | 31 |
// +------+----------+----------+----------+------------+----+----+
FORMAT_XT_XB,

// Format for instructions with an RT field encoding the target register and RA and RB fields
// encoding the source registers:
//
// +------+----------+----------+----------+----------------------+
// |      | RT       | RA       | RB       |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_RT_RA_RB,

// Format for instructions with an RA field encoding the target register and RS and RB fields
// encoding the source registers:
//
// +------+----------+----------+----------+----------------------+
// |      | RS       | RA       | RB       |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_RA_RS_RB,

// Format for instructions with a BF field encoding the target CC register and RA and RB fields
// encoding the source registers:
//
// +------+-----+----+----------+----------+----------------------+
// |      | BF  |    | RA       | RB       |                      |
// | 0    | 6   | 9  | 11       | 16       | 21                   |
// +------+-----+----+----------+----------+----------------------+
FORMAT_BF_RA_RB,

// Format for instructions with a BF field encoding the target CC register and FRA and FRB fields
// encoding the source FP registers:
//
// +------+-----+----+----------+----------+----------------------+
// |      | BF  |    | FRA      | FRB      |                      |
// | 0    | 6   | 9  | 11       | 16       | 21                   |
// +------+-----+----+----------+----------+----------------------+
FORMAT_BF_FRA_FRB,

// Format for instructions with an FRT field encoding the target FP register and FRA and FRB fields
// encoding the source registers:
//
// +------+----------+----------+----------+----------------------+
// |      | FRT      | FRA      | FRB      |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_FRT_FRA_FRB,

// Format for instructions with an RT field encoding the target vector register and RA and RB fields
// encoding the source registers:
//
// +------+----------+----------+----------+----------------------+
// |      | VRT      | RA       | RB       |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_VRT_RA_RB,

// Format for instructions with a VRT field encoding the target vector register and VRA and VRB
// fields encoding the source registers:
//
// +------+----------+----------+----------+----------------------+
// |      | VRT      | VRA      | VRB      |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_VRT_VRA_VRB,

// Format for instructions with an XT field encoding the target VSX register and XA and XB fields
// encoding the two source VSX registers:
//
// +------+----------+----------+----------+-------+----+----+----+
// |      | XT       | XA       | XB       |       | XA | XB | XT |
// | 0    | 6        | 11       | 16       | 21    | 29 | 30 | 31 |
// +------+----------+----------+----------+-------+----+----+----+
FORMAT_XT_XA_XB,

// Format for instructions with an FRT field encoding the target FP register and FRA and FRC fields
// encoding the source registers:
//
// +------+----------+----------+-------+----------+--------------+
// |      | FRT      | FRA      |       | FRC      |              |
// | 0    | 6        | 11       | 16    | 21       | 26           |
// +------+----------+----------+-------+----------+--------------+
FORMAT_FRT_FRA_FRC,

// Format for instructions with an RT field encoding the target register, an RA field encoding the
// source register, and a 16-bit SI field encoding a signed immediate:
//
// +------+----------+----------+---------------------------------+
// |      | RT       | RA       | SI                              |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_RT_RA_SI16,

// Format for instructions with an RA field encoding the target register, an RS field encoding the
// source register, and a 16-bit UI field encoding an unsigned immediate:
//
// +------+----------+----------+---------------------------------+
// |      | RS       | RA       | UI                              |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_RA_RS_UI16,

// Format for instructions with a BF field encoding the target CC register, an RA field encoding the
// source register, and a 16-bit SI field encoding a signed immediate:
//
// +------+-----+----+----------+---------------------------------+
// |      | BF  |    | RA       | SI                              |
// | 0    | 6   | 9  | 11       | 16                              |
// +------+-----+----+----------+---------------------------------+
FORMAT_BF_RA_SI16,

// Format for instructions with a BF field encoding the target CC register, an RA field encoding the
// source register, and a 16-bit UI field encoding an unsigned immediate:
//
// +------+-----+----+----------+---------------------------------+
// |      | BF  |    | RA       | UI                              |
// | 0    | 6   | 9  | 11       | 16                              |
// +------+-----+----+----------+---------------------------------+
FORMAT_BF_RA_UI16,

// Format for instructions with a BF field encoding the target CC register, an FRA field encoding
// the source FP register, and a DM field encoding an unsigned immediate mask:
//
// +------+-----+----+----------+--------+------------------------+
// |      | BF  |    | FRA      | DM     |                        |
// | 0    | 6   | 9  | 11       | 16     | 22                     |
// +------+-----+----+----------+--------+------------------------+
FORMAT_BF_FRA_DM,

// Format for instructions with an RA field encoding the target register, an RS field encoding the
// source register, and a 5-bit SH field encoding a shift amount:
//
// +------+----------+----------+----------+----------------------+
// |      | RS       | RA       | SH       |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
FORMAT_RA_RS_SH5,

// Format for instructions with an RA field encoding the target register, an RS field encoding the
// source register, and a 6-bit SH field encoding a shift amount:
//
// +------+----------+----------+----------+------------+----+----+
// |      | RS       | RA       | SH       |            | SH |    |
// | 0    | 6        | 11       | 16       | 21         | 30 | 31 |
// +------+----------+----------+----------+------------+----+----+
FORMAT_RA_RS_SH6,

// Formats for instructions with a VRT field encoding the target vector register, a VRB field
// encoding the source vector register, and a UIM field encoding an unsigned immediate:
//
// +------+----------+----------+----------+----------------------+
// |      | VRT      | UIM      | VRB      |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
//
// Note: The exact size of the usable part of the UIM field differs between instructions, with upper
//       bits beyond the size of the UIM field being undefined. These formats are differentiated
//       only for the purposes of checking that the UIM field is in range.
FORMAT_VRT_VRB_UIM4,
FORMAT_VRT_VRB_UIM3,
FORMAT_VRT_VRB_UIM2,

// Formats for instructions with an XT field encoding the target VSX register, an XB field encoding
// the source VSX register, and a UIM field encoding an unsigned immediate:
//
// +------+----------+----------+----------+------------+----+----+
// |      | XT       | UIM      | XB       |            | XB | XT |
// | 0    | 6        | 11       | 16       | 21         | 30 | 31 |
// +------+----------+----------+----------+------------+----+----+
//
// Note: The exact size of the usable part of the UIM field differs between instructions, with upper
//       bits beyond the size of the UIM field being undefined. These formats are differentiated
//       only for the purposes of checking that the UIM field is in range.
FORMAT_XT_XB_UIM2,

// Format for instructions with an RT field encoding the target register and a 16-bit SI field
// encoding a signed immediate:
//
// +------+----------+----------+---------------------------------+
// |      | RT       |          | SI                              |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_RT_SI16,

// Format for instructions with a BF field encoding the target CC register and a BFA field encoding
// an unsigned immediate:
//
// +------+-----+-----+-----+-------------------------------------+
// |      | BF  |     | BFA |                                     |
// | 0    | 6   | 9   | 11  | 14                                  |
// +------+-----+-----+-----+-------------------------------------+
FORMAT_BF_BFAI,

// Format for instructions with an RT field encoding the target register and a FXM field encoding an
// unsigned immediate mask:
//
// +------+----------+----+----------+----------------------------+
// |      | RT       |    | FXM      |                            |
// | 0    | 6        | 11 | 12       | 20                         |
// +------+----------+----+----------+----------------------------+
//
// Note: The "1" variant of this format asserts that only one bit in FXM should be set.
FORMAT_RT_FXM,
FORMAT_RT_FXM1,

// Formats for instructions with a VRT field encoding the target vector register and an SIM field
// encoding a signed immediate:
//
// +------+----------+----------+---------------------------------+
// |      | VRT      | SIM      |                                 |
// | 0    | 6        | 11       | 16                              |
// +------+----------+----------+---------------------------------+
FORMAT_VRT_SIM,

// Format for rldic-like instructions. The RA field encodes the target register and the RS field
// encodes the source register. The first immediate dictates the value of the SH(6) field. The
// second immediate encodes the mask that should actually end up being used by the instruction: it
// must have a contiguous group of 1 bits from some position MB to 63-SH and 0 bits everywhere
// else.
//
// +------+----------+----------+-------+---------+-----+----+----+
// |      | RS       | RA       | SH    | MB      |     | SH |    |
// | 0    | 6        | 11       | 16    | 21      | 27  | 30 | 31 |
// +------+----------+----------+-------+---------+-----+----+----+
FORMAT_RLDIC,

// Format for rldicl-like instructions. The RA field encodes the target register and the RS field
// encodes the source register. The first immediate dictates the value of the SH(6) field. The
// second immediate encodes the mask that should actually end up being used by the instruction: it
// must have a contiguous group of 1 bits from some position MB to 63 and 0 bits everywhere else.
//
// +------+----------+----------+-------+---------+-----+----+----+
// |      | RS       | RA       | SH    | MB      |     | SH |    |
// | 0    | 6        | 11       | 16    | 21      | 27  | 30 | 31 |
// +------+----------+----------+-------+---------+-----+----+----+
FORMAT_RLDICL,

// Format for rldicr-like instructions. The RA field encodes the target register and the RS field
// encodes the source register. The first immediate dictates the value of the SH(6) field. The
// second immediate encodes the mask that should actually end up being used by the instruction: it
// must have a contiguous group of 1 bits from 0 to some position ME and 0 bits everywhere else.
//
// +------+----------+----------+-------+---------+-----+----+----+
// |      | RS       | RA       | SH    | ME      |     | SH |    |
// | 0    | 6        | 11       | 16    | 21      | 27  | 30 | 31 |
// +------+----------+----------+-------+---------+-----+----+----+
FORMAT_RLDICR,

// Format for rlwinm-like instructions. The RA field encodes the target register and the RS field
// encodes the source register. The first immediate dictates the value of the SH(5) field. The
// second immediate encodes the mask that should actually end up being used by the instruction: it
// must have a contiguous group of 1 bits from some position 32+MB to some position 32+ME and 0
// bits everywhere else.
//
// +------+----------+----------+-------+---------+----------+----+
// |      | RS       | RA       | SH    | MB      | ME       |    |
// | 0    | 6        | 11       | 16    | 21      | 26       | 31 |
// +------+----------+----------+-------+---------+----------+----+
FORMAT_RLWINM,

// Format for instructions with a BF field encoding the target condition register, RA and RB fields
// encoding the source registers, and an L field encoding the 1-bit immediate:
//
// +------+-----+----+----+----------+----------+-----------------+
// |      | BF  |    | L  | RA       | RB       |                 |
// | 0    | 6   | 9  | 10 | 11       | 16       | 21              |
// +------+-----+----+----+----------+----------+-----------------+
//
// TODO This format really shouldn't exist. It's only used by the cmprb instruction, which should
//      really be split into cmprb and cmprb_l instructions to avoid needing to encode the L field
//      as an immediate.
FORMAT_BF_RA_RB_L,

// Format for instructions with a BT field encoding the target condition register bit and BA and BB
// fields encoding the source condition register bits:
//
// +------+----------+----------+----------+----------------------+
// |      | BT       | BA       | BB       |                      |
// | 0    | 6        | 11       | 16       | 21                   |
// +------+----------+----------+----------+----------------------+
//
// TODO Currently, instructions of this type use PPCTrg1Src2ImmInstruction to encode which bits of
//      each condition register should be modified. This is obviously suboptimal, as it does not
//      actually encode the semantics of these instructions. At some point, a new class should be
//      added specifically for CR logical instructions.
FORMAT_BT_BA_BB,

// Format for instructions with an FRT field encoding the target FP register, FRA and FRB encoding
// the source FP registers, and an RMC field encoding the rounding mode specified by the 2-bit
// unsigned immediate:
//
// +------+----------+----------+----------+-----+----------------+
// |      | FRT      | FRA      | FRB      | RMC |                |
// | 0    | 6        | 11       | 16       | 21  | 23             |
// +------+----------+----------+----------+-----+----------------+
FORMAT_FRT_FRA_FRB_RMC,

// Format for rldcl-like instructions. The RA field encodes the target register and the RS and RB
// fields encode the source registers. The immediate encodes the mask that should actually end up
// being used by the instruction: it must have a contiguous group of 1 bits from some position MB
// to 63 and 0 bits everywhere else.
//
// +------+---------+---------+---------+---------+---------------+
// |      | RS      | RA      | RB      | MB      |               |
// | 0    | 6       | 11      | 16      | 21      | 27            |
// +------+---------+---------+---------+---------+---------------+
FORMAT_RLDCL,

// Format for rlwnm-like instructions. The RA field encodes the target register and the RS and RB
// fields encode the source registers. The immediate encodes the mask that should actually end up
// being used by the instruction: it must have a contiguous group of 1 bits from some position
// 32+MB to some position 32+ME and 0 bits everywhere else.
//
// +------+---------+---------+---------+---------+----------+----+
// |      | RS      | RA      | RB      | MB      | ME       |    |
// | 0    | 6       | 11      | 16      | 21      | 26       | 31 |
// +------+---------+---------+---------+---------+----------+----+
FORMAT_RLWNM,

// Format for instructions with a VRT field encoding the target vector register, VRA and VRB fields
// encoding the source vector registers, and an SHB field encoding an unsigned 4-bit immediate:
//
// +------+----------+----------+----------+----+-----+-----------+
// |      | VRT      | VRA      | VRB      |    | SHB |           |
// | 0    | 6        | 11       | 16       | 21 | 22  | 26        |
// +------+----------+----------+----------+----+-----+-----------+
FORMAT_VRT_VRA_VRB_SHB,

// Format for instructions with an XT field encoding the target VSX register, XA and XB fields
// encoding the source VSX registers, and a DM field encoding an unsigned 2-bit immediate:
//
// +-----+-------+-------+-------+----+-----+------+----+----+----+
// |     | XT    | XA    | XB    |    | DM  |      | XA | XB | XT |
// | 0   | 6     | 11    | 16    | 21 | 22  | 24   | 29 | 30 | 31 |
// +-----+-------+-------+-------+----+-----+------+----+----+----+
FORMAT_XT_XA_XB_DM,

// Format for instructions with an XT field encoding the target VSX register, XA and XB fields
// encoding the source VSX registers, and an SHW field encoding an unsigned 2-bit immediate:
//
// +-----+-------+-------+-------+----+-----+------+----+----+----+
// |     | XT    | XA    | XB    |    | SHW |      | XA | XB | XT |
// | 0   | 6     | 11    | 16    | 21 | 22  | 24   | 29 | 30 | 31 |
// +-----+-------+-------+-------+----+-----+------+----+----+----+
FORMAT_XT_XA_XB_SHW,

// Format for instructions with an FRT field encoding the target register, and RA, RB, and RC
// fields encoding the source registers:
//
// +------+--------+--------+--------+--------+-------------------+
// |      | RT     | RA     | RB     | RC     |                   |
// | 0    | 6      | 11     | 16     | 21     | 26                |
// +------+--------+--------+--------+--------+-------------------+
FORMAT_RT_RA_RB_RC,

// Format for instructions with an FRT field encoding the target FP register, and FRA, FRC, and FRB
// fields encoding the source FP registers:
//
// +------+--------+--------+--------+--------+-------------------+
// |      | FRT    | FRA    | FRB    | FRC    |                   |
// | 0    | 6      | 11     | 16     | 21     | 26                |
// +------+--------+--------+--------+--------+-------------------+
FORMAT_FRT_FRA_FRC_FRB,

// Format for instructions with a VRT field encoding the target vector register, and VRA, VRB, and
// VRC fields encoding the source vector registers:
//
// +------+--------+--------+--------+--------+-------------------+
// |      | VRT    | VRA    | VRB    | VRC    |                   |
// | 0    | 6      | 11     | 16     | 21     | 26                |
// +------+--------+--------+--------+--------+-------------------+
FORMAT_VRT_VRA_VRB_VRC,

// Format for instructions with an XT field encoding the target VSX register, and XA, XB, and XC
// fields encoding the source VSX registers:
//
// +------+------+------+------+------+-----+----+----+----+----+
// |      | XT   | XA   | XB   | XC   |     | XC | XA | XB | XT |
// | 0    | 6    | 11   | 16   | 21   | 26  | 28 | 29 | 30 | 31 |
// +------+------+------+------+------+-----+----+----+----+----+
FORMAT_XT_XA_XB_XC,

// Format for instructions with RA and RB fields encoding the source registers:
//
// +-----------------+----------+----------+----------------------+
// |                 | RA       | RB       |                      |
// | 0               | 11       | 16       | 21                   |
// +-----------------+----------+----------+----------------------+
FORMAT_RA_RB

};

#define PPCOpProp_None              0x00000000
#define PPCOpProp_HasRecordForm     0x00000001
#define PPCOpProp_SetsCarryFlag     0x00000002
#define PPCOpProp_SetsOverflowFlag  0x00000004
#define PPCOpProp_ReadsCarryFlag    0x00000008
#define PPCOpProp_TMAbort           0x00000010
#define PPCOpProp_BranchOp          0x00000040
#define PPCOpProp_CRLogical         0x00000080
#define PPCOpProp_DoubleFP          0x00000100
#define PPCOpProp_SingleFP          0x00000200
#define PPCOpProp_UpdateForm        0x00000400
#define PPCOpProp_AltFormat         0x00000800  // use alternate instruction format
#define PPCOpProp_AltFormatx        0x00001000  // use alternate instruction format
#define PPCOpProp_IsRecordForm      0x00002000
#define PPCOpProp_IsLoad            0x00004000
#define PPCOpProp_IsStore           0x00008000
#define PPCOpProp_IsRegCopy         0x00010000
#define PPCOpProp_Trap              0x00020000
#define PPCOpProp_SetsCtr           0x00040000
#define PPCOpProp_UsesCtr           0x00080000
#define PPCOpProp_DWord             0x00100000
#define PPCOpProp_UseMaskEnd        0x00200000  // ME or MB should be encoded
#define PPCOpProp_IsSync            0x00400000
#define PPCOpProp_IsRotateOrShift   0x00800000
#define PPCOpProp_CompareOp         0x01000000
#define PPCOpProp_SetsFPSCR         0x02000000
#define PPCOpProp_ReadsFPSCR        0x04000000
#define PPCOpProp_OffsetRequiresWordAlignment 0x08000000

#define PPCOpProp_BranchLikelyMask            0x00600000
#define PPCOpProp_BranchUnlikelyMask          0x00400000
#define PPCOpProp_BranchLikelyMaskCtr         0x01200000
#define PPCOpProp_BranchUnlikelyMaskCtr       0x01000000
#define PPCOpProp_IsBranchCTR                 0x04000000
#define PPCOpProp_BranchLikely                1
#define PPCOpProp_BranchUnlikely              0

#define PPCOpProp_LoadReserveAtomicUpdate     0x00000000
#define PPCOpProp_LoadReserveExclusiveAccess  0x00000001
#define PPCOpProp_NoHint                      0xFFFFFFFF

#define PPCOpProp_IsVMX                       0x08000000
#define PPCOpProp_SyncSideEffectFree          0x10000000 // syncs redundant if seperated by these types of ops
#define PPCOpProp_IsVSX                       0x20000000
#define PPCOpProp_UsesTarget                  0x40000000

#if defined(TR_TARGET_64BIT)
#define Op_st        std
#define Op_stu       stdu
#define Op_stx       stdx
#define Op_stux      stdux
#define Op_load      ld
#define Op_loadu     ldu
#define Op_loadx     ldx
#define Op_loadux    ldux
#define Op_cmp       cmp8
#define Op_cmpi      cmpi8
#define Op_cmpl      cmpl8
#define Op_cmpli     cmpli8
#define Op_larx      ldarx
#define Op_stcx_r    stdcx_r
#else
#define Op_st        stw
#define Op_stu       stwu
#define Op_stx       stwx
#define Op_stux      stwux
#define Op_load      lwz
#define Op_loadu     lwzu
#define Op_loadx     lwzx
#define Op_loadux    lwzux
#define Op_cmp       cmp4
#define Op_cmpi      cmpi4
#define Op_cmpl      cmpl4
#define Op_cmpli     cmpli4
#define Op_larx      lwarx
#define Op_stcx_r    stwcx_r
#endif

#endif //ifndef TR_PPCOPSDEFINES_INCL
