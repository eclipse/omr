/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef THREADLOCAL_INCL
#define THREADLOCAL_INCL

#if defined(OMRZTPF) && !defined(SUPPORTS_THREAD_LOCAL)
/*
 * For this include file, ThreadLocal.h, and the z/TPF OS platform,
 * say that tls is supported. The z/TPF platform should use
 * pthread _key_t variables and pthread functions for
 * the tls* functions that are defined here.
 */
#define SUPPORTS_THREAD_LOCAL
#endif

#if defined(SUPPORTS_THREAD_LOCAL)

#if defined(WINDOWS) || (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || defined(AIXPPC)
 #if defined(WINDOWS)
  #include "windows.h"
  #define tlsDeclare(type, variable) extern DWORD variable
  #define tlsDefine(type, variable) DWORD variable = TLS_OUT_OF_INDEXES
  #define tlsAlloc(variable) if ((variable) == TLS_OUT_OF_INDEXES) { (variable) = TlsAlloc();}
  #define tlsFree(variable)  (TlsFree(variable))
  #define tlsSet(variable, value) TlsSetValue((variable), value)
  #define tlsGet(variable, type) ((type)TlsGetValue(variable))
 #else /* if defined(LINUX) || defined(AIXPPC) */
  #define tlsDeclare(type, variable) extern __thread type variable
  #define tlsDefine(type, variable) __thread type variable = NULL
  #define tlsAlloc(variable) // not required on win, linux or AIX
  #define tlsFree(variable)  // not required on win, linux or AIX
  #define tlsSet(variable, value) variable = value
  #define tlsGet(variable, type) (variable)
 #endif
#else /* !(defined(WINDOWS) || (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || defined(AIXPPC)) */  /* Mainly for defined(OMRZTPF) or defined(J9ZOS390) */
 #include <pthread.h>

 #define tlsDeclare(type, variable) extern pthread_key_t variable
 #define tlsDefine(type, variable) pthread_key_t variable
 #define tlsAlloc(variable) pthread_key_create(&variable, NULL)   // allocates a new key, which will have initial value NULL for all threads
 #define tlsFree(variable) pthread_key_delete(variable)
 #define tlsSet(variable, value) pthread_setspecific(variable, value)
 #if defined(J9ZOS390)
  #define tlsGet(variable, type) ((type) pthread_getspecific_d8_np(variable))
 #else
  #define tlsGet(variable, type) ((type) pthread_getspecific(variable))
 #endif
#endif

#else /* !defined(SUPPORTS_THREAD_LOCAL) */
 #define tlsDeclare(type, variable) extern type variable
 #define tlsDefine(type, variable) type variable = NULL
 #define tlsAlloc(variable)
 #define tlsFree(variable)
 #define tlsSet(variable, value) variable = value
 #define tlsGet(variable, type) (variable)
#endif

#endif /* THREADLOCAL_INCL */
