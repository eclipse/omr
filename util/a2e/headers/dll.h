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
#include_next <dll.h>                                              /*ibm@28725*/
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
