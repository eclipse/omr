/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#if !defined(OBJECTSCANNERSTATE_HPP_)
#define OBJECTSCANNERSTATE_HPP_

#include "MixedObjectScanner.hpp"

/**
 * This union is not intended for runtime usage -- it is required only to determine the maximal size of
 * GC_ObjectScanner subclasses used in the client language. An analogous definition
 * of GC_ObjectScannerState must be specified in the glue layer for the client language and that definition
 * will be used when OMR is built for the client language.
 */
typedef union GC_ObjectScannerState
{
	uint8_t scanner[sizeof(GC_MixedObjectScanner)];
} GC_ObjectScannerState;

#endif /* OBJECTSCANNERSTATE_HPP_ */
