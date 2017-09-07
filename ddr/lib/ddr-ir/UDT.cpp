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

#include "ddr/config.hpp"
#include "ddr/ir/UDT.hpp"
#include "ddr/ir/NamespaceUDT.hpp"

UDT::UDT(size_t size, unsigned int lineNumber)
	: Type(size), _outerNamespace(NULL), _lineNumber(lineNumber)
{
}

UDT::~UDT()
{
}

string
UDT::getFullName()
{
	return ((NULL == _outerNamespace) ? "" : _outerNamespace->getFullName() + "::") + _name;
}


void
UDT::checkDuplicate(Symbol_IR *ir)
{
	if (!this->isAnonymousType()) {
		string fullName = getFullName();
		if (ir->_fullTypeNames.end() != ir->_fullTypeNames.find(fullName)) {
			this->_isDuplicate = true;
		} else {
			ir->_fullTypeNames.insert(fullName);
		}
	}
}

NamespaceUDT *
UDT::getNamespace()
{
	return _outerNamespace;
}

bool
UDT::operator==(Type const & rhs) const
{
	return rhs.compareToUDT(*this);
}

bool
UDT::compareToUDT(UDT const &other) const
{
	return compareToType(other)
		&& (*_outerNamespace == *other._outerNamespace);
}
