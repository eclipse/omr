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

#include <limits.h>
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "gtest/gtest.h"
#include "il/DataTypes.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "tests/injectors/BinaryOpIlInjector.hpp"
#include "tests/injectors/CallIlInjector.hpp"
#include "tests/injectors/ChildlessUnaryOpIlInjector.hpp"
#include "tests/injectors/CmpBranchOpIlInjector.hpp"
#include "tests/injectors/IndirectLoadIlInjector.hpp"
#include "tests/injectors/IndirectStoreIlInjector.hpp"
#include "tests/injectors/StoreOpIlInjector.hpp"
#include "tests/injectors/SelectOpIlInjector.hpp"
#include "tests/injectors/UnaryOpIlInjector.hpp"

#include "tests/OpCodesTest.hpp"

extern "C" uint8_t *compileMethod(TR::IlGeneratorMethodDetails &, TR_Hotness, int32_t &);

namespace TestCompiler
{
//common variables definitions
const int64_t OpCodesTest::LONG_NEG = -9;
const int64_t OpCodesTest::LONG_POS = 9;
const int64_t OpCodesTest::LONG_MAXIMUM = LONG_MAX;
const int64_t OpCodesTest::LONG_MINIMUM = LONG_MIN;
const int64_t OpCodesTest::LONG_ZERO = 0;

const int32_t OpCodesTest::INT_NEG = -9;
const int32_t OpCodesTest::INT_POS = 9;
const int32_t OpCodesTest::INT_MAXIMUM = INT_MAX;
const int32_t OpCodesTest::INT_MINIMUM = INT_MIN;
const int32_t OpCodesTest::INT_ZERO = 0;

const int16_t OpCodesTest::SHORT_NEG = -9;
const int16_t OpCodesTest::SHORT_POS = 9;
const int16_t OpCodesTest::SHORT_MAXIMUM = SHRT_MAX;
const int16_t OpCodesTest::SHORT_MINIMUM = SHRT_MIN;
const int16_t OpCodesTest::SHORT_ZERO = 0;

const int8_t OpCodesTest::BYTE_NEG = -9;
const int8_t OpCodesTest::BYTE_POS = 9;
const int8_t OpCodesTest::BYTE_MAXIMUM = CHAR_MAX;
const int8_t OpCodesTest::BYTE_MINIMUM = CHAR_MIN;
const int8_t OpCodesTest::BYTE_ZERO = 0;

const double OpCodesTest::DOUBLE_MINIMUM = - DBL_MAX;
const double OpCodesTest::DOUBLE_MAXIMUM = DBL_MAX;
const double OpCodesTest::DOUBLE_POS = 9.34;
const double OpCodesTest::DOUBLE_NEG = -9.34;
const double OpCodesTest::DOUBLE_ZERO = 0;

const float OpCodesTest::FLOAT_MINIMUM = - FLT_MAX;
const float OpCodesTest::FLOAT_MAXIMUM = FLT_MAX;
const float OpCodesTest::FLOAT_POS = 9.31;
const float OpCodesTest::FLOAT_NEG = -9.31;
const float OpCodesTest::FLOAT_ZERO = 0;

const uint8_t OpCodesTest::UBYTE_POS = 9;
const uint8_t OpCodesTest::UBYTE_MAXIMUM = UCHAR_MAX;
const uint8_t OpCodesTest::UBYTE_MINIMUM = 0;

const uint16_t OpCodesTest::USHORT_POS = 9;
const uint16_t OpCodesTest::USHORT_MAXIMUM = USHRT_MAX;
const uint16_t OpCodesTest::USHORT_MINIMUM = 0;

const uint32_t OpCodesTest::UINT_POS = 9;
const uint32_t OpCodesTest::UINT_MAXIMUM = UINT_MAX;
const uint32_t OpCodesTest::UINT_MINIMUM = 0;

const uint64_t OpCodesTest::ULONG_POS = 9;
const uint64_t OpCodesTest::ULONG_MAXIMUM = ULONG_MAX;
const uint64_t OpCodesTest::ULONG_MINIMUM = 0;

const int8_t OpCodesTest::BYTE_PLACEHOLDER_1 = 50;
const int8_t OpCodesTest::BYTE_PLACEHOLDER_2 = 70;
const int8_t OpCodesTest::BYTE_PLACEHOLDER_3 = 80;

const int16_t OpCodesTest::SHORT_PLACEHOLDER_1 = 100;
const int16_t OpCodesTest::SHORT_PLACEHOLDER_2 = 200;
const int16_t OpCodesTest::SHORT_PLACEHOLDER_3 = 300;

const int32_t OpCodesTest::INT_PLACEHOLDER_1 = 100;
const int32_t OpCodesTest::INT_PLACEHOLDER_2 = 200;
const int32_t OpCodesTest::INT_PLACEHOLDER_3 = 300;

const int64_t OpCodesTest::LONG_PLACEHOLDER_1 = 100;
const int64_t OpCodesTest::LONG_PLACEHOLDER_2 = 200;
const int64_t OpCodesTest::LONG_PLACEHOLDER_3 = 300;

const float OpCodesTest::FLOAT_PLACEHOLDER_1 = 100.11;
const float OpCodesTest::FLOAT_PLACEHOLDER_2 = 200.12;
const float OpCodesTest::FLOAT_PLACEHOLDER_3 = 300.13;

const double OpCodesTest::DOUBLE_PLACEHOLDER_1 = 100.21;
const double OpCodesTest::DOUBLE_PLACEHOLDER_2 = 200.22;
const double OpCodesTest::DOUBLE_PLACEHOLDER_3 = 300.23;

const uintptr_t OpCodesTest::ADDRESS_PLACEHOLDER_1 = (uintptr_t)100;
const uintptr_t OpCodesTest::ADDRESS_PLACEHOLDER_2 = (uintptr_t)200;
const uintptr_t OpCodesTest::ADDRESS_PLACEHOLDER_3 = (uintptr_t)300;

TR::DataType OpCodesTest::_argTypesUnaryByte[_numberOfUnaryArgs] = {TR::Int8};
TR::DataType OpCodesTest::_argTypesUnaryShort[_numberOfUnaryArgs] = {TR::Int16};
TR::DataType OpCodesTest::_argTypesUnaryInt[_numberOfUnaryArgs] = {TR::Int32};
TR::DataType OpCodesTest::_argTypesUnaryLong[_numberOfUnaryArgs] = {TR::Int64};
TR::DataType OpCodesTest::_argTypesUnaryFloat[_numberOfUnaryArgs] = {TR::Float};
TR::DataType OpCodesTest::_argTypesUnaryDouble[_numberOfUnaryArgs] = {TR::Double};
TR::DataType OpCodesTest::_argTypesUnaryAddress[_numberOfUnaryArgs] = {TR::Address};

TR::DataType OpCodesTest::_argTypesBinaryByte[_numberOfBinaryArgs] = {TR::Int8, TR::Int8};
TR::DataType OpCodesTest::_argTypesBinaryShort[_numberOfBinaryArgs] = {TR::Int16, TR::Int16};
TR::DataType OpCodesTest::_argTypesBinaryInt[_numberOfBinaryArgs] = {TR::Int32, TR::Int32};
TR::DataType OpCodesTest::_argTypesBinaryLong[_numberOfBinaryArgs] = {TR::Int64, TR::Int64};
TR::DataType OpCodesTest::_argTypesBinaryFloat[_numberOfBinaryArgs] = {TR::Float, TR::Float};
TR::DataType OpCodesTest::_argTypesBinaryDouble[_numberOfBinaryArgs] = {TR::Double, TR::Double};
TR::DataType OpCodesTest::_argTypesBinaryAddress[_numberOfBinaryArgs] = {TR::Address, TR::Address};

TR::DataType OpCodesTest::_argTypesSelectByte[_numberOfSelectArgs] = {TR::Int32,TR::Int8, TR::Int8};
TR::DataType OpCodesTest::_argTypesSelectShort[_numberOfSelectArgs] = {TR::Int32,TR::Int16, TR::Int16};
TR::DataType OpCodesTest::_argTypesSelectInt[_numberOfSelectArgs] = {TR::Int32,TR::Int32, TR::Int32};
TR::DataType OpCodesTest::_argTypesSelectLong[_numberOfSelectArgs] = {TR::Int32,TR::Int64, TR::Int64};
TR::DataType OpCodesTest::_argTypesSelectFloat[_numberOfSelectArgs] = {TR::Int32,TR::Float, TR::Float};
TR::DataType OpCodesTest::_argTypesSelectDouble[_numberOfSelectArgs] = {TR::Int32,TR::Double, TR::Double};
TR::DataType OpCodesTest::_argTypesSelectAddress[_numberOfSelectArgs] = {TR::Int32,TR::Address, TR::Address};

TR::DataType OpCodesTest::_argTypesBinaryAddressByte[_numberOfBinaryArgs] = {TR::Address, TR::Int8};
TR::DataType OpCodesTest::_argTypesBinaryAddressShort[_numberOfBinaryArgs] = {TR::Address,TR::Int16};
TR::DataType OpCodesTest::_argTypesBinaryAddressInt[_numberOfBinaryArgs] = {TR::Address, TR::Int32};
TR::DataType OpCodesTest::_argTypesBinaryAddressLong[_numberOfBinaryArgs] = {TR::Address, TR::Int64};
TR::DataType OpCodesTest::_argTypesBinaryAddressFloat[_numberOfBinaryArgs] = {TR::Address, TR::Float};
TR::DataType OpCodesTest::_argTypesBinaryAddressDouble[_numberOfBinaryArgs] = {TR::Address, TR::Double};
TR::DataType OpCodesTest::_argTypesBinaryAddressAddress[_numberOfBinaryArgs] = {TR::Address, TR::Address};

signatureCharI_I_testMethodType  * OpCodesTest::_iByteswap = 0;
//Neg
signatureCharB_B_testMethodType  * OpCodesTest::_bNeg = 0;
signatureCharS_S_testMethodType  * OpCodesTest::_sNeg = 0;
signatureCharI_I_testMethodType  * OpCodesTest::_iNeg = 0;
signatureCharJ_J_testMethodType  * OpCodesTest::_lNeg = 0;
signatureCharD_D_testMethodType  * OpCodesTest::_dNeg = 0;
signatureCharF_F_testMethodType  * OpCodesTest::_fNeg = 0;

//Convert Group
signatureCharI_J_testMethodType  * OpCodesTest::_i2l = 0;
signatureCharI_F_testMethodType  * OpCodesTest::_i2f = 0;
signatureCharI_D_testMethodType  * OpCodesTest::_i2d = 0;
signatureCharI_B_testMethodType  * OpCodesTest::_i2b = 0;
signatureCharI_S_testMethodType  * OpCodesTest::_i2s = 0;
unsignedSignatureCharI_J_testMethodType  * OpCodesTest::_iu2l = 0;
unsignedSignatureCharI_F_testMethodType  * OpCodesTest::_iu2f = 0;
unsignedSignatureCharI_D_testMethodType  * OpCodesTest::_iu2d = 0;

signatureCharJ_I_testMethodType  * OpCodesTest::_l2i = 0;
signatureCharJ_F_testMethodType  * OpCodesTest::_l2f = 0;
signatureCharJ_D_testMethodType  * OpCodesTest::_l2d = 0;
signatureCharJ_B_testMethodType  * OpCodesTest::_l2b = 0;
signatureCharJ_S_testMethodType  * OpCodesTest::_l2s = 0;
unsignedSignatureCharJ_F_testMethodType  * OpCodesTest::_lu2f = 0;
unsignedSignatureCharJ_D_testMethodType  * OpCodesTest::_lu2d = 0;

signatureCharF_I_testMethodType  * OpCodesTest::_f2i = 0;
signatureCharF_J_testMethodType  * OpCodesTest::_f2l = 0;
signatureCharF_D_testMethodType  * OpCodesTest::_f2d = 0;
signatureCharF_B_testMethodType  * OpCodesTest::_f2b = 0;
signatureCharF_S_testMethodType  * OpCodesTest::_f2s = 0;

signatureCharD_I_testMethodType  * OpCodesTest::_d2i = 0;
signatureCharD_J_testMethodType  * OpCodesTest::_d2l = 0;
signatureCharD_F_testMethodType  * OpCodesTest::_d2f = 0;
signatureCharD_B_testMethodType  * OpCodesTest::_d2b = 0;
signatureCharD_S_testMethodType  * OpCodesTest::_d2s = 0;

signatureCharS_I_testMethodType  * OpCodesTest::_s2i = 0;
signatureCharS_J_testMethodType  * OpCodesTest::_s2l = 0;
signatureCharS_F_testMethodType  * OpCodesTest::_s2f = 0;
signatureCharS_D_testMethodType  * OpCodesTest::_s2d = 0;
signatureCharS_B_testMethodType  * OpCodesTest::_s2b = 0;
unsignedSignatureCharS_I_testMethodType  * OpCodesTest::_su2i = 0;
unsignedSignatureCharS_J_testMethodType  * OpCodesTest::_su2l = 0;
unsignedSignatureCharS_F_testMethodType  * OpCodesTest::_su2f = 0;
unsignedSignatureCharS_D_testMethodType  * OpCodesTest::_su2d = 0;

signatureCharB_I_testMethodType  * OpCodesTest::_b2i = 0;
signatureCharB_J_testMethodType  * OpCodesTest::_b2l = 0;
signatureCharB_F_testMethodType  * OpCodesTest::_b2f = 0;
signatureCharB_D_testMethodType  * OpCodesTest::_b2d = 0;
signatureCharB_S_testMethodType  * OpCodesTest::_b2s = 0;
unsignedSignatureCharB_I_testMethodType  * OpCodesTest::_bu2i = 0;
unsignedSignatureCharB_J_testMethodType  * OpCodesTest::_bu2l = 0;
unsignedSignatureCharB_F_testMethodType  * OpCodesTest::_bu2f = 0;
unsignedSignatureCharB_D_testMethodType  * OpCodesTest::_bu2d = 0;
unsignedSignatureCharB_S_testMethodType  * OpCodesTest::_bu2s = 0;

//Abs
signatureCharI_I_testMethodType  * OpCodesTest::_iAbs = 0;
signatureCharJ_J_testMethodType  * OpCodesTest::_lAbs = 0;
signatureCharD_D_testMethodType  * OpCodesTest::_dAbs = 0;
signatureCharF_F_testMethodType  * OpCodesTest::_fAbs = 0;

//Sqrt
signatureCharD_D_testMethodType  * OpCodesTest::_dSqrt = 0;
signatureCharF_F_testMethodType  * OpCodesTest::_fSqrt = 0;

//Load
signatureCharI_I_testMethodType  *OpCodesTest::_iLoad = 0;
signatureCharJ_J_testMethodType  *OpCodesTest::_lLoad = 0;
signatureCharD_D_testMethodType  *OpCodesTest::_dLoad = 0;
signatureCharF_F_testMethodType  *OpCodesTest::_fLoad = 0;
signatureCharB_B_testMethodType  *OpCodesTest::_bLoad = 0;
signatureCharS_S_testMethodType  *OpCodesTest::_sLoad = 0;

//Store
signatureCharI_I_testMethodType * OpCodesTest::_iStore = 0;
signatureCharJ_J_testMethodType * OpCodesTest::_lStore = 0;
signatureCharB_B_testMethodType * OpCodesTest::_bStore = 0;
signatureCharS_S_testMethodType * OpCodesTest::_sStore = 0;
signatureCharD_D_testMethodType * OpCodesTest::_dStore = 0;
signatureCharF_F_testMethodType * OpCodesTest::_fStore = 0;

//Return
signatureCharI_I_testMethodType  * OpCodesTest::_iReturn = 0;
signatureCharJ_J_testMethodType  * OpCodesTest::_lReturn = 0;
signatureCharD_D_testMethodType  * OpCodesTest::_dReturn = 0;
signatureCharF_F_testMethodType  * OpCodesTest::_fReturn = 0;

//direct call
signatureCharI_I_testMethodType  * OpCodesTest::_iCall = 0;
TR::ResolvedMethod * OpCodesTest::_int32Compilee;
signatureCharI_I_testMethodType  * OpCodesTest::_int32CompiledMethod = 0;
signatureCharJ_J_testMethodType  * OpCodesTest::_lCall = 0;
TR::ResolvedMethod * OpCodesTest::_int64Compilee;
signatureCharJ_J_testMethodType  * OpCodesTest::_int64CompiledMethod = 0;
signatureCharD_D_testMethodType  * OpCodesTest::_dCall = 0;
TR::ResolvedMethod * OpCodesTest::_doubleCompilee;
signatureCharD_D_testMethodType  * OpCodesTest::_doubleCompiledMethod = 0;
signatureCharF_F_testMethodType  * OpCodesTest::_fCall = 0;
TR::ResolvedMethod * OpCodesTest::_floatCompilee;
signatureCharF_F_testMethodType  * OpCodesTest::_floatCompiledMethod = 0;

//indirect load
signatureCharL_I_testMethodType  * OpCodesTest::_iLoadi = 0;
signatureCharL_J_testMethodType  * OpCodesTest::_lLoadi = 0;
signatureCharL_D_testMethodType  * OpCodesTest::_dLoadi = 0;
signatureCharL_F_testMethodType  * OpCodesTest::_fLoadi = 0;
signatureCharL_B_testMethodType  * OpCodesTest::_bLoadi = 0;
signatureCharL_S_testMethodType  * OpCodesTest::_sLoadi = 0;
signatureCharL_L_testMethodType  * OpCodesTest::_aLoadi = 0;

//indirect store
signatureCharLI_I_testMethodType  * OpCodesTest::_iStorei = 0;
signatureCharLJ_J_testMethodType  * OpCodesTest::_lStorei = 0;
signatureCharLD_D_testMethodType  * OpCodesTest::_dStorei = 0;
signatureCharLF_F_testMethodType  * OpCodesTest::_fStorei = 0;
signatureCharLB_B_testMethodType  * OpCodesTest::_bStorei = 0;
signatureCharLS_S_testMethodType  * OpCodesTest::_sStorei = 0;
signatureCharLL_L_testMethodType  * OpCodesTest::_aStorei = 0;

//Integer Arithmetic
signatureCharBB_B_testMethodType  * OpCodesTest::_bAdd = 0;
signatureCharBB_B_testMethodType  * OpCodesTest::_bSub = 0;
signatureCharBB_B_testMethodType  * OpCodesTest::_bMul = 0;
signatureCharBB_B_testMethodType  * OpCodesTest::_bDiv = 0;
signatureCharBB_B_testMethodType  * OpCodesTest::_bRem = 0;

signatureCharSS_S_testMethodType  * OpCodesTest::_sAdd = 0;
signatureCharSS_S_testMethodType  * OpCodesTest::_sSub = 0;
signatureCharSS_S_testMethodType  * OpCodesTest::_sMul = 0;
signatureCharSS_S_testMethodType  * OpCodesTest::_sDiv = 0;
signatureCharSS_S_testMethodType  * OpCodesTest::_sRem = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iDiv = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_iMul = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_iMulh = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_iRem = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_iAdd = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_iSub = 0;
unsignedSignatureCharII_I_testMethodType  * OpCodesTest::_iuRem = 0;
unsignedSignatureCharII_I_testMethodType  * OpCodesTest::_iuDiv = 0;
unsignedSignatureCharII_I_testMethodType  * OpCodesTest::_iuMul = 0;
unsignedSignatureCharII_I_testMethodType  * OpCodesTest::_iuMulh = 0;

signatureCharJJ_J_testMethodType  * OpCodesTest::_lAdd = 0;
signatureCharJJ_J_testMethodType  * OpCodesTest::_lSub = 0;
signatureCharJJ_J_testMethodType  * OpCodesTest::_lMul = 0;
signatureCharJJ_J_testMethodType  * OpCodesTest::_lDiv = 0;
signatureCharJJ_J_testMethodType  * OpCodesTest::_lRem = 0;
unsignedSignatureCharJJ_J_testMethodType  * OpCodesTest::_luDiv = 0;

//Float Arithmetic
signatureCharFF_F_testMethodType  * OpCodesTest::_fAdd = 0;
signatureCharFF_F_testMethodType  * OpCodesTest::_fSub = 0;
signatureCharFF_F_testMethodType  * OpCodesTest::_fMul = 0;
signatureCharFF_F_testMethodType  * OpCodesTest::_fDiv = 0;
signatureCharFF_F_testMethodType  * OpCodesTest::_fRem = 0;

//TR::Double
signatureCharDD_D_testMethodType  * OpCodesTest::_dAdd = 0;
signatureCharDD_D_testMethodType  * OpCodesTest::_dSub = 0;
signatureCharDD_D_testMethodType  * OpCodesTest::_dDiv = 0;
signatureCharDD_D_testMethodType  * OpCodesTest::_dMul = 0;
signatureCharDD_D_testMethodType  * OpCodesTest::_dRem = 0;

//Bitwise
signatureCharII_I_testMethodType * OpCodesTest::_iAnd = 0;
signatureCharJJ_J_testMethodType * OpCodesTest::_lAnd = 0;
signatureCharII_I_testMethodType * OpCodesTest::_iOr = 0;
signatureCharJJ_J_testMethodType * OpCodesTest::_lOr = 0;
signatureCharII_I_testMethodType * OpCodesTest::_iXor = 0;
signatureCharJJ_J_testMethodType * OpCodesTest::_lXor = 0;
signatureCharSS_S_testMethodType * OpCodesTest::_sAnd = 0;
signatureCharSS_S_testMethodType * OpCodesTest::_sOr = 0;
signatureCharSS_S_testMethodType * OpCodesTest::_sXor = 0;
signatureCharBB_B_testMethodType * OpCodesTest::_bAnd = 0;
signatureCharBB_B_testMethodType * OpCodesTest::_bOr = 0;
signatureCharBB_B_testMethodType * OpCodesTest::_bXor = 0;

//Compare
signatureCharII_I_testMethodType  * OpCodesTest::_iCmpeq = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmpeq = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmpeq = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmpeq = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmpeq = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmpeq = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iCmpne = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmpne = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmpne = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmpne = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmpne = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmpne = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iCmplt = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmplt = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmplt = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmplt = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmplt = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmplt = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iCmpgt = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmpgt = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmpgt = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmpgt = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmpgt = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmpgt = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iCmple = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmple = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmple = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmple = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmple = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmple = 0;

signatureCharII_I_testMethodType  * OpCodesTest::_iCmpge = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_lCmpge = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_dCmpge = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_fCmpge = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_sCmpge = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_bCmpge = 0;

unsignedCompareSignatureCharII_I_testMethodType  * OpCodesTest::_iuCmplt = 0;
unsignedCompareSignatureCharII_I_testMethodType  * OpCodesTest::_iuCmpge = 0;
unsignedCompareSignatureCharII_I_testMethodType  * OpCodesTest::_iuCmpgt = 0;
unsignedCompareSignatureCharII_I_testMethodType  * OpCodesTest::_iuCmple = 0;
unsignedCompareSignatureCharJJ_I_testMethodType  * OpCodesTest::_luCmplt = 0;
unsignedCompareSignatureCharJJ_I_testMethodType  * OpCodesTest::_luCmpge = 0;
unsignedCompareSignatureCharJJ_I_testMethodType  * OpCodesTest::_luCmpgt = 0;
unsignedCompareSignatureCharJJ_I_testMethodType  * OpCodesTest::_luCmple = 0;
unsignedCompareSignatureCharBB_I_testMethodType  * OpCodesTest::_buCmplt = 0;
unsignedCompareSignatureCharBB_I_testMethodType  * OpCodesTest::_buCmpge = 0;
unsignedCompareSignatureCharBB_I_testMethodType  * OpCodesTest::_buCmpgt = 0;
unsignedCompareSignatureCharBB_I_testMethodType  * OpCodesTest::_buCmple = 0;
unsignedCompareSignatureCharSS_I_testMethodType  * OpCodesTest::_suCmplt = 0;
unsignedCompareSignatureCharSS_I_testMethodType  * OpCodesTest::_suCmpge = 0;
unsignedCompareSignatureCharSS_I_testMethodType  * OpCodesTest::_suCmpgt = 0;
unsignedCompareSignatureCharSS_I_testMethodType  * OpCodesTest::_suCmple = 0;

signatureCharJJ_I_testMethodType * OpCodesTest::_lCmp = 0;
signatureCharFF_I_testMethodType * OpCodesTest::_fCmpl = 0;
signatureCharFF_I_testMethodType * OpCodesTest::_fCmpg = 0;
signatureCharDD_I_testMethodType * OpCodesTest::_dCmpl = 0;
signatureCharDD_I_testMethodType * OpCodesTest::_dCmpg = 0;

//CompareBranch
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmpeq = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmpne = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmpgt = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmplt = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmpge = 0;
signatureCharII_I_testMethodType  * OpCodesTest::_ifIcmple = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmpeq = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmpne = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmpgt = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmplt = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmpge = 0;
signatureCharJJ_I_testMethodType  * OpCodesTest::_ifLcmple = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmpeq = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmpne = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmpgt = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmplt = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmpge = 0;
signatureCharFF_I_testMethodType  * OpCodesTest::_ifFcmple = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmpeq = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmpne = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmpgt = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmplt = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmpge = 0;
signatureCharDD_I_testMethodType  * OpCodesTest::_ifDcmple = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmpeq = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmpne = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmpgt = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmplt = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmpge = 0;
signatureCharSS_I_testMethodType  * OpCodesTest::_ifScmple = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmpeq = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmpne = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmpgt = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmplt = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmpge = 0;
signatureCharBB_I_testMethodType  * OpCodesTest::_ifBcmple = 0;
unsignedCompareSignatureCharII_I_testMethodType * OpCodesTest::_ifIuCmplt = 0;
unsignedCompareSignatureCharII_I_testMethodType * OpCodesTest::_ifIuCmpge = 0;
unsignedCompareSignatureCharII_I_testMethodType * OpCodesTest::_ifIuCmpgt = 0;
unsignedCompareSignatureCharII_I_testMethodType * OpCodesTest::_ifIuCmple = 0;
unsignedCompareSignatureCharJJ_I_testMethodType * OpCodesTest::_ifLuCmplt = 0;
unsignedCompareSignatureCharJJ_I_testMethodType * OpCodesTest::_ifLuCmpge = 0;
unsignedCompareSignatureCharJJ_I_testMethodType * OpCodesTest::_ifLuCmpgt = 0;
unsignedCompareSignatureCharJJ_I_testMethodType * OpCodesTest::_ifLuCmple = 0;
unsignedCompareSignatureCharBB_I_testMethodType * OpCodesTest::_ifBuCmplt = 0;
unsignedCompareSignatureCharBB_I_testMethodType * OpCodesTest::_ifBuCmpge = 0;
unsignedCompareSignatureCharBB_I_testMethodType * OpCodesTest::_ifBuCmpgt = 0;
unsignedCompareSignatureCharBB_I_testMethodType * OpCodesTest::_ifBuCmple = 0;
unsignedCompareSignatureCharSS_I_testMethodType * OpCodesTest::_ifSuCmplt = 0;
unsignedCompareSignatureCharSS_I_testMethodType * OpCodesTest::_ifSuCmpge = 0;
unsignedCompareSignatureCharSS_I_testMethodType * OpCodesTest::_ifSuCmpgt = 0;
unsignedCompareSignatureCharSS_I_testMethodType * OpCodesTest::_ifSuCmple = 0;

//Select operators
signatureCharIBB_B_testMethodType * OpCodesTest::_bselect = 0;
signatureCharISS_S_testMethodType * OpCodesTest::_sselect = 0;
signatureCharIII_I_testMethodType * OpCodesTest::_iselect = 0;
signatureCharIJJ_J_testMethodType * OpCodesTest::_lselect = 0;
signatureCharIFF_F_testMethodType * OpCodesTest::_fselect = 0;
signatureCharIDD_D_testMethodType * OpCodesTest::_dselect = 0;

//Address opcodes
signatureCharL_L_testMethodType * OpCodesTest::_acall = 0;
TR::ResolvedMethod * OpCodesTest::_addressCompilee;
signatureCharL_L_testMethodType  * OpCodesTest::_addressCompiledMethod = 0;

signatureCharL_L_testMethodType * OpCodesTest::_aload = 0;
signatureCharL_L_testMethodType * OpCodesTest::_astore = 0;
signatureCharL_L_testMethodType * OpCodesTest::_areturn = 0;
signatureCharL_B_testMethodType * OpCodesTest::_a2b = 0;
signatureCharL_S_testMethodType * OpCodesTest::_a2s = 0;
signatureCharL_I_testMethodType * OpCodesTest::_a2i = 0;
signatureCharL_J_testMethodType * OpCodesTest::_a2l = 0;
signatureCharB_L_testMethodType * OpCodesTest::_b2a = 0;
signatureCharS_L_testMethodType * OpCodesTest::_s2a = 0;
signatureCharI_L_testMethodType * OpCodesTest::_i2a = 0;
signatureCharJ_L_testMethodType * OpCodesTest::_l2a = 0;
unsignedSignatureCharB_L_testMethodType * OpCodesTest::_bu2a = 0;
unsignedSignatureCharS_L_testMethodType * OpCodesTest::_su2a = 0;
unsignedSignatureCharI_L_testMethodType * OpCodesTest::_iu2a = 0;
unsignedSignatureCharJ_L_testMethodType * OpCodesTest::_lu2a = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmpeq = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmpne = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmplt = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmpge = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmple = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_acmpgt = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmpeq = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmpne = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmplt = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmpge = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmple = 0;
signatureCharLL_I_testMethodType * OpCodesTest::_ifacmpgt = 0;
signatureCharILL_L_testMethodType * OpCodesTest::_aselect = 0;


void
OpCodesTest::compileTestMethods()
   {
   }

void
OpCodesTest::addUnsupportedOpCodeTest(int32_t opCodeArgsNum,
      TR::ILOpCodes opCode,
      char * resolvedMethodName,
      TR::DataType * argTypes,
      TR::DataType returnType)
   {
   typedef void (*functype)();
   functype never_succeeds;

   int32_t returnCode = 0;
   compileOpCodeMethod(never_succeeds, opCodeArgsNum, opCode, resolvedMethodName, argTypes, returnType, returnCode);
   EXPECT_TRUE(COMPILATION_IL_GEN_FAILURE == returnCode || COMPILATION_REQUESTED == returnCode)
      << resolvedMethodName << " is " << returnCode << ", expected is 0 or " << COMPILATION_IL_GEN_FAILURE;
   }

TR::ResolvedMethod *
OpCodesTest::resolvedMethod(TR::DataType dataType)
   {
   switch (dataType)
      {
      case TR::Int32:
         return _int32Compilee;
      case TR::Int64:
         return _int64Compilee;
      case TR::Double:
         return _doubleCompilee;
      case TR::Float:
         return _floatCompilee;
      case TR::Address:
         return _addressCompilee;
      case TR::NoType:
      default:
         TR_ASSERT(0, "compilee dataType should be int32, in64, double, float or address");
         return NULL;
      }
   }

void
OpCodesTest::compileIntegerArithmeticTestMethods()
   {
   int32_t rc = 0;
   compileOpCodeMethod(_iAdd, _numberOfBinaryArgs, TR::iadd, "iAdd", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iSub, _numberOfBinaryArgs, TR::isub, "iSub", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iDiv, _numberOfBinaryArgs, TR::idiv, "iDiv", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iMul, _numberOfBinaryArgs, TR::imul, "iMul", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iMulh, _numberOfBinaryArgs, TR::imulh, "iMulh", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iRem, _numberOfBinaryArgs, TR::irem, "iRem", _argTypesBinaryInt, TR::Int32, rc);
   }

void
OpCodesTest::compileMemoryOperationTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_iLoad, _numberOfUnaryArgs, TR::iload, "iLoad", _argTypesUnaryInt, TR::Int32, rc);
   compileOpCodeMethod(_lLoad, _numberOfUnaryArgs, TR::lload, "lLoad", _argTypesUnaryLong, TR::Int64, rc);
   compileOpCodeMethod(_dLoad, _numberOfUnaryArgs, TR::dload, "dLoad", _argTypesUnaryDouble, TR::Double, rc);
   compileOpCodeMethod(_fLoad, _numberOfUnaryArgs, TR::fload, "fLoad", _argTypesUnaryFloat, TR::Float, rc);

   compileOpCodeMethod(_iStore, _numberOfUnaryArgs, TR::istore, "iStore", _argTypesUnaryInt, TR::Int32, rc);

   compileOpCodeMethod(_iLoadi, _numberOfUnaryArgs, TR::iloadi, "iLoadi", _argTypesUnaryAddress, TR::Int32, rc);
   compileOpCodeMethod(_lLoadi, _numberOfUnaryArgs, TR::lloadi, "lLoadi", _argTypesUnaryAddress, TR::Int64, rc);
   compileOpCodeMethod(_dLoadi, _numberOfUnaryArgs, TR::dloadi, "dLoadi", _argTypesUnaryAddress, TR::Double, rc);
   compileOpCodeMethod(_fLoadi, _numberOfUnaryArgs, TR::floadi, "fLoadi", _argTypesUnaryAddress, TR::Float, rc);
   compileOpCodeMethod(_bLoadi, _numberOfUnaryArgs, TR::bloadi, "bLoadi", _argTypesUnaryAddress, TR::Int8, rc);
   compileOpCodeMethod(_sLoadi, _numberOfUnaryArgs, TR::sloadi, "sLoadi", _argTypesUnaryAddress, TR::Int16, rc);
   compileOpCodeMethod(_aLoadi, _numberOfUnaryArgs, TR::aloadi, "aLoadi", _argTypesUnaryAddress, TR::Address, rc);

   }

