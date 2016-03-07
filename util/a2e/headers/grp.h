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
 * Replace the system header file "grp.h" so that we can redefine
 * the functions that take/produce character strings
 * with our own ATOE functions.
 *
 * Replacing the system header file like this means that we
 * do not need to touch the cross platform source code to
 * redefine the functions. The compiler will find
 * this header file in preference to the system one.
 * ===========================================================================
 */
#if __TARGET_LIB__ == 0X22080000
#include <//'PP.ADLE370.OS39028.SCEEH.H(grp)'>
#else
#include "prefixpath.h"
#include PREFIXPATH(grp.h)
#endif

#if !defined(IBM_ATOE_GRP)
   #define IBM_ATOE_GRP

   /******************************************************************/
   /*  Define prototypes for replacement functions.                  */
   /******************************************************************/

   #ifdef __cplusplus
      extern "C" {
   #endif

   #if defined(IBM_ATOE)
      struct group *atoe_getgrgid(gid_t);
   #endif

   #ifdef __cplusplus
      }
   #endif

   /******************************************************************/
   /*  Undefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)
      #undef getgrgid
   #endif

   /******************************************************************/
   /*  Redefine the functions                                        */
   /******************************************************************/

   #if defined(IBM_ATOE)
      #define getgrgid        atoe_getgrgid
   #endif
#endif

/* END OF FILE */
