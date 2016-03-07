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
#include "prefixpath.h"
#include PREFIXPATH(unistd.h)                                   /*ibm@28725*/
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
