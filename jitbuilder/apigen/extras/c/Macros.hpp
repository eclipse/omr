/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
 *  * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#ifndef CPP_BINDING_RUNTIME_INCL
#define CPP_BINDING_RUNTIME_INCL

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

#define GETIMPL(x, y) getImpl_##x(y)

#define ARG_SETUP(baretype, ptrImpl, byVarArg, parmArg)                 \
   TR::baretype *ptrImpl = NULL;                                        \
   TR::baretype **byVarArg = NULL;                                      \
   if (parmArg)                                                         \
      {                                                                 \
      byVarArg = &ptrImpl;                                              \
      if (*parmArg)                                                     \
         ptrImpl = reinterpret_cast<TR::baretype *>(GETIMPL(baretype, parmArg)); \
      }

#define ARG_RETURN(baretype, ptrImpl, parmArg)         \
   if (parmArg)                                        \
      {                                                \
      GET_CLIENT_OBJECT(clientObj, baretype, ptrImpl); \
      *parmArg = clientObj;                            \
      }

#define ARRAY_ARG_SETUP(baretype, arraySize, arrayImpl, parmArg, getImplType)               \
   TR::baretype **arrayImpl = new TR::baretype *[arraySize];                   \
   for (uint32_t i=0;i < arraySize;i++)                                        \
      {                                                                        \
      if (parmArg[i] != NULL)                                                  \
         arrayImpl[i] = reinterpret_cast<TR::baretype *>( GETIMPL(getImplType, parmArg[i]) ); \
      else                                                                     \
         arrayImpl[i] = NULL;                                                  \
      }

#define ARRAY_ARG_RETURN(baretype, arraySize, arrayImpl, parmArg) \
   for (uint32_t i=0;i < arraySize;i++)                           \
      {                                                           \
      if (arrayImpl[i] != NULL)                                   \
         {                                                        \
         GET_CLIENT_OBJECT(clientObj, baretype, arrayImpl[i])     \
         parmArg[i] = clientObj;                                  \
         }                                                        \
      else                                                        \
         parmArg[i] = NULL;                                       \
      }


// This macro defines clientObj in the scope where macro is used
#define GET_CLIENT_OBJECT(clientObj, baretype, implObj)            \
   baretype *clientObj = NULL;                                     \
   if (implObj != NULL)                                            \
      {                                                            \
      clientObj = reinterpret_cast<baretype *>(implObj->client()); \
      }

#endif // defined(CPP_BINDING_RUNTIME_INCL)
