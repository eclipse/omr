/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2007, 2016
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


#ifndef UNIXTHREADATTR_H_
#define UNIXTHREADATTR_H_

#include <pthread.h>
#include "common/omrthreadattr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* use the same levels of indirection as omrthread_attr_t definition,
 * for consistency
 */
typedef struct unixthread_attr {
	omrthread_attr hdr; /* must be first member */
	pthread_attr_t pattr;
} unixthread_attr;

typedef struct unixthread_attr *unixthread_attr_t;

#ifdef __cplusplus
}
#endif

#endif /* UNIXTHREADATTR_H_ */
