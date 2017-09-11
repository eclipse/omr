/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef X86OPS_INLINES_INCL
#define X86OPS_INLINES_INCL

inline bool TR_X86OpCode::OpCode_t::allowsAVX()
   {
   static bool enable = feGetEnv("TR_EnableAVX") && TR::CodeGenerator::getX86ProcessorInfo().supportsAVX();
   return enable;
   }

template <typename TBuffer> inline typename TBuffer::cursor_t TR_X86OpCode::OpCode_t::encode(typename TBuffer::cursor_t cursor, uint8_t rexbits) const
   {
   if (isX87())
      {
      TBuffer buffer(cursor);
      buffer.append(opcode);
      // Heuristics for X87 second byte opcode
      // It could be eliminated if GCC/MSVC fully support initializer list
      buffer.append((uint8_t)((modrm_opcode << 5) | (modrm_form << 3) | immediate_size));
      return buffer;
      }
   TBuffer buffer(cursor);
   // Prefixes
   TR::Instruction::REX rex(rexbits);
   rex.W = rex_w;
   TR_ASSERT(TR::Compiler->target.is64Bit() || !rex.value(), "ERROR: REX.W used on X86-32. OpCode = %d; rex = %02x", opcode, (uint32_t)(uint8_t)rex.value());
   // Use AVX if possible
   if (supportsAVX() && allowsAVX())
      {
      TR::Instruction::VEX<3> vex(rex, modrm_opcode);
      vex.m = escape;
      vex.L = vex_l;
      vex.p = prefixes;
      vex.opcode = opcode;
      if(vex.CanBeShortened())
         {
         buffer.append(TR::Instruction::VEX<2>(vex));
         }
      else
         {
         buffer.append(vex);
         }
      }
   else
      {
      switch (prefixes)
         {
         case PREFIX___:
            break;
         case PREFIX_66:
            buffer.append('\x66');
            break;
         case PREFIX_F2:
            buffer.append('\xf2');
            break;
         case PREFIX_F3:
            buffer.append('\xf3');
            break;
         default:
            break;
         }
      // REX
      if (rex.value() || rexbits)
         {
         buffer.append(rex);
         }
      // OpCode escape
      switch (escape)
         {
         case ESCAPE_____:
            break;
         case ESCAPE_0F__:
            buffer.append('\x0f');
            break;
         case ESCAPE_0F38:
            buffer.append('\x0f');
            buffer.append('\x38');
            break;
         case ESCAPE_0F3A:
            buffer.append('\x0f');
            buffer.append('\x3a');
            break;
         default:
            break;
         }
      // OpCode
      buffer.append(opcode);
      // ModRM
      if (modrm_form)
         {
         buffer.append(TR::Instruction::ModRM(modrm_opcode));
         }
      }
   return buffer;
   }

inline void TR_X86OpCode::OpCode_t::finalize(uint8_t* cursor) const
   {
   // Finalize VEX prefix
   switch (*cursor)
      {
      case 0xC4:
         {
         auto pVEX = (TR::Instruction::VEX<3>*)cursor;
         if (vex_v == VEX_vReg_)
            {
            pVEX->v = ~(modrm_form == ModRM_EXT_ ? pVEX->RM() : pVEX->Reg());
            }
         }
         break;
      case 0xC5:
         {
         auto pVEX = (TR::Instruction::VEX<2>*)cursor;
         if (vex_v == VEX_vReg_)
            {
            pVEX->v = ~(modrm_form == ModRM_EXT_ ? pVEX->RM() : pVEX->Reg());
            }
         }
         break;
      default:
         break;
      }
   }

inline void TR_X86OpCode::CheckAndFinishGroup07(uint8_t* cursor) const
   {
   if(info().isGroup07())
      {
      auto pModRM = (TR::Instruction::ModRM*)(cursor-1);
      switch(_opCode)
         {
         case XEND:
            pModRM->rm = 0x05; // 0b101
            break;
         default:
            TR_ASSERT(false, "INVALID OPCODE.");
            break;
         }
      }
   }

inline uint8_t TR_X86OpCode::length(uint8_t rex) const
   {
   return encode<Estimator>(0, rex);
   }
inline uint8_t* TR_X86OpCode::binary(uint8_t* cursor, uint8_t rex) const
   {
   uint8_t* ret = encode<Writer>(cursor, rex);
   CheckAndFinishGroup07(ret);
   return ret;
   }
inline void TR_X86OpCode::finalize(uint8_t* cursor) const
   {
   if (!isPseudoOp())
      info().finalize(cursor);
   }
#endif
