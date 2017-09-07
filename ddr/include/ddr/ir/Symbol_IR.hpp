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

#ifndef SYMBOL_IR_HPP
#define SYMBOL_IR_HPP

#include "ddr/config.hpp"

#include <vector>
#include <set>
#include "ddr/std/unordered_map.hpp"

#include "omrport.h"

#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/Field.hpp"
#include "ddr/ir/Type.hpp"

using std::vector;
using std::set;

struct FieldOverride {
	string structName;
	string fieldName;
	string overrideName;
	bool isTypeOverride;
};

class Field;
class NamespaceUDT;

class Symbol_IR {
public:
	vector<Type *> _types;
	/* Keep a set of names of structures already printed, to avoid printing duplicates in the superset. 
	 * Currently, only use this approach for AIX, where removeDuplicates() runs too slowly. Using this
	 * method on other platforms has not been tested yet. There may still be issues related to differences
	 * in the DWARF/intermediate representation structures between platforms which may be revealed by this
	 * approach.
	 */
	set<string> _fullTypeNames;
	set<Type *> _typeSet;
	unordered_map<string, set<Type *> > _typeMap;

	Symbol_IR() {}
	~Symbol_IR();

	DDR_RC applyOverrideList(OMRPortLibrary *portLibrary, const char *overrideFiles);
	void computeOffsets();
	void removeDuplicates();
	DDR_RC mergeIR(Symbol_IR *other);

private:
	DDR_RC applyOverrides(OMRPortLibrary *portLibrary, const char *overrideFile);
	template<typename T> void mergeTypes(vector<T *> *source, vector<T *> *other,
		NamespaceUDT *outerNamespace, vector<Type *> *merged);
	void mergeFields(vector<Field *> *source, vector<Field *> *other, Type *type, vector<Type *> *merged);
	void mergeEnums(vector<EnumMember *> *source, vector<EnumMember *> *other);
	void addTypeToMap(Type *type);
	Type *findTypeInMap(Type *typeToFind);
	DDR_RC replaceTypeUsingMap(Type **type, Type *outer);

	friend class MergeVisitor;
	friend class TypeReplaceVisitor;
};

#endif /* SYMBOL_IR_HPP */
