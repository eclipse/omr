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

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "ras/Debug.hpp"
#include "infra/SimpleRegex.hpp"

#define PSEUDO_RANDOM_NUMBER_PREFIX "#num"
#define PSEUDO_RANDOM_NUMBER_PREFIX_LEN 4
#define PSEUDO_RANDOM_SUFFIX '#'

#define FILTER_POOL_CHUNK_SIZE 32768

#define FILTER_SIZE (sizeof(TR::CompilationFilters) + sizeof(TR_FilterBST*) * FILTER_HASH_SIZE)
void
TR_Debug::clearFilters(TR::CompilationFilters * filters)
   {
   char *buf = (char*) findOrCreateFilters(filters);
   int32_t size = FILTER_SIZE;
   memset(buf, 0, size);

   filters->filterHash = (TR_FilterBST **)(buf + sizeof(TR::CompilationFilters));
   filters->setDefaultExclude(false);
   filters->excludedMethodFilter = NULL;
   }

void
TR_Debug::clearFilters(bool loadLimit)
   {
   if (loadLimit)
      clearFilters(_relocationFilters);
   else
      clearFilters(_compilationFilters);
   }

TR::CompilationFilters *
TR_Debug::findOrCreateFilters(TR::CompilationFilters * filters)
   {
   if (filters)
      return filters;
   else
      {
      int32_t size = sizeof(TR::CompilationFilters) + sizeof(TR_FilterBST*) * FILTER_HASH_SIZE;

      char * buf = (char *)(TR::Compiler->regionAllocator.allocate(size));

      filters = (TR::CompilationFilters *)buf;
      clearFilters(filters);
      return filters;
      }
  }

TR::CompilationFilters *
TR_Debug::findOrCreateFilters(bool loadLimit)
   {
   if (loadLimit)
      // initializing filters using the new interface
      return _relocationFilters=findOrCreateFilters(_relocationFilters);
   else
      // initializing filters using the new interface
      return _compilationFilters=findOrCreateFilters(_compilationFilters);
   }

TR_FilterBST *
TR_Debug::addFilter(char * & filterString, int32_t scanningExclude, int32_t optionSetIndex, int32_t lineNum, TR::CompilationFilters * anyFilters)
   {
   uint32_t filterType = scanningExclude ? TR_FILTER_EXCLUDE_NAME_ONLY : TR_FILTER_NAME_ONLY;

   // Allocate the filter hash table, if it hasn't been already.
   //
   TR::CompilationFilters * filters = findOrCreateFilters(anyFilters);

   TR_FilterBST * filterBST = new (TR::Compiler->regionAllocator) TR_FilterBST(filterType, optionSetIndex, lineNum);

   int32_t nameLength;
   if (*filterString == '{')
      {
      char *filterCursor = filterString;
      filterType = scanningExclude ? TR_FILTER_EXCLUDE_REGEX : TR_FILTER_REGEX;
      filterBST->setFilterType(filterType);

      // Create the regular expression from the regex string
      //
      TR::SimpleRegex *regex = TR::SimpleRegex::create(filterCursor);
      if (!regex)
         {
         TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", filterCursor);
         return 0;
         }
      nameLength = filterCursor - filterString;
      filterBST->setRegex(regex);
      filterBST->setNext(filters->hasRegexFilter()? filters->filterRegexList : NULL);
      filters->filterRegexList = filterBST;
      filters->setHasRegexFilter();
      }
   else
      {
      // Note - the following call changes the filterType field in the filterBST
      //
      nameLength = scanFilterName(filterString, filterBST);
      if (!nameLength)
         return 0;

      // Add the filter to the appropriate data structure
      //
      filterType = filterBST->getFilterType();
      if (filterType == TR_FILTER_EXCLUDE_NAME_ONLY ||
          filterType == TR_FILTER_NAME_ONLY)
         {
         if (filters->filterNameList)
            filterBST->insert(filters->filterNameList);
         else
            filters->filterNameList = filterBST;
         filters->setHasNameFilter();
         }
      else
         {
         TR_FilterBST **bucket = &(filters->filterHash[nameLength % FILTER_HASH_SIZE]);

         if (*bucket)
            filterBST->insert(*bucket);
         else
            *bucket = filterBST;

         if (filterType == TR_FILTER_NAME_AND_SIG ||
             filterType == TR_FILTER_EXCLUDE_NAME_AND_SIG)
            filters->setHasNameSigFilter();
         else
            filters->setHasClassNameSigFilter();
         }
      }

   // We start by assuming we are including everything by default.
   // If we find a +ve filter (i.e. include only this) which is not part of an
   // option subset, change the default to be exclude everything.
   //
   if (!scanningExclude && optionSetIndex == 0)
      {
      filters->setDefaultExclude(true);
      }

   filterString += nameLength;
   return filterBST;
   }

TR_FilterBST *
TR_Debug::addFilter(char * & filterString, int32_t scanningExclude, int32_t optionSetIndex, int32_t lineNum, bool loadLimit)
   {
   if (loadLimit)
      {
      // initializing filters using the new interface
      _relocationFilters=findOrCreateFilters(_relocationFilters);
      return addFilter(filterString, scanningExclude, optionSetIndex, lineNum, _relocationFilters);
      }
   else
      {
      // initializing filters using the new interface
      _compilationFilters=findOrCreateFilters(_compilationFilters);
      return addFilter(filterString, scanningExclude, optionSetIndex, lineNum, _compilationFilters);
      }
   }

