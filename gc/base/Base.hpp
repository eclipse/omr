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
 ******************************************************************************/

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
