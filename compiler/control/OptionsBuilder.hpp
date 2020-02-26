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

#ifndef OPTIONS_BUILDER_HPP
#define OPTIONS_BUILDER_HPP

#include "control/CompilerOptionsManager.hpp"
#include "control/OptionTableProperties.inc"

namespace TR
{

/**
 * @brief class OptionsBuilder containing a collection of functions and data members used for
 * processing command line and environment options
 */
class OptionsBuilder{

private:

    /**
     * @brief Process the option string containing a list of options
     *
     * @param options[in] the CompilerOptions being processed
     * @param optionString[in] the option string to process
     */
    static void processOptionString(CompilerOptions * options, char * optionString);

    /**
     * @brief Process an individual option by obtaining the option table entry for the
     * option and calling the processing method
     *
     * @param options[out] the CompilerOptions being processed
     * @param optionString[in] the option string to process
     */
    static void processOption(CompilerOptions * options, char * optionString);

    /**
     * @brief Handle an option set
     *
     * @param options[in] the CompilerOptions being processed
     * @param optionString[in] the remaining option string to process
     *
     * @return char * the remaining option string to process
     */
    static char * handleOptionSet(CompilerOptions * options, char * optionString);

    /**
     * @brief Handle an individual option
     *
     * @param options[out] the CompilerOptions to write to
     * @param optionString[in] the remaining option string to process
     *
     * @return char * the remaining option string to process
     */
    static char * handleOption(CompilerOptions * options, char * optionString);

    /**
     * @brief Handle method signature (and optionally opt-level) regular expression filter
     *
     * @param options[in] the CompilerOptions being processed
     * @param optionString[in] the remaining option string to process
     *
     * @return char * the remaining option string to process
     */
    static char * handleMethodRegexFilter(CompilerOptions * options, char * optionString);

    /**
     * @brief Handle limit file (verbose log) line range (or specific line) filter
     *
     * @param options[in] the CompilerOptions being processed
     * @param optionString[in] the remaining option string to process
     *
     * @return char * the remaining option string to process
     */
    static char * handleLimitFileLineRangeFilter(CompilerOptions * options, char * optionString);

    /**
     * @brief Handle option set index filter
     *
     * @param options[in] the CompilerOptions being processed
     * @param optionString[in] the remaining option string to process
     *
     * @return char * the remaining option string to process
     */
    static char * handleOptionSetIndexFilter(CompilerOptions * options, char * optionString);

    /**
     * @brief Get the option length including the length of the option parameter and '='
     *
     * @param optionString[in] the ptr to the option string pointing to the start of the option
     * @return size_t the length of the option string including parameter
     */
    static size_t getOptionLength(char * optionString);

    /**
     * @brief Get the option name length (excludes option parameters)
     *
     * @param optionString[in] the option string being processed
     * @return size_t the length of the option name
     */
    static size_t getOptionNameLength(char * optionString);

    /**
     * Get option parameter length i.e, the part after '=' in the option
     * This should return a positive number. This will return -1 if the
     * option does not have a parameter
     */

    /**
     * @brief Get option parameter length i.e, the part after '=' in the option
     * This should return a positive number. This will return -1 if the
     * option does not have a parameter and 0 if the parameter is empty
     *
     * @param optionString[in] the option string being processed
     * @return size_t the length of the option parameter
     * @return -1 if no parameter
     * @return 0 if empty parameter
     */
    static size_t getOptionParameterLength(char * optionString);

    /**
     * @brief Get the Option Table Entry from the hash table
     *
     * @param optionString[in] the option string ptr at the current option being processed
     * @return TR::OptionTableItem* the entry in the option table
     */
    static TR::OptionTableItem * getOptionTableEntry(char * optionString);

    /**
     * @brief A 2D array representing the option hash table generated at build time
     * and laid out in OptionTableEntries.inc
     */
    static TR::OptionTableItem _optionTable[][OPTION_TABLE_MAX_BUCKET_SIZE];

    /**
     * @brief The character values used for calculating hash values from the option names
     * in the option string. The values are determined at build time and laid out in
     * OptionCharMap.inc
     */
    static const unsigned char _hashingValues[];

public:

    /**
     * @brief Process the command line option string
     *
     * @param options[out] the "global" options object
     * @param optionString[in] the command-line option string after -Xjit:
     */
    static void processCmdLineOptions(CompilerOptions *options, char * optionString);

    /**
     * @brief Process the environment options
     *
     * @param options[out] the "global" options object
     * @param isAOT[in] default False, determines whether to check TR_Options or TR_OptionsAOT
     */
    static void processEnvOptions(CompilerOptions * options, bool isAOT = false);

}; /* class OptionsBuilder */


} /* namespace TR */

#endif /* OPTIONS_BUILDER_HPP */