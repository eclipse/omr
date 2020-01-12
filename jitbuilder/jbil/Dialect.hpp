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

#ifndef DIALECT_INCL
#define DIALECT_INCL

#include <stdint.h>

typedef int64_t Dialect;

#define DIALECT(name) Dialect_##name

const int64_t DIALECT(native) = 1;
#define DefineDialect(name,builton) \
	const int64_t DIALECT(name)=2*DIALECT(builton); \
	static_assert((DIALECT(builton) != ((uint64_t)1) << 63), "ran out of dialect identifiers")

// Let's define some dialects (doesn't mean much yet)

// native == native code

// codegen == instructions
DefineDialect(codegen, native);

// tril == compiler IL
DefineDialect(tril, codegen);

// jbil_ll == low level JitBuilder: only low level control flow operations like IfCmp* and Switch
DefineDialect(jbil_ll, tril);

// jbil == current JitBuilder API, including higher level operations like ForLoop, IfThenElse
DefineDialect(jbil, jbil_ll);

//
// Add any User Dialect definitions here:
// BEGIN {

// e.g. DefineDialect(UserDialect, jbil)

// } END
// Stop User Dialect definitions here
//

#endif // !defined(DIALECT_INCL)
