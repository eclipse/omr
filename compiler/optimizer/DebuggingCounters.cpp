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

#include "optimizer/DebuggingCounters.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "limits.h"
#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

namespace TR { class SymbolReference; }

/**
 *   Linked list of named counter infos.
 */
NamedCounterInfo * TR_DebuggingCounters::namedCounterInfos = NULL;

/**
 *   Return the counter info for the name provided.   Create if missing
 */
NamedCounterInfo* TR_DebuggingCounters::getOrCreateNamedCounter(TR::Compilation *comp, const char * name, int32_t d, int32_t bucketSize)
   {
   const char *hotnessName = comp->getHotnessName(comp->getMethodHotness());

   char appendedName[100];
   strcpy(appendedName, hotnessName);
   strcpy(appendedName + strlen(hotnessName)," : ");
   strcpy(appendedName + strlen(hotnessName) + 3, name);
   // Do a linear search for the counter of this name
   // If performance becomes a problem this can become a binary search,
   // but if used for debugging counters only this is probably not
   // necessary.
   //
   NamedCounterInfo *prevSimilarCounter = NULL;
   for (NamedCounterInfo * counterInfo = namedCounterInfos; counterInfo != NULL; counterInfo = counterInfo->_next)
      {
      if (0 == strcmp(counterInfo->counterName,appendedName))
         {
         if (0 == (d/counterInfo->bucketSize - counterInfo->delta))
            {
            // Found it existing
            return counterInfo;
            }
         prevSimilarCounter = counterInfo;
         }
      }

   // Named counter not found.   Create a new one.
   NamedCounterInfo * newCounterInfo = (NamedCounterInfo *)jitPersistentAlloc(sizeof(NamedCounterInfo));

   // Initialize the counter info with the name, and zero counts
   newCounterInfo->counterName = (char*)jitPersistentAlloc((strlen(hotnessName) + 3 + strlen(name)) * sizeof(char) + 1);
   strcpy(newCounterInfo->counterName,hotnessName);
   strcpy(newCounterInfo->counterName + strlen(hotnessName)," : ");
   strcpy(newCounterInfo->counterName + strlen(hotnessName) + 3,name);
   newCounterInfo->smallCount = 0;
   newCounterInfo->totalCount = 0;
   newCounterInfo->compilationCount = 0;
   newCounterInfo->bucketSize = bucketSize;
   newCounterInfo->delta = d/bucketSize;

   if (!prevSimilarCounter)
      {
      //
      // Insert it in the list of infos by making it the new head of the list
      //
      newCounterInfo->_next = namedCounterInfos;
      namedCounterInfos = newCounterInfo;
      }
   else
      {
      NamedCounterInfo *nextCounter = prevSimilarCounter->_next;
      newCounterInfo->_next = nextCounter;
      prevSimilarCounter->_next = newCounterInfo;
      }

   return newCounterInfo;
   }


/**
 *   Insert a counter after the given tree top:
 *
 * @name:   the name of the counter
 * @tt:   the treetop to insert the counter before
 */
