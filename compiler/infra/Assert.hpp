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

/**
 * \file Assert.hpp
 *
 * Defines a number of assert functions and macros.
 *
 * \def TR::trap                a convenience function for taking down the VM and
 *                              generating a core dump.
 *
 * \def TR_ASSERT_FATAL         an assertion that will always take down the VM.
 *                              Intended for the most egregious of errors.
 *
 * \def TR_ASSERT_SAFE_FATAL    an assertion that will take down the VM, unless
 *                              the non-fatal assert option is set (intended for
 *                              service purposes). The presumption is that the
 *                              user of this assert has ensured that the code
 *                              subsequent to the assert is safe to continue
 *                              executing.
 *
 *                              In production builds, only the line number and
 *                              file name are available to conserve DLL size.
 *
 * \def TR_ASSERT               a macro that defines an assertion which is compiled out
 *                              (including format strings) during production builds
 *
 * \def TR_UNIMPLEMENTED        a macro that expands into an unconditional assertion
 *                              failure. Used to indicate that a function has not been
 *                              implemented and may not be called.
 *
 * A number of helper macros are also provided for printing more detailed contextual
 * information on fatal assertion failures to assist with debugging.
 *
 * \def TR_ASSERT_FATAL_WITH_DETAIL
 * a fatal assertion which will print the provided contextual information upon failure
 * in addition to the provided message.
 *
 * \def TR_ASSERT_FATAL_WITH_NODE
 * a fatal assertion which will print NodeAssertionContext for the provided node upon
 * failure in addition to the provided message.
 *
 * \def TR_ASSERT_FATAL_WITH_INSTRUCTION
 * a fatal assertion which will print InstructionAssertionContext for the provided
 * instruction upon failure in addition to the provided message.
 *
 * We also provide Expect and Ensure, based on the developing C++ Core guidelines [1].
 *
 * \def Expect                  Precondition macro, only defined for DEBUG and
 *                              EXPECT_BUILDs.
 *
 * \def Ensure                  Postcondition macro, only defined for DEBUG and
 *                              EXPECT_BUILDs.
 *
 *
 * \note While not standardized, this header relies on compiler behaviour that
 *       will trim the trailing comma from a variadic macro when provided no
 *       variadic arguments, to avoid TR_ASSERT(cond, "message") from
 *       complaining about a trailing comma.
 *
 * [1]: https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#Ri-expects
 */


#ifndef TR_ASSERT_HPP
#define TR_ASSERT_HPP

#include "infra/Annotations.hpp"                // OMR_NORETURN
#include "compile/CompilationException.hpp"

#include <cstddef>

namespace TR
   {
   class Node;
   class Instruction;

   void OMR_NORETURN trap();

   /**
    * Represents context to be printed alongside an assertion failure message.
    *
    * After printing the message and stack trace associated with an assertion failure, the relevant context will be
    * printed to stderr by calling its printContext function. Contextual information generally includes relevant details
    * about the state of the compiler at the time of the failure, such as the tree containing a node which resulted in
    * an assertion failure.
    */
   class AssertionContext
      {
      public:

      /**
       * Prints contextual information about the assertion for debugging purposes.
       *
       * It is valid for this function to itself result in an assertion failure (potentially with its own context)
       * during the process of printing. To prevent boundless recursion, the context of such an assertion failure will
       * not be printed.
       */
      virtual void printContext() const = 0;
      };

   /**
    * An assertion context which does not print any information.
    */
   class NullAssertionContext : public AssertionContext
      {
      public:
      virtual void printContext() const {}
      };

   /**
    * An assertion context which prints the treetop containing a particular node.
    */
   class NodeAssertionContext : public AssertionContext
      {
      TR::Node *_node;

      public:
      NodeAssertionContext(TR::Node *node) : _node(node) {}

      virtual void printContext() const;
      };

   /**
    * An assertion context which prints instructions surrounding a particular instructions.
    */
   class InstructionAssertionContext : public AssertionContext
      {
      TR::Instruction *_instruction;

      public:
      InstructionAssertionContext(TR::Instruction *instruction) : _instruction(instruction) {}

      virtual void printContext() const;
      };

   // Don't use these directly.
   //
   // Use the TR_ASSERT* macros instead as they control the string
   // contents in production builds.
   void OMR_NORETURN fatal_assertion(const char *file, int line, const char *condition, const char *format, ...);
   void OMR_NORETURN fatal_assertion_with_detail(const AssertionContext& ctx, const char *file, int line, const char *condition, const char *format, ...);

   // Non fatal assertions may in some circumstances return, so do not mark them as
   // no-return.
   void              assertion(const char *file, int line, const char *condition, const char *format, ...);

   /**
    * Assertion failure exception type.
    *
    * Thrown only by TR_ASSERT_SAFE_FATAL, when softFailOnAssumes is set.
    */
   struct AssertionFailure : public virtual CompilationException
      {
      virtual const char* what() const throw() { return "Assertion Failure"; }
      };

   }



