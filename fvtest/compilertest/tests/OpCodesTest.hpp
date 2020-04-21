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

#include "TestDriver.hpp"

#include <cmath>
#include <stdint.h>
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "gtest/gtest.h"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "tests/injectors/BinaryOpIlInjector.hpp"
#include "tests/injectors/CallIlInjector.hpp"
#include "tests/injectors/ChildlessUnaryOpIlInjector.hpp"
#include "tests/injectors/CmpBranchOpIlInjector.hpp"
#include "tests/injectors/IndirectLoadIlInjector.hpp"
#include "tests/injectors/IndirectStoreIlInjector.hpp"
#include "tests/injectors/StoreOpIlInjector.hpp"
#include "tests/injectors/SelectOpIlInjector.hpp"
#include "tests/injectors/UnaryOpIlInjector.hpp"

#if defined(J9ZOS390) || defined(AIXPPC)
namespace std
{
   using ::isnan;
}
#endif

#define OMR_CT_EXPECT_EQ(compilee, a, b) if (compilee != NULL) EXPECT_EQ(a, b)
#define OMR_CT_EXPECT_DOUBLE_EQ(compilee, a, b) if (compilee != NULL) EXPECT_DOUBLE_EQ(a, b)
#define OMR_CT_EXPECT_FLOAT_EQ(compilee, a, b) if (compilee != NULL) EXPECT_FLOAT_EQ(a, b)

namespace TestCompiler
{
//unsigned signatureChars
typedef int32_t (unsignedSignatureCharS_I_testMethodType)(uint16_t);
typedef int64_t (unsignedSignatureCharS_J_testMethodType)(uint16_t);
typedef float (unsignedSignatureCharS_F_testMethodType)(uint16_t);
typedef double (unsignedSignatureCharS_D_testMethodType)(uint16_t);
typedef int64_t (unsignedSignatureCharI_J_testMethodType)(uint32_t);
typedef float (unsignedSignatureCharI_F_testMethodType)(uint32_t);
typedef double (unsignedSignatureCharI_D_testMethodType)(uint32_t);
typedef float (unsignedSignatureCharJ_F_testMethodType)(uint64_t);
typedef double (unsignedSignatureCharJ_D_testMethodType)(uint64_t);
typedef int32_t (unsignedSignatureCharB_I_testMethodType)(uint8_t);
typedef int64_t (unsignedSignatureCharB_J_testMethodType)(uint8_t);
typedef float (unsignedSignatureCharB_F_testMethodType)(uint8_t);
typedef double (unsignedSignatureCharB_D_testMethodType)(uint8_t);
typedef int16_t (unsignedSignatureCharB_S_testMethodType)(uint8_t);
typedef uint8_t (unsignedSignatureCharBB_B_testMethodType)(uint8_t, uint8_t);
typedef uint16_t (unsignedSignatureCharSS_S_testMethodType)(uint16_t, uint16_t);
typedef uint32_t (unsignedSignatureCharII_I_testMethodType)(uint32_t, uint32_t);
typedef uint64_t (unsignedSignatureCharJJ_J_testMethodType)(uint64_t, uint64_t);
typedef int32_t (unsignedCompareSignatureCharBB_I_testMethodType)(uint8_t, uint8_t);
typedef int32_t (unsignedCompareSignatureCharSS_I_testMethodType)(uint16_t, uint16_t);
typedef int32_t (unsignedCompareSignatureCharII_I_testMethodType)(uint32_t, uint32_t);
typedef int32_t (unsignedCompareSignatureCharJJ_I_testMethodType)(uint64_t, uint64_t);

//signed signatureChars
typedef int8_t (signatureCharB_B_testMethodType)(int8_t);
typedef int16_t (signatureCharS_S_testMethodType)(int16_t);
typedef int32_t (signatureCharI_I_testMethodType)(int32_t);
typedef int64_t (signatureCharJ_J_testMethodType)(int64_t);
typedef float (signatureCharF_F_testMethodType)(float);
typedef double (signatureCharD_D_testMethodType)(double);
typedef int8_t (signatureCharBB_B_testMethodType)(int8_t, int8_t);
typedef int16_t (signatureCharSS_S_testMethodType)(int16_t, int16_t);
typedef int32_t (signatureCharII_I_testMethodType)(int32_t, int32_t);
typedef int64_t (signatureCharJJ_J_testMethodType)(int64_t, int64_t);
typedef float (signatureCharFF_F_testMethodType)(float, float);
typedef double (signatureCharDD_D_testMethodType)(double, double);
typedef int32_t (signatureCharB_I_testMethodType)(int8_t);
typedef int64_t (signatureCharB_J_testMethodType)(int8_t);
typedef float (signatureCharB_F_testMethodType)(int8_t);
typedef double (signatureCharB_D_testMethodType)(int8_t);
typedef int16_t (signatureCharB_S_testMethodType)(int8_t);
typedef int32_t (signatureCharS_I_testMethodType)(int16_t);
typedef int64_t (signatureCharS_J_testMethodType)(int16_t);
typedef float (signatureCharS_F_testMethodType)(int16_t);
typedef double (signatureCharS_D_testMethodType)(int16_t);
typedef int8_t (signatureCharS_B_testMethodType)(int16_t);
typedef int64_t (signatureCharI_J_testMethodType)(int32_t);
typedef float (signatureCharI_F_testMethodType)(int32_t);
typedef double (signatureCharI_D_testMethodType)(int32_t);
typedef int8_t (signatureCharI_B_testMethodType)(int32_t);
typedef int16_t (signatureCharI_S_testMethodType)(int32_t);
typedef int32_t (signatureCharJ_I_testMethodType)(int64_t);
typedef float (signatureCharJ_F_testMethodType)(int64_t);
typedef double (signatureCharJ_D_testMethodType)(int64_t);
typedef int8_t (signatureCharJ_B_testMethodType)(int64_t);
typedef int16_t (signatureCharJ_S_testMethodType)(int64_t);
typedef int32_t (signatureCharF_I_testMethodType)(float);
typedef int64_t (signatureCharF_J_testMethodType)(float);
typedef double (signatureCharF_D_testMethodType)(float);
typedef int8_t (signatureCharF_B_testMethodType)(float);
typedef int16_t (signatureCharF_S_testMethodType)(float);
typedef int32_t (signatureCharD_I_testMethodType)(double);
typedef int64_t (signatureCharD_J_testMethodType)(double);
typedef float (signatureCharD_F_testMethodType)(double);
typedef int8_t (signatureCharD_B_testMethodType)(double);
typedef int16_t (signatureCharD_S_testMethodType)(double);
typedef int32_t (signatureCharJJ_I_testMethodType)(int64_t, int64_t);
typedef int32_t (signatureCharDD_I_testMethodType)(double, double);
typedef int32_t (signatureCharFF_I_testMethodType)(float, float);
typedef int32_t (signatureCharSS_I_testMethodType)(int16_t, int16_t);
typedef int32_t (signatureCharBB_I_testMethodType)(int8_t, int8_t);
typedef int8_t (signatureCharIBB_B_testMethodType)(int32_t, int8_t, int8_t);
typedef int16_t (signatureCharISS_S_testMethodType)(int32_t, int16_t, int16_t);
typedef int32_t (signatureCharIII_I_testMethodType)(int32_t, int32_t, int32_t);
typedef int64_t (signatureCharIJJ_J_testMethodType)(int32_t, int64_t, int64_t);
typedef float (signatureCharIFF_F_testMethodType)(int32_t, float, float);
typedef double (signatureCharIDD_D_testMethodType)(int32_t, double, double);

//address signatureChars
typedef int8_t (signatureCharL_B_testMethodType)(uintptr_t);
typedef int16_t (signatureCharL_S_testMethodType)(uintptr_t);
typedef int32_t (signatureCharL_I_testMethodType)(uintptr_t);
typedef int64_t (signatureCharL_J_testMethodType)(uintptr_t);
typedef double (signatureCharL_D_testMethodType)(uintptr_t);
typedef float (signatureCharL_F_testMethodType)(uintptr_t);
typedef uintptr_t (signatureCharB_L_testMethodType)(int8_t);
typedef uintptr_t (signatureCharS_L_testMethodType)(int16_t);
typedef uintptr_t (signatureCharI_L_testMethodType)(int32_t);
typedef uintptr_t (signatureCharJ_L_testMethodType)(int64_t);
typedef uintptr_t (unsignedSignatureCharB_L_testMethodType)(uint8_t);
typedef uintptr_t (unsignedSignatureCharS_L_testMethodType)(uint16_t);
typedef uintptr_t (unsignedSignatureCharI_L_testMethodType)(uint32_t);
typedef uintptr_t (unsignedSignatureCharJ_L_testMethodType)(uint64_t);
typedef uintptr_t (signatureCharL_L_testMethodType)(uintptr_t);
typedef int32_t (signatureCharLL_I_testMethodType)(uintptr_t, uintptr_t);
typedef uintptr_t (signatureCharILL_L_testMethodType)(int32_t, uintptr_t, uintptr_t);


typedef int32_t (signatureCharLI_I_testMethodType)(uintptr_t, int32_t);
typedef int64_t (signatureCharLJ_J_testMethodType)(uintptr_t, int64_t);
typedef double (signatureCharLD_D_testMethodType)(uintptr_t, double);
typedef float (signatureCharLF_F_testMethodType)(uintptr_t, float);
typedef int8_t (signatureCharLB_B_testMethodType)(uintptr_t, int8_t);
typedef int16_t (signatureCharLS_S_testMethodType)(uintptr_t, int16_t);
typedef uintptr_t (signatureCharLL_L_testMethodType)(uintptr_t, uintptr_t);

class OpCodesTest : public TestDriver
   {
   public:
   virtual void compileTestMethods();
   virtual void invokeTests();

