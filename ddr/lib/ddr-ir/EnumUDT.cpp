/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#include "ddr/ir/EnumUDT.hpp"

#include "ddr/ir/EnumMember.hpp"

EnumUDT::EnumUDT(unsigned int lineNumber)
	: UDT(4, lineNumber)
	, _enumMembers()
{
}

EnumUDT::~EnumUDT()
{
	for (vector<EnumMember *>::iterator it = _enumMembers.begin(); it != _enumMembers.end(); ++it) {
		delete *it;
	}
	_enumMembers.clear();
}

const string &
EnumUDT::getSymbolKindName() const
{
	static const string enumKind("enum");

	return enumKind;
}

DDR_RC
EnumUDT::acceptVisitor(const TypeVisitor &visitor)
{
	return visitor.visitEnum(this);
}

bool
EnumUDT::operator==(const Type & rhs) const
{
	return rhs.compareToEnum(*this);
}

bool
EnumUDT::compareToEnum(const EnumUDT &other) const
{
	bool enumMembersEqual = _enumMembers.size() == other._enumMembers.size();
	vector<EnumMember *>::const_iterator it2 = other._enumMembers.begin();
	for (vector<EnumMember *>::const_iterator it = _enumMembers.begin();
		it != _enumMembers.end() && it2 != other._enumMembers.end() && enumMembersEqual;
		++it, ++it2) {
		enumMembersEqual = ((*it)->_name == (*it2)->_name) && ((*it)->_value == (*it2)->_value);
	}
	return compareToUDT(other)
		&& enumMembersEqual;
}
