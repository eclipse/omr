/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef COMPILATIONEXCEPTION_HPP
#define COMPILATIONEXCEPTION_HPP

#pragma once

#include <exception>

namespace TR {

struct CompilationException : public virtual std::exception
   {
   virtual const char* what() const throw() { return "Compilation Exception"; }
   };

struct ILGenFailure : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "IL Gen Failure"; }
   };

struct RecoverableILGenException : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Recoverable IL Gen Exception"; }
   };

struct ExcessiveComplexity : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Excessive Complexity"; }
   };

struct MaxCallerIndexExceeded : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Max Caller Index Exceeded"; }
   };

struct CompilationInterrupted : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Compilation Interrupted"; }
   };

struct UnimplementedOpCode : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Unimplemented Op Code"; }
   };

struct InsufficientlyAggressiveCompilation : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "Insufficiently Aggressive Compilation"; }
   };

struct GCRPatchFailure : public virtual CompilationException
   {
   virtual const char* what() const throw() { return "GCR Patch Failure"; }
   };

}

#endif // COMPILATIONEXCEPTION_HPP
