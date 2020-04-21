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

#include <stdint.h>
#include <stdio.h>
#include "env/jittypes.h"
#include "gtest/gtest.h"

#include "tests/PPCOpCodesTest.hpp"

namespace TestCompiler
{

void
PPCOpCodesTest::compileUnaryTestMethods()
   {
   int32_t rc = 0;
   compileOpCodeMethod(_d2b, _numberOfUnaryArgs, TR::d2b, "d2b", _argTypesUnaryDouble, TR::Int8, rc);
   compileOpCodeMethod(_d2s, _numberOfUnaryArgs, TR::d2s, "d2s", _argTypesUnaryDouble, TR::Int16, rc);
   compileOpCodeMethod(_f2b, _numberOfUnaryArgs, TR::f2b, "f2b", _argTypesUnaryFloat, TR::Int8, rc);
   compileOpCodeMethod(_f2s, _numberOfUnaryArgs, TR::f2s, "f2s", _argTypesUnaryFloat, TR::Int16, rc);

   compileOpCodeMethod(_b2i, _numberOfUnaryArgs, TR::b2i, "b2i", _argTypesUnaryByte, TR::Int32, rc);
   compileOpCodeMethod(_b2l, _numberOfUnaryArgs, TR::b2l, "b2l", _argTypesUnaryByte, TR::Int64, rc);
   compileOpCodeMethod(_b2s, _numberOfUnaryArgs, TR::b2s, "b2s", _argTypesUnaryByte, TR::Int16, rc);
   compileOpCodeMethod(_bu2i, _numberOfUnaryArgs, TR::bu2i, "bu2i", _argTypesUnaryByte, TR::Int32, rc);
   compileOpCodeMethod(_bu2l, _numberOfUnaryArgs, TR::bu2l, "bu2l", _argTypesUnaryByte, TR::Int64, rc);
   compileOpCodeMethod(_bu2s, _numberOfUnaryArgs, TR::bu2s, "bu2s", _argTypesUnaryByte, TR::Int16, rc);

   compileOpCodeMethod(_s2i, _numberOfUnaryArgs, TR::s2i, "s2i", _argTypesUnaryShort, TR::Int32, rc);
   compileOpCodeMethod(_s2l, _numberOfUnaryArgs, TR::s2l, "s2l", _argTypesUnaryShort, TR::Int64, rc);
   compileOpCodeMethod(_s2b, _numberOfUnaryArgs, TR::s2b, "s2b", _argTypesUnaryShort, TR::Int8, rc);
   compileOpCodeMethod(_su2i, _numberOfUnaryArgs, TR::su2i, "su2i", _argTypesUnaryShort, TR::Int32, rc);
   compileOpCodeMethod(_su2l, _numberOfUnaryArgs, TR::su2l, "su2l", _argTypesUnaryShort, TR::Int64, rc);
   compileOpCodeMethod(_iByteswap, _numberOfUnaryArgs, TR::ibyteswap, "iByteswap", _argTypesUnaryInt, TR::Int32, rc);

   }

void
PPCOpCodesTest::compileMemoryOperationTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_bLoad, _numberOfUnaryArgs, TR::bload, "bLoad", _argTypesUnaryByte, TR::Int8, rc);
   compileOpCodeMethod(_sLoad, _numberOfUnaryArgs, TR::sload, "sLoad", _argTypesUnaryShort, TR::Int16, rc);

   compileOpCodeMethod(_bStore, _numberOfUnaryArgs, TR::bstore, "bStore", _argTypesUnaryByte, TR::Int8, rc);
   compileOpCodeMethod(_sStore, _numberOfUnaryArgs, TR::sstore, "sStore", _argTypesUnaryShort, TR::Int16, rc);
   compileOpCodeMethod(_lStore, _numberOfUnaryArgs, TR::lstore, "lStore", _argTypesUnaryLong, TR::Int64, rc);
   compileOpCodeMethod(_dStore, _numberOfUnaryArgs, TR::dstore, "dStore", _argTypesUnaryDouble, TR::Double, rc);
   compileOpCodeMethod(_fStore, _numberOfUnaryArgs, TR::fstore, "fStore", _argTypesUnaryFloat, TR::Float, rc);

   compileOpCodeMethod(_iStorei, _numberOfBinaryArgs, TR::istorei, "iStorei", _argTypesBinaryAddressInt, TR::Int32, rc);
   compileOpCodeMethod(_lStorei, _numberOfBinaryArgs, TR::lstorei, "lStorei", _argTypesBinaryAddressLong, TR::Int64, rc);
   compileOpCodeMethod(_dStorei, _numberOfBinaryArgs, TR::dstorei, "dStorei", _argTypesBinaryAddressDouble, TR::Double, rc);
   compileOpCodeMethod(_fStorei, _numberOfBinaryArgs, TR::fstorei, "fStorei", _argTypesBinaryAddressFloat, TR::Float, rc);
   compileOpCodeMethod(_sStorei, _numberOfBinaryArgs, TR::sstorei, "sStorei", _argTypesBinaryAddressShort, TR::Int16, rc);
   compileOpCodeMethod(_aStorei, _numberOfBinaryArgs, TR::astorei, "aStorei", _argTypesBinaryAddressAddress, TR::Address, rc);
   }

void
PPCOpCodesTest::compileSelectTestMethods()
   {
   int32_t rc = 0;
   compileOpCodeMethod(_bselect, _numberOfSelectArgs, TR::bselect, "bselect", _argTypesSelectByte, TR::Int8, rc);
   compileOpCodeMethod(_sselect, _numberOfSelectArgs, TR::sselect, "sselect", _argTypesSelectShort, TR::Int16, rc);
   }

void
PPCOpCodesTest::compileCompareTestMethods()
   {
   int32_t rc = 0;

   //Compare
   compileOpCodeMethod(_sCmpeq, _numberOfBinaryArgs, TR::scmpeq, "sCmpeq", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_sCmpne, _numberOfBinaryArgs, TR::scmpne, "sCmpne", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_sCmpgt, _numberOfBinaryArgs, TR::scmpgt, "sCmpgt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_sCmplt, _numberOfBinaryArgs, TR::scmplt, "sCmplt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_sCmpge, _numberOfBinaryArgs, TR::scmpge, "sCmpge", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_sCmple, _numberOfBinaryArgs, TR::scmple, "sCmple", _argTypesBinaryShort, TR::Int32, rc);

   compileOpCodeMethod(_bCmpeq, _numberOfBinaryArgs, TR::bcmpeq, "bCmpeq", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_bCmpgt, _numberOfBinaryArgs, TR::bcmpgt, "bCmpgt", _argTypesBinaryByte, TR::Int32, rc);

   compileOpCodeMethod(_iuCmpge, _numberOfBinaryArgs, TR::iucmpge, "iuCmpge", _argTypesBinaryInt, TR::Int32, rc);

   compileOpCodeMethod(_suCmplt, _numberOfBinaryArgs, TR::sucmplt, "suCmplt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_suCmpge, _numberOfBinaryArgs, TR::sucmpge, "suCmpge", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_suCmpgt, _numberOfBinaryArgs, TR::sucmpgt, "suCmpgt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_suCmple, _numberOfBinaryArgs, TR::sucmple, "suCmple", _argTypesBinaryShort, TR::Int32, rc);

   //CompareAndBranch
   compileOpCodeMethod(_ifScmpeq, _numberOfBinaryArgs, TR::ifscmpeq, "ifScmpeq", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifScmpne, _numberOfBinaryArgs, TR::ifscmpne, "ifScmpne", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifScmpgt, _numberOfBinaryArgs, TR::ifscmpgt, "ifScmpgt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifScmplt, _numberOfBinaryArgs, TR::ifscmplt, "ifScmplt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifScmpge, _numberOfBinaryArgs, TR::ifscmpge, "ifScmpge", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifScmple, _numberOfBinaryArgs, TR::ifscmple, "ifScmple", _argTypesBinaryShort, TR::Int32, rc);

   compileOpCodeMethod(_ifBcmpeq, _numberOfBinaryArgs, TR::ifbcmpeq, "ifBcmpeq", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBcmpgt, _numberOfBinaryArgs, TR::ifbcmpgt, "ifBcmpgt", _argTypesBinaryByte, TR::Int32, rc);

   compileOpCodeMethod(_ifBuCmplt, _numberOfBinaryArgs, TR::ifbucmplt, "ifBuCmplt", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBuCmpge, _numberOfBinaryArgs, TR::ifbucmpge, "ifBuCmpge", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBuCmpgt, _numberOfBinaryArgs, TR::ifbucmpgt, "ifBuCmpgt", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBuCmple, _numberOfBinaryArgs, TR::ifbucmple, "ifBuCmple", _argTypesBinaryByte, TR::Int32, rc);

   compileOpCodeMethod(_ifSuCmpgt, _numberOfBinaryArgs, TR::ifsucmpgt, "ifSuCmpgt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifSuCmple, _numberOfBinaryArgs, TR::ifsucmple, "ifSuCmple", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifSuCmplt, _numberOfBinaryArgs, TR::ifsucmplt, "ifSuCmplt", _argTypesBinaryShort, TR::Int32, rc);
   compileOpCodeMethod(_ifSuCmpge, _numberOfBinaryArgs, TR::ifsucmpge, "ifSuCmpge", _argTypesBinaryShort, TR::Int32, rc);

   }

void
PPCOpCodesTest::compileAddressTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_a2b, _numberOfUnaryArgs, TR::a2b, "a2b", _argTypesUnaryAddress, TR::Int8, rc);
   compileOpCodeMethod(_a2s, _numberOfUnaryArgs, TR::a2s, "a2s", _argTypesUnaryAddress, TR::Int16, rc);
   compileOpCodeMethod(_a2l, _numberOfUnaryArgs, TR::a2l, "a2l", _argTypesUnaryAddress, TR::Int64, rc);
   compileOpCodeMethod(_b2a, _numberOfUnaryArgs, TR::b2a, "b2a", _argTypesUnaryByte, TR::Address, rc);
   compileOpCodeMethod(_s2a, _numberOfUnaryArgs, TR::s2a, "s2a", _argTypesUnaryShort, TR::Address, rc);
   compileOpCodeMethod(_i2a, _numberOfUnaryArgs, TR::i2a, "i2a", _argTypesUnaryInt, TR::Address, rc);
   compileOpCodeMethod(_bu2a, _numberOfUnaryArgs, TR::bu2a, "bu2a", _argTypesUnaryByte, TR::Address, rc);
   compileOpCodeMethod(_lu2a, _numberOfUnaryArgs, TR::lu2a, "lu2a", _argTypesUnaryLong, TR::Address, rc);

   compileOpCodeMethod(_acmpeq, _numberOfBinaryArgs, TR::acmpeq, "acmpeq", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_acmpne, _numberOfBinaryArgs, TR::acmpne, "acmpne", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_acmplt, _numberOfBinaryArgs, TR::acmplt, "acmplt", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_acmpge, _numberOfBinaryArgs, TR::acmpge, "acmpge", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_acmple, _numberOfBinaryArgs, TR::acmple, "acmple", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_acmpgt, _numberOfBinaryArgs, TR::acmpgt, "acmpgt", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmpeq, _numberOfBinaryArgs, TR::ifacmpeq, "ifacmpeq", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmpne, _numberOfBinaryArgs, TR::ifacmpne, "ifacmpne", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmplt, _numberOfBinaryArgs, TR::ifacmplt, "ifacmplt", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmpge, _numberOfBinaryArgs, TR::ifacmpge, "ifacmpge", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmple, _numberOfBinaryArgs, TR::ifacmple, "ifacmple", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_ifacmpgt, _numberOfBinaryArgs, TR::ifacmpgt, "ifacmpgt", _argTypesBinaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_aselect, _numberOfSelectArgs, TR::aselect, "aselect", _argTypesSelectAddress, TR::Address, rc);

   }

void
PPCOpCodesTest::compileBitwiseTestMethods()
   {
   int32_t rc;

   compileOpCodeMethod(_sAnd, _numberOfBinaryArgs, TR::sand, "sAnd", _argTypesBinaryShort, TR::Int16, rc);
   compileOpCodeMethod(_sOr, _numberOfBinaryArgs, TR::sor, "sOr", _argTypesBinaryShort, TR::Int16, rc);
   compileOpCodeMethod(_sXor, _numberOfBinaryArgs, TR::sxor, "sXor", _argTypesBinaryShort, TR::Int16, rc);

   compileOpCodeMethod(_bAnd, _numberOfBinaryArgs, TR::band, "bAnd", _argTypesBinaryByte, TR::Int8, rc);
   compileOpCodeMethod(_bOr, _numberOfBinaryArgs, TR::bor, "bOr", _argTypesBinaryByte, TR::Int8, rc);
   compileOpCodeMethod(_bXor, _numberOfBinaryArgs, TR::bxor, "bXor", _argTypesBinaryByte, TR::Int8, rc);
   }

void
PPCOpCodesTest::invokeUnaryTests()
   {
   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_ZERO, FLOAT_MAXIMUM, FLOAT_MINIMUM};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_ZERO, DOUBLE_MAXIMUM, DOUBLE_MINIMUM};
   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};

   uint16_t ushortDataArray[] = {USHORT_POS, USHORT_MAXIMUM, USHORT_MINIMUM};
   uint8_t ubyteDataArray[] = {UBYTE_POS, UBYTE_MAXIMUM, UBYTE_MINIMUM};


   int32_t rc = 0;
   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharI_I_testMethodType  *iUnaryCons = 0;
   signatureCharD_D_testMethodType  *dUnaryCons = 0;
   signatureCharF_F_testMethodType  *fUnaryCons = 0;

   signatureCharD_B_testMethodType * d2bConst = 0;
   signatureCharD_S_testMethodType * d2sConst = 0;

   signatureCharF_B_testMethodType * f2bConst = 0;
   signatureCharF_S_testMethodType * f2sConst = 0;

   signatureCharS_I_testMethodType * s2iConst = 0;
   signatureCharS_J_testMethodType * s2lConst = 0;
   signatureCharS_B_testMethodType * s2bConst = 0;
   signatureCharB_I_testMethodType * b2iConst = 0;
   signatureCharB_J_testMethodType * b2lConst = 0;
   signatureCharB_S_testMethodType * b2sConst = 0;
   unsignedSignatureCharS_I_testMethodType * su2iConst = 0;
   unsignedSignatureCharS_J_testMethodType * su2lConst = 0;
   unsignedSignatureCharB_I_testMethodType * bu2iConst = 0;
   unsignedSignatureCharB_J_testMethodType * bu2lConst = 0;
   unsignedSignatureCharB_F_testMethodType * bu2fConst = 0;
   unsignedSignatureCharB_D_testMethodType * bu2dConst = 0;
   unsignedSignatureCharB_S_testMethodType * bu2sConst = 0;

   //Temporarily postpone converting FLOAT_MAXIMUM and FLOAT_MINIMUM to int and long.
   //the behavior is undefined since the truncated value of the min and max numbers
   //cannot be represented in the destination types.?? how about to short and byte??
   //and for ppc?

   //d2b d2s
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_d2b, convert(doubleDataArray[i], BYTE_POS), _d2b(doubleDataArray[i]));
      OMR_CT_EXPECT_EQ(_d2s, convert(doubleDataArray[i], SHORT_POS), _d2s(doubleDataArray[i]));

      sprintf(resolvedMethodName, "d2bConst%d", i + 1);
      compileOpCodeMethod(d2bConst, _numberOfUnaryArgs, TR::d2b,
            resolvedMethodName, _argTypesUnaryDouble, TR::Int8, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_EQ(d2bConst, convert(doubleDataArray[i], BYTE_POS), d2bConst(DOUBLE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "d2sConst%d", i + 1);
      compileOpCodeMethod(d2sConst, _numberOfUnaryArgs, TR::d2s,
            resolvedMethodName, _argTypesUnaryDouble, TR::Int16, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_EQ(d2sConst, convert(doubleDataArray[i], SHORT_POS), d2sConst(DOUBLE_PLACEHOLDER_1));
      }

   //f2b f2s
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_f2b, convert(floatDataArray[i], BYTE_POS), _f2b(floatDataArray[i]));
      OMR_CT_EXPECT_EQ(_f2s, convert(floatDataArray[i], SHORT_POS), _f2s(floatDataArray[i]));

      sprintf(resolvedMethodName, "f2bConst%d", i + 1);
      compileOpCodeMethod(f2bConst, _numberOfUnaryArgs, TR::f2b,
            resolvedMethodName, _argTypesUnaryFloat, TR::Int8, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_EQ(f2bConst, convert(floatDataArray[i], BYTE_POS), f2bConst(FLOAT_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "f2sConst%d", i + 1);
      compileOpCodeMethod(f2sConst, _numberOfUnaryArgs, TR::f2s,
            resolvedMethodName, _argTypesUnaryFloat, TR::Int16, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_EQ(f2sConst, convert(floatDataArray[i], SHORT_POS), f2sConst(FLOAT_PLACEHOLDER_1));
      }

   //b 2 i,l,s
   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_b2s, convert(byteDataArray[i], SHORT_POS), _b2s(byteDataArray[i]));
      OMR_CT_EXPECT_EQ(_b2i, convert(byteDataArray[i], INT_POS), _b2i(byteDataArray[i]));
      OMR_CT_EXPECT_EQ(_b2l, convert(byteDataArray[i], LONG_POS), _b2l(byteDataArray[i]));

      sprintf(resolvedMethodName, "b2sConst%d", i + 1);
      compileOpCodeMethod(b2sConst, _numberOfUnaryArgs, TR::b2s,
            resolvedMethodName, _argTypesUnaryByte, TR::Int16, rc, 2, 1, &byteDataArray[i]);
      OMR_CT_EXPECT_EQ(b2sConst, convert(byteDataArray[i], SHORT_POS), b2sConst(BYTE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "b2iConst%d", i + 1);
      compileOpCodeMethod(b2iConst, _numberOfUnaryArgs, TR::b2i,
            resolvedMethodName, _argTypesUnaryByte, TR::Int32, rc, 2, 1, &byteDataArray[i]);
      OMR_CT_EXPECT_EQ(b2iConst, convert(byteDataArray[i], INT_POS), b2iConst(BYTE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "b2lConst%d", i + 1);
      compileOpCodeMethod(b2lConst, _numberOfUnaryArgs, TR::b2l,
            resolvedMethodName, _argTypesUnaryByte, TR::Int64, rc, 2, 1, &byteDataArray[i]);
      OMR_CT_EXPECT_EQ(b2lConst, convert(byteDataArray[i], LONG_POS), b2lConst(BYTE_PLACEHOLDER_1));
      }

   //bu 2 i,l,s
   testCaseNum = sizeof(ubyteDataArray) / sizeof(ubyteDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bu2s, convert(ubyteDataArray[i], SHORT_POS), _bu2s(ubyteDataArray[i]));
      OMR_CT_EXPECT_EQ(_bu2i, convert(ubyteDataArray[i], INT_POS), _bu2i(ubyteDataArray[i]));
      OMR_CT_EXPECT_EQ(_bu2l, convert(ubyteDataArray[i], LONG_POS), _bu2l(ubyteDataArray[i]));

      sprintf(resolvedMethodName, "bu2sConst%d", i + 1);
      compileOpCodeMethod(bu2sConst, _numberOfUnaryArgs, TR::bu2s,
            resolvedMethodName, _argTypesUnaryByte, TR::Int16, rc, 2, 1, &ubyteDataArray[i]);
      OMR_CT_EXPECT_EQ(bu2sConst, convert(ubyteDataArray[i], SHORT_POS), bu2sConst(BYTE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "bu2iConst%d", i + 1);
      compileOpCodeMethod(bu2iConst, _numberOfUnaryArgs, TR::bu2i,
            resolvedMethodName, _argTypesUnaryByte, TR::Int32, rc, 2, 1, &ubyteDataArray[i]);
      OMR_CT_EXPECT_EQ(bu2iConst, convert(ubyteDataArray[i], INT_POS), bu2iConst(BYTE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "bu2lConst%d", i + 1);
      compileOpCodeMethod(bu2lConst, _numberOfUnaryArgs, TR::bu2l,
            resolvedMethodName, _argTypesUnaryByte, TR::Int64, rc, 2, 1, &ubyteDataArray[i]);
      OMR_CT_EXPECT_EQ(bu2lConst, convert(ubyteDataArray[i], LONG_POS), bu2lConst(BYTE_PLACEHOLDER_1));
      }

   //s 2 i,l,b
   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_s2b, convert(shortDataArray[i], BYTE_POS), _s2b(shortDataArray[i]));
      OMR_CT_EXPECT_EQ(_s2i, convert(shortDataArray[i], INT_POS), _s2i(shortDataArray[i]));
      OMR_CT_EXPECT_EQ(_s2l, convert(shortDataArray[i], LONG_POS), _s2l(shortDataArray[i]));

      sprintf(resolvedMethodName, "s2bConst%d", i + 1);
      compileOpCodeMethod(s2bConst, _numberOfUnaryArgs, TR::s2b,
            resolvedMethodName, _argTypesUnaryShort, TR::Int8, rc, 2, 1, &shortDataArray[i]);
      OMR_CT_EXPECT_EQ(s2bConst, convert(shortDataArray[i], BYTE_POS), s2bConst(SHORT_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "s2iConst%d", i + 1);
      compileOpCodeMethod(s2iConst, _numberOfUnaryArgs, TR::s2i,
            resolvedMethodName, _argTypesUnaryShort, TR::Int32, rc, 2, 1, &shortDataArray[i]);
      OMR_CT_EXPECT_EQ(s2iConst, convert(shortDataArray[i], INT_POS), s2iConst(SHORT_PLACEHOLDER_1));


      sprintf(resolvedMethodName, "s2lConst%d", i + 1);
      compileOpCodeMethod(s2lConst, _numberOfUnaryArgs, TR::s2l,
            resolvedMethodName, _argTypesUnaryShort, TR::Int64, rc, 2, 1, &shortDataArray[i]);
      OMR_CT_EXPECT_EQ(s2lConst, convert(shortDataArray[i], LONG_POS), s2lConst(SHORT_PLACEHOLDER_1));
      }
   //su 2 i,l
   testCaseNum = sizeof(ushortDataArray) / sizeof(ushortDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_su2i, convert(ushortDataArray[i], INT_POS), _su2i(ushortDataArray[i]));
      OMR_CT_EXPECT_EQ(_su2l, convert(ushortDataArray[i], LONG_POS), _su2l(ushortDataArray[i]));

      sprintf(resolvedMethodName, "s2iConst%d", i + 1);
      compileOpCodeMethod(su2iConst, _numberOfUnaryArgs, TR::su2i,
            resolvedMethodName, _argTypesUnaryShort, TR::Int32, rc, 2, 1, &ushortDataArray[i]);
      OMR_CT_EXPECT_EQ(su2iConst, convert(ushortDataArray[i], INT_POS), su2iConst(SHORT_PLACEHOLDER_1));


      sprintf(resolvedMethodName, "s2lConst%d", i + 1);
      compileOpCodeMethod(su2lConst, _numberOfUnaryArgs, TR::su2l,
            resolvedMethodName, _argTypesUnaryShort, TR::Int64, rc, 2, 1, &ushortDataArray[i]);
      OMR_CT_EXPECT_EQ(su2lConst, convert(ushortDataArray[i], LONG_POS), su2lConst(SHORT_PLACEHOLDER_1));
      }
   //ibyteswap
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; i++)
      {
      OMR_CT_EXPECT_EQ(_iByteswap, byteswap(intDataArray[i]), _iByteswap(intDataArray[i]));
      sprintf(resolvedMethodName, "iByteswapConst%d", i + 1);
      compileOpCodeMethod(iUnaryCons, _numberOfUnaryArgs, TR::ibyteswap,
            resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(iUnaryCons, byteswap(intDataArray[i]), iUnaryCons(INT_PLACEHOLDER_1));
      }

   }

void
PPCOpCodesTest::invokeMemoryOperationTests()
   {
   int32_t rc = 0;
   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharS_S_testMethodType  *sMemCons = 0;
   signatureCharB_B_testMethodType  *bMemCons = 0;
   signatureCharJ_J_testMethodType  *lMemCons = 0;
   signatureCharD_D_testMethodType  *dMemCons = 0;
   signatureCharF_F_testMethodType  *fMemCons = 0;

   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_MAXIMUM, FLOAT_MINIMUM, FLOAT_ZERO};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_MAXIMUM, DOUBLE_MINIMUM, DOUBLE_ZERO};

   OMR_CT_EXPECT_EQ(_bLoad, BYTE_ZERO, _bLoad(BYTE_ZERO));
   OMR_CT_EXPECT_EQ(_bLoad, BYTE_NEG, _bLoad(BYTE_NEG));
   OMR_CT_EXPECT_EQ(_bLoad, BYTE_POS, _bLoad(BYTE_POS));
   OMR_CT_EXPECT_EQ(_bLoad, BYTE_MAXIMUM, _bLoad(BYTE_MAXIMUM));
   OMR_CT_EXPECT_EQ(_bLoad, BYTE_MINIMUM, _bLoad(BYTE_MINIMUM));

   OMR_CT_EXPECT_EQ(_sLoad, SHORT_ZERO, _sLoad(SHORT_ZERO));
   OMR_CT_EXPECT_EQ(_sLoad, SHORT_NEG, _sLoad(SHORT_NEG));
   OMR_CT_EXPECT_EQ(_sLoad, SHORT_POS, _sLoad(SHORT_POS));
   OMR_CT_EXPECT_EQ(_sLoad, SHORT_MAXIMUM, _sLoad(SHORT_MAXIMUM));
   OMR_CT_EXPECT_EQ(_sLoad, SHORT_MINIMUM, _sLoad(SHORT_MINIMUM));

   //store
   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "bStoreConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_bStore, byteDataArray[i], _bStore(byteDataArray[i]));
      compileOpCodeMethod(bMemCons, _numberOfUnaryArgs, TR::bstore, resolvedMethodName, _argTypesUnaryByte, TR::Int8, rc, 2, 1, &(byteDataArray[i]));
      OMR_CT_EXPECT_EQ(bMemCons, byteDataArray[i], bMemCons(BYTE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "sStoreConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_sStore, shortDataArray[i], _sStore(shortDataArray[i]));
      compileOpCodeMethod(sMemCons, _numberOfUnaryArgs, TR::sstore, resolvedMethodName, _argTypesUnaryShort, TR::Int16, rc, 2, 1, &(shortDataArray[i]));
      OMR_CT_EXPECT_EQ(sMemCons, shortDataArray[i], sMemCons(SHORT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "lStoreConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_lStore, longDataArray[i], _lStore(longDataArray[i]));
      compileOpCodeMethod(lMemCons, _numberOfUnaryArgs, TR::lstore, resolvedMethodName, _argTypesUnaryLong, TR::Int64, rc, 2, 1, &(longDataArray[i]));
      OMR_CT_EXPECT_EQ(lMemCons, longDataArray[i], lMemCons(LONG_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "dStoreConst%d", i + 1);
      OMR_CT_EXPECT_DOUBLE_EQ(_dStore, doubleDataArray[i], _dStore(doubleDataArray[i]));
      compileOpCodeMethod(dMemCons, _numberOfUnaryArgs, TR::dstore, resolvedMethodName, _argTypesUnaryDouble, TR::Double, rc, 2, 1, &(doubleDataArray[i]));
      OMR_CT_EXPECT_DOUBLE_EQ(dMemCons, doubleDataArray[i], dMemCons(DOUBLE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "fStoreConst%d", i + 1);
      OMR_CT_EXPECT_FLOAT_EQ(_fStore, floatDataArray[i], _fStore(floatDataArray[i]));
      compileOpCodeMethod(fMemCons, _numberOfUnaryArgs, TR::fstore, resolvedMethodName, _argTypesUnaryFloat, TR::Float, rc, 2, 1, &(floatDataArray[i]));
      OMR_CT_EXPECT_FLOAT_EQ(fMemCons, floatDataArray[i], fMemCons(FLOAT_PLACEHOLDER_1));
      }

   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   int64_t longStoreDataArray[] = {0, 0, 0, 0, 0};
   int32_t intStoreDataArray[] = {0, 0, 0, 0, 0};
   int16_t shortStoreDataArray[] = {0, 0, 0, 0, 0};
   float floatStoreDataArray[] = {0, 0, 0, 0, 0};
   double doubleStoreDataArray[] = {0, 0, 0, 0, 0};

   uintptr_t addressDataArray[] = {(uintptr_t)&INT_NEG, (uintptr_t)&LONG_POS, (uintptr_t)&BYTE_MAXIMUM, (uintptr_t)&SHORT_MINIMUM, (uintptr_t)&FLOAT_ZERO};
   uintptr_t addressStoreDataArray[] = {0, 0, 0, 0, 0};

   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      if (_iStorei != NULL)
         {
         _iStorei((uintptr_t)(&intStoreDataArray[i]) , intDataArray[i]);
         EXPECT_EQ(intDataArray[i], intStoreDataArray[i]);
         }

      if (_lStorei != NULL)
         {
         _lStorei((uintptr_t)(&longStoreDataArray[i]) , longDataArray[i]);
         EXPECT_EQ(longDataArray[i], longStoreDataArray[i]);
         }

      if (_sStorei != NULL)
         {
         _sStorei((uintptr_t)(&shortStoreDataArray[i]) , shortDataArray[i]);
         EXPECT_EQ(shortDataArray[i], shortStoreDataArray[i]);
         }

      if (_fStorei != NULL)
         {
         _fStorei((uintptr_t)(&floatStoreDataArray[i]) , floatDataArray[i]);
         EXPECT_EQ(floatDataArray[i], floatStoreDataArray[i]);
         }

      if (_dStorei != NULL)
         {
         _dStorei((uintptr_t)(&doubleStoreDataArray[i]) , doubleDataArray[i]);
         EXPECT_EQ(doubleDataArray[i], doubleStoreDataArray[i]);
         }

   //load
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   signatureCharL_I_testMethodType  *iLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "iLoadiConst%d", i + 1);
      uintptr_t intDataAddress = (uintptr_t)(&intDataArray[i]);
      compileOpCodeMethod(iLoadiCons, _numberOfUnaryArgs, TR::iloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Int32, rc, 2, 1, &intDataAddress);
      OMR_CT_EXPECT_EQ(iLoadiCons, intDataArray[i], iLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   signatureCharL_S_testMethodType  *sLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "sLoadiConst%d", i + 1);
      uintptr_t shortDataAddress = (uintptr_t)(&shortDataArray[i]);
      compileOpCodeMethod(sLoadiCons, _numberOfUnaryArgs, TR::sloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Int16, rc, 2, 1, &shortDataAddress);
      OMR_CT_EXPECT_EQ(sLoadiCons, shortDataArray[i], sLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   signatureCharL_B_testMethodType  *bLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "bLoadiConst%d", i + 1);
      uintptr_t byteDataAddress = (uintptr_t)(&byteDataArray[i]);
      compileOpCodeMethod(bLoadiCons, _numberOfUnaryArgs, TR::bloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Int8, rc, 2, 1, &byteDataAddress);
      OMR_CT_EXPECT_EQ(bLoadiCons, byteDataArray[i], bLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   signatureCharL_J_testMethodType  *lLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "lLoadiConst%d", i + 1);
      uintptr_t longDataAddress = (uintptr_t)(&longDataArray[i]);
      compileOpCodeMethod(lLoadiCons, _numberOfUnaryArgs, TR::lloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Int64, rc, 2, 1, &longDataAddress);
      OMR_CT_EXPECT_EQ(lLoadiCons, longDataArray[i], lLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   signatureCharL_D_testMethodType  *dLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "dLoadiConst%d", i + 1);
      uintptr_t doubleDataAddress = (uintptr_t)(&doubleDataArray[i]);
      compileOpCodeMethod(dLoadiCons, _numberOfUnaryArgs, TR::dloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Double, rc, 2, 1, &doubleDataAddress);
      OMR_CT_EXPECT_EQ(dLoadiCons, doubleDataArray[i], dLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   signatureCharL_F_testMethodType  *fLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "fLoadiConst%d", i + 1);
      uintptr_t floatDataAddress = (uintptr_t)(&floatDataArray[i]);
      compileOpCodeMethod(fLoadiCons, _numberOfUnaryArgs, TR::floadi, resolvedMethodName, _argTypesUnaryAddress, TR::Float, rc, 2, 1, &floatDataAddress);
      OMR_CT_EXPECT_EQ(fLoadiCons, floatDataArray[i], fLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(addressDataArray) / sizeof(addressDataArray[0]);
   signatureCharL_L_testMethodType  *aLoadiCons = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "aLoadiConst%d", i + 1);
      uintptr_t addressDataAddress = (uintptr_t)(&addressDataArray[i]);
      compileOpCodeMethod(aLoadiCons, _numberOfUnaryArgs, TR::aloadi, resolvedMethodName, _argTypesUnaryAddress, TR::Address, rc, 2, 1, &addressDataAddress);
      OMR_CT_EXPECT_EQ(aLoadiCons, addressDataArray[i], aLoadiCons(ADDRESS_PLACEHOLDER_1));
      }

      if (_aStorei != NULL)
         {
         _aStorei((uintptr_t)(&addressStoreDataArray[i]) , addressDataArray[i]);
         EXPECT_EQ(addressDataArray[i], addressStoreDataArray[i]);
         }
      }

   }

void
PPCOpCodesTest::UnsupportedOpCodesTests()
   {
   addUnsupportedOpCodeTest(_numberOfUnaryArgs, TR::lu2f, "lu2f", _argTypesUnaryLong, TR::Float);
   addUnsupportedOpCodeTest(_numberOfUnaryArgs, TR::lu2d, "lu2d", _argTypesUnaryLong, TR::Double);

   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bdiv, "bDiv", _argTypesBinaryByte, TR::Int8);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::brem, "bRem", _argTypesBinaryByte, TR::Int8);

   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::sdiv, "sDiv", _argTypesBinaryShort, TR::Int16);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::srem, "sRem", _argTypesBinaryShort, TR::Int16);

   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmplt, "bucmplt", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmpge, "bucmpge", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmpgt, "bucmpgt", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmple, "bucmple", _argTypesBinaryByte, TR::Int32);
   }

