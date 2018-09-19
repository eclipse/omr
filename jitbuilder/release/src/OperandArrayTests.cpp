/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>

#include "Jit.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/VirtualMachineOperandArray.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineRegisterInStruct.hpp"
#include "OperandArrayTests.hpp"

using std::cout;
using std::cerr;

static bool verbose = false;
static int32_t numFailingTests = 0;
static int32_t numPassingTests = 0;
static ARRAYVALUETYPE **verifyArray = NULL;
static ARRAYVALUETYPE expectedResult8 = -1;
static char * result8Operator;

static void
setupResult8Equals()
   {
   expectedResult8 = 9;
   result8Operator = (char *)"==";
   }

static void
setupResult8NotEquals()
   {
   expectedResult8 = 11;
   result8Operator = (char *)"!=";
   }

int
main(int argc, char *argv[])
   {
   if (argc == 2 && strcmp(argv[1], "--verbose") == 0)
      verbose = true;

   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: compile operand array tests set up for equals\n";
   TR::TypeDictionary types2;
   OperandArrayTestMethod pointerMethod(&types2);
   uint8_t *entry2 = 0;
   int32_t rc2 = compileMethodBuilder(&pointerMethod, &entry2);
   if (rc2 != 0)
      {
      cerr << "FAIL: compilation error " << rc2 << "\n";
      exit(-2);
      }

   cout << "Step 3: invoke compiled code and print results\n";
   typedef void (OperandArrayTestMethodFunction)();
   OperandArrayTestMethodFunction *ptrTest = (OperandArrayTestMethodFunction *) entry2;
   verifyArray = pointerMethod.getArrayPtr();
   setupResult8Equals();
   ptrTest();

   cout << "Step 4: compile operand array tests set up for notequals\n";
   TR::TypeDictionary types4;
   OperandArrayTestUsingFalseMethod pointerMethodFalse(&types4);
   uint8_t *entry4 = 0;
   int32_t rc4 = compileMethodBuilder(&pointerMethodFalse, &entry4);
   if (rc4 != 0)
      {
      cerr << "FAIL: compilation error " << rc4 << "\n";
      exit(-4);
      }

   cout << "Step 5: invoke compiled code and print results\n";
   typedef void (OperandArrayTestUsingFalseMethodFunction)();
   OperandArrayTestUsingFalseMethodFunction *ptrTestFalse = (OperandArrayTestUsingFalseMethodFunction *) entry4;
   verifyArray = pointerMethodFalse.getArrayPtr();
   setupResult8NotEquals();
   ptrTestFalse();

   cout << "Step 6: shutdown JIT\n";
   shutdownJit();

   cout << "Number passing tests: " << numPassingTests << "\n";
   cout << "Number failing tests: " << numFailingTests << "\n";

   if (numFailingTests == 0)
      cout << "ALL PASS\n";
   else
      cout << "SOME FAILURES\n";
   }


ARRAYVALUETYPE *OperandArrayTestMethod::_realArray = NULL;
int32_t OperandArrayTestMethod::_realArrayLength = 0;

void
OperandArrayTestMethod::createArray()
   {
   int32_t arraySizeInBytes = _realArrayLength * sizeof(ARRAYVALUETYPE);
   _realArray = (ARRAYVALUETYPE *) malloc(arraySizeInBytes);
   memset(_realArray, 0, arraySizeInBytes);
   }

ARRAYVALUETYPE *
OperandArrayTestMethod::moveArray()
   {
   int32_t stackSizeInBytes = _realArrayLength * sizeof(ARRAYVALUETYPE);
   ARRAYVALUETYPE *newArray = (ARRAYVALUETYPE *) malloc(stackSizeInBytes);
   memcpy(newArray, _realArray, stackSizeInBytes);
   memset(_realArray, 0xFF, stackSizeInBytes);
   free(_realArray);
   _realArray = newArray;

   return _realArray;
   }

void
OperandArrayTestMethod::freeArray()
   {
   memset(_realArray, 0xFF, _realArrayLength * sizeof(ARRAYVALUETYPE));
   free(_realArray);
   _realArray = NULL;
   }

static void Fail()
   {
   numFailingTests++;
   }

static void Pass()
   {
   numPassingTests++;
   }

#define REPORT1(c,n,v)         { if (c) { Pass(); if (verbose) cout << "Pass\n"; } else { Fail(); if (verbose) cout << "Fail: " << (n) << " is " << (v) << "\n"; } }
#define REPORT2(c,n1,v1,n2,v2) { if (c) { Pass(); if (verbose) cout << "Pass\n"; } else { Fail(); if (verbose) cout << "Fail: " << (n1) << " is " << (v1) << ", " << (n2) << " is " << (v2) << "\n"; } }

