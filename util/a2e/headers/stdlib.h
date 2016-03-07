/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
#include "prefixpath.h"
#include PREFIXPATH(stdlib.h)                                   /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_STDLIB)
		#define IBM_ATOE_STDLIB

		#ifdef __cplusplus
            extern "C" {
		#endif

        double     atoe_atof      (const char *);                       /*ibm@27140*/
        int        atoe_atoi      (const char *);
        int        atoe_atol      (const char *);                       /*ibm@3817*/
        char*      atoe_getenv    (const char*);
        int        atoe_putenv    (const char*);
        char *     atoe_realpath  (const char*, char*);
        double     atoe_strtod    (const char *,char **);
        int        atoe_strtol    (const char *,char **, int);    /*ibm@3139*/
        unsigned long atoe_strtoul (const char *,char **, int);    /*ibm@4968*/
	unsigned long long atoe_strtoull( const char *, char **, int );
        int        atoe_system    (const char *);

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
		#undef strtoull
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
		#define strtoull atoe_strtoull
		#define system	 atoe_system

	#endif

#endif

/* END OF FILE */

