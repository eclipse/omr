/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#if !defined(OMR_OM_OBJECTITERATOR_HPP_)
#define OMR_OM_OBJECTITERATOR_HPP_

#include "SlotObject.hpp"
#include <cassert>

namespace OMR {
namespace Om {

class DummyObjectIterator {
public:
	GC_SlotObject* nextSlot() {
		assert(0);
		return NULL;
	}

	void restore(int32_t index) { assert(0); }

	DummyObjectIterator(OMR_VM* omrVM, omrobjectptr_t objectPtr) { assert(0); }
};

} // namespace Om
} // namespace OMR

using GC_ObjectIterator = OMR::Om::DummyObjectIterator;

#endif /* OMR_OM_OBJECTITERATOR_HPP_ */
