/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "ddr/blobgen/java/genSuperset.hpp"

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"
#include "ddr/std/sstream.hpp"

#include <assert.h>
#include <stdio.h>

using std::stringstream;

static string
replaceAll(string str, const string &subStr, const string &newStr)
{
	for (size_t i = 0;;) {
		i = str.find(subStr, i);
		if (string::npos == i) {
			break;
		}

		str.replace(i, subStr.length(), newStr);
		i += subStr.length() - newStr.length();
	}

	return str;
}

JavaSupersetGenerator::JavaSupersetGenerator(bool printEmptyTypes)
	: _file(0)
	, _portLibrary(NULL)
	, _printEmptyTypes(printEmptyTypes)
	, _pendingTypeHeading()
{
}

void
JavaSupersetGenerator::convertJ9BaseTypedef(Type *type, string *typeName)
{
	string name = type->getFullName();

	replaceBaseTypedef(type, &name);

	*typeName = name;
}

void
JavaSupersetGenerator::replaceBaseTypedef(Type *type, string *name)
{
	string::size_type start = name->rfind('(');

	if (string::npos == start) {
		start = 0;
	} else {
		start += 1;
	}

	string::size_type end = name->find_first_of("*[)", start);

	if (string::npos == end) {
		end = name->length();
	}

	string::size_type length = end - start;
	bool isSigned = false;
	size_t bitWidth = 0;

	/*
	 * In both the first and second printed type names in the superset,
	 * types such as "U_32" are replaced with "U32".
	 */
	if (Type::isStandardType(name->c_str() + start, (size_t)length, &isSigned, &bitWidth)) {
		stringstream ss;

		ss << (isSigned ? "I" : "U") << bitWidth;

		name->replace(start, length, ss.str());
	}
}

class SupersetFieldVisitor : public TypeVisitor
{
private:
	JavaSupersetGenerator * const _gen;
	string * const _typeName;
	string * const _simpleName;
	string * const _prefixBase;
	string * const _prefix;
	string * const _pointerTypeBase;

public:
	SupersetFieldVisitor(JavaSupersetGenerator *gen, string *typeName, string *simpleName, string *prefixBase, string *prefix, string *pointerTypeBase)
		: _gen(gen), _typeName(typeName), _simpleName(simpleName), _prefixBase(prefixBase), _prefix(prefix), _pointerTypeBase(pointerTypeBase)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;
};

