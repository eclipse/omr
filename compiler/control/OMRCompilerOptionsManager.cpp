/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "control/OMRCompilerOptionsManager.hpp"
#include "control/OptionsBuilder.hpp"
#include "codegen/FrontEnd.hpp"
#include "infra/Assert.hpp"
#include "control/Options.hpp"

TR::CompilerOptionsManager * OMR::CompilerOptionsManager::_optionsManager = 0;
TR::CompilerOptions * OMR::CompilerOptionsManager::_options = 0;
TR::OptionSet * OMR::CompilerOptionsManager::_optionSets = 0;

TR::CompilerOptionsManager*
OMR::CompilerOptionsManager::self(){
   return static_cast<TR::CompilerOptionsManager*>(this);
}

void
OMR::CompilerOptionsManager::initialize(char * cmdLineOptions){
#if 0
   /* Currently disabled because the options initialization is currently
   happening in the existing options class */
    _optionsManager = new (PERSISTENT_NEW) TR::CompilerOptionsManager();
    _options = new (PERSISTENT_NEW) TR::CompilerOptions();

    //_optionsManager->setDefaults();

    TR::OptionsBuilder::processCmdLineOptions(_options,cmdLineOptions);

    TR::OptionsBuilder::processEnvOptions(_options);

    //_optionsManager->postProcess();

    _options->optionsProcessed = true;
#endif
    return;
}

bool TR::CompilerOptions::*
OMR::CompilerOptionsManager::getMemberPtrFromOldEnum(uint32_t option){
   switch (option){
      #include "control/OptionTranslatingSwitch.inc"
      default:
//#if defined(NEW_OPTIONS_DEBUG)
      //fprintf(stderr, "Warning! Unknown boolean option queried!\n");
//#endif
         return &TR::CompilerOptions::unknownBooleanOption;

   return &TR::CompilerOptions::unknownBooleanOption;
   };
}

#if defined(NEW_OPTIONS_DEBUG)
char *
OMR::CompilerOptionsManager::getOptionNameFromOldEnum(uint32_t option){
   switch (option){
      #include "control/OptionEnumToStringSwitch.inc"
      default: return "Uknown option";
   }
}
#endif

TR::CompilerOptions*
OMR::CompilerOptionsManager::createNewOptions(){
   TR::CompilerOptions* newOptions = new (PERSISTENT_NEW) TR::CompilerOptions();
   return newOptions;
}

TR::OptionSet*
OMR::CompilerOptionsManager::createNewOptionSet(char * optionString){
   TR::OptionSet* newOptionSet = new (PERSISTENT_NEW) TR::OptionSet(optionString);
   newOptionSet->setNext(_optionSets);
   _optionSets = newOptionSet;
   return newOptionSet;
}

TR::CompilerOptions *
OMR::CompilerOptionsManager::getOptions(const char * methodSignature, const char * methodHotness){
   for (TR::OptionSet * optionSet = _optionSets; optionSet; optionSet = optionSet->getNext()){
         if (optionSet->getMethodRegex()){
            if (TR::SimpleRegex::match(optionSet->getMethodRegex(), methodSignature)){
               if (optionSet->getOptLevelRegex()){
                  if (TR::SimpleRegex::matchIgnoringLocale(optionSet->getOptLevelRegex(), methodHotness)){
                     return optionSet->getNewOptions();
                  }
               }
               else{
                  return optionSet->getNewOptions();
               }
            }
         }
   }

   return NULL;
}

TR::CompilerOptions *
OMR::CompilerOptionsManager::getOptions(int32_t lineNum, int32_t index){
   if (index){
      for (TR::OptionSet * optionSet = _optionSets; optionSet; optionSet = optionSet->getNext()){
         if (optionSet->getIndex() == index)
            return optionSet->getNewOptions();
      }
   }
   else if(lineNum){
      for (TR::OptionSet * optionSet = _optionSets; optionSet; optionSet = optionSet->getNext()){
         if ((optionSet->getStart() <= lineNum) && (optionSet->getEnd() >= lineNum))
            return optionSet->getNewOptions();
      }
   }

   return NULL;
}