void TR_DebuggingCounters::insertCounter(const char * name, TR::Compilation * comp, TR::TreeTop * tt, int32_t d)
   {
   // If we're given a null instruction to insert before, we can't do anything
   //
   if (!tt)
      return;

   int32_t bucketSize = comp->getOptions()->getInlineCntrAllBucketSize();

   if (comp->getOptions()->insertDebuggingCounters())
      {
      if (!strncmp(name, "callee has too many bytecodes", 29))
         bucketSize = comp->getOptions()->getInlineCntrCalleeTooBigBucketSize();
      else if (!strncmp(name, "cold callee has too many bytecodes", 34))
         bucketSize = comp->getOptions()->getInlineCntrColdAndNotTinyBucketSize();
      else if (!strncmp(name, "warm callee has too many bytecodes", 34))
         bucketSize = comp->getOptions()->getInlineCntrWarmCalleeTooBigBucketSize();
      else if (!strncmp(name, "caller exceeded inline budget", 29))
         bucketSize = comp->getOptions()->getInlineCntrRanOutOfBudgetBucketSize();
      else if (!strncmp(name, "callee graph has too many bytecodes", 35))
         bucketSize = comp->getOptions()->getInlineCntrCalleeTooDeepBucketSize();
      else if (!strncmp(name, "callee has too many nodes", 25))
         bucketSize = comp->getOptions()->getInlineCntrWarmCalleeHasTooManyNodesBucketSize();
      else if (!strncmp(name, "caller has too many nodes", 25))
         bucketSize = comp->getOptions()->getInlineCntrWarmCallerHasTooManyNodesBucketSize();
      else if (!strncmp(name, "inline depth exceeded", 21))
         bucketSize = comp->getOptions()->getInlineCntrDepthExceededBucketSize();
      }

   // Get (or create) the named counter info for the name passed.
   NamedCounterInfo* counterInfo = getOrCreateNamedCounter(comp, name, d, bucketSize);

   // Record the number of times saw this counter during compilation
   //
   counterInfo->compilationCount++;

   // Insert the instrumentation only if the command line option is
   // used
   if (comp->getOptions()->insertDebuggingCounters())
      {
      // Insert instrumentation to increment the counter

      // The instrumentation in the code increments the small count
      SMALL_COUNTER_TYPE * ctrAddress = &(counterInfo->smallCount);
      TR::SymbolReference* counterRef = comp->getSymRefTab()->createKnownStaticDataSymbolRef(ctrAddress, TR::Int32);

      // Treetops for counterRef = counterRef + 1;

      TR::Node * node = tt->getNode();
      TR::Node* loadNode = TR::Node::createWithSymRef(node, TR::iload, 0, counterRef);

      TR::Node* addNode =

         TR::Node::create(TR::iadd, 2, loadNode,
		         TR::Node::create(node, TR::iconst, 0, 1));


      TR::TreeTop* incrementTree =
         TR::TreeTop::create(comp, TR::Node::createWithSymRef(TR::istore, 1, 1,
					          addNode, counterRef));

      tt->getPrevTreeTop()->insertAfter(incrementTree);
      }
   }


/**
 * Dump the counters
 */
void TR_DebuggingCounters::report()
   {
   NamedCounterInfo * counterInfo;
   if (output)
      fflush(output);
   else
      output = stdout;

   // Make sure large counts are updated with the recent info
   //
   transferSmallCountsToTotalCounts();

   LARGE_COUNTER_TYPE dynamicSum = 0;
   SMALL_COUNTER_TYPE compilationSum = 0;

   // Iterate through to compute the sum
   //
   for (counterInfo = namedCounterInfos; counterInfo != NULL; counterInfo = counterInfo->_next)
      {
      dynamicSum += counterInfo->totalCount;
      compilationSum += counterInfo->compilationCount;
      }

   if (dynamicSum == 0)
      {
      //fprintf(output, "\nDEBUGGING COUNTERS REPORT: NO DYNAMIC COUNTS\n\n");
      return;
      }

   fprintf(output, "\nDEBUGGING COUNTERS REPORT: \n\n");
   fprintf(output, "\nName: [                    counterName (delta)] dynamic : (  %% ) static : (  %% )  \n\n");

   // Now go through and print them all
   //
   for (counterInfo = namedCounterInfos; counterInfo != NULL; counterInfo = counterInfo->_next)
      {
      // Only print if there is actually a non-zero count to avoid noisy report
      if (counterInfo->totalCount > 0)
         {
         int32_t deviation = ((counterInfo->delta + 1)*counterInfo->bucketSize);

         if (deviation != INT_MAX)
            fprintf(output, "Name: [%31s (%5d)] dynamic : (%5.2lf ) static : (%5.2lf ) [%lu]\n",
	   	             counterInfo->counterName,
                   ((counterInfo->delta + 1)*counterInfo->bucketSize),
                   //(uint32_t) counterInfo->totalCount,
                   (double) (((double) (counterInfo->totalCount*100)/((double) dynamicSum))),
                   //counterInfo->compilationCount,
                   (double) ((double) (counterInfo->compilationCount*100)/((double) compilationSum)),
                   counterInfo->totalCount);
//            fprintf(output, "DEBUG: totalCount = %x\n",counterInfo->totalCount);
         else
            fprintf(output, "Name: [%38s ] dynamic : (%5.2lf ) static : (%5.2lf )\n",
	   	             counterInfo->counterName,
                   //(uint32_t) counterInfo->totalCount,
                   (double) (((double) (counterInfo->totalCount*100)/((double) dynamicSum))),
                   //counterInfo->compilationCount,
                   (double) ((double) (counterInfo->compilationCount*100)/((double) compilationSum)));
//            fprintf(output, "DEBUG: totalCount = %x\n",counterInfo->totalCount);
	       }
      }

   fprintf(output, "Compilation sum %d Dynamic sum %lu \n", (int32_t) compilationSum, dynamicSum);
   fprintf(output, "\n");

   if (output != stdout)
      fclose(output);

   fflush(output);
   }

