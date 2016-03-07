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
 * Replace the system header file "stat.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if !defined(IBM_STAT_INCLUDED)  /*ibm@39757*/
#define IBM_STAT_INCLUDED        /*ibm@39757*/

#include "prefixpath.h"
#include PREFIXPATH(sys/stat.h)                          /*ibm@39429*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_SYS_STAT)
		#define IBM_ATOE_SYS_STAT

		#ifdef __cplusplus
            extern "C" {
		#endif

        int atoe_mkdir (const char*, mode_t);
        int atoe_remove (const char*);                     /*ibm@4838*/
        int atoe_chmod (const char*, mode_t);             /*ibm@11596*/
        int atoe_stat (const char*, struct stat*);
        int atoe_statvfs (const char*, struct statvfs *buf);

		#ifdef __cplusplus
            }
		#endif

		#undef mkdir
        #undef remove                                      /*ibm@4838*/
		#undef stat
		#undef chmod                                      /*ibm@11596*/
		#undef statvfs

		#define mkdir           atoe_mkdir
		#define remove          atoe_remove                /*ibm@4838*/
		#define stat(a,b)       atoe_stat(a,b)
		#define statvfs(a,b)    atoe_statvfs(a,b)
		#define lstat(a,b)      atoe_lstat(a,b)
		#define chmod           atoe_chmod                /*ibm@11596*/

	#endif

#endif
#endif /*ibm@39757 - IBM_STAT_INCLUDED*/
/* END OF FILE */

