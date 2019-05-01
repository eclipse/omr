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

#ifndef TYPE_HPP
#define TYPE_HPP

#include "ddr/ir/TypeVisitor.hpp"
#include "ddr/std/string.hpp"

#include <set>
#include <vector>

using std::set;
using std::string;
using std::vector;

class ClassUDT;
class ClassType;
class EnumUDT;
class Macro;
class NamespaceUDT;
class Symbol_IR;
class TypedefUDT;
class UDT;
class UnionUDT;
struct FieldOverride;

class Type
{
public:
	static bool isStandardType(const char *type, size_t typeLen, bool *isSigned, size_t *bitWidth);

	bool _blacklisted;
	bool _opaque;
	string _name;
	size_t _sizeOf; /* Size of type in bytes */

	explicit Type(size_t size);
	virtual ~Type();

	bool isAnonymousType() const;

	virtual string getFullName() const;
	virtual const string &getSymbolKindName() const;
	string getTypeNameKey() const;

	/* Visitor pattern function to allow the scanner/generator/IR to dispatch functionality based on type. */
	virtual DDR_RC acceptVisitor(const TypeVisitor &visitor);

	/*
	 * Insert this type into the map in 'ir' and return true if its
	 * name doesn't clash; otherwise do nothing and return false.
	 */
	virtual bool insertUnique(Symbol_IR *ir);

	virtual NamespaceUDT *getNamespace();
	virtual size_t getPointerCount();
	virtual size_t getArrayDimensions();
	virtual void addMacro(Macro *macro);
	virtual vector<UDT *> *getSubUDTS();
	virtual void renameFieldsAndMacros(const FieldOverride &fieldOverride, Type *replacementType);
	virtual Type *getBaseType();
	Type *getOpaqueType();

	bool operator==(const Type & rhs) const;
	virtual bool compareToClass(const ClassUDT &) const;
	virtual bool compareToClasstype(const ClassType &) const;
	virtual bool compareToEnum(const EnumUDT &) const;
	virtual bool compareToNamespace(const NamespaceUDT &) const;
	virtual bool compareToType(const Type &) const;
	virtual bool compareToTypedef(const TypedefUDT &) const;
	virtual bool compareToUDT(const UDT &) const;
	virtual bool compareToUnion(const UnionUDT &) const;
};

#endif /* TYPE_HPP */