   virtual void compileIntegerArithmeticTestMethods();
   virtual void compileMemoryOperationTestMethods();
   virtual void compileUnaryTestMethods();
   virtual void compileBitwiseMethods();
   virtual void compileCompareTestMethods();
   virtual void compileSelectTestMethods();
   virtual void compileAddressTestMethods();
   virtual void compileDisabledOpCodesTests();

   virtual void invokeIntegerArithmeticTests();
   virtual void invokeMemoryOperationTests();
   virtual void invokeUnaryTests();
   virtual void invokeBitwiseTests();
   virtual void invokeCompareTests();
   virtual void invokeSelectTests();
   virtual void invokeAddressTests();
   virtual void invokeDisabledOpCodesTests();

   //Temporarily put f2i(MAX), f2l(MAX), d2i(MAX) and d2l(MAX) into
   //another function for no helper issue tracking and testing.
   virtual void invokeNoHelperUnaryTests();

   //Unsupported OpCodes are tested in this function
   virtual void UnsupportedOpCodesTests();

   template <typename functiontype>
   int32_t
   compileOpCodeMethod(functiontype& resultpointer,
         int32_t opCodeArgsNum,
         TR::ILOpCodes opCode,
         char * resolvedMethodName,
         TR::DataType * argTypes,
         TR::DataType returnType,
         int32_t & returnCode,
         int32_t numArgs = 0,
         ...)
   {
   if ((numArgs % 2) != 0)
      {
      fprintf(stderr, "Error: numArgs must be called with zero or an even args, numArgs is %d", numArgs);
      exit(-1);
      }

   OpIlInjector * opCodeInjector = 0;
   TR::ILOpCode op(opCode);

   TR::TypeDictionary types;

   CmpBranchOpIlInjector        cmpBranchIlInjector(&types, this, opCode);
   BinaryOpIlInjector           opCodeBinaryIlInjector(&types, this, opCode);
   UnaryOpIlInjector            opCodeUnaryInjector(&types, this, opCode);
   SelectOpIlInjector          selectOpIlInjector(&types, this, opCode);
   ChildlessUnaryOpIlInjector   childlessUnaryOpIlInjector(&types, this, opCode);
   StoreOpIlInjector            storeOpIlInjector(&types, this, opCode);
   IndirectLoadIlInjector       indirectLoadIlInjector(&types, this, opCode);
   IndirectStoreIlInjector      indirectStoreIlInjector(&types, this, opCode);

   if (op.isBooleanCompare() && op.isBranch())
      {
      opCodeInjector = &cmpBranchIlInjector;
      }
   else if (op.isSelect())
      {
      opCodeInjector = &selectOpIlInjector;
      }
   else if (op.isStoreIndirect())
      {
      opCodeInjector = &indirectStoreIlInjector;
      }
   else if (op.isLoadIndirect())
      {
      opCodeInjector = &indirectLoadIlInjector;
      }
   else if (((op.isLoadVar() || op.isLoadConst()) && !op.isIndirect()) || op.isReturn() )
      {
      opCodeInjector = &childlessUnaryOpIlInjector;
      }
   else if (op.isStore() && !op.isStoreIndirect())
      {
      opCodeInjector = &storeOpIlInjector;
      }
   else
      {
      switch (opCodeArgsNum)
         {
         case 1:
            opCodeInjector = &opCodeUnaryInjector;
            break;
         case 2:
            opCodeInjector = &opCodeBinaryIlInjector;
            break;
         default:
            fprintf(stderr, "didn't select an injector based on argument number %d", opCodeArgsNum);
            exit(-1);
         }
      }

   TR_ASSERT(opCodeInjector, "Didn't select an injector!");

   TR::IlType **argIlTypes = new TR::IlType*[opCodeArgsNum];
   for (uint32_t a=0;a < opCodeArgsNum;a++)
      argIlTypes[a] = types.PrimitiveType(argTypes[a]);

   if (numArgs != 0)
      {
      va_list args;
      va_start(args, numArgs);
      for (int32_t i = 0; i < numArgs; i = i + 2)
         {

         uint32_t pos = va_arg(args, uint32_t);
         void * value = va_arg(args, void *);

         switch (argTypes[pos - 1])
             {
             case TR::Int8:
                {
                int8_t *int8Value = (int8_t *) value;
                opCodeInjector->bconstParm(pos, *int8Value);
                break;
                }
             case TR::Int16:
                {
                int16_t * int16Value = (int16_t *) value;
                opCodeInjector->sconstParm(pos, *int16Value);
                break;
                }
             case TR::Int32:
                {
                int32_t * int32Value = (int32_t *) value;
                opCodeInjector->iconstParm(pos, *int32Value);
                break;
                }
             case TR::Int64:
                {
                int64_t * int64Value = (int64_t *) value;
                opCodeInjector->lconstParm(pos, *int64Value);
                break;
                }
             case TR::Float:
                {
                float * floatValue = (float *) value;
                opCodeInjector->fconstParm(pos, *floatValue);
                break;
                }
             case TR::Double:
                {
                double * doubleValue = (double *) value;
                opCodeInjector->dconstParm(pos, *doubleValue);
                break;
                }
             case TR::Address:
                {
                uintptr_t * addressValue = (uintptr_t *) value;
                opCodeInjector->aconstParm(pos, *addressValue);
                break;
                }
             default:
                TR_ASSERT(0, "Wrong dataType or not supported dataType");
             }
          }
      va_end(args);
      }
   TR::ResolvedMethod opCodeCompilee(__FILE__, LINETOSTR(__LINE__), resolvedMethodName, opCodeArgsNum, argIlTypes, types.PrimitiveType(returnType), 0, opCodeInjector);
   TR::IlGeneratorMethodDetails opCodeDetails(&opCodeCompilee);
   uint8_t *startPC= compileMethod(opCodeDetails, warm, returnCode);
   EXPECT_TRUE(COMPILATION_SUCCEEDED == returnCode ||
               COMPILATION_IL_GEN_FAILURE == returnCode ||
               COMPILATION_REQUESTED == returnCode)
      << "compileOpCodeMethod: Compiling method " << resolvedMethodName << " failed unexpectedly";
   resultpointer = reinterpret_cast<functiontype>(startPC);
   return returnCode;
   }

