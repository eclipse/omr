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

#include "ddr/ir/ClassUDT.hpp"

ClassUDT::ClassUDT(size_t size, bool isClass, unsigned int lineNumber)
	: ClassType(size, lineNumber), _superClass(NULL), _isClass(isClass)
{
}

ClassUDT::~ClassUDT() {};

string
ClassUDT::getSymbolKindName()
{
	return _isClass ? "class" : "struct";
}

DDR_RC
ClassUDT::acceptVisitor(TypeVisitor const &visitor)
{
	return visitor.visitType(this);
}

bool
ClassUDT::operator==(Type const & rhs) const
{
	return rhs.compareToClass(*this);
}

bool
ClassUDT::compareToClass(ClassUDT const &other) const
{
	return compareToClasstype(other)
		&& (*_superClass == *other._superClass);
}
