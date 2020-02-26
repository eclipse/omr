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

#include "control/OptionsBuilder.hpp"

TR::OptionTableItem TR::OptionsBuilder::_optionTable[][OPTION_TABLE_MAX_BUCKET_SIZE] = {

#include "control/OptionTableEntries.inc"

};

const unsigned char TR::OptionsBuilder::_hashingValues[] = {

#include "control/OptionCharMap.inc"

};

void
TR::OptionsBuilder::processOptionString(CompilerOptions * options, char * optionString){

    char * currentOption = optionString;

    while (*currentOption != '\0'){
        if (*currentOption == ','){
            currentOption++;
        }
        else if (*currentOption == '{'){
            currentOption = handleMethodRegexFilter(options, currentOption);
        }
        else if (*currentOption == '['){
            currentOption = handleLimitFileLineRangeFilter(options, currentOption);
        }
        else if (*currentOption >= '0' && *currentOption <= '9'){
            currentOption = handleOptionSetIndexFilter(options, currentOption);
        }
        else {
            currentOption = handleOption(options, currentOption);
        }
    }
}


void
TR::OptionsBuilder::processCmdLineOptions(CompilerOptions *options, char * optionString){
    if (!optionString){
        return;
    }

    if (*optionString != 0){
        TR::OptionsBuilder::processOptionString(options, optionString);
    }

}

void
TR::OptionsBuilder::processEnvOptions(CompilerOptions * options, bool isAOT){
    char * envOptions;
    if (isAOT){
        envOptions = std::getenv("TR_OptionsAOT");
    }
    else{
        envOptions = std::getenv("TR_Options");
    }

    if (!envOptions){
        return;
    }

    TR::OptionsBuilder::processOptionString(options, envOptions);
}

void
TR::OptionsBuilder::processOption(CompilerOptions * options, char * optionString){

    TR::OptionTableItem * entry = getOptionTableEntry(optionString);
    if (entry) entry->fcn(optionString, (void *) options, entry);
    #if defined(NEW_OPTIONS_DEBUG)
    else fprintf(stderr,"Unrecognized option -->: %s\n"
                "Option not in option table entries! Not processed.\n",optionString);
    #endif
}

char *
TR::OptionsBuilder::handleOption(CompilerOptions * options, char * optionString){
    char * str = optionString;
    char c;

    while (1){
        c = *str;
        /**
         * The end of an option would be indicated by the following:
         * ",": when there are options following the current option
         * ")": the end of the options in a option subset
         * "\0": the current option is the last option in the option string
         */
        if (c == ',' || c == ')' ||  c == '\0'){
            processOption(options, optionString);
            break;
        }
        else {
            str++;
        }

    }
    return str;
};

char *
TR::OptionsBuilder::handleOptionSet(CompilerOptions * options, char * optionString){
    char * optionStringStart = optionString;
    if (*optionString == '(')
        optionString++;

    while (1){
        optionString = handleOption(options,optionString);
        if (*optionString == ')'){
            optionString++;
            break;
        }
        else if (*optionString == ','){
            optionString++;
        }
        else{
            fprintf(stderr, "Error processing Option Set -->%s\n", optionStringStart);
            TR_ASSERT(0, "error processing options\n");
            break;
        }
    }

    return optionString;
}

char *
TR::OptionsBuilder::handleMethodRegexFilter(CompilerOptions * options, char * optionString){
    char * regexStart = optionString;
    TR::SimpleRegex * methodRegex = NULL;
    TR::SimpleRegex * optLevelRegex = NULL;

    methodRegex = TR::SimpleRegex::create(optionString);
    if (!methodRegex){
        fprintf(stderr,"Bad regular expression at -->%s\n",regexStart);
        TR_ASSERT(0, "error processing options\n");
    }

    if (*optionString == '{'){ // optLevel regex can follow method regex
        optLevelRegex = TR::SimpleRegex::create(optionString);
        if (!optLevelRegex){
            fprintf(stderr,"Bad opt level regular expression at -->%s\n",regexStart);
            TR_ASSERT(0, "error processing options\n");
        }
    }
    if (*optionString != '('){ // at this point, expect to find an option set enclosed here
        fprintf(stderr, "No Option Set after filter at -->%s\n", regexStart);
        TR_ASSERT (0, "error processing options\n");
    }

    TR::CompilerOptions* newOptions = TR::CompilerOptionsManager::createNewOptions();

    char * startOptString = ++optionString;
    TR::OptionSet * newOptionSet = TR::CompilerOptionsManager::createNewOptionSet(startOptString);

    newOptionSet->setMethodRegex(methodRegex);
    newOptionSet->setOptLevelRegex(optLevelRegex);
    newOptionSet->setNewOptions(newOptions);

    return handleOptionSet(newOptions, optionString);
}

