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
#include "compilerunittest/CompilerUnitTest.hpp"
#include "VectorTestUtils.hpp"

#define MAX(x, y) (x > y) ? x : y
#define ABS(x) (x < 0) ? -x : x

#if defined(TR_TARGET_S390) || defined(TR_TARGET_X86)
class VectorTest : public TRTest::JitWithPortTest {};
#else
class VectorTest : public TRTest::JitTest {};
#endif // TR_TARGET_S390 || TR_TARGET_X86

class ParameterizedBinaryVectorArithmeticTest : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::ILOpCode, TR::VectorLength>> {};

void generateIO(TR::ILOpCode scalarOpcode, TR::VectorLength vl, void *output, void *inputA, void *inputB) {
    TR::ILOpCode vectorOpcode = OMR::ILOpCode::convertScalarToVector(scalarOpcode.getOpCodeValue(), vl);
    TR::DataType elementType = vectorOpcode.getType().getVectorElementType();
    TR::ILOpCode storeOpcode = OMR::ILOpCode::indirectStoreOpCode(elementType);
    TR::ILOpCode loadOpcode = OMR::ILOpCode::indirectLoadOpCode(elementType);
    char inputTrees[1024];

    if (inputB) {
        std::snprintf(inputTrees, sizeof(inputTrees),
                      "(method return=NoType args=[Address, Address, Address]"
                      "  (block"
                      "    (%s"
                      "      (aload parm=0)"
                      "        (%s"
                      "          (%s (aload parm=1))"
                      "          (%s (aload parm=2))))"
                      "    (return)))",
                      storeOpcode.getName(),
                      scalarOpcode.getName(),
                      loadOpcode.getName(),
                      loadOpcode.getName()
        );
    } else {
        std::snprintf(inputTrees, sizeof(inputTrees),
                      "(method return=NoType args=[Address, Address]"
                      "  (block"
                      "    (%s"
                      "      (aload parm=0)"
                      "        (%s"
                      "          (%s (aload parm=1))))"
                      "    (return)))",
                      storeOpcode.getName(),
                      scalarOpcode.getName(),
                      loadOpcode.getName()
        );
    }

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    int numBytes = TRTest::vectorSize(vl);
    int elementSize = TRTest::typeSize(elementType.getDataType());

    int numElements = numBytes / elementSize;

    if (inputB) {
        auto entry_point = compiler.getEntryPoint < void(*)(void * , void *, void *) > ();
        void *aOff = inputA;
        void *bOff = inputB;
        void *outOff = output;

        for (int i = 0; i < numElements; i++) {
            TRTest::generateByType(aOff, elementType, scalarOpcode.isDiv());
            TRTest::generateByType(bOff, elementType, scalarOpcode.isDiv());

            entry_point(outOff, aOff, bOff);

            aOff = static_cast<char *>(aOff) + elementSize;
            bOff = static_cast<char *>(bOff) + elementSize;
            outOff = static_cast<char *>(outOff) + elementSize;
        }
    } else {
        auto entry_point = compiler.getEntryPoint < void(*)(void * , void *) > ();
        void *aOff = inputA;
        void *outOff = output;

        for (int i = 0; i < numElements; i++) {
            TRTest::generateByType(aOff, elementType, scalarOpcode.isDiv());
            entry_point(outOff, aOff);

            aOff = static_cast<char *>(aOff) + elementSize;
            outOff = static_cast<char *>(outOff) + elementSize;
        }
    }
}

void generateAndExecuteVectorTest(TR::ILOpCode vectorOpcode, void *expected, void *inputA, void *inputB, void *inputC) {
    TR::VectorLength vl = (vectorOpcode.isVectorReduction() ? vectorOpcode.getVectorResultDataType() : vectorOpcode.getType()).getVectorLength();
    TR::DataType elementType = vectorOpcode.isVectorReduction() ? vectorOpcode.getType() : vectorOpcode.getType().getVectorElementType();
    TR::DataType vt = TR::DataType::createVectorType(elementType.getDataType(), vl);
    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vt);
    TR::ILOpCode storeOp = vectorOpcode.isVectorReduction() ?  TR::ILOpCode::indirectStoreOpCode(elementType) : TR::ILOpCode::createVectorOpCode(TR::vstorei, vt);

    char type[64];
    char inputTrees[1024];

    std::snprintf(type, sizeof(type), "Vector%i%s", TRTest::vectorSize(vl) * 8, TR::DataType::getName(elementType));

    if (vectorOpcode.expectedChildCount() == 1) {
        std::snprintf(inputTrees, sizeof(inputTrees),
                      "(method return= NoType args=[Address,Address]                   "
                      "  (block                                                        "
                      "     (%s%s  offset=0                                            "
                      "         (aload parm=0)                                         "
                      "            (%s%s                                               "
                      "                 (%s%s (aload parm=1))))                        "
                      "     (return)))                                                 ",

                      storeOp.getName(),
                      vectorOpcode.isVectorReduction() ? "" : type,
                      vectorOpcode.getName(),
                      type,
                      loadOp.getName(),
                      type
        );
    } else if (vectorOpcode.expectedChildCount() == 2) {
        std::snprintf(inputTrees, sizeof(inputTrees),
                      "(method return= NoType args=[Address,Address,Address]           "
                      "  (block                                                        "
                      "     (%s%s  offset=0                                            "
                      "         (aload parm=0)                                         "
                      "            (%s%s                                               "
                      "                 (%s%s (aload parm=1))                          "
                      "                 (%s%s (aload parm=2))))                        "
                      "     (return)))                                                 ",

                      storeOp.getName(),
                      type,
                      vectorOpcode.getName(),
                      type,
                      loadOp.getName(),
                      type,
                      loadOp.getName(),
                      type
        );
    } else if (vectorOpcode.expectedChildCount() == 3) {
        std::snprintf(inputTrees, sizeof(inputTrees),
                      "(method return= NoType args=[Address,Address,Address,Address]   "
                      "  (block                                                        "
                      "     (%s%s  offset=0                                            "
                      "         (aload parm=0)                                         "
                      "            (%s%s                                               "
                      "                 (%s%s (aload parm=1))                          "
                      "                 (%s%s (aload parm=2))                          "
                      "                 (%s%s (aload parm=3))))                        "
                      "     (return)))                                                 ",

                      storeOp.getName(),
                      type,
                      vectorOpcode.getName(),
                      type,
                      loadOp.getName(),
                      type,
                      loadOp.getName(),
                      type,
                      loadOp.getName(),
                      type
        );
    }

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    uint8_t result[64];
    if (vectorOpcode.expectedChildCount() == 1) {
        auto entry_point = compiler.getEntryPoint < void(*)(void *, void *) > ();
        entry_point(result, inputA);
    } else if (vectorOpcode.expectedChildCount() == 2){
        auto entry_point = compiler.getEntryPoint < void(*)(void *, void *, void *) > ();
        entry_point(result, inputA, inputB);
    } else if (vectorOpcode.expectedChildCount() == 3) {
        auto entry_point = compiler.getEntryPoint < void(*)(void *, void *, void *, void *) > ();
        entry_point(result, inputA, inputB, inputC);
    }

    TRTest::compareResults(expected, result, elementType.getDataType(), vl, vectorOpcode.isVectorReduction());
}

