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

#include "ddr/config.hpp"

#include "ddr/ir/Symbol_IR.hpp"

#include <assert.h>
#include <algorithm>
#include <map>
#include <stack>
#include <stdlib.h>
#include <string.h>
#include "ddr/std/unordered_map.hpp"
#include <vector>

#include "ddr/ir/ClassType.hpp"
#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/NamespaceUDT.hpp"
#include "ddr/ir/NamespaceUDT.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"

using std::map;
using std::stack;

Symbol_IR::~Symbol_IR()
{
	for (size_t i = 0; i < _types.size(); i++) {
		if (NULL == _types[i]) {
			ERRMSG("Null member, cannot free");
		} else {
			delete(_types[i]);
		}
	}
	_types.clear();
}

DDR_RC
Symbol_IR::applyOverrideList(OMRPortLibrary *portLibrary, const char *overrideFiles)
{
	DDR_RC rc = DDR_RC_OK;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* Read list of type override files from specified file. */
	intptr_t fd = omrfile_open(overrideFiles, EsOpenRead, 0);
	if (0 > fd) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", overrideFiles);
		rc = DDR_RC_ERROR;
	} else {
		char *buff = NULL;
		int64_t offset = omrfile_seek(fd, 0, EsSeekEnd);
		if (-1 != offset) {
			buff = (char *)malloc(offset + 1);
			memset(buff, 0, offset + 1);
			omrfile_seek(fd, 0, EsSeekSet);

			/* Read each line as a file name, then open that file to get type overrides. */
			if (0 < omrfile_read(fd, buff, offset)) {
				char *line = buff;
				char *nextLine = strchr(line, '\n');
				nextLine[0] = '\0';
				while ((NULL != nextLine) && (DDR_RC_OK == rc)) {
					nextLine[0] = '\0';
					rc = applyOverrides(portLibrary, line);
					line = nextLine + 1;
					nextLine = strchr(line, '\n');
				}
			}
			free(buff);
		}
	}
	return rc;
}

DDR_RC
Symbol_IR::applyOverrides(OMRPortLibrary *portLibrary, const char *overrideFile)
{
	DDR_RC rc = DDR_RC_OK;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* Read list of type overrides from specified file. */
	vector<FieldOverride> overrideList;
	intptr_t fd = omrfile_open(overrideFile, EsOpenRead, 0);
	if (0 > fd) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", overrideFile);
		rc = DDR_RC_ERROR;
	} else {
		char *buff = NULL;
		int64_t offset = omrfile_seek(fd, 0, EsSeekEnd);
		if (-1 != offset) {
			buff = (char *)malloc(offset + 1);
			memset(buff, 0, offset + 1);
			omrfile_seek(fd, 0, EsSeekSet);

			/* Read each line, expecting format of "[typeoverride/fieldoverride].[StructureName].[fieldName]=[overrideType]" */
			if (0 < omrfile_read(fd, buff, offset)) {
				char *nextLine = strtok(buff, "\n");
				while (NULL != nextLine) {
					string line = nextLine;

					/* Remove comment portion of the line, if present. */
					size_t commentPosition = line.find("#");
					if (string::npos != commentPosition) {
						line = line.substr(0, commentPosition);
					}
					line.erase(line.find_last_not_of(" \t") + 1);

					/* Ignore the prefix "ddrblob.", if present. */
					string prefix = "ddrblob.";
					if (string::npos != line.find(prefix)) {
						line = line.substr(prefix.length(), line.length());
					}

					/* Check if the override is a typeoverride or a fieldoverride. */
					string typeoverride = "typeoverride.";
					string fieldoverride = "fieldoverride.";
					bool isTypeOverride = false;
					if (string::npos != line.find(typeoverride)) {
						isTypeOverride = true;
						line = line.substr(typeoverride.length(), line.length());
					} else if (string::npos != line.find(fieldoverride)) {
						line = line.substr(fieldoverride.length(), line.length());
					}

					/* Remove any pointer or array specifiers in the line and use those the field has. */
					line.erase(remove(line.begin(), line.end(), '*'), line.end());
					line.erase(remove(line.begin(), line.end(), '['), line.end());
					line.erase(remove(line.begin(), line.end(), ']'), line.end());

					/* Correctly formatted lines must have exactly 1 dot and 1 equals. */
					size_t dotCount = count(line.begin(), line.end(), '.');
					size_t equalCount = count(line.begin(), line.end(), '=');
					if ((1 == dotCount) && (1 == equalCount)) {
						/* Get the structure name, field name, and override name. Add to list to process. */
						size_t dotPosition = line.find(".");
						size_t equalsPosition = line.find("=", dotPosition);
						FieldOverride to = {
							line.substr(0, dotPosition),
							line.substr(dotPosition + 1, equalsPosition - dotPosition - 1),
							line.substr(equalsPosition + 1, line.length() - equalsPosition - 1),
							isTypeOverride
						};
						overrideList.push_back(to);
						if (DDR_RC_OK != rc) {
							break;
						}
					}
					nextLine = strtok(NULL, "\n");
				}
			}
			free(buff);
		}
	}

	/* Create a map of type name to vector of types to check all types
	 * by name for type overrides.
	 */
	map<string, vector<Type *> > typeNames;
	for (vector<Type *>::iterator it = _types.begin(); it != _types.end(); it += 1) {
		Type *type = *it;
		typeNames[type->_name].push_back(type);
	}

	/* Apply the type overrides. */
	for (vector<FieldOverride>::iterator it = overrideList.begin(); it != overrideList.end(); it += 1) {
		FieldOverride type = *it;
		/* Check if the structure to override exists. */
		if (typeNames.find(type.structName) != typeNames.end()) {
			Type *replacementType = NULL;
			if (type.isTypeOverride) {
				/* If the type for the override exists in the IR, use it. Otherwise, create it. */
				if (typeNames.find(type.overrideName) == typeNames.end()) {
					replacementType = new Type(0);
					replacementType->_name = type.overrideName;
					typeNames[replacementType->_name].push_back(replacementType);
				} else {
					replacementType = typeNames[type.overrideName].front();
				}
			}

			/* Iterate over the types with a matching name for the override. */
			vector<Type *> *typesWithName = &typeNames[type.structName];
			for (vector<Type *>::iterator it2 = typesWithName->begin(); it2 != typesWithName->end(); it2 += 1) {
				(*it2)->renameFieldsAndMacros(type, replacementType);
			}
		}
	}

	return rc;
}

