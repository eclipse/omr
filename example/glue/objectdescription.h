/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#if !defined(OBJECTDESCRIPTION_H_)
#define OBJECTDESCRIPTION_H_

#include "omrcomp.h"
#include "omr.h"

/**
 * Object token definitions to be used by OMR components.
 */
typedef uintptr_t* languageobjectptr_t;
typedef uintptr_t* omrobjectptr_t;
typedef uintptr_t* omrarrayptr_t;

#if defined (OMR_GC_COMPRESSED_POINTERS)
typedef uint32_t fomrobject_t;
typedef uint32_t fomrarray_t;
#else
typedef uintptr_t fomrobject_t;
typedef uintptr_t fomrarray_t;
#endif

#endif /* OBJECTDESCRIPTION_H_ */