TR_FilterBST *
TR_Debug::addExcludedMethodFilter(bool loadLimit)
   {
   TR_FilterBST * filterBST = new (TR::Compiler->regionAllocator) TR_FilterBST(TR_FILTER_EXCLUDE_SPECIFIC_METHOD, TR_EXCLUDED_OPTIONSET_INDEX);
   if (loadLimit)
      {
      _relocationFilters=findOrCreateFilters(_relocationFilters);
      _relocationFilters->excludedMethodFilter = filterBST;
      }
   else
      {
      _compilationFilters = findOrCreateFilters(_compilationFilters);
      _compilationFilters->excludedMethodFilter = filterBST;
      }
   return filterBST;
   }

bool
TR_Debug::addSamplingPoint(char * filterString, TR_FilterBST * & lastSamplingPoint, bool loadLimit)
   {
   char *p;
   int32_t tickCount;
   if (1 != sscanf(filterString, "(%d) ", &tickCount))
      return false;
   uint32_t type;
   for (p = filterString; *p && *p != '\t'; p++)
      {}
   p++;
   if (*p == 'C')
      {
      type = TR_FILTER_SAMPLE_COMPILED;
      p += 9;
      }
   else if (*p == 'I')
      {
      type = TR_FILTER_SAMPLE_INTERPRETED;
      p += 12;
      }
   else
      return false;

   //  Skip the method name and scan the rest of the string to see if the
   // sampling point did anything
   //
   char *methodName = p;
   //for (p++; *p && *p != '>'; p++) // WILL CATCH <init>
   //   {}
   //if (!*p)
   //   return false;
   p = strstr(p, "-->");
   if (!p)
      return false;
   p += 2;

   TR::CompilationFilters *filters = findOrCreateFilters(loadLimit);
   TR_FilterBST * filterBST = new (TR::Compiler->regionAllocator) TR_FilterBST(type, tickCount);
   if (!scanFilterName(methodName, filterBST))
      return false;

   // Make sure a full method signature was specified on the sampling log line
   // and then reset the filter type to the proper value
   //
   if (filterBST->getFilterType() != TR_FILTER_SPECIFIC_METHOD)
      return false;
   filterBST->setFilterType(type);

   // Set up the rest of the information in the filter and add it to the
   // sampling point list.
   int32_t count;
   if (type == TR_FILTER_SAMPLE_INTERPRETED)
      {
      // Pick up new invocation count
      //
      if (1 != sscanf(p, "> %d", &count))
         return false;
      filterBST->setSampleCount(count);
      }
   else
      {
      // Pick up new compilation level, Use the top bit as a "profiled" tag bit.
      //
      if (1 != sscanf(p, "> recompile at level %d", &count))
         return false;
      filterBST->setSampleLevel(count);
      if (strstr(p+21,", profiled"))
         filterBST->setSampleProfiled(1);
      else
         filterBST->setSampleProfiled(0);
      }

   // Add the filter to the sampling point list
   //
   if (lastSamplingPoint)
      lastSamplingPoint->setNext(filterBST);
   else
      filters->samplingPoints = filterBST;

   lastSamplingPoint = filterBST;
   return true;
   }

bool
TR_Debug::scanInlineFilters(FILE * inlineFile, int32_t & lineNumber, TR::CompilationFilters * filters)
   {
   char          limitReadBuffer[1024];
   bool          inlineFileError = false;
   TR_FilterBST* filter = NULL;

   while(fgets(limitReadBuffer, sizeof(limitReadBuffer), inlineFile))
      {
      ++lineNumber;
      //ignore range for now
      //if (lineNumber < firstLine || lineNumber > lastLine)
      //   continue;

      char includeFlag = limitReadBuffer[0];

      if (includeFlag == '[')
         {
         //TR_VerboseLog::write("<JIT: sub inline file entry start --> '%s'>\n", limitReadBuffer);

         if (filter)
            {
            filter->subGroup = findOrCreateFilters(filter->subGroup);
            filter->subGroup->setDefaultExclude(true);
            inlineFileError = !scanInlineFilters(inlineFile, lineNumber, filter->subGroup);
            }

         }
      else if (includeFlag == ']')
         {
         //TR_VerboseLog::write("<JIT: sub inline file entry end --> '%s'>\n", limitReadBuffer);
         // always return true (success)
         // this will ignore the rest of the filters if no matching open bracket.
         return true;
         }
      else if (includeFlag == '+' || includeFlag == '-')
         {
         char *p = limitReadBuffer+1;
         int32_t optionSet;
         if (*p >= '0' && *p <= '9')
            optionSet = *(p++) - '0';
         else
            optionSet = 0;
         if (*(p++) != ' ')
            {
            inlineFileError = true;
            break;
            }
         // Skip hotness level if it is present
         //
         if (*p == '(')
            {
            for (p++; *p && *p != ')'; p++)
               {}
            if (*(p++) != ')')
               {
               inlineFileError = true;
               break;
               }
            if (*(p++) != ' ')
               {
               inlineFileError = true;
               break;
               }
            }

         filter = addFilter(p, (('+' == includeFlag) ? 0 : 1), optionSet, lineNumber, filters);

         if (!filter)
            {
            inlineFileError = true;
            TR_VerboseLog::write("<JIT: bad inline file entry --> '%s'>\n", limitReadBuffer);
            break;
            }
         }
      }

   return !inlineFileError;
   }

