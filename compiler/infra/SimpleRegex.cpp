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

#include "infra/SimpleRegex.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/DataTypes.hpp"
#include "ras/Debug.hpp"
#include "ras/IgnoreLocale.hpp"


/***
    This is a mechanism to define a "filter."  A "filter" is basically
    a (form of a) regular expression that allows one to specify a set
    of class names, method names, etc.
    <filter>:           <simple_pattern> | <simple_pattern> "[:|]" <filter>
    <simple_pattern>:   <component> | <component> <simple_pattern>
    <component>:        <str> | <wildcards> | <alternate>

    for example, proc[d-g]:func*z will define a filter that matches
    procd, proce, procf and procg as well as any string that begins
    with func and ends with z.

    If the first simple_pattern of a filter is a ^, the truth value
    of the entire filter is inverted.
***/


namespace TR
{

SimpleRegex *SimpleRegex::create(char *&s)
   {
   if (s == NULL || s[0] != '{')
      return NULL;
   char *origStr = s;
   ++s;
   bool negate = (s[0] == '^');
   if (negate)
      ++s;
   bool foundError = false;
   Regex *regex = processRegex(s, foundError);
   if (!regex || s[0] != '}' || foundError)
      return NULL;
   ++s;
   SimpleRegex *result = (SimpleRegex *)jitPersistentAlloc(sizeof(SimpleRegex));
   result->_regex = regex;
   result->_negate = negate;
   result->_regexStr = origStr;
   result->_regexStrLen = s - origStr;
   return result;
   }


SimpleRegex::Regex *SimpleRegex::processRegex(char *&s, bool &foundError)
   {
   // First get rid of all + and |
   //
   while (s[0] == ',' || s[0] == '|')
      ++s;
   if (s[0] == '}' || s[0] == '\0')
      return NULL;

   Regex *regex = (Regex *)jitPersistentAlloc(sizeof(Regex));
   regex->simple = processSimple(s, TR_maybe, foundError);
   if (foundError)
      return NULL;
   regex->remainder = processRegex(s, foundError);
   if (foundError)
      return NULL;
   return regex;
   }


void *SimpleRegex::Component::operator new (size_t size, PERSISTENT_NEW_DECLARE,  size_t numChars)
   {
   if (numChars > sizeof(Data))
      size += numChars - sizeof(Data);
   return jitPersistentAlloc(size);
   }


// Process a simple_pattern: a sequence of components
//
SimpleRegex::Simple *SimpleRegex::processSimple(char *&s, TR_YesNoMaybe allowAlternates, bool &foundError)
   {
   int32_t i,lo,hi;
   char *startSimple = s;

   if (s[0] == '\0' || s[0] == ',' || s[0] == '|' || s[0] == '}')
      return NULL;

   Simple *m = (Simple *)jitPersistentAlloc(sizeof(Simple));

   // Decide if we are going to allow alternatives - if there are no ']' chars
   // in the regex then '[' chars are treated as regular characters (since they
   // can occur in method signatures).
   //
   if (s[0] == '[' && allowAlternates == TR_maybe)
      {
      if (s[1] == '^')
         allowAlternates = TR_yes;
      else
         {
         allowAlternates = TR_no;
         for (i = 0; s[i] != '}' && s[i] != '\0'; i++)
            {
            if (s[i] == ']')
               {
               allowAlternates = TR_yes;
               break;
               }
            if (s[i] == '\\' && s[i+1] != '\0')
               i++;
            }
         }
      }
   if (s[0] == '[' && allowAlternates == TR_yes)
      {
      // process character alternatives
      //
      bool invert = false;
      m->component = new (PERSISTENT_NEW, 0) Component;
      m->component->type = char_alternatives;
      for (i = 0;
           i < sizeof(m->component->data.bit_map) / sizeof(m->component->data.bit_map[0]);
           m->component->data.bit_map[i++] = 0)
         {}
      ++s;
      if (s[0] == '^')
         {
         invert = true;
         allowAlternates = TR_yes;
         ++s;
         }
      while (s[0] != ']' && s[0] != '}' && s[0] != '\0')
         {
         if (s[0] == '\\' && s[1] != '\0')
            ++s;
         if (s[1] == '-' && s[2] != ']' && s[2] != '\0')
            {
            lo = s[0];
            s += 2;
            if (s[0] == '\\' && s[1] != ']' && s[1] != '\0')
               ++s;
            }
         else
            {
            lo = s[0];
            }
         hi = s[0];
         ++s;
         if (lo > hi)
            {
            i = lo; lo = hi; hi = i;
            }
         for (i = lo; i <= hi; ++i)
            {
            m->component->data.bit_map[BWORD(i)] |= (CONSTANT64(1) << BBIT(i));
            }
         }
      if (s[0] != ']')
         {
         s = startSimple;
         foundError = true;
         return NULL;
         }
      ++s;
      if (invert)
         {
         for (i = 0;
              i < sizeof(m->component->data.bit_map) / sizeof(m->component->data.bit_map[0]);
              i++)
            {
            m->component->data.bit_map[i] = ~m->component->data.bit_map[i];
            }
         }
      }
   else if (s[0] == '?' || s[0] == '*')
      {
      // process wild card characters
      //
      m->component = new (PERSISTENT_NEW, 0) Component;
      m->component->type = wildcards;
      m->component->data.counts = 0;
      while (s[0] == '?' || s[0] == '*')
         {
         if (s[0] == '?')
            m->component->data.counts += 2;
         else
            m->component->data.counts |= 1;
         ++s;
         }
      }
   else
      {
      // process ordinary characters
      //
      i = 0;
      while (s[i] != '\0' && s[i] != '*' && s[i] != '?' &&
             s[i] != ',' && s[i] != '|' && s[i] != '}')
         {
         if (s[i] == '[' && allowAlternates != TR_no)
            break;
         if (s[i] == '\\' && s[i+1] != '\0')
            ++i;
         ++i;
         }
      m->component = new (PERSISTENT_NEW, i+1) Component;
      m->component->type = simple_string;
      i = 0;
      while (s[0] != '\0' && s[0] != '*' && s[0] != '?' &&
             s[0] != ',' && s[0] != '|' && s[0] != '}')
         {
         if (s[0] == '[' && allowAlternates != TR_no)
            break;
         if (s[0] == '\\' && s[1] != '\0')
            ++s;
         m->component->data.str[i++] = s[0];
         ++s;
         }
      m->component->data.str[i++] = '\0';
      }

   m->remainder = processSimple(s, allowAlternates, foundError);
   if (foundError)
      return NULL;
   if (m->remainder == NULL)
      m->fixed_chars_right = 0;
   else
      {
      if ((m->remainder->component->type == wildcards &&
           (m->remainder->component->data.counts & 1) != 0) ||
          (m->remainder->fixed_chars_right == 0 &&
           m->remainder->remainder != NULL))
         {
         m->fixed_chars_right = 0;
         }
      else
         {
         i = 0;
         switch (m->remainder->component->type)
            {
            case simple_string:
               i = (int32_t)strlen(m->remainder->component->data.str);
               break;
            case wildcards:
               i = m->remainder->component->data.counts >> 1;
               break;
            case char_alternatives:
               i = 1;
               break;
            default:
               /*** ???? ***/
               break;
            }
         m->fixed_chars_right = m->remainder->fixed_chars_right + i;
         }
      }
   return m;
   }

bool SimpleRegex::Simple::matchesRemainder(
   const char *s,
   bool isCaseSensitive,
   bool useLocale
   )
   {
   if (remainder == NULL)
      return s[0] == '\0';

   return remainder->match(s, isCaseSensitive, useLocale);
   }

bool SimpleRegex::Simple::match(const char *s, bool isCaseSensitive, bool useLocale)
   {
   int i;

   switch (component->type)
      {
      case simple_string:
         for (i = 0; component->data.str[i]; ++i, ++s)
            {
            if (isCaseSensitive)
               {
               if (s[0] != component->data.str[i])
                  return false;
               }
            else
               {
               char lower_s0 = useLocale ? (tolower(s[0])) : (::tolower_ignore_locale(s[0]));
               char lower_stri = useLocale ? (tolower(component->data.str[i])) : (::tolower_ignore_locale(component->data.str[i]));
               bool notEqual = (lower_s0 != lower_stri);
               if (notEqual)
                  return false;
               }
            }
         return matchesRemainder(s, isCaseSensitive, useLocale);
         break;
      case wildcards:
         if (strlen(s) < component->data.counts >> 1)
            return false;
         s += (component->data.counts >> 1);
         if ((component->data.counts & 1) == 0)
            {
            return matchesRemainder(s, isCaseSensitive, useLocale);
            }
         if (fixed_chars_right != 0 || remainder == NULL)
            {
            return (strlen(s) >= fixed_chars_right) &&
                    matchesRemainder(s + strlen(s) - fixed_chars_right, isCaseSensitive, useLocale);
            }

         // tricky case abc*def*ghi
         //
         do
            {
            if (matchesRemainder(s, isCaseSensitive, useLocale))
               return true;
            } while (*++s);
         return false;
         break;
      case char_alternatives:
         {
         i = s[0];
         bool charIsInBitMap = ((component->data.bit_map[BWORD(i)] & (CONSTANT64(1) << BBIT(i))) != 0);
         if (!isCaseSensitive && !charIsInBitMap)
            {
            char lower_i = useLocale ? (tolower(i)) : (::tolower_ignore_locale(i));
            char upper_i = useLocale ? (toupper(i)) : (::toupper_ignore_locale(i));
            charIsInBitMap =
                  ((component->data.bit_map[BWORD(lower_i)] & (CONSTANT64(1) << BBIT(lower_i))) != 0)
               || ((component->data.bit_map[BWORD(upper_i)] & (CONSTANT64(1) << BBIT(upper_i))) != 0);
            }
         return charIsInBitMap && matchesRemainder(++s, isCaseSensitive, useLocale);
         break;
         }
      default:
         /*** ???? ***/
         return false;
      break;
      }
   // this is just to quiet the compiler -- it's never reached
   //
   return false;
   }


bool SimpleRegex::Regex::match(const char *s, bool isCaseSensitive, bool useLocale)
   {
   int32_t rc = false;
   for (Regex *p = this; p && !rc; p = p->remainder)
      {
      rc = p->simple->match(s, isCaseSensitive, useLocale);
      }
   return rc;
   }


bool SimpleRegex::match(const char *s, bool isCaseSensitive, bool useLocale)
   {
   bool result = _regex->match(s, isCaseSensitive, useLocale);
   if (_negate)
      result = !result;
   return result;
   }


void SimpleRegex::print(bool negate)
   {
   TR_VerboseLog::write("{");
   if (negate ^ _negate)
      TR_VerboseLog::write("^");
   _regex->print();
   TR_VerboseLog::write("}");
   }


void SimpleRegex::Regex::print()
   {
   if (simple)
      simple->print();
   if (remainder)
      {
      TR_VerboseLog::write("|");
      remainder->print();
      }
   }


void SimpleRegex::Simple::print()
   {
   int32_t i;
   switch (component->type)
      {
      case wildcards:
         for (i = 2; i <= component->data.counts; i += 2)
            TR_VerboseLog::write("?");
         if (component->data.counts & 1)
            TR_VerboseLog::write("*");
         break;
      case char_alternatives:
         {
         TR_VerboseLog::write("[");
         if ((component->data.bit_map[BWORD(0)] & (1 << BBIT(0))) != 0)
            {
            TR_VerboseLog::write("^");
            for (i = 1; i < 256; i++)
               {
               if ((component->data.bit_map[BWORD(i)] & (CONSTANT64(1) << BBIT(i))) == 0)
                  TR_VerboseLog::write("%c", i);
               }
            }
         else
            {
            for (i = 1; i < 256; i++)
               {
               if ((component->data.bit_map[BWORD(i)] & (CONSTANT64(1) << BBIT(i))) != 0)
                  TR_VerboseLog::write("%c", i);
               }
            }
         TR_VerboseLog::write("]");
         break;
         }
      case simple_string:
         TR_VerboseLog::write("%s", component->data.str);
         break;
      }
   if (remainder)
      remainder->print();
   }


bool SimpleRegex::match(TR::SimpleRegex *regex, const char *s, bool isCaseSensitive)
   {
   return regex->match(s, isCaseSensitive, true);
   }


bool SimpleRegex::match(TR::SimpleRegex *regex, int32_t i, bool isCaseSensitive)
   {
   char s[20];
   sprintf(s, "%d", i);
   return regex->match(s, isCaseSensitive, true);
   }


bool SimpleRegex::match(
      TR::SimpleRegex *regex,
      TR_ResolvedMethod * feMethod,
      bool isCaseSensitive)
   {
   TR::Compilation *comp = TR::comp();
   TR::StackMemoryRegion stackMemoryRegion(*comp->trMemory());

   bool result = regex->match(feMethod->signature(comp->trMemory(), stackAlloc), isCaseSensitive, true);

   return result;
   }


bool SimpleRegex::matchIgnoringLocale(
      TR::SimpleRegex *regex,
      const char *s,
      bool isCaseSensitive)
   {
   return regex->match(s, isCaseSensitive, false);
   }

}
