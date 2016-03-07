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
 * Replace the system header file "time.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#define _OPEN_MSGQ_EXT                                             /*ibm@57343*/

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(time)'>                    /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include "prefixpath.h"
#include PREFIXPATH(time.h)                                     /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_TIME)
		#define IBM_ATOE_TIME

		#ifdef __cplusplus
            extern "C" {
		#endif

        char * atoe_ctime     (const time_t*);
        size_t atoe_strftime  (char*, size_t,const char*, const struct tm*);

		#ifdef __cplusplus
            }
		#endif

		#undef ctime
		#undef strftime

		#define ctime           atoe_ctime
		#define strftime        atoe_strftime

	#endif

#endif

/* END OF FILE */