void
verifyResult0()
   {
   //VMOA      [0   ,null,null,...,null]
   //_realArray[null,null,null,...,null]
   if (verbose) cout << "Set(0, 0); [ no commit ]\n";
   OperandArrayTestMethod::verify("0", 0, 0);
   }

void
verifyResult1()
   {
   //VMOA      [0,null,null,...,null]
   //_realArray[0,null,null,...,null]
   if (verbose) cout << "Commit();\n";
   OperandArrayTestMethod::verify("1", 1, 1, 0);
   }

void
verifyResult2(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1   ,2   ,null,...,null]
   //_realArray[0,null,null,null,...,null]
   if (verbose) cout << "Set(1, 1); Set(2, 2); Get(2)   [ no commit]\n";
   if (verbose) cout << "\tResult 2: last value == 2: ";
   REPORT1(last == 2, "last", last);

   OperandArrayTestMethod::verify("2", 1, 1, 0);
   }

void
verifyResult3(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1,2,null,...,null]
   //_realArray[0,1,2,null,...,null]
   if (verbose) cout << "Commit(); Get(2)\n";
   if (verbose) cout << "\tResult 2: last value == 2: ";
   REPORT1(last == 2, "last", last);

   OperandArrayTestMethod::verify("3", 3, 3, 0, 1, 2);
   }

void
verifyResult4(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1,2,2   ,null,...,null]
   //_realArray[0,1,2,null,null,...,null]
   if (verbose) cout << "Move(3, 2)    [ no commit]\n";
   if (verbose) cout << "\tResult 4: last value == 2: ";
   REPORT1(last == 2, "last", last);

   OperandArrayTestMethod::verify("4", 3, 3, 0, 1, 2);
   }

void
verifyResult5(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1,2,2,null,...,null]
   //_realArray[0,1,2,2,null,...,null]
   if (verbose) cout << "Commit(); Get(3)\n";
   if (verbose) cout << "\tResult 5: last value == 2: ";
   REPORT1(last == 2, "last", last);

   OperandArrayTestMethod::verify("5", 4, 4, 0, 1, 2, 2);
   }

void
verifyResult6(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1,2,2,4,null,...,null]
   //_realArray[0,1,2,2,4,null,...,null]
   if (verbose) cout << "Set(4, Add(GET(2), Get(3))); Commit(); Get(4)\n";
   if (verbose) cout << "\tResult 6: last == 4: ";
   REPORT1(last == 4, "last", last);

   OperandArrayTestMethod::verify("6", 5, 5, 0, 1, 2, 2, 4);
   }

void
verifyResult7()
   {
   //VMOA      [0,1,2,2,4,5,5,null]
   //_realArray[0,1,2,2,4,5,5,null]
   if (verbose) cout << "Set(5,5); Set(6,5); Commit();\n";
   OperandArrayTestMethod::verify("7", 7, 7, 0, 1, 2, 2, 4, 5, 5);
   }

void
verifyResult8(ARRAYVALUETYPE last)
   {
   //VMOA      [0,1,2,{9,11},4,5,5,null]
   //_realArray[0,1,2,{9,11},4,5,5,null]
   if (verbose) cout << "if (5 " << result8Operator << " 5) { SET(3,9); } else { Set(3,11); } Commit(); Get(3);\n";
   if (verbose) cout << "\tResult 8: last == " << expectedResult8 << ": ";
   REPORT1(last == expectedResult8, "last", last);
   OperandArrayTestMethod::verify("8", 7, 7, 0, 1, 2, expectedResult8, 4, 5, 5);
   }

void
verifyResult9(ARRAYVALUETYPE six, ARRAYVALUETYPE seven)
   {
   //VMOA      [0,1,2,{9,11},4,5,22,22]
   //_realArray[0,1,2,{9,11},4,5,5 ,5]
   if (verbose) cout << "Set(7,GET(6)); Commit();\n";
   if (verbose) cout << "Set(7,22); ";
   if (verbose) cout << "Commit();\n";
   if (verbose) cout << "\tResult 9: six == 22: ";
   REPORT1(six == 22, "six", six);
   if (verbose) cout << "\tResult 9: seven == 22: ";
   REPORT1(seven == 22, "seven", seven);
   OperandArrayTestMethod::verify("9", 8, 8, 0, 1, 2, expectedResult8, 4, 5, 22, 22);
   }

