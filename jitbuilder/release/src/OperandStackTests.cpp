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
#include "ilgen/VirtualMachineOperandStack.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineRegisterInStruct.hpp"
#include "OperandStackTests.hpp"

using std::cout;
using std::cerr;

typedef struct Thread
   {
   int pad;
   STACKVALUETYPE *sp;
   } Thread;

class TestState : public TR::VirtualMachineState
   {
   public:
   TestState()
      : TR::VirtualMachineState(),
      _stack(NULL),
      _stackTop(NULL)
      { }

   TestState(TR::VirtualMachineOperandStack *stack, TR::VirtualMachineRegister *stackTop)
      : TR::VirtualMachineState(),
      _stack(stack),
      _stackTop(stackTop)
      {
      }

   virtual void Commit(TR::IlBuilder *b)
      {
      _stack->Commit(b);
      _stackTop->Commit(b);
      }

   virtual void Reload(TR::IlBuilder *b)
      {
      _stack->Reload(b);
      _stackTop->Reload(b);
      }

   virtual VirtualMachineState *MakeCopy()
      {
      TestState *newState = new TestState();
      newState->_stack = (TR::VirtualMachineOperandStack *)_stack->MakeCopy();
      newState->_stackTop = (TR::VirtualMachineRegister *) _stackTop->MakeCopy();
      return newState;
      }

   virtual void MergeInto(VirtualMachineState *other, TR::IlBuilder *b)
      {
      TestState *otherState = (TestState *)other;
      _stack->MergeInto(((TestState *)other)->_stack, b);
      _stackTop->MergeInto(((TestState *)other)->_stackTop, b);
      }

   TR::VirtualMachineOperandStack * _stack;
   TR::VirtualMachineRegister     * _stackTop;
   };

static bool verbose = false;
static int32_t numFailingTests = 0;
static int32_t numPassingTests = 0;
static STACKVALUETYPE **verifySP = NULL;
static STACKVALUETYPE expectedResult12Top = -1;
static char * result12Operator;
static Thread thread;
static bool useThreadSP = false;

static void
setupResult12Equals()
   {
   expectedResult12Top = 11;
   result12Operator = (char *)"==";
   }

static void
setupResult12NotEquals()
   {
   expectedResult12Top = 99;
   result12Operator = (char *)"!=";
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

   cout << "Step 2: compile operand stack tests using a straight pointer\n";
   TR::TypeDictionary types2;
   OperandStackTestMethod pointerMethod(&types2);
   uint8_t *entry2 = 0;
   int32_t rc2 = compileMethodBuilder(&pointerMethod, &entry2);
   if (rc2 != 0)
      {
      cerr << "FAIL: compilation error " << rc2 << "\n";
      exit(-2);
      }

   cout << "Step 3: invoke compiled code and print results\n";
   typedef void (OperandStackTestMethodFunction)();
   OperandStackTestMethodFunction *ptrTest = (OperandStackTestMethodFunction *) entry2;
   verifySP = pointerMethod.getSPPtr();
   setupResult12Equals();
   ptrTest();

   cout << "Step 4: compile operand stack tests using a Thread structure\n";
   TR::TypeDictionary types4;
   OperandStackTestUsingStructMethod threadMethod(&types4);
   uint8_t *entry4 = 0;
   int32_t rc4 = compileMethodBuilder(&threadMethod, &entry4);
   if (rc4 != 0)
      {
      cerr << "FAIL: compilation error " << rc4 << "\n";
      exit(-2);
      }

   cout << "Step 5: invoke compiled code and print results\n";
   typedef void (OperandStackTestUsingStructMethodFunction)(Thread *thread);
   OperandStackTestUsingStructMethodFunction *threadTest = (OperandStackTestUsingStructMethodFunction *) entry4;

   useThreadSP = true;
   verifySP = &thread.sp;
   setupResult12NotEquals();
   threadTest(&thread);

   cout << "Step 6: shutdown JIT\n";
   shutdownJit();

   cout << "Number passing tests: " << numPassingTests << "\n";
   cout << "Number failing tests: " << numFailingTests << "\n";

   if (numFailingTests == 0)
      cout << "ALL PASS\n";
   else
      cout << "SOME FAILURES\n";
   }


STACKVALUETYPE *OperandStackTestMethod::_realStack = NULL;
STACKVALUETYPE *OperandStackTestMethod::_realStackTop = _realStack - 1;
int32_t OperandStackTestMethod::_realStackSize = -1;

