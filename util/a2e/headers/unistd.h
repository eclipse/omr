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
 * Replace the system header file "unistd.h" so that we can redefine
 * the i/o functions that take/produce character strings
 * with our own ATOE functions.
 *
 * The compiler will find this header file in preference to the system one.
 * ===========================================================================
 */

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.H(unistd)'>                  /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include_next <unistd.h>                                           /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if !defined(IBM_ATOE_UNISTD)
   #define IBM_ATOE_UNISTD

   /******************************************************************/
   /*  Define prototypes for replacement functions.                  */
   /******************************************************************/

   #ifdef __cplusplus
      extern "C" {
   #endif

   #if defined(IBM_ATOE)
      int        atoe_access     (const char*, int);
      char *     atoe_getcwd     (char*, size_t);
      extern int atoe_gethostname(char *, int);
      char *     atoe_getlogin   (void);
      int        atoe_unlink     (const char*);
      int        atoe_chdir (const char*);                         /*ibm@28845*/
      int        atoe_chown      (const char*, uid_t, gid_t);
      int        atoe_rmdir      (const char *pathname);
   #endif

   #ifdef __cplusplus
      }
   #endif

   /******************************************************************/
   /*  Undefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)
      #undef access
      #undef getcwd
      #undef gethostname
      #undef getlogin
      #undef unlink
      #undef chown
   #endif

   /******************************************************************/
   /*  Redefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)
      #define access(a,b)     atoe_access(a,b)
      #define getcwd          atoe_getcwd
      #define gethostname     atoe_gethostname
      #define getlogin        atoe_getlogin
      #define unlink          atoe_unlink
      #define chdir           atoe_chdir                           /*ibm@28845*/
      #define chown           atoe_chown
      #define rmdir           atoe_rmdir                           

   #endif
#endif

/* END OF FILE */
