/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "compile/ResolvedMethod.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/List.hpp"
#include "infra/TrilDumper.hpp"

#include "stdio.h"

TR::TrilDumper::TrilDumper(FILE * file) : _outputFile(file)
   {
   }

void TR::TrilDumper::visitingMethod(TR::ResolvedMethodSymbol* method)
   {
   TR_ResolvedMethod * resolvedMethod = method->getResolvedMethod();

   // all methods must have a return type, so just print it
   fprintf(_outputFile, "(method return=%s ", TR::DataType::getName(resolvedMethod->returnType()));

   // print the method's name if it has one
   if (method->getName()) { fprintf(_outputFile, "name=%s ", method->getName()); }

   // print the argument types, skipping the `arg` field entirely if there are none
   auto paramList = method->getParameterList();
   auto paramIter = ListIterator<TR::ParameterSymbol>(&paramList);
   if (!paramIter.atEnd())
      {
      TR::ParameterSymbol* param = paramIter.getFirst();
      fprintf(_outputFile, "args=[%s", TR::DataType::getName(param->getDataType()));
      param = paramIter.getNext();
      for (; !paramIter.atEnd(); param = paramIter.getNext())
         {
         fprintf(_outputFile, ", %s", TR::DataType::getName(param->getDataType()));
         }
      fprintf(_outputFile, "] ");
      }
   }

void TR::TrilDumper::returnedToMethod(TR::ResolvedMethodSymbol* method)
   {
   fprintf(_outputFile, ") ");
   }

void TR::TrilDumper::visitingNode(TR::Node* node)
   {
   auto opcode = node->getOpCode();
   auto opcodeValue = opcode.getOpCodeValue();

   /*
    * Because Tril does not explicitly represent BBStart and BBEnd nodes (they
    * are implicitly part of a Block), we use them to define where a block has
    * to be printed. This is also required because we cannot rely on
    * `visitingBlock()` being emitted when traversing the IL of a method (see
    * TR::ILTraverser API documentation).
    */

   if (opcodeValue == TR::BBStart)
      {
      // a BBStart means we are entering a new block, so we "open" a new
      // block and print its name
      auto block = node->getBlock();
      fprintf(_outputFile, "(block name=\"block_%d\" ", block->getNumber());
      }
   else if (opcodeValue == TR::BBEnd)
      {
      // a BBEnd means we are exiting a block and need to "close" it
      fprintf(_outputFile, ") ");
      }
   else
      {

      // print the node's name
      fprintf(_outputFile, "(%s ", node->getOpCode().getName());

      if (opcode.isLoadConst())
          {
          // if the node is a `*const`, print the constant value
          if (opcode.isInteger())
              {
              fprintf(_outputFile, "%ld ", node->get64bitIntegralValue());
              }
          else if (opcode.isIntegerOrAddress()) // since there is no `isAddress()`
              {
              // it should be noted that although a correct address will be printed,
              // the address may not be valid if recompiling the output Tril code
              fprintf(_outputFile, "%ld ", node->getAddress());
              }
          else if (opcode.isDouble())
              {
              fprintf(_outputFile, "%f ", node->getDouble());
              }
          else if (opcode.isFloat())
              {
              fprintf(_outputFile, "%f ", static_cast<double>(node->getFloat()));
              }
          }
      else if (opcode.isLoadDirect() || opcode.isStoreDirect())
          {
          // if the node is a direct load/store, print an argument appropriate
          // for the contained symref
          auto sym = node->getSymbol();
          if (sym->isParm())
              {
              fprintf(_outputFile, "parm=%d ", sym->getParmSymbol()->getSlot());
              }
          else if (sym->isAuto())
              {
              fprintf(_outputFile, "temp=\"symref_%d\" ", node->getSymbolReference()->getCPIndex());
              }
          else
              {
              fprintf(_outputFile, "\n\n**** Unrecognized symref type **** \n\n");
              }
          }
      else if (opcode.isIndirect())
          {
          // if the node is an indirect load/store, print the offset on the symref
          fprintf(_outputFile, "offset=%ld ", node->getSymbolReference()->getOffset());
          }
      else if (opcode.isBranch())
          {
          // if the node is a branch, print the name of the target block
          auto target = node->getBranchDestination()->getEnclosingBlock(true)->getNumber();
          if (target == 1)
              {
              fprintf(_outputFile, "target=@exit ");
              }
          else
              {
              fprintf(_outputFile, "target=\"block_%d\" ", target);
              }
          }

      // use the node index as its id (used for commoning)
      fprintf(_outputFile, "id=\"n%dn\" ", node->getGlobalIndex());
      }
   }

void TR::TrilDumper::visitingCommonedChildNode(TR::Node* node)
   {
   // for commoned nodes, we can "close" the node right away since the children
   // are not re-visited
   fprintf(_outputFile, "(@id \"n%dn\") ", node->getGlobalIndex());
   }

void TR::TrilDumper::visitedAllChildrenOfNode(TR::Node* node)
   {
   auto opcode = node->getOpCode();
   auto opcodeValue = opcode.getOpCodeValue();

   // we must only "close" nodes that are neither BBStart nor BBEnd, since these
   // are used to delimit the start and end of `block` nodes
   if (opcodeValue != TR::BBStart && opcodeValue != TR::BBEnd)
      {
      fprintf(_outputFile, ") ");
      }
   }
