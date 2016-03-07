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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
