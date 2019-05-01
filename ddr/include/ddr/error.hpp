/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

 #if !defined(DDR_ERROR_HPP)
 #define DDR_ERROR_HPP

#include "ddr/config.hpp"

#include <stdio.h>

#define ERRMSG(...) do { \
	fprintf(stderr, "Error: %s:%d %s - ", __FILE__, __LINE__, __FUNCTION__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
} while (0)

#if 0
/* Enable debug printf output */
#define DEBUGPRINTF_ON
#endif

#if defined(DEBUGPRINTF_ON)
#define DEBUGPRINTF(...) do { \
	printf("DEBUG: %s:%d %s - ", __FILE__, __LINE__, __FUNCTION__); \
	printf(__VA_ARGS__); \
	printf("\n"); \
	fflush(stdout); \
} while (0)
#else
#define DEBUGPRINTF(...) do {} while(0)
#endif /* defined(DEBUG) */

enum DDR_RC {
	DDR_RC_OK,
	DDR_RC_ERROR
};

#endif /* DDR_ERROR_HPP */