void
OpCodesTest::compileUnaryTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_iNeg, _numberOfUnaryArgs, TR::ineg, "iNeg", _argTypesUnaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iAbs, _numberOfUnaryArgs, TR::iabs, "iAbs", _argTypesUnaryInt, TR::Int32, rc);

   compileOpCodeMethod(_iReturn, _numberOfUnaryArgs, TR::ireturn, "iReturn", _argTypesUnaryInt, TR::Int32, rc);

   compileOpCodeMethod(_i2l, _numberOfUnaryArgs, TR::i2l, "i2l", _argTypesUnaryInt, TR::Int64, rc);
   compileOpCodeMethod(_i2b, _numberOfUnaryArgs, TR::i2b, "i2b", _argTypesUnaryInt, TR::Int8, rc);
   compileOpCodeMethod(_i2s, _numberOfUnaryArgs, TR::i2s, "i2s", _argTypesUnaryInt, TR::Int16, rc);

   compileOpCodeMethod(_l2i, _numberOfUnaryArgs, TR::l2i, "l2i", _argTypesUnaryLong, TR::Int32, rc);
   compileOpCodeMethod(_l2b, _numberOfUnaryArgs, TR::l2b, "l2b", _argTypesUnaryLong, TR::Int8, rc);
   compileOpCodeMethod(_l2s, _numberOfUnaryArgs, TR::l2s, "l2s", _argTypesUnaryLong, TR::Int16, rc);

   compileOpCodeMethod(_f2i, _numberOfUnaryArgs, TR::f2i, "f2i", _argTypesUnaryFloat, TR::Int32, rc);
   compileOpCodeMethod(_d2i, _numberOfUnaryArgs, TR::d2i, "d2i", _argTypesUnaryDouble, TR::Int32, rc);

   }

