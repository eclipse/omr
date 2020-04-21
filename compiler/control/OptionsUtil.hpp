/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TR_OPTIONSUTIL_INCL
#define TR_OPTIONSUTIL_INCL

#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/SimpleRegex.hpp"
#include "infra/Assert.hpp"


namespace TR
{

struct OptionTable;
class Options;


// Shape of an option processing method
//
typedef char * (* OptionFunctionPtr)(char *option, void *base, OptionTable *entry);

// Flags used for the msgInfo field in TR::OptionTable
//
#define NOT_IN_SUBSET 0x1 // option can only be used as a top-level option
#define OPTION_FOUND  0x2 // option was specified on the command line


/**
 * Option table entry format. There are two option tables, one for jit options
 * and one for VM options, each consisting of an array of these entries. The
 * last entry in each array is marked by a null "name" field.
 */
struct OptionTable
   {
   /**
    * Name of the option, including '=' after the name if required
    */
   const char *name;

   /**
    * Help text
    *
    * The first character of the help text is a category identifier
    *
    *     ' ' ==> uncategorized
    *     'C' ==> code generation options
    *     'O' ==> optimization options
    *     'L' ==> log file options
    *     'D' ==> debugging options
    *     'M' ==> miscellaneous options
    *     'R' ==> recompilation and profiling options
    *     'I' ==> internal options (not reported by -Xjit:help) usually for experimentation or testing
    *     '0' - '9' ==> reserved for VM-specific categories
    *
    * The next part (up to the tab character) is the argument text.
    *
    * The last part is the option description - newlines can be used to break
    * the description text.
    */
   const char *helpText;

   /**
    * The method to be used to process the option. There are a number of
    * standard methods provided by options processing, or you can write your
    * own.
    */
   OptionFunctionPtr fcn;

   /**
    * Parameters to be passed to the processing method.
    */
   intptr_t parm1;
   uintptr_t parm2;

   /**
    * Message information to be printed if this option is in effect
    * The first character of the message is an indication of whether the message
    * is to be printed:
    *     ' ' ==> always print the option message
    *     'F' ==> only print if option found (msgInfo has the OPTION_FOUND bit set)
    *     'P' ==> only print if option value is non-zero (or non-null)
    */
   const char *msg;

   /**
    * Message insert info. This is set to 0 or 1 by the option table arrays.
    * The option processing routine can change it to a more meaningful value
    *
    * For the common option table, if the option table entry sets msgInfo to 1
    * this option can only be used as a top-level option, and not in an option subset.
    * For the VM-specific table this is not required since none of these options can
    * appear in an option subset.
    *
    * If the option was found in the option string the OPTION_FOUND bit will be set
    */

   intptr_t msgInfo;

   /**
    * Length of the option name. This is set to zero by the option table arrays,
    * and is caluclated as required and cached here by options processing.
    */
   int32_t length;

   /**
    * A field used to determine if option has been seen on the command line
    */
   bool enabled;

   /**
    * A field used to determine whether or not this is the option to find
    * in the table. This is used in the binary search in
    * OMR::Options::processOption.
    */
   bool isOptionToFind;

   };


// An option set - this is a set of options that apply to a subset of the
// methods to be compiled.
//
class OptionSet
   {
public:

   TR_ALLOC(TR_Memory::OptionSet)

   OptionSet(char *s) { init(s); }

   void init(char *s) { _optionString = s; _next = 0; _methodRegex = 0; _optLevelRegex = 0; _start=0; _end=0; }

   OptionSet *getNext() {return _next;}

   intptr_t getIndex() {intptr_t i = (intptr_t)_methodRegex; return (i&1) ? i >> 1 : 0 ; }
   TR::SimpleRegex *getMethodRegex() {intptr_t i = (intptr_t)_methodRegex; return (i&1) ? 0 : _methodRegex; }
   TR::SimpleRegex *getOptLevelRegex() {return _optLevelRegex; }
   bool match(const char *s) { TR_ASSERT(false, "should be unreachable"); return false; }
   TR::Options *getOptions() {return _options;}
   char *getOptionString() {return _optionString;}
   int32_t getStart() {return _start;}
   int32_t getEnd() {return  _end;}

   void setNext(OptionSet *n) {_next = n;}
   void setOptions(TR::Options *o) {_options = o;}
   void setIndex(intptr_t i) { _methodRegex = (TR::SimpleRegex*)(2*i+1);}
   void setMethodRegex(TR::SimpleRegex *r) {_methodRegex = r;}
   void setOptLevelRegex(TR::SimpleRegex *r) {_optLevelRegex = r;}
   void setStart(int32_t n) {_start=n;}
   void setEnd(int32_t n) {_end=n;}

private:

   OptionSet *_next;
   TR::SimpleRegex *_methodRegex;
   TR::SimpleRegex *_optLevelRegex;
   int32_t _start;
   int32_t _end;
   union
      {
      TR::Options *_options;
      char *_optionString;
      };
   };

}

#endif
