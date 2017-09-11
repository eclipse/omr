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

#include "ddr/ir/ClassType.hpp"

#include "ddr/config.hpp"

ClassType::ClassType(size_t size, unsigned int lineNumber)
	: NamespaceUDT(lineNumber)
{
	_sizeOf = size;
}

ClassType::~ClassType()
{
	/*enum members may be added to a classType in the case of anonymous enums */
	for (size_t i = 0; i < _enumMembers.size(); i++) {
		if (NULL == _enumMembers[i]) {
			ERRMSG("Null member, cannot free");
		} else {
			delete(_enumMembers[i]);
		}
	}
	_enumMembers.clear();

	for (size_t i = 0; i < _fieldMembers.size(); i++) {
		if (NULL == _fieldMembers[i]) {
			ERRMSG("Null member, cannot free");
		} else {
			delete(_fieldMembers[i]);
		}
	}
	_fieldMembers.clear();
}

bool
ClassType::isAnonymousType()
{
	return _name.empty();
}

void
ClassType::computeFieldOffsets()
{
	NamespaceUDT::computeFieldOffsets();
	
	/* For classes, structs, and unions, find the field offsets. */
	size_t offset = 0;
	for (size_t i = 0; i < _fieldMembers.size(); i += 1) {
		Field *field = (_fieldMembers[i]);

		if (!field->_isStatic) {
			/* Use the field size to compute offsets. */
			field->_offset = offset;
			offset += field->_sizeOf;
		}
	}
	/* If class has no total size, set it now. */
	if (0 == _sizeOf) {
		_sizeOf = offset;
	}
}

void
ClassType::renameFieldsAndMacros(FieldOverride fieldOverride, Type *replacementType)
{
	/* Iterate the fields of structures with matching names. */
	for (vector<Field *>::iterator it = _fieldMembers.begin(); it != _fieldMembers.end(); it += 1) {
		/* Once a matching structure and field name are found, apply the override. */
		if ((*it)->_name == fieldOverride.fieldName) {
			if (fieldOverride.isTypeOverride) {
				if (0 == replacementType->_sizeOf) {
					replacementType->_sizeOf = (*it)->_fieldType->_sizeOf;
				}
				(*it)->_fieldType = replacementType;
			} else {
				(*it)->_name = fieldOverride.overrideName;
			}
		}
	}
}

bool
ClassType::operator==(Type const & rhs) const
{
	return rhs.compareToClasstype(*this);
}

bool
ClassType::compareToClasstype(ClassType const &other) const
{
	bool enumMembersEqual = _enumMembers.size() == other._enumMembers.size();
	vector<EnumMember *>::const_iterator it2 = other._enumMembers.begin();
	for (vector<EnumMember *>::const_iterator it = _enumMembers.begin();
		it != _enumMembers.end() && it2 != other._enumMembers.end() && enumMembersEqual;
		++ it, ++ it2) {
		enumMembersEqual = ((*it)->_name == (*it2)->_name) && ((*it)->_value == (*it2)->_value);
	}

	bool fieldMembersEqual = _fieldMembers.size() == other._fieldMembers.size();
	vector<Field *>::const_iterator it3 = other._fieldMembers.begin();
	for (vector<Field *>::const_iterator it = _fieldMembers.begin();
		it != _fieldMembers.end() && it3 != other._fieldMembers.end() && fieldMembersEqual;
		++ it, ++ it3) {
		fieldMembersEqual = ((*it)->_name == (*it3)->_name)
			&& (NULL == (*it)->_fieldType || NULL == (*it3)->_fieldType || (*it)->_fieldType->_name == (*it3)->_fieldType->_name);
	}
	return compareToNamespace(other)
		&& enumMembersEqual
		&& fieldMembersEqual;
}
