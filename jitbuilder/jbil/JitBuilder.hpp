/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef JITBUILDER_INCL
#define JITBUILDER_INCL

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "Operation.hpp"
#include "Symbol.hpp"
#include "Transformer.hpp"
#include "TextWriter.hpp"
#include "Type.hpp"
#include "TypeDictionary.hpp"
#include "Value.hpp"

bool initializeJit();
bool constructFunctionBuilder(OMR::JitBuilder::FunctionBuilder * fb);
int32_t compileFunctionBuilder(OMR::JitBuilder::FunctionBuilder * fb, void ** entry);
void shutdownJit();

// Legacy definitions: may need to make IlBuilder a class....
//#define IlBuilder Builder
//#define MethodBuilder FunctionBuilder
//#define compileMethodBuilder(fb,e) compileFunctionBuilder(fb, e)

// Global user definitions
// BEGIN {
//

//
// } END
// Global user definitions

#endif // defined(JITBUILDER_INCL)