/** \brief
 *     This function opens the inlinefile and adds its entries to a TR::CompilationFilters.
 *
 *     An inlinefile is a file containing a list of methods, one per line, which the inliner will limit itself to
 *     when trying to perform inlining. In other words, only methods from the file can be inlined, but there is no
 *     guarantee that any of them will be inlined. The format for entry is:
 *
 *     + signature
 *
 *  \param option
 *     Points to the first char after inlinefile=
 *
 *  \param base
 *     Unused variable needed for downstream projects.
 *
 *  \param entry
 *     The option table entry for this option.
 *
 *  \param cmdLineOptions
 *     Unused variable needed for downstream projects.
 *
 *  \return
 *     The unmodified parameter option if there is a problem with the file, aborting JIT initialization.
 *     Otherwise a pointer to the next comma or NULL.
 */
char *
TR_Debug::inlinefileOption(char *option, void *base, TR::OptionTable *entry, TR::Options * cmdLineOptions)
   {
   char *endOpt = option;
   char *name = option;
   char *fail = option;

   // move to the end of this option
   for (; *endOpt && *endOpt != ','; endOpt++)
      {}

   int32_t len = endOpt - name;
   if (!len)
      return option;

   char *inlineFileName = (char *)(TR::Compiler->regionAllocator.allocate(len+1));
   memcpy(inlineFileName, name, len);
   inlineFileName[len] = 0; // NULL terminate the copied string
   entry->msgInfo = (intptr_t)inlineFileName;

   FILE *inlineFile = fopen(inlineFileName, "r");
   bool success = false;

   if (inlineFile)
      {
      // initializing _inlineFilters using the new interface
      _inlineFilters = findOrCreateFilters(_inlineFilters);
      TR::CompilationFilters * filters = _inlineFilters;

      filters->setDefaultExclude(true);

      int32_t lineNumber = 0;

      success = scanInlineFilters(inlineFile, lineNumber, filters);

      fclose(inlineFile);
      }

   if (!success)
      {
      TR_VerboseLog::write("<JIT: fatal: unable to read inline file --> '%s'>\n", inlineFileName);
      return fail; // We want to fail if we can't read the file because it is too easy to miss that the file wasn't picked up
      }
   return endOpt;
   }

/** \brief
 *     Processes a limitfile= option, parses and applies the limitfile to compilation control.
 *
 *     A limitfile is a compiler verbose log, produced by the option -Xjit:verbose,vlog=filename
 *     When a verbose log is used as a limitfile, only the methods contained within the file
 *     will be compiled if they are queued for compilation. The format of the method entries in
 *     the file must match that of a verbose log.
 *
 *     The option can be used in 2 ways:
 *     limitfile=filename
 *     limitfile=(filename,xx,yy)
 *
 *     The when the latter is used, xx-yy denotes a line number range (starting at zero and ignoring # comments)
 *     to restrict the limiting to. This is commonly used in debugging to narrow down a problem to a specific
 *     method by doing a binary search on the limitfile.
 *
 *  \param option
 *     Points to the first char after inlinefile=
 *
 *  \param base
 *     Unused variable needed for downstream projects.
 *
 *  \param entry
 *     The option table entry for this option.
 *
 *  \param cmdLineOptions
 *     The command line options.
 *
 *  \param loadLimit
 *     The load limit.
 *
 *  \param pseudoRandomListHeadPtr
 *     A list of pseudo random numbers.
 *
 *  \return
 *     The unmodified parameter option if there is a problem with the file, aborting JIT initialization.
 *     Otherwise a pointer to the next comma or NULL.
 */