   template <typename functiontype>
   int32_t
   compileDirectCallOpCodeMethod(functiontype& resultpointer,
         int32_t opCodeArgsNum,
         TR::ILOpCodes opCodeCompilee,
         TR::ILOpCodes opCode,
         char * compileeResolvedMethodName,
         char * testResolvedMethodName,
         TR::DataType * argTypes,
         TR::DataType returnType,
         int32_t & returnCode)
      {
      TR::TypeDictionary types;
      ChildlessUnaryOpIlInjector functionIlInjector(&types, this, opCodeCompilee);

      TR::IlType **argIlTypes = new TR::IlType*[opCodeArgsNum];
      for (int32_t i=0;i < opCodeArgsNum;i++)
         argIlTypes[i] = types.PrimitiveType(argTypes[i]);

      TR::ResolvedMethod functionCompilee(__FILE__, LINETOSTR(__LINE__), compileeResolvedMethodName, opCodeArgsNum, argIlTypes, types.PrimitiveType(returnType), 0, &functionIlInjector);
      TR::IlGeneratorMethodDetails functionDetails(&functionCompilee);
      switch (returnType)
         {
         case TR::Int32:
            _int32Compilee = &functionCompilee;
            _int32CompiledMethod = (signatureCharI_I_testMethodType *) (compileMethod(functionDetails, warm, returnCode));
            functionCompilee.setEntryPoint((void *)_int32CompiledMethod);
            break;
         case TR::Int64:
            _int64Compilee = &functionCompilee;
            _int64CompiledMethod = (signatureCharJ_J_testMethodType *) (compileMethod(functionDetails, warm, returnCode));
            functionCompilee.setEntryPoint((void *)_int64CompiledMethod);
            break;
         case TR::Double:
            _doubleCompilee = &functionCompilee;
            _doubleCompiledMethod = (signatureCharD_D_testMethodType *) (compileMethod(functionDetails, warm, returnCode));
            functionCompilee.setEntryPoint((void *)_doubleCompiledMethod);
            break;
         case TR::Float:
            _floatCompilee = &functionCompilee;
            _floatCompiledMethod = (signatureCharF_F_testMethodType *) (compileMethod(functionDetails, warm, returnCode));
            functionCompilee.setEntryPoint((void *)_floatCompiledMethod);
            break;
         case TR::Address:
            _addressCompilee = &functionCompilee;
            _addressCompiledMethod = (signatureCharL_L_testMethodType *) (compileMethod(functionDetails, warm, returnCode));
            functionCompilee.setEntryPoint((void *)_addressCompiledMethod);
            break;
         default:
            TR_ASSERT(0, "compilee dataType should be int32, int64, double, float or address");
         }
      EXPECT_TRUE(COMPILATION_SUCCEEDED == returnCode || COMPILATION_REQUESTED == returnCode)
         << "Compiling callee method " << compileeResolvedMethodName << " failed unexpectedly";

      CallIlInjector callIlInjector(&types, this, opCode);
      TR::ResolvedMethod callCompilee(__FILE__, LINETOSTR(__LINE__), testResolvedMethodName, opCodeArgsNum, argIlTypes, types.PrimitiveType(returnType), 0, &callIlInjector);
      TR::IlGeneratorMethodDetails callDetails(&callCompilee);
      uint8_t *startPC = compileMethod(callDetails, warm, returnCode);
      EXPECT_TRUE(COMPILATION_SUCCEEDED == returnCode || COMPILATION_REQUESTED == returnCode)
         << "Compiling test method " << testResolvedMethodName << " failed unexpectedly";
      resultpointer = reinterpret_cast<functiontype>(startPC);
      return returnCode;;
      }