// Macro Definitions

#define TR_ASSERT_FATAL(condition, format, ...) \
         do { (condition) ? (void)0 : TR::fatal_assertion(__FILE__, __LINE__, #condition, format, ##__VA_ARGS__); } while(0)

#define TR_ASSERT_FATAL_WITH_DETAIL(ctx, condition, format, ...) \
   do { \
      if (!(condition)) \
         TR::fatal_assertion_with_detail(ctx, __FILE__, __LINE__, #condition, format, ##__VA_ARGS__); \
   } while (0)

#define TR_ASSERT_FATAL_WITH_NODE(node, condition, format, ...) \
   do { \
      TR::Node *nodeVal = (node); \
      TR_ASSERT_FATAL_WITH_DETAIL( \
         TR::NodeAssertionContext(nodeVal), \
         condition, \
         "Node %p [%s]: " format, \
         nodeVal, \
         nodeVal ? nodeVal->getOpCode().getName() : "null", \
         ##__VA_ARGS__ \
      ); \
   } while (0)

#define TR_ASSERT_FATAL_WITH_INSTRUCTION(instr, condition, format, ...) \
   do { \
      TR::Instruction *instrVal = (instr); \
      TR::Node *instrNode = instrVal ? instrVal->getNode() : NULL; \
      TR_ASSERT_FATAL_WITH_DETAIL( \
         TR::InstructionAssertionContext(instrVal), \
         condition, \
         "Instruction %p [%s] (generated from node %p [%s]): " format, \
         instrVal, \
         instrVal ? instrVal->getOpCode().getMnemonicName() : "null", \
         instrNode, \
         instrNode ? instrNode->getOpCode().getName() : "null", \
         ##__VA_ARGS__ \
      ); \
   } while (0)

#define TR_UNIMPLEMENTED() \
         ::TR::fatal_assertion(__FILE__, __LINE__, NULL, "Unimplemented function: %s", __FUNCTION__)

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)

   #define TR_ASSERT(condition, format, ...) \
         do { (condition) ? (void)0 : TR::assertion(__FILE__, __LINE__, #condition, format, ##__VA_ARGS__); } while(0)

   #define TR_ASSERT_SAFE_FATAL(condition, format, ...) \
         do { (condition) ? (void)0 : TR::assertion(__FILE__, __LINE__, #condition, format, ##__VA_ARGS__); } while(0)

#else

   #define TR_ASSERT(condition, format, ...) (void)0

   #define TR_ASSERT_SAFE_FATAL(condition, format, ...) \
         do { (condition) ? (void)0 : TR::assertion(__FILE__, __LINE__, NULL, NULL); } while(0)


#endif



#if defined(DEBUG) || defined(EXPECT_BUILD)
   #define Expect(x) TR_ASSERT((x), "Expectation Failure:")
   #define Ensure(x) TR_ASSERT((x), "Ensure Failure:")
#else
   #define Expect(x) (void)0
   #define Ensure(x) (void)0
#endif



#endif
