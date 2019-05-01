/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef SIMPLEREGEX_INCL
#define SIMPLEREGEX_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"

class TR_ResolvedMethod;

#define BITSPERUL 32
#if BITSPERUL == 32
#define BWORD(x)        ((x) >> 5)
#define BBIT(x)         ((x) & 0x1f)
#else
#define BWORD(x)        ((x) / (CHAR_BIT * sizeof(unsigned long int)))
#define BBIT(x)         ((x) % (CHAR_BIT * sizeof(unsigned long int)))
#endif

namespace TR
{

// Simple regular expression
//
class SimpleRegex
   {
   public:
   TR_ALLOC(TR_Memory::SimpleRegex)

   // Create a new regular expression
   //
   static SimpleRegex *create(char *&s);

   // Check whether a string matches this regular expression
   //
   bool match(const char *s, bool isCaseSensitive=true, bool useLocale=true);

   static bool match(TR::SimpleRegex *regex, const char *, bool isCaseSensitive=true);
   static bool match(TR::SimpleRegex *regex, int, bool isCaseSensitive=true);
   static bool match(TR::SimpleRegex *regex, TR_ResolvedMethod *, bool isCaseSensitive=true);
   static bool matchIgnoringLocale(TR::SimpleRegex *regex, const char *, bool isCaseSensitive=true);

   void print(bool negate);

   // Get the original string the regex was parsed from.
   // This pointer is only valid as long as the original string is not freed.
   // It is NOT null terminated.
   //
   const char *regexStr() const { return _regexStr; }
   size_t regexStrLen() const { return _regexStrLen; }

   enum ComponentType{simple_string, wildcards, char_alternatives};

   struct Component
      {
      TR_ALLOC(TR_Memory::SimpleRegexComponent)
      void *operator new (size_t size, PERSISTENT_NEW_DECLARE, size_t numChars);

      ComponentType type;
      union Data {
         char     str[1]; /* really N */
         uint64_t bit_map[BWORD(256)];
         uint64_t counts;
         } data;
      };

   struct Simple
      {
      TR_ALLOC(TR_Memory::SimpleRegexSimple)
      bool match(const char *s, bool caseSensitive, bool useLocale);
      bool matchesRemainder(const char *s, bool caseSensitive, bool useLocale);
      void print();
      Component* component;
      Simple*    remainder;
      uint32_t   fixed_chars_right;
      };

   struct Regex
      {
      TR_ALLOC(TR_Memory::SimpleRegexRegex)
      bool match(const char *s, bool caseSensitive, bool useLocale);
      void print();
      Simple *simple;
      Regex  *remainder;
      };

   private:

   static Regex  *processRegex(char *&s, bool &foundError);
   static Simple *processSimple(char *&s, TR_YesNoMaybe allowAlternates, bool &foundError);

   Regex *_regex;
   bool   _negate;

   // Length and pointer to the original string that the regex was parsed from
   size_t _regexStrLen;
   char *_regexStr;
   };

}

#endif