   void
   addUnsupportedOpCodeTest(int32_t opCodeArgsNum,
         TR::ILOpCodes opCode,
         char * resolvedMethodName,
         TR::DataType * argTypes,
         TR::DataType returnType);

   static TR::ResolvedMethod * resolvedMethod(TR::DataType dataType);

   //number of arguments for datatypes
   static const int32_t RESOLVED_METHOD_NAME_LENGTH = 50;
   static const int32_t _numberOfUnaryArgs = 1;
   static const int32_t _numberOfBinaryArgs = 2;
   static const int32_t _numberOfSelectArgs = 3;

   //commonly used variables
   static const int64_t LONG_NEG;
   static const int64_t LONG_POS;
   static const int64_t LONG_MAXIMUM;
   static const int64_t LONG_MINIMUM;
   static const int64_t LONG_ZERO;

   static const int32_t INT_NEG;
   static const int32_t INT_POS;
   static const int32_t INT_MAXIMUM;
   static const int32_t INT_MINIMUM;
   static const int32_t INT_ZERO;

   static const int16_t SHORT_NEG;
   static const int16_t SHORT_POS;
   static const int16_t SHORT_MAXIMUM;
   static const int16_t SHORT_MINIMUM;
   static const int16_t SHORT_ZERO;

   static const int8_t BYTE_NEG;
   static const int8_t BYTE_POS;
   static const int8_t BYTE_MAXIMUM;
   static const int8_t BYTE_MINIMUM;
   static const int8_t BYTE_ZERO;

   static const double DOUBLE_MINIMUM;
   static const double DOUBLE_MAXIMUM;
   static const double DOUBLE_POS;
   static const double DOUBLE_NEG;
   static const double DOUBLE_ZERO;

   static const float FLOAT_MINIMUM;
   static const float FLOAT_MAXIMUM;
   static const float FLOAT_POS;
   static const float FLOAT_NEG;
   static const float FLOAT_ZERO;

   static const uint8_t UBYTE_POS;
   static const uint8_t UBYTE_MAXIMUM;
   static const uint8_t UBYTE_MINIMUM;

   static const uint16_t USHORT_POS;
   static const uint16_t USHORT_MAXIMUM;
   static const uint16_t USHORT_MINIMUM;

   static const uint32_t UINT_POS;
   static const uint32_t UINT_MAXIMUM;
   static const uint32_t UINT_MINIMUM;

   static const uint64_t ULONG_POS;
   static const uint64_t ULONG_MAXIMUM;
   static const uint64_t ULONG_MINIMUM;

   static const int8_t BYTE_PLACEHOLDER_1;
   static const int8_t BYTE_PLACEHOLDER_2;
   static const int8_t BYTE_PLACEHOLDER_3;

   static const int16_t SHORT_PLACEHOLDER_1;
   static const int16_t SHORT_PLACEHOLDER_2;
   static const int16_t SHORT_PLACEHOLDER_3;

   static const int32_t INT_PLACEHOLDER_1;
   static const int32_t INT_PLACEHOLDER_2;
   static const int32_t INT_PLACEHOLDER_3;

   static const int64_t LONG_PLACEHOLDER_1;
   static const int64_t LONG_PLACEHOLDER_2;
   static const int64_t LONG_PLACEHOLDER_3;

   static const float FLOAT_PLACEHOLDER_1;
   static const float FLOAT_PLACEHOLDER_2;
   static const float FLOAT_PLACEHOLDER_3;

   static const double DOUBLE_PLACEHOLDER_1;
   static const double DOUBLE_PLACEHOLDER_2;
   static const double DOUBLE_PLACEHOLDER_3;

   static const uintptr_t ADDRESS_PLACEHOLDER_1;
   static const uintptr_t ADDRESS_PLACEHOLDER_2;
   static const uintptr_t ADDRESS_PLACEHOLDER_3;

   protected:
   static signatureCharI_I_testMethodType *_iByteswap;
   //Neg
   static signatureCharB_B_testMethodType *_bNeg;
   static signatureCharS_S_testMethodType *_sNeg;
   static signatureCharI_I_testMethodType *_iNeg;
   static signatureCharJ_J_testMethodType *_lNeg;
   static signatureCharD_D_testMethodType *_dNeg;
   static signatureCharF_F_testMethodType *_fNeg;

