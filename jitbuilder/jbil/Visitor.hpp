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

#ifndef VISITOR_INCL
#define VISITOR_INCL

#include <vector>
#include <deque>
#include "Object.hpp"


namespace OMR
{

namespace JitBuilder
{

class Type;
class TypeDictionary;
class FunctionBuilder;
class Builder;
class Operation;

typedef std::deque<Builder *> BuilderWorklist;

class Visitor
   {
   public:
   Visitor(FunctionBuilder * fb, bool visitAppendedBuilders=false)
      : _fb(fb)
      , _visitAppendedBuilders(visitAppendedBuilders)
      { }

   void start();
   void start(Builder * b);
   void start(Operation * op);

   void setVisitAppendedBuilders(bool v) { _visitAppendedBuilders = v; }

   FunctionBuilder *fb() const { return _fb; }

   protected:
   void visitBuilder(Builder * b, std::vector<bool> & visited, BuilderWorklist & list);
   void visitOperations(Builder * b, BuilderWorklist & worklist);

   // subclass Visitor and override these functions as needed
   virtual void visitBegin()                                      { }
   virtual void visitFunctionBuilderPreOps(FunctionBuilder * fb)  { }
   virtual void visitFunctionBuilderPostOps(FunctionBuilder * fb) { }
   virtual void visitBuilderPreOps(Builder * b)                   { }
   virtual void visitBuilderPostOps(Builder * b)                  { }
   virtual void visitOperation(Operation * op)                    { }
   virtual void visitEnd()                                        { }

   // logging support: output msg to the log if enabled
   void trace(std::string msg);

   FunctionBuilder * _fb;
   bool _visitAppendedBuilders;
   };

} // namespace JitBuilder

} // namespace OMR

#endif // defined(VISITOR_INCL)
