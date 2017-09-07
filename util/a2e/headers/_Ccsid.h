/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/
/*
 * ===========================================================================
 * Module Information:
 *
 * DESCRIPTION:
 * Replace the system header file "_Ccsid.h" so that we can redefine
 * the ccsid functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   
#include <//'PP.ADLE370.OS39028.SCEEH.H(Ccsid)'>                   
#else                                                              
#include "prefixpath.h"
#include PREFIXPATH(_Ccsid.h)                                    
#endif                                                             

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_CCSID)
		#define IBM_ATOE_CCSID

		#ifdef __cplusplus
            extern "C" {
		#endif

	__csType atoe___CSNameType(char *codesetName);
	__ccsid_t atoe___toCcsid(char *codesetName);

		#ifdef __cplusplus
            }
		#endif

		#undef atoe___CSNameType
		#undef atoe___toCcsid

		#define __CSNameType 	atoe___CSNameType 
		#define __toCcsid	atoe___toCcsid	 

		#define vsnprintf	atoe_vsnprintf
	#endif

#endif
 