void
verifyResult10(ARRAYVALUETYPE one, ARRAYVALUETYPE two)
   {
   //VMOA      [0,7,19,{9,11},4,5,22,22]
   //_realArray[0,7,19,{9,11},4,5,5 ,5]
   if (verbose) cout << "update the real array at index 2 and 3; ";
   if (verbose) cout << "Reload();\n";
   if (verbose) cout << "\tResult 10: one == 7: ";
   REPORT1(one == 7, "one", one);
   if (verbose) cout << "\tResult 10: two == 19: ";
   REPORT1(two == 19, "two", two);
   OperandArrayTestMethod::verify("10", 8, 8, 0, 7, 19, expectedResult8, 4, 5, 22, 22);
   }

void
modifyIndex1And2(ARRAYVALUETYPE one, ARRAYVALUETYPE two)
   {
   //VMOA      [0,1,2,{9,11},4,5,22,22]
   //_realArray[0,1,2,{9,11},4,5,5 ,5]
   if (verbose) cout << "modify elements at index 1 and 2 and return";
   ARRAYVALUETYPE *realArray = *verifyArray;
   REPORT1(realArray[1]== 1, "modifyTop3Elements realArray[1]", realArray[1]);
   REPORT1(realArray[2]== 2, "modifyTop3Elements realArray[2]", realArray[2]);
   realArray[1] = one;
   realArray[2] = two;
   //VMOA      [0,1,2 ,{9,11},4,5,22,22]
   //_realArray[0,7,19,{9,11},4,5,5 ,5]
   }

bool
OperandArrayTestMethod::verifyUntouched(int32_t maxTouched)
   {
   for (int32_t i=maxTouched;i<_realArrayLength;i++)
      if (_realArray[i] != 0)
         return false;
   return true;
   }

void
OperandArrayTestMethod::verify(const char *step, int32_t max, int32_t num, ...)
   {
   va_list args;
   va_start(args, num);
   for (int32_t a=0;a < num;a++)
      {
      ARRAYVALUETYPE val = va_arg(args, ARRAYVALUETYPE);
      if (verbose) cout << "\tResult " << step << ": _realArray[" << a << "] == " << val << ": ";
      REPORT2(_realArray[a] == val, "_realArray[a]", _realArray[a], "val", val);
      }

   if (verbose) cout << "\tResult " << step << ": end of array untouched: ";
   REPORT1(verifyUntouched(max), "max", max);
   }


OperandArrayTestMethod::OperandArrayTestMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test");
   DefineReturnType(NoType);

   _realArrayLength = 8;
   _realArray = (ARRAYVALUETYPE *) malloc (_realArrayLength * sizeof(ARRAYVALUETYPE));
   memset(_realArray, 0, _realArrayLength*sizeof(ARRAYVALUETYPE));

   _valueType = ARRAYVALUEILTYPE;
   TR::IlType *pValueType = d->PointerTo(_valueType);

   DefineFunction("createArray", "0", "0", (void *)&OperandArrayTestMethod::createArray, NoType, 0);
   DefineFunction("moveArray", "0", "0", (void *)&OperandArrayTestMethod::moveArray, pValueType, 0);
   DefineFunction("freeArray", "0", "0", (void *)&OperandArrayTestMethod::freeArray, NoType, 0);
   DefineFunction("verifyResult0", "0", "0", (void *)&verifyResult0, NoType, 0);
   DefineFunction("verifyResult1", "0", "0", (void *)&verifyResult1, NoType, 0);
   DefineFunction("verifyResult2", "0", "0", (void *)&verifyResult2, NoType, 1, _valueType);
   DefineFunction("verifyResult3", "0", "0", (void *)&verifyResult3, NoType, 1, _valueType);
   DefineFunction("verifyResult4", "0", "0", (void *)&verifyResult4, NoType, 1, _valueType);
   DefineFunction("verifyResult5", "0", "0", (void *)&verifyResult5, NoType, 1, _valueType);
   DefineFunction("verifyResult6", "0", "0", (void *)&verifyResult6, NoType, 1, _valueType);
   DefineFunction("verifyResult7", "0", "0", (void *)&verifyResult7, NoType, 0);
   DefineFunction("verifyResult8", "0", "0", (void *)&verifyResult8, NoType, 1, _valueType);
   DefineFunction("verifyResult9", "0", "0", (void *)&verifyResult9, NoType, 2, _valueType, _valueType);
   DefineFunction("verifyResult10", "0", "0", (void *)&verifyResult10, NoType, 2, _valueType, _valueType);
   DefineFunction("modifyIndex1And2", "0", "0", (void *)&modifyIndex1And2, NoType, 2, _valueType, _valueType);
   }