void
PPCOpCodesTest::invokeCompareTests()
   {
   int16_t sCmpeqDataArr[][2] =
         {
         SHORT_NEG, SHORT_POS,
         SHORT_POS, SHORT_POS
         };
   int16_t sCmpneDataArr[][2] =
         {
         SHORT_POS, SHORT_MINIMUM,
         SHORT_POS, SHORT_POS
         };
   int16_t sCmpgtDataArr[][2] =
         {
         SHORT_ZERO, SHORT_POS,
         SHORT_POS, SHORT_ZERO
         };
   int16_t sCmpltDataArr[][2] =
         {
         SHORT_MAXIMUM, SHORT_ZERO,
         SHORT_ZERO, SHORT_MAXIMUM
         };
   int16_t sCmpgeDataArr[][2] =
         {
         SHORT_POS, SHORT_MAXIMUM,
         SHORT_MAXIMUM, SHORT_POS
         };
   int16_t sCmpleDataArr[][2] =
         {
         SHORT_MINIMUM, SHORT_NEG,
         SHORT_NEG, SHORT_MINIMUM
         };
   int8_t bCmpeqDataArr[][2] =
         {
         BYTE_NEG, BYTE_POS,
         BYTE_POS, BYTE_POS
         };
   int8_t bCmpgtDataArr[][2] =
         {
         BYTE_ZERO, BYTE_POS,
         BYTE_POS, BYTE_ZERO
         };
   int16_t ifScmpeqDataArr[][2] =
         {
         SHORT_NEG, SHORT_POS,
         SHORT_POS, SHORT_POS
         };
   int16_t ifScmpneDataArr[][2] =
         {
         SHORT_POS, SHORT_MINIMUM,
         SHORT_POS, SHORT_POS
         };
   int16_t ifScmpgtDataArr[][2] =
         {
         SHORT_ZERO, SHORT_POS,
         SHORT_POS, SHORT_ZERO
         };
   int16_t ifScmpltDataArr[][2] =
         {
         SHORT_MAXIMUM, SHORT_ZERO,
         SHORT_ZERO, SHORT_MAXIMUM
         };
   int16_t ifScmpgeDataArr[][2] =
         {
         SHORT_POS, SHORT_MAXIMUM,
         SHORT_MAXIMUM, SHORT_POS
         };
   int16_t ifScmpleDataArr[][2] =
         {
         SHORT_MINIMUM, SHORT_NEG,
         SHORT_NEG, SHORT_MINIMUM
         };
   int8_t ifBcmpeqDataArr[][2] =
         {
         BYTE_NEG, BYTE_POS,
         BYTE_POS, BYTE_POS
         };
   int8_t ifBcmpgtDataArr[][2] =
         {
         BYTE_ZERO, BYTE_POS,
         BYTE_POS, BYTE_ZERO
         };
   uint32_t iuCmpgeDataArr[][2] =
         {
         UINT_MAXIMUM, UINT_MAXIMUM,
         UINT_POS, UINT_MINIMUM,
         UINT_MINIMUM, UINT_POS
         };
   uint8_t ifBuCmpgtDataArr[][2] =
         {
         UBYTE_POS, UBYTE_POS,
         UBYTE_MAXIMUM, UBYTE_POS
         };
   uint8_t ifBuCmpltDataArr[][2] =
         {
         UBYTE_MAXIMUM, UBYTE_MINIMUM,
         UBYTE_MINIMUM, UBYTE_MAXIMUM
         };
   uint8_t ifBuCmpgeDataArr[][2] =
         {
         UBYTE_MINIMUM, UBYTE_MAXIMUM,
         UBYTE_MAXIMUM, UBYTE_MINIMUM
         };
   uint8_t ifBuCmpleDataArr[][2] =
         {
         UBYTE_MINIMUM, UBYTE_POS,
         UBYTE_POS, UBYTE_MINIMUM
         };
   uint16_t suCmpltDataArr[][2] =
         {
         USHORT_MAXIMUM, USHORT_MINIMUM,
         USHORT_MINIMUM, USHORT_MAXIMUM
         };
   uint16_t suCmpleDataArr[][2] =
         {
         USHORT_MAXIMUM, USHORT_POS,
         USHORT_POS, USHORT_MAXIMUM
         };
   uint16_t suCmpgtDataArr[][2] =
         {
         USHORT_MINIMUM, USHORT_MAXIMUM,
         USHORT_MAXIMUM, USHORT_MINIMUM
         };
   uint16_t suCmpgeDataArr[][2] =
         {
         USHORT_MAXIMUM, USHORT_MINIMUM,
         USHORT_MINIMUM, USHORT_MAXIMUM
         };
   uint16_t ifSuCmpgtDataArr[][2] =
         {
         USHORT_POS, USHORT_POS,
         USHORT_MAXIMUM, USHORT_POS
         };
   uint16_t ifSuCmpltDataArr[][2] =
         {
         USHORT_MAXIMUM, USHORT_MINIMUM,
         USHORT_MINIMUM, USHORT_MAXIMUM
         };
   uint16_t ifSuCmpgeDataArr[][2] =
         {
         USHORT_MINIMUM, USHORT_MAXIMUM,
         USHORT_MAXIMUM, USHORT_MINIMUM
         };
   uint16_t ifSuCmpleDataArr[][2] =
         {
         USHORT_MINIMUM, USHORT_POS,
         USHORT_POS, USHORT_MINIMUM
         };

   int32_t rc = 0;
   int32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharSS_I_testMethodType * sCompareConst = 0;
   signatureCharBB_I_testMethodType * bCompareConst = 0;
   unsignedCompareSignatureCharII_I_testMethodType * iuCompareConst = 0;
   unsignedCompareSignatureCharBB_I_testMethodType * buCompareConst = 0;
   unsignedCompareSignatureCharSS_I_testMethodType * suCompareConst = 0;


   //sCompare
   testCaseNum = sizeof(sCmpeqDataArr) / sizeof(sCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmpeq, compareEQ(sCmpeqDataArr[i][0], sCmpeqDataArr[i][1]), _sCmpeq(sCmpeqDataArr[i][0], sCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpeqDataArr[i][0]), 2, &(sCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(sCmpeqDataArr[i][0], sCmpeqDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(sCmpeqDataArr[i][0], sCmpeqDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(sCmpeqDataArr[i][0], sCmpeqDataArr[i][1]), sCompareConst(sCmpeqDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(sCmpneDataArr) / sizeof(sCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmpne, compareNE(sCmpneDataArr[i][0], sCmpneDataArr[i][1]), _sCmpne(sCmpneDataArr[i][0], sCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpneDataArr[i][0]), 2, &(sCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(sCmpneDataArr[i][0], sCmpneDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(sCmpneDataArr[i][0], sCmpneDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(sCmpneDataArr[i][0], sCmpneDataArr[i][1]), sCompareConst(sCmpneDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(sCmpgtDataArr) / sizeof(sCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmpgt, compareGT(sCmpgtDataArr[i][0], sCmpgtDataArr[i][1]), _sCmpgt(sCmpgtDataArr[i][0], sCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpgtDataArr[i][0]), 2, &(sCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(sCmpgtDataArr[i][0], sCmpgtDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(sCmpgtDataArr[i][0], sCmpgtDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(sCmpgtDataArr[i][0], sCmpgtDataArr[i][1]), sCompareConst(sCmpgtDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(sCmpltDataArr) / sizeof(sCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmplt, compareLT(sCmpltDataArr[i][0], sCmpltDataArr[i][1]), _sCmplt(sCmpltDataArr[i][0], sCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpltDataArr[i][0]), 2, &(sCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(sCmpltDataArr[i][0], sCmpltDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(sCmpltDataArr[i][0], sCmpltDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(sCmpltDataArr[i][0], sCmpltDataArr[i][1]), sCompareConst(sCmpltDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(sCmpgeDataArr) / sizeof(sCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmpge, compareGE(sCmpgeDataArr[i][0], sCmpgeDataArr[i][1]), _sCmpge(sCmpgeDataArr[i][0], sCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpgeDataArr[i][0]), 2, &(sCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(sCmpgeDataArr[i][0], sCmpgeDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(sCmpgeDataArr[i][0], sCmpgeDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(sCmpgeDataArr[i][0], sCmpgeDataArr[i][1]), sCompareConst(sCmpgeDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(sCmpleDataArr) / sizeof(sCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sCmple, compareLE(sCmpleDataArr[i][0], sCmpleDataArr[i][1]), _sCmple(sCmpleDataArr[i][0], sCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(sCmpleDataArr[i][0]), 2, &(sCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(sCmpleDataArr[i][0], sCmpleDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(sCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(sCmpleDataArr[i][0], sCmpleDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, sCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "sCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::scmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(sCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(sCmpleDataArr[i][0], sCmpleDataArr[i][1]), sCompareConst(sCmpleDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   //bCompare
   testCaseNum = sizeof(bCmpeqDataArr) / sizeof(bCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmpeq, compareEQ(bCmpeqDataArr[i][0], bCmpeqDataArr[i][1]), _bCmpeq(bCmpeqDataArr[i][0], bCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpeqDataArr[i][0]), 2, &(bCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(bCmpeqDataArr[i][0], bCmpeqDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(bCmpeqDataArr[i][0], bCmpeqDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(bCmpeqDataArr[i][0], bCmpeqDataArr[i][1]), bCompareConst(bCmpeqDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(bCmpgtDataArr) / sizeof(bCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmpgt, compareGT(bCmpgtDataArr[i][0], bCmpgtDataArr[i][1]), _bCmpgt(bCmpgtDataArr[i][0], bCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpgtDataArr[i][0]), 2, &(bCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(bCmpgtDataArr[i][0], bCmpgtDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(bCmpgtDataArr[i][0], bCmpgtDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(bCmpgtDataArr[i][0], bCmpgtDataArr[i][1]), bCompareConst(bCmpgtDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   //ifsCompare
   testCaseNum = sizeof(ifScmpeqDataArr) / sizeof(ifScmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmpeq, compareEQ(ifScmpeqDataArr[i][0], ifScmpeqDataArr[i][1]), _ifScmpeq(ifScmpeqDataArr[i][0], ifScmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpeqDataArr[i][0]), 2, &(ifScmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(ifScmpeqDataArr[i][0], ifScmpeqDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(ifScmpeqDataArr[i][0], ifScmpeqDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpeq, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareEQ(ifScmpeqDataArr[i][0], ifScmpeqDataArr[i][1]), sCompareConst(ifScmpeqDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifScmpneDataArr) / sizeof(ifScmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmpne, compareNE(ifScmpneDataArr[i][0], ifScmpneDataArr[i][1]), _ifScmpne(ifScmpneDataArr[i][0], ifScmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpneDataArr[i][0]), 2, &(ifScmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(ifScmpneDataArr[i][0], ifScmpneDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(ifScmpneDataArr[i][0], ifScmpneDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpne, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareNE(ifScmpneDataArr[i][0], ifScmpneDataArr[i][1]), sCompareConst(ifScmpneDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifScmpgtDataArr) / sizeof(ifScmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmpgt, compareGT(ifScmpgtDataArr[i][0], ifScmpgtDataArr[i][1]), _ifScmpgt(ifScmpgtDataArr[i][0], ifScmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpgtDataArr[i][0]), 2, &(ifScmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(ifScmpgtDataArr[i][0], ifScmpgtDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(ifScmpgtDataArr[i][0], ifScmpgtDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGT(ifScmpgtDataArr[i][0], ifScmpgtDataArr[i][1]), sCompareConst(ifScmpgtDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifScmpltDataArr) / sizeof(ifScmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmplt, compareLT(ifScmpltDataArr[i][0], ifScmpltDataArr[i][1]), _ifScmplt(ifScmpltDataArr[i][0], ifScmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpltDataArr[i][0]), 2, &(ifScmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(ifScmpltDataArr[i][0], ifScmpltDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(ifScmpltDataArr[i][0], ifScmpltDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLT(ifScmpltDataArr[i][0], ifScmpltDataArr[i][1]), sCompareConst(ifScmpltDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifScmpgeDataArr) / sizeof(ifScmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmpge, compareGE(ifScmpgeDataArr[i][0], ifScmpgeDataArr[i][1]), _ifScmpge(ifScmpgeDataArr[i][0], ifScmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpgeDataArr[i][0]), 2, &(ifScmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(ifScmpgeDataArr[i][0], ifScmpgeDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(ifScmpgeDataArr[i][0], ifScmpgeDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareGE(ifScmpgeDataArr[i][0], ifScmpgeDataArr[i][1]), sCompareConst(ifScmpgeDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifScmpleDataArr) / sizeof(ifScmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifScmple, compareLE(ifScmpleDataArr[i][0], ifScmpleDataArr[i][1]), _ifScmple(ifScmpleDataArr[i][0], ifScmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifScmpleDataArr[i][0]), 2, &(ifScmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(ifScmpleDataArr[i][0], ifScmpleDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifScmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifScmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(ifScmpleDataArr[i][0], ifScmpleDataArr[i][1]), sCompareConst(SHORT_PLACEHOLDER_1, ifScmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifScmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sCompareConst,
            _numberOfBinaryArgs, TR::ifscmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifScmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(sCompareConst, compareLE(ifScmpleDataArr[i][0], ifScmpleDataArr[i][1]), sCompareConst(ifScmpleDataArr[i][0], SHORT_PLACEHOLDER_2));
      }

   //ifbCompare
   testCaseNum = sizeof(ifBcmpeqDataArr) / sizeof(ifBcmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmpeq, compareEQ(ifBcmpeqDataArr[i][0], ifBcmpeqDataArr[i][1]), _ifBcmpeq(ifBcmpeqDataArr[i][0], ifBcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpeqDataArr[i][0]), 2, &(ifBcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(ifBcmpeqDataArr[i][0], ifBcmpeqDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(ifBcmpeqDataArr[i][0], ifBcmpeqDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpeq, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareEQ(ifBcmpeqDataArr[i][0], ifBcmpeqDataArr[i][1]), bCompareConst(ifBcmpeqDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBcmpgtDataArr) / sizeof(ifBcmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmpgt, compareGT(ifBcmpgtDataArr[i][0], ifBcmpgtDataArr[i][1]), _ifBcmpgt(ifBcmpgtDataArr[i][0], ifBcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpgtDataArr[i][0]), 2, &(ifBcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(ifBcmpgtDataArr[i][0], ifBcmpgtDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(ifBcmpgtDataArr[i][0], ifBcmpgtDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGT(ifBcmpgtDataArr[i][0], ifBcmpgtDataArr[i][1]), bCompareConst(ifBcmpgtDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iuCmpgeDataArr) / sizeof(iuCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iuCmpge, compareGE(iuCmpgeDataArr[i][0], iuCmpgeDataArr[i][1]), _iuCmpge(iuCmpgeDataArr[i][0], iuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iuCmpgeDataArr[i][0]), 2, &(iuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(iuCmpgeDataArr[i][0], iuCmpgeDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iuCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iuCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(iuCmpgeDataArr[i][0], iuCmpgeDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, iuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(iuCmpgeDataArr[i][0], iuCmpgeDataArr[i][1]), iuCompareConst(iuCmpgeDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(suCmpgtDataArr) / sizeof(suCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_suCmpgt, compareGT(suCmpgtDataArr[i][0], suCmpgtDataArr[i][1]), _suCmpgt(suCmpgtDataArr[i][0], suCmpgtDataArr[i][1])) << suCmpgtDataArr[i][0] << " : " << suCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(suCmpgtDataArr[i][0]), 2, &(suCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(suCmpgtDataArr[i][0], suCmpgtDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << suCmpgtDataArr[i][0] << " : " << suCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(suCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(suCmpgtDataArr[i][0], suCmpgtDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, suCmpgtDataArr[i][1])) << suCmpgtDataArr[i][0] << " : " << suCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(suCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(suCmpgtDataArr[i][0], suCmpgtDataArr[i][1]), suCompareConst(suCmpgtDataArr[i][0], SHORT_PLACEHOLDER_2)) << suCmpgtDataArr[i][0] << " : " << suCmpgtDataArr[i][1];
      }

   testCaseNum = sizeof(suCmpltDataArr) / sizeof(suCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_suCmplt, compareLT(suCmpltDataArr[i][0], suCmpltDataArr[i][1]), _suCmplt(suCmpltDataArr[i][0], suCmpltDataArr[i][1])) << suCmpltDataArr[i][0] << " : " << suCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(suCmpltDataArr[i][0]), 2, &(suCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(suCmpltDataArr[i][0], suCmpltDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << suCmpltDataArr[i][0] << " : " << suCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(suCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(suCmpltDataArr[i][0], suCmpltDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, suCmpltDataArr[i][1])) << suCmpltDataArr[i][0] << " : " << suCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(suCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(suCmpltDataArr[i][0], suCmpltDataArr[i][1]), suCompareConst(suCmpltDataArr[i][0], SHORT_PLACEHOLDER_2)) << suCmpltDataArr[i][0] << " : " << suCmpltDataArr[i][1];
      }

   testCaseNum = sizeof(suCmpgeDataArr) / sizeof(suCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_suCmpge, compareGE(suCmpgeDataArr[i][0], suCmpgeDataArr[i][1]), _suCmpge(suCmpgeDataArr[i][0], suCmpgeDataArr[i][1])) << suCmpgeDataArr[i][0] << " : " << suCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(suCmpgeDataArr[i][0]), 2, &(suCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(suCmpgeDataArr[i][0], suCmpgeDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << suCmpgeDataArr[i][0] << " : " << suCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(suCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(suCmpgeDataArr[i][0], suCmpgeDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, suCmpgeDataArr[i][1])) << suCmpgeDataArr[i][0] << " : " << suCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(suCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(suCmpgeDataArr[i][0], suCmpgeDataArr[i][1]), suCompareConst(suCmpgeDataArr[i][0], SHORT_PLACEHOLDER_2)) << suCmpgeDataArr[i][0] << " : " << suCmpgeDataArr[i][1];
      }

   testCaseNum = sizeof(suCmpleDataArr) / sizeof(suCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_suCmple, compareLE(suCmpleDataArr[i][0], suCmpleDataArr[i][1]), _suCmple(suCmpleDataArr[i][0], suCmpleDataArr[i][1])) << suCmpleDataArr[i][0] << " : " << suCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(suCmpleDataArr[i][0]), 2, &(suCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(suCmpleDataArr[i][0], suCmpleDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << suCmpleDataArr[i][0] << " : " << suCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(suCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(suCmpleDataArr[i][0], suCmpleDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, suCmpleDataArr[i][1])) << suCmpleDataArr[i][0] << " : " << suCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "suCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::sucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(suCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(suCmpleDataArr[i][0], suCmpleDataArr[i][1]), suCompareConst(suCmpleDataArr[i][0], SHORT_PLACEHOLDER_2)) << suCmpleDataArr[i][0] << " : " << suCmpleDataArr[i][1];
      }

   //ifBuCompare
   testCaseNum = sizeof(ifBuCmpgtDataArr) / sizeof(ifBuCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBuCmpgt, compareGT(ifBuCmpgtDataArr[i][0], ifBuCmpgtDataArr[i][1]), _ifBuCmpgt(ifBuCmpgtDataArr[i][0], ifBuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBuCmpgtDataArr[i][0]), 2, &(ifBuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGT(ifBuCmpgtDataArr[i][0], ifBuCmpgtDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBuCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBuCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGT(ifBuCmpgtDataArr[i][0], ifBuCmpgtDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, ifBuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpgt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGT(ifBuCmpgtDataArr[i][0], ifBuCmpgtDataArr[i][1]), buCompareConst(ifBuCmpgtDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBuCmpltDataArr) / sizeof(ifBuCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBuCmplt, compareLT(ifBuCmpltDataArr[i][0], ifBuCmpltDataArr[i][1]), _ifBuCmplt(ifBuCmpltDataArr[i][0], ifBuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBuCmpltDataArr[i][0]), 2, &(ifBuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLT(ifBuCmpltDataArr[i][0], ifBuCmpltDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBuCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBuCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLT(ifBuCmpltDataArr[i][0], ifBuCmpltDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, ifBuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLT(ifBuCmpltDataArr[i][0], ifBuCmpltDataArr[i][1]), buCompareConst(ifBuCmpltDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBuCmpgeDataArr) / sizeof(ifBuCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBuCmpge, compareGE(ifBuCmpgeDataArr[i][0], ifBuCmpgeDataArr[i][1]), _ifBuCmpge(ifBuCmpgeDataArr[i][0], ifBuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBuCmpgeDataArr[i][0]), 2, &(ifBuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGE(ifBuCmpgeDataArr[i][0], ifBuCmpgeDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBuCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBuCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGE(ifBuCmpgeDataArr[i][0], ifBuCmpgeDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, ifBuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareGE(ifBuCmpgeDataArr[i][0], ifBuCmpgeDataArr[i][1]), buCompareConst(ifBuCmpgeDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBuCmpleDataArr) / sizeof(ifBuCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBuCmple, compareLE(ifBuCmpleDataArr[i][0], ifBuCmpleDataArr[i][1]), _ifBuCmple(ifBuCmpleDataArr[i][0], ifBuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBuCmpleDataArr[i][0]), 2, &(ifBuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLE(ifBuCmpleDataArr[i][0], ifBuCmpleDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBuCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBuCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLE(ifBuCmpleDataArr[i][0], ifBuCmpleDataArr[i][1]), buCompareConst(BYTE_PLACEHOLDER_1, ifBuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBuCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(buCompareConst,
            _numberOfBinaryArgs, TR::ifbucmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(buCompareConst, compareLE(ifBuCmpleDataArr[i][0], ifBuCmpleDataArr[i][1]), buCompareConst(ifBuCmpleDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifSuCmpgtDataArr) / sizeof(ifSuCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifSuCmpgt, compareGT(ifSuCmpgtDataArr[i][0], ifSuCmpgtDataArr[i][1]), _ifSuCmpgt(ifSuCmpgtDataArr[i][0], ifSuCmpgtDataArr[i][1])) << ifSuCmpgtDataArr[i][0] << " : " << ifSuCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifSuCmpgtDataArr[i][0]), 2, &(ifSuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(ifSuCmpgtDataArr[i][0], ifSuCmpgtDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << ifSuCmpgtDataArr[i][0] << " : " << ifSuCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifSuCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(ifSuCmpgtDataArr[i][0], ifSuCmpgtDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, ifSuCmpgtDataArr[i][1])) << ifSuCmpgtDataArr[i][0] << " : " << ifSuCmpgtDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpgt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifSuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGT(ifSuCmpgtDataArr[i][0], ifSuCmpgtDataArr[i][1]), suCompareConst(ifSuCmpgtDataArr[i][0], SHORT_PLACEHOLDER_2)) << ifSuCmpgtDataArr[i][0] << " : " << ifSuCmpgtDataArr[i][1];
      }

   testCaseNum = sizeof(ifSuCmpltDataArr) / sizeof(ifSuCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifSuCmplt, compareLT(ifSuCmpltDataArr[i][0], ifSuCmpltDataArr[i][1]), _ifSuCmplt(ifSuCmpltDataArr[i][0], ifSuCmpltDataArr[i][1])) << ifSuCmpltDataArr[i][0] << " : " << ifSuCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifSuCmpltDataArr[i][0]), 2, &(ifSuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(ifSuCmpltDataArr[i][0], ifSuCmpltDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << ifSuCmpltDataArr[i][0] << " : " << ifSuCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifSuCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(ifSuCmpltDataArr[i][0], ifSuCmpltDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, ifSuCmpltDataArr[i][1])) << ifSuCmpltDataArr[i][0] << " : " << ifSuCmpltDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmplt, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifSuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLT(ifSuCmpltDataArr[i][0], ifSuCmpltDataArr[i][1]), suCompareConst(ifSuCmpltDataArr[i][0], SHORT_PLACEHOLDER_2)) << ifSuCmpltDataArr[i][0] << " : " << ifSuCmpltDataArr[i][1];
      }

   testCaseNum = sizeof(ifSuCmpgeDataArr) / sizeof(ifSuCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifSuCmpge, compareGE(ifSuCmpgeDataArr[i][0], ifSuCmpgeDataArr[i][1]), _ifSuCmpge(ifSuCmpgeDataArr[i][0], ifSuCmpgeDataArr[i][1])) << ifSuCmpgeDataArr[i][0] << " : " << ifSuCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifSuCmpgeDataArr[i][0]), 2, &(ifSuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(ifSuCmpgeDataArr[i][0], ifSuCmpgeDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << ifSuCmpgeDataArr[i][0] << " : " << ifSuCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifSuCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(ifSuCmpgeDataArr[i][0], ifSuCmpgeDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, ifSuCmpgeDataArr[i][1])) << ifSuCmpgeDataArr[i][0] << " : " << ifSuCmpgeDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmpge, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifSuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareGE(ifSuCmpgeDataArr[i][0], ifSuCmpgeDataArr[i][1]), suCompareConst(ifSuCmpgeDataArr[i][0], SHORT_PLACEHOLDER_2)) << ifSuCmpgeDataArr[i][0] << " : " << ifSuCmpgeDataArr[i][1];
      }

   testCaseNum = sizeof(ifSuCmpleDataArr) / sizeof(ifSuCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifSuCmple, compareLE(ifSuCmpleDataArr[i][0], ifSuCmpleDataArr[i][1]), _ifSuCmple(ifSuCmpleDataArr[i][0], ifSuCmpleDataArr[i][1])) << ifSuCmpleDataArr[i][0] << " : " << ifSuCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 4, 1, &(ifSuCmpleDataArr[i][0]), 2, &(ifSuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(ifSuCmpleDataArr[i][0], ifSuCmpleDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2)) << ifSuCmpleDataArr[i][0] << " : " << ifSuCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 1, &(ifSuCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(ifSuCmpleDataArr[i][0], ifSuCmpleDataArr[i][1]), suCompareConst(SHORT_PLACEHOLDER_1, ifSuCmpleDataArr[i][1])) << ifSuCmpleDataArr[i][0] << " : " << ifSuCmpleDataArr[i][1];

      sprintf(resolvedMethodName, "ifSuCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(suCompareConst,
            _numberOfBinaryArgs, TR::ifsucmple, resolvedMethodName, _argTypesBinaryShort, TR::Int32, rc, 2, 2, &(ifSuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(suCompareConst, compareLE(ifSuCmpleDataArr[i][0], ifSuCmpleDataArr[i][1]), suCompareConst(ifSuCmpleDataArr[i][0], SHORT_PLACEHOLDER_2)) << ifSuCmpleDataArr[i][0] << " : " << ifSuCmpleDataArr[i][1];
      }
   }

void
PPCOpCodesTest::invokeAddressTests()
   {
   int32_t rc = 0;

   int8_t byteDataArr[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   int16_t shortDataArr[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int32_t intDataArr[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   uint8_t ubyteDataArr[] = {UBYTE_POS, UBYTE_MAXIMUM, UBYTE_MINIMUM};
   uint64_t ulongDataArr[] = {ULONG_POS, ULONG_MAXIMUM, ULONG_MINIMUM};
   uintptr_t aUnaryDataArr[] =
      {
      (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,
      (uintptr_t) &LONG_POS,
      (uintptr_t) &LONG_MAXIMUM,
      (uintptr_t) &LONG_ZERO
      };

   uintptr_t acmpeqDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t acmpneDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_POS
      };
   uintptr_t acmpltDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t acmpgeDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t acmpleDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MAXIMUM
      };
   uintptr_t acmpgtDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_POS
      };
   uintptr_t ifacmpeqDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t ifacmpneDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_POS
      };
   uintptr_t ifacmpltDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t ifacmpgeDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MINIMUM
      };
   uintptr_t ifacmpleDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_MAXIMUM
      };
   uintptr_t ifacmpgtDataArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM,  (uintptr_t) &INT_MINIMUM,
      (uintptr_t) &INT_MINIMUM,  (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,   (uintptr_t) &INT_POS
      };

   int32_t aselectChild1Arr[] =
      {
      INT_MAXIMUM,
      INT_MAXIMUM,
      INT_MAXIMUM,
      INT_MINIMUM,
      INT_MINIMUM,
      INT_MINIMUM,
      INT_NEG,
      INT_NEG,
      INT_NEG,
      INT_POS,
      INT_POS,
      INT_POS,
      INT_ZERO,
      INT_ZERO,
      INT_ZERO
      };
   uintptr_t aselectArr[][2] =
      {
      (uintptr_t) &INT_MAXIMUM, (uintptr_t) &INT_ZERO,
      (uintptr_t) &INT_POS,     (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,    (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MAXIMUM, (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,     (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,    (uintptr_t) &INT_ZERO,
      (uintptr_t) &INT_MAXIMUM, (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_POS,     (uintptr_t) &INT_ZERO,
      (uintptr_t) &INT_ZERO,    (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MAXIMUM, (uintptr_t) &INT_POS,
      (uintptr_t) &INT_POS,     (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,    (uintptr_t) &INT_ZERO,
      (uintptr_t) &INT_MAXIMUM, (uintptr_t) &INT_ZERO,
      (uintptr_t) &INT_POS,     (uintptr_t) &INT_POS,
      (uintptr_t) &INT_ZERO,    (uintptr_t) &INT_MAXIMUM
      };

   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   signatureCharB_L_testMethodType *b2aConst = 0;
   signatureCharS_L_testMethodType *s2aConst = 0;
   signatureCharI_L_testMethodType *i2aConst = 0;
   signatureCharL_J_testMethodType *a2lConst = 0;
   signatureCharL_S_testMethodType *a2sConst = 0;
   signatureCharL_B_testMethodType *a2bConst = 0;
   unsignedSignatureCharB_L_testMethodType *bu2aConst = 0;
   unsignedSignatureCharJ_L_testMethodType *lu2aConst = 0;

   signatureCharLL_I_testMethodType *aCompareConst = 0;
   signatureCharILL_L_testMethodType *aSelectConst = 0;



   //address convert unary opcodes
   testCaseNum = sizeof(byteDataArr) / sizeof(byteDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_b2a, convert(byteDataArr[i], ADDRESS_PLACEHOLDER_1), _b2a(byteDataArr[i]));

      sprintf(resolvedMethodName, "b2aConst%d", i + 1);
      compileOpCodeMethod(b2aConst,
            _numberOfUnaryArgs, TR::b2a, resolvedMethodName, _argTypesUnaryByte, TR::Address, rc, 2, 1, &byteDataArr[i]);
      OMR_CT_EXPECT_EQ(b2aConst, convert(byteDataArr[i], ADDRESS_PLACEHOLDER_1), b2aConst(BYTE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(shortDataArr) / sizeof(shortDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_s2a, convert(shortDataArr[i], ADDRESS_PLACEHOLDER_1), _s2a(shortDataArr[i]));

      sprintf(resolvedMethodName, "s2aConst%d", i + 1);
      compileOpCodeMethod(s2aConst,
            _numberOfUnaryArgs, TR::s2a, resolvedMethodName, _argTypesUnaryShort, TR::Address, rc, 2, 1, &shortDataArr[i]);
      OMR_CT_EXPECT_EQ(s2aConst, convert(shortDataArr[i], ADDRESS_PLACEHOLDER_1), s2aConst(SHORT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(intDataArr) / sizeof(intDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_i2a, convert(intDataArr[i], ADDRESS_PLACEHOLDER_1), _i2a(intDataArr[i]));

      sprintf(resolvedMethodName, "i2aConst%d", i + 1);
      compileOpCodeMethod(i2aConst,
            _numberOfUnaryArgs, TR::i2a, resolvedMethodName, _argTypesUnaryInt, TR::Address, rc, 2, 1, &intDataArr[i]);
      OMR_CT_EXPECT_EQ(i2aConst, convert(intDataArr[i], ADDRESS_PLACEHOLDER_1), i2aConst(INT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(ubyteDataArr) / sizeof(ubyteDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bu2a, convert(ubyteDataArr[i], ADDRESS_PLACEHOLDER_1), _bu2a(ubyteDataArr[i]));

      sprintf(resolvedMethodName, "bu2aConst%d", i + 1);
      compileOpCodeMethod(bu2aConst,
            _numberOfUnaryArgs, TR::bu2a, resolvedMethodName, _argTypesUnaryByte, TR::Address, rc, 2, 1, &ubyteDataArr[i]);
      OMR_CT_EXPECT_EQ(bu2aConst, convert(ubyteDataArr[i], ADDRESS_PLACEHOLDER_1), bu2aConst(BYTE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(ulongDataArr) / sizeof(ulongDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_lu2a, convert(ulongDataArr[i], ADDRESS_PLACEHOLDER_1), _lu2a(ulongDataArr[i]));

      sprintf(resolvedMethodName, "lu2aConst%d", i + 1);
      compileOpCodeMethod(lu2aConst,
            _numberOfUnaryArgs, TR::lu2a, resolvedMethodName, _argTypesUnaryLong, TR::Address, rc, 2, 1, &ulongDataArr[i]);
      OMR_CT_EXPECT_EQ(lu2aConst, convert(ulongDataArr[i], ADDRESS_PLACEHOLDER_1), lu2aConst(LONG_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_a2l, convert(aUnaryDataArr[i], LONG_POS), _a2l(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "a2lConst%d", i + 1);
      compileOpCodeMethod(a2lConst, _numberOfUnaryArgs, TR::a2l, resolvedMethodName, _argTypesUnaryAddress, TR::Int64, rc, 2, 1, &aUnaryDataArr[i]);
      OMR_CT_EXPECT_EQ(a2lConst, convert(aUnaryDataArr[i], LONG_POS), a2lConst(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_a2s, convert(aUnaryDataArr[i], SHORT_POS), _a2s(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "a2sConst%d", i + 1);
      compileOpCodeMethod(a2sConst,
            _numberOfUnaryArgs, TR::a2s, resolvedMethodName, _argTypesUnaryAddress, TR::Int16, rc, 2, 1, &aUnaryDataArr[i]);
      OMR_CT_EXPECT_EQ(a2sConst, convert(aUnaryDataArr[i], SHORT_POS), a2sConst(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_a2b, convert(aUnaryDataArr[i], BYTE_POS), _a2b(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "a2bConst%d", i + 1);
      compileOpCodeMethod(a2bConst,
            _numberOfUnaryArgs, TR::a2b, resolvedMethodName, _argTypesUnaryAddress, TR::Int8, rc, 2, 1, &aUnaryDataArr[i]);
      OMR_CT_EXPECT_EQ(a2bConst, convert(aUnaryDataArr[i], BYTE_POS), a2bConst(ADDRESS_PLACEHOLDER_1));
      }

   //address compare
   testCaseNum = sizeof(acmpeqDataArr) / sizeof(acmpeqDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_acmpeq, compareEQ(acmpeqDataArr[i][0], acmpeqDataArr[i][1]), _acmpeq(acmpeqDataArr[i][0], acmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "aCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpeqDataArr[i][0]), 2, &(acmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(acmpeqDataArr[i][0], acmpeqDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "aCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(acmpeqDataArr[i][0], acmpeqDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "aCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(acmpeqDataArr[i][0], acmpeqDataArr[i][1]), aCompareConst(acmpeqDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(acmpneDataArr) / sizeof(acmpneDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_acmpne, compareNE(acmpneDataArr[i][0], acmpneDataArr[i][1]), _acmpne(acmpneDataArr[i][0], acmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "aCmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpneDataArr[i][0]), 2, &(acmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(acmpneDataArr[i][0], acmpneDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "aCmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(acmpneDataArr[i][0], acmpneDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "aCmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(acmpneDataArr[i][0], acmpneDataArr[i][1]), aCompareConst(acmpneDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(acmpgtDataArr) / sizeof(acmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_acmpgt, compareGT(acmpgtDataArr[i][0], acmpgtDataArr[i][1]), _acmpgt(acmpgtDataArr[i][0], acmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpgtDataArr[i][0]), 2, &(acmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(acmpgtDataArr[i][0], acmpgtDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "acmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(acmpgtDataArr[i][0], acmpgtDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(acmpgtDataArr[i][0], acmpgtDataArr[i][1]), aCompareConst(acmpgtDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(acmpltDataArr) / sizeof(acmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_acmplt, compareLT(acmpltDataArr[i][0], acmpltDataArr[i][1]), _acmplt(acmpltDataArr[i][0], acmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpltDataArr[i][0]), 2, &(acmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(acmpltDataArr[i][0], acmpltDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "acmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(acmpltDataArr[i][0], acmpltDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(acmpltDataArr[i][0], acmpltDataArr[i][1]), aCompareConst(acmpltDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(acmpgeDataArr) / sizeof(acmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_acmpge, compareGE(acmpgeDataArr[i][0], acmpgeDataArr[i][1]), _acmpge(acmpgeDataArr[i][0], acmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpgeDataArr[i][0]), 2, &(acmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(acmpgeDataArr[i][0], acmpgeDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "acmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(acmpgeDataArr[i][0], acmpgeDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(acmpgeDataArr[i][0], acmpgeDataArr[i][1]), aCompareConst(acmpgeDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(acmpleDataArr) / sizeof(acmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_acmple, compareLE(acmpleDataArr[i][0], acmpleDataArr[i][1]), _acmple(acmpleDataArr[i][0], acmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(acmpleDataArr[i][0]), 2, &(acmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(acmpleDataArr[i][0], acmpleDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "acmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(acmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(acmpleDataArr[i][0], acmpleDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, acmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "acmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::acmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(acmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(acmpleDataArr[i][0], acmpleDataArr[i][1]), aCompareConst(acmpleDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   //address ifcompare
   testCaseNum = sizeof(ifacmpeqDataArr) / sizeof(ifacmpeqDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_ifacmpeq, compareEQ(ifacmpeqDataArr[i][0], ifacmpeqDataArr[i][1]), _ifacmpeq(ifacmpeqDataArr[i][0], ifacmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpeqDataArr[i][0]), 2, &(ifacmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(ifacmpeqDataArr[i][0], ifacmpeqDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(ifacmpeqDataArr[i][0], ifacmpeqDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpeq, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareEQ(ifacmpeqDataArr[i][0], ifacmpeqDataArr[i][1]), aCompareConst(ifacmpeqDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifacmpneDataArr) / sizeof(ifacmpneDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_ifacmpne, compareNE(ifacmpneDataArr[i][0], ifacmpneDataArr[i][1]), _ifacmpne(ifacmpneDataArr[i][0], ifacmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpneDataArr[i][0]), 2, &(ifacmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(ifacmpneDataArr[i][0], ifacmpneDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(ifacmpneDataArr[i][0], ifacmpneDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpne, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareNE(ifacmpneDataArr[i][0], ifacmpneDataArr[i][1]), aCompareConst(ifacmpneDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifacmpgtDataArr) / sizeof(ifacmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifacmpgt, compareGT(ifacmpgtDataArr[i][0], ifacmpgtDataArr[i][1]), _ifacmpgt(ifacmpgtDataArr[i][0], ifacmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpgtDataArr[i][0]), 2, &(ifacmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(ifacmpgtDataArr[i][0], ifacmpgtDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(ifacmpgtDataArr[i][0], ifacmpgtDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpgt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGT(ifacmpgtDataArr[i][0], ifacmpgtDataArr[i][1]), aCompareConst(ifacmpgtDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifacmpltDataArr) / sizeof(ifacmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifacmplt, compareLT(ifacmpltDataArr[i][0], ifacmpltDataArr[i][1]), _ifacmplt(ifacmpltDataArr[i][0], ifacmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpltDataArr[i][0]), 2, &(ifacmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(ifacmpltDataArr[i][0], ifacmpltDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(ifacmpltDataArr[i][0], ifacmpltDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmplt, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLT(ifacmpltDataArr[i][0], ifacmpltDataArr[i][1]), aCompareConst(ifacmpltDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifacmpgeDataArr) / sizeof(ifacmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifacmpge, compareGE(ifacmpgeDataArr[i][0], ifacmpgeDataArr[i][1]), _ifacmpge(ifacmpgeDataArr[i][0], ifacmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpgeDataArr[i][0]), 2, &(ifacmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(ifacmpgeDataArr[i][0], ifacmpgeDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(ifacmpgeDataArr[i][0], ifacmpgeDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmpge, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareGE(ifacmpgeDataArr[i][0], ifacmpgeDataArr[i][1]), aCompareConst(ifacmpgeDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifacmpleDataArr) / sizeof(ifacmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifacmple, compareLE(ifacmpleDataArr[i][0], ifacmpleDataArr[i][1]), _ifacmple(ifacmpleDataArr[i][0], ifacmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 4, 1, &(ifacmpleDataArr[i][0]), 2, &(ifacmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(ifacmpleDataArr[i][0], ifacmpleDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifacmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 1, &(ifacmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(ifacmpleDataArr[i][0], ifacmpleDataArr[i][1]), aCompareConst(ADDRESS_PLACEHOLDER_1, ifacmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifacmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aCompareConst,
            _numberOfBinaryArgs, TR::ifacmple, resolvedMethodName, _argTypesBinaryAddress, TR::Int32, rc, 2, 2, &(ifacmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(aCompareConst, compareLE(ifacmpleDataArr[i][0], ifacmpleDataArr[i][1]), aCompareConst(ifacmpleDataArr[i][0], ADDRESS_PLACEHOLDER_2));
      }

   TR_ASSERT((sizeof(aselectChild1Arr) / sizeof(aselectChild1Arr[0])) == sizeof(aselectArr) / sizeof(aselectArr[0]),
         "Child1 array size is not equal to Child2 and Child3 array");
   testCaseNum = sizeof(aselectChild1Arr) / sizeof(aselectChild1Arr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_aselect, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), _aselect(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]));

      sprintf(resolvedMethodName, "aSelectConst1_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 6, 1, &aselectChild1Arr[i], 2, &aselectArr[i][0], 3, &aselectArr[i][1]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(INT_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2, ADDRESS_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "aSelectConst2_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 4, 1, &aselectChild1Arr[i], 2, &aselectArr[i][0]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(INT_PLACEHOLDER_1, ADDRESS_PLACEHOLDER_2, aselectArr[i][1]));

      sprintf(resolvedMethodName, "aSelectConst3_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 4, 1, &aselectChild1Arr[i], 3, &aselectArr[i][1]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(INT_PLACEHOLDER_1, aselectArr[i][0], ADDRESS_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "aSelectConst4_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 4, 2, &aselectArr[i][0], 3, &aselectArr[i][1]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(aselectChild1Arr[i], ADDRESS_PLACEHOLDER_2, ADDRESS_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "aSelectConst5_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 2, 1, &aselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(INT_PLACEHOLDER_1, aselectArr[i][0], aselectArr[i][1]));

      sprintf(resolvedMethodName, "aSelectConst6_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 2, 2, &aselectArr[i][0]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(aselectChild1Arr[i], ADDRESS_PLACEHOLDER_1, aselectArr[i][1]));

      sprintf(resolvedMethodName, "aSelectConst7_TestCase%d", i + 1);
      compileOpCodeMethod(aSelectConst,
            _numberOfSelectArgs, TR::aselect, resolvedMethodName, _argTypesSelectAddress, TR::Address, rc, 2, 3, &aselectArr[i][1]);
      OMR_CT_EXPECT_EQ(aSelectConst, select(aselectChild1Arr[i], aselectArr[i][0], aselectArr[i][1]), aSelectConst(aselectChild1Arr[i], aselectArr[i][0], ADDRESS_PLACEHOLDER_1));
      }

   }

void
PPCOpCodesTest::invokeSelectTests()
   {
   int32_t rc = 0;
   int32_t bselectChild1Arr[] =
      {
      INT_MAXIMUM, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO, INT_ZERO, INT_MINIMUM, INT_POS, INT_NEG,
      INT_NEG, INT_NEG, INT_ZERO, INT_POS, INT_MAXIMUM, INT_NEG, INT_ZERO, INT_MINIMUM, INT_POS
      };
   int32_t sselectChild1Arr[] =
      {
      INT_MAXIMUM, INT_MINIMUM, INT_POS, INT_ZERO, INT_NEG, INT_MINIMUM, INT_POS, INT_MAXIMUM, INT_ZERO,
      INT_NEG, INT_NEG, INT_ZERO, INT_POS, INT_NEG, INT_ZERO, INT_MINIMUM, INT_MAXIMUM, INT_POS
      };

   int8_t byteDataArr[][2] =
      {
      BYTE_NEG, BYTE_MINIMUM,
      BYTE_NEG, BYTE_POS,
      BYTE_NEG, BYTE_POS,
      BYTE_MAXIMUM, BYTE_ZERO,
      BYTE_ZERO, BYTE_POS,
      BYTE_ZERO, BYTE_POS,
      BYTE_ZERO, BYTE_MAXIMUM,
      BYTE_POS, BYTE_NEG,
      BYTE_POS, BYTE_MINIMUM,
      BYTE_MAXIMUM, BYTE_POS,
      BYTE_MINIMUM, BYTE_ZERO,
      BYTE_MINIMUM, BYTE_MAXIMUM,
      BYTE_NEG, BYTE_MAXIMUM,
      BYTE_ZERO, BYTE_NEG,
      BYTE_POS, BYTE_ZERO,
      BYTE_MAXIMUM, BYTE_NEG,
      BYTE_ZERO, BYTE_MINIMUM,
      BYTE_MINIMUM, BYTE_POS
      };

   int16_t shortDataArr[][2] =
      {
      SHORT_NEG, SHORT_MINIMUM,
      SHORT_NEG, SHORT_POS,
      SHORT_NEG, SHORT_POS,
      SHORT_MAXIMUM, SHORT_ZERO,
      SHORT_ZERO, SHORT_POS,
      SHORT_ZERO, SHORT_POS,
      SHORT_ZERO, SHORT_MAXIMUM,
      SHORT_POS, SHORT_NEG,
      SHORT_POS, SHORT_MINIMUM,
      SHORT_MAXIMUM, SHORT_POS,
      SHORT_MINIMUM, SHORT_ZERO,
      SHORT_MINIMUM, SHORT_MAXIMUM,
      SHORT_NEG, SHORT_MAXIMUM,
      SHORT_ZERO, SHORT_NEG,
      SHORT_POS, SHORT_ZERO,
      SHORT_MAXIMUM, SHORT_NEG,
      SHORT_ZERO, SHORT_MINIMUM,
      SHORT_MINIMUM, SHORT_POS
      };

   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNumCheck = 0;
   uint32_t testCaseNum = 0;

   signatureCharIBB_B_testMethodType * bSelectConst = 0;
   signatureCharISS_S_testMethodType * sSelectConst = 0;

   testCaseNum = sizeof(bselectChild1Arr) / sizeof(bselectChild1Arr[0]);
   testCaseNumCheck = sizeof(byteDataArr) / sizeof(byteDataArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in bselect input array");
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bselect, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), _bselect(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]));

      sprintf(resolvedMethodName, "bSelectConst1_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 6, 1, &bselectChild1Arr[i], 2, &byteDataArr[i][0], 3, &byteDataArr[i][1]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2, BYTE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "bSelectConst2_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 4, 1, &bselectChild1Arr[i], 2, &byteDataArr[i][0]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2, byteDataArr[i][1]));

      sprintf(resolvedMethodName, "bSelectConst3_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 4, 1, &bselectChild1Arr[i], 3, &byteDataArr[i][1]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(BYTE_PLACEHOLDER_1, byteDataArr[i][0], BYTE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "bSelectConst4_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 4, 2, &byteDataArr[i][0], 3, &byteDataArr[i][1]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(bselectChild1Arr[i], BYTE_PLACEHOLDER_2, BYTE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "bSelectConst5_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 2, 1, &bselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(BYTE_PLACEHOLDER_1, byteDataArr[i][0], byteDataArr[i][1]));

      sprintf(resolvedMethodName, "bSelectConst6_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 2, 2, &byteDataArr[i][0]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(bselectChild1Arr[i], BYTE_PLACEHOLDER_1, byteDataArr[i][1]));

      sprintf(resolvedMethodName, "bSelectConst7_Testcase%d", i + 1);
      compileOpCodeMethod(bSelectConst, _numberOfSelectArgs, TR::bselect,
            resolvedMethodName, _argTypesSelectByte, TR::Int8, rc, 2, 3, &byteDataArr[i][1]);
      OMR_CT_EXPECT_EQ(bSelectConst, select(bselectChild1Arr[i], byteDataArr[i][0], byteDataArr[i][1]), bSelectConst(bselectChild1Arr[i], byteDataArr[i][0], BYTE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(sselectChild1Arr) / sizeof(sselectChild1Arr[0]);
   testCaseNumCheck = sizeof(shortDataArr) / sizeof(shortDataArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in sselect input array");
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_sselect, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), _sselect(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]));

      sprintf(resolvedMethodName, "sSelectConst1_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 6, 1, &sselectChild1Arr[i], 2, &shortDataArr[i][0], 3, &shortDataArr[i][1]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2, SHORT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "sSelectConst2_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 4, 1, &sselectChild1Arr[i], 2, &shortDataArr[i][0]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2, shortDataArr[i][1]));

      sprintf(resolvedMethodName, "sSelectConst3_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 4, 1, &sselectChild1Arr[i], 3, &shortDataArr[i][1]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(SHORT_PLACEHOLDER_1, shortDataArr[i][0], SHORT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "sSelectConst4_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 4, 2, &shortDataArr[i][0], 3, &shortDataArr[i][1]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(sselectChild1Arr[i], SHORT_PLACEHOLDER_2, SHORT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "sSelectConst5_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 2, 1, &sselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(SHORT_PLACEHOLDER_1, shortDataArr[i][0], shortDataArr[i][1]));

      sprintf(resolvedMethodName, "sSelectConst6_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 2, 2, &shortDataArr[i][0]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(sselectChild1Arr[i], SHORT_PLACEHOLDER_1, shortDataArr[i][1]));

      sprintf(resolvedMethodName, "sSelectConst7_Testcase%d", i + 1);
      compileOpCodeMethod(sSelectConst, _numberOfSelectArgs, TR::sselect,
            resolvedMethodName, _argTypesSelectShort, TR::Int16, rc, 2, 3, &shortDataArr[i][1]);
      OMR_CT_EXPECT_EQ(sSelectConst, select(sselectChild1Arr[i], shortDataArr[i][0], shortDataArr[i][1]), sSelectConst(sselectChild1Arr[i], shortDataArr[i][0], SHORT_PLACEHOLDER_1));
      }
   }

void
PPCOpCodesTest::invokeBitwiseTests()
   {
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNum = 0;

   int8_t byteAndArr [][2] =
      {
      BYTE_MINIMUM, BYTE_MAXIMUM,
      BYTE_NEG, BYTE_NEG,
      BYTE_MAXIMUM, BYTE_ZERO,
      BYTE_MAXIMUM, BYTE_NEG,
      BYTE_MINIMUM, BYTE_ZERO,
      BYTE_NEG, BYTE_MAXIMUM,
      BYTE_POS, BYTE_MINIMUM
      };
   int8_t byteOrArr [][2] =
      {
      BYTE_MAXIMUM, BYTE_POS,
      BYTE_ZERO, BYTE_ZERO
      };
   int8_t byteXorArr [][2] =
      {
      BYTE_ZERO, BYTE_NEG
      };
   int16_t shortAndArr [][2] =
      {
      SHORT_MAXIMUM, SHORT_MINIMUM,
      SHORT_ZERO, SHORT_POS
      };
   int16_t shortOrArr [][2] =
      {
      SHORT_ZERO, SHORT_MINIMUM,
      SHORT_POS, SHORT_NEG,
      SHORT_ZERO, SHORT_MAXIMUM,
      SHORT_MINIMUM, SHORT_NEG,
      SHORT_NEG, SHORT_MINIMUM,
      SHORT_POS, SHORT_ZERO
      };
   int16_t shortXorArr [][2] =
      {
      SHORT_POS, SHORT_MAXIMUM,
      SHORT_MINIMUM, SHORT_POS,
      SHORT_NEG, SHORT_POS,
      SHORT_NEG, SHORT_NEG,
      SHORT_MINIMUM, SHORT_MINIMUM,
      SHORT_POS, SHORT_POS,
      SHORT_MAXIMUM, SHORT_MAXIMUM
      };

   signatureCharSS_S_testMethodType * sBitwiseConst = 0;
   signatureCharBB_B_testMethodType * bBitwiseConst = 0;

   //band
   testCaseNum = sizeof(byteAndArr) / sizeof(byteAndArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bAnd, tand(byteAndArr[i][0], byteAndArr[i][1]), _bAnd(byteAndArr[i][0], byteAndArr[i][1]));

      sprintf(resolvedMethodName, "bAndConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::band, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 4, 1, &(byteAndArr[i][0]), 2, &(byteAndArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tand(byteAndArr[i][0], byteAndArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bAndConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::band, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 1, &(byteAndArr[i][0]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tand(byteAndArr[i][0], byteAndArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, byteAndArr[i][1]));

      sprintf(resolvedMethodName, "bAndConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::band, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 2, &(byteAndArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tand(byteAndArr[i][0], byteAndArr[i][1]), bBitwiseConst(byteAndArr[i][0], BYTE_PLACEHOLDER_2));
     }

   //bor
   testCaseNum = sizeof(byteOrArr) / sizeof(byteOrArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bOr, tor(byteOrArr[i][0], byteOrArr[i][1]), _bOr(byteOrArr[i][0], byteOrArr[i][1]));

      sprintf(resolvedMethodName, "bOrConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 4, 1, &(byteOrArr[i][0]), 2, &(byteOrArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tor(byteOrArr[i][0], byteOrArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bOrConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 1, &(byteOrArr[i][0]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tor(byteOrArr[i][0], byteOrArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, byteOrArr[i][1]));

      sprintf(resolvedMethodName, "bOrConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 2, &(byteOrArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, tor(byteOrArr[i][0], byteOrArr[i][1]), bBitwiseConst(byteOrArr[i][0], BYTE_PLACEHOLDER_2));
     }

   //bxor
   testCaseNum = sizeof(byteXorArr) / sizeof(byteXorArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bXor, txor(byteXorArr[i][0], byteXorArr[i][1]), _bXor(byteXorArr[i][0], byteXorArr[i][1]));

      sprintf(resolvedMethodName, "bXorConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bxor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 4, 1, &(byteXorArr[i][0]), 2, &(byteXorArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, txor(byteXorArr[i][0], byteXorArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bXorConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bxor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 1, &(byteXorArr[i][0]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, txor(byteXorArr[i][0], byteXorArr[i][1]), bBitwiseConst(BYTE_PLACEHOLDER_1, byteXorArr[i][1]));

      sprintf(resolvedMethodName, "bXorConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bBitwiseConst,
            _numberOfBinaryArgs, TR::bxor, resolvedMethodName, _argTypesBinaryByte, TR::Int8, rc, 2, 2, &(byteXorArr[i][1]));
      OMR_CT_EXPECT_EQ(bBitwiseConst, txor(byteXorArr[i][0], byteXorArr[i][1]), bBitwiseConst(byteXorArr[i][0], BYTE_PLACEHOLDER_2));
      }

   //sand
   testCaseNum = sizeof(shortAndArr) / sizeof(shortAndArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_sAnd, tand(shortAndArr[i][0], shortAndArr[i][1]), _sAnd(shortAndArr[i][0], shortAndArr[i][1]));

      sprintf(resolvedMethodName, "sAndConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sand, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 4, 1, &(shortAndArr[i][0]), 2, &(shortAndArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tand(shortAndArr[i][0], shortAndArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sAndConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sand, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 1, &(shortAndArr[i][0]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tand(shortAndArr[i][0], shortAndArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, shortAndArr[i][1]));

      sprintf(resolvedMethodName, "sAndConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sand, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 2, &(shortAndArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tand(shortAndArr[i][0], shortAndArr[i][1]), sBitwiseConst(shortAndArr[i][0], SHORT_PLACEHOLDER_2));
      }

   //sor
   testCaseNum = sizeof(shortOrArr) / sizeof(shortOrArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_sOr, tor(shortOrArr[i][0], shortOrArr[i][1]), _sOr(shortOrArr[i][0], shortOrArr[i][1]));

      sprintf(resolvedMethodName, "sOrConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 4, 1, &(shortOrArr[i][0]), 2, &(shortOrArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tor(shortOrArr[i][0], shortOrArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sOrConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 1, &(shortOrArr[i][0]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tor(shortOrArr[i][0], shortOrArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, shortOrArr[i][1]));

      sprintf(resolvedMethodName, "sOrConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 2, &(shortOrArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, tor(shortOrArr[i][0], shortOrArr[i][1]), sBitwiseConst(shortOrArr[i][0], SHORT_PLACEHOLDER_2));
     }

   //sxor
   testCaseNum = sizeof(shortXorArr) / sizeof(shortXorArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_sXor, txor(shortXorArr[i][0], shortXorArr[i][1]), _sXor(shortXorArr[i][0], shortXorArr[i][1]));

      sprintf(resolvedMethodName, "sXorConst1_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sxor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 4, 1, &(shortXorArr[i][0]), 2, &(shortXorArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, txor(shortXorArr[i][0], shortXorArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, SHORT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "sXorConst2_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sxor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 1, &(shortXorArr[i][0]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, txor(shortXorArr[i][0], shortXorArr[i][1]), sBitwiseConst(SHORT_PLACEHOLDER_1, shortXorArr[i][1]));

      sprintf(resolvedMethodName, "sXorConst3_TestCase%d", i + 1);
      compileOpCodeMethod(sBitwiseConst,
            _numberOfBinaryArgs, TR::sxor, resolvedMethodName, _argTypesBinaryShort, TR::Int16, rc, 2, 2, &(shortXorArr[i][1]));
      OMR_CT_EXPECT_EQ(sBitwiseConst, txor(shortXorArr[i][0], shortXorArr[i][1]), sBitwiseConst(shortXorArr[i][0], SHORT_PLACEHOLDER_2));
     }
   }

void
PPCOpCodesTest::compileDisabledConvertTestMethods()
   {
   int32_t rc = 0;

   //Jazz103 109976
   //Defect Testarossa 119346
   compileOpCodeMethod(_iu2f, _numberOfUnaryArgs, TR::iu2f, "iU2f", _argTypesUnaryInt, TR::Float, rc);
   compileOpCodeMethod(_iu2d, _numberOfUnaryArgs, TR::iu2d, "iU2d", _argTypesUnaryInt, TR::Double, rc);
   compileOpCodeMethod(_iu2l, _numberOfUnaryArgs, TR::iu2l, "iu2l", _argTypesUnaryInt, TR::Int64, rc);
   compileOpCodeMethod(_f2d, _numberOfUnaryArgs, TR::f2d, "f2d", _argTypesUnaryFloat, TR::Double, rc);
   compileOpCodeMethod(_d2f, _numberOfUnaryArgs, TR::d2f, "d2f", _argTypesUnaryDouble, TR::Float, rc);
   compileOpCodeMethod(_bu2f, _numberOfUnaryArgs, TR::bu2f, "bu2f", _argTypesUnaryByte, TR::Float, rc);
   compileOpCodeMethod(_bu2d, _numberOfUnaryArgs, TR::bu2d, "bu2d", _argTypesUnaryByte, TR::Double, rc);

   //Jazz103 109605
   compileOpCodeMethod(_l2a, _numberOfUnaryArgs, TR::l2a, "l2a", _argTypesUnaryLong, TR::Address, rc);
   compileOpCodeMethod(_su2a, _numberOfUnaryArgs, TR::su2a, "su2a", _argTypesUnaryShort, TR::Address, rc);
   compileOpCodeMethod(_iu2a, _numberOfUnaryArgs, TR::iu2a, "iu2a", _argTypesUnaryInt, TR::Address, rc);

   }

void
PPCOpCodesTest::invokeDisabledConvertTests()
   {
   int32_t rc = 0;

   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_ZERO, FLOAT_MAXIMUM, FLOAT_MINIMUM};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_ZERO, DOUBLE_MAXIMUM, DOUBLE_MINIMUM};
   uint32_t uintDataArray[] = {UINT_MAXIMUM, UINT_MINIMUM, UINT_POS};
   uint8_t ubyteDataArray[] = {UBYTE_MAXIMUM, UBYTE_MINIMUM, UBYTE_POS};
   uint16_t ushortDataArray[] = {USHORT_POS, USHORT_MAXIMUM, USHORT_MINIMUM};
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};

   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   unsignedSignatureCharI_J_testMethodType * iu2lConst = 0;
   unsignedSignatureCharI_F_testMethodType * iu2fConst = 0;
   unsignedSignatureCharI_D_testMethodType * iu2dConst = 0;
   unsignedSignatureCharB_F_testMethodType * bu2fConst = 0;
   unsignedSignatureCharB_D_testMethodType * bu2dConst = 0;
   unsignedSignatureCharS_L_testMethodType *su2aConst = 0;
   unsignedSignatureCharI_L_testMethodType *iu2aConst = 0;
   signatureCharD_F_testMethodType * d2fConst = 0;
   signatureCharF_D_testMethodType * f2dConst = 0;
   signatureCharJ_L_testMethodType *l2aConst = 0;

   //Defect Testarossa 119346
   testCaseNum = sizeof(uintDataArray) / sizeof(uintDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iu2f, convert(uintDataArray[i], FLOAT_POS), _iu2f(uintDataArray[i]));
      OMR_CT_EXPECT_EQ(_iu2d, convert(uintDataArray[i], DOUBLE_POS), _iu2d(uintDataArray[i]));

      sprintf(resolvedMethodName, "iu2fConst%d", i + 1);
      compileOpCodeMethod(iu2fConst,
            _numberOfUnaryArgs, TR::iu2f, resolvedMethodName, _argTypesUnaryInt, TR::Float, rc, 2, 1, &uintDataArray[i]);
      OMR_CT_EXPECT_EQ(iu2fConst, convert(uintDataArray[i], FLOAT_POS), iu2fConst(INT_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "iu2dConst%d", i + 1);
      compileOpCodeMethod(iu2dConst,
            _numberOfUnaryArgs, TR::iu2d, resolvedMethodName, _argTypesUnaryInt, TR::Double, rc, 2, 1, &uintDataArray[i]);
      OMR_CT_EXPECT_EQ(iu2dConst, convert(uintDataArray[i], DOUBLE_POS), iu2dConst(INT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(ubyteDataArray) / sizeof(ubyteDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bu2f, convert(ubyteDataArray[i], FLOAT_POS), _bu2f(ubyteDataArray[i]));
      OMR_CT_EXPECT_EQ(_bu2d, convert(ubyteDataArray[i], DOUBLE_POS), _bu2d(ubyteDataArray[i]));

      sprintf(resolvedMethodName, "bu2fConst%d", i + 1);
      compileOpCodeMethod(bu2fConst,
            _numberOfUnaryArgs, TR::bu2f, resolvedMethodName, _argTypesUnaryByte, TR::Float, rc, 2, 1, &ubyteDataArray[i]);
      OMR_CT_EXPECT_EQ(bu2fConst, convert(ubyteDataArray[i], FLOAT_POS), bu2fConst(BYTE_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "bu2dConst%d", i + 1);
      compileOpCodeMethod(bu2dConst,
            _numberOfUnaryArgs, TR::bu2d, resolvedMethodName, _argTypesUnaryByte, TR::Double, rc, 2, 1, &ubyteDataArray[i]);
      OMR_CT_EXPECT_EQ(bu2dConst, convert(ubyteDataArray[i], DOUBLE_POS), bu2dConst(BYTE_PLACEHOLDER_1));
      }

   //iu2l
   testCaseNum = sizeof(uintDataArray) / sizeof(uintDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iu2l, convert(uintDataArray[i], LONG_POS), _iu2l(uintDataArray[i]));

      sprintf(resolvedMethodName, "iu2lConst%d", i + 1);
      compileOpCodeMethod(iu2lConst, _numberOfUnaryArgs, TR::iu2l,
            resolvedMethodName, _argTypesUnaryInt, TR::Int64, rc, 2, 1, &uintDataArray[i]);
      OMR_CT_EXPECT_EQ(iu2lConst, convert(uintDataArray[i], LONG_POS), iu2lConst(INT_PLACEHOLDER_1));
      }

   //f2d
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_f2d, convert(floatDataArray[i], DOUBLE_POS), _f2d(floatDataArray[i]));

      sprintf(resolvedMethodName, "f2dConst%d", i + 1);
      compileOpCodeMethod(f2dConst, _numberOfUnaryArgs, TR::f2d,
            resolvedMethodName, _argTypesUnaryFloat, TR::Double, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_EQ(f2dConst, convert(floatDataArray[i], DOUBLE_POS), f2dConst(FLOAT_PLACEHOLDER_1));
      }

   //d2f
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_d2f, convert(doubleDataArray[i], FLOAT_POS), _d2f(doubleDataArray[i]));

      sprintf(resolvedMethodName, "d2fConst%d", i + 1);
      compileOpCodeMethod(d2fConst, _numberOfUnaryArgs, TR::d2f,
            resolvedMethodName, _argTypesUnaryDouble, TR::Float, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_FLOAT_EQ(d2fConst, convert(doubleDataArray[i], FLOAT_POS), d2fConst(DOUBLE_PLACEHOLDER_1));
      }

   //Jazz103 109605:
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_l2a, convert(longDataArray[i], ADDRESS_PLACEHOLDER_1), _l2a(longDataArray[i]));

      sprintf(resolvedMethodName, "l2aConst%d", i + 1);
      compileOpCodeMethod(l2aConst,
            _numberOfUnaryArgs, TR::l2a, resolvedMethodName, _argTypesUnaryLong, TR::Address, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(l2aConst, convert(longDataArray[i], ADDRESS_PLACEHOLDER_1), l2aConst(LONG_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(ushortDataArray) / sizeof(ushortDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_su2a, convert(ushortDataArray[i], ADDRESS_PLACEHOLDER_1), _su2a(ushortDataArray[i]));

      sprintf(resolvedMethodName, "su2aConst%d", i + 1);
      compileOpCodeMethod(su2aConst,
            _numberOfUnaryArgs, TR::su2a, resolvedMethodName, _argTypesUnaryShort, TR::Address, rc, 2, 1, &ushortDataArray[i]);
      OMR_CT_EXPECT_EQ(su2aConst, convert(ushortDataArray[i], ADDRESS_PLACEHOLDER_1), su2aConst(SHORT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(uintDataArray) / sizeof(uintDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_iu2a, convert(uintDataArray[i], ADDRESS_PLACEHOLDER_1), _iu2a(uintDataArray[i]));

      sprintf(resolvedMethodName, "iu2aConst%d", i + 1);
      compileOpCodeMethod(iu2aConst,
            _numberOfUnaryArgs, TR::iu2a, resolvedMethodName, _argTypesUnaryInt, TR::Address, rc, 2, 1, &uintDataArray[i]);
      OMR_CT_EXPECT_EQ(iu2aConst, convert(uintDataArray[i], ADDRESS_PLACEHOLDER_1), iu2aConst(INT_PLACEHOLDER_1));
      }
   }

void
PPCOpCodesTest::compileDisabledCompareTestMethods()
   {
   int32_t rc = 0;

   //Jazz103 Work Item 106531
   //Compare
   compileOpCodeMethod(_lCmpeq, _numberOfBinaryArgs, TR::lcmpeq, "lCmpeq", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_lCmplt, _numberOfBinaryArgs, TR::lcmplt, "lCmplt", _argTypesBinaryLong, TR::Int32, rc);

   compileOpCodeMethod(_dCmpeq, _numberOfBinaryArgs, TR::dcmpeq, "dCmpeq", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmpne, _numberOfBinaryArgs, TR::dcmpne, "dCmpne", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmpgt, _numberOfBinaryArgs, TR::dcmpgt, "dCmpgt", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmplt, _numberOfBinaryArgs, TR::dcmplt, "dCmplt", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmpge, _numberOfBinaryArgs, TR::dcmpge, "dCmpge", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmple, _numberOfBinaryArgs, TR::dcmple, "dCmple", _argTypesBinaryDouble, TR::Int32, rc);

   compileOpCodeMethod(_fCmpeq, _numberOfBinaryArgs, TR::fcmpeq, "fCmpeq", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmpne, _numberOfBinaryArgs, TR::fcmpne, "fCmpne", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmpgt, _numberOfBinaryArgs, TR::fcmpgt, "fCmpgt", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmplt, _numberOfBinaryArgs, TR::fcmplt, "fCmplt", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmpge, _numberOfBinaryArgs, TR::fcmpge, "fCmpge", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmple, _numberOfBinaryArgs, TR::fcmple, "fCmple", _argTypesBinaryFloat, TR::Int32, rc);

   compileOpCodeMethod(_bCmpne, _numberOfBinaryArgs, TR::bcmpne, "bCmpne", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_bCmple, _numberOfBinaryArgs, TR::bcmple, "bCmple", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_bCmplt, _numberOfBinaryArgs, TR::bcmplt, "bCmplt", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_bCmpge, _numberOfBinaryArgs, TR::bcmpge, "bCmpge", _argTypesBinaryByte, TR::Int32, rc);

   compileOpCodeMethod(_lCmp, _numberOfBinaryArgs, TR::lcmp, "lCmp", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_fCmpl, _numberOfBinaryArgs, TR::fcmpl, "fCmpl", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_fCmpg, _numberOfBinaryArgs, TR::fcmpg, "fCmpg", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_dCmpl, _numberOfBinaryArgs, TR::dcmpl, "dCmpl", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_dCmpg, _numberOfBinaryArgs, TR::dcmpg, "dCmpg", _argTypesBinaryDouble, TR::Int32, rc);

   compileOpCodeMethod(_ifLcmpeq, _numberOfBinaryArgs, TR::iflcmpeq, "ifLcmpeq", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLcmpgt, _numberOfBinaryArgs, TR::iflcmpgt, "ifLcmpgt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLcmplt, _numberOfBinaryArgs, TR::iflcmplt, "ifLcmplt", _argTypesBinaryLong, TR::Int32, rc);

   compileOpCodeMethod(_ifDcmpeq, _numberOfBinaryArgs, TR::ifdcmpeq, "ifDcmpeq", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_ifDcmpne, _numberOfBinaryArgs, TR::ifdcmpne, "ifDcmpne", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_ifDcmpgt, _numberOfBinaryArgs, TR::ifdcmpgt, "ifDcmpgt", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_ifDcmplt, _numberOfBinaryArgs, TR::ifdcmplt, "ifDcmplt", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_ifDcmpge, _numberOfBinaryArgs, TR::ifdcmpge, "ifDcmpge", _argTypesBinaryDouble, TR::Int32, rc);
   compileOpCodeMethod(_ifDcmple, _numberOfBinaryArgs, TR::ifdcmple, "ifDcmple", _argTypesBinaryDouble, TR::Int32, rc);

   compileOpCodeMethod(_ifFcmpeq, _numberOfBinaryArgs, TR::iffcmpeq, "ifFcmpeq", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_ifFcmpne, _numberOfBinaryArgs, TR::iffcmpne, "ifFcmpne", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_ifFcmpgt, _numberOfBinaryArgs, TR::iffcmpgt, "ifFcmpgt", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_ifFcmplt, _numberOfBinaryArgs, TR::iffcmplt, "ifFcmplt", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_ifFcmpge, _numberOfBinaryArgs, TR::iffcmpge, "ifFcmpge", _argTypesBinaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_ifFcmple, _numberOfBinaryArgs, TR::iffcmple, "ifFcmple", _argTypesBinaryFloat, TR::Int32, rc);

   compileOpCodeMethod(_ifBcmpne, _numberOfBinaryArgs, TR::ifbcmpne, "ifBcmpne", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBcmple, _numberOfBinaryArgs, TR::ifbcmple, "ifBcmple", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBcmplt, _numberOfBinaryArgs, TR::ifbcmplt, "ifBcmplt", _argTypesBinaryByte, TR::Int32, rc);
   compileOpCodeMethod(_ifBcmpge, _numberOfBinaryArgs, TR::ifbcmpge, "ifBcmpge", _argTypesBinaryByte, TR::Int32, rc);

   }

void
PPCOpCodesTest::invokeDisabledCompareTests()
   {

   const float FLOAT_NaN = std::numeric_limits<float>::quiet_NaN();
   const double DOUBLE_NaN = std::numeric_limits<float>::quiet_NaN();

   //Compare op codes data array
   int64_t lCmpeqDataArr[][2] =
         {
         LONG_MAXIMUM, LONG_MINIMUM,
         LONG_MAXIMUM, LONG_MAXIMUM
         };
   int64_t lCmpltDataArr[][2] =
         {
         LONG_ZERO, LONG_MAXIMUM,
         LONG_MAXIMUM, LONG_ZERO
         };
   double dCmpeqDataArr[][2] =
         {
         DOUBLE_NEG, DOUBLE_POS,
         DOUBLE_POS, DOUBLE_POS
         };
   double dCmpneDataArr[][2] =
         {
         DOUBLE_POS, DOUBLE_MINIMUM,
         DOUBLE_POS, DOUBLE_POS
         };
   double dCmpgtDataArr[][2] =
         {
         DOUBLE_ZERO, DOUBLE_POS,
         DOUBLE_POS, DOUBLE_ZERO
         };
   double dCmpltDataArr[][2] =
         {
         DOUBLE_MAXIMUM, DOUBLE_ZERO,
         DOUBLE_ZERO, DOUBLE_MAXIMUM
         };
   double dCmpgeDataArr[][2] =
         {
         DOUBLE_POS, DOUBLE_MAXIMUM,
         DOUBLE_MAXIMUM, DOUBLE_POS
         };
   double dCmpleDataArr[][2] =
         {
         DOUBLE_MINIMUM, DOUBLE_NEG,
         DOUBLE_NEG, DOUBLE_MINIMUM
         };
   float fCmpeqDataArr[][2] =
         {
         FLOAT_ZERO, FLOAT_NEG,
         FLOAT_NEG, FLOAT_NEG
         };
   float fCmpneDataArr[][2] =
         {
         FLOAT_NEG, FLOAT_MAXIMUM,
         FLOAT_POS, FLOAT_POS
         };
   float fCmpgtDataArr[][2] =
         {
         FLOAT_MAXIMUM, FLOAT_ZERO,
         FLOAT_ZERO, FLOAT_MAXIMUM
         };
   float fCmpltDataArr[][2] =
         {
         FLOAT_POS, FLOAT_POS,
         FLOAT_ZERO, FLOAT_MAXIMUM
         };
   float fCmpgeDataArr[][2] =
         {
         FLOAT_MINIMUM, FLOAT_MINIMUM,
         FLOAT_POS, FLOAT_MAXIMUM
         };
   float fCmpleDataArr[][2] =
         {
         FLOAT_MINIMUM, FLOAT_NEG,
         FLOAT_NEG, FLOAT_MINIMUM
         };
   int8_t bCmpneDataArr[][2] =
         {
         BYTE_MINIMUM, BYTE_POS,
         BYTE_MINIMUM, BYTE_MINIMUM
         };
   int8_t bCmpleDataArr[][2] =
         {
         BYTE_MINIMUM, BYTE_NEG,
         BYTE_NEG, BYTE_MINIMUM,
         BYTE_NEG, BYTE_NEG
         };
   int8_t bCmpltDataArr[][2] =
         {
         BYTE_MAXIMUM, BYTE_ZERO,
         BYTE_ZERO, BYTE_MAXIMUM
         };
   int8_t bCmpgeDataArr[][2] =
         {
         BYTE_POS, BYTE_MAXIMUM,
         BYTE_MAXIMUM, BYTE_POS,
         BYTE_MAXIMUM, BYTE_MAXIMUM
         };

   //Compare data array
   int64_t lCmpDataArr [][2] =
         {
         LONG_ZERO, LONG_POS,
         LONG_NEG,  LONG_NEG,
         LONG_POS,  LONG_MINIMUM,
         LONG_MINIMUM, LONG_ZERO,
         LONG_MAXIMUM, LONG_MAXIMUM,
         };

   float fCmplDataArr [][2] =
         {
         FLOAT_MAXIMUM, FLOAT_ZERO,
         FLOAT_NEG,  FLOAT_MINIMUM,
         FLOAT_NEG, FLOAT_ZERO,
         FLOAT_MINIMUM, FLOAT_NEG,
         FLOAT_POS, FLOAT_POS,
         FLOAT_MAXIMUM, FLOAT_NaN,
         FLOAT_NaN, FLOAT_NEG,
         FLOAT_NaN, FLOAT_NaN
         };

   float fCmpgDataArr [][2] =
         {
         FLOAT_POS,  FLOAT_MAXIMUM,
         FLOAT_MINIMUM, FLOAT_POS,
         FLOAT_NEG,  FLOAT_MAXIMUM,
         FLOAT_MAXIMUM, FLOAT_MINIMUM,
         FLOAT_MAXIMUM, FLOAT_POS,
         FLOAT_POS,  FLOAT_NEG,
         FLOAT_POS,  FLOAT_POS,
         FLOAT_NaN, FLOAT_POS ,
         FLOAT_MINIMUM, FLOAT_NaN,
         FLOAT_NaN, FLOAT_NaN
         };

   double dCmplDataArr [][2] =
         {
         DOUBLE_MINIMUM, DOUBLE_MAXIMUM,
         DOUBLE_POS,  DOUBLE_ZERO,
         DOUBLE_ZERO, DOUBLE_MAXIMUM,
         DOUBLE_POS,DOUBLE_POS,
         DOUBLE_NEG,DOUBLE_MINIMUM,
         DOUBLE_MINIMUM, DOUBLE_NaN,
         DOUBLE_NaN, DOUBLE_POS,
         DOUBLE_NaN, DOUBLE_NaN
         };

   double dCmpgDataArr [][2] =
         {
         DOUBLE_ZERO, DOUBLE_MINIMUM,
         DOUBLE_NEG,  DOUBLE_ZERO,
         DOUBLE_MAXIMUM, DOUBLE_NEG,
         DOUBLE_POS,  DOUBLE_POS,
         DOUBLE_NEG,  DOUBLE_POS,
         DOUBLE_MINIMUM, DOUBLE_MINIMUM,
         DOUBLE_ZERO, DOUBLE_ZERO,
         DOUBLE_NaN, DOUBLE_ZERO ,
         DOUBLE_NEG, DOUBLE_NaN,
         DOUBLE_NaN, DOUBLE_NaN
         };

   //CompareAndBranch data array
   int64_t ifLcmpeqDataArr[][2] =
         {
         LONG_MAXIMUM, LONG_MINIMUM,
         LONG_MAXIMUM, LONG_MAXIMUM
         };
   int64_t ifLcmpgtDataArr[][2] =
         {
         LONG_MINIMUM, LONG_ZERO,
         LONG_ZERO, LONG_MINIMUM
         };
   int64_t ifLcmpltDataArr[][2] =
         {
         LONG_ZERO, LONG_MAXIMUM,
         LONG_MAXIMUM, LONG_ZERO
         };
   double ifDcmpeqDataArr[][2] =
         {
         DOUBLE_NEG, DOUBLE_POS,
         DOUBLE_POS, DOUBLE_POS
         };
   double ifDcmpneDataArr[][2] =
         {
         DOUBLE_POS, DOUBLE_MINIMUM,
         DOUBLE_POS, DOUBLE_POS
         };
   double ifDcmpgtDataArr[][2] =
         {
         DOUBLE_ZERO, DOUBLE_POS,
         DOUBLE_POS, DOUBLE_ZERO
         };
   double ifDcmpltDataArr[][2] =
         {
         DOUBLE_MAXIMUM, DOUBLE_ZERO,
         DOUBLE_ZERO, DOUBLE_MAXIMUM
         };
   double ifDcmpgeDataArr[][2] =
         {
         DOUBLE_POS, DOUBLE_MAXIMUM,
         DOUBLE_MAXIMUM, DOUBLE_POS
         };
   double ifDcmpleDataArr[][2] =
         {
         DOUBLE_MINIMUM, DOUBLE_NEG,
         DOUBLE_NEG, DOUBLE_MINIMUM
         };
   float ifFcmpeqDataArr[][2] =
         {
         FLOAT_ZERO, FLOAT_NEG,
         FLOAT_NEG, FLOAT_NEG
         };
   float ifFcmpneDataArr[][2] =
         {
         FLOAT_NEG, FLOAT_MAXIMUM,
         FLOAT_POS, FLOAT_POS
         };
   float ifFcmpgtDataArr[][2] =
         {
         FLOAT_MAXIMUM, FLOAT_ZERO,
         FLOAT_ZERO, FLOAT_MAXIMUM
         };
   float ifFcmpltDataArr[][2] =
         {
         FLOAT_POS, FLOAT_POS,
         FLOAT_ZERO, FLOAT_MAXIMUM
         };
   float ifFcmpgeDataArr[][2] =
         {
         FLOAT_MINIMUM, FLOAT_MINIMUM,
         FLOAT_POS, FLOAT_MAXIMUM
         };
   float ifFcmpleDataArr[][2] =
         {
         FLOAT_MINIMUM, FLOAT_NEG,
         FLOAT_NEG, FLOAT_MINIMUM
         };
   int8_t ifBcmpneDataArr[][2] =
         {
         BYTE_NEG, BYTE_POS,
         BYTE_POS, BYTE_POS
         };
   int8_t ifBcmpleDataArr[][2] =
         {
         BYTE_MINIMUM, BYTE_NEG,
         BYTE_NEG, BYTE_MINIMUM
         };
   int8_t ifBcmpltDataArr[][2] =
         {
         BYTE_MAXIMUM, BYTE_ZERO,
         BYTE_ZERO, BYTE_MAXIMUM
         };
   int8_t ifBcmpgeDataArr[][2] =
         {
         BYTE_POS, BYTE_MAXIMUM,
         BYTE_MAXIMUM, BYTE_POS
         };

   int32_t rc = 0;
   int32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharJJ_I_testMethodType * lCompareConst = 0;
   signatureCharDD_I_testMethodType * dCompareConst = 0;
   signatureCharFF_I_testMethodType * fCompareConst = 0;
   signatureCharBB_I_testMethodType * bCompareConst = 0;
   unsignedCompareSignatureCharBB_I_testMethodType * buCompareConst = 0;

   //lcmpeq
   testCaseNum = sizeof(lCmpeqDataArr) / sizeof(lCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmpeq, compareEQ(lCmpeqDataArr[i][0], lCmpeqDataArr[i][1]), _lCmpeq(lCmpeqDataArr[i][0], lCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpeqDataArr[i][0]), 2, &(lCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(lCmpeqDataArr[i][0], lCmpeqDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(lCmpeqDataArr[i][0], lCmpeqDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(lCmpeqDataArr[i][0], lCmpeqDataArr[i][1]), lCompareConst(lCmpeqDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //lcmplt
   testCaseNum = sizeof(lCmpltDataArr) / sizeof(lCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmplt, compareLT(lCmpltDataArr[i][0], lCmpltDataArr[i][1]), _lCmplt(lCmpltDataArr[i][0], lCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpltDataArr[i][0]), 2, &(lCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(lCmpltDataArr[i][0], lCmpltDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(lCmpltDataArr[i][0], lCmpltDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(lCmpltDataArr[i][0], lCmpltDataArr[i][1]), lCompareConst(lCmpltDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //dCompare
   testCaseNum = sizeof(dCmpeqDataArr) / sizeof(dCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpeq, compareEQ(dCmpeqDataArr[i][0], dCmpeqDataArr[i][1]), _dCmpeq(dCmpeqDataArr[i][0], dCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpeqDataArr[i][0]), 2, &(dCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(dCmpeqDataArr[i][0], dCmpeqDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(dCmpeqDataArr[i][0], dCmpeqDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(dCmpeqDataArr[i][0], dCmpeqDataArr[i][1]), dCompareConst(dCmpeqDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(dCmpneDataArr) / sizeof(dCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpne, compareNE(dCmpneDataArr[i][0], dCmpneDataArr[i][1]), _dCmpne(dCmpneDataArr[i][0], dCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpneDataArr[i][0]), 2, &(dCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(dCmpneDataArr[i][0], dCmpneDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(dCmpneDataArr[i][0], dCmpneDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(dCmpneDataArr[i][0], dCmpneDataArr[i][1]), dCompareConst(dCmpneDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(dCmpgtDataArr) / sizeof(dCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpgt, compareGT(dCmpgtDataArr[i][0], dCmpgtDataArr[i][1]), _dCmpgt(dCmpgtDataArr[i][0], dCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpgtDataArr[i][0]), 2, &(dCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(dCmpgtDataArr[i][0], dCmpgtDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(dCmpgtDataArr[i][0], dCmpgtDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(dCmpgtDataArr[i][0], dCmpgtDataArr[i][1]), dCompareConst(dCmpgtDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(dCmpltDataArr) / sizeof(dCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmplt, compareLT(dCmpltDataArr[i][0], dCmpltDataArr[i][1]), _dCmplt(dCmpltDataArr[i][0], dCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpltDataArr[i][0]), 2, &(dCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(dCmpltDataArr[i][0], dCmpltDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(dCmpltDataArr[i][0], dCmpltDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(dCmpltDataArr[i][0], dCmpltDataArr[i][1]), dCompareConst(dCmpltDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(dCmpgeDataArr) / sizeof(dCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpge, compareGE(dCmpgeDataArr[i][0], dCmpgeDataArr[i][1]), _dCmpge(dCmpgeDataArr[i][0], dCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpgeDataArr[i][0]), 2, &(dCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(dCmpgeDataArr[i][0], dCmpgeDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(dCmpgeDataArr[i][0], dCmpgeDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(dCmpgeDataArr[i][0], dCmpgeDataArr[i][1]), dCompareConst(dCmpgeDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(dCmpleDataArr) / sizeof(dCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmple, compareLE(dCmpleDataArr[i][0], dCmpleDataArr[i][1]), _dCmple(dCmpleDataArr[i][0], dCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpleDataArr[i][0]), 2, &(dCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(dCmpleDataArr[i][0], dCmpleDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(dCmpleDataArr[i][0], dCmpleDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "dCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(dCmpleDataArr[i][0], dCmpleDataArr[i][1]), dCompareConst(dCmpleDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   //fCompare
   testCaseNum = sizeof(fCmpeqDataArr) / sizeof(fCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpeq, compareEQ(fCmpeqDataArr[i][0], fCmpeqDataArr[i][1]), _fCmpeq(fCmpeqDataArr[i][0], fCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpeqDataArr[i][0]), 2, &(fCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(fCmpeqDataArr[i][0], fCmpeqDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(fCmpeqDataArr[i][0], fCmpeqDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(fCmpeqDataArr[i][0], fCmpeqDataArr[i][1]), fCompareConst(fCmpeqDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(fCmpneDataArr) / sizeof(fCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpne, compareNE(fCmpneDataArr[i][0], fCmpneDataArr[i][1]), _fCmpne(fCmpneDataArr[i][0], fCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpneDataArr[i][0]), 2, &(fCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(fCmpneDataArr[i][0], fCmpneDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(fCmpneDataArr[i][0], fCmpneDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(fCmpneDataArr[i][0], fCmpneDataArr[i][1]), fCompareConst(fCmpneDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(fCmpgtDataArr) / sizeof(fCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpgt, compareGT(fCmpgtDataArr[i][0], fCmpgtDataArr[i][1]), _fCmpgt(fCmpgtDataArr[i][0], fCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpgtDataArr[i][0]), 2, &(fCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(fCmpgtDataArr[i][0], fCmpgtDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(fCmpgtDataArr[i][0], fCmpgtDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(fCmpgtDataArr[i][0], fCmpgtDataArr[i][1]), fCompareConst(fCmpgtDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(fCmpltDataArr) / sizeof(fCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmplt, compareLT(fCmpltDataArr[i][0], fCmpltDataArr[i][1]), _fCmplt(fCmpltDataArr[i][0], fCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpltDataArr[i][0]), 2, &(fCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(fCmpltDataArr[i][0], fCmpltDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(fCmpltDataArr[i][0], fCmpltDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(fCmpltDataArr[i][0], fCmpltDataArr[i][1]), fCompareConst(fCmpltDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(fCmpgeDataArr) / sizeof(fCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpge, compareGE(fCmpgeDataArr[i][0], fCmpgeDataArr[i][1]), _fCmpge(fCmpgeDataArr[i][0], fCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpgeDataArr[i][0]), 2, &(fCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(fCmpgeDataArr[i][0], fCmpgeDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(fCmpgeDataArr[i][0], fCmpgeDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(fCmpgeDataArr[i][0], fCmpgeDataArr[i][1]), fCompareConst(fCmpgeDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(fCmpleDataArr) / sizeof(fCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmple, compareLE(fCmpleDataArr[i][0], fCmpleDataArr[i][1]), _fCmple(fCmpleDataArr[i][0], fCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpleDataArr[i][0]), 2, &(fCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(fCmpleDataArr[i][0], fCmpleDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(fCmpleDataArr[i][0], fCmpleDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "fCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(fCmpleDataArr[i][0], fCmpleDataArr[i][1]), fCompareConst(fCmpleDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   //bCompare
   testCaseNum = sizeof(bCmpneDataArr) / sizeof(bCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmpne, compareNE(bCmpneDataArr[i][0], bCmpneDataArr[i][1]), _bCmpne(bCmpneDataArr[i][0], bCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpneDataArr[i][0]), 2, &(bCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(bCmpneDataArr[i][0], bCmpneDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(bCmpneDataArr[i][0], bCmpneDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(bCmpneDataArr[i][0], bCmpneDataArr[i][1]), bCompareConst(bCmpneDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(bCmpleDataArr) / sizeof(bCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmple, compareLE(bCmpleDataArr[i][0], bCmpleDataArr[i][1]), _bCmple(bCmpleDataArr[i][0], bCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpleDataArr[i][0]), 2, &(bCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(bCmpleDataArr[i][0], bCmpleDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(bCmpleDataArr[i][0], bCmpleDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(bCmpleDataArr[i][0], bCmpleDataArr[i][1]), bCompareConst(bCmpleDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(bCmpltDataArr) / sizeof(bCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmplt, compareLT(bCmpltDataArr[i][0], bCmpltDataArr[i][1]), _bCmplt(bCmpltDataArr[i][0], bCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpltDataArr[i][0]), 2, &(bCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(bCmpltDataArr[i][0], bCmpltDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(bCmpltDataArr[i][0], bCmpltDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(bCmpltDataArr[i][0], bCmpltDataArr[i][1]), bCompareConst(bCmpltDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(bCmpgeDataArr) / sizeof(bCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bCmpge, compareGE(bCmpgeDataArr[i][0], bCmpgeDataArr[i][1]), _bCmpge(bCmpgeDataArr[i][0], bCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(bCmpgeDataArr[i][0]), 2, &(bCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(bCmpgeDataArr[i][0], bCmpgeDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "bCmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(bCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(bCmpgeDataArr[i][0], bCmpgeDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, bCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "bCmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::bcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(bCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(bCmpgeDataArr[i][0], bCmpgeDataArr[i][1]), bCompareConst(bCmpgeDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   //lcmp
   testCaseNum = sizeof(lCmpDataArr) / sizeof(lCmpDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmp, comparel(lCmpDataArr[i][0], lCmpDataArr[i][1]), _lCmp(lCmpDataArr[i][0], lCmpDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmp, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpDataArr[i][0]), 2, &(lCmpDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, comparel(lCmpDataArr[i][0], lCmpDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lCmpConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmp, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, comparel(lCmpDataArr[i][0], lCmpDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::lcmp, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, comparel(lCmpDataArr[i][0], lCmpDataArr[i][1]), lCompareConst(lCmpDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //dcmpl
   testCaseNum = sizeof(dCmplDataArr) / sizeof(dCmplDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpl, comparel(dCmplDataArr[i][0], dCmplDataArr[i][1]), _dCmpl(dCmplDataArr[i][0], dCmplDataArr[i][1])) <<
            dCmplDataArr[i][0] << " : " << dCmplDataArr[i][1];

      sprintf(resolvedMethodName, "dCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpl, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmplDataArr[i][0]), 2, &(dCmplDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, comparel(dCmplDataArr[i][0], dCmplDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2)) <<
            dCmplDataArr[i][0] << " : " << dCmplDataArr[i][1];

      sprintf(resolvedMethodName, "dCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpl, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmplDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, comparel(dCmplDataArr[i][0], dCmplDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmplDataArr[i][1])) <<
            dCmplDataArr[i][0] << " : " << dCmplDataArr[i][1];

      sprintf(resolvedMethodName, "dCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpl, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmplDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, comparel(dCmplDataArr[i][0], dCmplDataArr[i][1]), dCompareConst(dCmplDataArr[i][0], DOUBLE_PLACEHOLDER_2)) <<
            dCmplDataArr[i][0] << " : " << dCmplDataArr[i][1];
      }

   //dcmpg
   testCaseNum = sizeof(dCmpgDataArr) / sizeof(dCmpgDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dCmpg, compareg(dCmpgDataArr[i][0], dCmpgDataArr[i][1]), _dCmpg(dCmpgDataArr[i][0], dCmpgDataArr[i][1])) <<
            dCmpgDataArr[i][0] << " : " << dCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "dCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpg, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(dCmpgDataArr[i][0]), 2, &(dCmpgDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareg(dCmpgDataArr[i][0], dCmpgDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2)) <<
            dCmpgDataArr[i][0] << " : " << dCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "dCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpg, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(dCmpgDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareg(dCmpgDataArr[i][0], dCmpgDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, dCmpgDataArr[i][1])) <<
            dCmpgDataArr[i][0] << " : " << dCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "dCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::dcmpg, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(dCmpgDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareg(dCmpgDataArr[i][0], dCmpgDataArr[i][1]), dCompareConst(dCmpgDataArr[i][0], DOUBLE_PLACEHOLDER_2)) <<
            dCmpgDataArr[i][0] << " : " << dCmpgDataArr[i][1];
      }

   //fcmpl
   testCaseNum = sizeof(fCmplDataArr) / sizeof(fCmplDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpl, comparel(fCmplDataArr[i][0], fCmplDataArr[i][1]), _fCmpl(fCmplDataArr[i][0], fCmplDataArr[i][1])) <<
            fCmplDataArr[i][0] << " : " << fCmplDataArr[i][1];

      sprintf(resolvedMethodName, "fCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpl, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmplDataArr[i][0]), 2, &(fCmplDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, comparel(fCmplDataArr[i][0], fCmplDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2)) <<
            fCmplDataArr[i][0] << " : " << fCmplDataArr[i][1];

      sprintf(resolvedMethodName, "fCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpl, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmplDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, comparel(fCmplDataArr[i][0], fCmplDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmplDataArr[i][1])) <<
            fCmplDataArr[i][0] << " : " << fCmplDataArr[i][1];

      sprintf(resolvedMethodName, "fCmplConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpl, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmplDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, comparel(fCmplDataArr[i][0], fCmplDataArr[i][1]), fCompareConst(fCmplDataArr[i][0], FLOAT_PLACEHOLDER_2)) <<
            fCmplDataArr[i][0] << " : " << fCmplDataArr[i][1];
      }

   //fcmpg
   testCaseNum = sizeof(fCmpgDataArr) / sizeof(fCmpgDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fCmpg, compareg(fCmpgDataArr[i][0], fCmpgDataArr[i][1]), _fCmpg(fCmpgDataArr[i][0], fCmpgDataArr[i][1])) <<
            fCmpgDataArr[i][0] << " : " << fCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "fCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpg, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(fCmpgDataArr[i][0]), 2, &(fCmpgDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareg(fCmpgDataArr[i][0], fCmpgDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2)) <<
            fCmpgDataArr[i][0] << " : " << fCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "fCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpg, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(fCmpgDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareg(fCmpgDataArr[i][0], fCmpgDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, fCmpgDataArr[i][1])) <<
            fCmpgDataArr[i][0] << " : " << fCmpgDataArr[i][1];

      sprintf(resolvedMethodName, "fCmpgConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::fcmpg, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(fCmpgDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareg(fCmpgDataArr[i][0], fCmpgDataArr[i][1]), fCompareConst(fCmpgDataArr[i][0], FLOAT_PLACEHOLDER_2)) <<
            fCmpgDataArr[i][0] << " : " << fCmpgDataArr[i][1];
      }

   //iflCompare
   testCaseNum = sizeof(ifLcmpeqDataArr) / sizeof(ifLcmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmpeq, compareEQ(ifLcmpeqDataArr[i][0], ifLcmpeqDataArr[i][1]), _ifLcmpeq(ifLcmpeqDataArr[i][0], ifLcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpeqDataArr[i][0]), 2, &(ifLcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(ifLcmpeqDataArr[i][0], ifLcmpeqDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLcmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(ifLcmpeqDataArr[i][0], ifLcmpeqDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpeq, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareEQ(ifLcmpeqDataArr[i][0], ifLcmpeqDataArr[i][1]), lCompareConst(ifLcmpeqDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLcmpgtDataArr) / sizeof(ifLcmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmpgt, compareGT(ifLcmpgtDataArr[i][0], ifLcmpgtDataArr[i][1]), _ifLcmpgt(ifLcmpgtDataArr[i][0], ifLcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpgtDataArr[i][0]), 2, &(ifLcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(ifLcmpgtDataArr[i][0], ifLcmpgtDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLcmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(ifLcmpgtDataArr[i][0], ifLcmpgtDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(ifLcmpgtDataArr[i][0], ifLcmpgtDataArr[i][1]), lCompareConst(ifLcmpgtDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLcmpltDataArr) / sizeof(ifLcmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmplt, compareLT(ifLcmpltDataArr[i][0], ifLcmpltDataArr[i][1]), _ifLcmplt(ifLcmpltDataArr[i][0], ifLcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpltDataArr[i][0]), 2, &(ifLcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(ifLcmpltDataArr[i][0], ifLcmpltDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLcmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(ifLcmpltDataArr[i][0], ifLcmpltDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lCompareConst,
            _numberOfBinaryArgs, TR::iflcmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLT(ifLcmpltDataArr[i][0], ifLcmpltDataArr[i][1]), lCompareConst(ifLcmpltDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //ifdCompare
   testCaseNum = sizeof(dCmpeqDataArr) / sizeof(ifDcmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmpeq, compareEQ(ifDcmpeqDataArr[i][0], ifDcmpeqDataArr[i][1]), _ifDcmpeq(ifDcmpeqDataArr[i][0], ifDcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpeqDataArr[i][0]), 2, &(ifDcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(ifDcmpeqDataArr[i][0], ifDcmpeqDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(ifDcmpeqDataArr[i][0], ifDcmpeqDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpeq, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareEQ(ifDcmpeqDataArr[i][0], ifDcmpeqDataArr[i][1]), dCompareConst(ifDcmpeqDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifDcmpneDataArr) / sizeof(ifDcmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmpne, compareNE(ifDcmpneDataArr[i][0], ifDcmpneDataArr[i][1]), _ifDcmpne(ifDcmpneDataArr[i][0], ifDcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpneDataArr[i][0]), 2, &(ifDcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(ifDcmpneDataArr[i][0], ifDcmpneDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(ifDcmpneDataArr[i][0], ifDcmpneDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpne, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareNE(ifDcmpneDataArr[i][0], ifDcmpneDataArr[i][1]), dCompareConst(ifDcmpneDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifDcmpgtDataArr) / sizeof(ifDcmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmpgt, compareGT(ifDcmpgtDataArr[i][0], ifDcmpgtDataArr[i][1]), _ifDcmpgt(ifDcmpgtDataArr[i][0], ifDcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpgtDataArr[i][0]), 2, &(ifDcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(ifDcmpgtDataArr[i][0], ifDcmpgtDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(ifDcmpgtDataArr[i][0], ifDcmpgtDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpgt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGT(ifDcmpgtDataArr[i][0], ifDcmpgtDataArr[i][1]), dCompareConst(ifDcmpgtDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifDcmpltDataArr) / sizeof(ifDcmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmplt, compareLT(ifDcmpltDataArr[i][0], ifDcmpltDataArr[i][1]), _ifDcmplt(ifDcmpltDataArr[i][0], ifDcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpltDataArr[i][0]), 2, &(ifDcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(ifDcmpltDataArr[i][0], ifDcmpltDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(ifDcmpltDataArr[i][0], ifDcmpltDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmplt, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLT(ifDcmpltDataArr[i][0], ifDcmpltDataArr[i][1]), dCompareConst(ifDcmpltDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifDcmpgeDataArr) / sizeof(ifDcmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmpge, compareGE(ifDcmpgeDataArr[i][0], ifDcmpgeDataArr[i][1]), _ifDcmpge(ifDcmpgeDataArr[i][0], ifDcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpgeDataArr[i][0]), 2, &(ifDcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(ifDcmpgeDataArr[i][0], ifDcmpgeDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(ifDcmpgeDataArr[i][0], ifDcmpgeDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmpge, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareGE(ifDcmpgeDataArr[i][0], ifDcmpgeDataArr[i][1]), dCompareConst(ifDcmpgeDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifDcmpleDataArr) / sizeof(ifDcmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifDcmple, compareLE(ifDcmpleDataArr[i][0], ifDcmpleDataArr[i][1]), _ifDcmple(ifDcmpleDataArr[i][0], ifDcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 4, 1, &(ifDcmpleDataArr[i][0]), 2, &(ifDcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(ifDcmpleDataArr[i][0], ifDcmpleDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifDcmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 1, &(ifDcmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(ifDcmpleDataArr[i][0], ifDcmpleDataArr[i][1]), dCompareConst(DOUBLE_PLACEHOLDER_1, ifDcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifDcmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(dCompareConst,
            _numberOfBinaryArgs, TR::ifdcmple, resolvedMethodName, _argTypesBinaryDouble, TR::Int32, rc, 2, 2, &(ifDcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(dCompareConst, compareLE(ifDcmpleDataArr[i][0], ifDcmpleDataArr[i][1]), dCompareConst(ifDcmpleDataArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   //iffCompare
   testCaseNum = sizeof(ifFcmpeqDataArr) / sizeof(ifFcmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmpeq, compareEQ(ifFcmpeqDataArr[i][0], ifFcmpeqDataArr[i][1]), _ifFcmpeq(ifFcmpeqDataArr[i][0], ifFcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpeqConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpeqDataArr[i][0]), 2, &(ifFcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(ifFcmpeqDataArr[i][0], ifFcmpeqDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpeqConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(ifFcmpeqDataArr[i][0], ifFcmpeqDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpeqConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpeq, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareEQ(ifFcmpeqDataArr[i][0], ifFcmpeqDataArr[i][1]), fCompareConst(ifFcmpeqDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifFcmpneDataArr) / sizeof(ifFcmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmpne, compareNE(ifFcmpneDataArr[i][0], ifFcmpneDataArr[i][1]), _ifFcmpne(ifFcmpneDataArr[i][0], ifFcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpneDataArr[i][0]), 2, &(ifFcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(ifFcmpneDataArr[i][0], ifFcmpneDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(ifFcmpneDataArr[i][0], ifFcmpneDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpne, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareNE(ifFcmpneDataArr[i][0], ifFcmpneDataArr[i][1]), fCompareConst(ifFcmpneDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifFcmpgtDataArr) / sizeof(ifFcmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmpgt, compareGT(ifFcmpgtDataArr[i][0], ifFcmpgtDataArr[i][1]), _ifFcmpgt(ifFcmpgtDataArr[i][0], ifFcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpgtConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpgtDataArr[i][0]), 2, &(ifFcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(ifFcmpgtDataArr[i][0], ifFcmpgtDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpgtConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(ifFcmpgtDataArr[i][0], ifFcmpgtDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpgtConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpgt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGT(ifFcmpgtDataArr[i][0], ifFcmpgtDataArr[i][1]), fCompareConst(ifFcmpgtDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifFcmpltDataArr) / sizeof(ifFcmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmplt, compareLT(ifFcmpltDataArr[i][0], ifFcmpltDataArr[i][1]), _ifFcmplt(ifFcmpltDataArr[i][0], ifFcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpltDataArr[i][0]), 2, &(ifFcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(ifFcmpltDataArr[i][0], ifFcmpltDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(ifFcmpltDataArr[i][0], ifFcmpltDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmplt, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLT(ifFcmpltDataArr[i][0], ifFcmpltDataArr[i][1]), fCompareConst(ifFcmpltDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifFcmpgeDataArr) / sizeof(ifFcmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmpge, compareGE(ifFcmpgeDataArr[i][0], ifFcmpgeDataArr[i][1]), _ifFcmpge(ifFcmpgeDataArr[i][0], ifFcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpgeDataArr[i][0]), 2, &(ifFcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(ifFcmpgeDataArr[i][0], ifFcmpgeDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(ifFcmpgeDataArr[i][0], ifFcmpgeDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmpge, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareGE(ifFcmpgeDataArr[i][0], ifFcmpgeDataArr[i][1]), fCompareConst(ifFcmpgeDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifFcmpleDataArr) / sizeof(ifFcmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifFcmple, compareLE(ifFcmpleDataArr[i][0], ifFcmpleDataArr[i][1]), _ifFcmple(ifFcmpleDataArr[i][0], ifFcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 4, 1, &(ifFcmpleDataArr[i][0]), 2, &(ifFcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(ifFcmpleDataArr[i][0], ifFcmpleDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifFcmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 1, &(ifFcmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(ifFcmpleDataArr[i][0], ifFcmpleDataArr[i][1]), fCompareConst(FLOAT_PLACEHOLDER_1, ifFcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifFcmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(fCompareConst,
            _numberOfBinaryArgs, TR::iffcmple, resolvedMethodName, _argTypesBinaryFloat, TR::Int32, rc, 2, 2, &(ifFcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(fCompareConst, compareLE(ifFcmpleDataArr[i][0], ifFcmpleDataArr[i][1]), fCompareConst(ifFcmpleDataArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   //ifbCompare
   testCaseNum = sizeof(ifBcmpneDataArr) / sizeof(ifBcmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmpne, compareNE(ifBcmpneDataArr[i][0], ifBcmpneDataArr[i][1]), _ifBcmpne(ifBcmpneDataArr[i][0], ifBcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpneConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpneDataArr[i][0]), 2, &(ifBcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(ifBcmpneDataArr[i][0], ifBcmpneDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpneConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(ifBcmpneDataArr[i][0], ifBcmpneDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpneConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpne, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareNE(ifBcmpneDataArr[i][0], ifBcmpneDataArr[i][1]), bCompareConst(ifBcmpneDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBcmpleDataArr) / sizeof(ifBcmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmple, compareLE(ifBcmpleDataArr[i][0], ifBcmpleDataArr[i][1]), _ifBcmple(ifBcmpleDataArr[i][0], ifBcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpleConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpleDataArr[i][0]), 2, &(ifBcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(ifBcmpleDataArr[i][0], ifBcmpleDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpleConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(ifBcmpleDataArr[i][0], ifBcmpleDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpleConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmple, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLE(ifBcmpleDataArr[i][0], ifBcmpleDataArr[i][1]), bCompareConst(ifBcmpleDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBcmpltDataArr) / sizeof(ifBcmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmplt, compareLT(ifBcmpltDataArr[i][0], ifBcmpltDataArr[i][1]), _ifBcmplt(ifBcmpltDataArr[i][0], ifBcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpltConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpltDataArr[i][0]), 2, &(ifBcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(ifBcmpltDataArr[i][0], ifBcmpltDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpltConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(ifBcmpltDataArr[i][0], ifBcmpltDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpltConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmplt, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareLT(ifBcmpltDataArr[i][0], ifBcmpltDataArr[i][1]), bCompareConst(ifBcmpltDataArr[i][0], BYTE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifBcmpgeDataArr) / sizeof(ifBcmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifBcmpge, compareGE(ifBcmpgeDataArr[i][0], ifBcmpgeDataArr[i][1]), _ifBcmpge(ifBcmpgeDataArr[i][0], ifBcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpgeConst1_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 4, 1, &(ifBcmpgeDataArr[i][0]), 2, &(ifBcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(ifBcmpgeDataArr[i][0], ifBcmpgeDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, BYTE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifBcmpgeConst2_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 1, &(ifBcmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(ifBcmpgeDataArr[i][0], ifBcmpgeDataArr[i][1]), bCompareConst(BYTE_PLACEHOLDER_1, ifBcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifBcmpgeConst3_TestCase%d", i + 1);
      compileOpCodeMethod(bCompareConst,
            _numberOfBinaryArgs, TR::ifbcmpge, resolvedMethodName, _argTypesBinaryByte, TR::Int32, rc, 2, 2, &(ifBcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(bCompareConst, compareGE(ifBcmpgeDataArr[i][0], ifBcmpgeDataArr[i][1]), bCompareConst(ifBcmpgeDataArr[i][0], BYTE_PLACEHOLDER_2));
      }
   }

void
PPCOpCodesTest::compileDisabledIntegerArithmeticTestMethods()
   {
   int32_t rc = 0;

   //Jazz103 Work Item 101901
   compileOpCodeMethod(_lAdd, _numberOfBinaryArgs, TR::ladd, "lAdd", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lSub, _numberOfBinaryArgs, TR::lsub, "lSub", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lMul, _numberOfBinaryArgs, TR::lmul, "lMul", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lDiv, _numberOfBinaryArgs, TR::ldiv, "lDiv", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lRem, _numberOfBinaryArgs, TR::lrem, "lRem", _argTypesBinaryLong, TR::Int64, rc);

   //Jazz103 Work Item 103809
   //Testrossa Work Item 121966
   compileOpCodeMethod(_iuDiv, _numberOfBinaryArgs, TR::iudiv, "iuDiv", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iuRem, _numberOfBinaryArgs, TR::iurem, "iuRem", _argTypesBinaryInt, TR::Int32, rc);

   }

void
PPCOpCodesTest::invokeDisabledIntegerArithmeticTests()
   {
   //iudiv
   // TODO: Use ASSERT_DEATH() to catch Remainder by zero which will get "Floating point exception (core dumped)"
   // Test secnario : _iuDiv(UINT_MAXIMUM, 0)
   OMR_CT_EXPECT_EQ(_iuDiv, div(UINT_MAXIMUM, UINT_POS), _iuDiv(UINT_MAXIMUM, UINT_POS));
   OMR_CT_EXPECT_EQ(_iuDiv, div(UINT_MINIMUM, UINT_POS), _iuDiv(UINT_MINIMUM, UINT_POS));

   //iurem
   // TODO: Use ASSERT_DEATH() to catch Remainder by zero which will get "Floating point exception (core dumped)"
   // Test secnario : _iuRem(UINT_MAXIMUM, 0)
   OMR_CT_EXPECT_EQ(_iuRem, rem(UINT_POS, UINT_MAXIMUM),       _iuRem(UINT_POS, UINT_MAXIMUM));
   OMR_CT_EXPECT_EQ(_iuRem, rem(UINT_MAXIMUM, UINT_MAXIMUM),      _iuRem(UINT_MAXIMUM, UINT_MAXIMUM));

   int32_t rc = 0;

   int64_t longAddArr[][2] =
      {
      LONG_ZERO, LONG_ZERO,
      LONG_NEG, LONG_NEG,
      LONG_MINIMUM, LONG_POS,
      LONG_MAXIMUM, LONG_MAXIMUM,
      LONG_POS, LONG_MINIMUM
      };
   int64_t longSubArr[][2] =
      {
      LONG_MAXIMUM, LONG_MINIMUM,
      LONG_NEG, LONG_POS,
      LONG_POS, LONG_MAXIMUM,
      LONG_ZERO, LONG_NEG,
      LONG_MINIMUM, LONG_ZERO
      };
   int64_t longMulArr[][2] =
      {
      LONG_NEG, LONG_MINIMUM,
      LONG_ZERO, LONG_POS,
      LONG_MINIMUM, LONG_NEG,
      LONG_POS, LONG_ZERO,
      LONG_MAXIMUM, LONG_MINIMUM,
      LONG_MINIMUM, LONG_MAXIMUM
      };
   int64_t longDivArr[][2] =
      {
      LONG_NEG, LONG_MAXIMUM,
      LONG_POS, LONG_POS,
      LONG_MAXIMUM, LONG_NEG,
      LONG_ZERO, LONG_MINIMUM
      };
   int64_t longRemArr[][2] =
      {
      LONG_MINIMUM, LONG_MINIMUM,
      LONG_ZERO, LONG_MAXIMUM,
      LONG_POS, LONG_NEG,
      LONG_MAXIMUM, LONG_POS
      };

   uint32_t testCaseArrLength = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   signatureCharJJ_J_testMethodType * lBinaryCons = 0;

   //ladd
   testCaseArrLength = sizeof(longAddArr) / sizeof(longAddArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_lAdd, add(longAddArr[i][0], longAddArr[i][1]), _lAdd(longAddArr[i][0], longAddArr[i][1]));

      sprintf(resolvedMethodName, "lAddConst1_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::ladd,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &longAddArr[i][0], 2, &longAddArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, add(longAddArr[i][0], longAddArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lAddConst2_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::ladd,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &longAddArr[i][0]);
      OMR_CT_EXPECT_EQ(lBinaryCons, add(longAddArr[i][0], longAddArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, longAddArr[i][1]));

      sprintf(resolvedMethodName, "lAddConst3_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::ladd,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &longAddArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, add(longAddArr[i][0], longAddArr[i][1]), lBinaryCons(longAddArr[i][0], LONG_PLACEHOLDER_2));
      }

   //lsub
   testCaseArrLength = sizeof(longSubArr) / sizeof(longSubArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_lSub, sub(longSubArr[i][0], longSubArr[i][1]), _lSub(longSubArr[i][0], longSubArr[i][1]));

      sprintf(resolvedMethodName, "lSubConst1_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lsub,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &longSubArr[i][0], 2, &longSubArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, sub(longSubArr[i][0], longSubArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lSubConst2_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lsub,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &longSubArr[i][0]);
      OMR_CT_EXPECT_EQ(lBinaryCons, sub(longSubArr[i][0], longSubArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, longSubArr[i][1]));

      sprintf(resolvedMethodName, "lSubConst3_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lsub,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &longSubArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, sub(longSubArr[i][0], longSubArr[i][1]), lBinaryCons(longSubArr[i][0], LONG_PLACEHOLDER_2));
      }

   //lmul
   testCaseArrLength = sizeof(longMulArr) / sizeof(longMulArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_lMul, mul(longMulArr[i][0], longMulArr[i][1]), _lMul(longMulArr[i][0], longMulArr[i][1]));

      sprintf(resolvedMethodName, "lMulConst1_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lmul,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &longMulArr[i][0], 2, &longMulArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, mul(longMulArr[i][0], longMulArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lMulConst2_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lmul,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &longMulArr[i][0]);
      OMR_CT_EXPECT_EQ(lBinaryCons, mul(longMulArr[i][0], longMulArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, longMulArr[i][1]));

      sprintf(resolvedMethodName, "lMulConst3_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons, _numberOfBinaryArgs, TR::lmul,
            resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &longMulArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, mul(longMulArr[i][0], longMulArr[i][1]), lBinaryCons(longMulArr[i][0], LONG_PLACEHOLDER_2));
      }

   //ldiv
   //TODO: _lDiv(LONG_INT, 0)
   testCaseArrLength = sizeof(longDivArr) / sizeof(longDivArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_lDiv, div(longDivArr[i][0], longDivArr[i][1]), _lDiv(longDivArr[i][0], longDivArr[i][1]));

      sprintf(resolvedMethodName, "lDivConst1_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::ldiv, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &longDivArr[i][0], 2, &longDivArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, div(longDivArr[i][0], longDivArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lDivConst2_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::ldiv, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &longDivArr[i][0]);
      OMR_CT_EXPECT_EQ(lBinaryCons, div(longDivArr[i][0], longDivArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, longDivArr[i][1]));

      sprintf(resolvedMethodName, "lDivConst3_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::ldiv, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &longDivArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, div(longDivArr[i][0], longDivArr[i][1]), lBinaryCons(longDivArr[i][0], LONG_PLACEHOLDER_2));
      }
   //lrem
   //TODO: _lrem(LONG_INT, 0), _lrem(LONG_NEG, 0),
   testCaseArrLength = sizeof(longRemArr) / sizeof(longRemArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_lRem, rem(longRemArr[i][0], longRemArr[i][1]), _lRem(longRemArr[i][0], longRemArr[i][1]));

      sprintf(resolvedMethodName, "lRemConst1_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::lrem, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &longRemArr[i][0], 2, &longRemArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, rem(longRemArr[i][0], longRemArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lRemConst2_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::lrem, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &longRemArr[i][0]);
      OMR_CT_EXPECT_EQ(lBinaryCons, rem(longRemArr[i][0], longRemArr[i][1]), lBinaryCons(LONG_PLACEHOLDER_1, longRemArr[i][1]));

      sprintf(resolvedMethodName, "lRemConst3_Testcase%d", i);
      compileOpCodeMethod(lBinaryCons,
            _numberOfBinaryArgs, TR::lrem, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &longRemArr[i][1]);
      OMR_CT_EXPECT_EQ(lBinaryCons, rem(longRemArr[i][0], longRemArr[i][1]), lBinaryCons(longRemArr[i][0], LONG_PLACEHOLDER_2));
      }
   }

void
PPCOpCodesTest::compileDisabledFloatArithmeticTestMethods()
   {
   int32_t rc = 0;
   //Jazz103 Work Item 110358

   compileOpCodeMethod(_fAdd, _numberOfBinaryArgs, TR::fadd, "fAdd", _argTypesBinaryFloat, TR::Float, rc);
   compileOpCodeMethod(_fSub, _numberOfBinaryArgs, TR::fsub, "fSub", _argTypesBinaryFloat, TR::Float, rc);
   compileOpCodeMethod(_fMul, _numberOfBinaryArgs, TR::fmul, "fMul", _argTypesBinaryFloat, TR::Float, rc);
   compileOpCodeMethod(_fDiv, _numberOfBinaryArgs, TR::fdiv, "fDiv", _argTypesBinaryFloat, TR::Float, rc);
   compileOpCodeMethod(_dAdd, _numberOfBinaryArgs, TR::dadd, "dAdd", _argTypesBinaryDouble, TR::Double, rc);
   compileOpCodeMethod(_dSub, _numberOfBinaryArgs, TR::dsub, "dSub", _argTypesBinaryDouble, TR::Double, rc);
   compileOpCodeMethod(_dMul, _numberOfBinaryArgs, TR::dmul, "dMul", _argTypesBinaryDouble, TR::Double, rc);
   compileOpCodeMethod(_dDiv, _numberOfBinaryArgs, TR::ddiv, "dDiv", _argTypesBinaryDouble, TR::Double, rc);

   }
void
PPCOpCodesTest::invokeDisabledFloatArithmeticTests()
   {
   float floatAddArr[][2] =
      {
      FLOAT_ZERO, FLOAT_ZERO,
      FLOAT_NEG, FLOAT_NEG,
      FLOAT_MINIMUM, FLOAT_POS,
      FLOAT_MAXIMUM, FLOAT_MAXIMUM,
      FLOAT_NEG, FLOAT_MINIMUM
      };
   float floatSubArr[][2] =
      {
      FLOAT_MAXIMUM, FLOAT_MINIMUM,
      FLOAT_NEG, FLOAT_POS,
      FLOAT_POS, FLOAT_MAXIMUM,
      FLOAT_ZERO, FLOAT_POS,
      FLOAT_MINIMUM, FLOAT_ZERO
      };
   float floatMulArr[][2] =
      {
      FLOAT_NEG, FLOAT_MINIMUM,
      FLOAT_ZERO, FLOAT_POS,
      FLOAT_MINIMUM, FLOAT_NEG,
      FLOAT_POS, FLOAT_ZERO,
      FLOAT_MINIMUM, FLOAT_MAXIMUM,
      FLOAT_MAXIMUM, FLOAT_MINIMUM
      };
   float floatDivArr[][2] =
      {
      FLOAT_NEG, FLOAT_MAXIMUM,
      FLOAT_POS, FLOAT_POS,
      FLOAT_MAXIMUM, FLOAT_NEG,
      FLOAT_ZERO, FLOAT_MINIMUM,
      };
   double doubleAddArr[][2] =
      {
      DOUBLE_ZERO, DOUBLE_ZERO,
      DOUBLE_NEG, DOUBLE_NEG,
      DOUBLE_MINIMUM, DOUBLE_POS,
      DOUBLE_MAXIMUM, DOUBLE_MAXIMUM,
      DOUBLE_POS, DOUBLE_MINIMUM
      };
   double doubleSubArr[][2] =
      {
      DOUBLE_MAXIMUM, DOUBLE_MINIMUM,
      DOUBLE_NEG, DOUBLE_POS,
      DOUBLE_POS, DOUBLE_MAXIMUM,
      DOUBLE_ZERO, DOUBLE_NEG,
      DOUBLE_MINIMUM, DOUBLE_ZERO
      };
   double doubleMulArr[][2] =
      {
      DOUBLE_NEG, DOUBLE_MINIMUM,
      DOUBLE_ZERO, DOUBLE_POS,
      DOUBLE_MINIMUM, DOUBLE_NEG,
      DOUBLE_POS, DOUBLE_ZERO,
      DOUBLE_MINIMUM, DOUBLE_MAXIMUM,
      DOUBLE_MAXIMUM, DOUBLE_MINIMUM
      };
   double doubleDivArr[][2] =
      {
      DOUBLE_NEG, DOUBLE_MAXIMUM,
      DOUBLE_POS, DOUBLE_POS,
      DOUBLE_MAXIMUM, DOUBLE_NEG,
      DOUBLE_ZERO, DOUBLE_MINIMUM,
      };

   signatureCharDD_D_testMethodType * dBinaryConst = 0;
   signatureCharFF_F_testMethodType * fBinaryConst = 0;

   int32_t rc = 0;
   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   //fadd
   testCaseNum = sizeof(floatAddArr) / sizeof(floatAddArr[0]);
   for (uint32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_fAdd, add(floatAddArr[i][0], floatAddArr[i][1]), _fAdd(floatAddArr[i][0], floatAddArr[i][1]));

      sprintf(resolvedMethodName, "fAddConst1_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fadd,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 4, 1, &floatAddArr[i][0], 2, &floatAddArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, add(floatAddArr[i][0], floatAddArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fAddConst2_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fadd,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 1, &floatAddArr[i][0]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, add(floatAddArr[i][0], floatAddArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, floatAddArr[i][1]));

      sprintf(resolvedMethodName, "fAddConst3_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fadd,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 2, &floatAddArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, add(floatAddArr[i][0], floatAddArr[i][1]), fBinaryConst(floatAddArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   //fsub
   testCaseNum = sizeof(floatSubArr) / sizeof(floatSubArr[0]);
   for (uint32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_fSub, sub(floatSubArr[i][0], floatSubArr[i][1]), _fSub(floatSubArr[i][0], floatSubArr[i][1]));

      sprintf(resolvedMethodName, "fSubConst1_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fsub,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 4, 1, &floatSubArr[i][0], 2, &floatSubArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, sub(floatSubArr[i][0], floatSubArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fSubConst2_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fsub,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 1, &floatSubArr[i][0]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, sub(floatSubArr[i][0], floatSubArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, floatSubArr[i][1]));

      sprintf(resolvedMethodName, "fSubConst3_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fsub,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 2, &floatSubArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, sub(floatSubArr[i][0], floatSubArr[i][1]), fBinaryConst(floatSubArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   //fmul
   testCaseNum = sizeof(floatMulArr) / sizeof(floatMulArr[0]);
   for (uint32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_fMul, mul(floatMulArr[i][0], floatMulArr[i][1]), _fMul(floatMulArr[i][0], floatMulArr[i][1]));

      sprintf(resolvedMethodName, "fMulConst1_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fmul,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 4, 1, &floatMulArr[i][0], 2, &floatMulArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, mul(floatMulArr[i][0], floatMulArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fMulConst2_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fmul,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 1, &floatMulArr[i][0]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, mul(floatMulArr[i][0], floatMulArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, floatMulArr[i][1]));

      sprintf(resolvedMethodName, "fMulConst3_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fmul,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 2, &floatMulArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, mul(floatMulArr[i][0], floatMulArr[i][1]), fBinaryConst(floatMulArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   //fdiv
   testCaseNum = sizeof(floatDivArr) / sizeof(floatDivArr[0]);
   for (uint32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_fDiv, div(floatDivArr[i][0], floatDivArr[i][1]), _fDiv(floatDivArr[i][0], floatDivArr[i][1]));

      sprintf(resolvedMethodName, "fDivConst1_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fdiv,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 4, 1, &floatDivArr[i][0], 2, &floatDivArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, div(floatDivArr[i][0], floatDivArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "fDivConst2_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fdiv,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 1, &floatDivArr[i][0]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, div(floatDivArr[i][0], floatDivArr[i][1]), fBinaryConst(FLOAT_PLACEHOLDER_1, floatDivArr[i][1]));

      sprintf(resolvedMethodName, "fDivConst3_Testcase%d", i);
      compileOpCodeMethod(fBinaryConst, _numberOfBinaryArgs, TR::fdiv,
            resolvedMethodName, _argTypesBinaryFloat, TR::Float, rc, 2, 2, &floatDivArr[i][1]);
      OMR_CT_EXPECT_FLOAT_EQ(fBinaryConst, div(floatDivArr[i][0], floatDivArr[i][1]), fBinaryConst(floatDivArr[i][0], FLOAT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(doubleAddArr) / sizeof(doubleAddArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_dAdd, add(doubleAddArr[i][0], doubleAddArr[i][1]), _dAdd(doubleAddArr[i][0], doubleAddArr[i][1]));

      sprintf(resolvedMethodName, "dAddConst1_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dadd,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 4, 1, &doubleAddArr[i][0], 2, &doubleAddArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, add(doubleAddArr[i][0], doubleAddArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dAddConst2_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dadd,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 1, &doubleAddArr[i][0]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, add(doubleAddArr[i][0], doubleAddArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, doubleAddArr[i][1]));

      sprintf(resolvedMethodName, "dAddConst3_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dadd,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 2, &doubleAddArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, add(doubleAddArr[i][0], doubleAddArr[i][1]), dBinaryConst(doubleAddArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(doubleSubArr) / sizeof(doubleSubArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_dSub, sub(doubleSubArr[i][0], doubleSubArr[i][1]), _dSub(doubleSubArr[i][0], doubleSubArr[i][1]));

      sprintf(resolvedMethodName, "dSubConst1_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dsub,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 4, 1, &doubleSubArr[i][0], 2, &doubleSubArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, sub(doubleSubArr[i][0], doubleSubArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dSubConst2_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dsub,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 1, &doubleSubArr[i][0]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, sub(doubleSubArr[i][0], doubleSubArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, doubleSubArr[i][1]));

      sprintf(resolvedMethodName, "dSubConst3_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dsub,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 2, &doubleSubArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, sub(doubleSubArr[i][0], doubleSubArr[i][1]), dBinaryConst(doubleSubArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(doubleMulArr) / sizeof(doubleMulArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_dMul, mul(doubleMulArr[i][0], doubleMulArr[i][1]), _dMul(doubleMulArr[i][0], doubleMulArr[i][1]));

      sprintf(resolvedMethodName, "dMulConst1_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dmul,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 4, 1, &doubleMulArr[i][0], 2, &doubleMulArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, mul(doubleMulArr[i][0], doubleMulArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dMulConst2_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dmul,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 1, &doubleMulArr[i][0]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, mul(doubleMulArr[i][0], doubleMulArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, doubleMulArr[i][1]));

      sprintf(resolvedMethodName, "dMulConst3_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::dmul,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 2, &doubleMulArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, mul(doubleMulArr[i][0], doubleMulArr[i][1]), dBinaryConst(doubleMulArr[i][0], DOUBLE_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(doubleDivArr) / sizeof(doubleDivArr[0]);
   for (int32_t i = 0; i < testCaseNum; i++)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_dDiv, div(doubleDivArr[i][0], doubleDivArr[i][1]), _dDiv(doubleDivArr[i][0], doubleDivArr[i][1]));

      sprintf(resolvedMethodName, "dDivConst1_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::ddiv,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 4, 1, &doubleDivArr[i][0], 2, &doubleDivArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, div(doubleDivArr[i][0], doubleDivArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "dDivConst2_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::ddiv,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 1, &doubleDivArr[i][0]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, div(doubleDivArr[i][0], doubleDivArr[i][1]), dBinaryConst(DOUBLE_PLACEHOLDER_1, doubleDivArr[i][1]));

      sprintf(resolvedMethodName, "dDivConst3_Testcase%d", i);
      compileOpCodeMethod(dBinaryConst, _numberOfBinaryArgs, TR::ddiv,
            resolvedMethodName, _argTypesBinaryDouble, TR::Double, rc, 2, 2, &doubleDivArr[i][1]);
      OMR_CT_EXPECT_DOUBLE_EQ(dBinaryConst, div(doubleDivArr[i][0], doubleDivArr[i][1]), dBinaryConst(doubleDivArr[i][0], DOUBLE_PLACEHOLDER_2));
      }
   }

void
PPCOpCodesTest::compileDisabledMemoryOperationTestMethods()
   {
   int32_t rc = 0;

   //Jazz103 111411 : bstorei unexpected results
   compileOpCodeMethod(_bStorei, _numberOfBinaryArgs, TR::bstorei, "bStorei", _argTypesBinaryAddressByte, TR::Int8, rc);
   }

void
PPCOpCodesTest::invokeDisabledMemoryOperationTests()
   {
   int32_t rc = 0;

   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_MAXIMUM, FLOAT_MINIMUM, FLOAT_ZERO};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_MAXIMUM, DOUBLE_MINIMUM, DOUBLE_ZERO};
   uintptr_t addressDataArray[] = {(uintptr_t)&INT_NEG, (uintptr_t)&LONG_POS, (uintptr_t)&BYTE_MAXIMUM, (uintptr_t)&SHORT_MINIMUM, (uintptr_t)&FLOAT_ZERO};


   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];


   //Jazz103 114122 : indirect load unexpected results
   //indirect load constant

   //Jazz103 111411 : bstorei unexpected results
   int8_t byteStoreDataArray[] = {0, 0, 0, 0, 0};
   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   if (_bStorei != NULL)
      {
      for (int32_t i = 0 ; i < testCaseNum ; i++)
         {
         _bStorei((uintptr_t)(&byteStoreDataArray[i]) , byteDataArray[i]);
         EXPECT_EQ(byteDataArray[i], byteStoreDataArray[i]);
         }
      }
   }

void
PPCOpCodesTest::compileDisabledUnaryTestMethods()
   {
   int32_t rc = 0;
   compileOpCodeMethod(_bNeg, _numberOfUnaryArgs, TR::bneg, "bNeg", _argTypesUnaryByte, TR::Int8, rc);
   compileOpCodeMethod(_sNeg, _numberOfUnaryArgs, TR::sneg, "sNeg", _argTypesUnaryShort, TR::Int16, rc);
   //Jazz103 Work Item 109977
   compileOpCodeMethod(_dNeg, _numberOfUnaryArgs, TR::dneg, "dNeg", _argTypesUnaryDouble, TR::Double, rc);
   compileOpCodeMethod(_fNeg, _numberOfUnaryArgs, TR::fneg, "fNeg", _argTypesUnaryFloat, TR::Float, rc);
   compileOpCodeMethod(_lNeg, _numberOfUnaryArgs, TR::lneg, "lNeg", _argTypesUnaryLong, TR::Int64, rc);

   compileOpCodeMethod(_lAbs, _numberOfUnaryArgs, TR::labs, "lAbs", _argTypesUnaryLong, TR::Int64, rc);

   compileOpCodeMethod(_lReturn, _numberOfUnaryArgs, TR::lreturn, "lReturn", _argTypesUnaryLong, TR::Int64, rc);
   compileOpCodeMethod(_dReturn, _numberOfUnaryArgs, TR::dreturn, "dReturn", _argTypesUnaryDouble, TR::Double, rc);
   compileOpCodeMethod(_fReturn, _numberOfUnaryArgs, TR::freturn, "fReturn", _argTypesUnaryFloat, TR::Float, rc);

   compileOpCodeMethod(_fAbs, _numberOfUnaryArgs, TR::fabs, "fAbs", _argTypesUnaryFloat, TR::Float, rc);
   compileOpCodeMethod(_dAbs, _numberOfUnaryArgs, TR::dabs, "dAbs", _argTypesUnaryDouble, TR::Double, rc);
   }

void
PPCOpCodesTest::invokeDisabledUnaryTests()
   {
   int32_t rc = 0;

   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_ZERO, FLOAT_MAXIMUM, FLOAT_MINIMUM};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_ZERO, DOUBLE_MAXIMUM, DOUBLE_MINIMUM};
   uint32_t uintDataArray[] = {UINT_POS, UINT_MAXIMUM, UINT_MINIMUM};

   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNum = 0;

   signatureCharJ_J_testMethodType  *lUnaryCons = 0;
   signatureCharS_S_testMethodType * sUnaryCons = 0;
   signatureCharB_B_testMethodType * bUnaryCons = 0;
   signatureCharD_D_testMethodType  *dUnaryCons = 0;
   signatureCharF_F_testMethodType  *fUnaryCons = 0;

   //bneg
   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_bNeg, neg(byteDataArray[i]), _bNeg(byteDataArray[i]));
      sprintf(resolvedMethodName, "bNegConst%d", i + 1);
      compileOpCodeMethod(bUnaryCons, _numberOfUnaryArgs, TR::bneg,
            resolvedMethodName, _argTypesUnaryByte, TR::Int8, rc, 2, 1, &byteDataArray[i]);
      OMR_CT_EXPECT_EQ(bUnaryCons, neg(byteDataArray[i]), bUnaryCons(BYTE_PLACEHOLDER_1));
      }

   //sneg
   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_sNeg, neg(shortDataArray[i]), _sNeg(shortDataArray[i]));
      sprintf(resolvedMethodName, "sNegConst%d", i + 1);
      compileOpCodeMethod(sUnaryCons, _numberOfUnaryArgs, TR::sneg,
            resolvedMethodName, _argTypesUnaryShort, TR::Int16, rc, 2, 1, &shortDataArray[i]);
      OMR_CT_EXPECT_EQ(sUnaryCons, neg(shortDataArray[i]), sUnaryCons(SHORT_PLACEHOLDER_1));
      }

   //lneg
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lNeg, neg(longDataArray[i]), _lNeg(longDataArray[i]));
      sprintf(resolvedMethodName, "lNegConst%d", i + 1);
      compileOpCodeMethod(lUnaryCons, _numberOfUnaryArgs, TR::lneg,
            resolvedMethodName, _argTypesUnaryLong, TR::Int64, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(lUnaryCons, neg(longDataArray[i]), lUnaryCons(LONG_PLACEHOLDER_1));
      }

   //fneg
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_FLOAT_EQ(_fNeg, neg(floatDataArray[i]), _fNeg(floatDataArray[i]));
      sprintf(resolvedMethodName, "fNegConst%d", i + 1);
      compileOpCodeMethod(fUnaryCons, _numberOfUnaryArgs, TR::fneg,
            resolvedMethodName, _argTypesUnaryFloat, TR::Float, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_FLOAT_EQ(fUnaryCons, neg(floatDataArray[i]), fUnaryCons(FLOAT_PLACEHOLDER_1));
      }

   //dneg
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_DOUBLE_EQ(_dNeg, neg(doubleDataArray[i]), _dNeg(doubleDataArray[i]));
      sprintf(resolvedMethodName, "dNegConst%d", i + 1);
      compileOpCodeMethod(dUnaryCons, _numberOfUnaryArgs, TR::dneg,
            resolvedMethodName, _argTypesUnaryDouble, TR::Double, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_DOUBLE_EQ(dUnaryCons, neg(doubleDataArray[i]), dUnaryCons(DOUBLE_PLACEHOLDER_1));
      }

   //labs
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lAbs, abs(longDataArray[i]), _lAbs(longDataArray[i]));
      sprintf(resolvedMethodName, "lAbsConst%d", i + 1);
      compileOpCodeMethod(lUnaryCons, _numberOfUnaryArgs, TR::labs,
            resolvedMethodName, _argTypesUnaryLong, TR::Int64, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(lUnaryCons, abs(longDataArray[i]), lUnaryCons(LONG_PLACEHOLDER_1));
      }

   //lReturn
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "lReturnCons%d", i + 1);
      OMR_CT_EXPECT_EQ(_lReturn, longDataArray[i], _lReturn(longDataArray[i]));
      compileOpCodeMethod(lUnaryCons, _numberOfUnaryArgs, TR::lreturn, resolvedMethodName, _argTypesUnaryLong, TR::Int64, rc, 2, 1, &(longDataArray[i]));
      OMR_CT_EXPECT_EQ(lUnaryCons, longDataArray[i], lUnaryCons(LONG_PLACEHOLDER_1));
      }

   //dReturn
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "dReturnCons%d", i + 1);
      OMR_CT_EXPECT_DOUBLE_EQ(_dReturn, doubleDataArray[i], _dReturn(doubleDataArray[i]));
      compileOpCodeMethod(dUnaryCons, _numberOfUnaryArgs, TR::dreturn, resolvedMethodName, _argTypesUnaryDouble, TR::Double, rc, 2, 1, &(doubleDataArray[i]));
      OMR_CT_EXPECT_DOUBLE_EQ(dUnaryCons, doubleDataArray[i], dUnaryCons(DOUBLE_PLACEHOLDER_1));
      }

   //fReturn
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "fReturnCons%d", i + 1);
      OMR_CT_EXPECT_FLOAT_EQ(_fReturn, floatDataArray[i], _fReturn(floatDataArray[i]));
      compileOpCodeMethod(fUnaryCons, _numberOfUnaryArgs, TR::freturn, resolvedMethodName, _argTypesUnaryFloat, TR::Float, rc, 2, 1, &(floatDataArray[i]));
      OMR_CT_EXPECT_FLOAT_EQ(fUnaryCons, floatDataArray[i], fUnaryCons(FLOAT_PLACEHOLDER_1));
      }

   //lConst
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "lConst%d", i + 1);
      compileOpCodeMethod(lUnaryCons, _numberOfUnaryArgs, TR::lconst, resolvedMethodName, _argTypesUnaryLong, TR::Int64, rc, 2, 1, &(longDataArray[i]));
      OMR_CT_EXPECT_EQ(lUnaryCons, longDataArray[i], lUnaryCons(LONG_PLACEHOLDER_1));
      }

   //dConst
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "dConst%d", i + 1);
      compileOpCodeMethod(dUnaryCons, _numberOfUnaryArgs, TR::dconst, resolvedMethodName, _argTypesUnaryDouble, TR::Double, rc, 2, 1, &(doubleDataArray[i]));
      OMR_CT_EXPECT_DOUBLE_EQ(dUnaryCons, doubleDataArray[i], dUnaryCons(DOUBLE_PLACEHOLDER_1));
      }

   //fConst
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "fConst%d", i + 1);
      compileOpCodeMethod(fUnaryCons, _numberOfUnaryArgs, TR::fconst, resolvedMethodName, _argTypesUnaryFloat, TR::Float, rc, 2, 1, &(floatDataArray[i]));
      OMR_CT_EXPECT_FLOAT_EQ(fUnaryCons, floatDataArray[i], fUnaryCons(FLOAT_PLACEHOLDER_1));
      }

   //fabs
   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_fAbs, abs(floatDataArray[i]), _fAbs(floatDataArray[i]));
      sprintf(resolvedMethodName, "fAbsConst%d", i + 1);
      compileOpCodeMethod(fUnaryCons,
            _numberOfUnaryArgs, TR::fabs, resolvedMethodName, _argTypesUnaryFloat, TR::Float, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_EQ(fUnaryCons, abs(floatDataArray[i]), fUnaryCons(FLOAT_PLACEHOLDER_1));
      }

   //dabs
   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_dAbs, abs(doubleDataArray[i]), _dAbs(doubleDataArray[i]));
      sprintf(resolvedMethodName, "dAbsConst%d", i + 1);
      compileOpCodeMethod(dUnaryCons,
            _numberOfUnaryArgs, TR::dabs, resolvedMethodName, _argTypesUnaryDouble, TR::Double, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_EQ(dUnaryCons, abs(doubleDataArray[i]), dUnaryCons(DOUBLE_PLACEHOLDER_1));
      }

   }

void
PPCOpCodesTest::compileDisabledBitwiseTestMethods()
   {
   int32_t rc;

   //Jazz103 Work Item 110362
   compileOpCodeMethod(_lAnd, _numberOfBinaryArgs, TR::land, "lAnd", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lOr, _numberOfBinaryArgs, TR::lor, "lOr", _argTypesBinaryLong, TR::Int64, rc);
   compileOpCodeMethod(_lXor, _numberOfBinaryArgs, TR::lxor, "lXor", _argTypesBinaryLong, TR::Int64, rc);

   }

void
PPCOpCodesTest::invokeDisabledBitwiseTests()
   {
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNum = 0;

   int64_t longAndArr [][2] =
      {
      LONG_MINIMUM, LONG_ZERO,
      LONG_MAXIMUM, LONG_NEG
      };
   int64_t longOrArr [][2] =
      {
      LONG_MAXIMUM, LONG_MAXIMUM,
      LONG_ZERO, LONG_ZERO,
      LONG_NEG, LONG_NEG,
      LONG_POS, LONG_MINIMUM,
      LONG_POS, LONG_ZERO,
      LONG_NEG, LONG_ZERO,
      LONG_ZERO, LONG_MAXIMUM
      };
   int64_t longXorArr [][2] =
      {
      LONG_ZERO, LONG_MINIMUM,
      LONG_POS, LONG_NEG,
      LONG_NEG, LONG_POS,
      LONG_MINIMUM, LONG_MAXIMUM,
      LONG_ZERO, LONG_POS,
      LONG_MINIMUM, LONG_NEG
      };

   signatureCharJJ_J_testMethodType * lBitwiseConst = 0;

   //land
   testCaseNum = sizeof(longAndArr) / sizeof(longAndArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_lAnd, tand(longAndArr[i][0], longAndArr[i][1]), _lAnd(longAndArr[i][0], longAndArr[i][1]));

      sprintf(resolvedMethodName, "lAndConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::land, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &(longAndArr[i][0]), 2, &(longAndArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tand(longAndArr[i][0], longAndArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lAndConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::land, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &(longAndArr[i][0]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tand(longAndArr[i][0], longAndArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, longAndArr[i][1]));

      sprintf(resolvedMethodName, "lAndConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::land, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &(longAndArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tand(longAndArr[i][0], longAndArr[i][1]), lBitwiseConst(longAndArr[i][0], LONG_PLACEHOLDER_2));
     }

   //lor
   testCaseNum = sizeof(longOrArr) / sizeof(longOrArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_lOr, tor(longOrArr[i][0], longOrArr[i][1]), _lOr(longOrArr[i][0], longOrArr[i][1]));

      sprintf(resolvedMethodName, "lOrConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &(longOrArr[i][0]), 2, &(longOrArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tor(longOrArr[i][0], longOrArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lOrConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &(longOrArr[i][0]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tor(longOrArr[i][0], longOrArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, longOrArr[i][1]));

      sprintf(resolvedMethodName, "lOrConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &(longOrArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, tor(longOrArr[i][0], longOrArr[i][1]), lBitwiseConst(longOrArr[i][0], LONG_PLACEHOLDER_2));
     }

   //lxor
   testCaseNum = sizeof(longXorArr) / sizeof(longXorArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_lXor, txor(longXorArr[i][0], longXorArr[i][1]), _lXor(longXorArr[i][0], longXorArr[i][1]));

      sprintf(resolvedMethodName, "lXorConst1_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lxor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 4, 1, &(longXorArr[i][0]), 2, &(longXorArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, txor(longXorArr[i][0], longXorArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lXorConst2_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lxor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 1, &(longXorArr[i][0]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, txor(longXorArr[i][0], longXorArr[i][1]), lBitwiseConst(LONG_PLACEHOLDER_1, longXorArr[i][1]));

      sprintf(resolvedMethodName, "lXorConst3_TestCase%d", i + 1);
      compileOpCodeMethod(lBitwiseConst,
            _numberOfBinaryArgs, TR::lxor, resolvedMethodName, _argTypesBinaryLong, TR::Int64, rc, 2, 2, &(longXorArr[i][1]));
      OMR_CT_EXPECT_EQ(lBitwiseConst, txor(longXorArr[i][0], longXorArr[i][1]), lBitwiseConst(longXorArr[i][0], LONG_PLACEHOLDER_2));
     }
   }

void
PPCOpCodesTest::compileDisabledSelectTestMethods()
   {
   int32_t rc = 0;
   //Jazz103 Work Item 109979
   compileOpCodeMethod(_fselect, _numberOfSelectArgs, TR::fselect, "fSelect", _argTypesSelectFloat, TR::Float, rc);
   compileOpCodeMethod(_dselect, _numberOfSelectArgs, TR::dselect, "dSelect", _argTypesSelectDouble, TR::Double, rc);
   compileOpCodeMethod(_lselect, _numberOfSelectArgs, TR::lselect, "lSelect", _argTypesSelectLong, TR::Int64, rc);

   }

void
PPCOpCodesTest::invokeDisabledSelectTest()
   {

   int32_t rc = 0;
   int32_t fselectChild1Arr[] =
      {
      INT_MAXIMUM, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO, INT_MINIMUM, INT_POS, INT_ZERO, INT_NEG,
      INT_NEG, INT_ZERO, INT_POS, INT_NEG, INT_POS, INT_NEG, INT_ZERO, INT_MINIMUM, INT_MAXIMUM
      };
   int32_t dselectChild1Arr[] =
      {
      INT_MAXIMUM, INT_ZERO, INT_MAXIMUM, INT_MINIMUM, INT_MINIMUM, INT_POS, INT_ZERO, INT_POS, INT_NEG,
      INT_NEG, INT_ZERO, INT_POS, INT_NEG, INT_ZERO, INT_MAXIMUM, INT_MINIMUM, INT_NEG, INT_POS
      };
   int32_t lselectChild1Arr[] =
      {
      INT_ZERO,INT_MINIMUM,INT_NEG,INT_MAXIMUM,INT_POS,INT_MINIMUM,
      INT_MAXIMUM,INT_POS,INT_ZERO,INT_MAXIMUM,INT_MAXIMUM
      };

   int64_t longArr[][2] =
      {
      LONG_MAXIMUM, LONG_MINIMUM,
      LONG_POS, LONG_NEG,
      LONG_NEG, LONG_MAXIMUM,
      LONG_MINIMUM, LONG_NEG,
      LONG_ZERO, LONG_POS,
      LONG_MINIMUM, LONG_POS,
      LONG_POS, LONG_MAXIMUM,
      LONG_POS, LONG_MINIMUM,
      LONG_NEG, LONG_POS,
      LONG_NEG, LONG_ZERO,
      LONG_MAXIMUM, LONG_NEG
      };

   float floatDataArr[][2] =
      {
      FLOAT_NEG, FLOAT_MINIMUM,
      FLOAT_NEG, FLOAT_POS,
      FLOAT_NEG, FLOAT_POS,
      FLOAT_MAXIMUM, FLOAT_ZERO,
      FLOAT_ZERO, FLOAT_POS,
      FLOAT_ZERO, FLOAT_POS,
      FLOAT_ZERO, FLOAT_MAXIMUM,
      FLOAT_POS, FLOAT_NEG,
      FLOAT_POS, FLOAT_MINIMUM,
      FLOAT_MAXIMUM, FLOAT_POS,
      FLOAT_MINIMUM, FLOAT_ZERO,
      FLOAT_MINIMUM, FLOAT_MAXIMUM,
      FLOAT_NEG, FLOAT_MAXIMUM,
      FLOAT_ZERO, FLOAT_NEG,
      FLOAT_POS, FLOAT_ZERO,
      FLOAT_MAXIMUM, FLOAT_NEG,
      FLOAT_ZERO, FLOAT_MINIMUM,
      FLOAT_MINIMUM, FLOAT_POS
      };

   double doubleDataArr[][2] =
      {
      DOUBLE_NEG, DOUBLE_MINIMUM,
      DOUBLE_NEG, DOUBLE_POS,
      DOUBLE_NEG, DOUBLE_POS,
      DOUBLE_MAXIMUM, DOUBLE_ZERO,
      DOUBLE_ZERO, DOUBLE_POS,
      DOUBLE_ZERO, DOUBLE_POS,
      DOUBLE_ZERO, DOUBLE_MAXIMUM,
      DOUBLE_POS, DOUBLE_NEG,
      DOUBLE_POS, DOUBLE_MINIMUM,
      DOUBLE_MAXIMUM, DOUBLE_POS,
      DOUBLE_MINIMUM, DOUBLE_ZERO,
      DOUBLE_MINIMUM, DOUBLE_MAXIMUM,
      DOUBLE_NEG, DOUBLE_MAXIMUM,
      DOUBLE_ZERO, DOUBLE_NEG,
      DOUBLE_POS, DOUBLE_ZERO,
      DOUBLE_MAXIMUM, DOUBLE_NEG,
      DOUBLE_ZERO, DOUBLE_MINIMUM,
      DOUBLE_MINIMUM, DOUBLE_POS
      };

   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNumCheck = 0;
   uint32_t testCaseNum = 0;

   signatureCharIJJ_J_testMethodType * lSelectConst = 0;
   signatureCharIFF_F_testMethodType * fSelectConst = 0;
   signatureCharIDD_D_testMethodType * dSelectConst = 0;


   testCaseNum = sizeof(fselectChild1Arr) / sizeof(fselectChild1Arr[0]);
   testCaseNumCheck = sizeof(floatDataArr) / sizeof(floatDataArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in fselect input array");
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_fselect, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), _fselect(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]));

      sprintf(resolvedMethodName, "fSelectConst1_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 6, 1, &fselectChild1Arr[i], 2, &floatDataArr[i][0], 3, &floatDataArr[i][1]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2, FLOAT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "fSelectConst2_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 4, 1, &fselectChild1Arr[i], 2, &floatDataArr[i][0]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(FLOAT_PLACEHOLDER_1, FLOAT_PLACEHOLDER_2, floatDataArr[i][1]));

      sprintf(resolvedMethodName, "fSelectConst3_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 4, 1, &fselectChild1Arr[i], 3, &floatDataArr[i][1]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(FLOAT_PLACEHOLDER_1, floatDataArr[i][0], FLOAT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "fSelectConst4_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 4, 2, &floatDataArr[i][0], 3, &floatDataArr[i][1]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(fselectChild1Arr[i], FLOAT_PLACEHOLDER_2, FLOAT_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "fSelectConst5_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 2, 1, &fselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(FLOAT_PLACEHOLDER_1, floatDataArr[i][0], floatDataArr[i][1]));

      sprintf(resolvedMethodName, "fSelectConst6_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 2, 2, &floatDataArr[i][0]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(fselectChild1Arr[i], FLOAT_PLACEHOLDER_1, floatDataArr[i][1]));

      sprintf(resolvedMethodName, "fSelectConst7_Testcase%d", i + 1);
      compileOpCodeMethod(fSelectConst, _numberOfSelectArgs, TR::fselect,
            resolvedMethodName, _argTypesSelectFloat, TR::Float, rc, 2, 3, &floatDataArr[i][1]);
      OMR_CT_EXPECT_EQ(fSelectConst, select(fselectChild1Arr[i], floatDataArr[i][0], floatDataArr[i][1]), fSelectConst(fselectChild1Arr[i], floatDataArr[i][0], FLOAT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(dselectChild1Arr) / sizeof(dselectChild1Arr[0]);
   testCaseNumCheck = sizeof(doubleDataArr) / sizeof(doubleDataArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in dselect input array");
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_dselect, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), _dselect(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]));

      sprintf(resolvedMethodName, "dSelectConst1_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 6, 1, &dselectChild1Arr[i], 2, &doubleDataArr[i][0], 3, &doubleDataArr[i][1]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2, DOUBLE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "dSelectConst2_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 4, 1, &dselectChild1Arr[i], 2, &doubleDataArr[i][0]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(DOUBLE_PLACEHOLDER_1, DOUBLE_PLACEHOLDER_2, doubleDataArr[i][1]));

      sprintf(resolvedMethodName, "dSelectConst3_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 4, 1, &dselectChild1Arr[i], 3, &doubleDataArr[i][1]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(DOUBLE_PLACEHOLDER_1, doubleDataArr[i][0], DOUBLE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "dSelectConst4_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 4, 2, &doubleDataArr[i][0], 3, &doubleDataArr[i][1]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(dselectChild1Arr[i], DOUBLE_PLACEHOLDER_2, DOUBLE_PLACEHOLDER_3));

      sprintf(resolvedMethodName, "dSelectConst5_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 2, 1, &dselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(DOUBLE_PLACEHOLDER_1, doubleDataArr[i][0], doubleDataArr[i][1]));

      sprintf(resolvedMethodName, "dSelectConst6_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 2, 2, &doubleDataArr[i][0]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(dselectChild1Arr[i], DOUBLE_PLACEHOLDER_1, doubleDataArr[i][1]));

      sprintf(resolvedMethodName, "dSelectConst7_Testcase%d", i + 1);
      compileOpCodeMethod(dSelectConst, _numberOfSelectArgs, TR::dselect,
            resolvedMethodName, _argTypesSelectDouble, TR::Double, rc, 2, 3, &doubleDataArr[i][1]);
      OMR_CT_EXPECT_EQ(dSelectConst, select(dselectChild1Arr[i], doubleDataArr[i][0], doubleDataArr[i][1]), dSelectConst(dselectChild1Arr[i], doubleDataArr[i][0], DOUBLE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(lselectChild1Arr) / sizeof(lselectChild1Arr[0]);
   testCaseNumCheck = sizeof(longArr) / sizeof(longArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in lselect input array");
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "lSelectConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_lselect, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), _lselect(lselectChild1Arr[i], longArr[i][0], longArr[i][1]));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 6, 1, &lselectChild1Arr[i], 2, &longArr[i][0], 3, &longArr[i][1]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2, LONG_PLACEHOLDER_3));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 4, 1, &lselectChild1Arr[i], 2, &longArr[i][0]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2, longArr[i][1]));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 4, 1, &lselectChild1Arr[i], 3, &longArr[i][1]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(LONG_PLACEHOLDER_1, longArr[i][0], LONG_PLACEHOLDER_3));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 4, 2, &longArr[i][0], 3, &longArr[i][1]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(lselectChild1Arr[i], LONG_PLACEHOLDER_2, LONG_PLACEHOLDER_3));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 2, 1, &lselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(LONG_PLACEHOLDER_1, longArr[i][0], longArr[i][1]));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 2, 2, &longArr[i][0]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(lselectChild1Arr[i], LONG_PLACEHOLDER_1, longArr[i][1]));

      compileOpCodeMethod(lSelectConst, _numberOfSelectArgs, TR::lselect,
            resolvedMethodName, _argTypesSelectLong, TR::Int64, rc, 2, 3, &longArr[i][1]);
      OMR_CT_EXPECT_EQ(lSelectConst, select(lselectChild1Arr[i], longArr[i][0], longArr[i][1]), lSelectConst(lselectChild1Arr[i], longArr[i][0], LONG_PLACEHOLDER_1));
      }
   }

void
PPCOpCodesTest::compileDisabledDirectCallTestMethods()
   {
   int32_t rc = 0;

   //Jazz103 Work item 115240
   compileDirectCallOpCodeMethod(_acall, _numberOfUnaryArgs, TR::areturn, TR::acall, "aCompiledMethod", "acall", _argTypesUnaryAddress, TR::Address,    rc);
   compileDirectCallOpCodeMethod(_iCall, _numberOfUnaryArgs, TR::ireturn, TR::icall, "iCompiledMethod", "iCall", _argTypesUnaryInt,     TR::Int32,      rc);
   compileDirectCallOpCodeMethod(_lCall, _numberOfUnaryArgs, TR::lreturn, TR::lcall, "lCompiledMethod", "lCall", _argTypesUnaryLong,    TR::Int64,      rc);
   compileDirectCallOpCodeMethod(_dCall, _numberOfUnaryArgs, TR::dreturn, TR::dcall, "dCompiledMethod", "dCall", _argTypesUnaryDouble,  TR::Double,     rc);
   compileDirectCallOpCodeMethod(_fCall, _numberOfUnaryArgs, TR::freturn, TR::fcall, "fCompiledMethod", "fCall", _argTypesUnaryFloat,   TR::Float,      rc);
   }

void
PPCOpCodesTest::invokeDisabledDirectCallTest()
   {
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_MAXIMUM, FLOAT_MINIMUM, FLOAT_ZERO};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_MAXIMUM, DOUBLE_MINIMUM, DOUBLE_ZERO};
   uintptr_t aUnaryDataArr[] =
      {
      (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,
      (uintptr_t) &LONG_MAXIMUM,
      (uintptr_t) &LONG_ZERO
      };

   for (int32_t i = 0; i < 5; i++)
      {
      OMR_CT_EXPECT_EQ(_iCall, intDataArray[i], _iCall(intDataArray[i]));
      OMR_CT_EXPECT_DOUBLE_EQ(_dCall, doubleDataArray[i], _dCall(doubleDataArray[i]));
      OMR_CT_EXPECT_FLOAT_EQ(_fCall, floatDataArray[i], _fCall(floatDataArray[i]));
      OMR_CT_EXPECT_EQ(_lCall, longDataArray[i], _lCall(longDataArray[i]));
      OMR_CT_EXPECT_EQ(_acall, aUnaryDataArr[i], _acall(aUnaryDataArr[i]));
      }
   }
} // namespace TestCompiler

#if defined(TR_TARGET_POWER)
//groups by testname
TEST(JITPPCOpCodesTest, UnaryTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCUnaryTest;
   PPCUnaryTest.compileUnaryTestMethods();
   PPCUnaryTest.invokeUnaryTests();
   }

TEST(JITPPCOpCodesTest, MemoryOperationTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCMemoryOperationTest;
   PPCMemoryOperationTest.compileMemoryOperationTestMethods();
   PPCMemoryOperationTest.invokeMemoryOperationTests();
   }

TEST(JITPPCOpCodesTest, SelectTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCSelectTest;
   PPCSelectTest.compileSelectTestMethods();
   PPCSelectTest.invokeSelectTests();
   }

TEST(JITPPCOpCodesTest, CompareTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCCompareTest;
   PPCCompareTest.compileCompareTestMethods();
   PPCCompareTest.invokeCompareTests();
   }

TEST(JITPPCOpCodesTest, BitwiseTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCBitwiseTest;
   PPCBitwiseTest.compileBitwiseTestMethods();
   PPCBitwiseTest.invokeBitwiseTests();
   }

TEST(JITPPCOpCodesTest, PPCAddressTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCAddressTest;
   PPCAddressTest.compileAddressTestMethods();
   PPCAddressTest.invokeAddressTests();
   }

TEST(JITPPCOpCodesTest, UnsupportedOpCodesTest)
   {
   ::TestCompiler::PPCOpCodesTest PPCUnsupportedOpcodesTest;
   PPCUnsupportedOpcodesTest.UnsupportedOpCodesTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEConvertTests)
   {
   //Jazz103 Work Item 109976
   //To temporarily enable "DISABLED" test, append " --gtest_also_run_disabled_tests" in the command line.
   ::TestCompiler::PPCOpCodesTest disabledPPCLEConvertTest;
   disabledPPCLEConvertTest.compileDisabledConvertTestMethods();
   disabledPPCLEConvertTest.invokeDisabledConvertTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLECompareTests)
   {
   //Jazz103 Work Item 106531
   //To temporarily enable "DISABLED" test, append " --gtest_also_run_disabled_tests" in the command line.
   ::TestCompiler::PPCOpCodesTest disabledPPCLECompareTest;
   disabledPPCLECompareTest.compileDisabledCompareTestMethods();
   disabledPPCLECompareTest.invokeDisabledCompareTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEIntegerArithmeticTest)
   {
   //Jazz103 Work Item 101901
   //Jazz103 Work Item 103809
   //Testrossa Work Item 121966
   ::TestCompiler::PPCOpCodesTest disabledPPCLEIntegerArithmeticTest;
   disabledPPCLEIntegerArithmeticTest.compileDisabledIntegerArithmeticTestMethods();
   disabledPPCLEIntegerArithmeticTest.invokeDisabledIntegerArithmeticTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEFloatArithmeticTest)
   {
   //Jazz103 Work Item 110358
   ::TestCompiler::PPCOpCodesTest disabledPPCLEFloatArithmeticTest;
   disabledPPCLEFloatArithmeticTest.compileDisabledFloatArithmeticTestMethods();
   disabledPPCLEFloatArithmeticTest.invokeDisabledFloatArithmeticTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEMemoryOperationTest)
   {
   //Jazz103 Work Item 110360, 111411
   ::TestCompiler::PPCOpCodesTest disabledPPCLEMemoryOperationTest;
   disabledPPCLEMemoryOperationTest.compileDisabledMemoryOperationTestMethods();
   disabledPPCLEMemoryOperationTest.invokeDisabledMemoryOperationTests();
   //
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEUnaryTest)
   {
   //Jazz103 Work Item 109977
   ::TestCompiler::PPCOpCodesTest disabledPPCLEUnaryTest;
   disabledPPCLEUnaryTest.compileDisabledUnaryTestMethods();
   disabledPPCLEUnaryTest.invokeDisabledUnaryTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEBitwiseTest)
   {
   //Jazz103 Work Item 110362
   ::TestCompiler::PPCOpCodesTest disabledPPCLEBitwiseTest;
   disabledPPCLEBitwiseTest.compileDisabledBitwiseTestMethods();
   disabledPPCLEBitwiseTest.invokeDisabledBitwiseTests();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLESelectTest)
   {
   //Jazz103 Work Item 109979
   ::TestCompiler::PPCOpCodesTest disabledPPCLESelectTest;
   disabledPPCLESelectTest.compileDisabledSelectTestMethods();
   disabledPPCLESelectTest.invokeDisabledSelectTest();
   }

TEST(JITPPCOpCodesTest, DISABLED_PPCLEDirectCallTest)
   {
   //Jazz103 Work item 115240
   ::TestCompiler::PPCOpCodesTest disabledPPCDirectCallTest;
   disabledPPCDirectCallTest.compileDisabledDirectCallTestMethods();
   disabledPPCDirectCallTest.invokeDisabledDirectCallTest();
   }
#endif
