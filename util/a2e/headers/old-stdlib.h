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
 * Replace the system header file "stdlib.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(stdlib)'>                  /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include_next <stdlib.h>                                           /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_STDLIB)
		#define IBM_ATOE_STDLIB

		#ifdef __cplusplus
            extern "C" {
		#endif

        double     atoe_atof      (char *);                       /*ibm@27140*/
        int        atoe_atoi      (char *);
        int        atoe_atol      (char *);                       /*ibm@3817*/
        char*      atoe_getenv    (const char*);
        int        atoe_putenv    (const char*);
        char *     atoe_realpath  (const char*, char*);
        double     atoe_strtod    (char *,char **);
        int        atoe_strtol    (const char *,char **, int);    /*ibm@3139*/
        unsigned int atoe_strtoul (const char *,char **, int);    /*ibm@4968*/
        int        atoe_system    (char *);

		#ifdef __cplusplus
            }
		#endif

                #undef atof                                       /*ibm@3817*/
		#undef atoi
                #undef atol                                       /*ibm@3817*/
		#undef getenv
		#undef putenv
		#undef realpath
		#undef strtod
                #undef strtol                                     /*ibm@3139*/
                #undef strtoul                                    /*ibm@4968*/
		#undef system

                #define atof	 atoe_atof                        /*ibm@3817*/
		#define atoi	 atoe_atoi
                #define atol	 atoe_atol                        /*ibm@3817*/
		#define getenv	 atoe_getenv
		#define putenv	 atoe_putenv
		#define realpath atoe_realpath
		#define strtod	 atoe_strtod
                #define strtol   atoe_strtol                      /*ibm@3139*/
                #define strtoul  atoe_strtoul                     /*ibm@4968*/
		#define system	 atoe_system

	#endif

#endif

/* END OF FILE */