// convenience macros
#define ARRAY(b)         ((TR::VirtualMachineOperandArray *)(b)->vmState())
#define UPDATEARRAY(b,s) (ARRAY(b)->UpdateArray(b, s))
#define COMMIT(b)        (ARRAY(b)->Commit(b))
#define RELOAD(b)        (ARRAY(b)->Reload(b))
#define SET(b,i,v)       (ARRAY(b)->Set((b),(i),(v)))
#define GET(b,i)         (ARRAY(b)->Get((b),(i)))
#define MOVE(b, d, s)    (ARRAY(b)->Move(b, d, s))

bool
OperandArrayTestMethod::testArray(TR::BytecodeBuilder *builder, bool useEqual)
   {
   //VMOA      [null,null,null,...,null]
   //_realArray[null,null,null,...,null]

   SET(builder, 0, builder->ConstInteger(_valueType, 0));
   //VMOA      [0   ,null,null,...,null]
   //_realArray[null,null,null,...,null]
   builder->Call("verifyResult0", 0);

   COMMIT(builder);
   //VMOA      [0,null,null,...,null]
   //_realArray[0,null,null,...,null]
   builder->Call("verifyResult1", 0);

   SET(builder, 1, builder->ConstInteger(_valueType, 1));
   SET(builder, 2, builder->ConstInteger(_valueType, 2));
   //VMOA      [0,1   ,2   ,null,...,null]
   //_realArray[0,null,null,null,...,null]
   builder->Call("verifyResult2", 1, GET(builder, 2));

   COMMIT(builder);
   TR::IlValue *newArray = builder->Call("moveArray", 0);
   UPDATEARRAY(builder, newArray);
   //VMOA      [0,1,2,null,...,null]
   //_realArray[0,1,2,null,...,null]
   builder->Call("verifyResult3", 1, GET(builder, 2));

   MOVE(builder, 3, 2);
   //VMOA      [0,1,2,2   ,null,...,null]
   //_realArray[0,1,2,null,null,...,null]
   builder->Call("verifyResult4", 1, GET(builder, 3));

   COMMIT(builder);
   //VMOA      [0,1,2,2,null,...,null]
   //_realArray[0,1,2,2,null,...,null]
   builder->Call("verifyResult5", 1, GET(builder, 3));

   TR::IlValue *sum = builder->Add(GET(builder, 2), GET(builder, 3));
   SET(builder, 4, sum);
   COMMIT(builder);
   //VMOA      [0,1,2,2,4,null,...,null]
   //_realArray[0,1,2,2,4,null,...,null]
   builder->Call("verifyResult6", 1, GET(builder, 4));

   SET(builder, 5, builder->ConstInteger(_valueType, 5));
   SET(builder, 6, builder->ConstInteger(_valueType, 5));
   COMMIT(builder);
   //VMOA      [0,1,2,2,4,5,5,null]
   //_realArray[0,1,2,2,4,5,5,null]
   builder->Call("verifyResult7", 0);

   TR::BytecodeBuilder *thenBB = OrphanBytecodeBuilder(0, (char*)"BCI_then");
   TR::BytecodeBuilder *elseBB = OrphanBytecodeBuilder(1, (char*)"BCI_else");
   TR::BytecodeBuilder *mergeBB = OrphanBytecodeBuilder(2, (char*)"BCI_merge");

   TR::IlValue *v1 = GET(builder, 5);
   TR::IlValue *v2 = GET(builder, 6);

   if (useEqual)
      builder->IfCmpEqual(thenBB, v1, v2);
   else
      builder->IfCmpNotEqual(thenBB, v1, v2);

   builder->AddFallThroughBuilder(elseBB);

   SET(thenBB, 3, thenBB->ConstInteger(_valueType, 9));
   //VMOA      [0,1,2,9,4,5,5,null]
   //_realArray[0,1,2,2,4,5,5,null]
   thenBB->Goto(mergeBB);

   SET(elseBB, 3, elseBB->ConstInteger(_valueType, 11));
   //VMOA      [0,1,2,11,4,5,5,null]
   //_realArray[0,1,2,2, 4,5,5,null]
   elseBB->AddFallThroughBuilder(mergeBB);

   COMMIT(mergeBB);
   //VMOA      [0,1,2,{9,11},4,5,5,null]
   //_realArray[0,1,2,{9,11},4,5,5,null]
   // Since I used MOVE to set index 3 to the value of index 2 only index 3 changed
   mergeBB->Call("verifyResult8", 1, GET(mergeBB, 3));

   SET(mergeBB, 7, GET(mergeBB, 6));
   COMMIT(mergeBB);
   //VMOA      [0,1,2,{9,11},4,5,5,5]
   //_realArray[0,1,2,{9,11},4,5,5,5]

   TR::BytecodeBuilder *change7 = OrphanBytecodeBuilder(3, (char*)"BCI_change7");
   TR::BytecodeBuilder *nextMerge = OrphanBytecodeBuilder(4, (char*)"nextMerge");
   TR::BytecodeBuilder *view6and7 = OrphanBytecodeBuilder(5, (char*)"BCI_view6and7");

   mergeBB->IfCmpEqual(change7, GET(mergeBB, 5), GET(mergeBB, 6));
   mergeBB->AddFallThroughBuilder(nextMerge);

   nextMerge->AddFallThroughBuilder(view6and7);

   SET(change7, 7, change7->ConstInteger(_valueType, 22));
   //VMOA      [0,1,2,{9,11},4,5,5,22]
   //_realArray[0,1,2,{9,11},4,5,5,5]
   change7->Goto(view6and7);
   //Since the TR::IlValue at index 6 and 7 was the same value the merge will cause both to be updated
   //VMOA      [0,1,2,{9,11},4,5,22,22]
   //_realArray[0,1,2,{9,11},4,5,5 ,5]
   COMMIT(view6and7);
   //VMOA      [0,1,2,{9,11},4,5,22,22]
   //_realArray[0,1,2,{9,11},4,5,22,22]
   view6and7->Call("verifyResult9", 2, GET(view6and7, 6), GET(view6and7, 7));

   TR::IlValue *one = view6and7->ConstInteger(_valueType, 7);
   TR::IlValue *two = view6and7->ConstInteger(_valueType, 19);
   view6and7->Call("modifyIndex1And2", 2, one, two);
   //VMOA      [0,1,2 ,{9,11},4,5,22,22]
   //_realArray[0,7,19,{9,11},4,5,22,22]
   RELOAD(view6and7);
   //VMOA      [0,7,19,{9,11},4,5,22,22]
   //_realArray[0,7,19,{9,11},4,5,22,22]
   view6and7->Call("verifyResult10", 2, GET(view6and7, 1), GET(view6and7, 2));

   view6and7->Return();

   return true;
   }