void
OperandStackTestMethod::createStack()
   {
   int32_t stackSizeInBytes = _realStackSize * sizeof(STACKVALUETYPE);
   _realStack = (STACKVALUETYPE *) malloc(stackSizeInBytes);
   _realStackTop = _realStack - 1;
   thread.sp = _realStackTop;
   memset(_realStack, 0, stackSizeInBytes);
   }

STACKVALUETYPE *
OperandStackTestMethod::moveStack()
   {
   int32_t stackSizeInBytes = _realStackSize * sizeof(STACKVALUETYPE);
   STACKVALUETYPE *newStack = (STACKVALUETYPE *) malloc(stackSizeInBytes);
   int32_t delta = 0;
   if (useThreadSP)
      delta = thread.sp - _realStack;
   else
      delta = _realStackTop - _realStack;
   memcpy(newStack, _realStack, stackSizeInBytes);
   memset(_realStack, 0xFF, stackSizeInBytes);
   free(_realStack);
   _realStack = newStack;
   _realStackTop = _realStack + delta;
   thread.sp = _realStackTop;

   return _realStack - 1;
   }

void
OperandStackTestMethod::freeStack()
   {
   memset(_realStack, 0xFF, _realStackSize * sizeof(STACKVALUETYPE));
   free(_realStack);
   _realStack = NULL;
   _realStackTop = NULL;
   thread.sp = NULL;
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

// Result 0: empty stack even though Push has happened
void
verifyResult0()
   {
   if (verbose) cout << "Push(1)  [ no commit ]\n";
   OperandStackTestMethod::verifyStack("0", -1, 0);
   }

void
verifyResult1()
   {
   if (verbose) cout << "Commit(); Top()\n";
   OperandStackTestMethod::verifyStack("1", 0, 1, 1);
   }

void
verifyResult2(STACKVALUETYPE top)
   {
   if (verbose) cout << "Push(2); Push(3); Top()   [ no commit]\n";
   if (verbose) cout << "\tResult 2: top value == 3: ";
   REPORT1(top == 3, "top", top);

   OperandStackTestMethod::verifyStack("2", 0, 1, 1);
   }

void
verifyResult3(STACKVALUETYPE top)
   {
   if (verbose) cout << "Commit(); Top()\n";
   if (verbose) cout << "\tResult 3: top value == 3: ";
   REPORT1(top == 3, "top", top);

   OperandStackTestMethod::verifyStack("3", 2, 3, 1, 2, 3);
   }

void
verifyResult4(STACKVALUETYPE popValue)
   {
   if (verbose) cout << "Pop()    [ no commit]\n";
   if (verbose) cout << "\tResult 4: pop value == 3: ";
   REPORT1(popValue == 3, "popValue", popValue);

   OperandStackTestMethod::verifyStack("4", 2, 3, 1, 2, 3);
   }

void
verifyResult5(STACKVALUETYPE popValue)
   {
   if (verbose) cout << "Pop()    [ no commit]\n";
   if (verbose) cout << "\tResult 5: pop value == 2: ";
   REPORT1(popValue == 2, "popValue", popValue);

   OperandStackTestMethod::verifyStack("5", 2, 3, 1, 2, 3);
   }

void
verifyResult6(STACKVALUETYPE top)
   {
   if (verbose) cout << "Push(Add(popValue1, popValue2)); Commit(); Top()\n";
   if (verbose) cout << "\tResult 6: top == 5: ";
   REPORT1(top == 5, "top", top);

   OperandStackTestMethod::verifyStack("6", 2, 2, 1, 5);
   }

void
verifyResult7()
   {
   if (verbose) cout << "Drop(2); Commit(); [ empty stack ]\n";
   OperandStackTestMethod::verifyStack("7", 2, 0);
   }

void
verifyResult8(STACKVALUETYPE pick)
   {
   if (verbose) cout << "Push(5); Push(4); Push(3); Push(2); Push(1); Commit(); Pick(3)\n";
   if (verbose) cout << "\tResult 8: pick == 4: ";
   REPORT1(pick == 4, "pick", pick);

   OperandStackTestMethod::verifyStack("8", 2, 0);
   }

void
verifyResult9(STACKVALUETYPE top)
   {
   if (verbose) cout << "Drop(2); Top()\n";
   if (verbose) cout << "\tResult 9: top == 3: ";
   REPORT1(top == 3, "top", top);

   OperandStackTestMethod::verifyStack("9", 2, 0);
   }

void
verifyResult10(STACKVALUETYPE pick)
   {
   if (verbose) cout << "Dup(); Pick(2)\n";
   if (verbose) cout << "\tResult 10: pick == 4: ";
   REPORT1(pick == 4, "pick", pick);

   OperandStackTestMethod::verifyStack("10", 2, 0);
   }

void
verifyResult11()
   {
   if (verbose) cout << "Commit();\n";
   OperandStackTestMethod::verifyStack("11", 3, 4, 5, 4, 3, 3);
   }

void
verifyResult12(STACKVALUETYPE top)
   {
   if (verbose) cout << "Pop(); Pop(); if (3 " << result12Operator << " 3) { Push(11); } else { Push(99); } Commit(); Top();\n";
   if (verbose) cout << "\tResult 12: top == " << expectedResult12Top << ": ";
   REPORT1(top == expectedResult12Top, "top", top);
   OperandStackTestMethod::verifyStack("11", 3, 3, 5, 4, expectedResult12Top);
   }

// used to compare expected values and report fail it not equal
void
verifyValuesEqual(STACKVALUETYPE v1, STACKVALUETYPE v2)
   { 
   REPORT2(v1 == v2, "verifyValuesEqual v1", v1, "verifyValuesEqual v2", v2); 
   }

// take the arguments from the stack and modify them
void
modifyTop3Elements(int32_t amountToAdd)
   {
   if (verbose) cout << "Push();Push();Push() - modify elements passed in real stack and return";
   STACKVALUETYPE *realSP = *verifySP; 
   REPORT1(realSP[0]== 3, "modifyTop3Elements realSP[0]", realSP[0]); 
   REPORT1(realSP[-1]== 2, "modifyTop3Elements realSP[-1]", realSP[-1]); 
   REPORT1(realSP[-2]== 1, "modifyTop3Elements realSP[-2]", realSP[-2]); 
   realSP[0] += amountToAdd;
   realSP[-1] += amountToAdd;
   realSP[-2] += amountToAdd;  
   }




bool
OperandStackTestMethod::verifyUntouched(int32_t maxTouched)
   {
   for (int32_t i=maxTouched+1;i < _realStackSize;i++) 
      if (_realStack[i] != 0)
         return false;
   return true;
   }

void
OperandStackTestMethod::verifyStack(const char *step, int32_t max, int32_t num, ...)
   {

   STACKVALUETYPE *realSP = *verifySP;

   if (verbose) cout << "\tResult " << step << ": realSP-_realStack == " << (num-1) << ": ";
   REPORT2((realSP-_realStack) == (num-1), "_realStackTop-_realStack", (realSP-_realStack), "num-1", (num-1));

   va_list args;
   va_start(args, num);
   for (int32_t a=0;a < num;a++)
      {
      STACKVALUETYPE val = va_arg(args, STACKVALUETYPE);
      if (verbose) cout << "\tResult " << step << ": _realStack[" << a << "] == " << val << ": ";
      REPORT2(_realStack[a] == val, "_realStack[a]", _realStack[a], "val", val);
      }
   va_end(args);

   if (verbose) cout << "\tResult " << step << ": upper stack untouched: ";
   REPORT1(verifyUntouched(max), "max", max);
   }


OperandStackTestMethod::OperandStackTestMethod(TR::TypeDictionary *d)
   : MethodBuilder(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("test");
   DefineReturnType(NoType);

   _realStackSize = 32;
   _valueType = STACKVALUEILTYPE;
   TR::IlType *pValueType = d->PointerTo(_valueType);

   DefineFunction("createStack", "0", "0", (void *)&OperandStackTestMethod::createStack, NoType, 0);
   DefineFunction("moveStack", "0", "0", (void *)&OperandStackTestMethod::moveStack, pValueType, 0);
   DefineFunction("freeStack", "0", "0", (void *)&OperandStackTestMethod::freeStack, NoType, 0);
   DefineFunction("verifyResult0", "0", "0", (void *)&verifyResult0, NoType, 0);
   DefineFunction("verifyResult1", "0", "0", (void *)&verifyResult1, NoType, 0);
   DefineFunction("verifyResult2", "0", "0", (void *)&verifyResult2, NoType, 1, _valueType);
   DefineFunction("verifyResult3", "0", "0", (void *)&verifyResult3, NoType, 1, _valueType);
   DefineFunction("verifyResult4", "0", "0", (void *)&verifyResult4, NoType, 1, _valueType);
   DefineFunction("verifyResult5", "0", "0", (void *)&verifyResult5, NoType, 1, _valueType);
   DefineFunction("verifyResult6", "0", "0", (void *)&verifyResult6, NoType, 1, _valueType);
   DefineFunction("verifyResult7", "0", "0", (void *)&verifyResult7, NoType, 0);
   DefineFunction("verifyResult8", "0", "0", (void *)&verifyResult8, NoType, 1, _valueType);
   DefineFunction("verifyResult9", "0", "0", (void *)&verifyResult9, NoType, 1, _valueType);
   DefineFunction("verifyResult10", "0", "0", (void *)&verifyResult10, NoType, 1, _valueType);
   DefineFunction("verifyResult11", "0", "0", (void *)&verifyResult11, NoType, 0);
   DefineFunction("verifyResult12", "0", "0", (void *)&verifyResult12, NoType, 1, _valueType);
   DefineFunction("verifyValuesEqual", "0", "0", (void *)&verifyValuesEqual, NoType, 2, _valueType, _valueType);
   DefineFunction("modifyTop3Elements", "0", "0", (void *)&modifyTop3Elements, NoType, 1, _valueType);
   }

// convenience macros
#define STACK(b)           (((TestState *)(b)->vmState())->_stack)
#define STACKTOP(b)        (((TestState *)(b)->vmState())->_stackTop)
#define COMMIT(b)          ((b)->vmState()->Commit(b))
#define RELOAD(b)          ((b)->vmState()->Reload(b))
#define UPDATESTACK(b,s)   (STACK(b)->UpdateStack(b, s))
#define PUSH(b,v)          (STACK(b)->Push(b,v))
#define POP(b)             (STACK(b)->Pop(b))
#define TOP(b)             (STACK(b)->Top(b))
#define DUP(b)             (STACK(b)->Dup(b))
#define DROP(b,d)          (STACK(b)->Drop(b,d))
#define PICK(b,d)          (STACK(b)->Pick(b,d))

bool
OperandStackTestMethod::testStack(TR::BytecodeBuilder *b, bool useEqual)
   {
   PUSH(b, b->ConstInteger(_valueType, 1));
   b->Call("verifyResult0", 0);

   COMMIT(b);
   b->Call("verifyResult1", 0);

   PUSH(b, b->ConstInteger(_valueType, 2));
   PUSH(b, b->ConstInteger(_valueType, 3));
   b->Call("verifyResult2", 1, TOP(b));

   COMMIT(b);
   TR::IlValue *newStack = b->Call("moveStack", 0);
   UPDATESTACK(b, newStack);
   b->Call("verifyResult3", 1, TOP(b));

   TR::IlValue *val1 = POP(b);
   b->Call("verifyResult4", 1, val1);

   TR::IlValue *val2 = POP(b);
   b->Call("verifyResult5", 1, val2);

   TR::IlValue *sum = b->Add(val1, val2);
   PUSH(b, sum);
   COMMIT(b);
   newStack = b->Call("moveStack", 0);
   UPDATESTACK(b, newStack);
   b->Call("verifyResult6", 1, TOP(b));

   DROP(b, 2);
   COMMIT(b);
   b->Call("verifyResult7", 0);

   PUSH(b, b->ConstInteger(_valueType, 5));
   PUSH(b, b->ConstInteger(_valueType, 4));
   PUSH(b, b->ConstInteger(_valueType, 3));
   PUSH(b, b->ConstInteger(_valueType, 2));
   PUSH(b, b->ConstInteger(_valueType, 1));
   b->Call("verifyResult8", 1, PICK(b, 3));

   DROP(b, 2);
   b->Call("verifyResult9", 1, TOP(b));

   DUP(b);
   b->Call("verifyResult10", 1, PICK(b, 2));

   COMMIT(b);
   newStack = b->Call("moveStack", 0);
   UPDATESTACK(b, newStack);
   b->Call("verifyResult11", 0);

   TR::BytecodeBuilder *thenBB = OrphanBytecodeBuilder(0, (char*)"BCI_then");
   TR::BytecodeBuilder *elseBB = OrphanBytecodeBuilder(1, (char*)"BCI_else");
   TR::BytecodeBuilder *mergeBB = OrphanBytecodeBuilder(2, (char*)"BCI_merge");

   TR::IlValue *v1 = POP(b);
   TR::IlValue *v2 = POP(b);
   if (useEqual)
      b->IfCmpEqual(thenBB, v1, v2);
   else
      b->IfCmpNotEqual(thenBB, v1, v2);
   b->AddFallThroughBuilder(elseBB);

   PUSH(thenBB, thenBB->ConstInteger(_valueType, 11));
   thenBB->Goto(mergeBB);

   PUSH(elseBB, elseBB->ConstInteger(_valueType, 99));
   elseBB->AddFallThroughBuilder(mergeBB);

   COMMIT(mergeBB);
   newStack = b->Call("moveStack", 0);
   UPDATESTACK(b, newStack);
   mergeBB->Call("verifyResult12", 1, TOP(mergeBB));
 
   int amountToAdd = 10;
  // Reload test. Call a routine that modifies stack elements passed to it. 
   // Test by reloading and test the popped values
   PUSH(mergeBB, mergeBB->ConstInteger(_valueType, 1));
   PUSH(mergeBB, mergeBB->ConstInteger(_valueType, 2));
   PUSH(mergeBB, mergeBB->ConstInteger(_valueType, 3));  
   COMMIT(mergeBB); 
   mergeBB->Call("modifyTop3Elements",1, mergeBB->ConstInteger(_valueType, amountToAdd));  
   RELOAD(mergeBB);
   TR::IlValue *modifiedStackElement = POP(mergeBB);
   TR::IlValue *expected =  mergeBB->ConstInteger(_valueType, 3+amountToAdd); 
   mergeBB->Call("verifyValuesEqual", 2, modifiedStackElement, expected);  
   modifiedStackElement = POP(mergeBB);
   expected =  mergeBB->ConstInteger(_valueType, 2+amountToAdd);
   mergeBB->Call("verifyValuesEqual",2,  modifiedStackElement, expected);  
   modifiedStackElement = POP(mergeBB);
   expected =  mergeBB->ConstInteger(_valueType, 1+amountToAdd);
   mergeBB->Call("verifyValuesEqual",2,  modifiedStackElement, expected);  

   mergeBB->Return();

   return true;
   }

bool
OperandStackTestMethod::buildIL()
   {
   TR::IlType *pElementType = _types->PointerTo(_types->PointerTo(STACKVALUEILTYPE));

   Call("createStack", 0);

   TR::IlValue *realStackTopAddress = ConstAddress(&_realStackTop);
   TR::VirtualMachineRegister *stackTop = new TR::VirtualMachineRegister(this, "SP", pElementType, sizeof(STACKVALUETYPE), realStackTopAddress);
   TR::VirtualMachineOperandStack *stack = new TR::VirtualMachineOperandStack(this, 1, _valueType, stackTop);

   TestState *vmState = new TestState(stack, stackTop);
   setVMState(vmState);

   TR::BytecodeBuilder *bb = OrphanBytecodeBuilder(0, (char *) "entry");
   AppendBuilder(bb);

   testStack(bb, true);

   Call("freeStack", 0);

   return true;
   }

OperandStackTestUsingStructMethod::OperandStackTestUsingStructMethod(TR::TypeDictionary *d)
   : OperandStackTestMethod(d)
   {
   d->DefineStruct("Thread");
   d->DefineField("Thread", "sp", d->PointerTo(STACKVALUEILTYPE), offsetof(Thread, sp));
   d->CloseStruct("Thread");

   DefineParameter("thread", d->PointerTo("Thread"));
   }

bool
OperandStackTestUsingStructMethod::buildIL()
   {
   Call("createStack", 0);

   TR::VirtualMachineRegisterInStruct *stackTop = new TR::VirtualMachineRegisterInStruct(this, "Thread", "thread", "sp", "SP");
   TR::VirtualMachineOperandStack *stack = new TR::VirtualMachineOperandStack(this, 1, _valueType, stackTop);

   TestState *vmState = new TestState(stack, stackTop);
   setVMState(vmState);

   TR::BytecodeBuilder *bb = OrphanBytecodeBuilder(0, (char *) "entry");
   AppendBuilder(bb);

   testStack(bb, false);

   Call("freeStack", 0);

   return true;
   }
