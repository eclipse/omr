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

#include <pthread.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t MUTEX;


/* MUTEX_INIT */

#define MUTEX_INIT(mutex) (pthread_mutex_init(&(mutex), NULL) == 0)

/* MUTEX_DESTROY */

#define MUTEX_DESTROY(mutex) pthread_mutex_destroy(&(mutex))

/* MUTEX_ENTER */

#define MUTEX_ENTER(mutex) pthread_mutex_lock(&(mutex))

/*
 *  MUTEX_TRY_ENTER
 *  returns 0 on success
 */

#define MUTEX_TRY_ENTER(mutex) pthread_mutex_trylock(&(mutex))

/* MUTEX_EXIT */

#define MUTEX_EXIT(mutex) pthread_mutex_unlock(&(mutex))

#ifdef __cplusplus
}
#endif


#endif /* omrmutex_h */

