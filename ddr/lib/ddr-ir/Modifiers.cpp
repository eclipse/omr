/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#include <stdlib.h>

#include "ddr/config.hpp"
#include "ddr/ir/Modifiers.hpp"

using std::string;
using std::vector;

Modifiers::Modifiers()
	: _modifierFlags(NO_MOD),
	  _offset(0),
	  _pointerCount(0),
	  _referenceCount(0)
{
}

Modifiers::~Modifiers() {}

const string Modifiers::MODIFIER_NAMES[] = {"const", "volatile", "unaligned", "restrict", "shared"};

string
Modifiers::getModifierNames()
{
	string modifierString = "";
	for (int i = 1, index = 0; i < MODIFIER_FLAGS; i *= 2, index ++) {
		if (0 != (_modifierFlags & i)) {
			modifierString += MODIFIER_NAMES[index] + " ";
		}
	}
	return modifierString;
}

string
Modifiers::getPointerType()
{
	string s = string("");

	for (size_t i = 0; i < _pointerCount; i++) {
		s += "*";
	}

	for (size_t i = 0; i < _referenceCount; i++) {
		s += "&";
	}

	if (isArray()) {
		for (size_t i = 0; i < getArrayDimensions(); i++) {
			s += "[]";
		}
	}

	return s;
}

void
Modifiers::addArrayDimension(size_t length)
{
	_arrayLengths.push_back(length);
}

bool
Modifiers::isArray()
{
	return _arrayLengths.size() > 0;
}

size_t
Modifiers::getArrayLength(size_t i)
{
	if (i >= _arrayLengths.size()) {
		return 0;
	}
	return _arrayLengths[i];
}

size_t
Modifiers::getArrayDimensions()
{
	return _arrayLengths.size();
}

size_t
Modifiers::getSize(size_t typeSize)
{
	size_t sizeOf = 0;
	if (_pointerCount > 0) {
		sizeOf = sizeof(void *);
	} else {
		sizeOf = typeSize;
	}
	if (isArray()) {
		size_t arrayElements = 0;
		for (vector<size_t>::iterator it = _arrayLengths.begin(); it != _arrayLengths.end(); it += 1) {
			arrayElements += *it;
		}
		sizeOf *= arrayElements;
	}
	return sizeOf;
}

bool
Modifiers::operator==(Modifiers const& type) const
{
	bool arrayLengthsEqual = true;
	for (size_t i = 0; i < _arrayLengths.size(); i += 1) {
		if (_arrayLengths[i] != type._arrayLengths[i]) {
			arrayLengthsEqual = false;
			break;
		}
	}

	return (_modifierFlags == type._modifierFlags)
		&& (_referenceCount == type._referenceCount)
		&& (_pointerCount == type._pointerCount)
		&& arrayLengthsEqual;
}
