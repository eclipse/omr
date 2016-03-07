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
 * Replace the system header file "dll.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * This header file is unique in the set in that it has a atoe prefix this
 * is due to a name clash with the JVM header file dll.h
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(dll)'>                     /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include "prefixpath.h"
#include PREFIXPATH(dll.h)                                      /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_DLL)
		#define IBM_ATOE_DLL

		#ifdef __cplusplus
                  extern "C" {
		#endif
	        dllhandle* atoe_dllload(char *);
		#ifdef __cplusplus
                  }
		#endif

                #ifdef __cplusplus
                  extern "C" {
                #endif
                void *atoe_dllqueryvar(dllhandle* dllHandle, char* varName);
                #ifdef __cplusplus
                  }
                #endif

                #ifdef __cplusplus
                  extern "C" {
                #endif 
                void (*atoe_dllqueryfn(dllhandle* dllHandle, char* funcName)) ();
                #ifdef __cplusplus
                  }
                #endif

		#undef     dllload
		#undef     dllqueryfn
		#undef     dllqueryvar

		#define    dllload     atoe_dllload
		#define    dllqueryfn  atoe_dllqueryfn
		#define    dllqueryvar  atoe_dllqueryvar

	#endif

#endif

/* END OF FILE */
