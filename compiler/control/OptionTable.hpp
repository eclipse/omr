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

#ifndef OMR_OPTION_TABLE_HPP
#define OMR_OPTION_TABLE_HPP

#include <vector>

#include "env/jittypes.h"
#include "control/CompilerOptions.hpp"
#include <stdio.h>

/**
 * OPTION_MEMBER_TO_SET(x) macro used for specifying the member that is
 * used as lvalue when an option is being set
 * @param[in] x the option member name
 */
#define OPTION_MEMBER_TO_SET(x) {&TR::CompilerOptions::x}


namespace TR{

struct OptionTableItem;

/**
 * Option processing function pointer format, used as one of the members in struct OptionTableItem
 */
typedef void (* OptionProcessingFnPtr)(char * optionStr, void * dataObj, OptionTableItem * entry);

union OptionMemberToSet{
    bool TR::CompilerOptions::* booleanMember;
    size_t TR::CompilerOptions::* numericMember; // just an example, not actually used yet
    //other member types...
};

/**
 * struct OptionTableItem contains information on how to handle options that may appear in the command line and/or env
 * vars. This information is mainly used by the option processing function to process an option.
 */
struct OptionTableItem {

    /**
     * Option name as it would appear on the command line
     */
    char * optionName;

    /**
     * Option category, a single char mapping to the following:
     * todo: could easily use an enum here
     * 
     *  ' ' ==> uncategorized
     *  'C' ==> code generation options
     *  'O' ==> optimization options
     *  'L' ==> log file options
     *  'D' ==> debugging options
     *  'M' ==> miscellaneous options
     *  'R' ==> recompilation and profiling options
     *  'I' ==> internal options (not reported by -Xjit:help) usually for experimentation or testing
     *  '0' - '9' ==> reserved for VM-specific categories
     */
    char * optionCategory;

    /**
     * help text to output for an option if requested
     */
    char * helpText;

    /**
     * Function used for processsing an option
     */
    OptionProcessingFnPtr fcn;

    /**
     * member ptr to the options object member affected by the option being processed
     */
    OptionMemberToSet memberToSet;

    /**
     * parameter used by the processing function for setting fixed values of
     * non-boolean options if they appear on the command line
     */
    uintptrj_t parameter;

    /**
     * boolean member determining if the option can be used as in a option set. Currently
     * all entries set this as false until option subset handling support is added.
     */
    bool isSubsettable;

};


} /* namespace TR */

#endif /* OMR_OPTION_TABLE_HPP */