void
OpCodesTest::compileBitwiseMethods()
   {
   int32_t rc;

   compileOpCodeMethod(_iAnd, _numberOfBinaryArgs, TR::iand, "iAnd", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iOr, _numberOfBinaryArgs, TR::ior, "iOr", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iXor, _numberOfBinaryArgs, TR::ixor, "iXor", _argTypesBinaryInt, TR::Int32, rc);
   }

void
OpCodesTest::compileCompareTestMethods()
   {
   int32_t rc = 0;

   //Compare
   compileOpCodeMethod(_iCmpeq, _numberOfBinaryArgs, TR::icmpeq, "iCmpeq", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iCmpne, _numberOfBinaryArgs, TR::icmpne, "iCmpne", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iCmpgt, _numberOfBinaryArgs, TR::icmpgt, "iCmpgt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iCmplt, _numberOfBinaryArgs, TR::icmplt, "iCmplt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iCmpge, _numberOfBinaryArgs, TR::icmpge, "iCmpge", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iCmple, _numberOfBinaryArgs, TR::icmple, "iCmple", _argTypesBinaryInt, TR::Int32, rc);

   compileOpCodeMethod(_lCmpne, _numberOfBinaryArgs, TR::lcmpne, "lCmpne", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_lCmpgt, _numberOfBinaryArgs, TR::lcmpgt, "lCmpgt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_lCmpge, _numberOfBinaryArgs, TR::lcmpge, "lCmpge", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_lCmple, _numberOfBinaryArgs, TR::lcmple, "lCmple", _argTypesBinaryLong, TR::Int32, rc);

   compileOpCodeMethod(_iuCmplt, _numberOfBinaryArgs, TR::iucmplt, "iuCmplt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iuCmpgt, _numberOfBinaryArgs, TR::iucmpgt, "iuCmpgt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_iuCmple, _numberOfBinaryArgs, TR::iucmple, "iuCmple", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_luCmplt, _numberOfBinaryArgs, TR::lucmplt, "luCmplt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_luCmpge, _numberOfBinaryArgs, TR::lucmpge, "luCmpge", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_luCmpgt, _numberOfBinaryArgs, TR::lucmpgt, "luCmpgt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_luCmple, _numberOfBinaryArgs, TR::lucmple, "luCmple", _argTypesBinaryLong, TR::Int32, rc);

   //CompareAndBranch
   compileOpCodeMethod(_ifIcmpeq, _numberOfBinaryArgs, TR::ificmpeq, "ifIcmpeq", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIcmpne, _numberOfBinaryArgs, TR::ificmpne, "ifIcmpne", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIcmpgt, _numberOfBinaryArgs, TR::ificmpgt, "ifIcmpgt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIcmplt, _numberOfBinaryArgs, TR::ificmplt, "ifIcmplt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIcmpge, _numberOfBinaryArgs, TR::ificmpge, "ifIcmpge", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIcmple, _numberOfBinaryArgs, TR::ificmple, "ifIcmple", _argTypesBinaryInt, TR::Int32, rc);

   compileOpCodeMethod(_ifLcmpne, _numberOfBinaryArgs, TR::iflcmpne, "ifLcmpne", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLcmpge, _numberOfBinaryArgs, TR::iflcmpge, "ifLcmpge", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLcmple, _numberOfBinaryArgs, TR::iflcmple, "ifLcmple", _argTypesBinaryLong, TR::Int32, rc);

   compileOpCodeMethod(_ifIuCmplt, _numberOfBinaryArgs, TR::ifiucmplt, "ifIuCmplt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIuCmpge, _numberOfBinaryArgs, TR::ifiucmpge, "ifIuCmpge", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIuCmpgt, _numberOfBinaryArgs, TR::ifiucmpgt, "ifIuCmpgt", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifIuCmple, _numberOfBinaryArgs, TR::ifiucmple, "ifIuCmple", _argTypesBinaryInt, TR::Int32, rc);
   compileOpCodeMethod(_ifLuCmplt, _numberOfBinaryArgs, TR::iflucmplt, "ifLuCmplt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLuCmpge, _numberOfBinaryArgs, TR::iflucmpge, "ifLuCmpge", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLuCmpgt, _numberOfBinaryArgs, TR::iflucmpgt, "ifLuCmpgt", _argTypesBinaryLong, TR::Int32, rc);
   compileOpCodeMethod(_ifLuCmple, _numberOfBinaryArgs, TR::iflucmple, "ifLuCmple", _argTypesBinaryLong, TR::Int32, rc);
   }

void
OpCodesTest::compileSelectTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_iselect, _numberOfSelectArgs, TR::iselect, "iSelect", _argTypesSelectInt, TR::Int32, rc);
   }

void
OpCodesTest::compileAddressTestMethods()
   {
   int32_t rc = 0;

   compileOpCodeMethod(_aload, _numberOfUnaryArgs, TR::aload, "aload", _argTypesUnaryAddress, TR::Address, rc);
   compileOpCodeMethod(_astore, _numberOfUnaryArgs, TR::astore, "astore", _argTypesUnaryAddress, TR::Address, rc);
   compileOpCodeMethod(_areturn, _numberOfUnaryArgs, TR::areturn, "areturn", _argTypesUnaryAddress, TR::Address, rc);
   compileOpCodeMethod(_a2i, _numberOfUnaryArgs, TR::a2i, "a2i", _argTypesUnaryAddress, TR::Int32, rc);

   }

void
OpCodesTest::compileDisabledOpCodesTests()
   {
   int32_t rc = 0;
   //Jazz103 Work item 110364
   compileOpCodeMethod(_fRem, _numberOfBinaryArgs, TR::frem, "fRem", _argTypesBinaryFloat, TR::Float, rc);
   compileOpCodeMethod(_dRem, _numberOfBinaryArgs, TR::drem, "dRem", _argTypesBinaryDouble, TR::Double, rc);
   }

void
OpCodesTest::invokeTests()
   {

   }

void
OpCodesTest::invokeIntegerArithmeticTests()
   {
   //TR::Int32
   int32_t rc = 0;
   uint32_t testCaseArrLength = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharII_I_testMethodType * iBinaryCons = 0;
   signatureCharJJ_J_testMethodType * lBinaryCons = 0;

   int32_t intAddArr[][2] =
      {
      INT_ZERO, INT_ZERO,
      INT_NEG, INT_NEG,
      INT_MINIMUM, INT_POS,
      INT_MAXIMUM, INT_MAXIMUM,
      INT_POS, INT_MINIMUM
      };
   int32_t intSubArr[][2] =
      {
      INT_MAXIMUM, INT_MINIMUM,
      INT_NEG, INT_POS,
      INT_POS, INT_MAXIMUM,
      INT_ZERO, INT_NEG,
      INT_MINIMUM, INT_ZERO
      };
   int32_t intMulArr[][2] =
      {
      INT_NEG, INT_MINIMUM,
      INT_ZERO, INT_POS,
      INT_MINIMUM, INT_NEG,
      INT_POS, INT_ZERO,
      INT_MAXIMUM, INT_MINIMUM,
      INT_MINIMUM, INT_MAXIMUM
      };
   int32_t intDivArr[][2] =
      {
      INT_NEG, INT_MAXIMUM,
      INT_POS, INT_POS,
      INT_MAXIMUM, INT_NEG,
      INT_ZERO, INT_MINIMUM
      };
   int32_t intRemArr[][2] =
      {
      INT_MINIMUM, INT_MINIMUM,
      INT_ZERO, INT_MAXIMUM,
      INT_POS, INT_NEG,
      INT_MAXIMUM, INT_POS
      };

   //iadd
   testCaseArrLength = sizeof(intAddArr) / sizeof(intAddArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iAdd, add(intAddArr[i][0], intAddArr[i][1]), _iAdd(intAddArr[i][0], intAddArr[i][1]));

      sprintf(resolvedMethodName, "iAddConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::iadd,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intAddArr[i][0], 2, &intAddArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, add(intAddArr[i][0], intAddArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iAddConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::iadd,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intAddArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, add(intAddArr[i][0], intAddArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intAddArr[i][1]));

      sprintf(resolvedMethodName, "iAddConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::iadd,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intAddArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, add(intAddArr[i][0], intAddArr[i][1]), iBinaryCons(intAddArr[i][0], INT_PLACEHOLDER_2));
      }

   //isub
   testCaseArrLength = sizeof(intSubArr) / sizeof(intSubArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iSub, sub(intSubArr[i][0], intSubArr[i][1]), _iSub(intSubArr[i][0], intSubArr[i][1]));

      sprintf(resolvedMethodName, "iSubConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::isub,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intSubArr[i][0], 2, &intSubArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, sub(intSubArr[i][0], intSubArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iSubConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::isub,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intSubArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, sub(intSubArr[i][0], intSubArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intSubArr[i][1]));

      sprintf(resolvedMethodName, "iSubConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::isub,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intSubArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, sub(intSubArr[i][0], intSubArr[i][1]), iBinaryCons(intSubArr[i][0], INT_PLACEHOLDER_2));
      }

   //imul
   testCaseArrLength = sizeof(intMulArr) / sizeof(intMulArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iMul, mul(intMulArr[i][0], intMulArr[i][1]), _iMul(intMulArr[i][0], intMulArr[i][1]));

      sprintf(resolvedMethodName, "iMulConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imul,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intMulArr[i][0], 2, &intMulArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, mul(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iMulConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imul,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intMulArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, mul(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intMulArr[i][1]));

      sprintf(resolvedMethodName, "iMulConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imul,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intMulArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, mul(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(intMulArr[i][0], INT_PLACEHOLDER_2));
      }

   //imulh
   testCaseArrLength = sizeof(intMulArr) / sizeof(intMulArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iMulh, imulh(intMulArr[i][0], intMulArr[i][1]), _iMulh(intMulArr[i][0], intMulArr[i][1]));

      sprintf(resolvedMethodName, "iMulhConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imulh,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intMulArr[i][0], 2, &intMulArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, imulh(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iMulhConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imulh,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intMulArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, imulh(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intMulArr[i][1]));

      sprintf(resolvedMethodName, "iMulhConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::imulh,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intMulArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, imulh(intMulArr[i][0], intMulArr[i][1]), iBinaryCons(intMulArr[i][0], INT_PLACEHOLDER_2));
      }

   //idiv
   //TODO: Use ASSERT_DEATH() to catch Divide by zero which will get "Floating point exception (core dumped)"
   //Test secnario : _iDiv(INT_MIN, 0)
   testCaseArrLength = sizeof(intDivArr) / sizeof(intDivArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iDiv, div(intDivArr[i][0], intDivArr[i][1]), _iDiv(intDivArr[i][0], intDivArr[i][1]));

      sprintf(resolvedMethodName, "iDivConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::idiv,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intDivArr[i][0], 2, &intDivArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, div(intDivArr[i][0], intDivArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iDivConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::idiv,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intDivArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, div(intDivArr[i][0], intDivArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intDivArr[i][1]));

      sprintf(resolvedMethodName, "iDivConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::idiv,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intDivArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, div(intDivArr[i][0], intDivArr[i][1]), iBinaryCons(intDivArr[i][0], INT_PLACEHOLDER_2));
      }

   //irem
   //TODO: Use ASSERT_DEATH() to catch Remainder by zero which will get "Floating point exception (core dumped)"
   //Test secnario : _iRem(TR::getMaxSigned<TR::Int32>(), 0) , _Rem(-9, 0)
   testCaseArrLength = sizeof(intRemArr) / sizeof(intRemArr[0]);
   for(uint32_t i = 0; i < testCaseArrLength; ++i)
      {
      OMR_CT_EXPECT_EQ(_iRem, rem(intRemArr[i][0], intRemArr[i][1]), _iRem(intRemArr[i][0], intRemArr[i][1]));

      sprintf(resolvedMethodName, "iRemConst1_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::irem,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &intRemArr[i][0], 2, &intRemArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, rem(intRemArr[i][0], intRemArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iRemConst2_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::irem,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &intRemArr[i][0]);
      OMR_CT_EXPECT_EQ(iBinaryCons, rem(intRemArr[i][0], intRemArr[i][1]), iBinaryCons(INT_PLACEHOLDER_1, intRemArr[i][1]));

      sprintf(resolvedMethodName, "iRemConst3_Testcase%d", i);
      compileOpCodeMethod(iBinaryCons, _numberOfBinaryArgs, TR::irem,
            resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &intRemArr[i][1]);
      OMR_CT_EXPECT_EQ(iBinaryCons, rem(intRemArr[i][0], intRemArr[i][1]), iBinaryCons(intRemArr[i][0], INT_PLACEHOLDER_2));
      }
   }

void
OpCodesTest::invokeMemoryOperationTests()
   {
   //iload
   OMR_CT_EXPECT_EQ(_iLoad, INT_ZERO, _iLoad(INT_ZERO));
   OMR_CT_EXPECT_EQ(_iLoad, INT_NEG, _iLoad(INT_NEG));
   OMR_CT_EXPECT_EQ(_iLoad, INT_POS, _iLoad(INT_POS));
   OMR_CT_EXPECT_EQ(_iLoad, INT_MAXIMUM, _iLoad(INT_MAXIMUM));
   OMR_CT_EXPECT_EQ(_iLoad, INT_MINIMUM, _iLoad(INT_MINIMUM));

   OMR_CT_EXPECT_EQ(_lLoad, LONG_ZERO, _lLoad(LONG_ZERO));
   OMR_CT_EXPECT_EQ(_lLoad, LONG_NEG, _lLoad(LONG_NEG));
   OMR_CT_EXPECT_EQ(_lLoad, LONG_POS, _lLoad(LONG_POS));
   OMR_CT_EXPECT_EQ(_lLoad, LONG_MAXIMUM, _lLoad(LONG_MAXIMUM));
   OMR_CT_EXPECT_EQ(_lLoad, LONG_MINIMUM, _lLoad(LONG_MINIMUM));

   OMR_CT_EXPECT_DOUBLE_EQ(_dLoad, DOUBLE_ZERO, _dLoad(DOUBLE_ZERO));
   OMR_CT_EXPECT_DOUBLE_EQ(_dLoad, DOUBLE_NEG, _dLoad(DOUBLE_NEG));
   OMR_CT_EXPECT_DOUBLE_EQ(_dLoad, DOUBLE_POS, _dLoad(DOUBLE_POS));
   OMR_CT_EXPECT_DOUBLE_EQ(_dLoad, DOUBLE_MAXIMUM, _dLoad(DOUBLE_MAXIMUM));
   OMR_CT_EXPECT_DOUBLE_EQ(_dLoad, DOUBLE_MINIMUM, _dLoad(DOUBLE_MINIMUM));

   OMR_CT_EXPECT_FLOAT_EQ(_fLoad, FLOAT_ZERO, _fLoad(FLOAT_ZERO));
   OMR_CT_EXPECT_FLOAT_EQ(_fLoad, FLOAT_NEG, _fLoad(FLOAT_NEG));
   OMR_CT_EXPECT_FLOAT_EQ(_fLoad, FLOAT_POS, _fLoad(FLOAT_POS));
   OMR_CT_EXPECT_FLOAT_EQ(_fLoad, FLOAT_MAXIMUM, _fLoad(FLOAT_MAXIMUM));
   OMR_CT_EXPECT_FLOAT_EQ(_fLoad, FLOAT_MINIMUM, _fLoad(FLOAT_MINIMUM));

   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_MAXIMUM, FLOAT_MINIMUM, FLOAT_ZERO};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_MAXIMUM, DOUBLE_MINIMUM, DOUBLE_ZERO};
   uintptr_t addressDataArray[] = {(uintptr_t)&INT_NEG, (uintptr_t)&LONG_POS, (uintptr_t)&BYTE_MAXIMUM, (uintptr_t)&SHORT_MINIMUM, (uintptr_t)&FLOAT_ZERO};

   int32_t rc = 0;
   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharI_I_testMethodType  *iMemCons = 0;

   //istore
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "iStoreConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_iStore, intDataArray[i], _iStore(intDataArray[i]));
compileOpCodeMethod(      iMemCons, _numberOfUnaryArgs, TR::istore, resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &(intDataArray[i]));
      OMR_CT_EXPECT_EQ(iMemCons, intDataArray[i], iMemCons(INT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_iLoadi, intDataArray[i], _iLoadi((uintptr_t)(&intDataArray[i])));
      }

   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_sLoadi, shortDataArray[i], _sLoadi((uintptr_t)(&shortDataArray[i])));
      }

   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_bLoadi, byteDataArray[i], _bLoadi((uintptr_t)(&byteDataArray[i])));
      }

   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_lLoadi, longDataArray[i], _lLoadi((uintptr_t)(&longDataArray[i])));
      }

   testCaseNum = sizeof(doubleDataArray) / sizeof(doubleDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_dLoadi, doubleDataArray[i], _dLoadi((uintptr_t)(&doubleDataArray[i])));
      }

   testCaseNum = sizeof(floatDataArray) / sizeof(floatDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_fLoadi, floatDataArray[i], _fLoadi((uintptr_t)(&floatDataArray[i])));
      }

   testCaseNum = sizeof(addressDataArray) / sizeof(addressDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_aLoadi, addressDataArray[i], _aLoadi((uintptr_t)(&addressDataArray[i])));
      }
   }

