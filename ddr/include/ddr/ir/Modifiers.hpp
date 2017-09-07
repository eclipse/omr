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

#ifndef MODIFIERS_HPP
#define MODIFIERS_HPP

#include "ddr/std/string.hpp"
#include <vector>

using std::string;
using std::vector;

class Modifiers
{
public:
	vector<size_t> _arrayLengths;
	int _modifierFlags;
	size_t _offset;
	size_t _pointerCount;
	size_t _referenceCount;

	static const int MODIFIER_FLAGS = 31;
	static const int NO_MOD = 0;
	static const int CONST_TYPE = 1;
	static const int VOLATILE_TYPE = 2;
	static const int UNALIGNED_TYPE = 4;
	static const int RESTRICT_TYPE = 8;
	static const int SHARED_TYPE = 16;
	static const string MODIFIER_NAMES[];

	Modifiers();
	~Modifiers();

	string getPointerType();
	string getModifierNames();
	void addArrayDimension(size_t length);
	bool isArray();
	size_t getArrayLength(size_t i);
	size_t getArrayDimensions();
	size_t getSize(size_t typeSize);
	bool operator==(Modifiers const& type) const;
};

#endif /* MODIFIERS_HPP */
