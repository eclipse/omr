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

#ifndef GENSUPERSET_HPP
#define GENSUPERSET_HPP

#include "ddr/blobgen/genBlob.hpp"

class Field;
class SupersetFieldVisitor;
class SupersetVisitor;
class Symbol_IR;

class JavaSupersetGenerator : public SupersetGenerator
{
private:
	intptr_t _file;
	OMRPortLibrary *_portLibrary;
	bool _printEmptyTypes;
	string _pendingTypeHeading;

	void convertJ9BaseTypedef(Type *type, string *name);
	void replaceBaseTypedef(Type *type, string *name);
	DDR_RC getFieldType(Field *field, string *assembledTypeName, string *simpleTypeName);
	DDR_RC printType(Type *type, Type *superClass);
	DDR_RC printPendingType();
	DDR_RC printFieldMember(Field *field, const string &prefix);
	DDR_RC printConstantMember(const string &name);
	DDR_RC print(const string &text);

	friend class SupersetFieldVisitor;
	friend class SupersetVisitor;

public:
	explicit JavaSupersetGenerator(bool printEmptyTypes);

	DDR_RC printSuperset(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *supersetFile);
};

#endif /* GENSUPERSET_HPP */