   //convert
   static signatureCharI_J_testMethodType *_i2l;
   static signatureCharI_F_testMethodType *_i2f;
   static signatureCharI_D_testMethodType *_i2d;
   static signatureCharI_B_testMethodType *_i2b;
   static signatureCharI_S_testMethodType *_i2s;
   static unsignedSignatureCharI_J_testMethodType *_iu2l;
   static unsignedSignatureCharI_F_testMethodType *_iu2f;
   static unsignedSignatureCharI_D_testMethodType *_iu2d;

   static signatureCharJ_I_testMethodType *_l2i;
   static signatureCharJ_F_testMethodType *_l2f;
   static signatureCharJ_D_testMethodType *_l2d;
   static signatureCharJ_B_testMethodType *_l2b;
   static signatureCharJ_S_testMethodType *_l2s;
   static unsignedSignatureCharJ_F_testMethodType *_lu2f;
   static unsignedSignatureCharJ_D_testMethodType *_lu2d;

   static signatureCharD_I_testMethodType *_d2i;
   static signatureCharD_J_testMethodType *_d2l;
   static signatureCharD_F_testMethodType *_d2f;
   static signatureCharD_B_testMethodType *_d2b;
   static signatureCharD_S_testMethodType *_d2s;

   static signatureCharF_I_testMethodType *_f2i;
   static signatureCharF_J_testMethodType *_f2l;
   static signatureCharF_D_testMethodType *_f2d;
   static signatureCharF_B_testMethodType *_f2b;
   static signatureCharF_S_testMethodType *_f2s;

   static signatureCharS_I_testMethodType *_s2i;
   static signatureCharS_J_testMethodType *_s2l;
   static signatureCharS_F_testMethodType *_s2f;
   static signatureCharS_D_testMethodType *_s2d;
   static signatureCharS_B_testMethodType *_s2b;
   static unsignedSignatureCharS_I_testMethodType *_su2i;
   static unsignedSignatureCharS_J_testMethodType *_su2l;
   static unsignedSignatureCharS_F_testMethodType *_su2f;
   static unsignedSignatureCharS_D_testMethodType *_su2d;

   static signatureCharB_I_testMethodType *_b2i;
   static signatureCharB_J_testMethodType *_b2l;
   static signatureCharB_F_testMethodType *_b2f;
   static signatureCharB_D_testMethodType *_b2d;
   static signatureCharB_S_testMethodType *_b2s;
   static unsignedSignatureCharB_I_testMethodType *_bu2i;
   static unsignedSignatureCharB_J_testMethodType *_bu2l;
   static unsignedSignatureCharB_F_testMethodType *_bu2f;
   static unsignedSignatureCharB_D_testMethodType *_bu2d;
   static unsignedSignatureCharB_S_testMethodType *_bu2s;

   //Abs
   static signatureCharI_I_testMethodType *_iAbs;
   static signatureCharJ_J_testMethodType *_lAbs;
   static signatureCharD_D_testMethodType *_dAbs;
   static signatureCharF_F_testMethodType *_fAbs;

   //Sqrt
   static signatureCharD_D_testMethodType *_dSqrt;
   static signatureCharF_F_testMethodType *_fSqrt;

   //load
   static signatureCharI_I_testMethodType *_iLoad;
   static signatureCharJ_J_testMethodType *_lLoad;
   static signatureCharD_D_testMethodType *_dLoad;
   static signatureCharF_F_testMethodType *_fLoad;
   static signatureCharB_B_testMethodType *_bLoad;
   static signatureCharS_S_testMethodType *_sLoad;

   //store
   static signatureCharI_I_testMethodType *_iStore;
   static signatureCharJ_J_testMethodType *_lStore;
   static signatureCharB_B_testMethodType *_bStore;
   static signatureCharS_S_testMethodType *_sStore;
   static signatureCharD_D_testMethodType *_dStore;
   static signatureCharF_F_testMethodType *_fStore;

   //return
   static signatureCharI_I_testMethodType *_iReturn;
   static signatureCharJ_J_testMethodType *_lReturn;
   static signatureCharD_D_testMethodType *_dReturn;
   static signatureCharF_F_testMethodType *_fReturn;

   //Integer Arithmetic
   static signatureCharBB_B_testMethodType *_bAdd;
   static signatureCharBB_B_testMethodType *_bSub;
   static signatureCharBB_B_testMethodType *_bMul;
   static signatureCharBB_B_testMethodType *_bDiv;
   static signatureCharBB_B_testMethodType *_bRem;

   static signatureCharSS_S_testMethodType *_sAdd;
   static signatureCharSS_S_testMethodType *_sSub;
   static signatureCharSS_S_testMethodType *_sMul;
   static signatureCharSS_S_testMethodType *_sDiv;
   static signatureCharSS_S_testMethodType *_sRem;

   static signatureCharII_I_testMethodType *_iAdd;
   static signatureCharII_I_testMethodType *_iSub;
   static signatureCharII_I_testMethodType *_iDiv;
   static signatureCharII_I_testMethodType *_iMul;
   static signatureCharII_I_testMethodType *_iMulh;
   static signatureCharII_I_testMethodType *_iRem;
   static unsignedSignatureCharII_I_testMethodType *_iuDiv;
   static unsignedSignatureCharII_I_testMethodType *_iuMul;
   static unsignedSignatureCharII_I_testMethodType *_iuMulh;
   static unsignedSignatureCharII_I_testMethodType *_iuRem;

   static signatureCharJJ_J_testMethodType *_lAdd;
   static signatureCharJJ_J_testMethodType *_lSub;
   static signatureCharJJ_J_testMethodType *_lMul;
   static signatureCharJJ_J_testMethodType *_lDiv;
   static signatureCharJJ_J_testMethodType *_lRem;
   static unsignedSignatureCharJJ_J_testMethodType *_luDiv;

   //Float Arithmetic
   static signatureCharFF_F_testMethodType *_fAdd;
   static signatureCharFF_F_testMethodType *_fSub;
   static signatureCharFF_F_testMethodType *_fMul;
   static signatureCharFF_F_testMethodType *_fDiv;
   static signatureCharFF_F_testMethodType *_fRem;

