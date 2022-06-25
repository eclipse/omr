/*******************************************************************************
 * Copyright (c) 2017, 2022 IBM Corp. and others
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

#include "JitTest.hpp"
#include "default_compiler.hpp"
#include "compilerunittest/CompilerUnitTest.hpp"

class VectorTest : public TRTest::JitTest {};

class ParameterizedVectorTest : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorLength, TR::DataTypes>> {};

TEST_P(ParameterizedVectorTest, VLoadStore) {
    TR::VectorLength vl = std::get<0>(GetParam());
    TR::DataTypes et = std::get<1>(GetParam());

    SKIP_IF(vl > TR::NumVectorLengths, MissingImplementation) << "Vector length is not supported by the target platform";
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";

    TR::DataType vt = TR::DataType::createVectorType(et, vl);

    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vt);
    TR::ILOpCode storeOp = TR::ILOpCode::createVectorOpCode(TR::vstorei, vt);
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    bool platformSupport = TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, loadOp) && TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, loadOp);
    SKIP_IF(!platformSupport, MissingImplementation) << "Opcode is not supported by the target platform";

    char inputTrees[1024];
    char *formatStr = "(method return= NoType args=[Address,Address]   "
                      "  (block                                        "
                      "     (vstorei%s  offset=0                       "
                      "         (aload parm=0)                         "
                      "         (vloadi%s (aload parm=1)))             "
                      "     (return)))                                 ";

    sprintf(inputTrees, formatStr, vt.toString(), vt.toString());
    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<void (*)(void *,void *)>();

    const uint8_t maxVectorLength = 64;
    char output[maxVectorLength] = {0};
    char input[maxVectorLength] = {0};
    char zero[maxVectorLength] = {0};

    for (int i = 0; i < maxVectorLength; i++) {
        input[i] = i;
    }

    entry_point(output, input);

    EXPECT_EQ(0, memcmp(input, output, TR::DataType::getSize(vt)));
    EXPECT_EQ(0, memcmp(output + TR::DataType::getSize(vt), zero, maxVectorLength - TR::DataType::getSize(vt)));
}

INSTANTIATE_TEST_SUITE_P(VLoadStoreVectorTest, ParameterizedVectorTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorLength, TR::DataTypes>>(
    std::make_tuple(TR::VectorLength128, TR::Int8),
    std::make_tuple(TR::VectorLength128, TR::Int16),
    std::make_tuple(TR::VectorLength128, TR::Int32),
    std::make_tuple(TR::VectorLength128, TR::Int64),
    std::make_tuple(TR::VectorLength128, TR::Float),
    std::make_tuple(TR::VectorLength128, TR::Double),
    std::make_tuple(TR::VectorLength256, TR::Int8),
    std::make_tuple(TR::VectorLength256, TR::Int16),
    std::make_tuple(TR::VectorLength256, TR::Int32),
    std::make_tuple(TR::VectorLength256, TR::Int64),
    std::make_tuple(TR::VectorLength256, TR::Float),
    std::make_tuple(TR::VectorLength256, TR::Double),
    std::make_tuple(TR::VectorLength512, TR::Int8),
    std::make_tuple(TR::VectorLength512, TR::Int16),
    std::make_tuple(TR::VectorLength512, TR::Int32),
    std::make_tuple(TR::VectorLength512, TR::Int64),
    std::make_tuple(TR::VectorLength512, TR::Float),
    std::make_tuple(TR::VectorLength512, TR::Double)
)));

TEST_F(VectorTest, VDoubleAdd) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Double  offset=0                          "
                     "         (aload parm=0)                                         "
                     "            (vaddVector128Double                                "
                     "                 (vloadiVector128Double (aload parm=1))         "
                     "                 (vloadiVector128Double (aload parm=2))))       "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {1.0, 2.0};
    double inputB[] =  {1.0, 2.0};

    entry_point(output,inputA,inputB);
    EXPECT_DOUBLE_EQ(inputA[0] + inputB[0], output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ(inputA[1] + inputB[1], output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VInt8Add) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vaddVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};
    int8_t inputB[] =  {-14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] + inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt16Add) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int16 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vaddVector128Int16                                 "
                     "                 (vloadiVector128Int16 (aload parm=1))          "
                     "                 (vloadiVector128Int16 (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int16_t[],int16_t[],int16_t[])>();
    // This test currently assumes 128bit SIMD

    int16_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t inputA[] =  {60, 45, 30, 0, -3, -2, -1, 2};
    int16_t inputB[] =  {-5, -10, -1, 13, 15, 10, 7, 5};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] + inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VFloatAdd) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Float offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vaddVector128Float                                 "
                     "                 (vloadiVector128Float (aload parm=1))          "
                     "                 (vloadiVector128Float (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(float[],float[],float[])>();
    // This test currently assumes 128bit SIMD

    float output[] =  {0.0f, 0.0f, 0.0f, 0.0f};
    float inputA[] =  {6.0f, 0.0f, -0.1f, 0.6f};
    float inputB[] =  {-0.5f, 3.5f, 3.0f, 0.7f};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_FLOAT_EQ(inputA[i] + inputB[i], output[i]); // Epsilon = 4ULP -- is this necessary?
    }
}

TEST_F(VectorTest, VInt8Sub) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vsubVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 9};
    int8_t inputB[] =  {14, 12, 10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -12, -14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] - inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt16Sub) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int16 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vsubVector128Int16                                 "
                     "                 (vloadiVector128Int16 (aload parm=1))          "
                     "                 (vloadiVector128Int16 (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int16_t[],int16_t[],int16_t[])>();
    // This test currently assumes 128bit SIMD

    int16_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t inputA[] =  {60, 45, 30, 0, -3, -2, -1, 9};
    int16_t inputB[] =  {5, 10, 1, -13, -15, -10, -7, 2};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] - inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VFloatSub) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Float offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vsubVector128Float                                 "
                     "                 (vloadiVector128Float (aload parm=1))          "
                     "                 (vloadiVector128Float (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(float[],float[],float[])>();
    // This test currently assumes 128bit SIMD

    float output[] =  {0.0f, 0.0f, 0.0f, 0.0f};
    float inputA[] =  {6.0f, 0.0f, -0.1f, 2.0f};
    float inputB[] =  {0.5f, -3.5f, -3.0f, 0.7f};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_FLOAT_EQ(inputA[i] - inputB[i], output[i]); // Epsilon = 4ULP -- is this necessary?
    }
}

TEST_F(VectorTest, VDoubleSub) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Double offset=0                           "
                     "         (aload parm=0)                                         "
                     "            (vsubVector128Double                                "
                     "                 (vloadiVector128Double (aload parm=1))         "
                     "                 (vloadiVector128Double (aload parm=2))))       "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {1.0, -1.5};
    double inputB[] =  {1.1, -3.0};

    entry_point(output,inputA,inputB);
    EXPECT_DOUBLE_EQ(inputA[0] - inputB[0], output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ(inputA[1] - inputB[1], output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VInt8Mul) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vmulVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};
    int8_t inputB[] =  {-14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, -14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] * inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt16Mul) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int16 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vmulVector128Int16                                 "
                     "                 (vloadiVector128Int16 (aload parm=1))          "
                     "                 (vloadiVector128Int16 (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int16_t[],int16_t[],int16_t[])>();
    // This test currently assumes 128bit SIMD

    int16_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t inputA[] =  {60, 45, 30, 0, -3, -2, -1, 2};
    int16_t inputB[] =  {-5, -10, -1, 13, 15, 10, -7, 5};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] * inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VFloatMul) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Float offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vmulVector128Float                                 "
                     "                 (vloadiVector128Float (aload parm=1))          "
                     "                 (vloadiVector128Float (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(float[],float[],float[])>();
    // This test currently assumes 128bit SIMD

    float output[] =  {0.0f, 0.0f, 0.0f, 0.0f};
    float inputA[] =  {6.0f, 0.0f, -0.1f, 0.6f};
    float inputB[] =  {-0.5f, 3.5f, -3.0f, 0.7f};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_FLOAT_EQ(inputA[i] * inputB[i], output[i]); // Epsilon = 4ULP -- is this necessary?
    }
}

TEST_F(VectorTest, VDoubleMul) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Double offset=0                           "
                     "         (aload parm=0)                                         "
                     "            (vmulVector128Double                                "
                     "                 (vloadiVector128Double (aload parm=1))         "
                     "                 (vloadiVector128Double (aload parm=2))))       "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {1.0, -1.5};
    double inputB[] =  {-1.1, -3.0};

    entry_point(output,inputA,inputB);
    EXPECT_DOUBLE_EQ(inputA[0] * inputB[0], output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ(inputA[1] * inputB[1], output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VInt8Div) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstorei type=VectorInt8 offset=0                          "
                     "         (aload parm=0)                                         "
                     "            (vdiv                                               "
                     "                 (vloadi type=VectorInt8 (aload parm=1))        "
                     "                 (vloadi type=VectorInt8 (aload parm=2))))      "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {-128, 32, -96, 99, 35, -88, 45, 100, 17, 86, -28, -100, 71, 80, 15, 2};
    int8_t inputB[] =  {32, 64, -4, 7,15, 11, 9, -25, 5, 43, -5, 7, 3, 10, 4, 2};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] / inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt16Div) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstorei type=VectorInt16 offset=0                         "
                     "         (aload parm=0)                                         "
                     "            (vdiv                                               "
                     "                 (vloadi type=VectorInt16 (aload parm=1))       "
                     "                 (vloadi type=VectorInt16 (aload parm=2))))     "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int16_t[],int16_t[],int16_t[])>();
    // This test currently assumes 128bit SIMD

    int16_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t inputA[] =  {-1024, 32, -30000, 9999, 4096, -8888, 9086, 150};
    int16_t inputB[] =  {32, 2929, -40, 75, 1024, 11, 1, -3};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] / inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt32Div) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstorei type=VectorInt32 offset=0                         "
                     "         (aload parm=0)                                         "
                     "            (vdiv                                               "
                     "                 (vloadi type=VectorInt32 (aload parm=1))       "
                     "                 (vloadi type=VectorInt32 (aload parm=2))))     "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int32_t[],int32_t[],int32_t[])>();
    // This test currently assumes 128bit SIMD

    int32_t output[] =  {0, 0, 0, 0};
    int32_t inputA[] =  {1992385, 32, -788811, 9999};
    int32_t inputB[] =  {779, 2929, -4, 75};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] / inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt64Div) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstorei type=VectorInt64 offset=0                         "
                     "         (aload parm=0)                                         "
                     "            (vdiv                                               "
                     "                 (vloadi type=VectorInt64 (aload parm=1))       "
                     "                 (vloadi type=VectorInt64 (aload parm=2))))     "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int64_t[],int64_t[],int64_t[])>();
    // This test currently assumes 128bit SIMD

    int64_t output[] =  {0, 0, 0, 0};
    int64_t inputA[] =  {(int64_t)0x10ff339955820123L, (int64_t)0xff00295014747555L, -64, 9999};
    int64_t inputB[] =  {(int64_t)0x8000111122223333L, (int64_t)0xffffffff29231233L, 8, 75};

    entry_point(output,inputA,inputB);
    entry_point(&output[2],&inputA[2],&inputB[2]);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] / inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VFloatDiv) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Float offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vdivVector128Float                                 "
                     "                 (vloadiVector128Float (aload parm=1))          "
                     "                 (vloadiVector128Float (aload parm=2))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(float[],float[],float[])>();
    // This test currently assumes 128bit SIMD

    float output[] =  {0.0f, 0.0f, 0.0f, 0.0f};
    float inputA[] =  {6.0f, 0.0f, -9.0f, 0.6f};
    float inputB[] =  {-0.5f, 3.5f, -3.0f, 0.7f};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_FLOAT_EQ(inputA[i] / inputB[i], output[i]); // Epsilon = 4ULP -- is this necessary?
    }
}

TEST_F(VectorTest, VDoubleDiv) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Double offset=0                           "
                     "         (aload parm=0)                                         "
                     "            (vdivVector128Double                                "
                     "                 (vloadiVector128Double (aload parm=1))         "
                     "                 (vloadiVector128Double (aload parm=2))))       "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {12.0, -1.5};
    double inputB[] =  {-4.0, -3.0};

    entry_point(output,inputA,inputB);
    EXPECT_DOUBLE_EQ(inputA[0] / inputB[0], output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ(inputA[1] / inputB[1], output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VInt8And) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vandVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};
    int8_t inputB[] =  {-14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, -14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] & inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt8Or) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vorVector128Int8                                   "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};
    int8_t inputB[] =  {-14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, -14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] | inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt8Xor) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address]           "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vxorVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};
    int8_t inputB[] =  {-14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, -14, 1};

    entry_point(output,inputA,inputB);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(inputA[i] ^ inputB[i], output[i]);
    }
}

TEST_F(VectorTest, VInt8Neg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ((-1) * inputA[i], output[i]);
    }
}

TEST_F(VectorTest, VInt16Neg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int16 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Int16                                 "
                     "                 (vloadiVector128Int16 (aload parm=1))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int16_t[],int16_t[])>();
    // This test currently assumes 128bit SIMD

    int16_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0};
    int16_t inputA[] =  {60, 45, 30, 0, -3, -2, -1, 2};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ((-1) * inputA[i], output[i]);
    }
}

TEST_F(VectorTest, VInt32Neg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int32 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Int32                                 "
                     "                 (vloadiVector128Int32 (aload parm=1))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int32_t[],int32_t[])>();
    // This test currently assumes 128bit SIMD

    int32_t output[] =  {0, 0, 0, 0};
    int32_t inputA[] =  {567890, 1234, 0, -20};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ((-1) * inputA[i], output[i]);
    }
}

TEST_F(VectorTest, VInt64Neg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int64 offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Int64                                 "
                     "                 (vloadiVector128Int64 (aload parm=1))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_POWER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int64_t[],int64_t[])>();
    // This test currently assumes 128bit SIMD

    int64_t output[] =  {0, 0};
    int64_t inputA[] =  {60, -123456};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ((-1) * inputA[i], output[i]);
    }
}

TEST_F(VectorTest, VFloatNeg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Float offset=0                            "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Float                                 "
                     "                 (vloadiVector128Float (aload parm=1))))        "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(float[],float[])>();
    // This test currently assumes 128bit SIMD

    float output[] =  {0.0f, 0.0f, 0.0f, 0.0f};
    float inputA[] =  {6.0f, 0.0f, -9.0f, 0.6f};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ((-1) * inputA[i], output[i]);
    }
}

TEST_F(VectorTest, VDoubleNeg) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Double offset=0                           "
                     "         (aload parm=0)                                         "
                     "            (vnegVector128Double                                "
                     "                 (vloadiVector128Double (aload parm=1))))       "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {12.0, -1.5};

    entry_point(output,inputA);
    EXPECT_DOUBLE_EQ((-1) * inputA[0], output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ((-1) * inputA[1], output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VDoubleSQRT) {

    auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                      "  (block                                                        "
                      "     (vstoreiVector128Double offset=0                           "
                      "         (aload parm=0)                                         "
                      "            (vsqrtVector128Double                               "
                      "                 (vloadiVector128Double (aload parm=1))))       "
                      "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_AARCH64(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(double[],double[])>();
    // This test currently assumes 128bit SIMD

    double output[] =  {0.0, 0.0};
    double inputA[] =  {16.0, 100};

    entry_point(output,inputA);
    EXPECT_DOUBLE_EQ(sqrt(inputA[0]), output[0]); // Epsilon = 4ULP -- is this necessary?
    EXPECT_DOUBLE_EQ(sqrt(inputA[1]), output[1]); // Epsilon = 4ULP -- is this necessary?
}

TEST_F(VectorTest, VInt8Not) {

   auto inputTrees = "(method return= NoType args=[Address,Address]                   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vnotVector128Int8                                  "
                     "                 (vloadiVector128Int8 (aload parm=1))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    //TODO: Re-enable this test on S390 after issue #1843 is resolved.
    SKIP_ON_S390(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_S390X(KnownBug) << "This test is currently disabled on Z platforms because not all Z platforms have vector support (issue #1843)";
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t inputA[] =  {7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, 7};

    entry_point(output,inputA);

    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(~inputA[i], output[i]);
    }
}