/* Compute the field offsets for all types in the IR from the sizes of the fields. */
void
Symbol_IR::computeOffsets()
{
	/* For each Type in the ir, compute the field offsets from the size of each field. */
	for (size_t i = 0; i < _types.size(); i++) {
		_types[i]->computeFieldOffsets();
	}
}

void
Symbol_IR::removeDuplicates()
{
	for (vector<Type *>::iterator it = _types.begin(); it != _types.end(); ++it) {
		(*it)->checkDuplicate(this);
	}
}

class MergeVisitor : public TypeVisitor
{
private:
	Symbol_IR *_ir;
	Type *_other;
	vector<Type *> *_merged;

public:
	MergeVisitor(Symbol_IR *ir, Type *other, vector<Type *> *merged) : _ir(ir), _other(other), _merged(merged) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
};

DDR_RC
MergeVisitor::visitType(Type *type) const
{
	/* No merging necessary for base types. */
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitType(NamespaceUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	NamespaceUDT *other = (NamespaceUDT *)_other;
	_ir->mergeTypes(type->getSubUDTS(), other->getSubUDTS(), type, _merged);
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitType(EnumUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	EnumUDT *other = (EnumUDT *)_other;
	_ir->mergeEnums(&type->_enumMembers, &other->_enumMembers);
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitType(TypedefUDT *type) const
{
	/* No merging necessary for typedefs. */
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitType(ClassUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	ClassUDT *other = (ClassUDT *)_other;
	_ir->mergeTypes(type->getSubUDTS(), other->getSubUDTS(), type, _merged);
	_ir->mergeFields(&type->_fieldMembers, &other->_fieldMembers, type, _merged);
	_ir->mergeEnums(&type->_enumMembers, &other->_enumMembers);
	if ((NULL == type->_superClass) && (NULL != other->_superClass)) {
		type->_superClass = other->_superClass;
		_merged->push_back(type);
	}
	return DDR_RC_OK;
}

DDR_RC
MergeVisitor::visitType(UnionUDT *type) const
{
	/* Merge by adding the fields/subtypes of '_other' into 'type'. */
	UnionUDT *other = (UnionUDT *)_other;
	_ir->mergeTypes(type->getSubUDTS(), other->getSubUDTS(), type, _merged);
	_ir->mergeFields(&type->_fieldMembers, &other->_fieldMembers, type, _merged);
	_ir->mergeEnums(&type->_enumMembers, &other->_enumMembers);
	return DDR_RC_OK;
}

template<typename T> void
Symbol_IR::mergeTypes(vector<T *> *source, vector<T *> *other,
	NamespaceUDT *outerNamespace, vector<Type *> *merged)
{
	for (typename vector<T *>::iterator it = other->begin(); it != other->end();) {
		Type *type = (Type *)(*it);
		Type *originalType = findTypeInMap(type);

		/* Types in the other list not in the source list are added. */
		/* Types in both lists are merged. */
		if (NULL == originalType) {
			addTypeToMap(type);
			source->push_back((T *)type);
			merged->push_back(type);
			it = other->erase(it);
			if (NULL != outerNamespace) {
				((UDT *)type)->_outerNamespace = outerNamespace;
			}
		} else {
			originalType->acceptVisitor(MergeVisitor(this, type, merged));
			++ it;
		}
	}
}

void
Symbol_IR::mergeFields(vector<Field *> *source, vector<Field *> *other, Type *type, vector<Type *> *merged)
{
	/* Create a map of all fields in the source list. */
	set<string> fieldNames;
	for (vector<Field *>::iterator it = source->begin(); it != source->end(); ++it) {
		fieldNames.insert((*it)->_name);
	}
	bool fieldsMerged = false;
	for (vector<Field *>::iterator it = other->begin(); it != other->end();) {
		/* Fields in the other list not in the source list are added. */
		if (fieldNames.end() == fieldNames.find((*it)->_name)) {
			source->push_back(*it);
			fieldNames.insert((*it)->_name);
			fieldsMerged = true;
			it = other->erase(it);
		} else {
			++ it;
		}
	}
	if (fieldsMerged) {
		merged->push_back(type);
	}
}

void
Symbol_IR::mergeEnums(vector<EnumMember *> *source, vector<EnumMember *> *other)
{
	/* Create a map of all fields in the source list. */
	set<string> enumNames;
	for (vector<EnumMember *>::iterator it = source->begin(); it != source->end(); ++it) {
		enumNames.insert((*it)->_name);
	}
	for (vector<EnumMember *>::iterator it = other->begin(); it != other->end();) {
		/* Fields in the other list not in the source list are added. */
		if (enumNames.end() == enumNames.find((*it)->_name)) {
			source->push_back(*it);
			enumNames.insert((*it)->_name);
			it = other->erase(it);
		} else {
			++ it;
		}
	}
}

class TypeReplaceVisitor : public TypeVisitor
{
private:
	Symbol_IR *_ir;

public:
	TypeReplaceVisitor(Symbol_IR *ir)
		: _ir(ir) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
};

struct TypeCheck
{
    TypeCheck(const Type* other) : _other(other) {}
    bool operator()(Type *compare) const {
		return  *compare == *_other;
	}

private:
    const Type* _other;
};

DDR_RC
TypeReplaceVisitor::visitType(Type *type) const
{
	return DDR_RC_OK;
}

DDR_RC
TypeReplaceVisitor::visitType(NamespaceUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
		rc = (*it)->acceptVisitor(TypeReplaceVisitor(_ir));
	}
	return rc;
}

DDR_RC
TypeReplaceVisitor::visitType(EnumUDT *type) const
{
	return DDR_RC_OK;
}

DDR_RC
TypeReplaceVisitor::visitType(TypedefUDT *type) const
{
	return _ir->replaceTypeUsingMap(&type->_aliasedType, type);
}

DDR_RC
TypeReplaceVisitor::visitType(ClassUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
		rc = (*it)->acceptVisitor(TypeReplaceVisitor(_ir));
	}
	if (DDR_RC_OK == rc) {
		for (vector<Field *>::iterator it = type->_fieldMembers.begin(); it != type->_fieldMembers.end() && DDR_RC_OK == rc; ++it) {
			rc = _ir->replaceTypeUsingMap(&(*it)->_fieldType, type);
		}
	}

	if (DDR_RC_OK == rc) {
		rc = _ir->replaceTypeUsingMap((Type **)&type->_superClass, type);
	}
	return rc;
}

DDR_RC
TypeReplaceVisitor::visitType(UnionUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
		rc = (*it)->acceptVisitor(TypeReplaceVisitor(_ir));
	}
	if (DDR_RC_OK == rc) {
		for (vector<Field *>::iterator it = type->_fieldMembers.begin(); it != type->_fieldMembers.end() && DDR_RC_OK == rc; ++it) {
			rc = _ir->replaceTypeUsingMap(&(*it)->_fieldType, type);
		}
	}
	return rc;
}

DDR_RC
Symbol_IR::replaceTypeUsingMap(Type **type, Type *outer)
{
	DDR_RC rc = DDR_RC_OK;
	if ((NULL != type) && (NULL != *type) && (_typeSet.end() == _typeSet.find(*type))) {
		Type *replacementType = findTypeInMap(*type);
		if (NULL == replacementType) {
			ERRMSG("Error replacing type '%s' in '%s'", (*type)->getFullName().c_str(), outer->getFullName().c_str());
			rc = DDR_RC_ERROR;
		}
		*type = replacementType;
	}
	return rc;
}

Type *
Symbol_IR::findTypeInMap(Type *typeToFind)
{
	Type *returnType = NULL;
	if (NULL != typeToFind) {
		string nameKey = typeToFind->getFullName();
		if (nameKey.empty()) {
			nameKey = typeToFind->getSymbolKindName();
		} else if (NULL != typeToFind->getBaseType()) {
			nameKey = "typedef " + nameKey;
		}

		if (_typeMap.end() != _typeMap.find(nameKey)) {
			set<Type *> s = _typeMap.find(nameKey)->second;
			if ((1 == s.size()) && (!typeToFind->_name.empty())) {
				returnType = *s.begin();
			} else if (1 < s.size()) {
				set<Type *>::iterator it = find_if(s.begin(), s.end(), TypeCheck(typeToFind));
				if (s.end() != it) {
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
	stack<vector<UDT *>::iterator> itStack;
	if ((NULL != type->getSubUDTS()) && (!type->getSubUDTS()->empty())) {
		itStack.push(type->getSubUDTS()->begin());
	}

	Type *startType = type;
	while (NULL != type) {
		string nameKey = type->getFullName();
		if (nameKey.empty()) {
			nameKey = type->getSymbolKindName();
		} else if (NULL != type->getBaseType()) {
			nameKey = "typedef " + nameKey;
		}

		_typeMap[nameKey].insert(type);
		_typeSet.insert(type);
		if (NULL == type->getSubUDTS() || type->getSubUDTS()->empty()) {
			if (startType == type) {
				type = NULL;
			} else {
				type = type->getNamespace();
			}
		} else if (type->getSubUDTS()->end() == itStack.top()) {
			if (startType == type) {
				type = NULL;
			} else {
				type = type->getNamespace();
			}
			itStack.pop();
		} else {
			type = *itStack.top();
			itStack.top() ++;
			if (NULL != type->getSubUDTS() && !type->getSubUDTS()->empty()) {
				itStack.push(type->getSubUDTS()->begin());
			}
		}
	};
}

DDR_RC
Symbol_IR::mergeIR(Symbol_IR *other)
{
	DDR_RC rc = DDR_RC_OK;
	/* Create map of this source IR of type full name to type. Use it to replace all
	 * pointers in types acquired from other IRs. The problem of encountering
	 * types to replace that are not yet in this IR is solved by doing all of the type
	 * replacement after all types are merged.
	 */
	if (_typeMap.empty() && _typeSet.empty()) {
		for (vector<Type *>::iterator it = _types.begin(); it != _types.end(); ++it) {
			addTypeToMap(*it);
		}
	}

	vector<Type *> mergedTypes;
	mergeTypes(&_types, &other->_types, NULL, &mergedTypes);

	for (vector<Type *>::iterator it = mergedTypes.begin(); it != mergedTypes.end(); ++it) {
		rc = (*it)->acceptVisitor(TypeReplaceVisitor(this));
		if (DDR_RC_OK != rc) {
			break;
		}
	}
	return rc;
}
