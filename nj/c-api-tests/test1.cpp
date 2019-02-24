#include "nj_api.h"

#include <stdio.h>

/*
Define environment variable
TR_Options=traceIlGen,traceFull,log=trtrace.log
To obtain a nice trace of codegen
*/

static bool test1_il(JIT_ILInjectorRef ilinjector, void *userdata) {
  JIT_CreateBlocks(ilinjector, 1);
  JIT_SetCurrentBlock(ilinjector, 0);
  auto iconst = JIT_ConstInt32(42);
  auto node = JIT_CreateNode1C(OP_ireturn, iconst);
  JIT_GenerateTreeTop(ilinjector, node);
  JIT_CFGAddEdge(ilinjector,
                 JIT_BlockAsCFGNode(JIT_GetCurrentBlock(ilinjector)),
                 JIT_GetCFGEnd(ilinjector));
  return true;
}

static int test1(JIT_ContextRef ctx) {
  JIT_FunctionBuilderRef function_builder = JIT_CreateFunctionBuilder(
      ctx, "ret1", JIT_Int32, 0, NULL, test1_il, NULL);
  int rc = 0;
  typedef int (*F)(void);
  F f = (F)JIT_Compile(function_builder, 2);
  if (f) {
    rc = f();
    printf("Function call returned %d\n", rc);
  }
  JIT_DestroyFunctionBuilder(function_builder);
  return (rc == 42 ? 0 : 1);
}

static bool test2_il(JIT_ILInjectorRef ilinjector, void *userdata) {
  JIT_CreateBlocks(ilinjector, 1);
  JIT_SetCurrentBlock(ilinjector, 0);
  auto index = JIT_ConstInt32(4);               // Byte offset of int32_t[1]
  auto base = JIT_LoadParameter(ilinjector, 0); // Load arg
  auto value = JIT_ArrayLoad(ilinjector, base, index, JIT_Int32); // Get arg[1]
  JIT_GenerateTreeTop(
      ilinjector, value); // Without TreeTop here we get the stored value below
  JIT_ArrayStore(ilinjector, base, index,
                 JIT_ConstInt32(42)); // Set arg[1] = 42
  auto node = JIT_CreateNode1C(OP_ireturn, value);
  JIT_GenerateTreeTop(ilinjector, node);
  JIT_CFGAddEdge(ilinjector,
                 JIT_BlockAsCFGNode(JIT_GetCurrentBlock(ilinjector)),
                 JIT_GetCFGEnd(ilinjector));
  return true;
}

static int test2(JIT_ContextRef ctx) {
  JIT_Type params[1] = {
      JIT_Address
  };
  JIT_FunctionBuilderRef function_builder = JIT_CreateFunctionBuilder(
      ctx, "ret2", JIT_Int32, 1, params, test2_il, NULL);
  int rc = 0;
  typedef int32_t (*F)(int32_t *);
  F f = (F)JIT_Compile(function_builder, 2);
  if (f) {
    int32_t testdata[] = {422, -84};
    int32_t result = f(testdata);
    printf("Function returned %d, expected -84, updated value %d\n", result,
           testdata[1]);
    if (result == -84 && testdata[1] == 42)
      rc = 0;
    else
      rc = 1;
  }
  JIT_DestroyFunctionBuilder(function_builder);
  return rc;
}

static int callme(int a) { return a + 42; }

static bool test3_il(JIT_ILInjectorRef ilinjector, void *userdata) {
  JIT_CreateBlocks(ilinjector, 1);
  JIT_SetCurrentBlock(ilinjector, 0);
  auto input = JIT_LoadParameter(ilinjector, 0);         // Load arg
  JIT_NodeRef args[] = { input };
  auto value = JIT_Call(ilinjector, "callme", 1, args); // Call callme(input)
  auto node = JIT_CreateNode1C(OP_ireturn, value);
  JIT_GenerateTreeTop(ilinjector, node);
  JIT_CFGAddEdge(ilinjector,
                 JIT_BlockAsCFGNode(JIT_GetCurrentBlock(ilinjector)),
                 JIT_GetCFGEnd(ilinjector));
  return true;
}

static int test3(JIT_ContextRef ctx) {
  JIT_Type params[1] = {
      JIT_Int32
  };
  JIT_RegisterFunction(ctx, "callme", JIT_Int32, 1, params, (void *)callme);
  JIT_FunctionBuilderRef function_builder = JIT_CreateFunctionBuilder(
      ctx, "ret3", JIT_Int32, 1, params, test3_il, NULL);
  int rc = 0;
  typedef int32_t (*F)(int32_t);
  F f = (F)JIT_Compile(function_builder, 2);
  if (f) {
    int32_t result = f(-42);
    printf("Function returned %d, expected 0\n", result);
    if (result == 0)
      rc = 0;
    else
      rc = 1;
  }
  JIT_DestroyFunctionBuilder(function_builder);
  return rc;
}

int main(int argc, const char *argv[]) {
  int errorcount = 0;
  JIT_ContextRef ctx = JIT_CreateContext();
  if (ctx) {
    errorcount += test1(ctx);
    errorcount += test2(ctx);
    errorcount += test3(ctx);
  } else {
    errorcount = 1;
  }
  JIT_DestroyContext(ctx);
  if (errorcount == 0) {
    printf("All Tests PASSED\n");
  } else {
    printf("%d Tests FAILED\n", errorcount);
  }
  return errorcount == 0 ? 0 : 1;
}