char *
TR_Debug::limitfileOption(char *option, void *base, TR::OptionTable *entry, TR::Options * cmdLineOptions, bool loadLimit, TR_PseudoRandomNumbersListElement **pseudoRandomListHeadPtr)
   {
   char *endOpt = option;
   char *name = option;
   char *fail = option;

   bool range = false;
   if (*endOpt == '(')
      {
      ++endOpt;
      ++name;
      range = true;
      }

   for (; *endOpt && *endOpt != ','; endOpt++)
      {}
   int32_t len = endOpt - name;
   if (!len)
      return option;

   char *limitFileName = (char *)(TR::Compiler->regionAllocator.allocate(len+1));
   memcpy(limitFileName, name, len);
   limitFileName[len] = 0;
   entry->msgInfo = (intptr_t)limitFileName;

   intptr_t firstLine = 1, lastLine = INT_MAX;
   if (range)
      {
      if (!*endOpt)
         return option;
      firstLine = TR::Options::getNumericValue(++endOpt);
      if (*endOpt == ',')
         lastLine = TR::Options::getNumericValue(++endOpt);
      if (*endOpt != ')')
         return option;
      ++endOpt;
      }

   FILE *limitFile = fopen(limitFileName, "r");
   if (limitFile)
      {
      TR::CompilationFilters * filters = findOrCreateFilters(loadLimit);
      if (!cmdLineOptions->getOption(TR_OrderCompiles))
         filters->setDefaultExclude(true);

      char          limitReadBuffer[1024];
      bool          limitFileError = false;
      int32_t       lineNumber = 0;

      TR_PseudoRandomNumbersListElement *pseudoRandomListHead = NULL;
      if (pseudoRandomListHeadPtr)
         pseudoRandomListHead = *pseudoRandomListHeadPtr;
      TR_PseudoRandomNumbersListElement *curPseudoRandomListElem = pseudoRandomListHead;
      int32_t curIndex = 0;

      while(fgets(limitReadBuffer, sizeof(limitReadBuffer), limitFile))
         {
         ++lineNumber;
         if (lineNumber < firstLine || lineNumber > lastLine)
            continue;

         char includeFlag = limitReadBuffer[0];
         if (strncmp(limitReadBuffer,"-precompileMethod",17) == 0)
            {
            char *p = limitReadBuffer+18;
            if (!addFilter(p, 0, 0, lineNumber, loadLimit))
               {
               limitFileError = true;
               break;
               }
            }
         else if (strncmp(limitReadBuffer,"-noprecompileMethod",19) == 0)
            {
            char *p = limitReadBuffer+20;
            if (!addFilter(p, 1, 0, lineNumber, loadLimit))
               {
               limitFileError = true;
               break;
               }
            }
         else if (includeFlag == '+' || includeFlag == '-')
            {
            char *p = limitReadBuffer+1;
            int32_t optionSet;
            if (*p >= '0' && *p <= '9')
               optionSet = *(p++) - '0';
            else
               optionSet = 0;
            if (*(p++) != ' ')
               {
               limitFileError = true;
               break;
               }
            // Skip hotness level if it is present
            //
            if (*p == '(')
               {
               for (p++; *p && *p != ')'; p++)
                  {}
               if (*(p++) != ')')
                  {
                  limitFileError = true;
                  break;
                  }
               if (*(p++) != ' ')
                  {
                  limitFileError = true;
                  break;
                  }
               }

            //if (optionSet > 0)
            //   filters->setDefaultExclude(false);

            if (!addFilter(p, (('+' == includeFlag) ? 0 : 1), optionSet, lineNumber, loadLimit))
               {
               limitFileError = true;
               break;
               }
            }
         else if (strncmp(limitReadBuffer,PSEUDO_RANDOM_NUMBER_PREFIX, PSEUDO_RANDOM_NUMBER_PREFIX_LEN) == 0)
            {
            char *p = limitReadBuffer + PSEUDO_RANDOM_NUMBER_PREFIX_LEN;

            if (*(p++) != ' ')
               {
               limitFileError = true;
               break;
               }

            bool isNegative = false;
            if (*(p) == '-')
              {
	      p++;
	      isNegative = true;
	      }

            int32_t randNum;
            while (isdigit(p[0]))
               {
               randNum = atoi(p);
               while(isdigit(p[0]))
                  ++p;

               if (isNegative)
 		  randNum = -1*randNum;

               if ((curPseudoRandomListElem == NULL) ||
                   (curIndex == PSEUDO_RANDOM_NUMBERS_SIZE))
		  {
		  int32_t sz = sizeof(TR_PseudoRandomNumbersListElement);
		  TR_PseudoRandomNumbersListElement *newPseudoRandomListElem = (TR_PseudoRandomNumbersListElement *)(TR::Compiler->regionAllocator.allocate(sz));
                  newPseudoRandomListElem->_next = NULL;
                  curIndex = 0;
                  if (curPseudoRandomListElem == NULL)
		     *pseudoRandomListHeadPtr = newPseudoRandomListElem;
                  else
                     curPseudoRandomListElem->_next =  newPseudoRandomListElem;
                  curPseudoRandomListElem =  newPseudoRandomListElem;
		  }

               if (curPseudoRandomListElem == NULL)
                  {
                  limitFileError = true;
                  break;
                  }

               curPseudoRandomListElem->_pseudoRandomNumbers[curIndex++] = randNum;
               curPseudoRandomListElem->_curIndex = curIndex;

               if (*p == PSEUDO_RANDOM_SUFFIX)
		  break;

               if (*(p++) != ' ')
                  {
                  limitFileError = true;
                  break;
                  }

               isNegative = false;
               if (*(p) == '-')
		  {
		  p++;
	          isNegative = true;
		  }
	       }
            }
         else if (limitReadBuffer[0] == '(' && cmdLineOptions->getOption(TR_OrderCompiles))
            {
            // Recognize new sampling point
            //
            static TR_FilterBST *lastSamplingPoint = NULL;
            addSamplingPoint(limitReadBuffer, lastSamplingPoint, loadLimit);
            }
         }
      if (limitFileError)
         {
         TR_VerboseLog::write("<JIT: fatal: bad limit file entry --> '%s'>\n", limitReadBuffer);
         return fail;
         }
      fclose(limitFile);
      }
   else
      {
      TR_VerboseLog::write("<JIT: fatal: unable to read limit file --> '%s'>\n", limitFileName);
      return fail; //We want to fail if we can't read the file because it is too easy to miss that the file wasn't picked up
      }
   return endOpt;
   }

