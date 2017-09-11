/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef TEST_STACKATLASPOD_INCL
#define TEST_STACKATLASPOD_INCL

#include <stdint.h>

/*
 * This structure describes the shape of the encoded stack atlas information.
 * It must be a C++ POD object and follow all the rules behind POD formation.
 * Because its fields may be extracted by a runtime system, its exact layout
 * shape MUST be preserved.
 *
 * DO NOT subclass this struct.  A project may replace it by including an
 * identically named header file earlier in the IPATH.
 */

namespace OMR
{

struct StackAtlasPOD
   {
   uint16_t numberOfMaps;
   uint16_t bytesPerStackMap;
   int32_t frameObjectParmOffset;
   int32_t localBaseOffset;
   };

}

#endif // !defined(TEST_STACKATLASPOD_INCL)
