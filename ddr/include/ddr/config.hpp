/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "omrcfg.h"

/* Enable standard limit macros on z/OS. */

#if defined(J9ZOS390)
/* We need to define these for the limit macros to get defined in z/OS */
#define _ISOC99_SOURCE 1
#define __STDC_LIMIT_MACROS 1
#endif /* defined(J9ZOS390) */

/* C++ TR1 support */

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#define OMR_HAVE_TR1 1
#else
#define OMR_HAVE_CXX11 1
#endif /* !defined(AIXPPC) && !defined(J9ZOS390) */

/* Why is this disabled? */

#if defined(NDEBUG)
#undef NDEBUG
#endif

#endif /* CONFIG_HPP */
