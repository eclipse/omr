/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#ifndef omrmutex_h
#define omrmutex_h

/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */

typedef CRITICAL_SECTION MUTEX;

/* MUTEX_INIT */

#define MUTEX_INIT(mutex) (InitializeCriticalSection(&(mutex)), 1)

/* MUTEX_DESTROY */

#define MUTEX_DESTROY(mutex) DeleteCriticalSection(&(mutex))

/* MUTEX_ENTER */

#define MUTEX_ENTER(mutex) EnterCriticalSection(&(mutex))

/*
 *  MUTEX_TRY_ENTER
 *  returns 0 on success
 *  Beware: you may not have support for TryEnterCriticalSection
 */
#define MUTEX_TRY_ENTER(mutex) (!(TryEnterCriticalSection(&(mutex))))

/* MUTEX_EXIT */

#define MUTEX_EXIT(mutex) LeaveCriticalSection(&(mutex))




#endif /* omrmutex_h */

