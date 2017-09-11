<!--
Copyright (c) 2016, 2017 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath 
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# X86 Binary Encoding Scheme - Issue [#425](https://github.com/eclipse/omr/issues/425)

OMR's X86 Binary Encoding Scheme is inspired by Intel's VEX and EVEX prefixes, which compact mandatory prefixes and multi-byte opcode escape codes into 2 bits each.

The following structure is used to hold an instruction:
```c++
struct OpCode_t
{
    uint8_t vex_l : 2;
    uint8_t vex_v : 1;
    uint8_t prefixes : 2;
    uint8_t rex_w : 1;
    uint8_t escape : 2;
    uint8_t opcode;
    uint8_t modrm_opcode : 3;
    uint8_t modrm_form : 2;
    uint8_t immediate_size : 3;
}
```
- vex_l stores information about operand size, required for AVX / AVX-512
- vex_v is whether VEX.vvvv field is in-use when encoding using VEX/EVEX
- prefixes is the instruction's mandatory prefix.
- rex_w is whether REX.W or VEX.W field should be set.
- escape is the instruction's opcode escape.
- opcode is the opcode byte.
- modrm_opcode is the opcode extension in ModR/M byte.
- modrm_form stores information about the format of ModR/M byte, i.e. RM mode, MR mode, containing opcode extension, etc.
- immediate_size is the size of immediate value.

Possible values of each field are below:
```c++
enum TR_OpCodeVEX_L : uint8_t
{
    VEX_L128 = 0x0,
    VEX_L256 = 0x1,
    VEX_L512 = 0x2,
    VEX_L___ = 0x3, // Instruction does not support VEX encoding
};
enum TR_OpCodeVEX_v : uint8_t
{
    VEX_vNONE = 0x0,
    VEX_vReg_ = 0x1,
};
enum TR_InstructionREX_W : uint8_t
{
    REX__ = 0x0,
    REX_W = 0x1,
};
enum TR_OpCodePrefix : uint8_t
{
    PREFIX___ = 0x0,
    PREFIX_66 = 0x1,
    PREFIX_F3 = 0x2,
    PREFIX_F2 = 0x3,
};
enum TR_OpCodeEscape : uint8_t
{
    ESCAPE_____ = 0x0,
    ESCAPE_0F__ = 0x1,
    ESCAPE_0F38 = 0x2,
    ESCAPE_0F3A = 0x3,
};
enum TR_OpCodeModRM : uint8_t
{
    ModRM_NONE = 0x0,
    ModRM_RM__ = 0x1,
    ModRM_MR__ = 0x2,
    ModRM_EXT_ = 0x3,
};
enum TR_OpCodeImmediate : uint8_t
{
    Immediate_0 = 0x0,
    Immediate_1 = 0x1,
    Immediate_2 = 0x2,
    Immediate_4 = 0x3,
    Immediate_8 = 0x4,
    Immediate_S = 0x7,
};
```

## Generate Non-AVX Instruction
1. Generate legacy prefixes according to OpProperties and OpProperties2.
2. Generate prefixes according to prefixes field.
3. Obtain REX prefix from operand and set REX.W according to rex_w field.
3.1 Generate REX prefix if needed
4. Generate opcode escape according to escape field.
5. Write opcode
6. Set and write ModR/M field if necessary

## Generate AVX Instruction
1. Obtain REX prefix from operand and set REX.W according to rex_w field.
2. Setup 3-byte VEX structure.
2.1 Convert the 3-byte VEX to 2-byte VEX if possible
3. Write the VEX prefix

# FUTURE WORK

## Generate AVX-512 Instruction
1. Obtain REX prefix from operand and set REX.W according to rex_w field.
2. Setup EVEX structure and write it.