   //Shift
   static signatureCharII_I_testMethodType *_iShl;
   static signatureCharJJ_J_testMethodType *_lShl;
   static signatureCharII_I_testMethodType *_iShr;
   static signatureCharJJ_J_testMethodType *_lShr;
   static signatureCharSS_S_testMethodType *_sShl;
   static signatureCharBB_B_testMethodType *_bShl;
   static signatureCharSS_S_testMethodType *_sShr;
   static signatureCharBB_B_testMethodType *_bShr;
   static unsignedSignatureCharII_I_testMethodType *_iuShr;
   static unsignedSignatureCharJJ_J_testMethodType *_luShr;
   static unsignedSignatureCharSS_S_testMethodType *_suShr;
   static unsignedSignatureCharBB_B_testMethodType *_buShr;
   static signatureCharII_I_testMethodType *_iRol;
   static signatureCharJJ_J_testMethodType *_lRol;

   //Opcode Tests for TR::Double
   static signatureCharDD_D_testMethodType *_dAdd;
   static signatureCharDD_D_testMethodType *_dSub;
   static signatureCharDD_D_testMethodType *_dDiv;
   static signatureCharDD_D_testMethodType *_dMul;
   static signatureCharDD_D_testMethodType *_dRem;

   //Bitwise
   static signatureCharII_I_testMethodType *_iAnd;
   static signatureCharJJ_J_testMethodType *_lAnd;
   static signatureCharII_I_testMethodType *_iOr;
   static signatureCharJJ_J_testMethodType *_lOr;
   static signatureCharII_I_testMethodType *_iXor;
   static signatureCharJJ_J_testMethodType *_lXor;
   static signatureCharSS_S_testMethodType *_sAnd;
   static signatureCharSS_S_testMethodType *_sOr;
   static signatureCharSS_S_testMethodType *_sXor;
   static signatureCharBB_B_testMethodType *_bAnd;
   static signatureCharBB_B_testMethodType *_bOr;
   static signatureCharBB_B_testMethodType *_bXor;

   //Compare
   static signatureCharII_I_testMethodType *_iCmpeq;
   static signatureCharJJ_I_testMethodType *_lCmpeq;
   static signatureCharDD_I_testMethodType *_dCmpeq;
   static signatureCharFF_I_testMethodType *_fCmpeq;
   static signatureCharSS_I_testMethodType *_sCmpeq;
   static signatureCharBB_I_testMethodType *_bCmpeq;

   static signatureCharII_I_testMethodType *_iCmpne;
   static signatureCharJJ_I_testMethodType *_lCmpne;
   static signatureCharDD_I_testMethodType *_dCmpne;
   static signatureCharFF_I_testMethodType *_fCmpne;
   static signatureCharSS_I_testMethodType *_sCmpne;
   static signatureCharBB_I_testMethodType *_bCmpne;

   static signatureCharII_I_testMethodType *_iCmplt;
   static signatureCharJJ_I_testMethodType *_lCmplt;
   static signatureCharDD_I_testMethodType *_dCmplt;
   static signatureCharFF_I_testMethodType *_fCmplt;
   static signatureCharSS_I_testMethodType *_sCmplt;
   static signatureCharBB_I_testMethodType *_bCmplt;

   static signatureCharII_I_testMethodType *_iCmpgt;
   static signatureCharJJ_I_testMethodType *_lCmpgt;
   static signatureCharDD_I_testMethodType *_dCmpgt;
   static signatureCharFF_I_testMethodType *_fCmpgt;
   static signatureCharSS_I_testMethodType *_sCmpgt;
   static signatureCharBB_I_testMethodType *_bCmpgt;

   static signatureCharII_I_testMethodType *_iCmple;
   static signatureCharJJ_I_testMethodType *_lCmple;
   static signatureCharDD_I_testMethodType *_dCmple;
   static signatureCharFF_I_testMethodType *_fCmple;
   static signatureCharSS_I_testMethodType *_sCmple;
   static signatureCharBB_I_testMethodType *_bCmple;

   static signatureCharII_I_testMethodType *_iCmpge;
   static signatureCharJJ_I_testMethodType *_lCmpge;
   static signatureCharDD_I_testMethodType *_dCmpge;
   static signatureCharFF_I_testMethodType *_fCmpge;
   static signatureCharSS_I_testMethodType *_sCmpge;
   static signatureCharBB_I_testMethodType *_bCmpge;

   static unsignedCompareSignatureCharII_I_testMethodType *_iuCmplt;
   static unsignedCompareSignatureCharII_I_testMethodType *_iuCmpge;
   static unsignedCompareSignatureCharII_I_testMethodType *_iuCmpgt;
   static unsignedCompareSignatureCharII_I_testMethodType *_iuCmple;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_luCmplt;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_luCmpge;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_luCmpgt;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_luCmple;
   static unsignedCompareSignatureCharBB_I_testMethodType *_buCmplt;
   static unsignedCompareSignatureCharBB_I_testMethodType *_buCmpge;
   static unsignedCompareSignatureCharBB_I_testMethodType *_buCmpgt;
   static unsignedCompareSignatureCharBB_I_testMethodType *_buCmple;

   static unsignedCompareSignatureCharSS_I_testMethodType *_suCmplt;
   static unsignedCompareSignatureCharSS_I_testMethodType *_suCmpge;
   static unsignedCompareSignatureCharSS_I_testMethodType *_suCmpgt;
   static unsignedCompareSignatureCharSS_I_testMethodType *_suCmple;

   static signatureCharJJ_I_testMethodType *_lCmp;
   static signatureCharFF_I_testMethodType *_fCmpl;
   static signatureCharFF_I_testMethodType *_fCmpg;
   static signatureCharDD_I_testMethodType *_dCmpl;
   static signatureCharDD_I_testMethodType *_dCmpg;

