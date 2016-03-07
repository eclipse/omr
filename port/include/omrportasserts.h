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

#ifndef omrportasserts_h
#define omrportasserts_h

#include "ut_omrport.h"
#include <assert.h>

#define Assert_PRT_true_wrapper(arg) \
	do { \
		if (!(arg)) { \
			Assert_PRT_true(FALSE && (arg)); \
			assert(FALSE && (arg)); \
		} \
	} while(0)

#define Assert_PRT_ShouldNeverHappen_wrapper() \
	do { \
		Trc_PRT_Assert_ShouldNeverHappen(); \
		assert(FALSE); \
	} while (0)

#endif /* omrportasserts_h */
