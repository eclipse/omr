/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *   Multiple authors (IBM Corp.) - refactoring and modifications for z/TPF platform
 *******************************************************************************/

#ifndef OMRSYSINFO_HELPERS_H_
#define OMRSYSINFO_HELPERS_H_

#include "omrport.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * Retrieve z/Architecture facility bits.
 *
 * @param [in]  lastDoubleWord   Size of the bits array in number of uint64_t, minus 1.
 * @param [out] bits             Caller-supplied uint64_t array that gets populated with the facility bits.
 *
 * @return The index of the last valid uint64_t in the bits array.
 */
extern int getstfle(int lastDoubleWord, uint64_t *bits);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* OMRSYSINFO_HELPERS_H_ */
