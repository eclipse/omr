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

#include "ddr/ir/Symbol_IR.hpp"

#include <assert.h>
#include <algorithm>
#include <map>
#include <stack>
#include <stdlib.h>
#include <string.h>
#include "ddr/std/unordered_map.hpp"
#include <vector>

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/TextFile.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"

#undef DEBUG_PRINT_TYPES

#if defined(DEBUG_PRINT_TYPES)
#include "ddr/ir/TypePrinter.hpp"
#endif

using std::map;
using std::stack;

Symbol_IR::OverrideInfo::OverrideInfo()
	: opaqueTypeNames()
	, fieldOverrides()
{
	static const char * const scalarTypes[] = {
		"char",
		"I8", "I_8", "U8", "U_8",
		"I16", "I_16", "U16", "U_16",
		"I32", "I_32", "U32", "U_32",
		"I64", "I_64", "U64", "U_64",
		"I128", "I_128", "U128", "U_128",
		"IDATA", "intptr_t", "UDATA", "uintptr_t",
		NULL
	};

	for (const char * const *cursor = scalarTypes; NULL != *cursor; ++cursor) {
		opaqueTypeNames.insert(*cursor);
	}
}

Symbol_IR::~Symbol_IR()
{
	for (std::vector<Type *>::const_iterator it = _types.begin(); it != _types.end(); ++it) {
		delete *it;
	}
	_types.clear();
}

DDR_RC
Symbol_IR::applyOverridesList(const char *overridesListFile)
{
	DDR_RC rc = DDR_RC_OK;
	vector<string> filenames;
	OverrideInfo overrideInfo;
	TextFile listFile(_portLibrary);

	/* Read list of type override files from specified file. */
	if (!listFile.openRead(overridesListFile)) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", overridesListFile);
		rc = DDR_RC_ERROR;
	} else {
		string line;

		/* Read each line as a file name. */
		while (listFile.readLine(line)) {
			size_t commentPosition = line.find("#");
			if (string::npos != commentPosition) {
				line.erase(commentPosition, line.length());
			}
			line.erase(line.find_last_not_of(" \t") + 1, line.length());
			if (!line.empty()) {
				filenames.push_back(line);
			}
		}

		listFile.close();
	}

	for (vector<string>::const_iterator it = filenames.begin(); filenames.end() != it; ++it) {
		rc = readOverridesFile(it->c_str(), &overrideInfo);
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	if (DDR_RC_OK == rc) {
		/* mark opaque types */
		for (vector<Type *>::const_iterator it = _types.begin(); it != _types.end(); ++it) {
			Type *type = *it;
			if (overrideInfo.opaqueTypeNames.find(type->_name) != overrideInfo.opaqueTypeNames.end()) {
				type->_opaque = true;
			}
		}

		/* Create a map of type name to vector of types to check all types
		 * by name for type overrides.
		 */
		map<string, vector<Type *> > typeNames;
		for (vector<Type *>::const_iterator it = _types.begin(); it != _types.end(); ++it) {
			Type *type = *it;
			typeNames[type->_name].push_back(type);
		}

		/* Apply the type overrides. */
		for (vector<FieldOverride>::const_iterator it = overrideInfo.fieldOverrides.begin(); it != overrideInfo.fieldOverrides.end(); ++it) {
			const FieldOverride &type = *it;
			/* Check if the structure to override exists. */
			map<string, vector<Type *> >::const_iterator typeEntry = typeNames.find(type.structName);
			if (typeNames.end() != typeEntry) {
				Type *replacementType = NULL;
				if (type.isTypeOverride) {
					/* If the type for the override exists in the IR, use it. Otherwise, create it. */
					map<string, vector<Type *> >::const_iterator overEntry = typeNames.find(type.overrideName);
					if (typeNames.end() != overEntry) {
						replacementType = overEntry->second.front();
					} else {
						replacementType = new Type(0);
						replacementType->_name = type.overrideName;
						typeNames[replacementType->_name].push_back(replacementType);
					}
				}

				/* Iterate over the types with a matching name for the override. */
				const vector<Type *> &typesWithName = typeEntry->second;
				for (vector<Type *>::const_iterator it2 = typesWithName.begin(); it2 != typesWithName.end(); ++it2) {
					(*it2)->renameFieldsAndMacros(type, replacementType);
				}
			}
		}
	}

	return rc;
}

static bool
startsWith(const string &str, const string &prefix)
{
	return 0 == str.compare(0, prefix.length(), prefix);
}

