/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#if !defined(MODRONBASE_H_)
#define MODRONBASE_H_

/*
 * @ddr_namespace: default
 */

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

