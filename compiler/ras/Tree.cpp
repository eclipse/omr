/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "env/KnownObjectTable.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/VPConstraint.hpp"
#include "ras/Debug.hpp"


#define DEFAULT_TREETOP_INDENT     2
#define DEFAULT_INDENT_INCREMENT   2

#define VSNPRINTF_BUFFER_SIZE      512

extern int32_t addressWidth;

void
TR_Debug::printTopLegend(TR::FILE *pOutFile)
   {
   //-index--|--------------------------------------node---------------------------------------|--address---|-----bci-----|-rc-|-vc-|-vn-|--li--|-udi-|-nc-|--sa--

   if (pOutFile == NULL) return;

   trfprintf(pOutFile, "\n------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
   trfflush(pOutFile);
   }

void
TR_Debug::printBottomLegend(TR::FILE *pOutFile)
   {
   if (pOutFile == NULL) return;

   trfprintf(pOutFile,    "\n"
                              "index:       node global index\n"
                              );
   char const *lineOrStatement = "line-number";
   char const *bciOrLoc        = "bci";
   char const *bciOrLocVerbose = "bytecode-index";

   trfprintf(pOutFile, "%s=[x,y,z]: byte-code-info [callee-index, %s, %s]\n",
                 bciOrLoc, bciOrLocVerbose, lineOrStatement);

   trfprintf(pOutFile,    "rc:          reference count\n"
                              "vc:          visit count\n"
                              "vn:          value number\n"
                              "li:          local index\n"
                              "udi:         use/def index\n"
                              "nc:          number of children\n"
                              "addr:        address size in bytes\n"
                              "flg:         node flags\n"
                        );
   trfflush(pOutFile);
   }

// Prints the symbol reference table showing only symbols that were added during the last optimization and also sets TR::Compilationl::_prevSymRefTabSize
// to the current size of symbol reference table as it is the end of this optimization.
//
void
TR_Debug::printSymRefTable(TR::FILE *pOutFile, bool printFullTable)
   {
   if (pOutFile == NULL) return;

   TR_PrettyPrinterString output(this);
   TR::SymbolReferenceTable *symRefTab = _comp->getSymRefTab();
   TR::SymbolReference *symRefIterator;
   int32_t currSymRefTabSize = symRefTab->baseArray.size();

   if (printFullTable)
      _comp->setPrevSymRefTabSize(0);

   if (currSymRefTabSize > 0 && _comp->getPrevSymRefTabSize() < currSymRefTabSize) //entry(ies) have been added to symbol refetence table since last optimization was performed
      {
      if (printFullTable)
         trfprintf(pOutFile, "\nSymbol References:\n------------------\n");
      else
         trfprintf(pOutFile, "\nSymbol References (incremental):\n--------------------------------\n");
      for (int i = _comp->getPrevSymRefTabSize() ; i < currSymRefTabSize ; i++)
         {
         if ((symRefIterator = symRefTab->getSymRef(i)))
            {
            output.reset();
            print(symRefIterator, output, false/*hideHelperMethodInfo*/, true /*verbose*/);
            trfprintf(pOutFile, "%s\n", output.getStr());
            }
         } //end for
      trfflush(pOutFile);
      } //end if

   _comp->setPrevSymRefTabSize( _comp->getSymRefTab()->baseArray.size() );
   }

void
TR_Debug::printOptimizationHeader(const char * funcName, const char * optName, int32_t optIndex, bool mustBeDone)
   {
   if (_file == NULL)
      return;

   trfprintf(_file, "<optimization id=%d name=%s method=%s>\n", optIndex, optName ? optName : "???", funcName);
   trfprintf(_file, "Performing %d: %s%s\n", optIndex, optName ? optName : "???", mustBeDone? " mustBeDone" : "");
   }

static bool valueIsProbablyHex(TR::Node *node)
   {
   switch (node->getDataType())
      {
      case TR::Int16:
         if (node->getShortInt() > 16384 || node->getShortInt() < -16384)
            return true;
         return false;
      case TR::Int32:
         if (node->getInt() > 16384 || node->getInt() < -16384)
            return true;
         return false;
      case TR::Int64:
         if (node->getLongInt() > 16384 || node->getLongInt() < -16384) return true;
         return false;
      default:
         return false;
      }
   return false;
   }

void
TR_Debug::printLoadConst(TR::FILE *pOutFile, TR::Node *node)
   {
   TR_PrettyPrinterString output(this);
   printLoadConst(node, output);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength(output.getLength());
   }

void
TR_Debug::printLoadConst(TR::Node *node, TR_PrettyPrinterString& output)
   {
   bool isUnsigned = node->getOpCode().isUnsigned();
   switch (node->getDataType())
      {
      case TR::Int8:
         if(isUnsigned)
            output.append(" %3u", node->getUnsignedByte());
         else
            output.append(" %3d", node->getByte());
         break;
      case TR::Int16:
         if (valueIsProbablyHex(node))
            output.append(" 0x%4x", node->getConst<uint16_t>());
         else
            output.append(" '%5d' ", node->getConst<uint16_t>());
         break;
      case TR::Int32:
         if(isUnsigned)
            {
            if (valueIsProbablyHex(node))
               output.append(" 0x%x", node->getUnsignedInt());
            else
               output.append(" %u", node->getUnsignedInt());
            }
         else
            {
            if (valueIsProbablyHex(node))
               output.append(" 0x%x", node->getInt());
            else
               output.append(" %d", node->getInt());
            }
         break;
      case TR::Int64:
         if(isUnsigned)
            {
            if (valueIsProbablyHex(node))
               output.append(" " UINT64_PRINTF_FORMAT_HEX, node->getUnsignedLongInt());
            else
               output.append(" " UINT64_PRINTF_FORMAT , node->getUnsignedLongInt());
            }
         else
            {
            if (valueIsProbablyHex(node))
               output.append(" " INT64_PRINTF_FORMAT_HEX, node->getLongInt());
            else
               output.append(" " INT64_PRINTF_FORMAT , node->getLongInt());
            }
         break;
      case TR::Float:
            {
            output.append(" %g [0x%08x]", node->getFloat(), node->getFloatBits());
            }
         break;
      case TR::Double:
            {
            output.append(" %g [" UINT64_PRINTF_FORMAT_HEX "]", node->getDouble(), node->getDoubleBits());
            }
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:
            {
            output.append(" %g [0x%08x]", node->getFloat(), node->getFloatBits());
            }
         break;
      case TR::DecimalDouble:
            {
            output.append(" %g [" UINT64_PRINTF_FORMAT_HEX "]", node->getDouble(), node->getDouble(), node->getDoubleBits());
            }
         break;
#ifdef SUPPORT_DFP
      case TR::DecimalLongDouble:
            {
            output.append(" %llg", node->getLongDouble());
            }
         break;
#endif
#endif
      case TR::Address:
         if (node->getAddress() == 0)
            output.append(" NULL");
         else if (!inDebugExtension() &&
                  _comp->getOption(TR_MaskAddresses))
            output.append(" *Masked*");
         else
            output.append(" " UINT64_PRINTF_FORMAT_HEX, node->getAddress());
         if (!inDebugExtension() &&
             node->isClassPointerConstant())
            {
            TR_OpaqueClassBlock *clazz = (TR_OpaqueClassBlock*)node->getAddress();
            int32_t len; char *sig = TR::Compiler->cls.classNameChars(_comp, clazz, len);
            if (clazz)
               {
               if (TR::Compiler->cls.isInterfaceClass(_comp, clazz))
                  output.append(" Interface");
               else if (TR::Compiler->cls.isAbstractClass(_comp, clazz))
                  output.append(" Abstract");
               }
            output.append(" (%.*s.class)", len, sig);
            }
         break;

      default:
         output.append(" Bad Type %s", node->getDataType().toString());
         break;
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile,  TR::CFG * cfg)
   {
   if (pOutFile == NULL)
      return;

   int32_t     numNodes = 0;
   int32_t     index;
   TR::CFGNode *node;

   // If the depth-first numbering for the blocks has already been done, use this
   // numbering to order the blocks. Otherwise use a reasonable ordering.
   //
   for (node = cfg->getFirstNode(); node; node = node->getNext())
      {
      index = node->getNumber();
      if (index < 0)
         numNodes++;
      else if (index >= numNodes)
         numNodes = index+1;
      }

   void *stackMark = 0;
   TR::CFGNode **array;

   // From here, down, stack allocations will expire when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*cfg->comp()->trMemory());

   array = (TR::CFGNode**)cfg->comp()->trMemory()->allocateStackMemory(numNodes*sizeof(TR::CFGNode*));

   memset(array, 0, numNodes*sizeof(TR::CFGNode*));
   index = numNodes;

   for (node = cfg->getFirstNode(); node; node = node->getNext())
      {
      int32_t nodeNum = node->getNumber();
      array[nodeNum>=0?nodeNum:--index] = node;
      }

   trfprintf(pOutFile, "\n<cfg>\n");

   for (index = 0; index < numNodes; index++)
      if (array[index] != NULL)
         print(pOutFile, array[index], 6);

   if (cfg->getStructure())
      {
      trfprintf(pOutFile, "<structure>\n");
      print(pOutFile, cfg->getStructure(), 6);
      trfprintf(pOutFile, "</structure>");
      }
   trfprintf(pOutFile, "\n</cfg>\n");

   }

void
TR_Debug::print(TR::FILE *outFile, TR_RegionAnalysis * regionAnalysis, uint32_t indentation)
   {
   if (outFile == NULL)
      return;

   for (int32_t index = 0; index < regionAnalysis->_totalNumberOfNodes; index++)
      {
      TR_RegionAnalysis::StructInfo &node = regionAnalysis->getInfo(index);
      if (node._structure == NULL)
         continue;

      printBaseInfo(outFile, node._structure, indentation);

      // Dump successors
      trfprintf(outFile,
              "%*sout       = [",
              indentation+11,
              " ");
      int num = 0;
      TR_RegionAnalysis::StructureBitVector::Cursor cursor(node._succ);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
        TR_RegionAnalysis::StructInfo &succ = regionAnalysis->getInfo(cursor);

        trfprintf(outFile,"%d ", succ.getNumber());

        if (num > 20)
           {
           trfprintf(outFile,"\n");
           num = 0;
           }
         num ++;
         }
      trfprintf(outFile,"]\n");

      // Dump exception successors
      trfprintf(outFile,
              "%*sexceptions= [",
              indentation+11,
              " ");
      num = 0;
      TR_RegionAnalysis::StructureBitVector::Cursor eCursor(node._exceptionSucc);
      for (eCursor.SetToFirstOne(); eCursor.Valid(); eCursor.SetToNextOne())
         {
         TR_RegionAnalysis::StructInfo &succ = regionAnalysis->getInfo(eCursor);
         trfprintf(outFile,"%d ", succ.getNumber());

         if (num > 20)
           {
           trfprintf(outFile,"\n");
           num = 0;
           }
         num ++;

         }
      trfprintf(outFile,"]\n");
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_Structure * structure, uint32_t indentation)
   {
   if (structure->asBlock())
      print(pOutFile, structure->asBlock(), indentation);
   else
      print(pOutFile, structure->asRegion(), indentation);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_RegionStructure * regionStructure, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   TR_RegionStructure *versionedLoop = NULL;
   if (!inDebugExtension() &&
       debug("fullStructure"))
      printBaseInfo(pOutFile, regionStructure, indentation);
   else
      {
      // Do indentation and dump the address of this block

      const char *type;
      if (regionStructure->containsInternalCycles())
         type = "Improper region";
      else if (regionStructure->isNaturalLoop())
         {
         versionedLoop = regionStructure->getVersionedLoop();
         if (inDebugExtension())
            type = "Natural loop (unknown version)";
         else if (versionedLoop)
            {
            TR::Block *entryBlock = regionStructure->getEntryBlock();
            if (entryBlock->isCold())
              type = "Natural loop is the slow version of the fast versioned Natural loop ";
            //else if (entryBlock->isRare())
            //  type = "Natural loop is the slow version of the fast specialized Natural loop ";
            else
              type = "Natural loop is the fast version of the slow Natural loop ";
            }
         else
            type = "Natural loop";
         }
      else
         type = "Acyclic region";

      if (versionedLoop)
        trfprintf(pOutFile,
              "%*s%d [%s] %s %d\n",
              indentation, " ",
              regionStructure->getNumber(),
              getName(regionStructure),
              type, versionedLoop->getNumber());
      else
        trfprintf(pOutFile,
              "%*s%d [%s] %s\n",
              indentation, " ",
              regionStructure->getNumber(),
              getName(regionStructure),
              type);
      }

   // Dump induction variables
   //
   if (!inDebugExtension())
      for (TR_InductionVariable *v = regionStructure->getFirstInductionVariable(); v; v = v->getNext())
         print(pOutFile, v, indentation+3);

   // Dump members
   printSubGraph(pOutFile, regionStructure, indentation+3);
   }

void
TR_Debug::printPreds(TR::FILE *pOutFile, TR::CFGNode *node)
   {
   trfprintf(pOutFile, "in={");
   int num = 0;
   for (auto edge = node->getPredecessors().begin(); edge != node->getPredecessors().end(); ++edge)
      {
      trfprintf(pOutFile, "%d ", (*edge)->getFrom()->getNumber());

      if (num > 20)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }
   num = 0;
   trfprintf(pOutFile, "} exc-in={");
   for (auto edge = node->getExceptionPredecessors().begin(); edge != node->getExceptionPredecessors().end(); ++edge)
      {
      trfprintf(pOutFile, "%d ", (*edge)->getFrom()->getNumber());

      if (num > 20)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }
   trfprintf(pOutFile, "}");
   }

void
TR_Debug::printSubGraph(TR::FILE *pOutFile, TR_RegionStructure * regionStructure, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   int offset=3;
   int num   = 0;

   TR_StructureSubGraphNode *node, *next;

   trfprintf(pOutFile,
           "%*sSubgraph: (* = exit edge)\n",
           indentation, " ");

   TR_RegionStructure::Cursor si(*regionStructure);
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      // Dump successors
      if (node->getNumber() != node->getStructure()->getNumber())
         {
         // This is an error situation, but print it to aid in debugging
         //
         trfprintf(pOutFile,
                 "%*s%d(%d) -->",
                 indentation+offset*2,
                 " ",
                 node->getNumber(),
                 node->getStructure()->getNumber());
         }
      else
         {
         trfprintf(pOutFile,
                 "%*s(%s:%s)%d -->",
                 indentation+offset*2,
                 " ",
                 getName(node), getName(node->getStructure()), node->getNumber());
         }

      num = 0;
      for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
         {
         next = toStructureSubGraphNode((*edge)->getTo());
         trfprintf(pOutFile," %d(%s)", next->getNumber(), getName(next));
         if (regionStructure->isExitEdge(*edge))
            trfprintf(pOutFile,"*");

         if (num > 10)
            {
            trfprintf(pOutFile,"\n");
            num = 0;
            }
         num++;

         }
      trfprintf(pOutFile,"\n");

      // Dump exception successors
      if (!node->getExceptionSuccessors().empty())
         {
         trfprintf(pOutFile,
                 "%*s(%s:%s)%d >>>",
                 indentation+offset*2,
                 " ",
                 getName(node), getName(node->getStructure() ), node->getNumber());
         num = 0;
         for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
            {
            next = toStructureSubGraphNode((*edge)->getTo());
            trfprintf(pOutFile," %d(%s)", next->getNumber(), getName(next));
            if (regionStructure->isExitEdge(*edge))
               trfprintf(pOutFile,"*");

            if (num > 10)
               {
               trfprintf(pOutFile,"\n");
               num = 0;
               }
            num++;
            }
         trfprintf(pOutFile,"\n");
         }
      if (node->getStructure()->getParent() != regionStructure)
         trfprintf(pOutFile,"******* Structure %d does not refer back to its parent structure\n", node->getStructure()->getNumber());
      }

   ListElement<TR::CFGEdge> *firstExitEdge = regionStructure->getExitEdges().getListHead();

   if (firstExitEdge)
      trfprintf(pOutFile,
                    "%*s%s",
                    indentation+offset, " ",
                    "Exit edges:\n");

   num = 0;
   ListElement<TR::CFGEdge> *exitEdge;
   for (exitEdge = firstExitEdge; exitEdge != NULL; exitEdge = exitEdge->getNextElement())
      {
      trfprintf(pOutFile,
              "%*s(%s)%d -->%d\n",
              indentation+offset*2,
              " ",
              getName(exitEdge->getData()->getFrom() ),
              exitEdge->getData()->getFrom()->getNumber(),
              exitEdge->getData()->getTo()->getNumber());

      if (num > 10)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num++;

      }

   if (!inDebugExtension())
      {
      static char *verbose = ::feGetEnv("TR_VerboseStructures");
      if (verbose)
         {
         trfprintf(pOutFile, "%*sPred list:\n", indentation, " ");
         si.reset();
         for (node = si.getCurrent(); node != NULL; node = si.getNext())
            {
            trfprintf(pOutFile, "%*s%d:", indentation+offset*2, " ", node->getNumber());
            printPreds(pOutFile, node);
            trfprintf(pOutFile, "\n");
            }
         for (exitEdge = firstExitEdge; exitEdge != NULL; exitEdge = exitEdge->getNextElement())
            {
            trfprintf(pOutFile, "%*s*%d:", indentation+offset*2, " ", exitEdge->getData()->getTo()->getNumber());
            printPreds(pOutFile, exitEdge->getData()->getTo());
            trfprintf(pOutFile, "\n");
            }
         }
      }

   si.reset();
   for (node = si.getCurrent(); node != NULL; node = si.getNext())
      {
      print(pOutFile, node->getStructure(), indentation);
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_InductionVariable * inductionVariable, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   int offset=3;

   trfprintf(pOutFile, "%*sInduction variable [%s]\n", indentation, " ", getName(inductionVariable->getLocal()));
   trfprintf(pOutFile, "%*sEntry value: ", indentation+offset, " ");
   print(pOutFile, inductionVariable->getEntry());
   trfprintf(pOutFile, "\n%*sExit value:  ", indentation+offset, " ");
   print(pOutFile, inductionVariable->getExit());
   trfprintf(pOutFile, "\n%*sIncrement:   ", indentation+offset, " ");
   print(pOutFile, inductionVariable->getIncr());
   trfprintf(pOutFile, "\n");
   }

static const char *structNames[] = { "Blank", "Block", "Region" };

void
TR_Debug::printBaseInfo(TR::FILE *pOutFile, TR_Structure * structure, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   // Do indentation and dump the address of this block
   trfprintf(pOutFile,
           "%*s%d [%s] %s",
           indentation, " ",
           structure->getNumber(),
           getName(structure),
           structNames[structure->getKind()]);

   trfprintf(pOutFile, "\n");
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_BlockStructure * blockStructure, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   printBaseInfo(pOutFile, blockStructure, indentation);
   if (blockStructure->getBlock()->getStructureOf() != blockStructure)
      trfprintf(pOutFile,"******* Block %d does not refer back to block structure\n", blockStructure->getBlock()->getNumber());
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::CFGNode * cfgNode, uint32_t indentation)
   {
   if (cfgNode->asBlock())
      print(pOutFile, toBlock(cfgNode), indentation);
   else
      print(pOutFile, toStructureSubGraphNode(cfgNode), indentation);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR_StructureSubGraphNode * node, uint32_t indentation)
   {
   print(pOutFile, node->getStructure(), indentation);
   }

void
TR_Debug::printNodesInEdgeListIterator(TR::FILE *pOutFile, TR::CFGEdgeList &li, bool fromNode)
   {
   TR::CFGEdge *edge;
   TR::Block   *b;
   int num = 0;
   for (auto edge = li.begin(); edge != li.end(); ++edge)
      {
      b = fromNode ? toBlock((*edge)->getFrom()) : toBlock((*edge)->getTo());
      if ((*edge)->getFrequency() >= 0)
         trfprintf(pOutFile,"%d(%d) ", b->getNumber(), (*edge)->getFrequency());
      else
         trfprintf(pOutFile,"%d ", b->getNumber());

      if (num > 20)
         {
         trfprintf(pOutFile,"\n");
         num = 0;
         }
      num ++;
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::Block * block, uint32_t indentation)
   {
   if (pOutFile == NULL)
      return;

   // Do indentation and dump the address of this block
   trfprintf(pOutFile, "%*s", indentation, " ");
   if (block->getNumber() >= 0)
      trfprintf(pOutFile, "%4d ", block->getNumber());
   trfprintf(pOutFile, "[%s] ", getName(block));

   // If there are no nodes associated with this block, it must be the entry or
   // exit node.
   if (!block->getEntry())
      {
      TR_ASSERT( !block->getExit(), "both entry and exit must be specified for TR_Block");
      if (block->getPredecessors().empty())
         trfprintf(pOutFile, "entry\n");
      else
         trfprintf(pOutFile, "exit\n");
      }
   else
      {
      // Dump information about regular block
      trfprintf(pOutFile,
              "BBStart at %s",
              getName(block->getEntry()->getNode()));
      if (block->getFrequency() >= 0)
         trfprintf(pOutFile,
                 ", frequency = %d",
                 block->getFrequency());

      static const bool enableTracePartialInlining = feGetEnv("TR_EnableTracePartialInlining") != NULL;
      if (enableTracePartialInlining)
         {
         trfprintf(pOutFile, ", partialFlags = ");
         if (block->isUnsanitizeable())
            trfprintf(pOutFile, "U, ");
         if (block->containsCall())
            trfprintf(pOutFile, "C, ");
         if (block->isRestartBlock())
            trfprintf(pOutFile, "R, ");
         if (block->isPartialInlineBlock())
            trfprintf(pOutFile, "P, ");
         }

      trfprintf(pOutFile, "\n");
      }

   bool fromNode;

   // Dump predecessors
   trfprintf(pOutFile,
           "%*sin        = [",
           indentation+11,
           " ");
   printNodesInEdgeListIterator(pOutFile, block->getPredecessors(), fromNode=true);
   trfprintf(pOutFile,"]\n");

   // Dump successors
   trfprintf(pOutFile,
           "%*sout       = [",
           indentation+11,
           " ");
   printNodesInEdgeListIterator(pOutFile, block->getSuccessors(), fromNode=false);
   trfprintf(pOutFile,"]\n");

   // Dump exception predecessors
   trfprintf(pOutFile,
           "%*sexception in  = [",
           indentation+11,
           " ");
   printNodesInEdgeListIterator(pOutFile, block->getExceptionPredecessors(), fromNode=true);
   trfprintf(pOutFile,"]\n");

   // Dump exception successors
   trfprintf(pOutFile,
           "%*sexception out = [",
           indentation+11,
           " ");
   printNodesInEdgeListIterator(pOutFile, block->getExceptionSuccessors(), fromNode=false);
   trfprintf(pOutFile,"]\n");
   }

static void printInlinePath(TR::FILE *pOutFile, TR_InlinedCallSite *site, TR_Debug *debug)
   {
   int32_t callerIndex = site->_byteCodeInfo.getCallerIndex();
   if (callerIndex == -1)
      {
      trfprintf(pOutFile, "%02d",
                   site->_byteCodeInfo.getByteCodeIndex());
      }
   else
      {
      TR::Compilation *comp = debug->comp();
      printInlinePath(pOutFile, &comp->getInlinedCallSite(callerIndex), debug);
      trfprintf(pOutFile, ".%02d",
                  site->_byteCodeInfo.getByteCodeIndex());
      }
   }

void
TR_Debug::printIRTrees(TR::FILE *pOutFile, const char * title, TR::ResolvedMethodSymbol * methodSymbol)
   {
   if (pOutFile == NULL) return;

   if (!methodSymbol)
      methodSymbol = _comp->getMethodSymbol();

   const char *hotnessString = _comp->getHotnessName(_comp->getMethodHotness());

   const char * sig = signature(methodSymbol);

   trfprintf(pOutFile, "<trees\n"
                 "\ttitle=\"%s\"\n"
                 "\tmethod=\"%s\"\n"
                 "\thotness=\"%s\">\n",
                 title, sig, hotnessString);

   // we dump the name again?
   trfprintf(pOutFile, "\n%s: for %s\n", title, sig);

   if (_comp->getMethodSymbol() == methodSymbol && _comp->getNumInlinedCallSites() > 0)
      {
      trfprintf(pOutFile, "\nCall Stack Info\n", title, sig);
      trfprintf(pOutFile, "CalleeIndex CallerIndex ByteCodeIndex CalleeMethod\n", title, sig);

      for (int32_t i = 0; i < _comp->getNumInlinedCallSites(); ++i)
         {
         TR_InlinedCallSite & ics = _comp->getInlinedCallSite(i);
         TR_ResolvedMethod  *meth = _comp->getInlinedResolvedMethod(i);
         trfprintf(pOutFile, "    %4d       %4d       %5d       ",
            i,
            ics._byteCodeInfo.getCallerIndex(),
            ics._byteCodeInfo.getByteCodeIndex());

         TR::KnownObjectTable *knot = _comp->getKnownObjectTable();
         TR::KnownObjectTable::Index targetMethodHandleIndex = TR::KnownObjectTable::UNKNOWN;
         if (knot && meth && meth->convertToMethod()->isArchetypeSpecimen() && meth->getMethodHandleLocation())
            targetMethodHandleIndex = knot->getExistingIndexAt(meth->getMethodHandleLocation());
         if (targetMethodHandleIndex != TR::KnownObjectTable::UNKNOWN)
            trfprintf(pOutFile, "obj%d.", targetMethodHandleIndex);

         trfprintf(pOutFile, "%s\n",
            _comp->compileRelocatableCode() ?
                       ((TR_AOTMethodInfo *)ics._methodInfo)->resolvedMethod->signature(comp()->trMemory(), heapAlloc) :
                       fe()->sampleSignature(ics._methodInfo, 0, 0, _comp->trMemory()));

         if (debug("printInlinePath"))
            {
            trfprintf(pOutFile, "     = ");
            printInlinePath(pOutFile, &ics, this);
            trfprintf(pOutFile, "\n");
            }
         }
      }

   _nodeChecklist.empty();
   int32_t nodeCount = 0;

   printTopLegend(pOutFile);

    TR::TreeTop *tt = methodSymbol->getFirstTreeTop();
   for (; tt; tt = tt->getNextTreeTop())
      {
      nodeCount += print(pOutFile, tt);
      TR::Node *ttNode = tt->getNode();
      if (_comp->getOption(TR_TraceLiveness)
         && methodSymbol->getAutoSymRefs()
         && ttNode->getOpCodeValue() == TR::BBStart
         && ttNode->getBlock()->getLiveLocals())
         {
         trfprintf(pOutFile, "%*s// Live locals:", addressWidth + 48, "");
         TR_BitVector *liveLocals = ttNode->getBlock()->getLiveLocals();
         // ouch, this loop is painstaking
         for (int32_t i = _comp->getSymRefTab()->getIndexOfFirstSymRef(); i < _comp->getSymRefTab()->getNumSymRefs(); i++)
            {
            TR::SymbolReference *symRef = _comp->getSymRefTab()->getSymRef(i);
            if (symRef
               && symRef->getSymbol()->isAutoOrParm()
               && liveLocals->isSet(symRef->getSymbol()->castToRegisterMappedSymbol()->getLiveLocalIndex()))
               trfprintf(pOutFile, " #%d", symRef->getReferenceNumber());
            }
         trfprintf(pOutFile, "\n");
         }
      }

   printBottomLegend(pOutFile);
   printSymRefTable(pOutFile);

   trfprintf(pOutFile, "\nNumber of nodes = %d, symRefCount = %d\n", nodeCount, _comp->getSymRefTab()->getNumSymRefs());

   //trfprintf(pOutFile, "]]>\n");
   trfprintf(pOutFile, "</trees>\n");
   trfflush(pOutFile);

   }

void
TR_Debug::printBlockOrders(TR::FILE *pOutFile, const char * title, TR::ResolvedMethodSymbol * methodSymbol)
   {

    TR::TreeTop *tt = methodSymbol->getFirstTreeTop();

   trfprintf(pOutFile, "%s block ordering:\n", title);
   unsigned numberOfColdBlocks = 0;
   while (tt != NULL)
      {
      TR::Node *node = tt->getNode();
      if (node && node->getOpCodeValue() == TR::BBStart)
         {
         TR::Block *block = node->getBlock();
         trfprintf(pOutFile, "block_%-4d\t[ " POINTER_PRINTF_FORMAT "]\tfrequency %4d", block->getNumber(), block, block->getFrequency());
         if (block->isSuperCold())
            {
            numberOfColdBlocks++;
            trfprintf(pOutFile, "\t(super cold)\n");
            }
         else if (block->isCold())
            trfprintf(pOutFile, "\t(cold)\n");
         else
            trfprintf(pOutFile, "\n");

         TR::CFGEdgeList & successors = block->getSuccessors();
         for (auto succEdge = successors.begin(); succEdge != successors.end(); ++succEdge)
            trfprintf(pOutFile, "\t -> block_%-4d\tfrequency %4d\n", (*succEdge)->getTo()->getNumber(), (*succEdge)->getFrequency());
         }
      tt = tt->getNextTreeTop();
      }
   }

TR_PrettyPrinterString::TR_PrettyPrinterString(TR_Debug *debug) {
  len = 0;
  buffer[0] = '\0';
  _comp = debug->comp();
  _debug = debug;
}

char *
TR_Debug::formattedString(char *buf, uint32_t bufLen, const char *format, va_list args, TR_AllocationKind allocationKind)
   {
   va_list args_copy;
   va_copy(args_copy, args);

#if defined(J9ZOS390)
   // vsnprintf returns -1 on zos when buffer is too small
   char s[VSNPRINTF_BUFFER_SIZE];
   int resultLen = vsnprintf(s, VSNPRINTF_BUFFER_SIZE, format, args_copy);
   if (resultLen < 0 && _file != NULL)
      trfprintf(_file, "Failed to get length of string with format %s\n", format);
#else
   int resultLen = vsnprintf(NULL, 0, format, args_copy);
#endif

   va_copy_end(args_copy);

   if (resultLen + 1 > bufLen)
      {
      bufLen = resultLen + 1;
      buf = (char*)comp()->trMemory()->allocateMemory(bufLen, allocationKind);
      }
   vsnprintf(buf, bufLen, format, args);
   return buf;
   }

void TR_PrettyPrinterString::append(const char* format, ...)
   {
   va_list args;
   va_start (args, format);
   len+=vsnprintf(buffer+len,2000-len,format,args);
   va_end(args);

   }

int32_t
TR_Debug::print(TR::FILE *pOutFile,  TR::TreeTop * tt)
   {
   return print(pOutFile, tt->getNode(), DEFAULT_TREETOP_INDENT, true);
   }

// Dump this node and its children with a standard prefix.
// Return the number of nodes encountered.
int32_t
TR_Debug::print(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation, bool printChildren)
   {
   if (pOutFile == NULL) return 0;
   TR_ASSERT(node != NULL, "node is NULL\n");

   TR::SimpleRegex * regex = NULL;

   if (regex)
      {
      char buf[20];
      sprintf(buf, "%s", getName(node));

      char * p = buf;
      while (*p!='N')
         p++;

      if (TR::SimpleRegex::match(regex, p))
         breakOn();
      }

   if (node->getOpCodeValue() == TR::dbgFence && !debug("showDebugFence"))
      {
      _nodeChecklist.set(node->getGlobalIndex());
      return 0;
      }

   // If this node has already been printed, just print a reference to it.
   //
   if (_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      printBasicPreNodeInfoAndIndent(pOutFile, node, indentation);
      trfprintf(pOutFile, "==>%s", getName(node->getOpCode()));
      if (node->getOpCode().isLoadConst())
         printLoadConst(pOutFile, node);
#ifdef J9_PROJECT_SPECIFIC
      printBCDNodeInfo(pOutFile, node);
      printDFPNodeInfo(pOutFile, node);
#endif
      trfprintf(pOutFile, "\n");
      trfflush(pOutFile);
      _comp->setNodeOpCodeLength(0); //probably redundant - just to be safe
      return 0;
      }

   _nodeChecklist.set(node->getGlobalIndex());
   printBasicPreNodeInfoAndIndent(pOutFile, node, indentation);

   int32_t nodeCount = 1; // Count this node
   int32_t i;

   printNodeInfo(pOutFile, node);
   if (!debug("disableDumpNodeFlags"))
      printNodeFlags(pOutFile, node);
   printBasicPostNodeInfo(pOutFile, node, indentation);


   trfprintf(pOutFile, "\n");

   TR_PrettyPrinterString output(this);

   if (printChildren)
      {
      indentation += DEFAULT_INDENT_INCREMENT;
      if (node->getOpCode().isSwitch())
         {
         nodeCount += print(pOutFile, node->getFirstChild(), indentation, true);

         printBasicPreNodeInfoAndIndent(pOutFile, node->getSecondChild(), indentation);
         nodeCount++;

         output.append("default ");
         _comp->incrNodeOpCodeLength( output.getLength() );
         trfprintf(pOutFile, "%s", output.getStr());
         output.reset();

         printDestination(pOutFile, node->getSecondChild()->getBranchDestination());
         printBasicPostNodeInfo(pOutFile, node->getSecondChild(), indentation);
         trfprintf(pOutFile, "\n");
         TR::Node *oldParent = getCurrentParent();
         int32_t oldChildIndex = getCurrentChildIndex();
         if (node->getSecondChild()->getNumChildren() == 1) // a GlRegDep
            {
            setCurrentParentAndChildIndex(node->getSecondChild(), 1);
            nodeCount += print(pOutFile, node->getSecondChild()->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true);
            }
         setCurrentParentAndChildIndex(oldParent, oldChildIndex);

         uint16_t upperBound = node->getCaseIndexUpperBound();

         if (node->getOpCodeValue() == TR::lookup || node->getOpCodeValue() == TR::trtLookup)
            {
            bool unsigned_case = node->getFirstChild()->getOpCode().isUnsigned();

            TR::Node *oldParent = getCurrentParent();
            int32_t oldChildIndex = getCurrentChildIndex();
            for (i = 2; i < upperBound; i++)
               {
               printBasicPreNodeInfoAndIndent(pOutFile, node->getChild(i), indentation);
               nodeCount++;

               if (node->getOpCodeValue() == TR::trtLookup)
                  {
                  CASECONST_TYPE caseVal = node->getChild(i)->getCaseConstant();
                  output.append("0x%08X: ", (uint32_t) caseVal);
                  _comp->incrNodeOpCodeLength( output.getLength() );
                  trfprintf(pOutFile, "%s", output.getStr());
                  output.reset();

                  printDestination(pOutFile, node->getChild(i)->getBranchDestination());
                  }
               else
                  {
                  if (sizeof(CASECONST_TYPE) == 8)
                     {
                     if (unsigned_case)
                        output.append(INT64_PRINTF_FORMAT_HEX ":\t", node->getChild(i)->getCaseConstant());
                     else
                        output.append(INT64_PRINTF_FORMAT ":\t", node->getChild(i)->getCaseConstant());
                     }
                  else
                     {
                     if (unsigned_case)
                        output.append("%u:\t", node->getChild(i)->getCaseConstant());
                     else
                        output.append("%d:\t", node->getChild(i)->getCaseConstant());
                     }

                  _comp->incrNodeOpCodeLength( output.getLength() );
                  trfprintf(pOutFile, "%s", output.getStr());
                  output.reset();

                  printDestination(pOutFile, node->getChild(i)->getBranchDestination());
                  }

               printBasicPostNodeInfo(pOutFile, node->getChild(i), indentation);
               trfprintf(pOutFile, "\n");
               if (node->getChild(i)->getNumChildren() == 1) // a GlRegDep
                  {
                  setCurrentParentAndChildIndex(node->getChild(i), i);
                  nodeCount += print(pOutFile, node->getChild(i)->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true);
                  }
               }
            setCurrentParentAndChildIndex(oldParent, oldChildIndex);
            }
         else
            {
            TR::Node *oldParent = getCurrentParent();
            int32_t oldChildIndex = getCurrentChildIndex();
            for (i = 2; i < upperBound; i++)
               {
               printBasicPreNodeInfoAndIndent(pOutFile, node->getChild(i), indentation);
               nodeCount++;
               output.append("%d", i-2);
               _comp->incrNodeOpCodeLength( output.getLength() );
               trfprintf(pOutFile, "%s", output.getStr());
               output.reset();

               printDestination(pOutFile, node->getChild(i)->getBranchDestination());
               printBasicPostNodeInfo(pOutFile, node->getChild(i), indentation);
               trfprintf(pOutFile, "\n");
               if (node->getChild(i)->getNumChildren() == 1) // a GlRegDep
                  {
                  setCurrentParentAndChildIndex(node->getChild(i), i);
                  nodeCount += print(pOutFile, node->getChild(i)->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true);
                  }
               }
            setCurrentParentAndChildIndex(oldParent, oldChildIndex);
            // Check for branch table address
            if (upperBound < node->getNumChildren() && node->getChild(upperBound)->getOpCodeValue() != TR::GlRegDeps)
               {
               nodeCount += print(pOutFile, node->getChild(upperBound), indentation, true);
               }
            }
         }
      else
         {
         TR::Node *oldParent = getCurrentParent();
         int32_t oldChildIndex = getCurrentChildIndex();
         for (i = 0; i < node->getNumChildren(); i++)
            {
            setCurrentParentAndChildIndex(node, i);
            nodeCount += print(pOutFile, node->getChild(i), indentation, true);
            }
         setCurrentParentAndChildIndex(oldParent, oldChildIndex);
         }
      }

   trfflush(pOutFile);
   return nodeCount;
   }

uint32_t
TR_Debug::getIntLength( uint32_t num ) const
   {
       return ( num == 0 ? 1 : ((int) log10( (double)num ) + 1) );
   }

uint32_t
TR_Debug::getNumSpacesAfterIndex( uint32_t index, uint32_t maxIndexLength ) const
   {
   uint32_t indexLength = getIntLength( index );

   return maxIndexLength > indexLength ? (maxIndexLength - indexLength) : 0;
   }

// Dump this node and its children with a fixed prefix.
// Return the number of nodes encountered.
//
int32_t
TR_Debug::printWithFixedPrefix(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation, bool printChildren, bool printRefCounts, const char *prefix)
   {
   uint32_t numSpaces,
            globalIndex;

   TR_PrettyPrinterString globalIndexPrefix(this),
                          output(this);

   if (pOutFile == NULL) return 0;

   _comp->setNodeOpCodeLength(0);

      globalIndexPrefix.append( "n" );

   globalIndex = node->getGlobalIndex();
   numSpaces = getNumSpacesAfterIndex( globalIndex, MAX_GLOBAL_INDEX_LENGTH );

   // If this node has already been printed, just print a reference to it.
   //
   if (_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      if (printRefCounts)
         {
         trfprintf(pOutFile, "%s%s%dn%*s  (%3d) %*s==>%s", prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", node->getReferenceCount(), indentation, " ", getName(node->getOpCode()));
         if (node->getOpCode().isLoadConst())
            printLoadConst(pOutFile, node);
#ifdef J9_PROJECT_SPECIFIC
         printBCDNodeInfo(pOutFile, node);
         printDFPNodeInfo(pOutFile, node);
#endif
//         trfprintf(pOutFile, " at n%d", node->getGlobalIndex());
         }
      else
         {
         trfprintf(pOutFile, "%s%s%dn%*s  %*s==>%s", prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", indentation, " ", getName(node->getOpCode()));
         if (node->getOpCode().isLoadConst())
            printLoadConst(pOutFile, node);
#ifdef J9_PROJECT_SPECIFIC
         printBCDNodeInfo(pOutFile, node);
         printDFPNodeInfo(pOutFile, node);
#endif
         //  trfprintf(pOutFile, " at n%d", node->getGlobalIndex());
         }
      if (_comp->cg()->getAppendInstruction() != NULL && node->getDataType() != TR::NoType && node->getRegister() != NULL)
         {
         output.append(" (in %s)", getName(node->getRegister()));
         _comp->incrNodeOpCodeLength( output.getLength() );
         trfprintf(pOutFile, "%s", output.getStr());
         output.reset();
         }
      if (!debug("disableDumpNodeFlags"))
         printNodeFlags(pOutFile, node);

      trfflush(pOutFile);
      return 0;
      }

   _nodeChecklist.set(node->getGlobalIndex());

   if (printRefCounts)
      trfprintf(pOutFile, "%s%s%dn%*s  (%3d) %*s",prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", node->getReferenceCount(), indentation, " ");
   else
      trfprintf(pOutFile, "%s%s%dn%*s  %*s",prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", indentation, " ");

   int32_t nodeCount = 1; // Count this node
   int32_t i;

   printNodeInfo(pOutFile, node);
   if (_comp->cg()->getAppendInstruction() != NULL && node->getDataType() != TR::NoType && node->getRegister() != NULL)
      {
      output.append(" (in %s)", getName(node->getRegister()));
      _comp->incrNodeOpCodeLength( output.getLength() );
      trfprintf(pOutFile, "%s", output.getStr());
      output.reset();
      }
   if (!debug("disableDumpNodeFlags"))
      printNodeFlags(pOutFile, node);
   printBasicPostNodeInfo(pOutFile, node, indentation);

   if (printChildren)
      {
      indentation += DEFAULT_INDENT_INCREMENT;
      if (node->getOpCode().isSwitch())
         {
         trfprintf(pOutFile, "\n");
         nodeCount += printWithFixedPrefix(pOutFile, node->getFirstChild(), indentation, true, printRefCounts, prefix);

         globalIndex = node->getSecondChild()->getGlobalIndex();
         numSpaces = getNumSpacesAfterIndex( globalIndex, MAX_GLOBAL_INDEX_LENGTH );

         trfprintf(pOutFile,"\n%s%s%dn%*s  %*s",prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", indentation, " ");
         nodeCount++;
         output.append("default ");
         _comp->incrNodeOpCodeLength( output.getLength() );
         trfprintf(pOutFile, "%s", output.getStr());
         output.reset();

         printDestination(pOutFile, node->getSecondChild()->getBranchDestination());
         printBasicPostNodeInfo(pOutFile, node->getSecondChild(), indentation);
         if (node->getSecondChild()->getNumChildren() == 1) // a GlRegDep
            nodeCount += printWithFixedPrefix(pOutFile, node->getSecondChild()->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true, printRefCounts, prefix);

         uint16_t upperBound = node->getCaseIndexUpperBound();

         if (node->getOpCodeValue() == TR::lookup || node->getOpCodeValue() == TR::trtLookup)
            {
            for (i = 2; i < upperBound; i++)
               {

               globalIndex = node->getChild(i)->getGlobalIndex();
               numSpaces = getNumSpacesAfterIndex( globalIndex, MAX_GLOBAL_INDEX_LENGTH );

               trfprintf(pOutFile,"\n%s%s%dn%*s  %*s",prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", indentation, " ");
               nodeCount++;

               if (node->getOpCodeValue() == TR::trtLookup)
                  {
                  CASECONST_TYPE caseVal = node->getChild(i)->getCaseConstant();
                  output.append("0x%08X: ", (uint32_t) caseVal);
                  _comp->incrNodeOpCodeLength( output.getLength() );
                  trfprintf(pOutFile, "%s", output.getStr());
                  output.reset();

                  printDestination(pOutFile, node->getChild(i)->getBranchDestination());
                  }
               else
                  {
                  if (sizeof(CASECONST_TYPE) == 8)
                     {
                     if (node->getFirstChild()->getOpCode().isUnsigned())
                        output.append(INT64_PRINTF_FORMAT_HEX ":\t", node->getChild(i)->getCaseConstant());
                     else
                        output.append(INT64_PRINTF_FORMAT ":\t", node->getChild(i)->getCaseConstant());
                     }
                  else
                     {
                     if (node->getFirstChild()->getOpCode().isUnsigned())
                        output.append("%u:\t", node->getChild(i)->getCaseConstant());
                     else
                        output.append("%d:\t", node->getChild(i)->getCaseConstant());
                     }
                  _comp->incrNodeOpCodeLength( output.getLength() );
                  trfprintf(pOutFile, "%s", output.getStr());
                  output.reset();

                  printDestination(pOutFile, node->getChild(i)->getBranchDestination());
                  }

               printBasicPostNodeInfo(pOutFile, node->getChild(i), indentation);

               if (node->getChild(i)->getNumChildren() == 1) // a GlRegDep
                  nodeCount += printWithFixedPrefix(pOutFile, node->getChild(i)->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true, printRefCounts, prefix);
               }
            }
         else
            {
            for (i = 2; i < upperBound; i++)
               {
               globalIndex = node->getChild(i)->getGlobalIndex();
               numSpaces = getNumSpacesAfterIndex( globalIndex, MAX_GLOBAL_INDEX_LENGTH );

               trfprintf(pOutFile,"\n%s%s%dn%*s  %*s",prefix, globalIndexPrefix.getStr(), globalIndex, numSpaces, "", indentation, " ");
               nodeCount++;
               output.append("%d", i-2);
               _comp->incrNodeOpCodeLength( output.getLength() );
               trfprintf(pOutFile, "%s", output.getStr());
               output.reset();

               printDestination(pOutFile, node->getChild(i)->getBranchDestination());
               if (node->getChild(i)->getNumChildren() == 1) // a GlRegDep
                  nodeCount += printWithFixedPrefix(pOutFile, node->getChild(i)->getFirstChild(), indentation + DEFAULT_INDENT_INCREMENT, true, printRefCounts, prefix);
               }
            // Check for branch table address
            if (upperBound < node->getNumChildren() && node->getChild(upperBound)->getOpCodeValue() != TR::GlRegDeps)
               {
               nodeCount += printWithFixedPrefix(pOutFile, node->getChild(upperBound), indentation, true, printRefCounts, prefix);
               }
            }
         }
      else
         {
         TR::Node *oldParent = getCurrentParent();
         int32_t oldChildIndex = getCurrentChildIndex();
         setCurrentParent(node);
         for (i = 0; i < node->getNumChildren(); i++)
            {
            trfprintf(pOutFile, "\n");
            setCurrentChildIndex(i);
            nodeCount += printWithFixedPrefix(pOutFile, node->getChild(i), indentation, true, printRefCounts, prefix);
            }
         setCurrentParentAndChildIndex(oldParent, oldChildIndex);
         }
      }
   trfflush(pOutFile);
   return nodeCount;
   }

void
TR_Debug::printDestination(TR::FILE *pOutFile,  TR::TreeTop *treeTop)
   {
   if (pOutFile == NULL) return;
   TR_PrettyPrinterString output(this);
   printDestination(treeTop, output);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength( output.getLength() );
   }

void
TR_Debug::printDestination( TR::TreeTop *treeTop, TR_PrettyPrinterString& output)
   {
   if (treeTop==NULL) return;

   TR::Node *node = treeTop->getNode();
   TR::Block *block = node->getBlock();
   output.append(" --> ");
   if (block->getNumber() >= 0)
      output.append("block_%d", block->getNumber());
      output.append(" BBStart at n%dn", node->getGlobalIndex());
   }

void
TR_Debug::printBasicPreNodeInfoAndIndent(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation)
   {
   uint32_t numSpaces;
   TR_PrettyPrinterString globalIndexPrefix(this);

   if (pOutFile == NULL) return;

   if (node->getOpCodeValue() == TR::BBStart &&
         !node->getBlock()->isExtensionOfPreviousBlock() &&
         node->getBlock()->getPrevBlock()) //a block that is not an extension of previous block and is not the first block
      {
      trfprintf(pOutFile, "\n");
      }

      globalIndexPrefix.append( "n" );

   numSpaces = getNumSpacesAfterIndex( node->getGlobalIndex(), MAX_GLOBAL_INDEX_LENGTH );
   trfprintf(pOutFile, "%s%dn%*s %*s", globalIndexPrefix.getStr(), node->getGlobalIndex(), numSpaces, "", indentation, " ");

   _comp->setNodeOpCodeLength( 0 );
   }

void
TR_Debug::printBasicPostNodeInfo(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation)
   {
   TR_PrettyPrinterString output(this);
   if (pOutFile == NULL) return;

   output.append("  ");
   if ((_comp->getNodeOpCodeLength() + indentation) < DEFAULT_NODE_LENGTH + DEFAULT_INDENT_INCREMENT)
      output.append( "%*s", DEFAULT_NODE_LENGTH + DEFAULT_INDENT_INCREMENT - ( _comp->getNodeOpCodeLength() + indentation ), "");

   int32_t lineNumber = -1;
   if (!inDebugExtension())
      lineNumber = _comp->getLineNumber(node);

   output.append( "[%s] ",
      //node->getOpCode().getSize(),
      //getName(node->getDataType()),
      getName(node));

   char const * bciOrLoc = "bci";

   if (lineNumber < 0)
      {
      output.append(
         "%s=[%d,%d,-] rc=", bciOrLoc,
         node->getByteCodeInfo().getCallerIndex(),
         node->getByteCodeInfo().getByteCodeIndex());
      output.append("%d", node->getReferenceCount());
      }
   else
      {
      output.append(
         "%s=[%d,%d,%d] rc=", bciOrLoc,
         node->getByteCodeInfo().getCallerIndex(),
         node->getByteCodeInfo().getByteCodeIndex(),
         lineNumber);
      output.append("%d", node->getReferenceCount());
      }

   output.append(" vc=%d", node->getVisitCount());

   if (!inDebugExtension() &&
         _comp->getOptimizer() &&
         _comp->getOptimizer()->getValueNumberInfo())
      output.append(" vn=%d", _comp->getOptimizer()->getValueNumberInfo()->getValueNumber(node));
   else
      output.append(" vn=-");

   if (node->getLocalIndex())
      output.append(" li=%d", node->getLocalIndex());
   else
      output.append(" li=-");

   if (node->getUseDefIndex())
      output.append(" udi=%d", node->getUseDefIndex());
   else
      output.append(" udi=-");

   output.append(" nc=%d", node->getNumChildren());

   if (node->getFlags().getValue())
      output.append(" flg=0x%x", node->getFlags().getValue());

   trfprintf(pOutFile, "%s", output.getStr());

   _comp->setNodeOpCodeLength( 0 ); //redundant - just to be safe
   }

void
TR_Debug::printNodeInfo(TR::FILE *pOutFile, TR::Node * node)
   {
   if (pOutFile == NULL) return;
   TR_PrettyPrinterString output(this);
   printNodeInfo(node, output, false);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength( output.getLength() );
   }

// Dump the node information for this node
void
TR_Debug::printNodeInfo(TR::Node * node, TR_PrettyPrinterString& output, bool prettyPrint)
   {
   TR_PrettyPrinterString globalIndexPrefix(this);

      globalIndexPrefix.append( "n" );


   int32_t i;

   if (!prettyPrint || (node->getOpCodeValue() != TR::BBStart && node->getOpCodeValue() != TR::BBEnd))
      {
      output.append("%s", getName(node->getOpCode()));
      }


   if (node->getOpCode().isNullCheck())
      {
      if (node->getNullCheckReference())
         output.append(" on %s%dn", globalIndexPrefix.getStr(), node->getNullCheckReference()->getGlobalIndex());
      else output.append(" on null NullCheckReference ----- INVALID tree!!");
      }
   else if (node->getOpCodeValue() == TR::allocationFence)
      {
      if(node->getAllocation())
         output.append(" on %s%dn", globalIndexPrefix.getStr(), node->getAllocation()->getGlobalIndex());
      else
         output.append(" on ALL");
      }

   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference())
      {
      bool hideHelperMethodInfo = false;
      if (node->getOpCode().isCheck()) //Omit helper method info for OpCodes that are checked
         hideHelperMethodInfo = true;

      print(node->getSymbolReference(), output, hideHelperMethodInfo);
      }
   else if (node->getOpCode().isBranch())
      {
      printDestination(node->getBranchDestination(), output);
      }
   else if (node->getOpCodeValue() == TR::exceptionRangeFence)
      {
      if (node->getNumRelocations() > 0)
         {
         if (node->getRelocationType() == TR_AbsoluteAddress)
            output.append(" Absolute [");
         else if (node->getRelocationType() == TR_ExternalAbsoluteAddress)
            output.append(" External Absolute [");
         else
            output.append(" Relative [");
         if (inDebugExtension())
            output.append("...");
         else if (!_comp->getOption(TR_MaskAddresses))
            {
            for (i = 0; i < node->getNumRelocations(); ++i)
               output.append(" " POINTER_PRINTF_FORMAT, node->getRelocationDestination(i));
            }
         output.append(" ]");
         }
      }
   else if (node->getOpCodeValue() == TR::BBStart)
      {
      TR::Block *block = node->getBlock();
      if (block->getNumber() >= 0)
         {
         //trfprintf(pOutFile, " <block_%d>",block->getNumber());
         output.append(" <block_%d>",block->getNumber());
         }

      if (block->getFrequency()>=0)
         output.append(" (freq %d)",block->getFrequency());
      if (block->isExtensionOfPreviousBlock())
         output.append(" (extension of previous block)");
      if (block->isCatchBlock() && !inDebugExtension())
         {
         int32_t length;
         const char *classNameChars = block->getExceptionClassNameChars();
         if (classNameChars)
            {
            length = block->getExceptionClassNameLength();
            output.append(" (catches %.*s)", length, getName(classNameChars, length));
            }
         else
            {
            classNameChars = "...";
            length = 3;
            output.append(" (catches %.*s)", length, classNameChars);   // passing literal string shouldn't use getName()
            }

         if (block->isOSRCatchBlock())
            {
            output.append(" (OSR handler)");
            }
         }
      if (block->isSuperCold())
         output.append(" (super cold)");
      else if (block->isCold())
         output.append(" (cold)");

      if (block->isLoopInvariantBlock())
         output.append(" (loop pre-header)");
      TR_BlockStructure *blockStructure = block->getStructureOf();
      if (blockStructure)
         {
         if (!inDebugExtension()
             && _comp->getFlowGraph()->getStructure())
            {
            TR_Structure *parent = blockStructure->getParent();
            while (parent)
               {
               TR_RegionStructure *region = parent->asRegion();
               if (region->isNaturalLoop() ||
                   region->containsInternalCycles())
                  {
                  output.append(" (in loop %d)", region->getNumber());
                  break;
                  }
               parent = parent->getParent();
               }
            TR_BlockStructure *dupBlock = blockStructure->getDuplicatedBlock();
            if (dupBlock)
               output.append(" (dup of block_%d)", dupBlock->getNumber());
            }
         }
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      TR::Block *block = node->getBlock(),
               *nextBlock;
      if (block->getNumber() >= 0)
         {
         output.append(" </block_%d>",block->getNumber());
         if (block->isSuperCold())
            output.append(" (super cold)");
         else if (block->isCold())
            output.append(" (cold)");
         }


      if ((nextBlock = block->getNextBlock()) && !nextBlock->isExtensionOfPreviousBlock()) //end of a block that is not the last block and the next block is not an extension of it
         output.append(" =====");
      }
   else if (node->getOpCode().isArrayLength())
      {
      int32_t stride = node->getArrayStride();

      if (stride > 0)
         output.append(" (stride %d)",stride);
      }
   else if (!inDebugExtension() &&
            (node->getOpCode().isLoadReg() || node->getOpCode().isStoreReg()) )
      {
      if ((node->getType().isInt64() && _comp->target().is32Bit() && !_comp->cg()->use64BitRegsOn32Bit()))
         output.append(" %s:%s ", getGlobalRegisterName(node->getHighGlobalRegisterNumber()), getGlobalRegisterName(node->getLowGlobalRegisterNumber()));
      else
         output.append(" %s ", getGlobalRegisterName(node->getGlobalRegisterNumber()));

      if (node->getOpCode().isLoadReg())
         print(node->getRegLoadStoreSymbolReference(), output);
      }
   else if (!inDebugExtension() &&
            node->getOpCodeValue() == TR::PassThrough)
      {
      // print only if under a GlRegDep
      bool isParentGlRegDep = getCurrentParent() ? (getCurrentParent()->getOpCodeValue() == TR::GlRegDeps) : false;
      if (isParentGlRegDep)
         {
         TR::DataType t = node->getDataType();
         // This is a half-hearted attempt at getting better register sizes, I know
         TR_RegisterSizes size;
         if (t == TR::Int8)       size = TR_ByteReg;
         else if (t == TR::Int16) size = TR_HalfWordReg;
         else if (t == TR::Int32) size = TR_WordReg;
         else                    size = TR_DoubleWordReg;
         if ((node->getFirstChild()->getType().isInt64() &&
              _comp->target().is32Bit()))
            output.append(" %s:%s ",
                          getGlobalRegisterName(node->getHighGlobalRegisterNumber(), size),
                          getGlobalRegisterName(node->getLowGlobalRegisterNumber(), size));
         else
            output.append(" %s ",
                          getGlobalRegisterName(node->getGlobalRegisterNumber(), size));
         }
      }
   else if(node->getOpCode().hasNoDataType())
      {
      output.append(" (%s)", getName(node->getDataType()));
      }

   if (node->getOpCode().isLoadConst())
      {
      printLoadConst(node, output);

      if (getCurrentParent() && getCurrentParent()->getOpCodeValue() == TR::newarray && getCurrentParent()->getSecondChild() == node)
         {
         output.append("   ; array type is ");
         switch (node->getInt())
            {
            case 4:
               output.append("boolean");
               break;
            case 5:
               output.append("char");
               break;
            case 6:
               output.append("float");
               break;
            case 7:
               output.append("double");
               break;
            case 8:
               output.append("byte");
               break;
            case 9:
               output.append("short");
               break;
            case 10:
               output.append("int");
               break;
            case 11:
               output.append("long");
               break;
            default:
               TR_ASSERT(0, "Wrong array type");
            }
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   printBCDNodeInfo(node, output);
   printDFPNodeInfo(node, output);
#endif
   }

// Dump the node flags for this node
// All flag printers should be called from here
void
TR_Debug::printNodeFlags(TR::FILE *pOutFile, TR::Node * node)
   {
   TR_PrettyPrinterString output(this);
   if (pOutFile == NULL) return;

   if (node->getFlags().getValue())
      {
      output.append(" (");
      if (inDebugExtension())
         output.append("...)");
      else
         {
         nodePrintAllFlags(node, output);
         output.append(")");
         }
      }

   trfprintf(pOutFile, "%s", output.getStr());

   _comp->incrNodeOpCodeLength( output.getLength() );
   }


#ifdef J9_PROJECT_SPECIFIC
void
TR_Debug::printBCDNodeInfo(TR::FILE *pOutFile, TR::Node * node)
   {
   TR_PrettyPrinterString output(this);
   printBCDNodeInfo(node, output);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength(output.getLength());
   }


void
TR_Debug::printBCDNodeInfo(TR::Node * node, TR_PrettyPrinterString& output)
   {
   if (node->getType().isBCD())
      {
      if (node->getOpCode().isStore() ||
               node->getOpCode().isCall() ||
               node->getOpCode().isLoadConst() ||
               (node->getOpCode().isConversion() && !node->getOpCode().isConversionWithFraction()))
         {
         if (node->hasSourcePrecision())
            {
            output.append(" <prec=%d (len=%d) srcprec=%d> ",
                          node->getDecimalPrecision(),
                          node->getSize(),
                          node->getSourcePrecision());
            }
         else
            {
            output.append(" <prec=%d (len=%d)> ",
                          node->getDecimalPrecision(),
                          node->getSize());
            }
         }
      else if (node->getOpCode().isLoad())
         {
         output.append(" <prec=%d (len=%d) adj=%d> ",
                       node->getDecimalPrecision(),
                       node->getSize(),
                       node->getDecimalAdjust());
         }
      else if (node->canHaveSourcePrecision())
         {
         output.append(" <prec=%d (len=%d) srcprec=%d %s=%d round=%d> ",
                       node->getDecimalPrecision(),
                       node->getSize(),
                       node->getSourcePrecision(),
                       node->getOpCode().isConversionWithFraction() ? "frac":"adj",
                       node->getOpCode().isConversionWithFraction() ? node->getDecimalFraction() : node->getDecimalAdjust(),
                       node->getDecimalRound());
         }
      else
         {
         output.append(" <prec=%d (len=%d) %s=%d round=%d> ",
                       node->getDecimalPrecision(),
                       node->getSize(),
                       node->getOpCode().isConversionWithFraction() ? "frac":"adj",
                       node->getOpCode().isConversionWithFraction() ? node->getDecimalFraction() : node->getDecimalAdjust(),
                       node->getDecimalRound());
         }
      if (!node->getOpCode().isStore())
         {
         output.append("sign=");
         if (node->hasKnownOrAssumedCleanSign() || node->hasKnownOrAssumedPreferredSign() || node->hasKnownOrAssumedSignCode())
            {
            if (node->signStateIsKnown())
               output.append("known(");
            else
               output.append("assumed(");
            if (node->hasKnownOrAssumedCleanSign())
               output.append("clean");
            if (node->hasKnownOrAssumedPreferredSign())
               output.append("%spreferred",node->hasKnownOrAssumedCleanSign() ? "/":"");
            if (node->hasKnownOrAssumedSignCode())
               output.append("%s%s",
                             node->hasKnownOrAssumedCleanSign() || node->hasKnownOrAssumedPreferredSign() ? "/":"",
                             getName(node->hasKnownSignCode() ? node->getKnownSignCode() : node->getAssumedSignCode()));
            output.append(") ");
            }
         else if (node->getOpCode().isLoad())
            {
            output.append("%s ",node->hasSignStateOnLoad()?"hasState":"noState");
            }
         else
            {
            output.append("? ");
            }
         }
      if (node->isSetSignValueOnNode())
         {
         output.append("setSign=%s ",getName(node->getSetSign()));
         }
      }
   else
      if (node->getOpCode().isConversionWithFraction())
      {
      output.append(" <frac=%d> ",node->getDecimalFraction());
      }
   else if ((node->getType()).isAggregate())
      {
      output.append(" <size=%lld bytes>", (int64_t)0);
      }
   if (node->castedToBCD())
      {
      output.append(" <castedToBCD=true> ");
      }
   }

void
TR_Debug::printDFPNodeInfo(TR::FILE *pOutFile, TR::Node * node)
   {
   TR_PrettyPrinterString output(this);
   printDFPNodeInfo(node, output);
   trfprintf(pOutFile, "%s", output.getStr());
   _comp->incrNodeOpCodeLength(output.getLength());
   }


void
TR_Debug::printDFPNodeInfo(TR::Node * node, TR_PrettyPrinterString& output)
   {
   if (node->getType().isDFP())
      {
      if (node->isDFPModifyPrecision())
         output.append(" <prec=%d> ", node->getDFPPrecision());
      }
   }
#endif

// Prints out a specification of the control flow graph in VCG format.
//
void
TR_Debug::printVCG(TR::FILE *pOutFile,  TR::CFG * cfg, const char *sig)
   {
   if (pOutFile == NULL)
      return;

   _nodeChecklist.empty();
   _structureChecklist.empty();
   if (debug("nestedVCG"))
      {
      trfprintf(pOutFile, "graph: {\n");
      trfprintf(pOutFile, "title: \"Nested Flow Graph\"\n");
      trfprintf(pOutFile, "splines: yes\n");
      trfprintf(pOutFile, "portsharing: no\n");
      trfprintf(pOutFile, "manhatten_edges: no\n");
      trfprintf(pOutFile, "layoutalgorithm: dfs\n");
      trfprintf(pOutFile, "finetuning: no\n");
      trfprintf(pOutFile, "xspace: 60\n");
      trfprintf(pOutFile, "yspace: 50\n\n");
      trfprintf(pOutFile, "node.borderwidth: 2\n");
      trfprintf(pOutFile, "node.color: white\n");
      trfprintf(pOutFile, "node.textcolor: black\n");
      trfprintf(pOutFile, "edge.color: black\n");
      trfprintf(pOutFile, "node: {title: \"Top1\" label: \"%s\" vertical_order: 0 horizontal_order: 0 textcolor: blue borderwidth: 1}\n", sig);

      if (cfg->getStructure())
         printVCG(pOutFile, cfg->getStructure());

      trfprintf(pOutFile, "\n}\n");
      }
   else if (debug("bseqVCG"))
      {
      trfprintf(pOutFile, "graph: {\n");
      trfprintf(pOutFile, "title: \"Nested Flow Graph\"\n");
      trfprintf(pOutFile, "splines: yes\n");
      trfprintf(pOutFile, "portsharing: yes\n");
      trfprintf(pOutFile, "manhatten_edges: no\n");
      trfprintf(pOutFile, "layoutalgorithm: dfs\n");
      trfprintf(pOutFile, "finetuning: no\n");
      trfprintf(pOutFile, "xspace: 60\n");
      trfprintf(pOutFile, "yspace: 50\n\n");
      trfprintf(pOutFile, "node.borderwidth: 2\n");
      trfprintf(pOutFile, "node.color: white\n");
      trfprintf(pOutFile, "node.textcolor: black\n");
      trfprintf(pOutFile, "edge.color: black\n");
      trfprintf(pOutFile, "node: {title: \"Top1\" label: \"%s\" vertical_order: 0 horizontal_order: 0 textcolor: blue borderwidth: 1}\n", sig);

      int32_t order;
       TR::TreeTop *exitTree, *treeTop;
      for (order = 1, treeTop = _comp->getStartTree();
           treeTop;
           treeTop = exitTree->getNextTreeTop(), ++order)
         {
         TR::Block *block = treeTop->getNode()->getBlock();
         exitTree = block->getExit();
         printVCG(pOutFile, block, order, 1);
         }
      printVCG(pOutFile, toBlock(cfg->getStart()), 0, 1);
      printVCG(pOutFile, toBlock(cfg->getEnd()), order, 1);

      trfprintf(pOutFile, "\n}\n");
      }
   else
      {
      trfprintf(pOutFile, "graph: {\n");
      trfprintf(pOutFile, "title: \"Linear Flow Graph\"\n");
      trfprintf(pOutFile, "splines: no\n");
      trfprintf(pOutFile, "portsharing: no\n");
      trfprintf(pOutFile, "manhatten_edges: no\n");
      trfprintf(pOutFile, "layoutalgorithm: dfs\n");
      trfprintf(pOutFile, "finetuning: no\n");
      trfprintf(pOutFile, "xspace: 60\n");
      trfprintf(pOutFile, "yspace: 50\n\n");
      trfprintf(pOutFile, "node.borderwidth: 2\n");
      trfprintf(pOutFile, "node.color: white\n");
      trfprintf(pOutFile, "node.textcolor: black\n");
      trfprintf(pOutFile, "edge.color: black\n");
      trfprintf(pOutFile, "node: {title: \"Top1\" label: \"%s\" vertical_order: 0 textcolor: blue borderwidth: 1}\n", sig);

      TR::CFGNode *node;
      for (node = cfg->getFirstNode(); node; node = node->getNext())
         printVCG(pOutFile, toBlock(node));

      trfprintf(pOutFile, "\n}\n");
      }
   }

void
TR_Debug::printVCG(TR::FILE *pOutFile, TR_Structure * structure)
   {
   if (structure->asRegion())
      printVCG(pOutFile, structure->asRegion());
   }

void
TR_Debug::printVCG(TR::FILE *pOutFile, TR_RegionStructure * regionStructure)
   {
   trfprintf(pOutFile, "graph: {\n");
   trfprintf(pOutFile, "title: \"%s\"\n", getName(regionStructure));

   printVCG(pOutFile, regionStructure->getEntry(), true);
   TR_RegionStructure::Cursor it(*regionStructure);
   TR_StructureSubGraphNode *node;
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      printVCG(pOutFile, node, false);
      }
   it.reset();
   for (node = it.getFirst(); node != NULL; node = it.getNext())
      {
      printVCGEdges(pOutFile, node);
      }

   trfprintf(pOutFile, "}\n");
   }

void
TR_Debug::printVCG(TR::FILE *pOutFile, TR_StructureSubGraphNode * node, bool isEntry)
   {
   if (_structureChecklist.isSet(node->getNumber()))
      return;
   _structureChecklist.set(node->getNumber());

   trfprintf(pOutFile, "node: {title: \"%s\" ", getName(node));
   trfprintf(pOutFile, "label: \"%d\" ", node->getNumber());
   if (isEntry)
      trfprintf(pOutFile, "vertical_order: 1 ");
   if (node->getStructure() == NULL) //exit destination
      trfprintf(pOutFile, "color: red}\n");
   else
      {
      if (node->getStructure()->asRegion())
         trfprintf(pOutFile, "color: lightcyan ");
      trfprintf(pOutFile, "}\n");
      printVCG(pOutFile, node->getStructure());
      }
   }

void
TR_Debug::printVCGEdges(TR::FILE *pOutFile, TR_StructureSubGraphNode * node)
   {
   for (auto edge = node->getSuccessors().begin(); edge != node->getSuccessors().end(); ++edge)
      {
      TR_StructureSubGraphNode *to = toStructureSubGraphNode((*edge)->getTo());
      printVCG(pOutFile, to, false); //print it out if it is an exit destination
      trfprintf(pOutFile, "edge: { sourcename: \"%s\" targetname: \"%s\" }\n", getName(node), getName(to));
      }

   for (auto edge = node->getExceptionSuccessors().begin(); edge != node->getExceptionSuccessors().end(); ++edge)
      {
      TR_StructureSubGraphNode *to = toStructureSubGraphNode((*edge)->getTo());
      printVCG(pOutFile, to, false); //print it out if it is an exit destination
      trfprintf(pOutFile, "edge: { sourcename: \"%s\" targetname: \"%s\" color: pink}\n", getName(node), getName(to));
      }
   }

//                                                      no-opt   cold    warm         hot            veryhot scorching reducedWarm  unknown
static const char * blockColours[numHotnessLevels] = { "white", "blue", "lightblue", "lightyellow", "gold", "red", "orange",    "white" };
static const char *  edgeColours[numHotnessLevels] = { "black", "blue", "lightblue", "lightyellow", "gold", "red", "orange",    "black" };

void
TR_Debug::printVCG(TR::FILE *pOutFile, TR::Block * block, int32_t vorder, int32_t horder)
   {
   if (pOutFile == NULL) return;

    TR::CFG *cfg = _comp->getFlowGraph();

   trfprintf(pOutFile, "node: {title: \"%d\" ", block->getNumber());
   if (!block->getEntry())
      {
      TR_ASSERT( !block->getExit(), "both entry and exit must be specified for TR_Block");
      if (block->getPredecessors().empty())
         trfprintf(pOutFile, "vertical_order: 0 label: \"Entry\" shape: ellipse color: lightgreen ");
      else
         trfprintf(pOutFile, "label: \"Exit\" shape: ellipse color: lightyellow ");
      }
   else
      {
      trfprintf(pOutFile, "label: \"%d",block->getNumber());
      trfprintf(pOutFile,"\" ");

      trfprintf(pOutFile, "color: %s ", blockColours[unknownHotness]);
      if (vorder != -1)
         trfprintf(pOutFile, "vertical_order: %d ", vorder);
      if (horder != -1)
         trfprintf(pOutFile, "horizontal_order: %d ", horder);
      }
   trfprintf(pOutFile, "}\n");

   TR::Block *b;
   for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
      {
      b = toBlock((*edge)->getTo());
      if (b->getNumber() >= 0)
         {
         trfprintf(pOutFile, "edge: { sourcename: \"%d\" targetname: \"%d\" color: %s}\n",
            block->getNumber(), b->getNumber(), edgeColours[unknownHotness]);
         }
      }

   for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
      {
      b = toBlock((*edge)->getTo());
      if (b->getNumber() >= 0)
         {
         trfprintf(pOutFile, "edge: { sourcename: \"%d\" targetname: \"%d\" linestyle: dotted label: \"exception\" color: %s }\n",
            block->getNumber(), b->getNumber(), edgeColours[unknownHotness]);
         }
      }
   }

// Output a representation of this node in the VCG output
//
void
TR_Debug::printVCG(TR::FILE *pOutFile, TR::Node * node, uint32_t indentation)
   {
   if (pOutFile == NULL) return;

   int32_t i;

   if (node->getOpCodeValue() == TR::dbgFence)
      {
      _nodeChecklist.set(node->getGlobalIndex());
      return;
      }

   if (_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      trfprintf(pOutFile, "%*s==>%s at %s\\n", 12 + indentation, " ", getName(node->getOpCode()), getName(node));
      return;
      }

   _nodeChecklist.set(node->getGlobalIndex());
   trfprintf(pOutFile, "%s  ", getName(node));
   trfprintf(pOutFile, "%*s", indentation, " ");
   printNodeInfo(pOutFile, node);
   trfprintf(pOutFile, "\\n");

   indentation += 5;

   if (node->getOpCode().isSwitch())
      {
      trfprintf(pOutFile, "%*s ***can't print switches yet***\\n", indentation+10, " ");
      }
   else
      {
      for (i = 0; i < node->getNumChildren(); i++)
         printVCG(pOutFile, node->getChild(i), indentation);
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::VPConstraint *info)
   {
   if (pOutFile == NULL)
      return;

   if (!info)
      {
      trfprintf(pOutFile, "none");
      return;
      }

   if (info->asIntConst())
      {
      trfprintf(pOutFile, "%dI", info->getLowInt());
      return;
      }
   if (info->asIntRange())
      {
      if (info->getLowInt() == TR::getMinSigned<TR::Int32>())
         trfprintf(pOutFile, "(TR::getMinSigned<TR::Int32>() ");
      else
         trfprintf(pOutFile, "(%d ", info->getLowInt());
      if (info->getHighInt() == TR::getMaxSigned<TR::Int32>())
         trfprintf(pOutFile, "to TR::getMaxSigned<TR::Int32>())");
      else
         trfprintf(pOutFile, "to %d)", info->getHighInt());
      trfprintf(pOutFile, "I");
      return;
      }
   if (info->asLongConst())
      {
      trfprintf(pOutFile, INT64_PRINTF_FORMAT "L", info->getLowLong());
      return;
      }

   if (info->asLongRange())
      {
      if (info->getLowLong() == TR::getMinSigned<TR::Int64>())
         trfprintf(pOutFile, "(TR::getMinSigned<TR::Int64>() ");
      else
         trfprintf(pOutFile, "(" INT64_PRINTF_FORMAT " ", info->getLowLong());
      if (info->getHighLong() == TR::getMaxSigned<TR::Int64>())
         trfprintf(pOutFile, "to TR::getMaxSigned<TR::Int64>())");
      else
         trfprintf(pOutFile, "to " INT64_PRINTF_FORMAT ")", info->getHighLong());
      trfprintf(pOutFile, "L");
      return;
      }
   trfprintf(pOutFile, "unprintable constraint");
   }

const char *
TR_Debug::getName(TR::DataType type)
   {
   return TR::DataType::getName(type);
   }

// this should be the same as in OMRDataTypes.cpp
static const char *pRawSignCodeNames[TR_NUM_RAW_SIGN_CODES] =
   {
   "bcd_sign_unknown",
   "0xc",
   "0xd",
   "0xf",
   };

const char *
TR_Debug::getName(TR_RawBCDSignCode type)
   {
   if (type < TR_NUM_RAW_SIGN_CODES)
      return pRawSignCodeNames[type];
   else
      return "unknown sign";
   }

const char *
TR_Debug::getName(TR::ILOpCodes opCode)
   {
   return TR::ILOpCode(opCode).getName();
   }

const char *
TR_Debug::getName(TR::ILOpCode opCode)
   {
   return opCode.getName();
   }

void
TR_Debug::verifyGlobalIndices(TR::Node * node, TR::Node **nodesByGlobalIndex)
   {
   TR::Node *nodeFromArray = nodesByGlobalIndex[node->getGlobalIndex()];
   if (nodeFromArray == node)
      return;
   TR_ASSERT(nodeFromArray == NULL, "Found two nodes with the same global index: %p and %p", nodeFromArray, node);
   nodesByGlobalIndex[node->getGlobalIndex()] = node;

   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      verifyGlobalIndices(node->getChild(i), nodesByGlobalIndex);
   }

void
TR_Debug::verifyTrees(TR::ResolvedMethodSymbol *methodSymbol)
   {
#ifndef ASSUMES
   if (getFile() == NULL)
      return;
#endif

   // Pre-allocate the bitvector to the correct size.
   // This prevents un-necessary growing and memory waste
   _nodeChecklist.set(comp()->getNodeCount()+1);

   _nodeChecklist.empty();
    TR::TreeTop * tt, * firstTree = methodSymbol->getFirstTreeTop();
   for (tt = firstTree; tt; tt = tt->getNextTreeTop())
      {
      TR::Node * node = tt->getNode();
      node->setLocalIndex(0);
      verifyTreesPass1(node);
      }

   _nodeChecklist.empty();
   for (tt = firstTree; tt; tt = tt->getNextTreeTop())
      verifyTreesPass2(tt->getNode(), true);

   size_t size = _comp->getNodeCount() * sizeof(TR::Node*);
   TR::Node **nodesByGlobalIndex = (TR::Node**)_comp->trMemory()->allocateStackMemory(size);
   memset(nodesByGlobalIndex, 0, size);
   for (tt = firstTree; tt; tt = tt->getNextTreeTop())
      verifyGlobalIndices(tt->getNode(), nodesByGlobalIndex);
   }

// Node verification. This is done in 2 passes. In pass 1 the reference count is
// accumulated in the node's localIndex. In pass 2 the reference count is checked.
//
void
TR_Debug::verifyTreesPass1(TR::Node *node)
   {
   // If this is the first time through this node, verify the children
   //
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());

      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         {
         TR::Node *child = node->getChild(i);
         if (_nodeChecklist.isSet(child->getGlobalIndex()))
            {
            // Just inc simulated ref count
            child->incLocalIndex();
            }
         else
            {
            // Initialize simulated ref count and visit it
            child->setLocalIndex(1);
            verifyTreesPass1(child);
            }

         // Check the type of the child against the expected type
         //
         auto expectedType = node->getOpCode().expectedChildType(i);
         if (node->getOpCodeValue() == TR::multianewarray &&
             i == node->getNumChildren()-1)
            {
            expectedType = TR::Address;
            }
            
         if (debug("checkTypes") && expectedType != TR::NoType)
            {
            // See if the child's type is compatible with this node's type
            // Temporarily allow known cases to succeed
            //
            TR::ILOpCodes conversionOp = TR::BadILOp;
            TR::DataType childType = child->getDataType();
            if (childType != expectedType &&
                childType != TR::NoType &&
                !((node->getOpCodeValue() == TR::imul || node->getOpCodeValue() == TR::ishl) && child->getOpCodeValue() == TR::loadaddr))
               {
               if (getFile() != NULL)
                  {
                  trfprintf(getFile(),
                            "TREE VERIFICATION ERROR -- node [%s] has wrong type for child [%s] (%s), expected %s\n",
                            getName(node),
                            getName(child),
                            getName(childType),
                            getName(expectedType));
                  }
               TR_ASSERT( debug("fixTrees"), "Tree verification error");
               }
            }
         }
      }
   }

void
TR_Debug::verifyTreesPass2(TR::Node *node, bool isTreeTop)
   {

   // Verify the reference count. Pass 1 should have set the localIndex to the
   // reference count.
   //
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());

      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         verifyTreesPass2(node->getChild(i), false);

      if (isTreeTop)
         {
         if (node->getReferenceCount() != 0)
            {
            if (getFile() != NULL)
               trfprintf(getFile(), "TREE VERIFICATION ERROR -- treetop node [%s] with ref count %d\n",
                    getName(node), node->getReferenceCount());
            TR_ASSERT( debug("fixTrees"), "Tree verification error");
            node->setReferenceCount(0);
            }
         }

      if (node->getReferenceCount() > 1 &&
          (node->getOpCodeValue() == TR::call || node->getOpCodeValue() == TR::calli))
         {
         if (getFile() != NULL)
            trfprintf(getFile(), "TREE VERIFICATION ERROR -- void call node [%s] with ref count %d\n",
                  getName(node), node->getReferenceCount());
         TR_ASSERT( debug("fixTrees"), "Tree verification error");
         }

      if (node->getReferenceCount() != node->getLocalIndex())
         {
         if (getFile() != NULL)
            trfprintf(getFile(), "TREE VERIFICATION ERROR -- node [%s] ref count is %d and should be %d\n",
                 getName(node), node->getReferenceCount(), node->getLocalIndex());
         TR_ASSERT(debug("fixTrees"), "Tree verification error");
         // if there is logging, don't fix the ref count!
         if (getFile() == NULL)
            node->setReferenceCount(node->getLocalIndex());
         }
      }
   }

TR::Node *
TR_Debug::verifyFinalNodeReferenceCounts(TR::ResolvedMethodSymbol *methodSymbol)
   {
   _nodeChecklist.empty();

   TR::Node *firstBadNode = NULL;
   for ( TR::TreeTop *tt = methodSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *badNode = verifyFinalNodeReferenceCounts(tt->getNode());
      if (!firstBadNode)
         firstBadNode = badNode;
      }

   if (getFile() != NULL)
      trfflush(getFile());

   if (debug("enforceFinalNodeReferenceCounts"))
      {
      TR_ASSERT(!firstBadNode,
         "Node [%s] final ref count is %d and should be zero\n",
         getName(firstBadNode),
         firstBadNode->getReferenceCount());
      }

   return firstBadNode;
   }

TR::Node *
TR_Debug::verifyFinalNodeReferenceCounts(TR::Node *node)
   {
   TR::Node *badNode = NULL;
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());

      // Check this node
      //
      if (node->getReferenceCount() != 0)
         {
         badNode = node;
         if (getFile() != NULL)
            trfprintf(getFile(), "WARNING -- node [%s] has final ref count %d and should be zero\n",
                 getName(badNode), badNode->getReferenceCount());
         }

      // Recursively check its children
      //
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         TR::Node *badChild = verifyFinalNodeReferenceCounts(node->getChild(i));
         if (!badNode)
            badNode = badChild;
         }
      }

   return badNode;
   }

// Verifies the number of times a node is referenced within a block
//
void
TR_Debug::verifyBlocks(TR::ResolvedMethodSymbol * methodSymbol)
   {
#ifndef ASSUMES
   if (getFile() == NULL)
      return;
#endif

    TR::TreeTop *tt, *exitTreeTop;
   for (tt = methodSymbol->getFirstTreeTop(); tt; tt = exitTreeTop->getNextTreeTop())
      {
       TR::TreeTop * firstTreeTop = tt;
      exitTreeTop = tt->getExtendedBlockExitTreeTop();

      _nodeChecklist.empty();
      for (; tt != exitTreeTop->getNextTreeTop(); tt = tt->getNextTreeTop())
         {
         TR_ASSERT( tt, "TreeTop problem in verifyBlocks");
         TR::Node *node = tt->getNode();
         node->setLocalIndex(node->getReferenceCount());
         verifyBlocksPass1(node);
         }

      _nodeChecklist.empty();

      // go back to the start of the block, and check the localIndex to make sure it is 0
      // do not walk the tree backwards as this causes huge stack usage in verifyBlocksPass2
      _nodeChecklist.empty();
      for (tt = firstTreeTop; tt != exitTreeTop->getNextTreeTop(); tt = tt->getNextTreeTop())
         verifyBlocksPass2(tt->getNode());
      }
   }

// Child access within a Block verification. In pass 1, the verifyRefCount for each child is
// decremented for each visit.  The second pass is to make sure that the verifyRefCount is zero
// by the end of the block.
//
void
TR_Debug::verifyBlocksPass1(TR::Node *node)
   {

   // If this is the first time through this node, verify the children
   //
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());
      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         {
         TR::Node *child = node->getChild(i);
         if (_nodeChecklist.isSet(child->getGlobalIndex()))
            {
            // If the child has already been visited, decrement its verifyRefCount.
            child->decLocalIndex();
            }
         else
            {
            // If the child has not yet been visited, set its localIndex and visit it
            child->setLocalIndex(child->getReferenceCount() - 1);
            verifyBlocksPass1(child);
            }
         }
      }
   }

void
TR_Debug::verifyBlocksPass2(TR::Node *node)
   {

   // Pass through and make sure that the localIndex == 0 for each child
   //

   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());
      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         verifyBlocksPass2(node->getChild(i));

      if (node->getLocalIndex() != 0)
         {
         char buffer[150];
         sprintf(buffer, "BLOCK VERIFICATION ERROR -- node [%s] accessed outside of its (extended) basic block: %d time(s)\n",
                 getName(node), node->getLocalIndex());
         if (getFile() != NULL)
            trfprintf(getFile(), buffer);
         TR_ASSERT( debug("fixTrees"), buffer);
         }
      }
   }
