/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "genSuperset.hpp"

#include <assert.h>
#include <stdio.h>

#include "config.hpp"
#include "ClassUDT.hpp"
#include "EnumMember.hpp"
#include "EnumUDT.hpp"
#include "TypedefUDT.hpp"
#include "UnionUDT.hpp"

JavaSupersetGenerator::JavaSupersetGenerator() : _file(0), _portLibrary(NULL)
{
	initBaseTypedefSet();
}

string
JavaSupersetGenerator::getUDTname(Type *type)
{
	UDT *udt = dynamic_cast<UDT *>(type);
	assert(NULL != udt);

	string name;
	if (NULL != udt->_outerUDT) {
		name = getUDTname(udt->_outerUDT) + "$" + udt->_name;
	} else {
		name = udt->_name;
	}

	return name;
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
	string name = type->_name;

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

DDR_RC
JavaSupersetGenerator::getTypeName(Field *f, string *typeName)
{
	DDR_RC rc = DDR_RC_OK;
	if (NULL != f->_fieldType) {
		SymbolType st;
		rc = f->getBaseSymbolType(&st);

		if (DDR_RC_OK == rc) {
			Type *type = f->_fieldType;
			string name = type->_name;
			TypedefUDT *td = dynamic_cast<TypedefUDT *>(type);
			while ((NULL != td) && (_baseTypedefIgnore.find(name) == _baseTypedefIgnore.end()) && (NULL != td->_type)) {
				type = td->_type;
				name = td->_type->_name;
				td = dynamic_cast<TypedefUDT *>(type);
			}
			if (BASE == st) {
				convertJ9BaseTypedef(type, typeName);
			} else {
				*typeName = getUDTname(type);
			}
		}
	} else {
		ERRMSG("NULL _fieldType");
		rc = DDR_RC_ERROR;
	}

	return rc;
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

	/* Get the translated and simple type names for the field. */
	string typeName;
	SymbolType st;
	rc = getTypeName(f, &typeName);
	if (DDR_RC_OK != rc) {
		ERRMSG("Could not get typeName");
	} else {
		rc = f->getBaseSymbolType(&st);
	}

	string simpleName = "";
	if (DDR_RC_OK == rc) {
		if (NULL != f->_fieldType) {
			if (BASE == st) {
				simpleName = f->_fieldType->_name;
				replaceBaseTypedef(f->_fieldType, &simpleName);
			} else {
				simpleName = getUDTname(f->_fieldType);
			}
		}
	}

	/* Get the field type. */
	string prefixBase;
	string prefix;
	TypedefUDT *td = dynamic_cast<TypedefUDT *>(f->_fieldType);
	if ((DDR_RC_OK == rc) && (NULL != td)) {
		if ((NULL != td) && (NULL != td->_type) && !td->_type->_name.empty()) {
			switch (st) {
			case CLASS:
				prefixBase = "class ";
				break;
			case STRUCT:
				prefixBase = "struct ";
				break;
			case UNION:
				break;
			case ENUM:
				prefixBase = "enum ";
				break;
			case BASE:
				break;
			case TYPEDEF:
				break;
			case NAMESPACE:
				break;
			default:
				ERRMSG("unhandled fieldType: %d", st);
				rc = DDR_RC_ERROR;
			}
			if (td->_type->_name == f->_fieldType->_name) {
				prefix = prefixBase;
			}
		}
	}

	/* Get field pointer/array notation. */
	string pointerType = "";
	string pointerTypeBase = "";
	if (DDR_RC_OK == rc) {
		pointerType = f->_modifiers.getPointerType();

		/* For typedef types, follow the typedef recursively and add pointer/array notation. */
		size_t pointerCount = f->_modifiers._pointerCount;
		size_t arrayDimensions = f->_modifiers.getArrayDimensions();
		TypedefUDT *td = dynamic_cast<TypedefUDT *>(f->_fieldType);
		while (NULL != td) {
			pointerCount += td->_modifiers._pointerCount;
			arrayDimensions += td->_modifiers.getArrayDimensions();
			td = dynamic_cast<TypedefUDT *>(td->_type);
		}
		stringstream ss;
		ss << string(pointerCount, '*');
		for (size_t i = 0; i < arrayDimensions; i += 1) {
			ss << "[]";
		}
		pointerTypeBase = ss.str();
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
	if (NULL == field->_fieldType) {
		rc = DDR_RC_ERROR;
		ERRMSG("Field %s has NULL type", field->_name.c_str());
	} else if (!field->_isStatic) {
		if (field->_fieldType->isAnonymousType()) {
			field->_fieldType->printToSuperset(this, true, prefix + field->_name + ".");
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
		i += subStr.length();
	}

	return str;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(Type *type, bool addFieldsOnly, string prefix)
{
	/* No-op: do not print base types at this time. */
	return DDR_RC_OK;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(TypedefUDT *type, bool addFieldsOnly, string prefix)
{
	/* No-op: do not print typedefs at this time. */
	return DDR_RC_OK;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(ClassUDT *type, bool addFieldsOnly, string prefix)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	DDR_RC rc = DDR_RC_OK;

	if ((!type->isAnonymousType() || addFieldsOnly) && (type->_fieldMembers.size() > 0)) {
		if (!addFieldsOnly) {
			string nameFormatted = getUDTname(type);
			string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|";
			if (NULL != type->_superClass) {
				string superClassFormatted = getUDTname(type->_superClass);
				lineToPrint += superClassFormatted + "\n";
			} else {
				lineToPrint += "\n";
			}
#if defined(AIXPPC)
			if (_printedTypes.end() != _printedTypes.find(lineToPrint)) {
				type->_isDuplicate = true;

				/* Still print the sub UDTs. This is because a previous duplicate of this UDT could have
				 * contained an empty version of the same sub UDT as this one, which was skipped over due
				 * to its emptiness.
				 */
				 goto PrintSubUDTS;
			} else {
				_printedTypes.insert(lineToPrint);
			}
#endif /* defined(AIXPPC) */
			omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
		}

		for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
			printConstantMember((*m)->_name);
		}

		for (vector<Field *>::iterator m = type->_fieldMembers.begin(); m != type->_fieldMembers.end(); ++m) {
			rc = printFieldMember(*m, prefix);
			if (DDR_RC_OK != rc) {
				break;
			}
		}

		if (DDR_RC_OK == rc) {
			for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
				if (DDR_RC_OK == m->getNumeric(NULL)) {
					printConstantMember(m->_name);
				}
			}
		}
	}

PrintSubUDTS:
	if ((DDR_RC_OK == rc) && (!addFieldsOnly)) {
		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			rc = (*v)->printToSuperset(this, false, prefix);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}
	return rc;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(UnionUDT *type, bool addFieldsOnly, string prefix)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	DDR_RC rc = DDR_RC_OK;

	if ((!type->isAnonymousType() || addFieldsOnly) && (type->_fieldMembers.size() > 0)) {
		if (!addFieldsOnly) {
			string nameFormatted = getUDTname(type);
			string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
#if defined(AIXPPC)
			if (_printedTypes.end() != _printedTypes.find(lineToPrint)) {
				type->_isDuplicate = true;

				/* Still print the sub UDTs. This is because a previous duplicate of this UDT could have
				 * contained an empty version of the same sub UDT as this one, which was skipped over due
				 * to its emptiness.
				 */
				goto PrintSubUDTS;
			} else {
				_printedTypes.insert(lineToPrint);
			}
#endif /* defined(AIXPPC) */
			omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
		}

		for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
			printConstantMember((*m)->_name);
		}

		if (DDR_RC_OK == rc) {
			for (vector<Field *>::iterator m = type->_fieldMembers.begin(); m != type->_fieldMembers.end(); ++m) {
				rc = printFieldMember(*m, prefix);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}

		if (DDR_RC_OK == rc) {
			for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
				if (DDR_RC_OK == m->getNumeric(NULL)) {
					printConstantMember(m->_name);
				}
			}
		}
	}

PrintSubUDTS:
	if ((DDR_RC_OK == rc) && (!addFieldsOnly)) {
		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			rc = (*v)->printToSuperset(this, false, prefix);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}
	return rc;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(EnumUDT *type, bool addFieldsOnly, string prefix)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	DDR_RC rc = DDR_RC_OK;

	if ((!type->isAnonymousType() || addFieldsOnly) && (type->_enumMembers.size() > 0)) {
		if (!addFieldsOnly) {
			string nameFormatted = getUDTname(type);
			string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
#if defined(AIXPPC)
			if (_printedTypes.end() != _printedTypes.find(lineToPrint)) {
				type->_isDuplicate = true;
				return rc;
			} else {
				_printedTypes.insert(lineToPrint);
			}
#endif /* defined(AIXPPC) */
			omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
		}

		for (vector<EnumMember *>::iterator m = type->_enumMembers.begin(); m != type->_enumMembers.end(); ++m) {
			printConstantMember((*m)->_name);
		}
	}
	return rc;
}

DDR_RC
JavaSupersetGenerator::dispatchPrintToSuperset(NamespaceUDT *type, bool addFieldsOnly, string prefix)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	DDR_RC rc = DDR_RC_OK;

	if (!type->isAnonymousType() || addFieldsOnly) {
		if (!addFieldsOnly) {
			string nameFormatted = getUDTname(type);
			string lineToPrint = "S|" + nameFormatted + "|" + nameFormatted + "Pointer|\n";
#if defined(AIXPPC)
			if (_printedTypes.end() != _printedTypes.find(lineToPrint)) {
				type->_isDuplicate = true;

				/* Still print the sub UDTs. This is because a previous duplicate of this UDT could have
				 * contained an empty version of the same sub UDT as this one, which was skipped over due
				 * to its emptiness.
				 */
				goto PrintSubUDTS;
			} else {
				_printedTypes.insert(lineToPrint);
			}
#endif /* defined(AIXPPC) */
			omrfile_write(_file, lineToPrint.c_str(), lineToPrint.length());
		}

		for (vector<Macro>::iterator m = type->_macros.begin(); m != type->_macros.end(); ++m) {
			if (DDR_RC_OK == m->getNumeric(NULL)) {
				printConstantMember(m->_name);
			}
		}
	}

PrintSubUDTS:
	if (!addFieldsOnly) {
		for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
			rc = (*v)->printToSuperset(this, false, prefix);
			if (DDR_RC_OK != rc) {
				break;
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
		rc = (*v)->printToSuperset(this, false, "");
		if (DDR_RC_OK != rc) {
			break;
		}
	}
	return rc;
}
