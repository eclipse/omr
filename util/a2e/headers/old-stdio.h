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
 * Replace the system header file "stdio.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(stdio)'>                   /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include_next <stdio.h>                                            /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_STDIO)
		#define IBM_ATOE_STDIO

		#ifdef __cplusplus
            extern "C" {
		#endif

        FILE *     atoe_fopen     (const char*, char*);
        int        atoe_fprintf   (FILE*, const char*, ...);
        size_t     atoe_fread     (void*, size_t, size_t, FILE*);
        FILE *     atoe_freopen   (const char*, char*, FILE*);
        size_t     atoe_fwrite    (const void*, size_t, size_t, FILE*);
        void       atoe_perror    (const char*);
        int        atoe_printf    (const char*, ...);
        int        atoe_putchar   (int);
        int        atoe_rename    (const char*, char*);
        int        atoe_sprintf   (const char*, char*, ...);
        int        std_sprintf    (const char*, char*, ...);
        int        atoe_sscanf    (const char*, const char*, ...); /*ibm@2609*/
        char *     atoe_tempnam   (const char *, char *);
        int        atoe_vprintf   (const char *, va_list);
        int        atoe_vfprintf  (FILE *, const char *, va_list);
        int        atoe_vsprintf  (char *, const char *, va_list); /*ibm@2580*/

		#ifdef __cplusplus
            }
		#endif

		#undef fopen
		#undef fprintf
		#undef fread
		#undef freopen
		#undef fwrite
		#undef perror
		#undef printf
		#undef putchar
		#undef rename
		#undef sprintf
		#undef sscanf                                     /*ibm@2609*/
		#undef tempnam
		#undef vfprintf
		#undef vsprintf                                   /*ibm@2580*/


		#define fopen           atoe_fopen
		#define fprintf         atoe_fprintf
		#define fread           atoe_fread
		#define freopen         atoe_freopen
		#define fwrite          atoe_fwrite
		#define perror          atoe_perror
		#define printf          atoe_printf
		#define putchar         atoe_putchar
		#define rename          atoe_rename
		#define sprintf         atoe_sprintf
		#define sscanf          atoe_sscanf               /*ibm@2609*/
		#define tempnam         atoe_tempnam
		#define vfprintf        atoe_vfprintf
		#define vprintf         atoe_vprintf
		#define vsprintf        atoe_vsprintf             /*ibm@2580*/
	#endif

#endif

/* END OF FILE */

