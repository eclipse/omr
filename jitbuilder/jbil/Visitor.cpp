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

#include "Visitor.hpp"
#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "TextWriter.hpp"

void
OMR::JitBuilder::Visitor::start()
   {
   visitBegin();

   BuilderWorklist worklist;
   std::vector<bool> visited(Builder::maxIndex());

   visitFunctionBuilderPreOps(_fb);
   visitOperations(_fb, worklist);
   visitFunctionBuilderPostOps(_fb);

   while(!worklist.empty())
      {
      Builder *b = worklist.back();
      visitBuilder(b, visited, worklist);
      worklist.pop_back();
      }
   visitEnd();
   }

void
OMR::JitBuilder::Visitor::start(Builder * b)
   {
   BuilderWorklist worklist;
   std::vector<bool> visited(Builder::maxIndex());
   visitBuilder(b, visited, worklist);
   }

void
OMR::JitBuilder::Visitor::start(Operation * op)
   {
   visitOperation(op);
   }

void
OMR::JitBuilder::Visitor::visitBuilder(Builder *b, std::vector<bool> & visited, BuilderWorklist & worklist)
   {
   int64_t id = b->id();
   if (visited[id])
      return;

   visited[id] = true;

   visitBuilderPreOps(b);
   visitOperations(b, worklist);
   visitBuilderPostOps(b);
   }

void
OMR::JitBuilder::Visitor::visitOperations(Builder *b, BuilderWorklist & worklist)
   {
   for (OperationIterator opIt = b->OperationsBegin(); opIt != b->OperationsEnd(); opIt++)
      {
      Operation * op = *opIt;
      visitOperation(op);

      for (BuilderIterator bIt = op->BuildersBegin(); bIt != op->BuildersEnd(); bIt++)
         {
         Builder * inner_b = *bIt;
         if (inner_b)
            worklist.push_front(inner_b);
         }
      }
   }

void
OMR::JitBuilder::Visitor::trace(std::string msg)
   {
   TextWriter *log = _fb->logger();
   if (log)
      {
      log->indent() << msg << log->endl();
      }
   }

