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
 * Replace the system header file "stat.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if !defined(IBM_STAT_INCLUDED)  /*ibm@39757*/
#define IBM_STAT_INCLUDED        /*ibm@39757*/

#include_next <sys/stat.h>                          /*ibm@39429*/

#if defined(IBM_ATOE)

	#if !defined(IBM_ATOE_SYS_STAT)
		#define IBM_ATOE_SYS_STAT

		#ifdef __cplusplus
            extern "C" {
		#endif

        int atoe_mkdir (const char*, mode_t);
        int atoe_remove (const char*);                     /*ibm@4838*/
        int atoe_chmod (const char*, mode_t);             /*ibm@11596*/
        int atoe_lstat(const char *pathname, struct stat *sbuf);
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

