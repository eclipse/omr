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

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "tests/injectors/OpIlInjector.hpp"

namespace TestCompiler
{
void
OpIlInjector::setDataType()
   {
   TR::ILOpCode op(_opCode);
   if (op.isConversion() || op.isBooleanCompare() || op.isSignum())
      {
      switch (_opCode)
         {
         case TR::i2l:
         case TR::i2f:
         case TR::i2d:
         case TR::i2b:
         case TR::i2s:
         case TR::i2a:
         case TR::iu2l:
         case TR::iu2f:
         case TR::iu2d:
         case TR::iu2a:
         case TR::icmpeq:
         case TR::icmpne:
         case TR::icmplt:
         case TR::icmpge:
         case TR::icmpgt:
         case TR::icmple:
         case TR::iucmplt:
         case TR::iucmpge:
         case TR::iucmpgt:
         case TR::iucmple:
         case TR::ificmpeq:
         case TR::ificmpne:
         case TR::ificmplt:
         case TR::ificmpge:
         case TR::ificmpgt:
         case TR::ificmple:
         case TR::ifiucmplt:
         case TR::ifiucmpge:
         case TR::ifiucmpgt:
         case TR::ifiucmple:
            _dataType = TR::Int32;
            break;
         case TR::l2i:
         case TR::l2f:
         case TR::l2d:
         case TR::l2b:
         case TR::l2s:
         case TR::l2a:
         case TR::lu2f:
         case TR::lu2d:
         case TR::lu2a:
         case TR::lcmpeq:
         case TR::lcmpne:
         case TR::lcmplt:
         case TR::lcmpge:
         case TR::lcmpgt:
         case TR::lcmple:
         case TR::lucmplt:
         case TR::lucmpge:
         case TR::lucmpgt:
         case TR::lucmple:
         case TR::lcmp:
         case TR::iflcmpeq:
         case TR::iflcmpne:
         case TR::iflcmplt:
         case TR::iflcmpge:
         case TR::iflcmpgt:
         case TR::iflcmple:
         case TR::iflucmplt:
         case TR::iflucmpge:
         case TR::iflucmpgt:
         case TR::iflucmple:
            _dataType = TR::Int64;
            break;
         case TR::f2i:
         case TR::f2l:
         case TR::f2d:
         case TR::f2b:
         case TR::f2s:
         case TR::fcmpeq:
         case TR::fcmpne:
         case TR::fcmplt:
         case TR::fcmpge:
         case TR::fcmpgt:
         case TR::fcmple:
         case TR::fcmpequ:
         case TR::fcmpneu:
         case TR::fcmpltu:
         case TR::fcmpgeu:
         case TR::fcmpgtu:
         case TR::fcmpleu:
         case TR::fcmpl:
         case TR::fcmpg:
         case TR::iffcmpeq:
         case TR::iffcmpne:
         case TR::iffcmplt:
         case TR::iffcmpge:
         case TR::iffcmpgt:
         case TR::iffcmple:
         case TR::iffcmpequ:
         case TR::iffcmpneu:
         case TR::iffcmpltu:
         case TR::iffcmpgeu:
         case TR::iffcmpgtu:
         case TR::iffcmpleu:
            _dataType = TR::Float;
            break;
         case TR::d2i:
         case TR::d2l:
         case TR::d2f:
         case TR::d2b:
         case TR::d2s:
         case TR::dcmpeq:
         case TR::dcmpne:
         case TR::dcmplt:
         case TR::dcmpge:
         case TR::dcmpgt:
         case TR::dcmple:
         case TR::dcmpequ:
         case TR::dcmpneu:
         case TR::dcmpltu:
         case TR::dcmpgeu:
         case TR::dcmpgtu:
         case TR::dcmpleu:
         case TR::dcmpl:
         case TR::dcmpg:
         case TR::ifdcmpeq:
         case TR::ifdcmpne:
         case TR::ifdcmplt:
         case TR::ifdcmpge:
         case TR::ifdcmpgt:
         case TR::ifdcmple:
         case TR::ifdcmpequ:
         case TR::ifdcmpneu:
         case TR::ifdcmpltu:
         case TR::ifdcmpgeu:
         case TR::ifdcmpgtu:
         case TR::ifdcmpleu:
            _dataType = TR::Double;
            break;
         case TR::b2i:
         case TR::b2l:
         case TR::b2f:
         case TR::b2d:
         case TR::b2s:
         case TR::b2a:
         case TR::bu2i:
         case TR::bu2l:
         case TR::bu2f:
         case TR::bu2d:
         case TR::bu2s:
         case TR::bu2a:
         case TR::bcmpeq:
         case TR::bcmpne:
         case TR::bcmplt:
         case TR::bcmpge:
         case TR::bcmpgt:
         case TR::bcmple:
         case TR::bucmplt:
         case TR::bucmpge:
         case TR::bucmpgt:
         case TR::bucmple:
         case TR::ifbcmpeq:
         case TR::ifbcmpne:
         case TR::ifbcmplt:
         case TR::ifbcmpge:
         case TR::ifbcmpgt:
         case TR::ifbcmple:
         case TR::ifbucmplt:
         case TR::ifbucmpge:
         case TR::ifbucmpgt:
         case TR::ifbucmple:
            _dataType = TR::Int8;
            break;
         case TR::s2i:
         case TR::s2l:
         case TR::s2f:
         case TR::s2d:
         case TR::s2b:
         case TR::s2a:
         case TR::su2i:
         case TR::su2l:
         case TR::su2f:
         case TR::su2d:
         case TR::su2a:
         case TR::scmpeq:
         case TR::scmpne:
         case TR::scmplt:
         case TR::scmpge:
         case TR::scmpgt:
         case TR::scmple:
         case TR::sucmplt:
         case TR::sucmpge:
         case TR::sucmpgt:
         case TR::sucmple:
         case TR::ifscmpeq:
         case TR::ifscmpne:
         case TR::ifscmplt:
         case TR::ifscmpge:
         case TR::ifscmpgt:
         case TR::ifscmple:
         case TR::ifsucmplt:
         case TR::ifsucmpge:
         case TR::ifsucmpgt:
         case TR::ifsucmple:
            _dataType = TR::Int16;
            break;
         case TR::a2i:
         case TR::a2l:
         case TR::a2b:
         case TR::a2s:
         case TR::acmpeq:
         case TR::acmpne:
         case TR::acmplt:
         case TR::acmpge:
         case TR::acmpgt:
         case TR::acmple:
         case TR::ifacmpeq:
         case TR::ifacmpne:
         case TR::ifacmplt:
         case TR::ifacmpge:
         case TR::ifacmpgt:
         case TR::ifacmple:
            _dataType = TR::Address;
            break;
         default:
            TR_ASSERT(0, "not supported converting or boolean compare opcode");
         }
      }
   else
      {
      if (op.isByte())
         _dataType = TR::Int8;
      else if (op.isShort())
         _dataType = TR::Int16;
      else if (op.isInt())
         _dataType = TR::Int32;
      else if (op.isLong())
         _dataType = TR::Int64;
      else if (op.isFloat())
         _dataType = TR::Float;
      else if (op.isDouble())
         _dataType = TR::Double;
      else if (op.isRef())
         _dataType = TR::Address;
      //TODO : may need to add other dataType
      else
         TR_ASSERT(0, "Wrong dataType or not supported dataType");
      }
   }

bool
OpIlInjector::isOpCodeSupported()
   {
   return comp()->cg()->isILOpCodeSupported(_opCode);
   }

TR::Node *
OpIlInjector::parm(uint32_t slot)
   {
   TR_ASSERT(slot > 0 && slot <= _numOptArgs, "parm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);

   int32_t index = slot-1;
   ParmNode *pnode = _optArgs[index];

   if (pnode != NULL)
      {
      switch (pnode->type)
         {
         case ParmInteger:
            return iconst(pnode->value.parmInt);
         case ParmLong:
            return lconst(pnode->value.parmLong);
         case ParmByte:
            return bconst(pnode->value.parmByte);
         case ParmShort:
            return sconst(pnode->value.parmShort);
         case ParmAddress:
            return aconst(pnode->value.parmAddress);
         case ParmDouble:
            return dconst(pnode->value.parmDouble);
         case ParmFloat:
            return fconst(pnode->value.parmFloat);
         default:
            TR_ASSERT(0, "Unknown parm type: %d\n", pnode->type);
            return NULL;
         }
      }
   if(TR::NoType != _conditionalDataType && 0 == index)
      {
      return parameter(index, _types->PrimitiveType(_conditionalDataType));
      }

   TR::ILOpCode op(_opCode);
   if (op.isStoreIndirect() && 0 == index)
      {
      return parameter(index, _types->PrimitiveType(TR::Address));
      }
   else if (op.isLoadIndirect())
   	{
   	return parameter(index, _types->PrimitiveType(TR::Address));
   	}

   return parameter(index, _types->PrimitiveType(_dataType));
   }
} // namespace TestCompiler