char *
TR_Debug::limitOption(char *option, void *base, TR::OptionTable *entry, TR::Options * cmdLineOptions, bool loadLimit)
   {
   char *p = option;

   // this use the old interface
   TR_FilterBST *filter = addFilter(p, entry->parm1, 0, 0, loadLimit);

   if (!filter)
      return option;

   int32_t len = p - option;
   char *limitName = (char *)(TR::Compiler->regionAllocator.allocate(len+1));
   memcpy(limitName, option, len);
   limitName[len] = 0;
   entry->msgInfo = (intptr_t)limitName;

   // Look for option subset if this is "limit" rather than "exclude"
   //
   TR::SimpleRegex *methodRegex = filter->getRegex();
   if (methodRegex && !entry->parm1 && (*p == '(' || *p == '{'))
      {
      TR::SimpleRegex *optLevelRegex = NULL;

      // Scan off the opt level regex if it is present
      //
      if (*p == '{')
         {
         optLevelRegex = TR::SimpleRegex::create(p);
         if (!optLevelRegex || *p != '(')
            {
            if (!optLevelRegex)
               TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", p);
            return option;
            }
         }
      // If an option subset was found, save the information for later
      // processing
      //
      char *startOptString = ++p;
      int32_t parenNest = 1;
      for (; *p; p++)
         {
         if (*p == '(')
            parenNest++;
         else if (*p == ')')
            {
            if (--parenNest == 0)
               {
               p++;
               break;
               }
            }
         }
      if (parenNest)
         return startOptString;

      // Save the option set - its option string will be processed after
      // the main options have been finished.
      //
      TR::OptionSet *newSet = (TR::OptionSet *)(TR::Compiler->regionAllocator.allocate(sizeof(TR::OptionSet)));
      newSet->init(startOptString);
      newSet->setMethodRegex(methodRegex);
      newSet->setOptLevelRegex(optLevelRegex);
      cmdLineOptions->addOptionSet(newSet);
      }

   return p;
   }


void * TR_FilterBST::operator new(size_t size, TR::PersistentAllocator &allocator)
   {
   return allocator.allocate(size);
   }


TR_FilterBST *TR_FilterBST::find(const char *methodName, int32_t methodNameLen)
   {
   // Find the filter for the given method name in the tree rooted at node.
   //
   int32_t       rc;
   TR_FilterBST *node;

   for (node = this; node; node = node->getChild(rc))
      {
      rc = strncmp(methodName, node->getName(), methodNameLen);
      if (rc == 0)
         {
         rc = methodNameLen - node->getNameLen();
         if (rc == 0)
            break;
         }
      rc = (rc < 0) ? 0 : 1;
      }
   return node;
   }

TR_FilterBST *TR_FilterBST::find(const char *methodName, int32_t methodNameLen, const char *methodClass, int32_t methodClassLen, const char *methodSignature, int32_t methodSignatureLen)
   {
   // Find the filter for the given method name, class and signature in the
   // tree rooted at node.
   //
   int32_t       rc;
   TR_FilterBST *node;

   for (node = this; node; node = node->getChild(rc))
      {
      rc = strncmp(methodName, node->getName(), methodNameLen);
      if (rc == 0)
         rc = methodNameLen - node->getNameLen();
      if (rc == 0)
         {
         rc = strncmp(methodClass, node->getClass(), methodClassLen);
         if (rc == 0)
            rc = methodClassLen - strlen(node->getClass());
         if (rc == 0)
            {
            rc = strncmp(methodSignature, node->getSignature(), methodSignatureLen);
            if (rc == 0)
               rc = methodSignatureLen - strlen(node->getSignature());
            if (rc == 0)
               break;
            }
         }
      rc = (rc < 0) ? 0 : 1;
      }
   return node;
   }

TR_FilterBST *TR_FilterBST::findRegex(const char *methodSpec)
   {
   TR_FilterBST *f;
   for (f=this; f && !f->_regex->match(methodSpec); f = f->getNext());
   return f;
   }

void TR_FilterBST::insert(TR_FilterBST *node)
   {
   // Insert this filter into the tree rooted at node. If the filter already
   // exists, don't insert the new one.
   //
   int32_t rc;

   while (node)
      {
      rc = strcmp(getName(), node->getName());
      if (rc == 0)
         {
         rc = strcmp(getClass(), node->getClass());
         if (rc == 0)
            {
            rc = strcmp(getSignature(), node->getSignature());
            if (rc == 0)
               break;
            }
         }

      TR_FilterBST *child;
      rc = (rc < 0) ? 0 : 1;
      child = node->getChild(rc);
      if (child)
         node = child;
      else
	 {
         node->setChild(rc, this);
         break;
	 }
      }
   }