TEST_P(ParameterizedBinaryVectorArithmeticTest, VLoadStore) {
    TR::ILOpCode scalarOpcode = std::get<0>(GetParam());
    TR::VectorLength vl = std::get<1>(GetParam());
    TR::DataTypes et = scalarOpcode.getType().getDataType();

    SKIP_IF(vl > TR::NumVectorLengths, MissingImplementation) << "Vector length is not supported by the target platform";

    TR::ILOpCode vectorOpcode = OMR::ILOpCode::convertScalarToVector(scalarOpcode.getOpCodeValue(), vl);
    ASSERT_NE(TR::BadILOp, vectorOpcode.getOpCodeValue());
    TR::DataType elementType = vectorOpcode.getType().getVectorElementType();
    TR::DataType vt = TR::DataType::createVectorType(et, vl);
    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vt);
    TR::ILOpCode storeOp = TR::ILOpCode::createVectorOpCode(TR::vstorei, vt);

    char type[64];
    std::snprintf(type, sizeof(type), "Vector%i%s", TRTest::vectorSize(vl) * 8, TR::DataType::getName(elementType));

    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    bool platformSupport = TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, loadOp) &&
                           TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, storeOp) &&
                           TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, vectorOpcode);

    SKIP_IF(!platformSupport, MissingImplementation) << "Opcode " << vectorOpcode.getName() << type << " is not supported by the target platform";

    uint8_t expected[128];
    uint8_t a[128];
    uint8_t b[128];

    generateIO(scalarOpcode, vl, expected, a, vectorOpcode.expectedChildCount() == 1 ? NULL : b);
    generateAndExecuteVectorTest(vectorOpcode, expected, a, b, NULL);
}

#define ALL_VL(opcode) \
    std::make_tuple(opcode, TR::VectorLength128),\
    std::make_tuple(opcode, TR::VectorLength256),\
    std::make_tuple(opcode, TR::VectorLength512)

INSTANTIATE_TEST_CASE_P(VectorArithmetic, ParameterizedBinaryVectorArithmeticTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::ILOpCode, TR::VectorLength>>(
    /* vadd */

    ALL_VL(TR::badd),
    ALL_VL(TR::sadd),
    ALL_VL(TR::iadd),
    ALL_VL(TR::ladd),
    ALL_VL(TR::dadd),
    ALL_VL(TR::fadd),

    /* vsub */

    ALL_VL(TR::bsub),
    ALL_VL(TR::ssub),
    ALL_VL(TR::isub),
    ALL_VL(TR::lsub),
    ALL_VL(TR::fsub),
    ALL_VL(TR::dsub),

    /* vmul */

    ALL_VL(TR::bmul),
    ALL_VL(TR::smul),
    ALL_VL(TR::imul),
    ALL_VL(TR::lmul),
    ALL_VL(TR::fmul),
    ALL_VL(TR::dmul),

    /* vdiv */

    ALL_VL(TR::bdiv),
    ALL_VL(TR::sdiv),
    ALL_VL(TR::idiv),
    ALL_VL(TR::ldiv),
    ALL_VL(TR::fdiv),
    ALL_VL(TR::ddiv),

    /* vand */

    ALL_VL(TR::band),
    ALL_VL(TR::sand),
    ALL_VL(TR::iand),
    ALL_VL(TR::land),

    /* vor */

    ALL_VL(TR::bor),
    ALL_VL(TR::sor),
    ALL_VL(TR::ior),
    ALL_VL(TR::lor),

    /* vxor */

    ALL_VL(TR::bxor),
    ALL_VL(TR::sxor),
    ALL_VL(TR::ixor),
    ALL_VL(TR::lxor),

    /* vmin */

    /* No opcode for bmin, smin */

    ALL_VL(TR::imin),
    ALL_VL(TR::lmin),

    /* fmin is not supported */
    /* dmin is not supported */

    /* vmax */

    /* No opcode for bmax, smax */

    ALL_VL(TR::imax),
    ALL_VL(TR::lmax),

    /* fmax is not supported */
    /* dmax is not supported */

    /* ABS */

    ALL_VL(TR::iabs),
    ALL_VL(TR::labs),
    ALL_VL(TR::fabs),
    ALL_VL(TR::dabs),

    /* vsqrt */

    /* No opcode for bsqrt, ssqrt, isqrt, lsqrt */
    ALL_VL(TR::fsqrt),
    ALL_VL(TR::dsqrt),

    /* vneg */

    ALL_VL(TR::bneg),
    ALL_VL(TR::sneg),
    ALL_VL(TR::ineg),
    ALL_VL(TR::lneg),
    ALL_VL(TR::fneg),
    ALL_VL(TR::dneg)
)));


class ParameterizedVectorTest : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorLength, TR::DataTypes>> {};

TEST_P(ParameterizedVectorTest, VLoadStore) {
    TR::VectorLength vl = std::get<0>(GetParam());
    TR::DataTypes et = std::get<1>(GetParam());

    SKIP_IF(vl > TR::NumVectorLengths, MissingImplementation) << "Vector length is not supported by the target platform";

    TR::DataType vt = TR::DataType::createVectorType(et, vl);

    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vt);
    TR::ILOpCode storeOp = TR::ILOpCode::createVectorOpCode(TR::vstorei, vt);
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);
    bool platformSupport = TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, loadOp) && TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, storeOp);
    SKIP_IF(!platformSupport, MissingImplementation) << "Opcode is not supported by the target platform";

    const size_t inputTreesSize = 1024;
    char inputTrees[inputTreesSize];
    char *formatStr = "(method return= NoType args=[Address,Address]   "
                      "  (block                                        "
                      "     (vstorei%s  offset=0                       "
                      "         (aload parm=0)                         "
                      "         (vloadi%s (aload parm=1)))             "
                      "     (return)))                                 ";

    std::snprintf(inputTrees, inputTreesSize, formatStr, vt.toString(), vt.toString());
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

