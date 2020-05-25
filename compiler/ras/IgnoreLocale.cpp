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

#include "ras/IgnoreLocale.hpp"

#include <ctype.h>
#include <stddef.h>
#include "env/FrontEnd.hpp"

#define   LOCALE_ENV_VAR   "TR_ProcessOptionsWithLocale"
#define   LOCALE_DEF   static const char *ignoreLocaleOption=feGetEnv(LOCALE_ENV_VAR)
#define USE_LOCALE   (ignoreLocaleOption != NULL)

// tolower that does not respect the locale: for ascii only
int
tolower_ignore_locale(int c)
   {
   LOCALE_DEF;
   if (USE_LOCALE)
      return tolower(c);

   return ((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c;
   }

// toupper that does not respect the locale: for ascii only
int
toupper_ignore_locale(int c)
   {
   LOCALE_DEF;
   if (USE_LOCALE)
      return toupper(c);

   return ((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c;
   }

// stricmp that does not respect the locale: for ascii comparisons only
int
stricmp_ignore_locale(const char *s1, const char *s2)
   {
   LOCALE_DEF;
   if (USE_LOCALE)
      return STRICMP(s1, s2);

   while (1)
      {
      char c1 = *s1++;
      char c2 = *s2++;
      int diff = tolower_ignore_locale(c1) - tolower_ignore_locale(c2);
      if (diff != 0)
         {
         return diff;
         }
      else
         {
         if('\0' == c1)
            {
            return 0;
            }
         }
      }
      return 0;
   }


// strnicmp that does not respect the locale: for ascii comparisons only
int
strnicmp_ignore_locale(const char *s1, const char *s2, size_t length)
   {
   LOCALE_DEF;
   if (USE_LOCALE)
      return STRNICMP(s1, s2, length);

   while (length-- > 0)
      {
      char c1 = *s1++;
      char c2 = *s2++;
      int diff = tolower_ignore_locale(c1) - tolower_ignore_locale(c2);
      if (diff != 0)
         {
         return diff;
         }
      else
         {
         if('\0' == c1)
            {
            return 0;
            }
         }
      }
      return 0;
   }