char *
TR::OptionsBuilder::handleLimitFileLineRangeFilter(CompilerOptions * options, char * optionString){
    char * rangeExpressionStart = optionString;
    int32_t startValue = 0;
    int32_t endValue = 0;
    char c;

    // Red the start value
    optionString++; // go past the '['
    if (*optionString <= '0' || *optionString >= '9'){
        fprintf(stderr, "expected limit file line number at -->%s\n", rangeExpressionStart);
        TR_ASSERT(0, "error processing options\n");
    }
    while(1){
        c = *optionString;
        if (c >= '0' && c <= '9'){
            startValue = (startValue * 10) + (c - '0');
            optionString++;
        }
        else if (c == ',' || c == '-' || c == ']') {
            break;
        }
        else {
            fprintf(stderr, "Unexpected limit file range expression at -->%s\n", rangeExpressionStart);
            TR_ASSERT(0, "error processing options\n");
            break;
        }
    }

    // c being ']' would indicate a specific line instead of a range
    if (c == ']'){
        endValue = startValue;
    }
    // read the optional end value delimited by ',' or '-'
    else {
        optionString++; // go past the delimiter
        if (*optionString <= '0' || *optionString >= '9'){
            fprintf(stderr, "expected limit file line number at -->%s\n", rangeExpressionStart);
            TR_ASSERT(0, "error processing options\n");
        }
        while (1){
            c = *optionString;
            if (c >= '0' && c <= '9'){
                endValue = (endValue * 10) + (c - '0');
                optionString++;
            }
            else if (c == ']') {
                break;
            }
            else {
                fprintf(stderr, "Unexpected limit file range expression at -->%s\n", rangeExpressionStart);
                TR_ASSERT(0, "error processing options\n");
                break;
            }
        }
    }

    optionString++; // go past the ']'

    if (*optionString != '('){
        fprintf(stderr, "Missing option set for range expression at -->%s\n", rangeExpressionStart);
        TR_ASSERT(0, "error processing options\n");
    }

    TR::CompilerOptions * newOptions = TR::CompilerOptionsManager::createNewOptions();

    char * startOptString = ++optionString;
    TR::OptionSet * newOptionSet = TR::CompilerOptionsManager::createNewOptionSet(startOptString);

    newOptionSet->setStart(startValue);
    newOptionSet->setEnd(endValue);
    newOptionSet->setNewOptions(newOptions);

    return handleOptionSet(newOptions, optionString);
}

char *
TR::OptionsBuilder::handleOptionSetIndexFilter(CompilerOptions * options, char * optionString){
    char * indexExpressionStart = optionString;
    int32_t optionSetIndex = 0;
    char c;
    while (1){
        c = *optionString;
        if (c >= '0' && c <= '9'){
            optionSetIndex = (optionSetIndex * 10) + (c - '0');
            optionString++;
        }
        else if (c == '('){
            break;
        }
        else{
            fprintf(stderr, "Invalid expression at -->%s", indexExpressionStart);
            TR_ASSERT(0, "error processing options\n");
            break;
        }
    }

    TR::CompilerOptions * newOptions = TR::CompilerOptionsManager::createNewOptions();

    char * startOptString = ++optionString;
    TR::OptionSet * newOptionSet = TR::CompilerOptionsManager::createNewOptionSet(startOptString);

    newOptionSet->setIndex(optionSetIndex);
    newOptionSet->setNewOptions(newOptions);

    return handleOptionSet(newOptions, optionString);
}

size_t
TR::OptionsBuilder::getOptionLength(char * optionString){
    size_t length = 0;
    char c;
    while ((c = *optionString++)){
        if (c == ',' || c == '\0' || c == ')')
            break;
        length++;
    }

    return length;
}

size_t
TR::OptionsBuilder::getOptionNameLength(char * optionString){
    size_t length = 0;
    char c;
    while ((c = *optionString++)){
        if (c == ',' || c == '\0' || c == ')' || c == '=')
            break;
        length++;
    }

    return length;
}

size_t
TR::OptionsBuilder::getOptionParameterLength(char * optionString){
    size_t parameterLength = getOptionLength(optionString)
                - getOptionNameLength(optionString) - 1; // -1 for the '='
}

TR::OptionTableItem *
TR::OptionsBuilder::getOptionTableEntry(char * optionString){

    char c;
    char * optionNamePtr = optionString;
    size_t hash = 5381;
    while ((c = *optionNamePtr++)){
        if (c == ',' || c == '\0' || c == ')' || c == '=')
            break;
        hash = ((hash * 7) + _hashingValues[c]) % OPTION_TABLE_SIZE;
    }

   if (0 == _optionTable[hash][0].optionName){
      return NULL;
   } else {
      char * entryNamePtr;
      for (int i = 0; i < OPTION_TABLE_MAX_BUCKET_SIZE; i++){
         entryNamePtr = _optionTable[hash][i].optionName;
         if (0 == entryNamePtr){
            break;
         }
         optionNamePtr = optionString;
         if (getOptionNameLength(optionString) == strlen(entryNamePtr)){
            char c1, c2;
            int diff = 0;
            while ((c1 = *entryNamePtr++) && ((c2 = *optionNamePtr++))){
               diff = _hashingValues[c1] - _hashingValues[c2];
               if (diff != 0) break;
            }
            if (diff == 0) return &_optionTable[hash][i];
         }
      }
      return NULL;
   }
}