TEST_P(ParameterizedVectorTest, VSplats) {
    TR::VectorLength vl = std::get<0>(GetParam());
    TR::DataTypes et = std::get<1>(GetParam());

    SKIP_IF(vl > TR::NumVectorLengths, MissingImplementation) << "Vector length is not supported by the target platform";

    TR::DataType vt = TR::DataType::createVectorType(et, vl);

    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vt);
    TR::ILOpCode storeOp = TR::ILOpCode::createVectorOpCode(TR::vstorei, vt);
    TR::ILOpCode splatsOp = TR::ILOpCode::createVectorOpCode(TR::vsplats, vt);
    TR::ILOpCode elementLoadOp = OMR::ILOpCode::indirectLoadOpCode(et);
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);

    bool platformSupport = TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, loadOp) &&
            TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, storeOp) &&
            TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(&cpu, splatsOp);

    SKIP_IF(!platformSupport, MissingImplementation) << "Opcode " << splatsOp.getName() << vt.toString() << " is not supported by the target platform";

    const size_t inputTreesSize = 1024;
    char inputTrees[inputTreesSize];
    char *formatStr = "(method return= NoType args=[Address,Address]   "
                      "  (block                                        "
                      "     (vstorei%s  offset=0                       "
                      "         (aload parm=0)                         "
                      "         (vsplats%s                             "
                      "             (%s (aload parm=1))))              "
                      "     (return)))                                 ";

    std::snprintf(inputTrees, inputTreesSize, formatStr, vt.toString(), vt.toString(), elementLoadOp.getName());

    auto trees = parseString(inputTrees);
    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<void (*)(void *,void *)>();

    const uint8_t maxVectorLength = 64;
    char output[maxVectorLength] = {0};
    char expected[maxVectorLength] = {0};
    char input[maxVectorLength] = {0};

    int etSize = TRTest::typeSize(et);
    int vlSize = TRTest::vectorSize(vl);
    TRTest::generateByType(input, et, false);

    for (int i = 0; i < vlSize; i += etSize) {
        memcpy(expected + i, input, etSize);
    }

    entry_point(output, input);

    EXPECT_EQ(0, memcmp(expected, output, vlSize));
}

