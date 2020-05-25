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

#ifndef COMPILATIONEXCEPTION_HPP
#define COMPILATIONEXCEPTION_HPP

#pragma once

#include <exception>

namespace TR {

/**
 * Compilation Failure exception type.
 *
 * The most general type of exception thrown during a compilation that is
 * not related to an Out of Memory condition (which results in std::bad_alloc
 * or an exception derived from std::bad_alloc thrown).
 *
 * Functionally, unless a compilation error requires special processing,
 * TR::CompilationException is sufficient. However, it is good practice to
 * define somewhat general subtypes for RAS purposes.
 */
struct CompilationException : public virtual std::exception
   {
   virtual const char* what() const throw() { return "Compilation Exception"; }
   };


/**
 * IL Validation Failure exception type.
 *
 * Thrown on an IL Validation Failure condition.
 */
struct ILValidationFailure : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "IL Validation Failure"; }
   };

/**
 * IL Generation Failure exception type.
 *
 * Thrown on an IL Generation Failure condition.
 */
struct ILGenFailure : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "IL Gen Failure"; }
   };

/**
 * Unsupported value type operation exception type.
 *
 * Thrown if an unsupported value type operation is encountered that requires
 * compilation to be aborted.
 */
struct UnsupportedValueTypeOperation : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Unsupported value type operation"; }
   };

/**
 * Recoverable IL Generation Failure exception type.
 *
 * Thrown on an IL Generation Failure condition which the compiler can
 * recover either by
 * 1. continuing the compilation of the caller method if the failing method is an inlined callee
 * 2. allowing a recompilation to occur for the failing method if it's the outermost method
 */
struct RecoverableILGenException : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Recoverable IL Gen Exception"; }
   };

/**
 * Special recoverable IL Generation Failure exception type.
 *
 * Different from general RecoverableILGenException in that
 * recompilation is not allowed when the failing method is the outermost method.
 */
struct NoRecompilationRecoverableILGenException: public RecoverableILGenException
   {
   virtual const char* what() const throw() { return "Recoverable IL Gen Exception with no recompilation if failing method is outermost"; }
   };

/**
 * Excessive Complexity exception type.
 *
 * Thrown when the complexity of the compile exceeds the compiler's
 * threshold to sucessfully finish compilation, for example, if the
 * compilation created more TR::Node objects than is supported.
 */
struct ExcessiveComplexity : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Excessive Complexity"; }
   };

/**
 * Max Caller Index Exceeded exception type.
 *
 * Thrown when the number of calls to other methods from the method being
 * compiled exceeds the compiler's threshold.
 */
struct MaxCallerIndexExceeded : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Max Caller Index Exceeded"; }
   };

/**
 * Compilation Interrupted exception type.
 *
 * Thrown when the compilation has to be interrupted, for example, if a runtime
 * is going into its shutdown phase.
 */
struct CompilationInterrupted : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Compilation Interrupted"; }
   };

/**
 * Unimplemented Op Code exception type.
 *
 * Thrown when the compiler encounters an unimplemented opt code.
 */
struct UnimplementedOpCode : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Unimplemented Op Code"; }
   };

/**
 * Insufficiently Aggressive Compilation exception type.
 *
 * Thrown when the compiler determines that optimization level of the current
 * compilation is not aggressive enough.
 */
struct InsufficientlyAggressiveCompilation : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Insufficiently Aggressive Compilation"; }
   };

/**
 * GCR Patch Failure exception type.
 *
 * Only thrown from J9_PROJECT_SPECIFIC guarded code. Thrown when address of
 * the GCR Patch Point is not known at Binary Encoding.
 */
struct GCRPatchFailure : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "GCR Patch Failure"; }
   };

}

#endif // COMPILATIONEXCEPTION_HPP
