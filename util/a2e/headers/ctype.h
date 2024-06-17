/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/*
 * ===========================================================================
 * Module Information:
 *
 * DESCRIPTION:
 * Replace the system header file "ctype.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(ctype)'>                   /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include_next <ctype.h>                                            /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

        #if !defined(IBM_ATOE_CTYPE)
                #define IBM_ATOE_CTYPE

                #undef isalnum
                #undef isalpha
                #undef iscntrl
                #undef isdigit
                #undef isgraph
                #undef islower
                #undef isprint
                #undef ispunct
                #undef isspace
                #undef isupper
                #undef isxdigit
                #undef toupper
                #undef tolower

                extern int _ascii_is_tab[256];

                #define _ISALNUM_ASCII  0x0001
                #define _ISALPHA_ASCII  0x0002
                #define _ISCNTRL_ASCII  0x0004
                #define _ISDIGIT_ASCII  0x0008
                #define _ISGRAPH_ASCII  0x0010
                #define _ISLOWER_ASCII  0x0020
                #define _ISPRINT_ASCII  0x0040
                #define _ISPUNCT_ASCII  0x0080
                #define _ISSPACE_ASCII  0x0100
                #define _ISUPPER_ASCII  0x0200
                #define _ISXDIGIT_ASCII 0x0400

                #define _XUPPER_ASCII   0xdf                        /*ibm@4345*/
                #define _XLOWER_ASCII   0x20                        /*ibm@4345*/

                #define _IN_RANGE(c)   ((c >= 0) && (c <= 255))

                #define isalnum(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISALNUM_ASCII) : 0)
                #define isalpha(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISALPHA_ASCII) : 0)
                #define iscntrl(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISCNTRL_ASCII) : 0)
                #define isdigit(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISDIGIT_ASCII) : 0)
                #define isgraph(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISGRAPH_ASCII) : 0)
                #define islower(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISLOWER_ASCII) : 0)
                #define isprint(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISPRINT_ASCII) : 0)
                #define ispunct(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISPUNCT_ASCII) : 0)
                #define isspace(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISSPACE_ASCII) : 0)
                #define isupper(c)     (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISUPPER_ASCII) : 0)
                #define isxdigit(c)    (_IN_RANGE(c) ? (_ascii_is_tab[c] & _ISXDIGIT_ASCII) : 0)

                /*
                 *  In ASCII, upper case characters have the bit off    ibm@4345
                 */
                #define toupper(c)     (islower(c) ? (c & _XUPPER_ASCII) : c)
                #define tolower(c)     (isupper(c) ? (c | _XLOWER_ASCII) : c)

        #endif

#endif

/* END OF FILE */