INSTANTIATE_TEST_CASE_P(VectorTypeParameters, ParameterizedVectorTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorLength, TR::DataTypes>>(
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

template<typename T, int n>
struct BinaryTestData {

    T expected[n];
    T inputA[n];
    T inputB[n];
};

template<typename T, int n>
struct TernaryTestData {
    T expected[n];
    T inputA[n];
    T inputB[n];
    T inputC[n];
};

typedef BinaryTestData<int8_t, 64> BinaryByteTest;
typedef BinaryTestData<int16_t, 32> BinaryShortTest;
typedef BinaryTestData<int32_t, 16> BinaryIntTest;
typedef BinaryTestData<int64_t, 8> BinaryLongTest;
typedef BinaryTestData<float, 16> BinaryFloatTest;
typedef BinaryTestData<double, 8> BinaryDoubleTest;
typedef TernaryTestData<int8_t, 64> TernaryByteTest;
typedef TernaryTestData<int16_t, 32> TernaryShortTest;
typedef TernaryTestData<int32_t, 16> TernaryIntTest;
typedef TernaryTestData<int64_t, 8> TernaryLongTest;
typedef TernaryTestData<float, 16> TernaryFloatTest;
typedef TernaryTestData<double, 8> TernaryDoubleTest;

void dataDrivenTestEvaluator(TR::VectorOperation operation, TR::VectorLength vl, TR::DataType dt, TR::CPU *cpu, void *expected, void *inputA, void* inputB, void* inputC) {
    SKIP_IF(vl > TR::NumVectorLengths, MissingImplementation) << "Vector length is not supported by the target platform";

    TR::DataType vectorType = TR::DataType::createVectorType(dt.getDataType(), vl);
    TR::ILOpCode vectorOpcode = OMR::ILOpCode::createVectorOpCode(operation, vectorType);

    TR::ILOpCode loadOp = TR::ILOpCode::createVectorOpCode(TR::vloadi, vectorType);
    TR::ILOpCode storeOp = TR::ILOpCode::createVectorOpCode(TR::vstorei, vectorType);
    bool platformSupport = TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(cpu, loadOp) &&
                           TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(cpu, storeOp) &&
                           TR::CodeGenerator::getSupportsOpCodeForAutoSIMD(cpu, vectorOpcode);

    SKIP_IF(!platformSupport, MissingImplementation) << "Opcode " << vectorOpcode.getName() << TR::DataType::getName(vectorType) << " is not supported by the target platform";

    generateAndExecuteVectorTest(vectorOpcode, expected, inputA, vectorOpcode.expectedChildCount() >= 2 ? inputB : NULL,
                                 vectorOpcode.expectedChildCount() == 3 ? inputC : NULL);
}

#define ParameterizedBinaryIOTest(type, testType) \
class BinaryDataDriven##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {};    \
TEST_P(BinaryDataDriven##type##Test, BinaryVector128##type##Test) {                                                                            \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength128, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}                                                                                                                                              \
TEST_P(BinaryDataDriven##type##Test, BinaryVector256##type##Test) {                                                                            \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength256, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}                                                                                                                                              \
TEST_P(BinaryDataDriven##type##Test, BinaryVector512##type##Test) {                                                                            \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength512, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}                                                                                                                                              \
class BinaryDataDriven128##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(BinaryDataDriven128##type##Test, BinaryVector128##type##Test) {                                                                         \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength128, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}                                                                                                                                              \
class BinaryDataDriven256##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(BinaryDataDriven256##type##Test, BinaryVector256##type##Test) {                                                                         \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength256, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}                                                                                                                                              \
class BinaryDataDriven512##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(BinaryDataDriven512##type##Test, BinaryVector512##type##Test) {                                                                         \
    testType data = std::get<1>(GetParam());                                                                                                   \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                      \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength512, TR::type, &cpu, data.expected, data.inputA, data.inputB, NULL);      \
}


#define ParameterizedTernaryIOTest(type, testType) \
class TernaryDataDriven##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {};    \
TEST_P(TernaryDataDriven##type##Test, TernaryVector128##type##Test) {                                                                           \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength128, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}                                                                                                                                               \
TEST_P(TernaryDataDriven##type##Test, TernaryVector256##type##Test) {                                                                           \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength256, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}                                                                                                                                               \
TEST_P(TernaryDataDriven##type##Test, TernaryVector512##type##Test) {                                                                           \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength512, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}                                                                                                                                               \
class TernaryDataDriven128##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(TernaryDataDriven128##type##Test, TernaryVector128##type##Test) {                                                                        \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength128, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}                                                                                                                                               \
class TernaryDataDriven256##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(TernaryDataDriven256##type##Test, TernaryVector256##type##Test) {                                                                        \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength256, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}                                                                                                                                               \
class TernaryDataDriven512##type##Test : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::VectorOperation, testType>> {}; \
TEST_P(TernaryDataDriven512##type##Test, TernaryVector512##type##Test) {                                                                        \
    testType data = std::get<1>(GetParam());                                                                                                    \
    TR::CPU cpu = TR::CPU::detect(privateOmrPortLibrary);                                                                                       \
    dataDrivenTestEvaluator(std::get<0>(GetParam()), TR::VectorLength512, TR::type, &cpu, data.expected, data.inputA, data.inputB, data.inputC);\
}

ParameterizedBinaryIOTest(Int8, BinaryByteTest)
ParameterizedBinaryIOTest(Int16, BinaryShortTest)
ParameterizedBinaryIOTest(Int32, BinaryIntTest)
ParameterizedBinaryIOTest(Int64, BinaryLongTest)
ParameterizedBinaryIOTest(Float, BinaryFloatTest)
ParameterizedBinaryIOTest(Double, BinaryDoubleTest)
ParameterizedTernaryIOTest(Int8, TernaryByteTest)
ParameterizedTernaryIOTest(Int16, TernaryShortTest)
ParameterizedTernaryIOTest(Int32, TernaryIntTest)
ParameterizedTernaryIOTest(Int64, TernaryLongTest)
ParameterizedTernaryIOTest(Float, TernaryFloatTest)
ParameterizedTernaryIOTest(Double, TernaryDoubleTest)

#define FNAN std::numeric_limits<float>::quiet_NaN()
#define DNAN std::numeric_limits<double>::quiet_NaN()
#define FINF std::numeric_limits<float>::infinity()
#define DINF std::numeric_limits<double>::infinity()

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
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    SKIP_ON_S390(MissingImplementation);
    SKIP_ON_S390X(MissingImplementation);

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

TEST_F(VectorTest, VInt8BitSelect) {

   auto inputTrees = "(method return= NoType args=[Address,Address,Address,Address]   "
                     "  (block                                                        "
                     "     (vstoreiVector128Int8 offset=0                             "
                     "         (aload parm=0)                                         "
                     "            (vbitselectVector128Int8                            "
                     "                 (vloadiVector128Int8 (aload parm=1))           "
                     "                 (vloadiVector128Int8 (aload parm=2))           "
                     "                 (vloadiVector128Int8 (aload parm=3))))         "
                     "     (return)))                                                 ";

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    SKIP_ON_RISCV(MissingImplementation);

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;


    auto entry_point = compiler.getEntryPoint<void (*)(int8_t[],int8_t[],int8_t[],int8_t[])>();
    // This test currently assumes 128bit SIMD

    int8_t output[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int8_t falseInput[] =  {8, -3, 62, 56, -108, -13, 114, -100, 69, -80, 6, 104, 67, 78, 12, -72};
    int8_t trueInput[] =  {55, 107, -12, 39, 77, 103, -3, 15, -17, -16, -62, -41, 71, 77, 111, -119};
    int8_t maskInput[] =  {-121, 28, -85, 63, 59, 19, 21, 95, -14, -21, 8, -41, 8, 103, -100, -16};

    entry_point(output,maskInput,trueInput,falseInput);

    // The expected result is a^((a^b)&c), which extracts bits from b if the corresponding bit of c is 1.
    for (int i = 0; i < (sizeof(output) / sizeof(*output)); i++) {
        EXPECT_EQ(falseInput[i] ^ ((falseInput[i] ^ trueInput[i]) & maskInput[i]), output[i]);
    }
}

/* 128/256/512-Bit Float tests*/
#if !defined(J9ZOS390) && !defined(AIXPPC)
/* XLC won't accept FNAN/DNAN or 0.0 / 0.0 */
INSTANTIATE_TEST_CASE_P(BinaryFloatNaNTest, BinaryDataDrivenFloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vmin, BinaryFloatTest {
        { FNAN,  0.1, FNAN,  5, FNAN,  0.1, FNAN, 5,  FNAN,  0.1, FNAN,  5, FNAN, 25.5, FNAN,  5},
        { FNAN, 25.5, 15.5, 12, FNAN,  0.1, 15.5, 12, FNAN, 25.5, 15.5, 12, FNAN, 25.5, 15.5, 12},
        {   10,  0.1, FNAN,  5,   10, 25.5, FNAN,  5,   10,  0.1, FNAN,  5,   10, 55.1, FNAN,  5},
    }),
    std::make_tuple(TR::vmax, BinaryFloatTest {
        { FNAN, 25.5, FNAN, 12, FNAN, 25.5, FNAN, 12, FNAN, 25.5, FNAN, 12, FNAN, 55.1, FNAN, 12},
        { FNAN, 25.5, 15.5, 12, FNAN,  0.1, 15.5, 12, FNAN, 25.5, 15.5, 12, FNAN, 25.5, 15.5, 12},
        {   10,  0.1, FNAN,  5,   10, 25.5, FNAN,  5,   10,  0.1, FNAN,  5,   10, 55.1, FNAN,  5},
    })
)));

INSTANTIATE_TEST_CASE_P(FloatAbsTest, BinaryDataDrivenFloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vabs, BinaryFloatTest {
        { 10.5,  10.5, FNAN,  0.5, 100,  100, 50.5, 10e10,  10e10, 0.0,  0.0, FNAN, 1,  1, 0, 0},
        { 10.5, -10.5, FNAN, -0.5, 100, -100, 50.5, 10e10, -10e10, 0.0, -0.0, FNAN, 1, -1, 0, 0},
        { },
    })
)));

INSTANTIATE_TEST_CASE_P(DoubleAbsTest, BinaryDataDrivenDoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vabs, BinaryDoubleTest {
        { FNAN,  10.5, 0.5,  0.5, 100,  100, 50.5, 10e10},
        { FNAN, -10.5, 0.5, -0.5, 100, -100, 50.5, 10e10},
        { },
    })
)));

INSTANTIATE_TEST_CASE_P(TarnaryFloatNaNInfTest, TernaryDataDrivenFloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryFloatTest>>(
    std::make_tuple(TR::vfma, TernaryFloatTest {
        { FNAN, 1, 2,  FINF,  -FINF, 35e35,  FINF, FNAN, FNAN, FNAN, -FINF,  FNAN, FNAN,  FNAN, FNAN, FNAN},
        { FNAN, 0, 1, 35e35,  35e35, 35e35, 25e37,    1,   10, FNAN,  FINF,  FINF, FINF, -FINF,    1,    0},
        {    0, 0, 1, 10e10, -10e10,     1,     1,    2, FNAN, FNAN, -FINF,     1,    0,     1, FNAN,    1},
        {    0, 1, 1,     0,      0,     0, 25e37, FNAN,    1, FNAN,     0, -FINF, FINF,  FINF,    1, FNAN}
    }),
    std::make_tuple(TR::vfma, TernaryFloatTest {
        { FNAN, FNAN, FNAN, -FINF,  FNAN, FNAN,  FNAN, FNAN, FNAN, FNAN, 1, 2,  FINF,  -FINF, 35e35,  FINF},
        {    1,   10, FNAN,  FINF,  FINF, FINF, -FINF,    1,    0, FNAN, 0, 1, 35e35,  35e35, 35e35, 25e37},
        {    2, FNAN, FNAN, -FINF,     1,    0,     1, FNAN,    1,    0, 0, 1, 10e10, -10e10,     1,     1},
        { FNAN,    1, FNAN,     0, -FINF, FINF,  FINF,    1, FNAN,    0, 1, 1,     0,      0,     0, 25e37}
    }),
    std::make_tuple(TR::vfma, TernaryFloatTest {
        {   FINF, FNAN, FNAN,  FNAN},
        {  35e35,    1, FINF, -FINF},
        {  10e10,    2,    0,     1},
        {      0, FNAN, FINF,  FINF}
    }),
    std::make_tuple(TR::vfma, TernaryFloatTest {
        {   FINF, FNAN, -FINF,  FNAN},
        {  35e35,    1,  FINF,  FINF},
        {  10e10,    2, -FINF,     1},
        {      0, FNAN,     0, -FINF}
    })
)));

