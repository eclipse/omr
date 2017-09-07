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

#include "ddr/ir/Type.hpp"

Type::Type(size_t size)
	: _sizeOf(size), _isDuplicate(false)
{
}

Type::~Type() {}

bool
Type::isAnonymousType()
{
	return false;
}

string
Type::getFullName()
{
	return _name;
}

string
Type::getSymbolKindName()
{
	return "";
}

DDR_RC
Type::acceptVisitor(TypeVisitor const &visitor)
{
	return visitor.visitType(this);
}

void
Type::checkDuplicate(Symbol_IR *ir)
{
	/* No-op: since Types aren't printed, there's no need to check if they're duplicates either */
}

NamespaceUDT *
Type::getNamespace()
{
	return NULL;
}

size_t
Type::getPointerCount()
{
	return 0;
}

size_t
Type::getArrayDimensions()
{
	return 0;
}

void
Type::computeFieldOffsets()
{
	/* No-op: base types have no fields. */
}

void
Type::addMacro(Macro *macro)
{
	/* No-op: macros cannot be associated with base types. */
}

std::vector<UDT *> *
Type::getSubUDTS()
{
	return NULL;
}

void
Type::renameFieldsAndMacros(FieldOverride fieldOverride, Type *replacementType)
{
	/* No-op: base types have no fields. */
}

Type *
Type::getBaseType()
{
	return NULL;
}

bool
Type::operator==(Type const & rhs) const
{
	return rhs.compareToType(*this);
}

bool
Type::compareToType(Type const &other) const
{
	return (_name == other._name)
		&& ((0 == _sizeOf) || (0 == other._sizeOf) || (_sizeOf == other._sizeOf));
}

bool
Type::compareToUDT(UDT const &) const
{
	return false;
}

bool
Type::compareToNamespace(NamespaceUDT const &) const
{
	return false;
}

bool
Type::compareToEnum(EnumUDT const &) const
{
	return false;
}

bool
Type::compareToTypedef(TypedefUDT const &) const
{
	return false;
}

bool
Type::compareToClasstype(ClassType const &) const
{
	return false;
}

bool
Type::compareToUnion(UnionUDT const &) const
{
	return false;
}

bool
Type::compareToClass(ClassUDT const &) const
{
	return false;
}