   //CompareBranch
   static signatureCharII_I_testMethodType *_ifIcmpeq;
   static signatureCharII_I_testMethodType *_ifIcmpne;
   static signatureCharII_I_testMethodType *_ifIcmpgt;
   static signatureCharII_I_testMethodType *_ifIcmplt;
   static signatureCharII_I_testMethodType *_ifIcmpge;
   static signatureCharII_I_testMethodType *_ifIcmple;
   static signatureCharJJ_I_testMethodType *_ifLcmpeq;
   static signatureCharJJ_I_testMethodType *_ifLcmpne;
   static signatureCharJJ_I_testMethodType *_ifLcmpgt;
   static signatureCharJJ_I_testMethodType *_ifLcmplt;
   static signatureCharJJ_I_testMethodType *_ifLcmpge;
   static signatureCharJJ_I_testMethodType *_ifLcmple;
   static signatureCharFF_I_testMethodType *_ifFcmpeq;
   static signatureCharFF_I_testMethodType *_ifFcmpne;
   static signatureCharFF_I_testMethodType *_ifFcmpgt;
   static signatureCharFF_I_testMethodType *_ifFcmplt;
   static signatureCharFF_I_testMethodType *_ifFcmpge;
   static signatureCharFF_I_testMethodType *_ifFcmple;
   static signatureCharDD_I_testMethodType *_ifDcmpeq;
   static signatureCharDD_I_testMethodType *_ifDcmpne;
   static signatureCharDD_I_testMethodType *_ifDcmpgt;
   static signatureCharDD_I_testMethodType *_ifDcmplt;
   static signatureCharDD_I_testMethodType *_ifDcmpge;
   static signatureCharDD_I_testMethodType *_ifDcmple;
   static signatureCharSS_I_testMethodType *_ifScmpeq;
   static signatureCharSS_I_testMethodType *_ifScmpne;
   static signatureCharSS_I_testMethodType *_ifScmpgt;
   static signatureCharSS_I_testMethodType *_ifScmplt;
   static signatureCharSS_I_testMethodType *_ifScmpge;
   static signatureCharSS_I_testMethodType *_ifScmple;
   static signatureCharBB_I_testMethodType *_ifBcmpeq;
   static signatureCharBB_I_testMethodType *_ifBcmpne;
   static signatureCharBB_I_testMethodType *_ifBcmpgt;
   static signatureCharBB_I_testMethodType *_ifBcmplt;
   static signatureCharBB_I_testMethodType *_ifBcmpge;
   static signatureCharBB_I_testMethodType *_ifBcmple;
   static unsignedCompareSignatureCharII_I_testMethodType *_ifIuCmplt;
   static unsignedCompareSignatureCharII_I_testMethodType *_ifIuCmpge;
   static unsignedCompareSignatureCharII_I_testMethodType *_ifIuCmpgt;
   static unsignedCompareSignatureCharII_I_testMethodType *_ifIuCmple;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_ifLuCmplt;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_ifLuCmpge;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_ifLuCmpgt;
   static unsignedCompareSignatureCharJJ_I_testMethodType *_ifLuCmple;
   static unsignedCompareSignatureCharBB_I_testMethodType *_ifBuCmplt;
   static unsignedCompareSignatureCharBB_I_testMethodType *_ifBuCmpge;
   static unsignedCompareSignatureCharBB_I_testMethodType *_ifBuCmpgt;
   static unsignedCompareSignatureCharBB_I_testMethodType *_ifBuCmple;
   static unsignedCompareSignatureCharSS_I_testMethodType *_ifSuCmplt;
   static unsignedCompareSignatureCharSS_I_testMethodType *_ifSuCmpge;
   static unsignedCompareSignatureCharSS_I_testMethodType *_ifSuCmpgt;
   static unsignedCompareSignatureCharSS_I_testMethodType *_ifSuCmple;

   //Select operators
   static signatureCharIBB_B_testMethodType *_bselect;
   static signatureCharISS_S_testMethodType *_sselect;
   static signatureCharIII_I_testMethodType *_iselect;
   static signatureCharIJJ_J_testMethodType *_lselect;
   static signatureCharIFF_F_testMethodType *_fselect;
   static signatureCharIDD_D_testMethodType *_dselect;

   static signatureCharI_I_testMethodType *_int32CompiledMethod;
   static signatureCharJ_J_testMethodType *_int64CompiledMethod;
   static signatureCharD_D_testMethodType *_doubleCompiledMethod;
   static signatureCharF_F_testMethodType *_floatCompiledMethod;
   static signatureCharI_I_testMethodType *_iCall;
   static signatureCharJ_J_testMethodType *_lCall;
   static signatureCharF_F_testMethodType *_fCall;
   static signatureCharD_D_testMethodType *_dCall;

   static signatureCharL_I_testMethodType *_iLoadi;
   static signatureCharL_J_testMethodType *_lLoadi;
   static signatureCharL_D_testMethodType *_dLoadi;
   static signatureCharL_F_testMethodType *_fLoadi;
   static signatureCharL_B_testMethodType *_bLoadi;
   static signatureCharL_S_testMethodType *_sLoadi;
   static signatureCharL_L_testMethodType *_aLoadi;

   static signatureCharLI_I_testMethodType *_iStorei;
   static signatureCharLJ_J_testMethodType *_lStorei;
   static signatureCharLD_D_testMethodType *_dStorei;
   static signatureCharLF_F_testMethodType *_fStorei;
   static signatureCharLB_B_testMethodType *_bStorei;
   static signatureCharLS_S_testMethodType *_sStorei;
   static signatureCharLL_L_testMethodType *_aStorei;

   static TR::ResolvedMethod *_int32Compilee;
   static TR::ResolvedMethod *_int64Compilee;
   static TR::ResolvedMethod *_doubleCompilee;
   static TR::ResolvedMethod *_floatCompilee;
   static TR::ResolvedMethod *_addressCompilee;

