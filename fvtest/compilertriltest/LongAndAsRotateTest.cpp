/*******************************************************************************
 * Copyright IBM Corp. and others 2017
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "JitTest.hpp"
#include "default_compiler.hpp"

// these two arrays hold the value loaded by the iload node.
int32_t i2lInput []  = {0x0, 0x1, 0x0000FFFF, 0x2, 0x3E, (int32_t)0x80000000, (int32_t)0xFFFF0000,
                          0x40000000, 0x7C000000, (int32_t)0xFFFFFFFF, 0x0003C000, 0x40000001};

uint32_t iu2lInput [] = {0x0,0x1,0x0000FFFF,0x2,0x3E,0x80000000,0xFFFF0000,0x40000000,0x7C000000,
                           0xFFFFFFFF,0x0003C000,0x40000001};

// This array consists of all cases where contiguous 1's appear at most once in the binary value
// ex: 001111000, 1111111, 011110, 1110000, 00001111
uint64_t lconstInput_zerosAroundOnes [] =               {   0x7000000000000000,
                                                            0x0,
                                                            0x1,                //00000..00001
                                                            0x00000000FFFFFFFF, //00000..11111
                                                            0x2,                //00000..00010
                                                            0x3E,               //00...0111110    
                                                            0x8000000000000000, //10000000..00
                                                            0xFFFFFFFF00000000, //111111..0000
                                                            0x4000000000000000, //010000..0000
                                                            0x7C00000000000000, //0111100..000
                                                            0xFFFFFFFFFFFFFFFF, //1111111..111
                                                            0x00000003C0000000, //0..011110..0
                                                            0x00007FFFFFFFFFFF,
                                                            0x0000FFFFFFFFFFFF,
                                                            0x000000000000FFFF,
                                                            0xFFFFFFFFFFFE0000,
                                                            0xFFFFFFFFFFFF0000,
                                                            0xFFFF000000000000
                                                        };

// this array is for tests where contiguous zeros only appear once in the binary value.
// ex: 11100001111, 1000001, 0000000
uint64_t lconstInput_onesAroundZeros [] = {0x0, 0x8000000000000001, 0xE000000000000003, 0xFFFFFFF00FFFFFFF,
                                             0xFFFFFFFEFFFFFFFF, 0xFFFFFFFE7FFFFFFF, 0xF0FFFFFFFFFFFFFF,
                                             0xFFFFFFFFFFFFFF0F, 0XFFFF000000000001, 0x8000FFFFFFFFFFFF};

// These input values cannot be handled by a RISBG instruction
uint64_t lconstInput_invalidPatterns [] = {0x4000000000000001, 0xF0F0F0F0F0F0F0F0, 0x8000000000000002};

template <typename VarType, typename ConstType>
class LongAndAsRotateTest : public TRTest::TestWithPortLib, public ::testing::WithParamInterface<std::tuple<VarType, ConstType, ConstType (*) (VarType, ConstType)>>
   {
   public:

   static void SetUpTestCase()
      {
      const char *options = "-Xjit:disableTreeSimplification,disableAsyncCompilation,acceptHugeMethods,enableBasicBlockHoisting,"
                            "omitFramePointer,useIlValidator,paranoidoptcheck";
      auto initSuccess = initializeJitWithOptions(const_cast<char*>(options));

      ASSERT_TRUE(initSuccess) << "Failed to initialize JIT.";
      }

   static void TearDownTestCase()
      {
      shutdownJit();
      }
   };

template <typename VarType, typename ConstType>
struct LongAndAsRotateParamStruct
   {
   VarType variableValue;  //not known at compile time
   ConstType constantValue;  //known at compile time
   ConstType (*oracle)(VarType, ConstType);
   };

template <typename VarType, typename ConstType>
LongAndAsRotateParamStruct<VarType, ConstType> to_struct(const std::tuple<VarType, ConstType, ConstType (*) (VarType, ConstType)> &param)
   {
   LongAndAsRotateParamStruct<VarType, ConstType> s;

   s.variableValue = std::get<0>(param);
   s.constantValue = std::get<1>(param);
   s.oracle = std::get<2>(param);

   return s;
   }

template <typename VarType, typename ConstType>
static ConstType longAndAsRotateOracle(VarType a, ConstType b)
   {
   return ((ConstType)a & b);
   }

class i2lLongAndAsRotateTest : public LongAndAsRotateTest<int32_t, uint64_t> {};

TEST_P(i2lLongAndAsRotateTest, SimpleTest)
   {
   auto param = to_struct(GetParam());

   char inputTrees[512] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int64 args=[Int32]  "
         " (block                            "
         "  (lreturn                         "
         "  (land                            "
         "      (i2l (iload parm=0))         "
         "      (lconst %" OMR_PRIu64 ")))))            ",
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.constantValue), entry_point(param.variableValue));
   }

TEST_P(i2lLongAndAsRotateTest, iConstTest)
   {
   auto param = to_struct(GetParam());

   // this is an arbitrary value that will be used to create
   // a tree where the optimizer will set the highWordZero
   // flag on an i2l node. Since treeSimplification is
   // disabled for these google tests, such a tree is required
   // for basic block extension to set the highWordZero flag.
   const int32_t constValue = 0x4321;

   char inputTrees[512] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int64 args=[Int32]  "
         " (block                            "
         "  (lreturn                         "
         "  (land                            " 
         "      (i2l                         "
         "         (iand                     "
         "            (iload parm=0)         "
         "            (iconst %d)            "
         "         )                         "
         "      )                            " 
         "      (lconst %" OMR_PRIu64 "))))) ",
         constValue,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle((param.variableValue & constValue), param.constantValue), entry_point(param.variableValue)); 

   }

TEST_P(i2lLongAndAsRotateTest, GreaterThanOneRefCount1)
   {
   auto param = to_struct(GetParam());

   // this is an arbitrary value that will be used to create
   // a tree where an i2l node has reference count greater than 1.
   // The actual value doesn't really matter for constLongValue (as long
   // as it's not simple enough to get optimized away).
   const uint64_t constLongValue = 0xFFFFFFFFFFFF0000;

   char inputTrees[2048] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method name=\"AddAndLand\" return=Int64 args=[Int32]"
         "   (block                                            "
         "      (lstore temp=\"x\"                             "
         "         (land                                       "
         "            (i2l id=\"myNode\"                       "
         "               (iload parm=0)                        "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "      (lreturn                                       "
         "         (land                                       "
         "            (@id \"myNode\")                         "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "   )                                                 "
         ")                                                    ",
         constLongValue,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.constantValue), entry_point(param.variableValue)); 

   }

TEST_P(i2lLongAndAsRotateTest, GreaterThanOneRefCount2)
   {
   auto param = to_struct(GetParam());

   // these are arbitrary values that will be used to create
   // a tree where an i2l node has reference count greater than 1.
   // The actual value doesn't really matter for constLongValue and constValue1 
   // (as long as it's not simple enough to get optimized away).
   const uint64_t constLongValue = 0xFFFFFFFFFFFF0000;
   const int32_t constValue1 = 0x4321;
   const int32_t constValue2 = 0x1234;
   
   char inputTrees[2048] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method name=\"AddAndLand\" return=Int64 args=[Int32]"
         "   (block                                            "
         "      (lstore temp=\"x\"                             "
         "         (land                                       "
         "            (i2l                                     "
         "               (iand                                 "
         "                  (iload parm=0 id=\"myNode\")       "
         "                  (iconst %d)                        "
         "               )                                     "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "      (lreturn                                       "
         "         (land                                       "
         "            (i2l                                     "
         "               (iand                                 "                
         "                  (@id \"myNode\")                   "
         "                  (iconst %d)                        "
         "               )                                     "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "   )                                                 "
         ")                                                    ",
         constValue1,
         constLongValue,
         constValue2,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(int32_t)>();

   ASSERT_EQ(param.oracle((param.variableValue & constValue2), param.constantValue), entry_point(param.variableValue));

   }

//iu2l test class
class iu2lLongAndAsRotateTest : public LongAndAsRotateTest<uint32_t, uint64_t> {};

TEST_P(iu2lLongAndAsRotateTest, SimpleTest)
   {
   SKIP_ON_RISCV(KnownBug);

   auto param = to_struct(GetParam());

   char inputTrees[512] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int64 args=[Int32]  "
         " (block                            "
         "  (lreturn                         "
         "  (land                            " 
         "      (iu2l (iload parm=0))        "
         "      (lconst %" OMR_PRIu64 "))))) ",
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.constantValue), entry_point(param.variableValue)); 
   }

TEST_P(iu2lLongAndAsRotateTest, iConstTest)
   {
   SKIP_ON_RISCV(KnownBug);

   auto param = to_struct(GetParam());

   // this is an arbitrary value that will be used to create
   // a tree where the optimizer will set the highWordZero
   // flag on an i2l node. Since treeSimplification is
   // disabled for these google tests, such a tree is required
   // for basic block extension to set the highWordZero flag.
   const uint32_t constValue = 0x4321;

   char inputTrees[512] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method return=Int64 args=[Int32]  "
         " (block                            "
         "  (lreturn                         "
         "  (land                            "
         "      (iu2l                        "
         "         (iand                     "
         "            (iload parm=0)         "
         "            (iconst %u)            "
         "         )                         "
         "      )                            "
         "      (lconst %" OMR_PRIu64 "))))) ",
         constValue,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint32_t)>();

   ASSERT_EQ(param.oracle((param.variableValue & constValue), param.constantValue), entry_point(param.variableValue)); 
   }

TEST_P(iu2lLongAndAsRotateTest, GreaterThanOneRefCount1)
   {
   SKIP_ON_RISCV(KnownBug);

   auto param = to_struct(GetParam());

   // this is an arbitrary value that will be used to create
   // a tree where an i2l node has reference count greater than 1.
   // The actual value doesn't really matter for constLongValue (as long
   // as it's not simple enough to get optimized away).
   const uint64_t constLongValue = 0xFFFFFFFFFFFF0000;

   char inputTrees[2048] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method name=\"AddAndLand\" return=Int64 args=[Int32]"
         "   (block                                            "
         "      (lstore temp=\"x\"                             "
         "         (land                                       "
         "            (iu2l id=\"myNode\"                      "
         "               (iload parm=0)                        "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "      (lreturn                                       "
         "         (land                                       "
         "            (@id \"myNode\")                         "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "   )                                                 "
         ")                                                    ",
         constLongValue,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint32_t)>();

   ASSERT_EQ(param.oracle(param.variableValue, param.constantValue), entry_point(param.variableValue)); 
   }

TEST_P(iu2lLongAndAsRotateTest, GreaterThanOneRefCount2)
   {
   SKIP_ON_RISCV(KnownBug);

   auto param = to_struct(GetParam());

   // these are arbitrary values that will be used to create
   // a tree where an i2l node has reference count greater than 1.
   // The actual value doesn't really matter for constLongValue and constValue1 
   // (as long as it's not simple enough to get optimized away).
   const uint64_t constLongValue = 0xFFFFFFFFFFFF0000;
   const uint32_t constValue1 = 0x4321;
   const uint32_t constValue2 = 0x1234;

   char inputTrees[2048] = {0};
   Tril::format(inputTrees, sizeof(inputTrees),
         "(method name=\"AddAndLand\" return=Int64 args=[Int32]"
         "   (block                                            "
         "      (lstore temp=\"x\"                             "
         "         (land                                       "
         "            (iu2l                                    "
         "               (iand                                 "
         "                  (iload parm=0 id=\"myNode\")       "
         "                  (iconst %u)                        "
         "               )                                     "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "      (lreturn                                       "
         "         (land                                       "
         "            (iu2l                                    "
         "               (iand                                 "               
         "                  (@id \"myNode\")                   "
         "                  (iconst %u)                        "
         "               )                                     "
         "            )                                        "
         "            (lconst %" OMR_PRIu64 ")                 "
         "         )                                           "
         "      )                                              "
         "   )                                                 "
         ")                                                    ",
         constValue1,
         constLongValue,
         constValue2,
         param.constantValue
         );

   auto trees = parseString(inputTrees);

   ASSERT_NOTNULL(trees);

   Tril::DefaultCompiler compiler(trees);

   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\nInput trees: " << inputTrees;

   auto entry_point = compiler.getEntryPoint<uint64_t (*)(uint32_t)>();

   ASSERT_EQ(param.oracle((param.variableValue & constValue2), param.constantValue), entry_point(param.variableValue)); 
   }


// for cases where contiguous ones only appear once in the binary value
// ex: 00011110000, 0111110, 1111111
// i2l case
INSTANTIATE_TEST_CASE_P(i2lZerosAroundOnes, i2lLongAndAsRotateTest,
              ::testing::Combine(
                 ::testing::ValuesIn(i2lInput),
                 ::testing::ValuesIn(lconstInput_zerosAroundOnes),
                 ::testing::Values(static_cast<uint64_t (*) (int32_t, uint64_t)> (longAndAsRotateOracle))));

//iu2l case
INSTANTIATE_TEST_CASE_P(iu2lZerosAroundOnes, iu2lLongAndAsRotateTest,
              ::testing::Combine(
                 ::testing::ValuesIn(iu2lInput),
                 ::testing::ValuesIn(lconstInput_zerosAroundOnes),
                 ::testing::Values(static_cast<uint64_t (*) (uint32_t, uint64_t)> (longAndAsRotateOracle))));


// for cases where contiguous zeros only appear once in the binary value
// ex: 11100001111, 1000001, 0000000
// i2l case
INSTANTIATE_TEST_CASE_P(i2lOnesAroundZeros, i2lLongAndAsRotateTest,
              ::testing::Combine(                                                   
                 ::testing::ValuesIn(i2lInput), 
                 ::testing::ValuesIn(lconstInput_onesAroundZeros),
                 ::testing::Values(static_cast<uint64_t (*) (int32_t, uint64_t)> (longAndAsRotateOracle))));

// iu2l case
INSTANTIATE_TEST_CASE_P(iu2lOnesAroundZeros, iu2lLongAndAsRotateTest,
              ::testing::Combine(                                                   
                 ::testing::ValuesIn(iu2lInput), 
                 ::testing::ValuesIn(lconstInput_onesAroundZeros),
                 ::testing::Values(static_cast<uint64_t (*) (uint32_t, uint64_t)> (longAndAsRotateOracle))));

// none of the tests in this group should generate a RISBG instruction
// i2l case
INSTANTIATE_TEST_CASE_P(i2lInvalidPatterns, i2lLongAndAsRotateTest,
              ::testing::Combine(
                 ::testing::ValuesIn(i2lInput),
                 ::testing::ValuesIn(lconstInput_invalidPatterns),
                 ::testing::Values(static_cast<uint64_t (*) (int32_t, uint64_t)> (longAndAsRotateOracle))));

// iu2l case
INSTANTIATE_TEST_CASE_P(iu2lInvalidPatterns, iu2lLongAndAsRotateTest,
              ::testing::Combine(
                 ::testing::ValuesIn(iu2lInput),
                 ::testing::ValuesIn(lconstInput_invalidPatterns),
                 ::testing::Values(static_cast<uint64_t (*) (uint32_t, uint64_t)> (longAndAsRotateOracle))));
