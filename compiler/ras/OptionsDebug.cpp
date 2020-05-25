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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"
#include "env/FilePointer.hpp"
#include "env/ObjectModel.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "infra/Assert.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Optimizations.hpp"
#include "ras/Debug.hpp"
#include "ras/IgnoreLocale.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

// Help text constant information
//
#define OPTION_NAME_INDENT          3
#define DESCRIPTION_START_COLUMN   40
#define DESCRIPTION_TEXT_INDENT     3
#define DEFAULT_OPTION_LINE_WIDTH  80

static char  optionCategories[] = {' ','C','O','L','D','R','I','M',0}; // Must match categories[] in codegen.dev/Options.cpp
static char *optionCategoryNames[] = {"\nGeneral options:\n",
                                      "\nCode generation options:\n",
                                      "\nOptimization options:\n",
                                      "\nLogging and display options:\n",
                                      "\nDebugging options:\n",
                                      "\nRecompilation and profiling options:\n",
                                      "\nInternal options:\n",
                                      "\nOther options:\n",
                                      0 // Fail quickly if we run past the end of this array
                                      };



TR::FILE *TR_Debug::findLogFile(TR::Options *cmdLineOptions, TR::OptionSet *optSet, char *logFileName)
   {
   char *fileName = cmdLineOptions->getLogFileName();
   TR::FILE *logFile = NULL;
   if (fileName && !STRICMP(logFileName, fileName))
      logFile = cmdLineOptions->getLogFile();
   else
      {
      for (TR::OptionSet *prev = cmdLineOptions->getFirstOptionSet(); prev && prev != optSet; prev = prev->getNext())
         {
         fileName = prev->getOptions()->getLogFileName();
         if (fileName && !STRICMP(logFileName, fileName))
            {
            logFile = prev->getOptions()->getLogFile();
            break;
            }
         }
      }
   return logFile;
   }

TR::FILE *TR_Debug::findLogFile(TR::Options *aotCmdLineOptions, TR::Options *jitCmdLineOptions, TR::OptionSet *optSet, char *logFileName)
   {
   if (aotCmdLineOptions)
      {
      TR::FILE *logFile = findLogFile(aotCmdLineOptions, optSet, logFileName);
      if (logFile != NULL)
         return logFile;
      }

   if (jitCmdLineOptions)
      {
      TR::FILE *logFile = findLogFile(jitCmdLineOptions, optSet, logFileName);
      if (logFile != NULL)
         return logFile;
      }

   return NULL;
   }


// Search all options sets, jitCmdLineOptions and aotCmdLineOptions to see if they
// have speciafied a log file with the given name. All options that match are
// returned in optionsArray. if the size of the array is insufficient to hold them
// all, the required size is returned so that the caller can provide a bigger array
int32_t TR_Debug::findLogFile(const char *logFileName, TR::Options *aotCmdLineOptions, TR::Options *jitCmdLineOptions, TR::Options **optionsArray, int32_t arraySize)
   {
   int32_t index = 0;
   findLogFile(logFileName, aotCmdLineOptions, optionsArray, arraySize, index);
   findLogFile(logFileName, jitCmdLineOptions, optionsArray, arraySize, index);
   return index;
   }

void TR_Debug::findLogFile(const char *logFileName, TR::Options *cmdOptions, TR::Options **optionsArray,
                           int32_t arraySize, int32_t &index)
   {
   if (!cmdOptions)
      return;
   if (cmdOptions->getLogFileName() && !STRICMP(logFileName, cmdOptions->getLogFileName()))
      {
      if (index < arraySize)
         optionsArray[index] = cmdOptions;
      index++;
      }
   for (TR::OptionSet *optSet = cmdOptions->getFirstOptionSet(); optSet; optSet = optSet->getNext())
      {
      if (optSet->getOptions()->getLogFileName() && !STRICMP(logFileName, optSet->getOptions()->getLogFileName()))
         {
         if (index < arraySize)
            optionsArray[index] = optSet->getOptions();
         index++;
         }
      }
   }

static bool regexExcludes(TR::SimpleRegex *regex, const char *string)
   {
   if (regex)
      return !regex->match(string, false); // case-insensitive
   else
      return false;
   }

