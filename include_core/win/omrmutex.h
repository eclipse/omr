/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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

