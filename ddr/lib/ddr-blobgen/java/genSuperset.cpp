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

#include "ddr/blobgen/java/genSuperset.hpp"

#include <assert.h>
#include <stdio.h>

#include "ddr/config.hpp"
#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/TypeVisitor.hpp"
#include "ddr/ir/UnionUDT.hpp"

JavaSupersetGenerator::JavaSupersetGenerator(bool printEmptyTypes) : _file(0), _portLibrary(NULL), _printEmptyTypes(printEmptyTypes)
{
	initBaseTypedefSet();
}

void
JavaSupersetGenerator::initBaseTypedefSet()
{
	_baseTypedefSet.insert("unsigned char");
	_baseTypedefSet.insert("uint8_t");
	_baseTypedefSet.insert("__uint8_t");
	_baseTypedefSet.insert("signed char");
	_baseTypedefSet.insert("char");
	_baseTypedefSet.insert("int8_t");
	_baseTypedefSet.insert("__int8_t");

	_baseTypedefSet.insert("unsigned short int");
	_baseTypedefSet.insert("short unsigned int");
	_baseTypedefSet.insert("uint16_t");
	_baseTypedefSet.insert("__uint16_t");
	_baseTypedefSet.insert("signed short int");
	_baseTypedefSet.insert("short signed int");
	_baseTypedefSet.insert("short int");
	_baseTypedefSet.insert("int16_t");
	_baseTypedefSet.insert("__int16_t");

	_baseTypedefSet.insert("unsigned int");
	_baseTypedefSet.insert("uint32_t");
	_baseTypedefSet.insert("__uint32_t");
	_baseTypedefSet.insert("signed int");
	_baseTypedefSet.insert("int");
	_baseTypedefSet.insert("int32_t");
	_baseTypedefSet.insert("__int32_t");

	_baseTypedefSet.insert("long unsigned int");
	_baseTypedefSet.insert("unsigned long int");
	_baseTypedefSet.insert("long long unsigned int");
	_baseTypedefSet.insert("unsigned long long int");
	_baseTypedefSet.insert("uint64_t");
	_baseTypedefSet.insert("long signed int");
	_baseTypedefSet.insert("signed long int");
	_baseTypedefSet.insert("long long signed int");
	_baseTypedefSet.insert("signed long long int");
	_baseTypedefSet.insert("long int");
	_baseTypedefSet.insert("long long int");
	_baseTypedefSet.insert("int64_t");

	_baseTypedefMap["intptr_t"] = "IDATA";
	_baseTypedefMap["uintptr_t"] = "UDATA";
	_baseTypedefMap["char"] = "U8";

	_baseTypedefReplace["U_8"] = "U8";
	_baseTypedefReplace["U_16"] = "U16";
	_baseTypedefReplace["U_32"] = "U32";
	_baseTypedefReplace["U_64"] = "U64";
	_baseTypedefReplace["U_128"] = "U128";
	_baseTypedefReplace["I_8"] = "I8";
	_baseTypedefReplace["I_16"] = "I16";
	_baseTypedefReplace["I_32"] = "I32";
	_baseTypedefReplace["I_64"] = "I64";
	_baseTypedefReplace["I_128"] = "I128";

	_baseTypedefIgnore.insert("IDATA");
	_baseTypedefIgnore.insert("UDATA");
	for (unordered_map<string, string>::iterator it = _baseTypedefReplace.begin(); it != _baseTypedefReplace.end(); ++ it) {
		_baseTypedefIgnore.insert(it->first);
		_baseTypedefIgnore.insert(it->second);
	}
	for (unordered_map<string, string>::iterator it = _baseTypedefMap.begin(); it != _baseTypedefMap.end(); ++ it) {
		_baseTypedefIgnore.insert(it->first);
		_baseTypedefIgnore.insert(it->second);
	}
}

void
JavaSupersetGenerator::convertJ9BaseTypedef(Type *type, string *typeName)
{
	string name = type->getFullName();

	/* Convert int types to J9 base typedefs. */
	if (_baseTypedefMap.find(name) != _baseTypedefMap.end()) {
		name = _baseTypedefMap[name];
	} else if (_baseTypedefSet.find(name) != _baseTypedefSet.end()) {
		stringstream ss;
		ss << (0 == strchr(name.c_str(), 'u') ? "I" : "U")
		   << (type->_sizeOf * 8);
		name = ss.str();
	} else {
		replaceBaseTypedef(type, &name);
	}
	*typeName = name;
}

