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


#ifndef OMR_COMPILER_OPTIONS_MANAGER_HPP
#define OMR_COMPILER_OPTIONS_MANAGER_HPP

#ifndef OMR_COMPILER_OPTIONS_MANAGER_CONNECTOR
#define OMR_COMPILER_OPTIONS_MANAGER_CONNECTOR
namespace OMR { class CompilerOptionsManager; }
namespace OMR { typedef OMR::CompilerOptionsManager CompilerOptionsManagerConnector; }
#endif

#include "control/CompilerOptions.hpp"
#include "control/OptionTable.hpp"
#include "control/OptionProcessors.hpp"
#include "control/OptionsUtil.hpp"
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"
#include "infra/SimpleRegex.hpp"

#include "control/OptionTableProperties.inc"

namespace TR {class CompilerOptionsManager;}

namespace OMR {

/**
 * @brief class CompilerOptionsManager serves as the primary interface for
 * the new Options processing framework
 */
class OMR_EXTENSIBLE CompilerOptionsManager{

protected:

    /**
     * @brief get the ptr to the most specialized implementation of CompilerOptions
     *
     * @return TR::CompilerOptionsManager*
     */
    TR::CompilerOptionsManager * self();

    /**
     * @brief The top-level options object. Currently this field is unused, as this responsibility
     * is handled by TR::Options during the transition phase.
     *
     */
    static TR::CompilerOptions         *_options;

    /**
     * @brief The compilerOptions manager static field
     *
     */
    static TR::CompilerOptionsManager  *_optionsManager;
    static TR::OptionSet *_optionSets;

public:
    TR_ALLOC(TR_Memory::CompilerOptionsManager)

    /**
     * @brief Process the command line options and set default values that can only be determined
     * at runtime. Currently this is unused as option initialization is handled by TR::Options during
     * the transition period
     *
     * @param cmdLineOptions[in] the commandline option string following -Xjit:
     */
    static void initialize(char * cmdLineOptions);

    /**
     * @brief Get the global CompilerOptions object
     *
     * @return TR::CompilerOptions*
     */
    static TR::CompilerOptions * getOptions(){ return _options;}

    /**
     * @brief Get the ptr to member of CompilerOptions corresponding to the existing
     * TR_CompilationOptions enum value, which can be used to set/get option values in
     * CompilerOptions class using the old API
     *
     * @param[in] option the TR_CompilationOptions enum value
     * @return bool TR::CompilerOptions::* the member ptr
     */
    static bool TR::CompilerOptions::* getMemberPtrFromOldEnum(uint32_t option);

#if defined(NEW_OPTIONS_DEBUG)
    /**
     * @brief Get the Option Name From Old TR_CompilationOptions enum value
     *
     * @param[] option the TR_CompilationOptions enum value
     * @return char * the option name
     */
    static char * getOptionNameFromOldEnum(uint32_t option);
#endif

    /**
     * @brief Create a new CompilerOptions object
     *
     * @return TR::CompilerOptions*
     */
    static TR::CompilerOptions* createNewOptions();

    /**
     * We do not need the optionString parameter. This is just temporary to allow us
     * to use the existing OptionSet class
     */

    /**
     * @brief Create a new OptionSet object and add it to the chain of option sets.
     * We do not need the optionString parameter. This is just temporary to allow us
     * to use the existing OptionSet class
     *
     * @param[in] optionString the option string starting at the start of the option set
     * @return TR::OptionSet* the newly initialized OptionSet
     */
    static TR::OptionSet * createNewOptionSet(char * optionString);

    /**
     * @brief Get the CompilerOptions from the optionsets using method signature filter, and
     * optionally the optLevel filter
     *
     * @param[in] methodSignature the method signature string
     * @paramp[in] methodHotness the method hotness string
     * @return TR::CompilerOptions* the CompilerOptions if a match has been found
     */
    static TR::CompilerOptions * getOptions(const char * methodSignature, const char * methodHotness = NULL);

    /**
     * @brief Get the CompilerOptions from the optionsets using line number or option set index
     *
     * @param[in] lineNum the line number in the limit file
     * @paramp[in] index the option set index
     * @return TR::CompilerOptions* the CompilerOptions if a match has been found
     */
    static TR::CompilerOptions * getOptions(int32_t lineNum, int32_t index = 0);

}; /* class CompilerOptionsManager */

} /* namespace OMR */


#endif /* OMR_COMPILER_OPTIONS_MANAGER_HPP */