void
TR_Debug::print(TR_FilterBST * filter)
   {
   /*
   if (filter->_optionSet)
      TR_VerboseLog::write("   {%d}", filter->_optionSet);

   if (filter->_lineNumber)
      TR_VerboseLog::write("   [%d]", filter->_lineNumber);
   */

   switch (filter->_filterType)
      {
      case TR_FILTER_EXCLUDE_NAME_ONLY:
         TR_VerboseLog::write("   -%s", "NAME_ONLY");
         break;
      case TR_FILTER_EXCLUDE_NAME_AND_SIG:
         TR_VerboseLog::write("   -%s", "NAME_AND_SIG");
         break;
      case TR_FILTER_EXCLUDE_SPECIFIC_METHOD:
         TR_VerboseLog::write("   -%s", "SPECIFIC_METHOD");
         break;
      case TR_FILTER_EXCLUDE_REGEX:
         TR_VerboseLog::write("   -%s", "REGEX");
         break;
      case TR_FILTER_NAME_ONLY:
         TR_VerboseLog::write("   +%s", "NAME_ONLY");
         break;
      case TR_FILTER_NAME_AND_SIG:
         TR_VerboseLog::write("   +%s", "NAME_AND_SIG");
         break;
      case TR_FILTER_SPECIFIC_METHOD:
         TR_VerboseLog::write("   +%s", "SPECIFIC_METHOD");
         break;
      case TR_FILTER_REGEX:
         TR_VerboseLog::write("   +%s", "REGEX");
         break;
      }

   switch (filter->_filterType)
      {
      case TR_FILTER_EXCLUDE_NAME_ONLY:
         TR_VerboseLog::write("   {^*.%s(*}\n", filter->getName());
         break;
      case TR_FILTER_EXCLUDE_NAME_AND_SIG:
         TR_VerboseLog::write("   {^*.%s%s}\n", filter->getName(), filter->getSignature());
         break;
      case TR_FILTER_EXCLUDE_SPECIFIC_METHOD:
         TR_VerboseLog::write("   {^%s.%s%s}\n", filter->getClass(), filter->getName(), filter->getSignature());
         break;
      case TR_FILTER_EXCLUDE_REGEX:
         TR_VerboseLog::write("  ");
         filter->getRegex()->print(true);
         TR_VerboseLog::write("\n");
         break;
      case TR_FILTER_NAME_ONLY:
         TR_VerboseLog::write("   {*.%s(*}\n", filter->getName());
         break;
      case TR_FILTER_NAME_AND_SIG:
         TR_VerboseLog::write("   {*.%s%s}\n", filter->getName(), filter->getSignature());
         break;
      case TR_FILTER_SPECIFIC_METHOD:
         TR_VerboseLog::write("   {%s.%s%s}\n", filter->getClass(), filter->getName(), filter->getSignature());
         break;
      case TR_FILTER_REGEX:
         TR_VerboseLog::write("  ");
         filter->getRegex()->print(false);
         TR_VerboseLog::write("\n");
         break;
      }

      if (filter->subGroup)
         {
         TR_VerboseLog::write("   [\n");
         printFilters(filter->subGroup);
         TR_VerboseLog::write("   ]\n");
         }
   }

void
TR_Debug::printFilters(TR::CompilationFilters * filters)
   {
   int32_t i;
   if (filters)
      {
      if (filters->filterHash)
         {
         for (i = 0; i < FILTER_HASH_SIZE; i++)
            if (filters->filterHash[i])
               printFilterTree(filters->filterHash[i]);
         }

      if (filters->filterNameList)
         {
         printFilterTree(filters->filterNameList);
         }

      if (filters->filterRegexList)
         {
         for (TR_FilterBST * filter = filters->filterRegexList; filter; filter = filter->getNext())
            print(filter);
         }

     }
   }

void
TR_Debug::printFilters()
   {
   TR_VerboseLog::write("<compilationFilters>\n");
   printFilters(_compilationFilters);
   TR_VerboseLog::write("</compilationFilters>\n");

   TR_VerboseLog::write("<relocationFilters>\n");
   printFilters(_relocationFilters);
   TR_VerboseLog::write("</relocationFilters>\n");

   TR_VerboseLog::write("<inlineFilters>\n");
   printFilters(_inlineFilters);
   TR_VerboseLog::write("</inlineFilters>\n");
   }

void
TR_Debug::printFilterTree(TR_FilterBST *root)
   {
   if (root->getChild(0))
      printFilterTree(root->getChild(0));
   print(root);
   if (root->getChild(1))
      printFilterTree(root->getChild(1));
   }

void
TR_Debug::printSamplingPoints()
   {
   for (TR_FilterBST *filter=_compilationFilters->samplingPoints; filter; filter = filter->getNext())
   {
      if (filter->getFilterType() == TR_FILTER_SAMPLE_INTERPRETED)
      {
         TR_VerboseLog::write("(%d)\tInterpreted %s.%s%s\tcount=%d\n",
            filter->getTickCount(),
            filter->getClass(), filter->getName(), filter->getSignature(),
            filter->getSampleCount()
            );
      }
      else
      {
         TR_VerboseLog::write("(%d)\tCompiled %s.%s%s\tlevel=%d%s\n",
            filter->getTickCount(),
            filter->getClass(), filter->getName(), filter->getSignature(),
            filter->getSampleLevel(),
            (filter->getSampleProfiled() ? ", profiled": "")
            );
      }
   }
   }

