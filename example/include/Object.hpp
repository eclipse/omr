/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH
 *Assembly-exception
 *******************************************************************************/

#if !defined(OBJECT_HPP_)
#define OBJECT_HPP_

#include "omrcfg.h"

#include <cstdlib>
#include <stdint.h>

typedef uint8_t ObjectFlags;

#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
typedef uint32_t RawObjectHeader;
typedef uint32_t ObjectSize;
#else
typedef uintptr_t RawObjectHeader;
typedef uintptr_t ObjectSize;
#endif

/**
 * A header containing basic information about an object. Contains the object's size and an 8 bit object flag.
 * The size and flags are masked together into a single fomrobjectptr_t.
 */
class ObjectHeader
{
public:

	ObjectHeader() {}

	explicit ObjectHeader(RawObjectHeader value) : _value(value) {}

	explicit ObjectHeader(ObjectSize sizeInBytes, ObjectFlags flags) { assign(sizeInBytes, flags); }

	ObjectSize sizeInBytes() const { return _value >> SIZE_SHIFT; }

	void sizeInBytes(ObjectSize value) { assign(value, flags()); }

	ObjectFlags flags() const { return (ObjectFlags)_value; }

	void flags(ObjectFlags value) { assign(sizeInBytes(), value); }

	void assign(ObjectSize sizeInBytes, ObjectFlags flags) { _value = (sizeInBytes << SIZE_SHIFT) | flags; }

	RawObjectHeader raw() const { return _value; }

	void raw(RawObjectHeader raw) { _value = raw; }

private:
	static const size_t SIZE_SHIFT = sizeof(ObjectFlags)*8;

	RawObjectHeader _value;
};

class Object
{
public:
	static ObjectSize allocSize(ObjectSize nslots) {
		return ObjectSize(sizeof(ObjectHeader) + sizeof(fomrobject_t) * nslots);
	}

	explicit Object(ObjectSize sizeInBytes, ObjectFlags flags = 0) : header(sizeInBytes, flags) {}

	size_t sizeOfSlotsInBytes() const { return header.sizeInBytes() - sizeof(ObjectHeader); }

	size_t slotCount() const { return sizeOfSlotsInBytes() / sizeof(Slot); }

	Slot* slots() { return (Slot*)(this + 1); }

	const Slot* slots() const { return (Slot*)(this + 1); }

	Slot* begin() { return slots(); }

	const Slot* begin() const { return slots(); }

	Slot* end() { return begin() + slotCount(); }

	const Slot* end() const { return begin() + slotCount(); }

	const Slot* cbegin() const { return begin(); }

	const Slot* cend() const { return end(); }

	ObjectHeader header;
};

#endif /* OBJECT_HPP_ */
