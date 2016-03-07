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
 
