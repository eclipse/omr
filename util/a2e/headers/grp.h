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
