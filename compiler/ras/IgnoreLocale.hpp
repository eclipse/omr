/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include <ctype.h>
#include <stddef.h>

// These functions should be used anywhere the locale should not influence how characters are compared
// e.g. option names are ascii strings that should not be compared differently in different locales

int tolower_ignore_locale(int c);
int toupper_ignore_locale(int c);
int stricmp_ignore_locale(const char *s1, const char *s2);
int strnicmp_ignore_locale(const char *s1, const char *s2, size_t n);

#if defined(OMR_OS_WINDOWS)
   #define STRICMP _stricmp
   #define STRNICMP _strnicmp
#else
   #include <strings.h>
   #define STRICMP strcasecmp
   #define STRNICMP strncasecmp
#endif /* OMR_OS_WINDOWS */