INSTANTIATE_TEST_CASE_P(Float512NaNReductionTest, BinaryDataDriven512FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, FNAN}, {} })
)));

INSTANTIATE_TEST_CASE_P(Double512NaNReductionTest, BinaryDataDriven512DoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { DNAN, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, DNAN, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, DNAN, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, DNAN, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, 2, DNAN, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, 2, 2, DNAN, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, 2, 2, 2, DNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, 2, 2, 2, 2, DNAN}, {} })
)));

INSTANTIATE_TEST_CASE_P(Float256NaNReductionTest, BinaryDataDriven256FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { FNAN, 2, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, FNAN, 2, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, FNAN, 2, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, FNAN, 2, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, FNAN, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, FNAN, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, FNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, 2, 2, 2, 2, FNAN}, {} })
)));

INSTANTIATE_TEST_CASE_P(Double256NaNReductionTest, BinaryDataDriven256DoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { DNAN, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, DNAN, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2,DNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, 2, 2, DNAN}, {} })
)));

INSTANTIATE_TEST_CASE_P(Float128NaNInfReductionTest, BinaryDataDriven128FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { FNAN, 2, 4, 2}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { 2, FNAN, 4, 5}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { 2, 3, FNAN, 5}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { 2, 3, 4, FNAN}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FINF }, { 25e37, 25e37, 25e37, 25e37}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { FINF, 10, 100, -FINF}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { 1, FINF, 1, -FINF}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { FNAN }, { FINF, 2, -FINF, 4}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, FNAN, 5, FNAN}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { FNAN, 2, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, FNAN, 2, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, FNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { FNAN }, { 2, 2, 2, FNAN}, {} })
)));

INSTANTIATE_TEST_CASE_P(Double128NaNInfReductionTest, BinaryDataDriven128DoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { DNAN }, { DNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { DNAN }, { 2, DNAN}, {} }),
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { DINF }, { 25e200, 25e200}, {} }),
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { -DINF }, { DINF, -DINF}, {} }),
    std::make_tuple(TR::vreductionAdd, BinaryDoubleTest { { DNAN }, { DINF, -DINF}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { DNAN, 2}, {} }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { DNAN }, { 2, DNAN}, {} })
)));
#endif

INSTANTIATE_TEST_CASE_P(BinaryFloatTest, BinaryDataDrivenFloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vmin, BinaryFloatTest {
        {  -20, -0, 15.5,  5, 10.5,  0.1,  9.5, 5,   4,  0.1, 0,  5, -1,  0,  10,  5},
        {  -20,  0, 15.5, 12, 22.1,  0.1, 15.5, 12,  4, 25.5, 0, 12,  1,  1,  10, 12},
        {   10, -0, 21.5,  5, 10.5, 25.5,  9.5,  5, 10,  0.1, 0,  5, -1,  0, 100,  5},
    }),
    std::make_tuple(TR::vmax, BinaryFloatTest {
        {   10,  0, 21.5, 12, 22.1, 25.5, 15.5, 12, 10, 25.5, 0, 12,  1,  1, 100, 12},
        {  -20,  0, 15.5, 12, 22.1,  0.1, 15.5, 12,  4, 25.5, 0, 12,  1,  1,  10, 12},
        {   10, -0, 21.5,  5, 10.5, 25.5,  9.5,  5, 10,  0.1, 0,  5, -1,  0, 100,  5},
    })
)));


/* 128/256/512-Bit Double tests*/
#if !defined(J9ZOS390) && !defined(AIXPPC)
INSTANTIATE_TEST_CASE_P(BinaryDoubleNaNTest, BinaryDataDrivenDoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vmin, BinaryDoubleTest {
        { DNAN,  0.1, DNAN,  5, DNAN,  0.1, DNAN, 5},
        { DNAN, 25.5, 15.5, 12, DNAN,  0.1, 15.5, 12},
        {   10,  0.1, DNAN,  5,   10, 25.5, DNAN,  5},
    }),
    std::make_tuple(TR::vmax, BinaryDoubleTest {
        { DNAN, 25.5, DNAN, 12, DNAN, 25.5, DNAN, 12},
        { DNAN, 25.5, 15.5, 12, DNAN,  0.1, 15.5, 12},
        {   10,  0.1, DNAN,  5,   10, 25.5, DNAN,  5},
    })
)));
INSTANTIATE_TEST_CASE_P(TarnaryDoubleNaNInfTest, TernaryDataDrivenDoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryDoubleTest>>(
    std::make_tuple(TR::vfma, TernaryDoubleTest {
        { DNAN, 1, 2,   DINF,   -DINF, 35e35, 40e37, DNAN},
        { DNAN, 0, 1, 35e200,  35e200, 35e35, 20e37,    1},
        {    0, 0, 1, 10e200, -10e200,     1,     1,    2},
        {    0, 1, 1,      0,       0,     0, 20e37, DNAN}
    }),
    std::make_tuple(TR::vfma, TernaryDoubleTest {
        { DNAN, DNAN, -DINF,  DNAN, DNAN,  DNAN, DNAN, DNAN},
        {   10, DNAN,  DINF,  DINF, DINF, -DINF,    1,    0},
        { DNAN, DNAN, -DINF,     1,    0,     1, DNAN,    1},
        {    1, DNAN,     0, -DINF, DINF,  DINF,    1, DNAN}
    }),
    std::make_tuple(TR::vfma, TernaryDoubleTest {
        { DNAN,  DNAN, DNAN, DNAN, DNAN, DNAN, -DINF,  DNAN},
        { DINF, -DINF,    1,    0,   10, DNAN,  DINF,  DINF},
        {    0,     1, DNAN,    1, DNAN, DNAN, -DINF,     1},
        { DINF,  DINF,    1, DNAN,    1, DNAN,     0, -DINF}
    })
)));
#endif

