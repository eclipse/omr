/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
   static SimpleRegex *create(const char *& s);

   // Check whether a string matches this regular expression
   //
   bool match(const char *s, bool isCaseSensitive=true, bool useLocale=true);

   static bool match(TR::SimpleRegex *regex, const char *, bool isCaseSensitive=true);
   static bool match(TR::SimpleRegex *regex, int, bool isCaseSensitive=true);
   static bool match(TR::SimpleRegex *regex, TR_ResolvedMethod *, bool isCaseSensitive=true);

   /**
    * \brief Check whether a location identified by the specified \ref TR_ByteCodeInfo
    *        matches the specified regular expression
    *
    * The location described by the \c bcInfo argument is expanded into a string of the
    * form
    *
    * [<tt>\#</tt> <em>outer-method-sig</em>] <tt>\@</tt> <em>bc-offset</em> { <tt>\#</tt> <em>callee-method-sig</em> <tt>\@</tt> <em>bc-offset</em> }*
    *
    * where each <i>callee-method-sig</i> is the signature of an inlined method invocation,
    * and each <i>bc-offset</i> is a bytecode offset within the particular method.  The outermost method
    * signature is optional.
    *
    * For example, if the outermost method <code>Outer.out()V</code> has an inlined reference to
    * <code>Middle.mid()Z</code> at bytecode offset 13, and that in turn has an inlined
    * reference to <code>Inner.in()I</code> at bytecode offset 17, then bytecode offset 19 of
    * that innermost inlined reference would have the following two forms:
    *
    * <ul>
    * <li><tt>#Outer.out()V@13#Middle.mid()Z@17#Inner.in()I@19</tt>
    * <li><tt>@13#Middle.mid()Z@17#Inner.in()I@19</tt>
    * </ul>
    *
    * If either form of the location matches the regular expression, the match is successful.
    *
    * \param[in] regex The regular expression against which to match
    * \param[in] bcInfo A location in the IL
    * \param[in] isCaseSensitive Optional.  Specifies whether the case of letters is significant in matching.  Default is \c true.
    * \return \c true if the location matches the specified regular expression; \c false otherwise
    */
   static bool match(TR::SimpleRegex *regex, TR_ByteCodeInfo &bcInfo, bool isCaseSensitive=true);
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

   static Regex  *processRegex(const char *&s, bool &foundError);
   static Simple *processSimple(const char *&s, TR_YesNoMaybe allowAlternates, bool &foundError);

   Regex *_regex;
   bool   _negate;

   // Length and pointer to the original string that the regex was parsed from
   size_t _regexStrLen;
   const char *_regexStr;
   };

}

#endif