/**
 *   Go through the int counters and move the counts into the large counters (to avoid overflow)
 */
void TR_DebuggingCounters::transferSmallCountsToTotalCounts()
   {
   for (NamedCounterInfo * counterInfo = namedCounterInfos; counterInfo != NULL; counterInfo = counterInfo->_next)
      {
      counterInfo->totalCount += counterInfo->smallCount;
      counterInfo->smallCount = 0;
      }
   }

/**
 *   Called at the beginning of compilation
 */
void TR_DebuggingCounters::initializeCompilation()
   {
   // Clear the list of counted call sites
   countedCallSiteList = NULL;
   }


/****************
 *
 *   The routines below are specific to inlining counters, but use the
 *   generic debugging counters above as the implementation
 *
 ******************/

::FILE* TR_DebuggingCounters::output = NULL;

/**
 *   name:   the name of the counter
 *   tt:   the treetop to insert the counter before
 */
void TR_DebuggingCounters::insertInliningCounter(const char * name, TR::Compilation * comp, TR::TreeTop * callTreeTop, int32_t size, int32_t d, int32_t numLocals)
   {
   if (!comp->getOptions()->insertDebuggingCounters())
      return;
   // If we're given a null instruction to insert before, we can't do anything
   if (!callTreeTop)
      return;

   bool siteIsNew = insertIfMissing(comp, name, false, callTreeTop, size, numLocals);

   if (siteIsNew)
      {
      insertCounter(name,comp,callTreeTop, d);
      }
   }

void TR_DebuggingCounters::insertInliningCounterOK(const char * name, TR::Compilation * comp, TR::TreeTop *origTT, TR::TreeTop * callTreeTop, int32_t size, int32_t numLocals)
   {
   if (!comp->getOptions()->insertDebuggingCounters())
      return;
   // If we're given a null instruction to insert before, we can't do anything
   if (!callTreeTop)
      return;

   bool siteIsNew = insertIfMissing(comp, name, true, origTT, size, numLocals);
   if (siteIsNew)
      {
      insertCounter(name,comp,callTreeTop, 0);
      }
   }