INSTANTIATE_TEST_CASE_P(BinaryDoubleTest, BinaryDataDrivenDoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vmin, BinaryDoubleTest {
        { -0,  0.1, 0.01,  5, -0.00,  0.1, 15.5, 5},
        { -0, 25.5, 15.5, 12,   0.0,  0.1, 15.5, 12},
        {  0,  0.1, 0.01,  5, -0.00, 25.5, 2222,  5},
    }),
    std::make_tuple(TR::vmax, BinaryDoubleTest {
        {  0, 25.5, 15.5, 12,  0, 25.5,  10, 12},
        { -0, 25.5, 15.5, 12,  0,  0.1,  10, 12},
        {  0,  0.1, 10.5,  5, -0, 25.5, -10,  5},
    })
)));

INSTANTIATE_TEST_CASE_P(TarnaryFloatTest, TernaryDataDrivenFloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryFloatTest>>(
    std::make_tuple(TR::vfma, TernaryFloatTest {
        { 110, 5, 65, 2},
        {  10, 0,  8, 1},
        {  10, 0,  8, 1},
        {  10, 5,  1, 1}
    })
)));

INSTANTIATE_TEST_CASE_P(TarnaryDoubleTest, TernaryDataDrivenDoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryDoubleTest>>(
    std::make_tuple(TR::vfma, TernaryDoubleTest {
        { 110, 5},
        {  10, 0},
        {  10, 0},
        {  10, 5}
    }),
    std::make_tuple(TR::vfma, TernaryDoubleTest {
        { 65, 2},
        {  8, 1},
        {  8, 1},
        {  1, 1}
    })
)));

