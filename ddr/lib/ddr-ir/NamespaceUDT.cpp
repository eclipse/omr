/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#include "ddr/ir/NamespaceUDT.hpp"

#include "ddr/ir/Symbol_IR.hpp"

NamespaceUDT::NamespaceUDT(unsigned int lineNumber)
	: UDT(0, lineNumber)
	, _subUDTs()
	, _macros()
	, _enumMembers()
{
}

NamespaceUDT::~NamespaceUDT()
{
	for (vector<EnumMember *>::iterator it = _enumMembers.begin(); it != _enumMembers.end(); ++it) {
		delete *it;
	}
	_enumMembers.clear();

	for (vector<UDT *>::const_iterator it = _subUDTs.begin(); it != _subUDTs.end(); ++it) {
		delete *it;
	}
	_subUDTs.clear();
}

DDR_RC
NamespaceUDT::acceptVisitor(const TypeVisitor &visitor)
{
	return visitor.visitNamespace(this);
}

bool
NamespaceUDT::insertUnique(Symbol_IR *ir)
{
	/* Even if this is a duplicate, there may be additions to this namespace. */
	for (vector<UDT *>::iterator it = _subUDTs.begin(); it != _subUDTs.end();) {
		if ((*it)->insertUnique(ir)) {
			++it;
		} else {
			it = _subUDTs.erase(it);
		}
	}
	return UDT::insertUnique(ir);
}

const string &
NamespaceUDT::getSymbolKindName() const
{
	static const string namespaceKind("namespace");

	return namespaceKind;
}

void
NamespaceUDT::addMacro(Macro *macro)
{
	/* Add or update a macro: The last of a #define/#undef sequence applies. */
	for (vector<Macro>::iterator it = _macros.begin();; ++it) {
		if (_macros.end() == it) {
			_macros.push_back(*macro);
			break;
		} else if (macro->_name == it->_name) {
			it->setValue(macro->getValue());
			break;
		}
	}
}

std::vector<UDT *> *
NamespaceUDT::getSubUDTS()
{
	return &_subUDTs;
}

void
NamespaceUDT::renameFieldsAndMacros(const FieldOverride &fieldOverride, Type *replacementType)
{
	UDT::renameFieldsAndMacros(fieldOverride, replacementType);

	if (!fieldOverride.isTypeOverride) {
		for (vector<Macro>::iterator it = _macros.begin(); it != _macros.end(); ++it) {
			if (it->_name == fieldOverride.fieldName) {
				it->_name = fieldOverride.overrideName;
			}
		}
	}
	for (vector<UDT *>::iterator it = _subUDTs.begin(); it != _subUDTs.end(); ++it) {
		(*it)->renameFieldsAndMacros(fieldOverride, replacementType);
	}
}

bool
NamespaceUDT::operator==(const Type & rhs) const
{
	return rhs.compareToNamespace(*this);
}

bool
NamespaceUDT::compareToNamespace(const NamespaceUDT &other) const
{
	bool subUDTsEqual = _subUDTs.size() == other._subUDTs.size();
	vector<UDT *>::const_iterator it2 = other._subUDTs.begin();
	for (vector<UDT *>::const_iterator it = _subUDTs.begin();
		it != _subUDTs.end() && it2 != other._subUDTs.end() && subUDTsEqual;
		++ it, ++ it2) {
		subUDTsEqual = (**it == **it2);
	}
	return compareToUDT(other)
		&& subUDTsEqual;
}
