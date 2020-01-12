/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef TRANSFORMER_INCL
#define TRANSFORMER_INCL

#include <string>
#include "Object.hpp"
#include "Visitor.hpp"

namespace OMR
{

namespace JitBuilder
{

class Transformer
   {
   public:
   Transformer(FunctionBuilder * fb)
      : _fb(fb)
      , _traceEnabled(false)
      { }

   FunctionBuilder *fb() const { return _fb; }

   void transform();
   void transform(FunctionBuilder *fb)
      {
      FunctionBuilder *saved = _fb;
      _fb = fb;
      transform();
      _fb = saved;
      }
   Transformer * setTraceEnabled(bool v=true) { _traceEnabled = v; return this; }

   private:
   // used internally to iteratively visit each builder once
   void processBuilder(Builder * b, std::vector<bool> & visited, BuilderWorklist & worklist);

   protected:
   bool traceEnabled()               { return _traceEnabled; }

   void appendOrInline(Builder * root, Builder * branch);

   // To implement any transformation, subclass Transformer
   //   and override the virtual functions below as needed

   // called once on each FunctionBuilder before any other processing
   virtual FunctionBuilder * transformFunctionBuilder(FunctionBuilder * fb) { return NULL; }

   // called once each Builder object before its operations are processed
   virtual Builder * transformBuilderBeforeOperations(Builder * b) { return NULL; }

   // called once on each operation
   // the operation will be replaced by the contents of any non-NULL Builder object returned
   virtual Builder * transformOperation(Operation * op) { return NULL; }

   // called once each Builder object after its operations are processed
   virtual Builder * transformBuilderAfterOperations(Builder * b) { return NULL; }

   // called once on each FunctionBuilder after transformation is complete
   virtual FunctionBuilder * transformFunctionBuilderAtEnd(FunctionBuilder * fb) { return NULL; }

   // logging support: output msg to the log if enabled
   void trace(std::string msg);

   // logging support: returns true if the transformation is allowed
   //                  and if log enabled, logs details if performed and "not applied" message if not
   bool performTransformation(Operation * op, Builder * transformed, std::string msg="");
   bool performTransformation(Builder * b, Builder * transformed, std::string msg="");
   bool performTransformation(FunctionBuilder * fb, Builder * transformed, std::string msg="");

   FunctionBuilder * _fb;
   bool              _traceEnabled;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(TRANSFORMER_INCL)
