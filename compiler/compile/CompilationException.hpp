/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

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

struct TransformationUnsupportedUnderInvolentaryOSR: public virtual RecoverableILGenException
   {
   virtual const char* what() const throw() { return "cannot do OSR under InvoluntaryOSR mode"; }
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