void
JavaSupersetGenerator::replaceBaseTypedef(Type *type, string *name)
{
	/* In both the first and second printed type name in the superset,
	 * types such as "U_32" are replaced with "U32" and function pointers
	 * are replaced with "void*".
	 */
	if (_baseTypedefReplace.find(*name) != _baseTypedefReplace.end()) {
		*name = _baseTypedefReplace[*name];
	} else {
		for (unordered_map<string, string>::iterator it = _baseTypedefReplace.begin(); it != _baseTypedefReplace.end(); it ++) {
			size_t index = name->find(it->first);
			if (string::npos != index) {
				name->replace(index, it->first.length(), it->second);
			}
		}
	}
}

class SupersetFieldVisitor : public TypeVisitor
{
private:
	JavaSupersetGenerator *_gen;
	string *_typeName;
	string *_simpleName;
	string *_prefixBase;
	string *_prefix;
	string *_pointerTypeBase;

public:
	SupersetFieldVisitor(JavaSupersetGenerator *gen, string *typeName, string *simpleName, string *prefixBase, string *prefix, string *pointerTypeBase)
		: _gen(gen), _typeName(typeName), _simpleName(simpleName), _prefixBase(prefixBase), _prefix(prefix), _pointerTypeBase(pointerTypeBase) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
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
SupersetFieldVisitor::visitType(NamespaceUDT *type) const
{
	*_simpleName = _gen->replace(type->getFullName(), "::", "$");
	*_typeName = *_simpleName;
	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitType(EnumUDT *type) const
{
	*_simpleName = _gen->replace(type->getFullName(), "::", "$");
	*_typeName = *_simpleName;
	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitType(TypedefUDT *type) const
{
	Type *baseType = type;
	string name = baseType->_name;
	while ((_gen->_baseTypedefIgnore.find(name) == _gen->_baseTypedefIgnore.end()) && (NULL != baseType->getBaseType())) {
		baseType = baseType->getBaseType();
		name = baseType->_name;
	}
	if (baseType == type) {
		*_typeName = _gen->replace(type->getFullName(), "::", "$");
		_gen->convertJ9BaseTypedef(type, _typeName);
	} else {
		baseType->acceptVisitor(SupersetFieldVisitor(_gen, _typeName, _simpleName, _prefixBase, _prefix, _pointerTypeBase));
	}
			
	if (type->getSymbolKindName().empty()) {
		*_simpleName = type->_name;
		_gen->replaceBaseTypedef(type, _simpleName);
	} else {
		*_simpleName = _gen->replace(type->getFullName(), "::", "$");
	}

	/* Get the field type. */
	/* Should "union " be printed in superset or is that not allowed? */
	*_prefixBase = type->getSymbolKindName();
	if (!_prefixBase->empty()) {
		*_prefixBase += " ";
	}
	if ((NULL != type->_aliasedType) && (type->_aliasedType->_name == type->_name)) {
		*_prefix = *_prefixBase;
	}

	/* Get field pointer/array notation. */
	for (size_t i = 0; i < type->getPointerCount(); i += 1) {
		*_pointerTypeBase = "*" + *_pointerTypeBase;
	}
	for (size_t i = 0; i < type->getArrayDimensions(); i += 1) {
		*_pointerTypeBase = *_pointerTypeBase + "[]";
	}

	return DDR_RC_OK;
}

DDR_RC
SupersetFieldVisitor::visitType(ClassUDT *type) const
{
	return visitType((NamespaceUDT *)type);
}

DDR_RC
SupersetFieldVisitor::visitType(UnionUDT *type) const
{
	return visitType((NamespaceUDT *)type);
}

/* The printed format for fields is:
 *     F|PREFIX OUTER_TYPE$assembledBaseTypeName:BIT_FIELD|MODIFIERS PREFIX OUTER_TYPE.typeName:BIT_FIELD
 * - For fields with a type which are not typedefs, the assembled base type name
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
JavaSupersetGenerator::getFieldType(Field *f, string *assembledTypeName, string *simpleTypeName)
{
	DDR_RC rc = DDR_RC_OK;

	/* Get modifier name list for the field. */
	string modifiers;
	if (0 != (f->_modifiers._modifierFlags & ~Modifiers::MODIFIER_FLAGS)) {
		ERRMSG("Unhandled field modifer flags: %d", f->_modifiers._modifierFlags);
		rc = DDR_RC_ERROR;
	} else {
		modifiers =  f->_modifiers.getModifierNames();
	}

	/* Get field pointer/array notation. */
	string pointerType = "";
	if (DDR_RC_OK == rc) {
		pointerType = f->_modifiers.getPointerType();
	}

	string typeName = "";
	string simpleName = "";
	string prefix = "";
	string prefixBase = "";
	string pointerTypeBase = pointerType;
	if (DDR_RC_OK == rc) {
		if (NULL == f->_fieldType) {
			typeName = "void";
			simpleName = "void";
		} else {
			rc = f->_fieldType->acceptVisitor(SupersetFieldVisitor(this, &typeName, &simpleName, &prefixBase, &prefix, &pointerTypeBase));
		}
	}
	
	/* Get the bit field, if it has one. */
	string bitField;
	if ((DDR_RC_OK == rc) && (0 != f->_bitField)) {
		stringstream ss;
		ss << ":" << f->_bitField;
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
	JavaSupersetGenerator *_supersetGen;
	bool _addFieldsOnly;
	string _prefix;

public:
	SupersetVisitor(JavaSupersetGenerator *supersetGen, bool addFieldsOnly, string prefix) : _supersetGen(supersetGen), _addFieldsOnly(addFieldsOnly), _prefix(prefix) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
};

/* Print a field member with the format:
 *     F|PREFIX OUTER_TYPE$assembledBaseTypeName:BIT_FIELD|MODIFIERS PREFIX OUTER_TYPE.typeName:BIT_FIELD
 * See getFieldType() for more details on how these type names are constructed.
 */
DDR_RC
JavaSupersetGenerator::printFieldMember(Field *field, string prefix = "")
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	DDR_RC rc = DDR_RC_OK;
	/* If the type of a field is an anonymous inner type, print its fields prefixed by its field
	 * name in place of the field.
	 */
	if (!field->_isStatic) {
		if ((NULL != field->_fieldType) && field->_fieldType->isAnonymousType()) {
			field->_fieldType->acceptVisitor(SupersetVisitor(this, true, prefix + field->_name + "."));
		} else {
			string nameFormatted = prefix + (field->_name == "class" ? "klass" : field->_name);
			string lineToPrint = "F|" + replace(nameFormatted, ".", "$") + "|" + nameFormatted + "|";

			string assembledTypeName;
			string simpleTypeName;

			rc = getFieldType(field, &assembledTypeName, &simpleTypeName);
			if (DDR_RC_OK != rc) {
				ERRMSG("Could not assemble field type name");
			}

			if (DDR_RC_OK == rc) {
				string fieldFormatted = replace(assembledTypeName, "::", "$");
				lineToPrint += fieldFormatted + "|" + simpleTypeName + "\n";
				omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
			}
		}
	}

	return rc;
}

void
JavaSupersetGenerator::printConstantMember(string name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	string lineToPrint = "C|" + name + "\n";
	omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
}

string
JavaSupersetGenerator::replace(string str, string subStr, string newStr)
{
	size_t i = 0;
	while (true) {
		i = str.find(subStr, i);
		if (string::npos == i) {
			break;
		}

		str.replace(i, subStr.length(), newStr);
		i += subStr.length() - 1;
	}

	return str;
}

DDR_RC
SupersetVisitor::visitType(Type *type) const
{
	/* No-op: do not print base types at this time. */
	return DDR_RC_OK;
}

DDR_RC
SupersetVisitor::visitType(TypedefUDT *type) const
{
	/* No-op: do not print base types at this time. */
	return DDR_RC_OK;
}

DDR_RC
SupersetVisitor::visitType(ClassUDT *type) const
{
	OMRPORT_ACCESS_FROM_OMRPORT(_supersetGen->_portLibrary);
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		if ((!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _supersetGen->_printEmptyTypes)) {
			if (!_addFieldsOnly) {
				string nameFormatted = _supersetGen->replace(type->getFullName(), "::", "$");
				string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|";
				if (NULL != type->_superClass) {
					string superClassFormatted = _supersetGen->replace(type->_superClass->getFullName(), "::", "$");
					lineToPrint += superClassFormatted + "\n";
				} else {
					lineToPrint += "\n";
				}
				omrfile_write(_supersetGen->_file, lineToPrint.c_str(), lineToPrint.length());
			}

			for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
				_supersetGen->printConstantMember((*m)->_name);
			}

			for (vector<Field *>::iterator m = type->_fieldMembers.begin(); m != type->_fieldMembers.end(); ++m) {
				rc = _supersetGen->printFieldMember(*m, _prefix);
				if (DDR_RC_OK != rc) {
					break;
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
					if (DDR_RC_OK == m->getNumeric(NULL)) {
						_supersetGen->printConstantMember(m->_name);
					}
				}
			}
		}
	}