void TR_Debug::dumpOptionHelp(TR::OptionTable * firstOjit, TR::OptionTable * firstOfe, TR::SimpleRegex *nameFilter)
   {
   char *p;
   int32_t i, lastPos;
   static int optionLineWidth=0;

   if (!optionLineWidth)
      {
      static char *columns = ::feGetEnv("COLUMNS");
      if (columns)
         optionLineWidth = atoi(columns);
      else
         optionLineWidth = DEFAULT_OPTION_LINE_WIDTH;
      }

   TR::OptionTable * entry;

   TR_VerboseLog::writeLine(TR_Vlog_INFO,"Usage: -Xjit:option([,option]*)\n");

   for (int32_t cat = 0; optionCategories[cat]; cat++)
      {
      // Don't print internal options
      // TODO: Perhaps add something like -Xjit:helpInternal to print these
      //
      if (optionCategories[cat] == 'I')
         continue;

      TR::OptionTable * ojit = firstOjit;
      TR::OptionTable * ofe = firstOfe;
      bool entryFound = false;
      while (ojit->name || ofe->name)
         {
         if (ojit->name
            && (  ojit->helpText == NULL
               || ojit->helpText[0] != optionCategories[cat]
               || (regexExcludes(nameFilter, ojit->name) && regexExcludes(nameFilter, ojit->helpText))))
            {
            ojit++;
            continue;
            }
         if (ofe->name
            && (  ofe->helpText == NULL
               || ofe->helpText[0] != optionCategories[cat]
               || (regexExcludes(nameFilter, ofe->name) && regexExcludes(nameFilter, ofe->helpText))))
            {
            ofe++;
            continue;
            }
         if (!ojit->name)
            entry = ofe++;
         else if (!ofe->name)
            entry = ojit++;
         else
            {
            int32_t diff = STRICMP(ojit->name, ofe->name);
            TR_ASSERT(diff != 0, "Two options with name '%s'", ojit->name);
            if (diff < 0)
               entry = ojit++;
            else
               entry = ofe++;
            }

         // Print category header if this is the first entry in category
         //
         if (!entryFound)
            {
            entryFound = true;
            TR_VerboseLog::writeLine(TR_Vlog_INFO,optionCategoryNames[cat]);
            }

         // Set up the option name
         //
         if (!entry->length)
            entry->length = strlen(entry->name);
         TR_VerboseLog::write("%*s%s",OPTION_NAME_INDENT," ",entry->name);
         int32_t currentColumn = entry->length+OPTION_NAME_INDENT;

         // Set up the argument text
         //
         int32_t i;
         for (i = 1; entry->helpText[i] && entry->helpText[i] != '\t'; i++)
            {}
         if (i > 1)
            TR_VerboseLog::write("%.*s", i-1, entry->helpText+1);
         currentColumn += i-1;

         // Align option description
         //
         if (currentColumn < DESCRIPTION_START_COLUMN)
            TR_VerboseLog::write("%*s", DESCRIPTION_START_COLUMN-currentColumn, " ");
         else
            {
            TR_VerboseLog::writeLine(TR_Vlog_INFO,"%*s", DESCRIPTION_START_COLUMN, " ");
            }
         currentColumn = DESCRIPTION_START_COLUMN;

         // Write option description
         //
         if (entry->helpText[i] == '\t')
            i++;
         int32_t start = i, lastWordBreak = i;
         while (entry->helpText[i])
            {
            if (entry->helpText[i] == '\n')
               // Force a line break
               {
               lastWordBreak = i;
               i = 9999;
               }
            if ((i - start) >= (optionLineWidth - DESCRIPTION_START_COLUMN))
               {
               if (lastWordBreak == start)
                  lastWordBreak = i;
               TR_VerboseLog::write("%.*s",lastWordBreak-start,entry->helpText+start);
               currentColumn = DESCRIPTION_START_COLUMN + DESCRIPTION_TEXT_INDENT;
               TR_VerboseLog::writeLine(TR_Vlog_INFO,"%*s", currentColumn, " ");
               start = i = ++lastWordBreak;
               continue;
               }
            if (entry->helpText[i] == ' ')
               lastWordBreak = i;
            i++;
            }

         TR_VerboseLog::write("%s",entry->helpText+start);
         }
      }
   TR_VerboseLog::writeLine(TR_Vlog_INFO,"");
   }

