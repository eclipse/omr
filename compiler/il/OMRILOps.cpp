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

#include "il/ILOps.hpp"

#include <stdint.h>
#include <stddef.h>
#include "env/CompilerEnv.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/ILProps.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

/**
 * Table of opcode properties.
 *
 * \note A note on the syntax of the table below. The commented field names,
 * or rather, the designated initializer syntax would actually work in
 * C99, however we can't use that feature because microsoft compiler
 * still doesn't support C99. If microsoft ever supports this we can
 * make the table even more robust and type safe by uncommenting the
 * field initializers.
 */
OMR::OpCodeProperties OMR::ILOpCode::_opCodeProperties[] =
   {
#include "il/ILOpCodeProperties.hpp"
   };

void
OMR::ILOpCode::checkILOpArrayLengths()
   {
   for (int i = TR::FirstOMROp; i < TR::NumIlOps; i++)
      {
      TR::ILOpCodes opCode = (TR::ILOpCodes)i;
      TR::ILOpCode  op(opCode);
      OMR::OpCodeProperties &props = TR::ILOpCode::_opCodeProperties[opCode];

      TR_ASSERT(props.opcode == opCode, "_opCodeProperties table out of sync at index %d, has %s\n", i, op.getName());
      }
   }

// FIXME: We should put the smarts in the getSize() routine in TR::DataType
// instead of modifying this large table that should in fact be read-only
//
void
OMR::ILOpCode::setTarget()
   {
   if (TR::Compiler->target.is64Bit())
      {
      for (int32_t i = 0; i < TR::NumIlOps; ++i)
         {
         flags32_t *tp = (flags32_t*)(&_opCodeProperties[i].typeProperties); // so ugly
         if (tp->getValue() == ILTypeProp::Reference)
            {
            tp->reset(ILTypeProp::Size_Mask);
            tp->set(ILTypeProp::Size_8);
            }
         }
      TR::DataType::setSize(TR::Address, 8);
      }
   else
      {
      for (int32_t i = 0; i < TR::NumIlOps; ++i)
         {
         flags32_t *tp = (flags32_t*)(&_opCodeProperties[i].typeProperties); // so ugly
         if (tp->getValue() == ILTypeProp::Reference)
            {
            tp->reset(ILTypeProp::Size_Mask);
            tp->set(ILTypeProp::Size_4);
            }
         }
      TR::DataType::setSize(TR::Address, 4);
      }
   }


TR::ILOpCodes
OMR::ILOpCode::compareOpCode(TR::DataType dt,
                             enum TR_ComparisonTypes ct,
                             bool unsignedCompare)
   {
   if (unsignedCompare)
      {
      switch(dt)
         {
         case TR::Int8:
            {
            switch(ct)
               {
               case TR_cmpLT: return TR::bucmplt;
               case TR_cmpLE: return TR::bucmple;
               case TR_cmpGT: return TR::bucmpgt;
               case TR_cmpGE: return TR::bucmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int16:
            {
            switch(ct)
               {
               case TR_cmpLT: return TR::sucmplt;
               case TR_cmpLE: return TR::sucmple;
               case TR_cmpGT: return TR::sucmpgt;
               case TR_cmpGE: return TR::sucmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int32:
            {
            switch(ct)
               {
               case TR_cmpLT: return TR::iucmplt;
               case TR_cmpLE: return TR::iucmple;
               case TR_cmpGT: return TR::iucmpgt;
               case TR_cmpGE: return TR::iucmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int64:
            {
            switch(ct)
               {
               case TR_cmpLT: return TR::lucmplt;
               case TR_cmpLE: return TR::lucmple;
               case TR_cmpGT: return TR::lucmpgt;
               case TR_cmpGE: return TR::lucmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Float:
            {
            switch(ct)
               {
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Double:
            {
            switch(ct)
               {
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Address:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::acmpeq;
               case TR_cmpNE: return TR::acmpne;
               case TR_cmpLT: return TR::acmplt;
               case TR_cmpLE: return TR::acmple;
               case TR_cmpGT: return TR::acmpgt;
               case TR_cmpGE: return TR::acmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         default: return TR::BadILOp;
         }
      }
   else
      {
      switch(dt)
         {
         case TR::Int8:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::bcmpeq;
               case TR_cmpNE: return TR::bcmpne;
               case TR_cmpLT: return TR::bcmplt;
               case TR_cmpLE: return TR::bcmple;
               case TR_cmpGT: return TR::bcmpgt;
               case TR_cmpGE: return TR::bcmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int16:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::scmpeq;
               case TR_cmpNE: return TR::scmpne;
               case TR_cmpLT: return TR::scmplt;
               case TR_cmpLE: return TR::scmple;
               case TR_cmpGT: return TR::scmpgt;
               case TR_cmpGE: return TR::scmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int32:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::icmpeq;
               case TR_cmpNE: return TR::icmpne;
               case TR_cmpLT: return TR::icmplt;
               case TR_cmpLE: return TR::icmple;
               case TR_cmpGT: return TR::icmpgt;
               case TR_cmpGE: return TR::icmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Int64:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::lcmpeq;
               case TR_cmpNE: return TR::lcmpne;
               case TR_cmpLT: return TR::lcmplt;
               case TR_cmpLE: return TR::lcmple;
               case TR_cmpGT: return TR::lcmpgt;
               case TR_cmpGE: return TR::lcmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Float:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::fcmpeq;
               case TR_cmpNE: return TR::fcmpneu;
               case TR_cmpLT: return TR::fcmplt;
               case TR_cmpLE: return TR::fcmple;
               case TR_cmpGT: return TR::fcmpgt;
               case TR_cmpGE: return TR::fcmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Double:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::dcmpeq;
               case TR_cmpNE: return TR::dcmpneu;
               case TR_cmpLT: return TR::dcmplt;
               case TR_cmpLE: return TR::dcmple;
               case TR_cmpGT: return TR::dcmpgt;
               case TR_cmpGE: return TR::dcmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         case TR::Address:
            {
            switch(ct)
               {
               case TR_cmpEQ: return TR::acmpeq;
               case TR_cmpNE: return TR::acmpne;
               case TR_cmpLT: return TR::acmplt;
               case TR_cmpLE: return TR::acmple;
               case TR_cmpGT: return TR::acmpgt;
               case TR_cmpGE: return TR::acmpge;
               default: return TR::BadILOp;
               }
            break;
            }
         default: return TR::BadILOp;
         }
      }
   return TR::BadILOp;
   }

/**
 * \brief
 *    Return a comparison type given the compare opcode.
 *
 * \parm op
 *    The compare opcode.
 *
 * \return
 *    The compareison type.
 */
TR_ComparisonTypes
OMR::ILOpCode::getCompareType(TR::ILOpCodes op)
   {
   if (isStrictlyLessThanCmp(op))
      return TR_cmpLT;
   else if (isStrictlyGreaterThanCmp(op))
      return TR_cmpGT;
   else if (isLessCmp(op))
      return TR_cmpLE;
   else if (isGreaterCmp(op))
      return TR_cmpGE;
   else if (isEqualCmp(op))
      return TR_cmpEQ;
   else
      return TR_cmpNE;
   }