	/* Anonymous sub udt's not used as fields are to have their fields added to this struct.*/
	if ((DDR_RC_OK == rc) && (!_addFieldsOnly)) {
		for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
			if ((*it)->isAnonymousType()) {
				bool isUsedAsField = false;
				for (vector<Field *>::iterator fit = type->_fieldMembers.begin(); fit != type->_fieldMembers.end(); fit += 1) {
					if ((*fit)->_fieldType == (*it)) {
						isUsedAsField = true;
						break;
					}
				}
				if (!isUsedAsField) {
					rc = (*it)->acceptVisitor(SupersetVisitor(_supersetGen, true, _prefix));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
	}

	if ((DDR_RC_OK == rc) && (!_addFieldsOnly)) {
		for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
			bool isUsedAsField = false;
			if ((*it)->isAnonymousType()) {
				for (vector<Field *>::iterator fit = type->_fieldMembers.begin(); fit != type->_fieldMembers.end(); fit += 1) {
					if ((*fit)->_fieldType == (*it)) {
						isUsedAsField = true;
						break;
					}
				}
			}
			if (!(*it)->isAnonymousType() || isUsedAsField) {
				rc = (*it)->acceptVisitor(SupersetVisitor(_supersetGen, false, _prefix));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}
	return rc;
}

DDR_RC
SupersetVisitor::visitType(UnionUDT *type) const
{
	OMRPORT_ACCESS_FROM_OMRPORT(_supersetGen->_portLibrary);
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		if ((!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _supersetGen->_printEmptyTypes)) {
			if (!_addFieldsOnly) {
				string nameFormatted = _supersetGen->replace(type->getFullName(), "::", "$");
				string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
				omrfile_write(_supersetGen->_file, lineToPrint.c_str(), lineToPrint.length());
			}

			for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
				_supersetGen->printConstantMember((*m)->_name);
			}

			if (DDR_RC_OK == rc) {
				for (vector<Field *>::iterator m = type->_fieldMembers.begin(); m != type->_fieldMembers.end(); ++m) {
					rc = _supersetGen->printFieldMember(*m, _prefix);
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
					if (DDR_RC_OK == m->getNumeric(NULL)) {
						_supersetGen->printConstantMember(m->_name);
					}
				}
			}
		}
	}
	if ((DDR_RC_OK == rc) && (!_addFieldsOnly)) {
		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			rc = (*v)->acceptVisitor(SupersetVisitor(_supersetGen, false, ""));
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}
	return rc;
}

DDR_RC
SupersetVisitor::visitType(EnumUDT *type) const
{
	OMRPORT_ACCESS_FROM_OMRPORT(_supersetGen->_portLibrary);
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		if ((!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_enumMembers.size() > 0) || _supersetGen->_printEmptyTypes)) {
			if (!_addFieldsOnly) {
				string nameFormatted = _supersetGen->replace(type->getFullName(), "::", "$");
				string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
				omrfile_write(_supersetGen->_file, lineToPrint.c_str(), lineToPrint.length());
			}

			for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
				_supersetGen->printConstantMember((*m)->_name);
			}
		}
	}
	return rc;
}

