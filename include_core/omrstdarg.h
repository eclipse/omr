/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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


