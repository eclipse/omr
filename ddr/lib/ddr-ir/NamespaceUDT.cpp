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

#include "ddr/ir/NamespaceUDT.hpp"

NamespaceUDT::NamespaceUDT(unsigned int lineNumber)
	: UDT(0, lineNumber)
{
}

NamespaceUDT::~NamespaceUDT()
{
	for (size_t i = 0; i < _subUDTs.size(); i += 1) {
		delete(_subUDTs[i]);
	}
	_subUDTs.clear();
}

DDR_RC
NamespaceUDT::acceptVisitor(TypeVisitor const &visitor)
{
	return visitor.visitType(this);
}

void
NamespaceUDT::checkDuplicate(Symbol_IR *ir)
{
	UDT::checkDuplicate(ir);

	/* Still check the sub UDTs. This is because a previous duplicate of this UDT could have
	 * contained an empty version of the same sub UDT as this one, which was skipped over due
	 * to its emptiness.
	 */
	for (vector<UDT *>::iterator v = this->_subUDTs.begin(); v != this->_subUDTs.end(); ++v) {
		(*v)->checkDuplicate(ir);
	}
}

string
NamespaceUDT::getSymbolKindName()
{
	return "namespace";
}

void
NamespaceUDT::computeFieldOffsets()
{
	for (vector<UDT *>::iterator it = _subUDTs.begin(); it != _subUDTs.end(); it += 1) {
		(*it)->computeFieldOffsets();
	}
}

void
NamespaceUDT::addMacro(Macro *macro)
{
	/* Check if the macro already exists before adding it. */
	bool alreadyExists = false;
	for (vector<Macro>::iterator it = _macros.begin(); it != _macros.end(); ++it) {
		if (macro->_name == it->_name) {
			alreadyExists = true;
			break;
		}
	}
	if (!alreadyExists) {
		_macros.push_back(*macro);
	}
}

std::vector<UDT *> *
NamespaceUDT::getSubUDTS()
{
	return &_subUDTs;
}

void
NamespaceUDT::renameFieldsAndMacros(FieldOverride fieldOverride, Type *replacementType)
{
	if (!fieldOverride.isTypeOverride) {
		for (vector<Macro>::iterator it = _macros.begin(); it != _macros.end(); it += 1) {
			if (it->_name == fieldOverride.fieldName) {
				it->_name = fieldOverride.overrideName;
			}
		}
	}
	for (vector<UDT *>::iterator it = _subUDTs.begin(); it != _subUDTs.end(); it += 1) {
		(*it)->renameFieldsAndMacros(fieldOverride, replacementType);
	}		
}

bool
NamespaceUDT::operator==(Type const & rhs) const
{
	return rhs.compareToNamespace(*this);
}

bool
NamespaceUDT::compareToNamespace(NamespaceUDT const &other) const
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
