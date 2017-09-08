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

#ifndef omrvmem_h
#define omrvmem_h

/* Constants for page size used by omrvmem.c (in increasing order) */
#define FOUR_K			((uintptr_t) 4*1024)
#define SIXTY_FOUR_K 	((uintptr_t) 64*1024)
#define ONE_M 			((uintptr_t) 1*1024*1024)
#define SIXTEEN_M 		((uintptr_t) 16*1024*1024)
#define TWO_G 			((uintptr_t) 2*1024*1024*1024)
#define ZOS_REAL_FRAME_SIZE_SHIFT 12 /* real frame size is 2^12=4096K */

#define ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(value, pageSize)	Assert_PRT_true(0 == ((uintptr_t)(value) % (uintptr_t)(pageSize)))

#endif /* omrvmem_h */