DDR_RC
Symbol_IR::readOverridesFile(const char *overridesFile, OverrideInfo *overrideInfo)
{
	DDR_RC rc = DDR_RC_OK;
	TextFile file(_portLibrary);

	/* Read list of type overrides from specified file. */
	if (!file.openRead(overridesFile)) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", overridesFile);
		rc = DDR_RC_ERROR;
	} else {
		const string blobprefix = "ddrblob.";
		const string opaquetype = "opaquetype=";
		const string fieldoverride = "fieldoverride.";
		const string typeoverride = "typeoverride.";
		string line;

		/*
		 * Read each line, expecting one of the following formats:
		 *   opaquetype={TypeName}
		 *   fieldoverride.{StructureName}.{fieldName}={newName}
		 *   typeoverride.{StructureName}.{fieldName}={newType}"
		 */
		while (file.readLine(line)) {
			/* Remove comment portion of the line, if present. */
			size_t commentPosition = line.find("#");
			if (string::npos != commentPosition) {
				line.erase(commentPosition, line.length());
			}
			line.erase(line.find_last_not_of(" \t") + 1, line.length());

			/* Ignore the prefix "ddrblob.", if present. */
			if (startsWith(line, blobprefix)) {
				line.erase(0, blobprefix.length());
			}

			/* Check for opaque types. */
			if (startsWith(line, opaquetype)) {
				line.erase(0, opaquetype.length());
				if (!line.empty()) {
					overrideInfo->opaqueTypeNames.insert(line);
				}
				continue;
			}

			/* Check if the override is a typeoverride or a fieldoverride. */
			bool isTypeOverride = false;
			if (startsWith(line, fieldoverride)) {
				line.erase(0, fieldoverride.length());
			} else if (startsWith(line, typeoverride)) {
				isTypeOverride = true;
				line.erase(0, typeoverride.length());
			} else {
				continue;
			}

			/* Correctly formatted lines must have exactly 1 dot and 1 equals in that order. */
			size_t dotPosition = line.find('.');
			if ((string::npos == dotPosition) || (string::npos != line.find('.', dotPosition + 1))) {
				/* not exactly one '.' */
				continue;
			}
			size_t equalsPosition = line.find('=', dotPosition);
			if ((string::npos == equalsPosition) || (string::npos != line.find('=', equalsPosition + 1))) {
				/* not exactly one '=' after the '.' */
				continue;
			}
			/* Get the structure name, field name, and override name. Add to list to process. */
			FieldOverride override = {
					line.substr(0, dotPosition),
					line.substr(dotPosition + 1, equalsPosition - dotPosition - 1),
					line.substr(equalsPosition + 1),
					isTypeOverride
			};
			overrideInfo->fieldOverrides.push_back(override);
		}

		file.close();
	}

	return rc;
}

void
Symbol_IR::removeDuplicates()
{
	for (vector<Type *>::iterator it = _types.begin(); it != _types.end();) {
		if ((*it)->insertUnique(this)) {
			++it;
		} else {
			it = _types.erase(it);
		}
	}
}

class MergeVisitor : public TypeVisitor
{
private:
	Symbol_IR * const _ir;
	Type * const _other;
	vector<Type *> *const _merged;

public:
	MergeVisitor(Symbol_IR *ir, Type *other, vector<Type *> *merged)
		: _ir(ir), _other(other), _merged(merged)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;

private:
	DDR_RC visitComposite(ClassType *type) const;
};