void
OpCodesTest::invokeUnaryTests()
   {
   int64_t longDataArray[] = {LONG_NEG, LONG_POS, LONG_MAXIMUM, LONG_MINIMUM, LONG_ZERO};
   int32_t intDataArray[] = {INT_NEG, INT_POS, INT_MAXIMUM, INT_MINIMUM, INT_ZERO};
   float floatDataArray[] = {FLOAT_NEG, FLOAT_POS, FLOAT_ZERO, FLOAT_MAXIMUM, FLOAT_MINIMUM};
   double doubleDataArray[] = {DOUBLE_NEG, DOUBLE_POS, DOUBLE_ZERO, DOUBLE_MAXIMUM, DOUBLE_MINIMUM};
   uint64_t ulongDataArray[] = {ULONG_POS, ULONG_MAXIMUM, ULONG_MINIMUM};
   uint32_t uintDataArray[] = {UINT_POS, UINT_MAXIMUM, UINT_MINIMUM};

   int16_t shortDataArray[] = {SHORT_NEG, SHORT_POS, SHORT_MAXIMUM, SHORT_MINIMUM, SHORT_ZERO};
   int8_t byteDataArray[] = {BYTE_NEG, BYTE_POS, BYTE_MAXIMUM, BYTE_MINIMUM, BYTE_ZERO};

   uint16_t ushortDataArray[] = {USHORT_POS, USHORT_MAXIMUM, USHORT_MINIMUM};
   uint8_t ubyteDataArray[] = {UBYTE_POS, UBYTE_MAXIMUM, UBYTE_MINIMUM};

   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   int32_t rc = 0;
   signatureCharI_I_testMethodType  *iUnaryCons = 0;
   signatureCharS_S_testMethodType  *sUnaryCons = 0;
   signatureCharB_B_testMethodType  *bUnaryCons = 0;

   signatureCharI_J_testMethodType * i2lConst = 0;
   signatureCharI_B_testMethodType * i2bConst = 0;
   signatureCharI_S_testMethodType * i2sConst = 0;
   unsignedSignatureCharI_F_testMethodType * iu2fConst = 0;
   unsignedSignatureCharI_D_testMethodType * iu2dConst = 0;

   signatureCharJ_I_testMethodType * l2iConst = 0;
   signatureCharJ_B_testMethodType * l2bConst = 0;
   signatureCharJ_S_testMethodType * l2sConst = 0;
   unsignedSignatureCharJ_F_testMethodType * lu2fConst = 0;
   unsignedSignatureCharJ_D_testMethodType * lu2dConst = 0;

   signatureCharD_I_testMethodType * d2iConst = 0;
   signatureCharF_I_testMethodType * f2iConst = 0;

   //ineg
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iNeg, neg(intDataArray[i]), _iNeg(intDataArray[i]));
      sprintf(resolvedMethodName, "iNegConst%d", i + 1);
      compileOpCodeMethod(      iUnaryCons, _numberOfUnaryArgs, TR::ineg,
            resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(iUnaryCons, neg(intDataArray[i]), iUnaryCons(INT_PLACEHOLDER_1));
      }

   //iabs
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iAbs, abs(intDataArray[i]), _iAbs(intDataArray[i]));
      sprintf(resolvedMethodName, "iAbsConst%d", i + 1);
      compileOpCodeMethod(      iUnaryCons, _numberOfUnaryArgs, TR::iabs,
            resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(iUnaryCons, abs(intDataArray[i]), iUnaryCons(INT_PLACEHOLDER_1));
      }

   //return group
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "iReturnCons%d", i + 1);
      OMR_CT_EXPECT_EQ(_iReturn, intDataArray[i], _iReturn(intDataArray[i]));
      compileOpCodeMethod(      iUnaryCons, _numberOfUnaryArgs, TR::ireturn, resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &(intDataArray[i]));
      OMR_CT_EXPECT_EQ(iUnaryCons, intDataArray[i], iUnaryCons(INT_PLACEHOLDER_1));
      }

   //const
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "iConst%d", i + 1);
      compileOpCodeMethod(      iUnaryCons, _numberOfUnaryArgs, TR::iconst, resolvedMethodName, _argTypesUnaryInt, TR::Int32, rc, 2, 1, &(intDataArray[i]));
      OMR_CT_EXPECT_EQ(iUnaryCons, intDataArray[i], iUnaryCons(INT_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(byteDataArray) / sizeof(byteDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "bConst%d", i + 1);
      compileOpCodeMethod(      bUnaryCons, _numberOfUnaryArgs, TR::bconst, resolvedMethodName, _argTypesUnaryByte, TR::Int8, rc, 2, 1, &(byteDataArray[i]));
      OMR_CT_EXPECT_EQ(bUnaryCons, byteDataArray[i], bUnaryCons(BYTE_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(shortDataArray) / sizeof(shortDataArray[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "sConst%d", i + 1);
      compileOpCodeMethod(      sUnaryCons, _numberOfUnaryArgs, TR::sconst, resolvedMethodName, _argTypesUnaryShort, TR::Int16, rc, 2, 1, &(shortDataArray[i]));
      OMR_CT_EXPECT_EQ(sUnaryCons, shortDataArray[i], sUnaryCons(SHORT_PLACEHOLDER_1));
      }

   //int 2 l,b,s
   testCaseNum = sizeof(intDataArray) / sizeof(intDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_i2b, convert(intDataArray[i], BYTE_POS), _i2b(intDataArray[i]));
      OMR_CT_EXPECT_EQ(_i2s, convert(intDataArray[i], SHORT_POS), _i2s(intDataArray[i]));
      OMR_CT_EXPECT_EQ(_i2l, convert(intDataArray[i], LONG_POS), _i2l(intDataArray[i]));

      sprintf(resolvedMethodName, "i2bConst%d", i + 1);
      compileOpCodeMethod(      i2bConst, _numberOfUnaryArgs, TR::i2b,
            resolvedMethodName, _argTypesUnaryInt, TR::Int8, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(i2bConst, convert(intDataArray[i], BYTE_POS), i2bConst(INT_PLACEHOLDER_1));


      sprintf(resolvedMethodName, "i2sConst%d", i + 1);
compileOpCodeMethod(      i2sConst, _numberOfUnaryArgs, TR::i2s,
            resolvedMethodName, _argTypesUnaryInt, TR::Int16, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(i2sConst, convert(intDataArray[i], SHORT_POS), i2sConst(INT_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "i2lConst%d", i + 1);
compileOpCodeMethod(      i2lConst, _numberOfUnaryArgs, TR::i2l,
            resolvedMethodName, _argTypesUnaryInt, TR::Int64, rc, 2, 1, &intDataArray[i]);
      OMR_CT_EXPECT_EQ(i2lConst, convert(intDataArray[i], LONG_POS), i2lConst(INT_PLACEHOLDER_1));
      }

   //l 2 i,b,s
   testCaseNum = sizeof(longDataArray) / sizeof(longDataArray[0]);
   for (uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_l2b, convert(longDataArray[i], BYTE_POS), _l2b(longDataArray[i]));
      OMR_CT_EXPECT_EQ(_l2s, convert(longDataArray[i], SHORT_POS), _l2s(longDataArray[i]));
      OMR_CT_EXPECT_EQ(_l2i, convert(longDataArray[i], INT_POS), _l2i(longDataArray[i]));

      sprintf(resolvedMethodName, "l2bConst%d", i + 1);
compileOpCodeMethod(      l2bConst, _numberOfUnaryArgs, TR::l2b,
            resolvedMethodName, _argTypesUnaryLong, TR::Int8, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(l2bConst, convert(longDataArray[i], BYTE_POS), l2bConst(LONG_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "l2sConst%d", i + 1);
compileOpCodeMethod(      l2sConst, _numberOfUnaryArgs, TR::l2s,
            resolvedMethodName, _argTypesUnaryLong, TR::Int16, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(l2sConst, convert(longDataArray[i], SHORT_POS), l2sConst(LONG_PLACEHOLDER_1));

      sprintf(resolvedMethodName, "l2iConst%d", i + 1);
compileOpCodeMethod(      l2iConst, _numberOfUnaryArgs, TR::l2i,
            resolvedMethodName, _argTypesUnaryLong, TR::Int32, rc, 2, 1, &longDataArray[i]);
      OMR_CT_EXPECT_EQ(l2iConst, convert(longDataArray[i], INT_POS), l2iConst(LONG_PLACEHOLDER_1));
      }

   //f2i
   //Temporarily postpone converting FLOAT_MAXIMUM and FLOAT_MINIMUM to int and long.
   //the behavior is undefined since the truncated value of the min and max numbers
   //cannot be represented in the destination types. This operation is lack of helper,
   //which will lead to Segmentation fault, use i < 3 temporarily.
   //When the helper for f2i MAXIMUM and MINIMUM is done, this part should be moved
   //back to the loop above.
   for (uint32_t i = 0; i < 3; ++i)
      {
      OMR_CT_EXPECT_EQ(_f2i, convert(floatDataArray[i], INT_POS), _f2i(floatDataArray[i]));

      sprintf(resolvedMethodName, "f2iConst%d", i + 1);
      compileOpCodeMethod(      f2iConst, _numberOfUnaryArgs, TR::f2i,
                                resolvedMethodName, _argTypesUnaryFloat, TR::Int32, rc, 2, 1, &floatDataArray[i]);
      OMR_CT_EXPECT_EQ(f2iConst, convert(floatDataArray[i], INT_POS), f2iConst(FLOAT_PLACEHOLDER_1));
      }

   //d2i
   //Temporarily postpone converting DOUBLE_MAXIMUM and DOUBLE_MINIMUM to int and long.
   //the behavior is undefined since the truncated value of the min and max numbers
   //cannot be represented in the destination types. This operation is lack of helper,
   //which will lead to Segmentation fault, use i < 3 temporarily
   //When the helper for d2i MAXIMUM and MINIMUM is done, this part should be moved
   //back to the loop above.
   for (uint32_t i = 0; i < 3; ++i)
      {
      OMR_CT_EXPECT_EQ(_d2i, convert(doubleDataArray[i], INT_POS), _d2i(doubleDataArray[i]));

      sprintf(resolvedMethodName, "d2iConst%d", i + 1);
      compileOpCodeMethod(      d2iConst, _numberOfUnaryArgs, TR::d2i,
                                resolvedMethodName, _argTypesUnaryDouble, TR::Int32, rc, 2, 1, &doubleDataArray[i]);
      OMR_CT_EXPECT_EQ(d2iConst, convert(doubleDataArray[i], INT_POS), d2iConst(DOUBLE_PLACEHOLDER_1));
      }

   }

//This function is used to test the JIT helper for converting MAXIMUM and MINIMUM from
//float or double to int or long. When JIT helper can handle these conversion properly,
//these tests will be moved back to incokeUnaryTest() function. This function is
//disabled by default. To enable this function, uncomment in google test framework.
void
OpCodesTest::invokeNoHelperUnaryTests()
   {
   int32_t rc = 0;

   compileOpCodeMethod(   _f2i, _numberOfUnaryArgs, TR::f2i, "f2i", _argTypesUnaryFloat, TR::Int32, rc);
   compileOpCodeMethod(   _d2i, _numberOfUnaryArgs, TR::d2i, "d2i", _argTypesUnaryDouble, TR::Int32, rc);

   _f2i(FLOAT_MAXIMUM);
   _f2i(FLOAT_MINIMUM);
   _d2i(DOUBLE_MAXIMUM);
   _d2i(DOUBLE_MINIMUM);
   }

void
OpCodesTest::invokeBitwiseTests()
   {
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNum = 0;

   int32_t intAndArr [][2] =
      {
      INT_ZERO, INT_NEG,
      INT_POS, INT_MAXIMUM,
      INT_NEG, INT_MINIMUM,
      INT_MAXIMUM, INT_POS,
      INT_MAXIMUM, INT_MINIMUM,
      INT_POS, INT_POS,
      INT_NEG, INT_MAXIMUM
      };
   int32_t intOrArr [][2] =
      {
      INT_MINIMUM, INT_POS
      };
   int32_t intXorArr [][2] =
      {
      INT_MAXIMUM, INT_ZERO,
      INT_MINIMUM, INT_MINIMUM
      };

   signatureCharII_I_testMethodType * iBitwiseConst = 0;

   //iand
   testCaseNum = sizeof(intAndArr) / sizeof(intAndArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_iAnd, tand(intAndArr[i][0], intAndArr[i][1]), _iAnd(intAndArr[i][0], intAndArr[i][1]));

      sprintf(resolvedMethodName, "iAndConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::iand, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(intAndArr[i][0]), 2, &(intAndArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tand(intAndArr[i][0], intAndArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iAndConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::iand, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(intAndArr[i][0]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tand(intAndArr[i][0], intAndArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, intAndArr[i][1]));

      sprintf(resolvedMethodName, "iAndConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::iand, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(intAndArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tand(intAndArr[i][0], intAndArr[i][1]), iBitwiseConst(intAndArr[i][0], INT_PLACEHOLDER_2));
     }

   //ior
   testCaseNum = sizeof(intOrArr) / sizeof(intOrArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_iOr, tor(intOrArr[i][0], intOrArr[i][1]), _iOr(intOrArr[i][0], intOrArr[i][1]));

      sprintf(resolvedMethodName, "iOrConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ior, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(intOrArr[i][0]), 2, &(intOrArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tor(intOrArr[i][0], intOrArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iOrConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ior, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(intOrArr[i][0]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tor(intOrArr[i][0], intOrArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, intOrArr[i][1]));

      sprintf(resolvedMethodName, "iOrConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ior, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(intOrArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, tor(intOrArr[i][0], intOrArr[i][1]), iBitwiseConst(intOrArr[i][0], INT_PLACEHOLDER_2));
     }

   //ixor
   testCaseNum = sizeof(intXorArr) / sizeof(intXorArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_iXor, txor(intXorArr[i][0], intXorArr[i][1]), _iXor(intXorArr[i][0], intXorArr[i][1]));

      sprintf(resolvedMethodName, "iXorConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ixor, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(intXorArr[i][0]), 2, &(intXorArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, txor(intXorArr[i][0], intXorArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iXorConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ixor, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(intXorArr[i][0]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, txor(intXorArr[i][0], intXorArr[i][1]), iBitwiseConst(INT_PLACEHOLDER_1, intXorArr[i][1]));

      sprintf(resolvedMethodName, "iXorConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iBitwiseConst,
            _numberOfBinaryArgs, TR::ixor, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(intXorArr[i][1]));
      OMR_CT_EXPECT_EQ(iBitwiseConst, txor(intXorArr[i][0], intXorArr[i][1]), iBitwiseConst(intXorArr[i][0], INT_PLACEHOLDER_2));
     }
   }

void
OpCodesTest::invokeDisabledOpCodesTests()
   {

   //frem
   OMR_CT_EXPECT_FLOAT_EQ(_fRem, remainderf(FLOAT_MINIMUM, FLOAT_MINIMUM), _fRem(FLOAT_MINIMUM, FLOAT_MINIMUM));
   OMR_CT_EXPECT_FLOAT_EQ(_fRem, remainderf(FLOAT_ZERO, FLOAT_MAXIMUM), _fRem(FLOAT_ZERO, FLOAT_MAXIMUM));
   OMR_CT_EXPECT_FLOAT_EQ(_fRem, remainderf(FLOAT_POS, FLOAT_NEG), _fRem(FLOAT_POS, FLOAT_NEG));
   OMR_CT_EXPECT_FLOAT_EQ(_fRem, remainderf(FLOAT_MAXIMUM, FLOAT_POS), _fRem(FLOAT_MAXIMUM, FLOAT_POS));


   //drem
   OMR_CT_EXPECT_DOUBLE_EQ(_dRem, remainder(DOUBLE_MINIMUM, DOUBLE_MINIMUM), _dRem(DOUBLE_MINIMUM, DOUBLE_MINIMUM));
   OMR_CT_EXPECT_DOUBLE_EQ(_dRem, remainder(DOUBLE_ZERO, DOUBLE_MAXIMUM), _dRem(DOUBLE_ZERO, DOUBLE_MAXIMUM));
   OMR_CT_EXPECT_DOUBLE_EQ(_dRem, remainder(DOUBLE_POS, DOUBLE_NEG), _dRem(DOUBLE_POS, DOUBLE_NEG));
   OMR_CT_EXPECT_DOUBLE_EQ(_dRem, remainder(DOUBLE_MAXIMUM, DOUBLE_POS), _dRem(DOUBLE_MAXIMUM, DOUBLE_POS));
   }

void
OpCodesTest::UnsupportedOpCodesTests()
   {
   //bdiv, brem
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bdiv, "bDiv", _argTypesBinaryByte, TR::Int8);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::brem, "bRem", _argTypesBinaryByte, TR::Int8);

   //sdiv, srem
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::sdiv, "sDiv", _argTypesBinaryShort, TR::Int16);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::srem, "sRem", _argTypesBinaryShort, TR::Int16);

   //bucmplt, bucmple, bucmpgt, bucmpge
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmplt, "buCmplt", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmpge, "buCmpge", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmpgt, "buCmpgt", _argTypesBinaryByte, TR::Int32);
   addUnsupportedOpCodeTest(_numberOfBinaryArgs, TR::bucmple, "buCmple", _argTypesBinaryByte, TR::Int32);
   }

void
OpCodesTest::invokeCompareTests()
   {

   //Compare op codes data array
   int32_t iCmpeqDataArr[][2] =
         {
         INT_MINIMUM, INT_MAXIMUM,
         INT_POS, INT_POS
         };
   int32_t iCmpneDataArr[][2] =
         {
         INT_MAXIMUM, INT_NEG,
         INT_NEG, INT_NEG
         };
   int32_t iCmpgtDataArr[][2] =
         {
         INT_NEG, INT_MINIMUM,
         INT_MINIMUM, INT_NEG
         };
   int32_t iCmpltDataArr[][2] =
         {
         INT_MINIMUM, INT_ZERO,
         INT_ZERO, INT_MINIMUM
         };
   int32_t iCmpgeDataArr[][2] =
         {
         INT_ZERO, INT_POS,
         INT_POS, INT_ZERO
         };
   int32_t iCmpleDataArr[][2] =
         {
         INT_POS, INT_ZERO,
         INT_ZERO, INT_POS
         };
   int64_t lCmpneDataArr[][2] =
         {
         LONG_ZERO, LONG_ZERO,
         LONG_MAXIMUM, LONG_MINIMUM
         };
   int64_t lCmpgtDataArr[][2] =
         {
         LONG_MINIMUM, LONG_ZERO,
         LONG_ZERO, LONG_MINIMUM
         };
   int64_t lCmpgeDataArr[][2] =
         {
         LONG_POS,LONG_NEG,
         LONG_NEG,LONG_POS
         };
   int64_t lCmpleDataArr[][2] =
         {
         LONG_NEG, LONG_POS,
         LONG_POS, LONG_NEG
         };
   uint32_t iuCmpgtDataArr[][2] =
         {
         UINT_POS, UINT_MINIMUM,
         UINT_MINIMUM, UINT_POS,
         UINT_POS, UINT_POS
         };
   uint32_t iuCmpltDataArr[][2] =
         {
         UINT_MAXIMUM, UINT_POS,
         UINT_POS, UINT_MAXIMUM,
         UINT_MINIMUM, UINT_POS,
         UINT_POS, UINT_MINIMUM
         };
   uint32_t iuCmpleDataArr[][2] =
         {
         UINT_MINIMUM, UINT_MINIMUM,
         UINT_POS, UINT_MAXIMUM,
         UINT_MAXIMUM, UINT_POS
         };
   uint64_t luCmpgtDataArr[][2] =
         {
         ULONG_MINIMUM, ULONG_MAXIMUM,
         ULONG_MAXIMUM, ULONG_MINIMUM
         };
   uint64_t luCmpltDataArr[][2] =
         {
         ULONG_MAXIMUM, ULONG_MINIMUM,
         ULONG_MINIMUM, ULONG_MAXIMUM

         };
   uint64_t luCmpgeDataArr[][2] =
         {
         ULONG_POS, ULONG_MINIMUM,
         ULONG_MINIMUM, ULONG_POS

         };
   uint64_t luCmpleDataArr[][2] =
         {
         ULONG_MAXIMUM, ULONG_POS,
         ULONG_POS, ULONG_MAXIMUM
         };

   //CompareAndBranch data array
   int32_t ifIcmpeqDataArr[][2] =
         {
         INT_MINIMUM, INT_MAXIMUM,
         INT_POS, INT_POS
         };
   int32_t ifIcmpneDataArr[][2] =
         {
         INT_MAXIMUM, INT_NEG,
         INT_NEG, INT_NEG
         };
   int32_t ifIcmpgtDataArr[][2] =
         {
         INT_NEG, INT_MINIMUM,
         INT_MINIMUM, INT_NEG
         };
   int32_t ifIcmpltDataArr[][2] =
         {
         INT_MINIMUM, INT_ZERO,
         INT_ZERO, INT_MINIMUM
         };
   int32_t ifIcmpgeDataArr[][2] =
         {
         INT_ZERO, INT_POS,
         INT_POS, INT_ZERO
         };
   int32_t ifIcmpleDataArr[][2] =
         {
         INT_POS, INT_ZERO,
         INT_ZERO, INT_POS
         };
   int64_t ifLcmpneDataArr[][2] =
         {
         LONG_ZERO, LONG_ZERO,
         LONG_MAXIMUM, LONG_MINIMUM
         };
   int64_t ifLcmpgeDataArr[][2] =
         {
         LONG_POS,LONG_NEG,
         LONG_NEG,LONG_POS
         };
   int64_t ifLcmpleDataArr[][2] =
         {
         LONG_NEG, LONG_POS,
         LONG_POS, LONG_NEG
         };
   uint32_t ifIuCmpgtDataArr[][2] =
         {
         UINT_MAXIMUM, UINT_MAXIMUM,
         UINT_MAXIMUM, UINT_POS
         };
   uint32_t ifIuCmpltDataArr[][2] =
         {
         UINT_MINIMUM, UINT_POS,
         UINT_POS, UINT_MINIMUM
         };
   uint32_t ifIuCmpgeDataArr[][2] =
         {
         UINT_POS, UINT_MINIMUM,
         UINT_MINIMUM, UINT_POS
         };
   uint32_t ifIuCmpleDataArr[][2] =
         {
         UINT_POS, UINT_MINIMUM,
         UINT_MINIMUM, UINT_POS
         };
   uint64_t ifLuCmpgtDataArr[][2] =
         {
         ULONG_MINIMUM, ULONG_MINIMUM,
         ULONG_MAXIMUM, ULONG_POS
         };
   uint64_t ifLuCmpltDataArr[][2] =
         {
         ULONG_POS,ULONG_MAXIMUM,
         ULONG_MAXIMUM, ULONG_POS
         };
   uint64_t ifLuCmpgeDataArr[][2] =
         {
         ULONG_MAXIMUM, ULONG_POS,
         ULONG_POS,ULONG_MAXIMUM
         };
   uint64_t ifLuCmpleDataArr[][2] =
         {
         ULONG_MAXIMUM, ULONG_MAXIMUM,
         ULONG_POS, ULONG_MINIMUM
         };

   int32_t rc = 0;
   int32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];

   signatureCharII_I_testMethodType * iCompareConst = 0;
   signatureCharJJ_I_testMethodType * lCompareConst = 0;
   unsignedCompareSignatureCharII_I_testMethodType * iuCompareConst = 0;
   unsignedCompareSignatureCharJJ_I_testMethodType * luCompareConst = 0;


   //iCompare
   testCaseNum = sizeof(iCmpeqDataArr) / sizeof(iCmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmpeq, compareEQ(iCmpeqDataArr[i][0], iCmpeqDataArr[i][1]), _iCmpeq(iCmpeqDataArr[i][0], iCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpeqConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpeqDataArr[i][0]), 2, &(iCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(iCmpeqDataArr[i][0], iCmpeqDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpeqConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(iCmpeqDataArr[i][0], iCmpeqDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpeqConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(iCmpeqDataArr[i][0], iCmpeqDataArr[i][1]), iCompareConst(iCmpeqDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iCmpneDataArr) / sizeof(iCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmpne, compareNE(iCmpneDataArr[i][0], iCmpneDataArr[i][1]), _iCmpne(iCmpneDataArr[i][0], iCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpneConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpneDataArr[i][0]), 2, &(iCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(iCmpneDataArr[i][0], iCmpneDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpneConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(iCmpneDataArr[i][0], iCmpneDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpneConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(iCmpneDataArr[i][0], iCmpneDataArr[i][1]), iCompareConst(iCmpneDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iCmpgtDataArr) / sizeof(iCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmpgt, compareGT(iCmpgtDataArr[i][0], iCmpgtDataArr[i][1]), _iCmpgt(iCmpgtDataArr[i][0], iCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpgtDataArr[i][0]), 2, &(iCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(iCmpgtDataArr[i][0], iCmpgtDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(iCmpgtDataArr[i][0], iCmpgtDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(iCmpgtDataArr[i][0], iCmpgtDataArr[i][1]), iCompareConst(iCmpgtDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iCmpltDataArr) / sizeof(iCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmplt, compareLT(iCmpltDataArr[i][0], iCmpltDataArr[i][1]), _iCmplt(iCmpltDataArr[i][0], iCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpltDataArr[i][0]), 2, &(iCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(iCmpltDataArr[i][0], iCmpltDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(iCmpltDataArr[i][0], iCmpltDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(iCmpltDataArr[i][0], iCmpltDataArr[i][1]), iCompareConst(iCmpltDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iCmpgeDataArr) / sizeof(iCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmpge, compareGE(iCmpgeDataArr[i][0], iCmpgeDataArr[i][1]), _iCmpge(iCmpgeDataArr[i][0], iCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpgeDataArr[i][0]), 2, &(iCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(iCmpgeDataArr[i][0], iCmpgeDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(iCmpgeDataArr[i][0], iCmpgeDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(iCmpgeDataArr[i][0], iCmpgeDataArr[i][1]), iCompareConst(iCmpgeDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iCmpleDataArr) / sizeof(iCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iCmple, compareLE(iCmpleDataArr[i][0], iCmpleDataArr[i][1]), _iCmple(iCmpleDataArr[i][0], iCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iCmpleDataArr[i][0]), 2, &(iCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(iCmpleDataArr[i][0], iCmpleDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(iCmpleDataArr[i][0], iCmpleDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, iCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "iCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::icmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(iCmpleDataArr[i][0], iCmpleDataArr[i][1]), iCompareConst(iCmpleDataArr[i][0], INT_PLACEHOLDER_2));
      }

   //lCompare
   testCaseNum = sizeof(lCmpneDataArr) / sizeof(lCmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmpne, compareNE(lCmpneDataArr[i][0], lCmpneDataArr[i][1]), _lCmpne(lCmpneDataArr[i][0], lCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpneConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpneDataArr[i][0]), 2, &(lCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(lCmpneDataArr[i][0], lCmpneDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

#ifndef TR_TARGET_POWER
      sprintf(resolvedMethodName, "lCmpneConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(lCmpneDataArr[i][0], lCmpneDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpneConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(lCmpneDataArr[i][0], lCmpneDataArr[i][1]), lCompareConst(lCmpneDataArr[i][0], LONG_PLACEHOLDER_2));
#endif
      }

   testCaseNum = sizeof(lCmpgtDataArr) / sizeof(lCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmpgt, compareGT(lCmpgtDataArr[i][0], lCmpgtDataArr[i][1]), _lCmpgt(lCmpgtDataArr[i][0], lCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpgtDataArr[i][0]), 2, &(lCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(lCmpgtDataArr[i][0], lCmpgtDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

#ifndef TR_TARGET_POWER
      sprintf(resolvedMethodName, "lCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(lCmpgtDataArr[i][0], lCmpgtDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGT(lCmpgtDataArr[i][0], lCmpgtDataArr[i][1]), lCompareConst(lCmpgtDataArr[i][0], LONG_PLACEHOLDER_2));
#endif
      }

   testCaseNum = sizeof(lCmpgeDataArr) / sizeof(lCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmpge, compareGE(lCmpgeDataArr[i][0], lCmpgeDataArr[i][1]), _lCmpge(lCmpgeDataArr[i][0], lCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpgeDataArr[i][0]), 2, &(lCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(lCmpgeDataArr[i][0], lCmpgeDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lCmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(lCmpgeDataArr[i][0], lCmpgeDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(lCmpgeDataArr[i][0], lCmpgeDataArr[i][1]), lCompareConst(lCmpgeDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(lCmpleDataArr) / sizeof(lCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_lCmple, compareLE(lCmpleDataArr[i][0], lCmpleDataArr[i][1]), _lCmple(lCmpleDataArr[i][0], lCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(lCmpleDataArr[i][0]), 2, &(lCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(lCmpleDataArr[i][0], lCmpleDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "lCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(lCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(lCmpleDataArr[i][0], lCmpleDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, lCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "lCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::lcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(lCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(lCmpleDataArr[i][0], lCmpleDataArr[i][1]), lCompareConst(lCmpleDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //iuCompare
   testCaseNum = sizeof(iuCmpgtDataArr) / sizeof(iuCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iuCmpgt, compareGT(iuCmpgtDataArr[i][0], iuCmpgtDataArr[i][1]), _iuCmpgt(iuCmpgtDataArr[i][0], iuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iuCmpgtDataArr[i][0]), 2, &(iuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(iuCmpgtDataArr[i][0], iuCmpgtDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iuCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iuCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(iuCmpgtDataArr[i][0], iuCmpgtDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, iuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(iuCmpgtDataArr[i][0], iuCmpgtDataArr[i][1]), iuCompareConst(iuCmpgtDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iuCmpltDataArr) / sizeof(iuCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iuCmplt, compareLT(iuCmpltDataArr[i][0], iuCmpltDataArr[i][1]), _iuCmplt(iuCmpltDataArr[i][0], iuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iuCmpltDataArr[i][0]), 2, &(iuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(iuCmpltDataArr[i][0], iuCmpltDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iuCmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iuCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(iuCmpltDataArr[i][0], iuCmpltDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, iuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(iuCmpltDataArr[i][0], iuCmpltDataArr[i][1]), iuCompareConst(iuCmpltDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(iuCmpleDataArr) / sizeof(iuCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_iuCmple, compareLE(iuCmpleDataArr[i][0], iuCmpleDataArr[i][1]), _iuCmple(iuCmpleDataArr[i][0], iuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(iuCmpleDataArr[i][0]), 2, &(iuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(iuCmpleDataArr[i][0], iuCmpleDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "iuCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(iuCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(iuCmpleDataArr[i][0], iuCmpleDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, iuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "iuCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::iucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(iuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(iuCmpleDataArr[i][0], iuCmpleDataArr[i][1]), iuCompareConst(iuCmpleDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(luCmpgtDataArr) / sizeof(luCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_luCmpgt, compareGT(luCmpgtDataArr[i][0], luCmpgtDataArr[i][1]), _luCmpgt(luCmpgtDataArr[i][0], luCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(luCmpgtDataArr[i][0]), 2, &(luCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(luCmpgtDataArr[i][0], luCmpgtDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "luCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(luCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(luCmpgtDataArr[i][0], luCmpgtDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, luCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(luCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(luCmpgtDataArr[i][0], luCmpgtDataArr[i][1]), luCompareConst(luCmpgtDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(luCmpltDataArr) / sizeof(luCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_luCmplt, compareLT(luCmpltDataArr[i][0], luCmpltDataArr[i][1]), _luCmplt(luCmpltDataArr[i][0], luCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(luCmpltDataArr[i][0]), 2, &(luCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(luCmpltDataArr[i][0], luCmpltDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "luCmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(luCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(luCmpltDataArr[i][0], luCmpltDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, luCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(luCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(luCmpltDataArr[i][0], luCmpltDataArr[i][1]), luCompareConst(luCmpltDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(luCmpgeDataArr) / sizeof(luCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_luCmpge, compareGE(luCmpgeDataArr[i][0], luCmpgeDataArr[i][1]), _luCmpge(luCmpgeDataArr[i][0], luCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(luCmpgeDataArr[i][0]), 2, &(luCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(luCmpgeDataArr[i][0], luCmpgeDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "luCmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(luCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(luCmpgeDataArr[i][0], luCmpgeDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, luCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(luCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(luCmpgeDataArr[i][0], luCmpgeDataArr[i][1]), luCompareConst(luCmpgeDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(luCmpleDataArr) / sizeof(luCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_luCmple, compareLE(luCmpleDataArr[i][0], luCmpleDataArr[i][1]), _luCmple(luCmpleDataArr[i][0], luCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(luCmpleDataArr[i][0]), 2, &(luCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(luCmpleDataArr[i][0], luCmpleDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "luCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(luCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(luCmpleDataArr[i][0], luCmpleDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, luCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "luCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::lucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(luCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(luCmpleDataArr[i][0], luCmpleDataArr[i][1]), luCompareConst(luCmpleDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   //ifiCompare
   testCaseNum = sizeof(ifIcmpeqDataArr) / sizeof(ifIcmpeqDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmpeq, compareEQ(ifIcmpeqDataArr[i][0], ifIcmpeqDataArr[i][1]), _ifIcmpeq(ifIcmpeqDataArr[i][0], ifIcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpeqConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpeqDataArr[i][0]), 2, &(ifIcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(ifIcmpeqDataArr[i][0], ifIcmpeqDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpeqConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpeqDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(ifIcmpeqDataArr[i][0], ifIcmpeqDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpeqDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpeqConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpeq, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpeqDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareEQ(ifIcmpeqDataArr[i][0], ifIcmpeqDataArr[i][1]), iCompareConst(ifIcmpeqDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIcmpneDataArr) / sizeof(ifIcmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmpne, compareNE(ifIcmpneDataArr[i][0], ifIcmpneDataArr[i][1]), _ifIcmpne(ifIcmpneDataArr[i][0], ifIcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpneConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpneDataArr[i][0]), 2, &(ifIcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(ifIcmpneDataArr[i][0], ifIcmpneDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpneConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(ifIcmpneDataArr[i][0], ifIcmpneDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpneConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpne, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareNE(ifIcmpneDataArr[i][0], ifIcmpneDataArr[i][1]), iCompareConst(ifIcmpneDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIcmpgtDataArr) / sizeof(ifIcmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmpgt, compareGT(ifIcmpgtDataArr[i][0], ifIcmpgtDataArr[i][1]), _ifIcmpgt(ifIcmpgtDataArr[i][0], ifIcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpgtDataArr[i][0]), 2, &(ifIcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(ifIcmpgtDataArr[i][0], ifIcmpgtDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(ifIcmpgtDataArr[i][0], ifIcmpgtDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGT(ifIcmpgtDataArr[i][0], ifIcmpgtDataArr[i][1]), iCompareConst(ifIcmpgtDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIcmpltDataArr) / sizeof(ifIcmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmplt, compareLT(ifIcmpltDataArr[i][0], ifIcmpltDataArr[i][1]), _ifIcmplt(ifIcmpltDataArr[i][0], ifIcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpltDataArr[i][0]), 2, &(ifIcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(ifIcmpltDataArr[i][0], ifIcmpltDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(ifIcmpltDataArr[i][0], ifIcmpltDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLT(ifIcmpltDataArr[i][0], ifIcmpltDataArr[i][1]), iCompareConst(ifIcmpltDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIcmpgeDataArr) / sizeof(ifIcmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmpge, compareGE(ifIcmpgeDataArr[i][0], ifIcmpgeDataArr[i][1]), _ifIcmpge(ifIcmpgeDataArr[i][0], ifIcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpgeDataArr[i][0]), 2, &(ifIcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(ifIcmpgeDataArr[i][0], ifIcmpgeDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(ifIcmpgeDataArr[i][0], ifIcmpgeDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareGE(ifIcmpgeDataArr[i][0], ifIcmpgeDataArr[i][1]), iCompareConst(ifIcmpgeDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIcmpleDataArr) / sizeof(ifIcmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIcmple, compareLE(ifIcmpleDataArr[i][0], ifIcmpleDataArr[i][1]), _ifIcmple(ifIcmpleDataArr[i][0], ifIcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIcmpleDataArr[i][0]), 2, &(ifIcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(ifIcmpleDataArr[i][0], ifIcmpleDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIcmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIcmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(ifIcmpleDataArr[i][0], ifIcmpleDataArr[i][1]), iCompareConst(INT_PLACEHOLDER_1, ifIcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIcmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iCompareConst,
            _numberOfBinaryArgs, TR::ificmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iCompareConst, compareLE(ifIcmpleDataArr[i][0], ifIcmpleDataArr[i][1]), iCompareConst(ifIcmpleDataArr[i][0], INT_PLACEHOLDER_2));
      }

   //iflCompare
   testCaseNum = sizeof(ifLcmpneDataArr) / sizeof(ifLcmpneDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmpne, compareNE(ifLcmpneDataArr[i][0], ifLcmpneDataArr[i][1]), _ifLcmpne(ifLcmpneDataArr[i][0], ifLcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpneConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpneDataArr[i][0]), 2, &(ifLcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(ifLcmpneDataArr[i][0], ifLcmpneDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

#ifndef TR_TARGET_POWER
      sprintf(resolvedMethodName, "ifLcmpneConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpneDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(ifLcmpneDataArr[i][0], ifLcmpneDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpneDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpneConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpne, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpneDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareNE(ifLcmpneDataArr[i][0], ifLcmpneDataArr[i][1]), lCompareConst(ifLcmpneDataArr[i][0], LONG_PLACEHOLDER_2));
#endif
      }

   testCaseNum = sizeof(ifLcmpgeDataArr) / sizeof(ifLcmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmpge, compareGE(ifLcmpgeDataArr[i][0], ifLcmpgeDataArr[i][1]), _ifLcmpge(ifLcmpgeDataArr[i][0], ifLcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpgeDataArr[i][0]), 2, &(ifLcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(ifLcmpgeDataArr[i][0], ifLcmpgeDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLcmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(ifLcmpgeDataArr[i][0], ifLcmpgeDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareGE(ifLcmpgeDataArr[i][0], ifLcmpgeDataArr[i][1]), lCompareConst(ifLcmpgeDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLcmpleDataArr) / sizeof(ifLcmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLcmple, compareLE(ifLcmpleDataArr[i][0], ifLcmpleDataArr[i][1]), _ifLcmple(ifLcmpleDataArr[i][0], ifLcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLcmpleDataArr[i][0]), 2, &(ifLcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(ifLcmpleDataArr[i][0], ifLcmpleDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLcmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLcmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(ifLcmpleDataArr[i][0], ifLcmpleDataArr[i][1]), lCompareConst(LONG_PLACEHOLDER_1, ifLcmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLcmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      lCompareConst,
            _numberOfBinaryArgs, TR::iflcmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLcmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(lCompareConst, compareLE(ifLcmpleDataArr[i][0], ifLcmpleDataArr[i][1]), lCompareConst(ifLcmpleDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIuCmpgtDataArr) / sizeof(ifIuCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIuCmpgt, compareGT(ifIuCmpgtDataArr[i][0], ifIuCmpgtDataArr[i][1]), _ifIuCmpgt(ifIuCmpgtDataArr[i][0], ifIuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIuCmpgtDataArr[i][0]), 2, &(ifIuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(ifIuCmpgtDataArr[i][0], ifIuCmpgtDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIuCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIuCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(ifIuCmpgtDataArr[i][0], ifIuCmpgtDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, ifIuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpgt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGT(ifIuCmpgtDataArr[i][0], ifIuCmpgtDataArr[i][1]), iuCompareConst(ifIuCmpgtDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIuCmpltDataArr) / sizeof(ifIuCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIuCmplt, compareLT(ifIuCmpltDataArr[i][0], ifIuCmpltDataArr[i][1]), _ifIuCmplt(ifIuCmpltDataArr[i][0], ifIuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIuCmpltDataArr[i][0]), 2, &(ifIuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(ifIuCmpltDataArr[i][0], ifIuCmpltDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIuCmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIuCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(ifIuCmpltDataArr[i][0], ifIuCmpltDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, ifIuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmplt, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLT(ifIuCmpltDataArr[i][0], ifIuCmpltDataArr[i][1]), iuCompareConst(ifIuCmpltDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIuCmpgeDataArr) / sizeof(ifIuCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIuCmpge, compareGE(ifIuCmpgeDataArr[i][0], ifIuCmpgeDataArr[i][1]), _ifIuCmpge(ifIuCmpgeDataArr[i][0], ifIuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIuCmpgeDataArr[i][0]), 2, &(ifIuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(ifIuCmpgeDataArr[i][0], ifIuCmpgeDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIuCmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIuCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(ifIuCmpgeDataArr[i][0], ifIuCmpgeDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, ifIuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmpge, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareGE(ifIuCmpgeDataArr[i][0], ifIuCmpgeDataArr[i][1]), iuCompareConst(ifIuCmpgeDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifIuCmpleDataArr) / sizeof(ifIuCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifIuCmple, compareLE(ifIuCmpleDataArr[i][0], ifIuCmpleDataArr[i][1]), _ifIuCmple(ifIuCmpleDataArr[i][0], ifIuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 4, 1, &(ifIuCmpleDataArr[i][0]), 2, &(ifIuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(ifIuCmpleDataArr[i][0], ifIuCmpleDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifIuCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 1, &(ifIuCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(ifIuCmpleDataArr[i][0], ifIuCmpleDataArr[i][1]), iuCompareConst(INT_PLACEHOLDER_1, ifIuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifIuCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      iuCompareConst,
            _numberOfBinaryArgs, TR::ifiucmple, resolvedMethodName, _argTypesBinaryInt, TR::Int32, rc, 2, 2, &(ifIuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(iuCompareConst, compareLE(ifIuCmpleDataArr[i][0], ifIuCmpleDataArr[i][1]), iuCompareConst(ifIuCmpleDataArr[i][0], INT_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLuCmpgtDataArr) / sizeof(ifLuCmpgtDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLuCmpgt, compareGT(ifLuCmpgtDataArr[i][0], ifLuCmpgtDataArr[i][1]), _ifLuCmpgt(ifLuCmpgtDataArr[i][0], ifLuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpgtConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLuCmpgtDataArr[i][0]), 2, &(ifLuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(ifLuCmpgtDataArr[i][0], ifLuCmpgtDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLuCmpgtConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLuCmpgtDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(ifLuCmpgtDataArr[i][0], ifLuCmpgtDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, ifLuCmpgtDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpgtConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpgt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLuCmpgtDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGT(ifLuCmpgtDataArr[i][0], ifLuCmpgtDataArr[i][1]), luCompareConst(ifLuCmpgtDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLuCmpltDataArr) / sizeof(ifLuCmpltDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLuCmplt, compareLT(ifLuCmpltDataArr[i][0], ifLuCmpltDataArr[i][1]), _ifLuCmplt(ifLuCmpltDataArr[i][0], ifLuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpltConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLuCmpltDataArr[i][0]), 2, &(ifLuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(ifLuCmpltDataArr[i][0], ifLuCmpltDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLuCmpltConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLuCmpltDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(ifLuCmpltDataArr[i][0], ifLuCmpltDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, ifLuCmpltDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpltConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmplt, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLuCmpltDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLT(ifLuCmpltDataArr[i][0], ifLuCmpltDataArr[i][1]), luCompareConst(ifLuCmpltDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLuCmpgeDataArr) / sizeof(ifLuCmpgeDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLuCmpge, compareGE(ifLuCmpgeDataArr[i][0], ifLuCmpgeDataArr[i][1]), _ifLuCmpge(ifLuCmpgeDataArr[i][0], ifLuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpgeConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLuCmpgeDataArr[i][0]), 2, &(ifLuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(ifLuCmpgeDataArr[i][0], ifLuCmpgeDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLuCmpgeConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLuCmpgeDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(ifLuCmpgeDataArr[i][0], ifLuCmpgeDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, ifLuCmpgeDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpgeConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmpge, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLuCmpgeDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareGE(ifLuCmpgeDataArr[i][0], ifLuCmpgeDataArr[i][1]), luCompareConst(ifLuCmpgeDataArr[i][0], LONG_PLACEHOLDER_2));
      }

   testCaseNum = sizeof(ifLuCmpleDataArr) / sizeof(ifLuCmpleDataArr[0]);
   for(uint32_t i = 0; i < testCaseNum; ++i)
      {
      OMR_CT_EXPECT_EQ(_ifLuCmple, compareLE(ifLuCmpleDataArr[i][0], ifLuCmpleDataArr[i][1]), _ifLuCmple(ifLuCmpleDataArr[i][0], ifLuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpleConst1_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 4, 1, &(ifLuCmpleDataArr[i][0]), 2, &(ifLuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(ifLuCmpleDataArr[i][0], ifLuCmpleDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, LONG_PLACEHOLDER_2));

      sprintf(resolvedMethodName, "ifLuCmpleConst2_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 1, &(ifLuCmpleDataArr[i][0]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(ifLuCmpleDataArr[i][0], ifLuCmpleDataArr[i][1]), luCompareConst(LONG_PLACEHOLDER_1, ifLuCmpleDataArr[i][1]));

      sprintf(resolvedMethodName, "ifLuCmpleConst3_TestCase%d", i + 1);
compileOpCodeMethod(      luCompareConst,
            _numberOfBinaryArgs, TR::iflucmple, resolvedMethodName, _argTypesBinaryLong, TR::Int32, rc, 2, 2, &(ifLuCmpleDataArr[i][1]));
      OMR_CT_EXPECT_EQ(luCompareConst, compareLE(ifLuCmpleDataArr[i][0], ifLuCmpleDataArr[i][1]), luCompareConst(ifLuCmpleDataArr[i][0], LONG_PLACEHOLDER_2));
      }
   }

void
OpCodesTest::invokeSelectTests()
   {
   int32_t rc = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   uint32_t testCaseNum = 0;
   uint32_t testCaseNumCheck = 0;

   int32_t iselectChild1Arr[] =
      {
      INT_MAXIMUM, INT_MAXIMUM, INT_MINIMUM, INT_POS, INT_ZERO, INT_MINIMUM, INT_POS, INT_ZERO, INT_NEG,
      INT_NEG, INT_NEG, INT_ZERO, INT_POS, INT_NEG, INT_ZERO, INT_MINIMUM, INT_MAXIMUM, INT_POS
      };

   int32_t intArr[][2] =
      {
      INT_NEG, INT_MINIMUM,
      INT_NEG, INT_POS,
      INT_NEG, INT_POS,
      INT_MAXIMUM, INT_ZERO,
      INT_ZERO, INT_POS,
      INT_ZERO, INT_POS,
      INT_ZERO, INT_MAXIMUM,
      INT_POS, INT_NEG,
      INT_POS, INT_MINIMUM,
      INT_MAXIMUM, INT_POS,
      INT_MINIMUM, INT_ZERO,
      INT_MINIMUM, INT_MAXIMUM,
      INT_NEG, INT_MAXIMUM,
      INT_ZERO, INT_NEG,
      INT_POS, INT_ZERO,
      INT_MAXIMUM, INT_NEG,
      INT_ZERO, INT_MINIMUM,
      INT_MINIMUM, INT_POS
      };

   testCaseNum = sizeof(iselectChild1Arr) / sizeof(iselectChild1Arr[0]);
   testCaseNumCheck = sizeof(intArr) / sizeof(intArr[0]);
   TR_ASSERT( (testCaseNum > 0) && (testCaseNum == testCaseNumCheck), "There is problem in iselect input array");
   signatureCharIII_I_testMethodType * iSelectConst = 0;
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "iSelectConst%d", i + 1);
      OMR_CT_EXPECT_EQ(_iselect, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), _iselect(iselectChild1Arr[i], intArr[i][0], intArr[i][1]));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 6, 1, &iselectChild1Arr[i], 2, &intArr[i][0], 3, &intArr[i][1]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2, INT_PLACEHOLDER_3));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 4, 1, &iselectChild1Arr[i], 2, &intArr[i][0]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(INT_PLACEHOLDER_1, INT_PLACEHOLDER_2, intArr[i][1]));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 4, 1, &iselectChild1Arr[i], 3, &intArr[i][1]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(INT_PLACEHOLDER_1, intArr[i][0], INT_PLACEHOLDER_3));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 4, 2, &intArr[i][0], 3, &intArr[i][1]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(iselectChild1Arr[i], INT_PLACEHOLDER_2, INT_PLACEHOLDER_3));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 2, 1, &iselectChild1Arr[i]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(INT_PLACEHOLDER_1, intArr[i][0], intArr[i][1]));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 2, 2, &intArr[i][0]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(iselectChild1Arr[i], INT_PLACEHOLDER_1, intArr[i][1]));

compileOpCodeMethod(      iSelectConst, _numberOfSelectArgs, TR::iselect,
            resolvedMethodName, _argTypesSelectInt, TR::Int32, rc, 2, 3, &intArr[i][1]);
      OMR_CT_EXPECT_EQ(iSelectConst, select(iselectChild1Arr[i], intArr[i][0], intArr[i][1]), iSelectConst(iselectChild1Arr[i], intArr[i][0], INT_PLACEHOLDER_1));
      }
   }

void
OpCodesTest::invokeAddressTests()
   {
   int32_t rc = 0;

   uintptr_t aUnaryDataArr[] =
      {
      (uintptr_t) &INT_POS,
      (uintptr_t) &INT_MAXIMUM,
      (uintptr_t) &INT_ZERO,
      (uintptr_t) &LONG_POS,
      (uintptr_t) &LONG_MAXIMUM,
      (uintptr_t) &LONG_ZERO
      };

   uint32_t testCaseNum = 0;
   char resolvedMethodName [RESOLVED_METHOD_NAME_LENGTH];
   signatureCharL_L_testMethodType *aUnaryCons = 0;
   signatureCharL_I_testMethodType *a2iConst = 0;

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_aload, aUnaryDataArr[i], _aload(aUnaryDataArr[i]));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      sprintf(resolvedMethodName, "aConst%d", i + 1);
compileOpCodeMethod(      aUnaryCons, _numberOfUnaryArgs, TR::aconst, resolvedMethodName, _argTypesUnaryAddress, TR::Address, rc, 2, 1, &(aUnaryDataArr[i]));
      OMR_CT_EXPECT_EQ(aUnaryCons, aUnaryDataArr[i], aUnaryCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_areturn, aUnaryDataArr[i], _areturn(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "aReturnConst%d", i + 1);
compileOpCodeMethod(      aUnaryCons, _numberOfUnaryArgs, TR::areturn, resolvedMethodName, _argTypesUnaryAddress, TR::Address, rc, 2, 1, &(aUnaryDataArr[i]));
      OMR_CT_EXPECT_EQ(aUnaryCons, aUnaryDataArr[i], aUnaryCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_astore, aUnaryDataArr[i], _astore(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "aStoreConst%d", i + 1);
compileOpCodeMethod(      aUnaryCons, _numberOfUnaryArgs, TR::astore, resolvedMethodName, _argTypesUnaryAddress, TR::Address, rc, 2, 1, &(aUnaryDataArr[i]));
      OMR_CT_EXPECT_EQ(aUnaryCons, aUnaryDataArr[i], aUnaryCons(ADDRESS_PLACEHOLDER_1));
      }

   testCaseNum = sizeof(aUnaryDataArr) / sizeof(aUnaryDataArr[0]);
   for (int32_t i = 0 ; i < testCaseNum ; i++)
      {
      OMR_CT_EXPECT_EQ(_a2i, convert(aUnaryDataArr[i], INT_POS), _a2i(aUnaryDataArr[i]));

      sprintf(resolvedMethodName, "a2iConst%d", i + 1);
      compileOpCodeMethod(a2iConst, _numberOfUnaryArgs, TR::a2i, resolvedMethodName, _argTypesUnaryAddress, TR::Int32, rc, 2, 1, &aUnaryDataArr[i]);
      OMR_CT_EXPECT_EQ(a2iConst, convert(aUnaryDataArr[i], INT_POS), a2iConst(ADDRESS_PLACEHOLDER_1));
      }
   }

} // namespace TestCompiler

//groups by testname
TEST(JITCrossPlatformsOpCodesTest, UnaryTest)
   {
   ::TestCompiler::OpCodesTest unaryTest;
   unaryTest.compileUnaryTestMethods();
   unaryTest.invokeUnaryTests();
   }

TEST(JITCrossPlatformsOpCodesTest, IntegerArithmeticTest)
   {
   ::TestCompiler::OpCodesTest integerArithmeticTest;
   integerArithmeticTest.compileIntegerArithmeticTestMethods();
   integerArithmeticTest.invokeIntegerArithmeticTests();
   }

TEST(JITCrossPlatformsOpCodesTest, MemoryOperationTest)
   {
   ::TestCompiler::OpCodesTest memoryOperationTest;
   memoryOperationTest.compileMemoryOperationTestMethods();
   memoryOperationTest.invokeMemoryOperationTests();
   }

TEST(JITCrossPlatformsOpCodesTest, BitwiseTest)
   {
   ::TestCompiler::OpCodesTest bitwiseTest;
   bitwiseTest.compileBitwiseMethods();
   bitwiseTest.invokeBitwiseTests();
   }

TEST(JITCrossPlatformsOpCodesTest, CompareTest)
   {
   ::TestCompiler::OpCodesTest compareTest;
   compareTest.compileCompareTestMethods();
   compareTest.invokeCompareTests();
   }

TEST(JITCrossPlatformsOpCodesTest, SelectTest)
   {
   ::TestCompiler::OpCodesTest selectTest;
   selectTest.compileSelectTestMethods();
   selectTest.invokeSelectTests();
   }

TEST(JITCrossPlatformsOpCodesTest, AddressTest)
   {
   ::TestCompiler::OpCodesTest addressTest;
   addressTest.compileAddressTestMethods();
   addressTest.invokeAddressTests();
   }

TEST(JITCrossPlatformsOpCodesTest, UnsupportedOpCodesTest)
   {
   ::TestCompiler::OpCodesTest unsupportedOpcodesTest;
   unsupportedOpcodesTest.UnsupportedOpCodesTests();
   }

TEST(JITCrossPlatformsOpCodesTest, DISABLED_OpCodesTests)
   {
   //Jazz103 Work item 110364
   //To temporarily enable "DISABLED" test, append " --gtest_also_run_disabled_tests" in the command line.
   ::TestCompiler::OpCodesTest disabledOpCodesTest;
   disabledOpCodesTest.compileDisabledOpCodesTests();
   disabledOpCodesTest.invokeDisabledOpCodesTests();
   }


TEST(JITCrossPlatformsOpCodesTest, DISABLED_UnaryTest)
   {
   //Jazz103 Work Item 110363
   //This defect is related to 97974: Separate group to temporarily disable crashed (will work on) testcases
   //Please move this test and recover f2i testcase number from 3 to 5.
   ::TestCompiler::OpCodesTest disabledUnaryTest;
   disabledUnaryTest.invokeNoHelperUnaryTests();
   }