void TR_DebuggingCounters::inliningReportForMethod(TR::Compilation *comp)
   {
   CountedCallSite * curSite = countedCallSiteList;

   if (output == NULL)
      output = fopen("inlinereport.txt", "wt");

   fprintf(output,"------------------------------------------------------------------------------------------------------------------\n");
   fprintf(output,"%s\n", comp->signature());
   while (curSite != NULL)
      {
      if (!curSite->method && !curSite->opaque_method)
         {
         fprintf(output, "\t%d %d %d %d %d %s \t%s\n", curSite->flag, -1,
                  curSite->frequency, curSite->size,
                  curSite->numLocals, curSite->name, "Unknown");

         curSite = curSite->_next;
         continue;
         }
      else if (curSite->bcInfo.getCallerIndex() < 0)
         {
#ifdef J9_PROJECT_SPECIFIC
         char buffer[512];

         if (curSite->opaque_method)
            fprintf(output, "\t%d %d %d %d %d %s \t%s\n", curSite->flag, (comp->fej9()->getIProfilerCallCount(curSite->bcInfo, comp)),
                  curSite->frequency, curSite->size,
                  curSite->numLocals, curSite->name, comp->fe()->sampleSignature(curSite->opaque_method, buffer, 512, comp->trMemory()));
         else
            fprintf(output, "\t%d %d %d %d %d %s \t%s\n", curSite->flag, (comp->fej9()->getIProfilerCallCount(curSite->bcInfo, comp)),
                  curSite->frequency, curSite->size,
                  curSite->numLocals, curSite->name, curSite->method->signature(comp->trMemory()));
#endif
         }


       curSite = curSite->_next;
       }

   fprintf(output,"------------------------------------------------------------------------------------------------------------------\n");
   }


/**
 *   This list is used to avoid instrumenting the same call site twice.
 */
CountedCallSite * TR_DebuggingCounters::countedCallSiteList = NULL;


/**
 *   Returns true if was missing and inserted, false if it already existed
 */
bool TR_DebuggingCounters::insertIfMissing(TR::Compilation * comp, const char * name, bool flag, TR::TreeTop * tt, int32_t size, int32_t numLocals)
   {
   CountedCallSite * curSite = countedCallSiteList;
   while (curSite != NULL)
      {
      if (curSite->callTreeTop == tt)
         {
         return false;   // already existed
         }
      curSite = curSite->_next;
      }

   CountedCallSite * newHead = (CountedCallSite *) comp->trMemory()->allocateHeapMemory(sizeof(CountedCallSite));
   newHead->_next = countedCallSiteList;
   newHead->callTreeTop = tt;
   strcpy(newHead->name, name);
   newHead->flag = flag;
   newHead->size = size;
   newHead->numLocals = numLocals;

   newHead->frequency = -1;
   while (tt && (newHead->frequency == -1))
      {
      while (tt->getNode()->getOpCodeValue() != TR::BBStart) tt = tt->getPrevTreeTop();

      TR::Block *block = NULL;
      if (tt) block = tt->getNode()->getBlock();
      if (block && tt->getNode()->getInlinedSiteIndex()<0) newHead->frequency = block->getFrequency();

      tt = tt->getPrevTreeTop();
      }

   if (!newHead->callTreeTop->getNode() || newHead->callTreeTop->getNode()->getNumChildren()<=0)
      {
      newHead->method        = NULL;
      newHead->opaque_method = NULL;

      if (newHead->callTreeTop->getNode())
         {
         TR::Node * callNode = newHead->callTreeTop->getNode();
         memcpy((void *)&newHead->bcInfo, (void *)&callNode->getByteCodeInfo(), sizeof(struct TR_ByteCodeInfo));
   	     newHead->opaque_method = callNode->getOwningMethod();
         }
      }
   else
      {
      TR::Node * callNode = newHead->callTreeTop->getNode()->getFirstChild();
      memcpy((void *)&newHead->bcInfo, (void *)&callNode->getByteCodeInfo(), sizeof(struct TR_ByteCodeInfo));

      if (callNode->getSymbolReference() && callNode->getOpCode().isCall())
         {
         newHead->method = callNode->getSymbol()->castToMethodSymbol()->getMethod();
         newHead->opaque_method = NULL;
         }
      else
         {
   	     newHead->opaque_method = callNode->getOwningMethod();
   	     newHead->method = NULL;
   	     }
      }

   countedCallSiteList = newHead;

   return true;
   }
