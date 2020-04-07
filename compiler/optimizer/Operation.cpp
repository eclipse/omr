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

#include "optimizer/TreeInterpreter.hpp"
#include <cstring>

void
TR::TreeInterpreter::performOp(TR::Node * node){
   VALUE operand1, operand2, result;
   memset(&result, 0, sizeof(VALUE));

   switch (node->getOpCodeValue()){
      case TR::BBStart:
         break;
      case TR::BBEnd:
         break;
      case TR::treetop:
         break;

      // long operations
      case TR::lconst:
      {
         result.type = Int64;
         result.data.lconst = node->getLongInt();
         break;
      }
      case TR::lload:
      {
         result = symbolTable[node->getSymbol()];
         break;
      }
      case TR::lloadi:
      {
         result.type = TR::TreeInterpreter::DATATYPE::Address;
         result.data.aconst = (uintptrj_t)&(symbolTable[node->getSymbol()]);
         break;
      }
      case TR::lstore:
      {
         symbolTable[node->getSymbol()] = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         break;
      }
      case TR::ladd:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst + operand2.data.lconst;
         break;
      }
      case TR::lsub:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst - operand2.data.lconst;
         break;
      }
      case TR::lmul:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst * operand2.data.lconst;
         break;
      }
      case TR::ldiv:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst / operand2.data.lconst;
         break;
      }
      case TR::lrem:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst % operand2.data.lconst;
         break;
      }
      case TR::lneg:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = ~operand1.data.lconst;
         break;
      }
      case TR::labs:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int64;
         if (operand1.data.lconst >= 0)
            result.data.lconst = operand1.data.lconst;
         else
            result.data.lconst = -(operand1.data.lconst);
         break;
      }
      case TR::lshl:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst << operand2.data.lconst;
         break;
      }
      case TR::lshr:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst >> operand2.data.lconst;
         break;
      }
      case TR::lrol:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = (operand1.data.lconst << operand2.data.lconst) 
                              | (operand1.data.lconst >> (64 - operand2.data.lconst));
         break;
      }
      case TR::land:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst & operand2.data.lconst;
         break;
      }
      case TR::lor:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst | operand2.data.lconst;
         break;
      }
      case TR::lxor:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.lconst ^ operand2.data.lconst;
         break;
      }
      case TR::l2i:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int32;
         result.data.iconst = (int32_t)operand1.data.lconst;
         break;
      }
      case TR::l2f:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Float;
         result.data.fconst = (float_t)operand1.data.lconst;
         break;
      }
      case TR::l2d:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Double;
         result.data.dconst = (double_t)operand1.data.lconst;
         break;
      }
      case TR::l2b:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int8;
         result.data.bconst = (int8_t)operand1.data.lconst;
         break;
      }
      case TR::l2s:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int16;
         result.data.sconst = (int16_t)operand1.data.lconst;
         break;
      }
      case TR::l2a:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Address;
         result.data.aconst = (uintptrj_t)operand1.data.lconst;
         break;
      }
      case TR::lcmpeq:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst == operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmpne:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst != operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmplt:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst <  operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmpge:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst >= operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmpgt:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst >  operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmple:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Boolean;
         result.data.lconst = operand1.data.lconst <= operand2.data.lconst ? true : false;
         break;
      }
      case TR::lcmp:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int8;
         result.data.lconst = operand1.data.lconst >  operand2.data.lconst ? 1 :
                              operand1.data.lconst == operand2.data.lconst ? 0 : -1;
         break;
      }
      case TR::lternary:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = operand1.data.boolean
                                 ? nodeValueMap[node->getChild(1)->getGlobalIndex()].data.lconst
                                 : nodeValueMap[node->getChild(2)->getGlobalIndex()].data.lconst;
         break;
      }
      case TR::lmulh:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = (((__int128_t)operand1.data.lconst) * ((__int128_t)operand2.data.lconst))>>64;
         break;
      }
      case TR::lbits2d:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         result.type = Double;
         result.data.lconst = operand1.data.lconst;
         break;
      }
      case TR::lcmpset:
      {
         int64_t * pointer = (int64_t *)&nodeValueMap[node->getChild(0)->getGlobalIndex()].data.lconst;
         operand1 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(2)->getGlobalIndex()];
         result.type = Boolean;
         if (*pointer == operand1.data.lconst){
            *pointer = operand2.data.lconst;
            result.data.boolean = false;
         }
         else{
            result.data.boolean = true;
         }
         break;
      }
      case TR::lexp:
      {
         operand1 = nodeValueMap[node->getChild(0)->getGlobalIndex()];
         operand2 = nodeValueMap[node->getChild(1)->getGlobalIndex()];
         result.type = Int64;
         result.data.lconst = pow(operand1.data.lconst,operand2.data.lconst);
         break;
      }
      case TR::lmax:
      {
         int64_t max, temp;
         max = (nodeValueMap[node->getChild(0)->getGlobalIndex()]).data.lconst;
         for (int i = 1; i < node->getNumChildren(); ++i){
            temp = (nodeValueMap[node->getChild(i)->getGlobalIndex()]).data.lconst;
            if (temp > max)
               max = temp;
         }
         result.type = Int64;
         result.data.lconst = max;
         break;
      }
      case TR::lmin:
      {
         int64_t min, temp;
         min = nodeValueMap[node->getChild(0)->getGlobalIndex()].data.lconst;
         for (int i = 1; i < node->getNumChildren(); ++i){
            int64_t temp = nodeValueMap[node->getChild(i)->getGlobalIndex()].data.lconst;
            if (min > temp)
               min = temp;
         }
         result.type = Int64;
         result.data.lconst = min;
         break;
      }
      default:
         TR_ASSERT_FATAL(1, "Unexpected opcode for n%dn [%p]\n", node->getGlobalIndex(), node);
   }
   if (node->getReferenceCount() != 0){
      nodeValueMap[node->getGlobalIndex()] = result;
   }
   return;
}
