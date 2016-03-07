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

/*
 * Do NOT dispense with this wrapper without relocating FD_SETSIZE below
 */
#ifndef FD_SETSIZE                                               /*ibm@55297.1*/
#define FD_SETSIZE  65535                                        /*ibm@55297.1*/
#endif                                                           /*ibm@55297.1*/

#if __TARGET_LIB__ == 0X22080000                                   /*ibm@28725*/
#include <//'PP.ADLE370.OS39028.SCEEH.SYS.H(time)'>                /*ibm@28725*/
#else                                                              /*ibm@28725*/
#include "prefixpath.h"
#include PREFIXPATH(sys/time.h)                                 /*ibm@28725*/
#endif                                                             /*ibm@28725*/

#if !defined(IBM_ATOE_SYS_TIME)
   #define IBM_ATOE_SYS_TIME

   /******************************************************************/
   /*  Define prototypes for replacement functions.                  */
   /******************************************************************/

   #ifdef __cplusplus
      extern "C" {
   #endif

   #if defined(IBM_ATOE)

      int atoe_utimes(const char *, const struct timeval *);

   #endif

   #ifdef __cplusplus
      }
   #endif

   /******************************************************************/
   /*  Undefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)

      #undef  utimes

   #endif

   /******************************************************************/
   /*  Redefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)

      #define utimes   atoe_utimes

   #endif
#endif

/* END OF FILE */