int32_t
TR_Debug::scanFilterName(char *string, TR_FilterBST *filter)
   {
   // help for OMR parsing
   bool seenFileName = false;
   bool seenLineNumber = false;
   bool omrPattern = false;

   // Walk the filter to determine the type.
   //
   //TR_VerboseLog::write("filterName: %s\n", string);
   char *nameChars = NULL;
   int32_t nameLen = 0;
   char *classChars = NULL;
   int32_t classLen = 0;
   char *signatureChars = string;
   int32_t signatureLen = 0;
   char  filterType = filter->getFilterType();
   if (*string == '/' || *string == '.') // hack that works for linux
      omrPattern = true;

   while (*string && *string != '\t' && *string != ',' && *string != '\n')
      {
      if (omrPattern)
         {
         if (*string == ':')
            {
            if (!seenFileName)
               {
               classChars = signatureChars;
               classLen = signatureLen;
               signatureChars = string+1;
               signatureLen = 0;
               seenFileName = true;
               }
            else if (!seenLineNumber)
               {
               nameChars = signatureChars;
               nameLen = signatureLen;
               signatureChars = string+1;
               signatureLen = 0;
               seenLineNumber = true;
               }
            }
         else if (seenLineNumber && *string == ' ')
            {
            break;
            }

         else
            signatureLen++;
         }
      else
         {
         if (*string == ' ')
            break;

         if (*string == '.')
	    {
            classChars = signatureChars;
            classLen = signatureLen;
            signatureChars = string+1;
            signatureLen = 0;
	    filterType = filter->isExclude() ? TR_FILTER_EXCLUDE_SPECIFIC_METHOD : TR_FILTER_SPECIFIC_METHOD;
	    }

         else if (*string == '(')
	    {
            nameChars = signatureChars;
            nameLen = signatureLen;
            signatureChars = string;
            signatureLen = 1;
            if (filterType == TR_FILTER_EXCLUDE_NAME_ONLY ||
                filterType == TR_FILTER_NAME_ONLY)
               {
               filterType = filter->isExclude() ? TR_FILTER_EXCLUDE_NAME_AND_SIG : TR_FILTER_NAME_AND_SIG;
               }
	    }
         else
            signatureLen++;
         }

      string++;
      }

   if (!nameChars)
      {
      nameChars = signatureChars;
      nameLen = signatureLen;
      signatureChars = NULL;
      signatureLen = 0;
      }

   // signal for OMR style signature
   if (omrPattern)
      {
      // need to swap name and signature, since name is currently the line number
      char *tempChars = nameChars;
      int32_t tempLen = nameLen;
      nameChars = signatureChars;
      nameLen = signatureLen;
      signatureChars = tempChars;
      signatureLen = tempLen;
      filterType = filter->isExclude() ? TR_FILTER_EXCLUDE_SPECIFIC_METHOD : TR_FILTER_SPECIFIC_METHOD;
      }

   // Keep copies of the method name, class, and signature, and point
   // the filter members to them
   //
   int32_t length = nameLen + classLen + signatureLen;
   char *buf = (char*)(TR::Compiler->regionAllocator.allocate(length + 3)); // NULL terminated strings

   filter->setName(buf, nameLen);
   if (nameChars)
      {
      strncpy(buf, nameChars, nameLen);
      buf += nameLen;
      }
   *(buf++) = 0;
   filter->setClass(buf);
   if (classChars)
      {
      strncpy(buf, classChars, classLen);
      buf += classLen;
      }
   *(buf++) = 0;
   filter->setSignature(buf);
   if (signatureChars)
      {
      strncpy(buf, signatureChars, signatureLen);
      buf += signatureLen;
      }
   *(buf++) = 0;

   filter->setFilterType(filterType);
   return length;
   }


bool
TR_Debug::methodSigCanBeCompiled(const char *methodSig, TR_FilterBST * & filter, TR::Method::Type methodType)
   {
   return methodSigCanBeCompiledOrRelocated(methodSig, filter, false, methodType);
   }

bool
TR_Debug::methodSigCanBeRelocated(const char *methodSig, TR_FilterBST * & filter)
   {
   return methodSigCanBeCompiledOrRelocated(methodSig, filter, true, TR::Method::J9);
   }

bool
TR_Debug::methodSigCanBeFound(const char *methodSig, TR::CompilationFilters * filters, TR_FilterBST * & filter, TR::Method::Type methodType)
   {
   const char *methodClass, *methodName, *methodSignature;
   uint32_t methodClassLen, methodNameLen, methodSignatureLen;

   methodClass = methodSig;
   if (methodType != TR::Method::J9)
      {
      if (methodSig[0] == '/' || methodSig[0] == '.') // omr method pattern
         {
         methodClass = methodSig;
         methodSignature = strchr(methodSig, ':');
         methodClassLen = methodSignature - methodClass;
         methodSignature++;
         methodName = strchr(methodSignature, ':');
         methodSignatureLen = methodName - methodSignature;
         methodName++;
         methodNameLen = strlen(methodName);
         }
      else
         {
         methodName = methodSig;
         methodClassLen = 0;
         methodSignature = "";
         methodSignatureLen = 0;
         methodNameLen = strlen(methodName);
         }
      }
   else
      {
      if (methodSig[0] == '/') // omr method pattern
         {
         methodClass = methodSig;
         methodSignature = strchr(methodSig, ':');
         methodClassLen = methodSignature - methodClass;
         methodSignature++;
         methodName = strchr(methodSignature, ':');
         methodSignatureLen = methodName - methodSignature;
         methodName++;
         methodNameLen = strlen(methodName);
         }
      else
         {
         methodName  = strchr(methodSig, '.');
         methodClassLen = methodName - methodClass;
         methodName++;
         methodSignature = strchr(methodName, '(');
         methodSignatureLen = strlen(methodSignature);
         TR_ASSERT(methodSignature, "unable to pattern match java method signature");
         methodNameLen = methodSignature - methodName;
         }
      }

   int32_t length = methodNameLen + methodSignatureLen;

   if (filters->hasClassNameSigFilter() || filters->hasNameSigFilter())
      {
      if (filters->hasClassNameSigFilter())
	 {
	 // Search for the class+name+signature.
	 //
	 filter = filters->filterHash[(length+methodClassLen) % FILTER_HASH_SIZE];
         if (filter)
            filter = filter->find(methodName, methodNameLen, methodClass, methodClassLen, methodSignature, methodSignatureLen);
	 }

      if (!filter && filters->hasNameSigFilter())
	 {
	 // Search for the name+signature.
	 //
	 filter = filters->filterHash[length % FILTER_HASH_SIZE];
         if (filter)
            filter = filter->find(methodName, methodNameLen, "", 0, methodSignature, methodSignatureLen);
	 }
      }

   if (!filter && filters->hasNameFilter())
      {
      // Search the name filter list.
      //
      filter = filters->filterNameList;
      if (filter)
         filter = filter->find(methodName, methodNameLen);
      }

   if (!filter && filters->hasRegexFilter())
      {
      // Search the regex filter list.
      //
      filter = filters->filterRegexList;
      if (filter)
         filter = filter->findRegex(methodSig);
      }

   bool excluded = filters->defaultExclude();
   if (filter)
      {
      switch (filter->getFilterType())
         {
         case TR_FILTER_EXCLUDE_NAME_ONLY:
         case TR_FILTER_EXCLUDE_NAME_AND_SIG:
         case TR_FILTER_EXCLUDE_SPECIFIC_METHOD:
         case TR_FILTER_EXCLUDE_REGEX:
            excluded = true;
            break;
         default:
            excluded = false;
            break;
         }
      }

   return !excluded;
   }