DDR_RC
SupersetFieldVisitor::visitType(Type *type) const
{
	*_simpleName = type->_name;
	_gen->replaceBaseTypedef(type, _simpleName);
	_gen->convertJ9BaseTypedef(type, _typeName);
	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitNamespace(NamespaceUDT *type) const
{
	*_simpleName = replaceAll(type->getFullName(), "::", "$");
	*_typeName = *_simpleName;
	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitEnum(EnumUDT *type) const
{
	*_simpleName = replaceAll(type->getFullName(), "::", "$");
	*_typeName = *_simpleName;
	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitTypedef(TypedefUDT *type) const
{
	Type *opaqueType = type->getOpaqueType();
	if (opaqueType == type) {
		*_typeName = replaceAll(type->getFullName(), "::", "$");
		_gen->convertJ9BaseTypedef(type, _typeName);
	} else {
		opaqueType->acceptVisitor(SupersetFieldVisitor(_gen, _typeName, _simpleName, _prefixBase, _prefix, _pointerTypeBase));
	}

	string symbolKind = type->getSymbolKindName();
	if (symbolKind.empty()) {
		*_simpleName = type->_name;
		_gen->replaceBaseTypedef(type, _simpleName);
	} else {
		*_simpleName = replaceAll(type->getFullName(), "::", "$");
	}

	/* Get the field type. */
	/* Prefix "union" should not be printed in superset */
	if (symbolKind.empty() || ("union" == symbolKind)) {
		*_prefixBase = "";
	} else {
		*_prefixBase = symbolKind + " ";
	}
	*_prefix = *_prefixBase;

	/* Get field pointer/array notation. */
	for (size_t i = type->getPointerCount(); i != 0; i -= 1) {
		*_pointerTypeBase = "*" + *_pointerTypeBase;
	}
	for (size_t i = type->getArrayDimensions(); i != 0; i -= 1) {
		*_pointerTypeBase = *_pointerTypeBase + "[]";
	}

	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitClass(ClassUDT *type) const
{
	return visitNamespace(type);
}

DDR_RC
SupersetFieldVisitor::visitUnion(UnionUDT *type) const
{
	return visitNamespace(type);
}

/* The printed format for fields is:
 *     F|PREFIX OUTER_TYPE$assembledBaseTypeName:BIT_FIELD|MODIFIERS PREFIX OUTER_TYPE.typeName:BIT_FIELD
 * - For fields with a type which is not a typedef, the assembled base type name
 *   and type name are identical. For fields with a typedef type, the type name
 *   is of the type directly and the assembled base type name is constructed by
 *   recursively following the chain of typedefs until a non-typedef type is found.
 * - MODIFIERS: Field modifiers, such as const or volatile, are only added to the second
 *   type name and not the assembled type name.
 * - OUTER_TYPE: Outer type names are prefixed both to assembled type names and type names,
     but with different formatting.
 * - PREFIX: class/struct/enum is only prefixed to the assembled type name of types
 *   which are defined with the format "typedef struct NAME {...} NAME". In dwarf, for
 *   example, these are fields of a typedef of an identically named type.
 * - BIT_FIELD: the member's bit field, added to both assembled and type names.
 */
DDR_RC
JavaSupersetGenerator::getFieldType(Field *field, string *assembledTypeName, string *simpleTypeName)
{
	DDR_RC rc = DDR_RC_OK;

	/* Get modifier name list for the field. */
	string modifiers;
	if (0 != (field->_modifiers._modifierFlags & ~Modifiers::MODIFIER_FLAGS)) {
		ERRMSG("Unhandled field modifer flags: %d", field->_modifiers._modifierFlags);
		rc = DDR_RC_ERROR;
	} else {
		modifiers = field->_modifiers.getModifierNames();
	}

	/* Get field pointer/array notation. */
	string pointerType = "";

	if (DDR_RC_OK == rc) {
		pointerType = field->_modifiers.getPointerType();
	}

	string typeName = "";
	string simpleName = "";
	string prefix = "";
	string prefixBase = "";
	string pointerTypeBase = pointerType;

	if (DDR_RC_OK == rc) {
		Type *fieldType = field->_fieldType;

		if ((NULL == fieldType) || fieldType->_blacklisted || (field->_modifiers._pointerCount > 1)) {
			typeName = "void";
			simpleName = "void";
		} else {
			rc = fieldType->acceptVisitor(SupersetFieldVisitor(this, &typeName, &simpleName, &prefixBase, &prefix, &pointerTypeBase));
		}
	}

	/* Get the bit field, if it has one. */
	string bitField;

	if ((DDR_RC_OK == rc) && (0 != field->_bitField)) {
		stringstream ss;

		ss << ":" << field->_bitField;
		bitField = ss.str();
	}

	/* Assemble the type name. */
	if (NULL != assembledTypeName) {
		*assembledTypeName = prefixBase + typeName + pointerTypeBase + bitField;
	}

	if (NULL != simpleTypeName) {
		*simpleTypeName = modifiers + prefix + simpleName + pointerType + bitField;
	}

	return rc;
}

class SupersetVisitor : public TypeVisitor
{
private:
	JavaSupersetGenerator * const _supersetGen;
	const bool _addFieldsOnly;
	const string _prefix;

	DDR_RC visitComposite(ClassType *type, Type *superClass) const;

public:
	explicit SupersetVisitor(JavaSupersetGenerator *supersetGen)
		: _supersetGen(supersetGen)
		, _addFieldsOnly(false)
		, _prefix()
	{
	}

	SupersetVisitor(JavaSupersetGenerator *supersetGen, bool addFieldsOnly, const string &prefix)
		: _supersetGen(supersetGen)
		, _addFieldsOnly(addFieldsOnly)
		, _prefix(prefix)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;
};

/* Print a field member with the format:
 *     F|PREFIX OUTER_TYPE$assembledBaseTypeName:BIT_FIELD|MODIFIERS PREFIX OUTER_TYPE.typeName:BIT_FIELD
 * See getFieldType() for more details on how these type names are constructed.
 */
DDR_RC
JavaSupersetGenerator::printFieldMember(Field *field, const string &prefix)
{
	DDR_RC rc = DDR_RC_OK;

	/* If the type of a field is an anonymous inner type, print its fields prefixed by its field
	 * name in place of the field.
	 */
	if (!field->_isStatic) {
		Type *fieldType = field->_fieldType;

		if ((NULL != fieldType) && fieldType->isAnonymousType()) {
			rc = fieldType->acceptVisitor(SupersetVisitor(this, true, prefix + field->_name + "."));
		} else {
			string assembledTypeName;
			string simpleTypeName;

			rc = getFieldType(field, &assembledTypeName, &simpleTypeName);
			if (DDR_RC_OK != rc) {
				ERRMSG("Could not assemble field type name");
			} else {
				rc = printPendingType();

				if (DDR_RC_OK == rc) {
					string nameFormatted = prefix + (field->_name == "class" ? "klass" : field->_name);
					string lineToPrint = "F|" + replaceAll(nameFormatted, ".", "$")
									    + "|" + nameFormatted
									    + "|" + replaceAll(assembledTypeName, "::", "$")
									    + "|" + simpleTypeName + "\n";

					rc = print(lineToPrint);
				}
			}
		}
	}

	return rc;
}

DDR_RC
JavaSupersetGenerator::printType(Type *type, Type *superClass)
{
	const string nameFormatted = replaceAll(type->getFullName(), "::", "$");
	string superFormatted;

	if (NULL != superClass) {
		superFormatted = replaceAll(superClass->getFullName(), "::", "$");
	}

	_pendingTypeHeading = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|" + superFormatted + "\n";

	return _printEmptyTypes ? printPendingType() : DDR_RC_OK;
}

DDR_RC
JavaSupersetGenerator::printPendingType()
{
	DDR_RC rc = DDR_RC_OK;

	if (0 != _pendingTypeHeading.length()) {
		rc = print(_pendingTypeHeading);
		_pendingTypeHeading = "";
	}

	return rc;
}

DDR_RC
JavaSupersetGenerator::printConstantMember(const string &name)
{
	DDR_RC rc = printPendingType();

	if (DDR_RC_OK == rc) {
		const string lineToPrint = "C|" + name + "\n";

		rc = print(lineToPrint);
	}

	return rc;
}

DDR_RC
JavaSupersetGenerator::print(const string &text)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	const size_t lengthToPrint = text.length();
	DDR_RC rc = DDR_RC_OK;

	if (lengthToPrint != (size_t)omrfile_write(_file, text.c_str(), lengthToPrint)) {
		ERRMSG("Write failed");
		rc = DDR_RC_ERROR;
	}

	return rc;
}

DDR_RC
SupersetVisitor::visitType(Type *type) const
{
	/* No-op: do not print base types at this time. */
	return DDR_RC_OK;
}

DDR_RC
SupersetVisitor::visitTypedef(TypedefUDT *type) const
{
	/* No need to write this typedef: references are expanded. */
	return DDR_RC_OK;
}

DDR_RC
SupersetVisitor::visitComposite(ClassType *type, Type *superClass) const
{
	DDR_RC rc = DDR_RC_OK;

	if (!_addFieldsOnly) {
		if (!type->isAnonymousType()) {
			rc = _supersetGen->printType(type, superClass);
		}

		for (vector<EnumMember *>::const_iterator m = type->_enumMembers.begin();
				(DDR_RC_OK == rc) && (m != type->_enumMembers.end()); ++m) {
			rc = _supersetGen->printConstantMember((*m)->_name);
		}

		for (vector<Macro>::const_iterator m = type->_macros.begin();
				(DDR_RC_OK == rc) && (m != type->_macros.end()); ++m) {
			if (DDR_RC_OK == m->getNumeric(NULL)) {
				rc = _supersetGen->printConstantMember(m->_name);
			}
		}
	}

	if (_addFieldsOnly || !type->isAnonymousType()) {
		for (vector<Field *>::const_iterator m = type->_fieldMembers.begin();
				(DDR_RC_OK == rc) && (m != type->_fieldMembers.end()); ++m) {
			rc = _supersetGen->printFieldMember(*m, _prefix);
		}
	}

	if (!_addFieldsOnly) {
		for (vector<UDT *>::const_iterator it = type->_subUDTs.begin();
				(DDR_RC_OK == rc) && (it != type->_subUDTs.end()); ++it) {
			UDT *nested = *it;

			if (nested->_blacklisted) {
				continue;
			}

			bool includeSubUDT = true;

			if (nested->isAnonymousType()) {
				includeSubUDT = false;
				for (vector<Field *>::const_iterator fit = type->_fieldMembers.begin(); fit != type->_fieldMembers.end(); ++fit) {
					if ((*fit)->_fieldType == nested) {
						includeSubUDT = true;
						break;
					}
				}
			}

			if (includeSubUDT) {
				rc = nested->acceptVisitor(*this);
			}
		}
	}

	return rc;
}

DDR_RC
SupersetVisitor::visitClass(ClassUDT *type) const
{
	return visitComposite(type, type->_superClass);
}

DDR_RC
SupersetVisitor::visitUnion(UnionUDT *type) const
{
	return visitComposite(type, NULL);
}

DDR_RC
SupersetVisitor::visitEnum(EnumUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;

	if (_addFieldsOnly || !type->isAnonymousType()) {
		if (!_addFieldsOnly) {
			rc = _supersetGen->printType(type, NULL);
		}

		for (vector<EnumMember *>::const_iterator m = type->_enumMembers.begin();
				(DDR_RC_OK == rc) && (m != type->_enumMembers.end()); ++m) {
			rc = _supersetGen->printConstantMember((*m)->_name);
		}
	}

	return rc;
}

DDR_RC
SupersetVisitor::visitNamespace(NamespaceUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;

	if (_addFieldsOnly || !type->isAnonymousType()) {
		if (!_addFieldsOnly) {
			rc = _supersetGen->printType(type, NULL);
		}

		for (vector<Macro>::const_iterator m = type->_macros.begin();
				(DDR_RC_OK == rc) && (m != type->_macros.end()); ++m) {
			if (DDR_RC_OK == m->getNumeric(NULL)) {
				rc = _supersetGen->printConstantMember(m->_name);
			}
		}
	}

	if (!_addFieldsOnly) {
		for (vector<UDT *>::const_iterator v = type->_subUDTs.begin();
				(DDR_RC_OK == rc) && (v != type->_subUDTs.end()); ++v) {
			UDT *nested = *v;

			if (!nested->_blacklisted && !nested->isAnonymousType()) {
				rc = nested->acceptVisitor(*this);
			}
		}
	}

	return rc;
}

DDR_RC
JavaSupersetGenerator::printSuperset(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *supersetFile)
{
	DDR_RC rc = DDR_RC_OK;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	_portLibrary = portLibrary;
	_file = omrfile_open(supersetFile, EsOpenCreate | EsOpenWrite | EsOpenAppend | EsOpenTruncate, 0644);

	const SupersetVisitor printer(this);

	for (vector<Type *>::const_iterator v = ir->_types.begin();
			(DDR_RC_OK == rc) && (v != ir->_types.end()); ++v) {
		Type *type = *v;
		if (!type->_blacklisted) {
			rc = type->acceptVisitor(printer);
		}
	}

	return rc;
}