bool
OperandArrayTestMethod::buildIL()
   {
   TR::IlType *ppElementType = _types->PointerTo(_types->PointerTo(ARRAYVALUEILTYPE));

   Call("createArray", 0);

   TR::IlValue *arrayBaseAddress = ConstAddress(&_realArray);
   TR::VirtualMachineRegister *arrayBase = new TR::VirtualMachineRegister(this, "ARRAY", ppElementType, sizeof(ARRAYVALUETYPE), arrayBaseAddress);
   TR::VirtualMachineOperandArray *array = new TR::VirtualMachineOperandArray(this, _realArrayLength, _valueType, arrayBase);

   setVMState(array);

   TR::BytecodeBuilder *bb = OrphanBytecodeBuilder(0, (char *) "entry");
   AppendBuilder(bb);

   testArray(bb, true);

   Call("freeArray", 0);

   return true;
   }

OperandArrayTestUsingFalseMethod::OperandArrayTestUsingFalseMethod(TR::TypeDictionary *d)
   : OperandArrayTestMethod(d)
   {
   }

bool
OperandArrayTestUsingFalseMethod::buildIL()
   {
   TR::IlType *ppElementType = _types->PointerTo(_types->PointerTo(ARRAYVALUEILTYPE));

   Call("createArray", 0);

   TR::IlValue *arrayBaseAddress = ConstAddress(&_realArray);
   TR::VirtualMachineRegister *arrayBase = new TR::VirtualMachineRegister(this, "ARRAY", ppElementType, sizeof(ARRAYVALUETYPE), arrayBaseAddress);
   TR::VirtualMachineOperandArray *array = new TR::VirtualMachineOperandArray(this, _realArrayLength, _valueType, arrayBase);

   setVMState(array);

   TR::BytecodeBuilder *bb = OrphanBytecodeBuilder(0, (char *) "entry");
   AppendBuilder(bb);

   testArray(bb, false);

   Call("freeArray", 0);

   return true;
   }
