/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef omrstdarg_h
#define omrstdarg_h

#include <stdarg.h>
#include <string.h>

#include "omrcomp.h"

#if defined(va_copy)
#define COPY_VA_LIST(new,old) va_copy(new,  old)
#else
#define COPY_VA_LIST(new,old) memcpy(VA_PTR(new), VA_PTR(old), sizeof(va_list))
#endif

#if defined(va_copy)
#define END_VA_LIST_COPY(args) va_end(args)
#else
#define END_VA_LIST_COPY(args)
#endif

#endif     /* omrstdarg_h */


