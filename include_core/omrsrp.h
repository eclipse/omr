/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#ifndef OMRSRP_H_INCLUDED
#define OMRSRP_H_INCLUDED

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/* OMRTODO
 * The #ifndef checks are exploited by j9dbgext.h to override the SRP macros with out-of-process variants.
 * j9dbgext.h is used by trdbgext.c.
 */
typedef int32_t J9SRP;
#ifndef NNSRP_GET
#define NNSRP_GET(field, type) ((type) (((uint8_t *) &(field)) + (J9SRP)(field)))
#endif
#ifndef SRP_GET
#define SRP_GET(field, type) ((type) ((field) ? NNSRP_GET(field, type) : NULL))
#endif
#define NNSRP_SET(field, value) (field) = (J9SRP) (((uint8_t *) (value)) - (uint8_t *) &(field))
#define SRP_SET(field, value) (field) = (J9SRP) ((value) ? (((uint8_t *) (value)) - (uint8_t *) &(field)) : 0)
#define SRP_SET_TO_NULL(field) (field) = 0
#ifndef SRP_PTR_GET
#define SRP_PTR_GET(ptr, type) SRP_GET(*((J9SRP *) (ptr)), type)
#endif
#define SRP_PTR_SET(ptr, value) SRP_SET(*((J9SRP *) (ptr)), (value))
#define SRP_PTR_SET_TO_NULL(ptr) SRP_SET_TO_NULL(*((J9SRP *) (ptr)))
#define NNSRP_PTR_GET(ptr, type) NNSRP_GET(*((J9SRP *) (ptr)), type)
#define NNSRP_PTR_SET(ptr, value) NNSRP_SET(*((J9SRP *) (ptr)), (value))


typedef intptr_t J9WSRP;
#ifndef NNWSRP_GET
#define NNWSRP_GET(field, type) ((type) (((uint8_t *) &(field)) + (J9WSRP)(field)))
#endif
#ifndef WSRP_GET
#define WSRP_GET(field, type) ((type) ((field) ? NNWSRP_GET(field, type) : NULL))
#endif
#define NNWSRP_SET(field, value) (field) = (J9WSRP) (((uint8_t *) (value)) - (uint8_t *) &(field))
#define WSRP_SET(field, value) (field) = (J9WSRP) ((value) ? (((uint8_t *) (value)) - (uint8_t *) &(field)) : 0)
#define WSRP_SET_TO_NULL(field) (field) = 0
#ifndef WSRP_PTR_GET
#define WSRP_PTR_GET(ptr, type) WSRP_GET(*((J9WSRP *) (ptr)), type)
#endif
#define WSRP_PTR_SET(ptr, value) WSRP_SET(*((J9WSRP *) (ptr)), (value))
#define WSRP_PTR_SET_TO_NULL(ptr) SRP_SET_TO_NULL(*((J9WSRP *) (ptr)))
#define NNWSRP_PTR_GET(ptr, type) NNWSRP_GET(*((J9WSRP *) (ptr)), type)
#define NNWSRP_PTR_SET(ptr, value) NNWSRP_SET(*((J9WSRP *) (ptr)), (value))

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* OMRSRP_H_INCLUDED */