void
TR_Debug::dumpOptions(
      char *optionsType,
      char *options,
      char *envOptions,
      TR::Options *cmdLineOptions,
      TR::OptionTable *ojit,
      TR::OptionTable *ofe,
      void * feBase,
      TR_FrontEnd *fe)
   {
   TR::OptionTable *entry;
   char *base;
   TR::Compilation* comp = TR::comp();
   static int printCounter = 0;
   static int vmCounter = 0;

#ifdef J9_PROJECT_SPECIFIC
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
#endif

   if(!vmCounter)
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"_______________________________________");

   if(!printCounter)
      {
      fe->printVerboseLogHeader(cmdLineOptions);
      printCounter++;
      }
   bool extendCheck = false;
   if (cmdLineOptions->getVerboseOption(TR_VerboseExtended))
      {
      extendCheck = true;
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%s type: Testarossa (Full) ", optionsType);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"%s ", optionsType);
      }
   TR_VerboseLog::writeLine(TR_Vlog_INFO,"_______________________________________");
   if(vmCounter)
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"JIT ");
   else
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"AOT ");
   vmCounter++;
   TR_VerboseLog::writeLine(TR_Vlog_INFO,"options specified:");
   TR_VerboseLog::writeLine(TR_Vlog_INFO,"     %s", options);
   if (envOptions)
      {
      if(*options)
         TR_VerboseLog::write(",");
      TR_VerboseLog::write(envOptions);
      }

   TR_VerboseLog::writeLine(TR_Vlog_INFO,"");
   TR_VerboseLog::writeLine(TR_Vlog_INFO,"options in effect:");

   while (ojit->name || ofe->name)
      {
      if (ojit->name &&
          (!ojit->msg || (ojit->msg[0] == 'F' && !(ojit->msgInfo & OPTION_FOUND))) && !ojit->enabled)
         {
         ojit++;
         continue;
         }
      if (ofe->name &&
          (!ofe->msg || (ofe->msg[0] == 'F' && !(ofe->msgInfo & OPTION_FOUND))) && !ofe->enabled)
         {
         ofe++;
         continue;
         }
      if (!ojit->name)
         {
         entry = ofe++;
         base = (char*)feBase;
         }
      else if (!ofe->name)
         {
         entry = ojit++;
         base = (char*)cmdLineOptions;
         }
      else
         {
         int32_t diff = STRICMP(ojit->name, ofe->name);
         TR_ASSERT(diff != 0, "Two options with help text");
         if (diff < 0)
            {
            entry = ojit++;
            base = (char*)cmdLineOptions;
            }
         else
            {
            entry = ofe++;
            base = (char*)feBase;
            }
         }

      // See if this entry needs to be printed
      //
      bool printIt = true;
      intptr_t value = 0;
      TR::SimpleRegex * regex = 0;
      if (entry->fcn == TR::Options::setBit)
         {
         if (entry->msg[0] == 'P')
            {
            uint32_t word=*((uint32_t*)(base+entry->parm1));
            printIt = (word & (entry->parm2)) != 0;
            }
         }
      else if (entry->fcn == TR::Options::setVerboseBits)
         {
         if (entry->msg[0] == 'P')
            printIt = cmdLineOptions->isAnyVerboseOptionSet();
         }
      else if (entry->fcn == TR::Options::resetBit)
         {
         if (entry->msg[0] == 'P')
            printIt = ((*((int32_t*)(base+entry->parm1))) & entry->parm2) == 0;
         }
      else if (entry->fcn == TR::Options::setValue)
         {
         value = *(base+entry->parm1);
         if (entry->msg[0] == 'P')
            printIt = (value == entry->parm2);
         }
      else if (entry->fcn == TR::Options::set32BitValue)
         {
         value = (intptr_t)(*((int32_t*)(base+entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value == entry->parm2);
         }
      else if (entry->fcn == TR::Options::setNumeric)
         {
         value = (intptr_t)(*((intptr_t*)(base+entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::set32BitNumeric || entry->fcn == TR::Options::set32BitSignedNumeric || entry->fcn == TR::Options::setCount)
         {
         value = (intptr_t)(*((int32_t*)(base+entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::setStaticNumericKBAdjusted)
         {
         value = *(int32_t *)entry->parm1;
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::setStaticNumeric)
         {
         value = *(int32_t *)entry->parm1;
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::setString)
         {
         value = (intptr_t)(*((char**)(base+entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::setStaticString)
         {
         value = (intptr_t)(*((char**)(entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
#ifdef J9_PROJECT_SPECIFIC
      else if (entry->fcn == TR::Options::setStringForPrivateBase)
         {
         char *localBase = (char*)fej9->getPrivateConfig();
         value = (intptr_t)(*((char**)(localBase+entry->parm1)));
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }
#endif
      else if (entry->fcn == TR::Options::disableOptimization)
         {
         value = ((TR::Options *)base)->isDisabled((OMR::Optimizations)entry->parm1);
         printIt = (value != 0);
         }
      else if (entry->fcn == TR::Options::setRegex)
         {
         regex = *(TR::SimpleRegex **)(base+entry->parm1);
         printIt = (regex != 0);
         }
      else if (entry->fcn == TR::Options::breakOnLoad)
         {
         printIt = false;
         }
      else
         {
         value = entry->msgInfo;
         if (entry->msg[0] == 'P')
            printIt = (value != 0);
         }

      if (printIt && (entry->msg[0]=='F'|| extendCheck))
         {
         TR_VerboseLog::writeLine(TR_Vlog_INFO,"     %s", entry->name);

         if (regex)
            regex->print(false);
         else if (entry->msg[1])
            TR_VerboseLog::write(entry->msg+1, value);
         else if (entry->fcn == TR::Options::setVerboseBits || entry->fcn == TR::Options::setVerboseBitsInJitPrivateConfig)
            {
            //since java uses different function, we need to check for that to get our verbose options printed
            TR_VerboseLog::write("{");
            base = (char*)cmdLineOptions;
            char *sep = "";
            for (int i=0; i < TR_NumVerboseOptions; i++)
               {
               TR_VerboseFlags flag = (TR_VerboseFlags)i;
               if (cmdLineOptions->getVerboseOption(flag))
                  {
                  TR_VerboseLog::write("%s%s", sep, cmdLineOptions->getVerboseOptionName(flag));
                  sep = "|";
                  }
               }
            TR_VerboseLog::write("}");
            }
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   if (fej9->generateCompressedPointers())
      {
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"     ");
      TR_VerboseLog::write("compressedRefs shiftAmount=%d", TR::Compiler->om.compressedReferenceShift());
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"     ");
      TR_VerboseLog::write("compressedRefs isLowMemHeap=%d", (TR::Compiler->vm.heapBaseAddress() == 0));
      }
#endif
   }