DDR_RC
MergeVisitor::visitType(Type *type) const
{
	/* No merging necessary for base types. */
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitNamespace(NamespaceUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	NamespaceUDT *other = (NamespaceUDT *)_other;
	/* This type may be derived from only a forward declaration; if so, update the size now. */
	if (0 == type->_sizeOf) {
		type->_sizeOf = other->_sizeOf;
	}
	_ir->mergeEnums(&type->_enumMembers, &other->_enumMembers);
	_ir->mergeTypes(type->getSubUDTS(), other->getSubUDTS(), type, _merged);
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitEnum(EnumUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	EnumUDT *other = (EnumUDT *)_other;
	/* This type may be derived from only a forward declaration; if so, update the size now. */
	if (0 == type->_sizeOf) {
		type->_sizeOf = other->_sizeOf;
	}
	_ir->mergeEnums(&type->_enumMembers, &other->_enumMembers);
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitTypedef(TypedefUDT *type) const
{
	/* No merging necessary for typedefs. */
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitComposite(ClassType *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	visitNamespace(type);
	ClassType *other = (ClassType *)_other;
	_ir->mergeFields(&type->_fieldMembers, &other->_fieldMembers, type, _merged);
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitClass(ClassUDT *type) const
{
	if (NULL == type->_superClass) {
		ClassUDT *otherSuper = ((ClassUDT *)_other)->_superClass;

		if (NULL != otherSuper) {
			Type *thisSuper = _ir->findTypeInMap(otherSuper);

			if (NULL != thisSuper) {
				otherSuper = (ClassUDT *)thisSuper;
			} else {
				_merged->push_back(otherSuper);
			}
			type->_superClass = otherSuper;
		}
	}

	return visitComposite(type);
}

DDR_RC
MergeVisitor::visitUnion(UnionUDT *type) const
{
	return visitComposite(type);
}

template<typename T> void
Symbol_IR::mergeTypes(vector<T *> *source, vector<T *> *other,
		NamespaceUDT *outerNamespace, vector<Type *> *merged)
{
	if ((NULL != source) && (NULL != other)) {
		for (typename vector<T *>::iterator it = other->begin(); it != other->end();) {
			Type *otherType = (Type *)*it;
			Type *sourceType = findTypeInMap(otherType);

			/* Types in the other list not in the source list are added. */
			/* Types in both lists are merged. */
			if (NULL != sourceType) {
				sourceType->acceptVisitor(MergeVisitor(this, otherType, merged));
				++it;
			} else {
				it = other->erase(it);
				source->push_back((T *)otherType);
				if (NULL != outerNamespace) {
					((UDT *)otherType)->_outerNamespace = outerNamespace;
				}
				addTypeToMap(otherType);
				merged->push_back(otherType);
			}
		}
	}
}

void
Symbol_IR::mergeFields(vector<Field *> *source, vector<Field *> *other, Type *type, vector<Type *> *merged)
{
	/* Create a map of all fields in the source list. */
	set<string> fieldNames;
	for (vector<Field *>::const_iterator it = source->begin(); it != source->end(); ++it) {
		fieldNames.insert((*it)->_name);
	}
	bool fieldsMerged = false;
	for (vector<Field *>::iterator it = other->begin(); it != other->end();) {
		/* Fields in the other list not in the source list are added. */
		Field * field = *it;
		const string &fieldName = field->_name;
		if (fieldNames.end() == fieldNames.find(fieldName)) {
			it = other->erase(it);
			source->push_back(field);
			fieldNames.insert(fieldName);
			fieldsMerged = true;
		} else {
			++it;
		}
	}
	if (fieldsMerged) {
		/* We must revisit 'type' to fix the type of the newly added fields. */
		merged->push_back(type);
	}
}

void
Symbol_IR::mergeEnums(vector<EnumMember *> *source, vector<EnumMember *> *other)
{
	/* Create a map of all fields in the source list. */
	set<string> enumNames;
	for (vector<EnumMember *>::const_iterator it = source->begin(); it != source->end(); ++it) {
		enumNames.insert((*it)->_name);
	}
	for (vector<EnumMember *>::iterator it = other->begin(); it != other->end();) {
		EnumMember *member = *it;
		const string & memberName = (*it)->_name;
		/* Fields in the other list not in the source list are added. */
		if (enumNames.end() == enumNames.find(memberName)) {
			it = other->erase(it);
			source->push_back(member);
			enumNames.insert(memberName);
		} else {
			++it;
		}
	}
}

class TypeReplaceVisitor : public TypeVisitor
{
private:
	Symbol_IR * const _ir;

public:
	explicit TypeReplaceVisitor(Symbol_IR *ir)
		: _ir(ir)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;

private:
	DDR_RC visitComposite(ClassType *type) const;
};

class TypeCheck
{
private:
    const Type * const _other;

public:
    explicit TypeCheck(const Type *other)
    	: _other(other)
    {
    }

    bool operator()(Type *compare) const {
		return *compare == *_other;
	}
};

DDR_RC
TypeReplaceVisitor::visitType(Type *type) const
{
	return DDR_RC_OK;
}

DDR_RC
TypeReplaceVisitor::visitNamespace(NamespaceUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;

	for (vector<UDT *>::const_iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
		rc = (*it)->acceptVisitor(*this);
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	return rc;
}

DDR_RC
TypeReplaceVisitor::visitEnum(EnumUDT *type) const
{
	return DDR_RC_OK;
}

DDR_RC
TypeReplaceVisitor::visitTypedef(TypedefUDT *type) const
{
	return _ir->replaceTypeUsingMap(&type->_aliasedType, type);
}

DDR_RC
TypeReplaceVisitor::visitComposite(ClassType *type) const
{
	DDR_RC rc = visitNamespace(type);

	if (DDR_RC_OK == rc) {
		for (vector<Field *>::const_iterator it = type->_fieldMembers.begin(); it != type->_fieldMembers.end(); ++it) {
			rc = _ir->replaceTypeUsingMap(&(*it)->_fieldType, type);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}

	return rc;
}

DDR_RC
TypeReplaceVisitor::visitClass(ClassUDT *type) const
{
	DDR_RC rc = visitComposite(type);

	if (DDR_RC_OK == rc) {
		rc = _ir->replaceTypeUsingMap((Type **)&type->_superClass, type);
	}

	return rc;
}

DDR_RC
TypeReplaceVisitor::visitUnion(UnionUDT *type) const
{
	return visitComposite(type);
}

DDR_RC
Symbol_IR::replaceTypeUsingMap(Type **type, Type *outer)
{
	DDR_RC rc = DDR_RC_OK;
	if (NULL != type) {
		Type *oldType = *type;
		if ((NULL != oldType) && (_typeSet.end() == _typeSet.find(oldType))) {
			Type *replacementType = findTypeInMap(oldType);
			if (NULL == replacementType) {
				ERRMSG("Error replacing type '%s' in '%s'",
						oldType->getFullName().c_str(),
						outer->getFullName().c_str());
				rc = DDR_RC_ERROR;
			} else if (oldType == replacementType) {
				/* don't create a cycle */
				ERRMSG("Cycle found replacing type '%s' in '%s'",
						oldType->getFullName().c_str(),
						outer->getFullName().c_str());
				rc = DDR_RC_ERROR;
				*type = NULL;
			} else {
				*type = replacementType;
			}
		}
	}
	return rc;
}

Type *
Symbol_IR::findTypeInMap(Type *typeToFind)
{
	Type *returnType = NULL;
	if ((NULL != typeToFind) && !typeToFind->isAnonymousType()) {
		const string nameKey = typeToFind->getTypeNameKey();
		unordered_map<string, set<Type *> >::const_iterator map_it = _typeMap.find(nameKey);

		if (_typeMap.end() != map_it) {
			const set<Type *> &matches = map_it->second;
			if (1 == matches.size()) {
				returnType = *matches.begin();
			} else if (1 < matches.size()) {
				set<Type *>::const_iterator it = find_if(matches.begin(), matches.end(), TypeCheck(typeToFind));
				if (matches.end() != it) {
					returnType = *it;
				}
			}
		}
	}
	return returnType;
}

void
Symbol_IR::addTypeToMap(Type *type)
{
	Type * const startType = type;
	stack<vector<UDT *>::const_iterator> itStack;
	vector<UDT *> *subs = NULL;

	subs = type->getSubUDTS();
	if ((NULL != subs) && !subs->empty()) {
		itStack.push(subs->begin());
	}

	while (NULL != type) {
		if (!type->isAnonymousType()) {
			_typeMap[type->getTypeNameKey()].insert(type);
		}
		_typeSet.insert(type);

		subs = type->getSubUDTS();

		if (NULL == subs || subs->empty()) {
			if (startType == type) {
				type = NULL;
			} else {
				type = type->getNamespace();
			}
		} else if (subs->end() == itStack.top()) {
			if (startType == type) {
				type = NULL;
			} else {
				type = type->getNamespace();
			}
			itStack.pop();
		} else {
			type = *itStack.top();
			itStack.top()++;
			subs = type->getSubUDTS();
			if ((NULL != subs) && !subs->empty()) {
				itStack.push(subs->begin());
			}
		}
	}
}

#if defined(DEBUG_PRINT_TYPES)
static Symbol_IR *previousIR = NULL;
#endif /* DEBUG_PRINT_TYPES */

DDR_RC
Symbol_IR::mergeIR(Symbol_IR *other)
{
#if defined(DEBUG_PRINT_TYPES)
	TypePrinter printer(_portLibrary, TypePrinter::LITERALS);

	if (this != previousIR) {
		printf("\n");
		printf("== Merge before ==\n");
		printer.printUDTs(_types);
		previousIR = this;
	}
	printf("\n");
	printf("== Merge source ==\n");
	printer.printUDTs(other->_types);
#endif /* DEBUG_PRINT_TYPES */

	DDR_RC rc = DDR_RC_OK;

	/* Create map of this source IR of type full name to type. Use it to replace all
	 * pointers in types acquired from other IRs. The problem of encountering
	 * types to replace that are not yet in this IR is solved by doing all of the type
	 * replacement after all types are merged.
	 */
	if (_typeMap.empty() && _typeSet.empty()) {
		for (vector<Type *>::const_iterator it = _types.begin(); it != _types.end(); ++it) {
			addTypeToMap(*it);
		}
	}

	vector<Type *> mergedTypes;

	mergeTypes(&_types, &other->_types, NULL, &mergedTypes);

	const TypeReplaceVisitor typeReplacer(this);

	for (vector<Type *>::const_iterator it = mergedTypes.begin(); it != mergedTypes.end(); ++it) {
		rc = (*it)->acceptVisitor(typeReplacer);
		if (DDR_RC_OK != rc) {
			break;
		}
	}

#if defined(DEBUG_PRINT_TYPES)
	printf("\n");
	printf("== Merge result ==\n");
	printer.printUDTs(_types);
#endif /* DEBUG_PRINT_TYPES */

	return rc;
}
