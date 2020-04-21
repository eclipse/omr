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

#include <stdint.h>
#include "env/jittypes.h"

#ifdef TR_BIGEND_TO_LITEND
// byte order big to little long (8 bytes)
inline uint64_t bol(uint64_t arg)
   {
   char *x = (char *) &arg;
   for (int i = 0; i < 4; ++i)
      {
      char y = x[i];
      x[i] = x[7 - i];
      x[7 - i] = y;
      }
   return arg;
   }

// byte order big to little int (4 bytes)
inline uint32_t boi(uint32_t arg)
   {
   char *x = (char *) &arg;
   for (int i = 0; i < 2; ++i)
      {
      char y = x[i];
      x[i] = x[3 - i];
      x[3 - i] = y;
      }
   return arg;
   }

// byte order big to little address (4 or 8 bytes)
inline intptr_t boa(intptr_t arg)
   {
   if (sizeof(intptr_t) == 4)
      return boi(arg);
   else
      return bol(arg);
   }

// byte order big to little short (2 bytes)
inline uint16_t bos(uint16_t arg)
   {
   char *x = (char *) &arg;
   int i = 0;
   char y = x[i];
   x[i] = x[1];
   x[1] = y;
   return arg;
   }
#else
inline uint64_t bol(uint64_t arg)   { return arg; }
inline uint32_t boi(uint32_t arg)   { return arg; }
inline intptr_t boa(intptr_t arg) { return arg; }
inline uint16_t bos(uint16_t arg)   { return arg; }
#endif
