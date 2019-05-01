/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef JB_TEST_UTIL_HPP
#define JB_TEST_UTIL_HPP

#include "gtest/gtest.h"

#include "JitBuilder.hpp"

#include <vector>
#include <utility>

/*
 * Convenience macro for defining type dictionary objects. `name` is the name of
 * the class that will be created. The "body" of the macro should contain the
 * JitBuilder type definitions.
 *
 * Example use:
 *
 *    DEFINE_TYPES(MyTypeDictionary)
 *       {
 *       DefineStruct("MyStruct");
 *       DefineField("MyStruct", "field", Int32);
 *       CloseStruct("MyStruct");
 *       }
 */
#define DEFINE_TYPES(name) \
   struct name : public OMR::JitBuilder::TypeDictionary { name(); }; \
   inline name::name() : OMR::JitBuilder::TypeDictionary()

/*
 * A convenience macro for declaring a MethodBuilder class. `name` is the name
 * of the class that will be created. The macro will ensure the class inherits
 * from `OMR::JitBuilder::MethodBuilder` and that the constructor and `buildIL()` method are
 * declared. A ';' is required at the end of the macro invocation.
 *
 * Example use:
 *
 *    DECLARE_BUILDER(MyFunctionBuilder);
 */
#define DECLARE_BUILDER(name) \
   class name : public OMR::JitBuilder::MethodBuilder { \
      public: \
      name(OMR::JitBuilder::TypeDictionary *); \
      virtual bool buildIL(); \
   }

/*
 * A convenience macro for defining the constructor of a declared builder class.
 * It will ensure that the parent constructor is invoked correctly.
 *
 * The argument is the name of the builder class whose constructor is being defined.
 * The body should contain the actual definition of the constructor.
 *
 * This macro is best used in conjunction with the `DECLARE_BUILDER` macro.
 *
 * Example use:
 *
 *    DECLARE_BUILDER(MyMethod);
 *    DEFINE_BUILDER_CTOR(MyMethod)
 *       {
 *       DefineLine(LINETOSTR(__LINE__));
 *       DefineFile(__FILE__);
 *       DefineName("MyMethod");
 *       DefineReturnType(NoType);
 *       }
 */
#define DEFINE_BUILDER_CTOR(name) \
   inline name::name(OMR::JitBuilder::TypeDictionary *types) : OMR::JitBuilder::MethodBuilder(types)

/*
 * A convenience macro for defining the `buildIL()` function of a declared
 * builder class. The argument is the name of the builder class. The body should
 * be the definition of the `buildIL()` function.
 *
 * This macro is intended to be used in conjunction with `DECLARE_BUILDER` and
 * `DEFINE_BUILDER_CTOR` macros.
 *
 * Example use:
 *
 *    DECLARE_BUILDER(MyMethod);
 *
 *    DEFINE_BUILDER_CTOR(MyMethod)
 *       {
 *       DefineLine(LINETOSTR(__LINE__));
 *       DefineFile(__FILE__);
 *       DefineName("MyMethod");
 *       DefineReturnType(NoType);
 *       }
 *
 *    DEFINE_BUILDIL(MyMethod)
 *       {
 *       Return();
 *       return true;
 *       }
 */
#define DEFINE_BUILDIL(name) \
   inline bool name::buildIL()

/*
 * `DEFINE_BUILDER` is a convenience macro for defining MethodBuilder class
 * using a compact syntax. The goal of the macro is to abstract away some of the
 * boiler plate code that is required for defining instances of MethodBuilder.
 *
 * The first argument to the macro is the name of the MethodBuilder class that
 * will be constructed. This is also used as the name of the method built by the
 * class.
 *
 * The second argument is an instance of `OMR::JitBuilder::IlValue *` that represents the
 * return type of the method.
 *
 * The following variadic argument are pairs with a `const char *` component
 * and a `OMR::JitBuilder::IlValue *` component. The first component is the name of parameter
 * and the second is a `OMR::JitBuilder::IlValue` instance representing the parameter's type.
 * The `PARAM` macro should be used when defining these pairs because simple
 * brace initializers cannot be used when invoking a macro.
 *
 * The body of the macro should contain the code that will be used as body for
 * the `buildIL()` function.
 *
 * Because the arguments to the macro are used within the scope of the class
 * definition, the services provided by `OMR::JitBuilder::MethodBuilder` are available for
 * use (e.g. `typeDictionary()` can be used when defining types).
 *
 * For convenience, the following methods from `OMR::JitBuilder::TypeDictionary` are also
 * made available:
 *
 *    toIlType<T>()
 *    LookupStruct(const char *name)
 *    LookupUnion(const char *name)
 *    PointerTo(OMR::JitBuilder::IlType *type)
 *    PointerTo(const char *structName)
 *    PointerTo(OMR::JitBuilder::DataType type)
 *    PrimitiveType(OMR::JitBuilder::DataType type)
 *    GetFieldType(const char *structName, const char *fieldName)
 *    UnionFieldType(const char *unionName, const char *fieldName)
 *
 * Example use:
 *
 *    DEFINE_BUILDER( MyMethod,                                            // name of the builder/method
 *                    toIlType<int *>(),                                   // return type of the method
 *                    PARAM("arg1", PointerTo(LookupStruct("MyStruct"))),  // first parameter of the method
 *                    PARAM("arg2", Double) )                              // second parameter of the method
 *       {
 *       // Implementation of `buildIL()`
 *
 *       return true;
 *       }
 */
