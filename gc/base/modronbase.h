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

#if !defined(MODRONBASE_H_)
#define MODRONBASE_H_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrgcconsts.h"

/* TODO Imports from j9consts.h */

/* These are duplicated in builder J9VMConstantValue.st
 * but cannot be removed from there as the JIT needs these in j9consts.h:
 * 		J9_GC_MULTI_SLOT_HOLE
 * 		J9_GC_SINGLE_SLOT_HOLE
 * 	tr.source\frontend\j9\...
 */

#ifndef J9_GC_OBJ_HEAP_HOLE
#define J9_GC_OBJ_HEAP_HOLE 0x1
#endif
#ifndef J9_GC_OBJ_HEAP_HOLE_MASK
#define J9_GC_OBJ_HEAP_HOLE_MASK 0x3
#endif
#ifndef J9_GC_MULTI_SLOT_HOLE
#define J9_GC_MULTI_SLOT_HOLE 0x1
#endif
#ifndef J9_GC_SINGLE_SLOT_HOLE
#define J9_GC_SINGLE_SLOT_HOLE 0x3
#endif

#endif /* MODRONBASE_H_ */