   //Address opcodes
   static signatureCharL_L_testMethodType *_addressCompiledMethod;
   static signatureCharL_L_testMethodType *_acall;
   static signatureCharL_L_testMethodType *_aload;
   static signatureCharL_L_testMethodType *_astore;
   static signatureCharL_L_testMethodType *_areturn;
   static signatureCharL_B_testMethodType *_a2b;
   static signatureCharL_S_testMethodType *_a2s;
   static signatureCharL_I_testMethodType *_a2i;
   static signatureCharL_J_testMethodType *_a2l;
   static signatureCharB_L_testMethodType *_b2a;
   static signatureCharS_L_testMethodType *_s2a;
   static signatureCharI_L_testMethodType *_i2a;
   static signatureCharJ_L_testMethodType *_l2a;
   static unsignedSignatureCharB_L_testMethodType *_bu2a;
   static unsignedSignatureCharS_L_testMethodType *_su2a;
   static unsignedSignatureCharI_L_testMethodType *_iu2a;
   static unsignedSignatureCharJ_L_testMethodType *_lu2a;
   static signatureCharLL_I_testMethodType *_acmpeq;
   static signatureCharLL_I_testMethodType *_acmpne;
   static signatureCharLL_I_testMethodType *_acmplt;
   static signatureCharLL_I_testMethodType *_acmpge;
   static signatureCharLL_I_testMethodType *_acmple;
   static signatureCharLL_I_testMethodType *_acmpgt;
   static signatureCharLL_I_testMethodType *_ifacmpeq;
   static signatureCharLL_I_testMethodType *_ifacmpne;
   static signatureCharLL_I_testMethodType *_ifacmplt;
   static signatureCharLL_I_testMethodType *_ifacmpge;
   static signatureCharLL_I_testMethodType *_ifacmple;
   static signatureCharLL_I_testMethodType *_ifacmpgt;
   static signatureCharILL_L_testMethodType *_aselect;

   static TR::DataType _argTypesUnaryByte[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryShort[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryInt[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryLong[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryFloat[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryDouble[_numberOfUnaryArgs];
   static TR::DataType _argTypesUnaryAddress[_numberOfUnaryArgs];

   static TR::DataType _argTypesBinaryByte[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryShort[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryInt[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryLong[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryFloat[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryDouble[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddress[_numberOfBinaryArgs];

   static TR::DataType _argTypesSelectByte[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectShort[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectInt[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectLong[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectFloat[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectDouble[_numberOfSelectArgs];
   static TR::DataType _argTypesSelectAddress[_numberOfSelectArgs];

   static TR::DataType _argTypesBinaryAddressByte[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressShort[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressInt[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressLong[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressFloat[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressDouble[_numberOfBinaryArgs];
   static TR::DataType _argTypesBinaryAddressAddress[_numberOfBinaryArgs];

   // straight C(++) implementations of testMethodType for initial test validation purposes
   uint16_t byteswap(uint16_t a) { return (((a & 0xff) << 8) | ((a & 0xff00) >> 8));}
   uint32_t byteswap(uint32_t a) { return (uint32_t)byteswap(uint16_t(a & 0xffff)) << 16 | (uint32_t)byteswap(uint16_t((a >> 16) & 0xffff)); }
   uint64_t byteswap(uint64_t a) { return (uint64_t)byteswap(uint32_t(a & 0xffffffff)) << 32 | (uint64_t)byteswap(uint32_t((a >> 32) & 0xffffffff)); }
   int16_t byteswap(int16_t a) { return byteswap((uint16_t)a); }
   int32_t byteswap(int32_t a) { return byteswap((uint32_t)a); }
   int64_t byteswap(int64_t a) { return byteswap((uint64_t)a); }

   template <typename T> static T neg(T a) { return -a;}
   template <typename T> static T abs(T a) { return a >= (T) 0 ? a : -a;}

   template <typename T> static T add(T a, T b) { return a + b;}
   template <typename T> static T sub(T a, T b) { return a - b;}
   template <typename T> static T mul(T a, T b) { return a * b;}
   template <typename T> static T div(T a, T b) { return a / b;}
   template <typename T> static T rem(T a, T b) { return a % b;}
   template <typename T> static T imulh(T a, T b) { return (int32_t)((((int64_t)a * (int64_t)b) >> 32) & 0x00000000ffffffff);}
   template <typename T> static T iumulh(T a, T b) { return (uint32_t)((((uint64_t)a * (uint64_t)b) >> 32) & 0x00000000ffffffff);}

   template <typename T> static T txor(T a, T b) { return a ^ b;}
   template <typename T> static T tor(T a, T b) { return a | b;}
   template <typename T> static T tand(T a, T b) { return a & b;}
   template <typename T> static T shl(T a, T b) { return a << b;}
   template <typename T> static T shr(T a, T b) { return a >> b;}
   template <typename T> static T rol(T a, T b)
      {
      bool isOne = false;
      int32_t wordSize = sizeof(a);
      int32_t bitMask = 8;
      while (1 != wordSize)
         {
         bitMask = bitMask << 1;
         wordSize = wordSize >> 1;
         }
      bitMask -= 1;
      int32_t rotateNumber = b & bitMask;
      for (int i = 0; i < rotateNumber; i++)
         {
         if (a < 0)
            {
            isOne = true;
            }
         a = a << 1;
         if (isOne)
            {
            a = a | 1;
            }
         isOne = false;
         }
      return a;
      }
   template <typename T1, typename T2> static T2 convert(T1 a, T2 b) { return (T2) a;}
   template <typename T> static int32_t compareEQ(T a, T b) { return a == b;}
   template <typename T> static int32_t compareNE(T a, T b) { return a != b;}
   template <typename T> static int32_t compareLT(T a, T b) { return a < b;}
   template <typename T> static int32_t compareGE(T a, T b) { return a >= b;}
   template <typename T> static int32_t compareGT(T a, T b) { return a > b;}
   template <typename T> static int32_t compareLE(T a, T b) { return a <= b;}
   template <typename T> static int32_t comparel(T a, T b) { return std::isnan(static_cast<long double>(a)) ? -1 : std::isnan(static_cast<long double>(b)) ? -1 : a > b ? 1 : a == b ? 0 : -1 ; }
   template <typename T> static int32_t compareg(T a, T b) { return std::isnan(static_cast<long double>(a)) ? 1 :  std::isnan(static_cast<long double>(b)) ? 1 : a > b ? 1 : a == b ? 0 : -1 ; }
   template <typename C, typename T> static T select(C a, T b, T c) {return a ? b : c;}
   };

} // namespace TestCompiler
