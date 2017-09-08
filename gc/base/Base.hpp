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

#if !defined(BASE_HPP_)
#define BASE_HPP_

/* NULL hack for now until cleanup */
#ifndef NULL
#ifdef __cplusplus
#define NULL (0)
#else
#define NULL ((void *)0)
#endif
#endif

#include <stdlib.h>
/* We include stddef.h for the PALM, because this is where size_t is defined for that platform. */
#include <stddef.h>

/**
 * Base Class
 * @ingroup GC_Base_Core
 */
class MM_Base
{
private:
protected:
public:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; }
	void operator delete(void *, void *) {}

	/**
	 * Create a Base object.
	 */
	MM_Base() {}
};

#endif /* BASE_HPP_ */