DDR_RC
SupersetVisitor::visitType(NamespaceUDT *type) const
{
	OMRPORT_ACCESS_FROM_OMRPORT(_supersetGen->_portLibrary);
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		if (!type->isAnonymousType() || _addFieldsOnly) {
			if (!_addFieldsOnly) {
				string nameFormatted = _supersetGen->replace(type->getFullName(), "::", "$");
				string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
				omrfile_write(_supersetGen->_file, lineToPrint.c_str(), lineToPrint.length());
			}

			for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
				if (DDR_RC_OK == m->getNumeric(NULL)) {
					_supersetGen->printConstantMember(m->_name);
				}
			}
		}
	}
	if (!_addFieldsOnly) {
		/* Anonymous sub udt's are to have their fields added to this struct. */
		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			if ((*v)->isAnonymousType()) {
				rc = (*v)->acceptVisitor(SupersetVisitor(_supersetGen, true, _prefix));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}

		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			if (!(*v)->isAnonymousType()) {
				rc = (*v)->acceptVisitor(SupersetVisitor(_supersetGen, false, _prefix));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}
	return rc;
}

DDR_RC
JavaSupersetGenerator::printSuperset(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *supersetFile)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	_portLibrary = portLibrary;
	_file = omrfile_open(supersetFile, EsOpenCreate | EsOpenWrite | EsOpenAppend | EsOpenTruncate, 0644);

	DDR_RC rc = DDR_RC_OK;
	for (vector<Type *>::iterator v = ir->_types.begin(); v != ir->_types.end(); ++v) {
		rc = (*v)->acceptVisitor(SupersetVisitor(this, false, ""));
		if (DDR_RC_OK != rc) {
			break;
		}
	}
	return rc;
}