INSTANTIATE_TEST_CASE_P(Float512ReductionTest, BinaryDataDriven512FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { 116 }, { 1, 1, 1, 1, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 16}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Float256ReductionTest, BinaryDataDriven256FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { 20 }, { 1, 1, 1, 1, 4, 4, 4, 4}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Float128ReductionTest, BinaryDataDriven128FloatTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryFloatTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryFloatTest { { 10 },  { 1, 2, 3, 4}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryFloatTest { { 32 },  { 2, 1, 4, 4}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { -14 }, { 145, -14, 600, 500}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryFloatTest { { -60 }, { -17, -14, -60, -50}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryFloatTest { { 600 }, { -145, 14, 600, 500}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryFloatTest { { -60 }, { -660, -6000, -601, -60}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Double128ReductionTest, BinaryDataDriven128DoubleTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryDoubleTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryDoubleTest { { 300.5 }, { 100.5, 200}, {}, }),
    std::make_tuple(TR::vreductionAdd, BinaryDoubleTest { { 0.5 },   { -1, 1.5}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { 15 },    { 10, 1.5}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryDoubleTest { { -15 },   { -10, 1.5}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { 14 },    { 14, 145}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { 14 },    { 145, 14}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryDoubleTest { { -145 },  { -145, -14}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryDoubleTest { { 600 },   { 600, 14}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryDoubleTest { { 600 },   { 14, 600}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryDoubleTest { { -1 },    { -1, -2}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Byte128ReductionTest, BinaryDataDriven128Int8Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryByteTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryByteTest { { 18 }, { 1, 2, 3, 4, 1, 1, 1, -1, 1, 1, 1, -1, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryByteTest { { 32 }, { 2, 1, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryByteTest { { -32 }, { 2, 1, 4, 4, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionAnd, BinaryByteTest { { 0 }, { 32, 16, 8, 5, 4, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, {}, }),
    std::make_tuple(TR::vreductionOr,  BinaryByteTest { { 63 }, { 32, 16, 8, 5, 4, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryByteTest { { 0 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryByteTest { { 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryByteTest { { -59 }, { 2, -1, 2, 4, 0, 1, -6, 10, -59, 2, 100, 100, 55, 10, 20, 2}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryByteTest { { 100 }, { 2, 1, 2, 4, 0, 1, -6, 10, 59, 2, -100, 100, -55, 10, 20, 2}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Short128ReductionTest, BinaryDataDriven128Int16Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryShortTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryShortTest { { 6 },  { 1, 2, 3, -4, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryShortTest { { 32 }, { 2, 1, 4, 4, 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionAnd, BinaryShortTest { { 0 },  { 32, 16, 8, 5, 4, 3, 1, 1 }, {}, }),
    std::make_tuple(TR::vreductionOr,  BinaryShortTest { { 63 }, { 32, 16, 8, 5, 4, 3, 1, 1 }, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryShortTest { { 0 },  { 1, 1, 1, 1, 1, 1, 1, 1 }, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryShortTest { { 1 },  { 1, 1, 1, 1, 1, 1, 1, 0}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryShortTest { { 0 },  { 2, 1, 2, 4, 0, 1, 6, 10}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryShortTest { { -1 }, { 2, 1, 2, 4, 0, -1, 6, 10}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryShortTest { { 10 }, { 2, 1, 2, 4, 0, 1, 6, 10}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryShortTest { { 10 }, { 2, -1, 2, -4, 0, 10, -6, 9}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Int128ReductionTest, BinaryDataDriven128Int32Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryIntTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryIntTest { { 8 },  { -1, 2, 3, 4}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryIntTest { { 32 }, { 2, 1, 4, 4}, {}, }),
    std::make_tuple(TR::vreductionAnd, BinaryIntTest { { 0 },  { 32, 16, 8, 5}, {}, }),
    std::make_tuple(TR::vreductionOr,  BinaryIntTest { { 61 }, { 32, 16, 8, 5}, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryIntTest { { 0 },  { 1, 1, 1, 1}, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryIntTest { { 1 },  { 1, 1, 1, 0}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryIntTest { { 0 },  { 2, 1, 2, 0}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryIntTest { { -1},  { 2, -1, 2, 0}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryIntTest { { 10 }, { 2, 10, 2, 4}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryIntTest { { -2 }, { -2, -10, -2, -4}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Long128ReductionTest, BinaryDataDriven128Int64Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryLongTest>>(
    std::make_tuple(TR::vreductionAdd, BinaryLongTest { { 11 }, { 5, 6}, {}, }),
    std::make_tuple(TR::vreductionAdd, BinaryLongTest { { -1 }, { 5, -6}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryLongTest { { 24 }, { 12, 2}, {}, }),
    std::make_tuple(TR::vreductionMul, BinaryLongTest { { 24 }, { -12, -2}, {}, }),
    std::make_tuple(TR::vreductionAnd, BinaryLongTest { { 0 }, { 32, 16 }, {}, }),
    std::make_tuple(TR::vreductionAnd, BinaryLongTest { { 32 }, { 32, 32 }, {}, }),
    std::make_tuple(TR::vreductionOr,  BinaryLongTest { { 48 }, { 32, 16 }, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryLongTest { { 3 }, { 2, 1}, {}, }),
    std::make_tuple(TR::vreductionXor, BinaryLongTest { { 0 }, { 1, 1}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryLongTest { { -1 }, { -1, 12}, {}, }),
    std::make_tuple(TR::vreductionMin, BinaryLongTest { { 9223372036854775801 }, { 9223372036854775801, 9223372036854775804}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryLongTest { { 100 }, { 100, -100}, {}, }),
    std::make_tuple(TR::vreductionMax, BinaryLongTest { { 9223372036854775804 }, { 9223372036854775801, 9223372036854775804}, {}, })
)));

INSTANTIATE_TEST_CASE_P(Short128ShiftTest, BinaryDataDriven128Int16Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryShortTest>>(
    std::make_tuple(TR::vshl,  BinaryShortTest { { 2, 2, 16, 320  }, { 1, 2, 4, 5 }, { 1, 0, 2, 70  }, }),
    std::make_tuple(TR::vshl,  BinaryShortTest { { 1, 2, 4, 5     }, { 1, 2, 4, 5 }, { 0, 0, 0, 16  }, }),
    std::make_tuple(TR::vshr,  BinaryShortTest { { 1, 4, 0, 4     }, { 2, 4, 8, 9 }, { 1, 0, 10, 1  }, }),
    std::make_tuple(TR::vshr,  BinaryShortTest { { 2, 4, 8, 9     }, { 2, 4, 8, 9 }, { 0, 0, 16, 0  }, }),
    std::make_tuple(TR::vshr,  BinaryShortTest { { 2, 2, 4, -1    }, { 2, 4, 8, -1}, { 0, 1, 17, 17 }, }),
    std::make_tuple(TR::vushr, BinaryShortTest { { 2, 2, 2, 4     }, { 2, 4, 8, 9 }, { 0, 1, 2, 1   }, }),
    std::make_tuple(TR::vushr, BinaryShortTest { { 2, 4, 8, 9     }, { 2, 4, 8, 9 }, { 0, 0, 16, 0  }, }),
    std::make_tuple(TR::vushr, BinaryShortTest { { 2, 2, 4, 4     }, { 2, 4, 8, 9 }, { 0, 1, 17, 1  }, })
)));

INSTANTIATE_TEST_CASE_P(Short128RotateTest, BinaryDataDriven128Int16Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryShortTest>>(
    std::make_tuple(TR::vrol,  BinaryShortTest { { 7, 4, 8, 8193  }, { 0x7000, 4, 8, 9 }, { 4, 0, 16, -3 }, }),
    std::make_tuple(TR::vrol,  BinaryShortTest { { 1, 2, 4, 5     }, { 1, 2, 4, 5 },      { 0, 0, 0, 16  }, }),
    std::make_tuple(TR::vrol,  BinaryShortTest { { 2, 4, 8, 10    }, { 1, 2, 4, 5 },      { 1, 1, 1, 17  }, })
)));

INSTANTIATE_TEST_CASE_P(Short128MaskedRotateTest, TernaryDataDriven128Int16Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryShortTest>>(
    std::make_tuple(TR::vmrol, TernaryShortTest { { 7, 4, 8, 9 }, { 0x7000, 4, 8, 9 }, { 4, 0, 16, -3 }, { -1, 0, -1, 0 }, }),
    std::make_tuple(TR::vmrol, TernaryShortTest { { 2, 2, 8, 5 }, { 1, 2, 4, 5 },      { 17, 17, 17, 17 }, { -1, 0, -1, 0 }, })
)));

INSTANTIATE_TEST_CASE_P(Int128ShiftTest, BinaryDataDriven128Int32Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryIntTest>>(
    std::make_tuple(TR::vshl,  BinaryIntTest  { { 2, 2, 16, 320   }, { 1, 2, 4, 5 }, { 1, 0, 2, 70  }, }),
    std::make_tuple(TR::vshl,  BinaryIntTest  { { 1, 2, 4, 5      }, { 1, 2, 4, 5 }, { 0, 0, 0, 32  }, }),
    std::make_tuple(TR::vshl,  BinaryIntTest  { { 2, 4, 8, 10     }, { 1, 2, 4, 5 }, { 1, 1, 1, 33  }, }),
    std::make_tuple(TR::vshr,  BinaryIntTest  { { 1, 4, 0, 4      }, { 2, 4, 8, 9 }, { 1, 0, 10, 1  }, }),
    std::make_tuple(TR::vshr,  BinaryIntTest  { { 2, 4, 8, 9      }, { 2, 4, 8, 9 }, { 0, 0, 32, 0  }, }),
    std::make_tuple(TR::vshr,  BinaryIntTest  { { 2, 2, 4, -1     }, { 2, 4, 8, -1}, { 0, 1, 33, 33 }, }),
    std::make_tuple(TR::vushr, BinaryIntTest  { { 2, 2, 2, 4      }, { 2, 4, 8, 9 }, { 0, 1, 2, 1   }, }),
    std::make_tuple(TR::vushr, BinaryIntTest  { { 2, 4, 8, 9      }, { 2, 4, 8, 9 }, { 0, 0, 32, 0  }, }),
    std::make_tuple(TR::vushr, BinaryIntTest  { { 2, 2, 4, 4      }, { 2, 4, 8, 9 }, { 0, 1, 33, 1  }, })
)));

INSTANTIATE_TEST_CASE_P(Int128RotateTest, BinaryDataDriven128Int32Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryIntTest>>(
    std::make_tuple(TR::vrol,  BinaryIntTest  { { 7, 4, 8, 536870913 }, { 0x70000000, 4, 8, 9 }, { 4, 0, 32, -3 }, }),
    std::make_tuple(TR::vrol,  BinaryIntTest  { { 1, 2, 4, 5         }, { 1, 2, 4, 5 },          { 0, 0, 0, 32 }, }),
    std::make_tuple(TR::vrol,  BinaryIntTest  { { 2, 4, 8, 10        }, { 1, 2, 4, 5 },          { 1, 1, 1, 33 }, }),
    std::make_tuple(TR::vrol,  BinaryIntTest  { { 1, 4, 8, -1        }, { 1, 2, 4, -1 },         { 0, 1, 33, 33 }, })
)));

INSTANTIATE_TEST_CASE_P(Int128MaskedRotateTest, TernaryDataDriven128Int32Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryIntTest>>(
    std::make_tuple(TR::vmrol, TernaryIntTest { { 7, 4, 8, 9 }, { 0x70000000, 4, 8, 9 }, { 4, 0, 32, -3 }, { -1, 0, -1, 0 }, }),
    std::make_tuple(TR::vmrol, TernaryIntTest { { 2, 2, 8, 5 }, { 1, 2, 4, 5 },          { 33, 33, 33, 33 }, { -1, 0, -1, 0 }, })
)));

INSTANTIATE_TEST_CASE_P(Long128ShiftTest, BinaryDataDriven128Int64Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryLongTest>>(
    std::make_tuple(TR::vshl,  BinaryLongTest { { 2, 8 },   { 1, 2 },  { 1, 2 }, }),
    std::make_tuple(TR::vshl,  BinaryLongTest { { 1, 2 },   { 1, 2 },  { 64, 64 }, }),
    std::make_tuple(TR::vshl,  BinaryLongTest { { 2, 128 }, { 1, 2 },  { 65, 70 }, }),
    std::make_tuple(TR::vshr,  BinaryLongTest { { 1, 2 },   { 2, 8 },  { 1, 2 }, }),
    std::make_tuple(TR::vshr,  BinaryLongTest { { 2, 8 },   { 2, 8 },  { 64, 64 }, }),
    std::make_tuple(TR::vshr,  BinaryLongTest { { 4, -1 },  { 8, -1 }, { 65, 66 }, }),
    std::make_tuple(TR::vushr, BinaryLongTest { { 1, 2 },   { 2, 8 },  { 1, 2 }, }),
    std::make_tuple(TR::vushr, BinaryLongTest { { 2, 8 },   { 2, 8 },  { 64, 64 }, }),
    std::make_tuple(TR::vushr, BinaryLongTest { { 4, 2 },   { 8, 9 },  { 65, 66 }, })
)));

INSTANTIATE_TEST_CASE_P(Long128RotateTest, BinaryDataDriven128Int64Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, BinaryLongTest>>(
    std::make_tuple(TR::vrol,  BinaryLongTest { { 30064771072, 4  }, { 0x70000000, 4 }, { 4, 0 }, }),
    std::make_tuple(TR::vrol,  BinaryLongTest { { 8, 2305843009213693953 }, { 8, 9 }, { 64, -3 }, }),
    std::make_tuple(TR::vrol,  BinaryLongTest { { 1, 2 }, { 1, 2 }, { 0, 64 }, }),
    std::make_tuple(TR::vrol,  BinaryLongTest { { 2, 4 }, { 1, 2 }, { 1, 65 }, })
)));

INSTANTIATE_TEST_CASE_P(Long128MaskedRotateTest, TernaryDataDriven128Int64Test, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::VectorOperation, TernaryLongTest>>(
    std::make_tuple(TR::vmrol, TernaryLongTest { { 30064771072, 9 }, { 0x70000000, 9 }, { 4, -3 },   { -1, 0 }, }),
    std::make_tuple(TR::vmrol, TernaryLongTest { { 2, 2 },            { 1, 2 },           { 65, 65 },  { -1, 0 }, })
)));

typedef bool (*vconvEqFunc)(void *, void *);

bool eq_f2i(void *pf, void *pi)
{
    return (static_cast<int32_t>(*static_cast<float *>(pf)) == *static_cast<int32_t *>(pi));
}
bool eq_i2f(void *pi, void *pf)
{
    return (static_cast<float>(*static_cast<int32_t *>(pi)) == *static_cast<float *>(pf));
}
bool eq_d2l(void *pd, void *pl)
{
    return (static_cast<int64_t>(*static_cast<double *>(pd)) == *static_cast<int64_t *>(pl));
}
bool eq_l2d(void *pl, void *pd)
{
    return (static_cast<double>(*static_cast<int64_t *>(pl)) == *static_cast<double *>(pd));
}

class ParameterizedVectorConvTest : public VectorTest, public ::testing::WithParamInterface<std::tuple<TR::DataTypes, TR::DataTypes, vconvEqFunc>> {};

TEST_P(ParameterizedVectorConvTest, Vector128ConvTest) {
    // Assuming VectorLength128 and conversion between types of same size only
    TR::DataTypes sourceType = std::get<0>(GetParam());
    TR::DataTypes resultType = std::get<1>(GetParam());
    vconvEqFunc fn = std::get<2>(GetParam());

    int sourceTypeSize = TRTest::typeSize(sourceType);
    int resultTypeSize = TRTest::typeSize(resultType);
    ASSERT_NE(sourceType, resultType);
    ASSERT_EQ(sourceTypeSize, resultTypeSize);

    char sourceTypeName[8];
    char resultTypeName[8];
    char inputTrees[1024];

    std::snprintf(sourceTypeName, sizeof(sourceTypeName), "%s", TR::DataType::getName(sourceType));
    std::snprintf(resultTypeName, sizeof(resultTypeName), "%s", TR::DataType::getName(resultType));
    std::snprintf(inputTrees, sizeof(inputTrees),
                  "(method return= NoType args=[Address,Address] "
                  "  (block                                      "
                  "    (vstoreiVector128%s offset=0              "
                  "      (aload parm=0)                          "
                  "        (vconvVector128%s_Vector128%s         "
                  "          (vloadiVector128%s (aload parm=1))))"
                  "    (return)))                                ",
                  resultTypeName, sourceTypeName, resultTypeName, sourceTypeName);

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);
    SKIP_ON_RISCV(MissingImplementation);
    SKIP_ON_X86(MissingImplementation);
    SKIP_ON_HAMMER(MissingImplementation);
    if (!(sourceType == TR::Int64 && resultType == TR::Double)) {
        SKIP_ON_POWER(MissingImplementation);
        SKIP_ON_S390(MissingImplementation);
        SKIP_ON_S390X(MissingImplementation);
    }

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<void (*)(void *, void *)>();

    int8_t output[16] = { 0 };
    int8_t input[16];

    for (int32_t i = 0; i < sizeof(input)/sourceTypeSize; i++) {
        int32_t offset = sourceTypeSize * i;
        TRTest::generateByType(input + offset, sourceType, false);
    }

    entry_point(output, input);

    for (int32_t i = 0; i < sizeof(input)/sourceTypeSize; i++) {
        int32_t offset = sourceTypeSize * i;
        EXPECT_TRUE(fn(input + offset, output + offset));
    }

}

#if !defined(AIXPPC)
INSTANTIATE_TEST_CASE_P(Vector128Conv, ParameterizedVectorConvTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::DataTypes, TR::DataTypes, vconvEqFunc>>(
    std::make_tuple(TR::Int32, TR::Float, eq_i2f),
    std::make_tuple(TR::Float, TR::Int32, eq_f2i),
    std::make_tuple(TR::Int64, TR::Double, eq_l2d),
    std::make_tuple(TR::Double, TR::Int64, eq_d2l)
)));
#endif /* !defined(AIXPPC) */
