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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "ddr/ir/TypePrinter.hpp"

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"

void
TypePrinter::printIndent() const
{
	for (int32_t i = _indent; i > 0; --i) {
		_portLibrary->tty_printf(_portLibrary, "  ");
	}
}

void
TypePrinter::printFields(const std::vector<Field *> &fields) const
{
	if (0 != (_flags & FIELDS)) {
		for (vector<Field *>::const_iterator it = fields.begin(); it != fields.end(); ++it) {
			printIndent();
			_portLibrary->tty_printf(_portLibrary, "field '%s'\n", (*it)->_name.c_str());
		}
	}
}

void
TypePrinter::printLiterals(const std::vector<EnumMember *> &literals) const
{
	if (0 != (_flags & LITERALS)) {
		for (vector<EnumMember *>::const_iterator it = literals.begin(); it != literals.end(); ++it) {
			printIndent();
			_portLibrary->tty_printf(_portLibrary, "literal '%s' = %d\n", (*it)->_name.c_str(), (*it)->_value);
		}
	}
}

void
TypePrinter::printMacros(const std::vector<Macro> &macros) const
{
	if (0 != (_flags & MACROS)) {
		for (vector<Macro>::const_iterator it = macros.begin(); it != macros.end(); ++it) {
			printIndent();
			_portLibrary->tty_printf(_portLibrary, "macro '%s' = %s\n", it->_name.c_str(), it->getValue().c_str());
		}
	}
}

DDR_RC
TypePrinter::visitType(Type *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "type '%s' size(%zu)\n", type->_name.c_str(), type->_sizeOf);
	return DDR_RC_OK;
}

DDR_RC
TypePrinter::visitClass(ClassUDT *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "%s '%s' size(%zu)",
			type->_isClass ? "class" : "struct",
			type->_name.c_str(),
			type->_sizeOf);
	if (NULL != type->_superClass) {
		_portLibrary->tty_printf(_portLibrary, ":  %s", type->_superClass->_name.c_str());
	}
	_portLibrary->tty_printf(_portLibrary, " {\n");

	{
		const TypePrinter indented(this);

		indented.printFields(type->_fieldMembers);
		indented.printLiterals(type->_enumMembers);
		indented.printMacros(type->_macros);
		indented.printUDTs(type->_subUDTs);
	}

	printIndent();
	_portLibrary->tty_printf(_portLibrary, "}\n");

	return DDR_RC_OK;
}

DDR_RC
TypePrinter::visitEnum(EnumUDT *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "enum '%s' size(%zu) {\n", type->_name.c_str(), type->_sizeOf);

	{
		const TypePrinter indented(this);

		indented.printLiterals(type->_enumMembers);
	}

	printIndent();
	_portLibrary->tty_printf(_portLibrary, "}\n");

	return DDR_RC_OK;
}

DDR_RC
TypePrinter::visitNamespace(NamespaceUDT *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "namespace '%s' {\n", type->_name.c_str());

	{
		const TypePrinter indented(this);

		indented.printLiterals(type->_enumMembers);
		indented.printMacros(type->_macros);
		indented.printUDTs(type->_subUDTs);
	}

	printIndent();
	_portLibrary->tty_printf(_portLibrary, "}\n");

	return DDR_RC_OK;
}

DDR_RC
TypePrinter::visitTypedef(TypedefUDT *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "typedef '%s'\n", type->_name.c_str());

	return DDR_RC_OK;
}

DDR_RC
TypePrinter::visitUnion(UnionUDT *type) const
{
	printIndent();
	_portLibrary->tty_printf(_portLibrary, "union '%s' size(%zu) {\n", type->_name.c_str(), type->_sizeOf);

	{
		const TypePrinter indented(this);

		indented.printFields(type->_fieldMembers);
		indented.printLiterals(type->_enumMembers);
		indented.printMacros(type->_macros);
		indented.printUDTs(type->_subUDTs);
	}

	printIndent();
	_portLibrary->tty_printf(_portLibrary, "}\n");

	return DDR_RC_OK;
}