bool
TR_Debug::methodCanBeFound(TR_Memory *trMemory, TR_ResolvedMethod *method, TR::CompilationFilters * filters, TR_FilterBST * & filter)
   {
   const char * methodSig = method->signature(trMemory);
   return methodSigCanBeFound(methodSig, filters, filter, method->convertToMethod()->methodType());
   }

bool
TR_Debug::methodSigCanBeCompiledOrRelocated(const char *methodSig, TR_FilterBST * & filter, bool loadLimit, TR::Method::Type methodType)
   {
   TR::CompilationFilters *compOrReloFilter = NULL;

   if (loadLimit)
      {
      if (!_relocationFilters)
         return true;
      else
         compOrReloFilter = _relocationFilters;
      }
   else
      {
      if (!_compilationFilters)
         return true;
      else
         compOrReloFilter = _compilationFilters;
      }

   bool found = methodSigCanBeFound(methodSig, compOrReloFilter, filter, methodType);
   if (!found)
      {
      if (compOrReloFilter->excludedMethodFilter)
         {
         // Excluded methods SHOULD be compiled in this case.
         //
         found = true;
         filter = compOrReloFilter->excludedMethodFilter;
         }
      }

   return found;
   }

bool
TR_Debug::methodCanBeCompiled(TR_Memory *trMemory, TR_ResolvedMethod *method, TR_FilterBST * & filter)
   {
   const char * methodSig = method->signature(trMemory);
   return methodSigCanBeCompiled(methodSig, filter, method->convertToMethod()->methodType());
   }

bool
TR_Debug::methodCanBeRelocated(TR_Memory *trMemory, TR_ResolvedMethod *method, TR_FilterBST * & filter)
   {
   const char * methodSig = method->signature(trMemory);
   return methodSigCanBeRelocated(methodSig, filter);
   }

int32_t *
TR_Debug::loadCustomStrategy(char *fileName)
   {
   int32_t *customStrategy = NULL;
   FILE *optFile = fopen(fileName, "r");
   if (optFile)
      {
      char lineBuffer[1000];
      int32_t optNumBuffer[1000];
      int32_t optCount = 0;

      while(fgets(lineBuffer, sizeof(lineBuffer), optFile))
         {
         if (optCount >= (sizeof(optNumBuffer)/sizeof(optNumBuffer[0])))
            {
            TR_VerboseLog::write("<JIT: WARNING: reached limit of %d optFile lines; ignoring subsequent lines\n", optCount);
            break;
            }

         int optIndex;
         if (!sscanf(lineBuffer, "Performing %d: ", &optIndex))
            continue;

         char *name = strchr(lineBuffer, ':') + 2; // 2 moves past the colon and the space
         int32_t nameLen = strcspn(name, " \n");

         int32_t optNum;
         for (optNum = 0; optNum < OMR::numOpts; optNum++)
            {
            const char *actualName = OMR::Optimizer::getOptimizationName((OMR::Optimizations)optNum);
            if (!strncmp(name, actualName, nameLen))
               {
               int32_t flags = 0;
               if (strstr(name+nameLen, "mustBeDone"))
                  flags |= TR::Options::MustBeDone;
               optNumBuffer[optCount++] = optNum | flags;
               break;
               }
            }
         if (optNum == OMR::numOpts)
            TR_VerboseLog::write("<JIT: WARNING: ignoring optFile line; no matching opt name for '%s'\n", name);

         }

      if (optCount > 0)
         {
         int32_t srcSize = optCount * sizeof(optNumBuffer[0]);
         customStrategy = (int32_t*)(TR::Compiler->regionAllocator.allocate(srcSize + sizeof(optNumBuffer[0]))); // One extra for the endOpts entry
         memcpy(customStrategy, optNumBuffer, srcSize);
         customStrategy[optCount] = OMR::endOpts;
         }
      else
         {
         TR_VerboseLog::write("<JIT: WARNING: ignoring optFile; contains no suitable opt names\n");
         }
      }
   else
      {
      TR_VerboseLog::write("<JIT: WARNING: optFile not found: %s\n", fileName);
      }
   return customStrategy;
   }