#define DEFINE_BUILDER(name, returnType, ...) \
   struct name : public OMR::JitBuilder::MethodBuilder \
      { \
      name(OMR::JitBuilder::TypeDictionary *types) : OMR::JitBuilder::MethodBuilder(types) \
         { \
         DefineLine(LINETOSTR(__LINE__)); \
         DefineFile(__FILE__); \
         DefineName(#name); \
         DefineReturnType(returnType); \
         std::vector<std::pair<const char *, OMR::JitBuilder::IlType *> > args = {__VA_ARGS__}; \
         for (int i = 0, s = args.size(); i < s; ++i) \
            { \
            DefineParameter(args[i].first, args[i].second); \
            } \
         } \
      bool buildIL(); \
      template <typename T> OMR::JitBuilder::IlType * toIlType() { return typeDictionary()->toIlType<T>(); } \
      OMR::JitBuilder::IlType * LookupStruct(const char *name) { return typeDictionary()->LookupStruct(name); } \
      OMR::JitBuilder::IlType * LookupUnion(const char *name) { return typeDictionary()->LookupUnion(name); } \
      OMR::JitBuilder::IlType * PointerTo(OMR::JitBuilder::IlType *type) { return typeDictionary()->PointerTo(type); } \
      OMR::JitBuilder::IlType * PointerTo(const char *structName) { return typeDictionary()->PointerTo(structName); } \
      OMR::JitBuilder::IlType * GetFieldType(const char *structName, const char *fieldName) { return typeDictionary()->GetFieldType(structName, fieldName); } \
      OMR::JitBuilder::IlType * UnionFieldType(const char *unionName, const char *fieldName) { return typeDictionary()->UnionFieldType(unionName, fieldName); } \
      }; \
   inline bool name::buildIL()

/*
 * A convenience macro for defining MethodBuilder parameter pairs. The first
 * argument to the macro is the name of the parameter, the second is the the
 * `OMR::JitBuilder::IlType` instance representing the type of the parameter.
 */
#define PARAM(name, type) {name, type}

/**
 * @brief The JitBuilderTest class is a basic test fixture for JitBuilder test cases.
 *
 * Most JitBuilder test case fixtures should publically inherit from this class.
 *
 * Example use:
 *
 *    class MyTestCase : public JitBuilderTest {};
 */
class JitBuilderTest : public ::testing::Test
   {
   public:

   static void SetUpTestCase()
      {
      ASSERT_TRUE(initializeJit()) << "Failed to initialize the JIT.";
      }

   static void TearDownTestCase()
      {
      shutdownJit();
      }
   };

/**
 * @brief try_compile is a convenience template function for invoking JitBuilder
 *
 * Note: directly calling this function in a test is not recommend. Using the
 * ASSERT_COMPILE macro is preferred.
 *
 * The first template argument (`TypeDictionary`) is the class type that should
 * be passed to construct the MethodBuilder instance. The specified type *must*
 * inherit from `OMR::JitBuilder::TypeDictionary`.
 *
 * The second template argument (`MethodBuilder`) is the class type of the
 * MethodBuilder instance that should be constructed. The specified type *must*
 * inherit from `OMR::JitBuilder::MethodBuilder`.
 *
 * The last template argument (`Function`) is the type of the function pointer
 * that should be used to store the address of the entry point to the compiled
 * body.
 *
 * The formal parameter to the function is a reference to the variable in which
 * the address of the entry point will be stored.
 *
 * Only the first two template arguments must be specified because that last one
 * can be deduced by the compiler from the type of the argument passed to the
 * function.
 *
 * If compilation of the method fails, the function will assert and return.
 * `::testing::Test::HasFatalFailure()` can be used to detect the assert from
 * the calling test.
 *
 * Example use:
 *
 *    typedef void (*MyFunctionType)(int);   // define type of entry point
 *    MyFunctionType myFunction;             // variable to store the entry point
 *
 *    try_compile<MyTypeDictionary,MyMethodBuilder>(myFunction);
 *       // If compilation succeeds, `myFunction` will point to the entry point
 *       // of the compiled body. Otherwise, the function will assert.
 *
 *    if (::testing::Test::HasFatalFailure())
 *       return;        // return because compilation failed
 *    else
 *       myFunction(2); // execute the compiled body
 */
template <typename TypeDictionary, typename MethodBuilder, typename Function>
void try_compile(Function& f)
   {
   TypeDictionary types;
   MethodBuilder builder(&types);
   void *entry;
   int32_t rc = compileMethodBuilder(&builder, &entry);
   ASSERT_EQ(0, rc) << "Failed to compile method " << builder.GetMethodName();
   f = (Function)entry;
   }

/**
 * @brief ASSERT_COMPILE is a convenience macro for invoking JitBuilder
 *
 * Note: invoking ASSERT_COMPILE is preferred to directly calling `try_compile()`.
 *
 * This macro simply calls `try_compile()` and asserts that compilation
 * succeeded. It forwards its first argument to the TypeDictionary template
 * parameter, its second argument to the MethodBuilder template parameter, and
 * its last last argument becomes the function's argument.
 *
 * Example use:
 *
 *    typedef void (*MyFunctionType)(int);   // define type of entry point
 *    MyFunctionType myFunction;             // variable to store the entry point
 *
 *    ASSERT_COMPILE(MyTypeDictionary, MyMethodBuilder, myFunction);
 *       // Compile method and assert that compilation succeeds. If successful,
 *       // `myFunction` will point to the entry point of the compiled body.
 *
 *    myFunction(2); // execute the compiled body
 */
#define ASSERT_COMPILE(TypeDictionary, MethodBuilder, function) do {\
   try_compile<TypeDictionary,MethodBuilder>(function); \
   ASSERT_FALSE(::testing::Test::HasFatalFailure()); \
} while (0)

#endif // JB_TEST_UTIL_HPP
