/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "ddr/ir/Type.hpp"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

Type::Type(size_t size)
	: _blacklisted(false)
	, _opaque(true)
	, _name()
	, _sizeOf(size)
{
}

Type::~Type()
{
}

bool
Type::isAnonymousType() const
{
	return _name.empty();
}

string
Type::getFullName() const
{
	return _name;
}

const string &
Type::getSymbolKindName() const
{
	static const string typeKind("");

	return typeKind;
}

string
Type::getTypeNameKey() const
{
	string prefix = getSymbolKindName();
	string fullname = getFullName();

	return prefix.empty() ? fullname : (prefix + " " + fullname);
}

DDR_RC
Type::acceptVisitor(const TypeVisitor &visitor)
{
	return visitor.visitType(this);
}

bool
Type::insertUnique(Symbol_IR *ir)
{
	/* No-op: since Types aren't printed, there's no need to check if they're duplicates either. */
	return false;
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
Type::addMacro(Macro *macro)
{
	/* No-op: macros cannot be associated with base types. */
}

vector<UDT *> *
Type::getSubUDTS()
{
	return NULL;
}

void
Type::renameFieldsAndMacros(const FieldOverride &fieldOverride, Type *replacementType)
{
	/* No-op: base types have no fields. */
}

Type *
Type::getBaseType()
{
	return NULL;
}

Type *
Type::getOpaqueType()
{
	Type *type = this;
	set<Type *> visitedTypes;

	/* stop if we find a type that should treated as opaque */
	while (!type->_opaque) {
		Type * baseType = type->getBaseType();

		if (NULL == baseType) {
			/* this is the end of the chain */
			break;
		}

		visitedTypes.insert(type);

		if (visitedTypes.find(baseType) != visitedTypes.end()) {
			/* this signals a cycle in the chain of base types */
			break;
		}

		type = baseType;
	}

	return type;
}

bool
Type::operator==(const Type & rhs) const
{
	return rhs.compareToType(*this);
}

bool
Type::compareToType(const Type &other) const
{
	return (_name == other._name)
		&& ((0 == _sizeOf) || (0 == other._sizeOf) || (_sizeOf == other._sizeOf));
}

bool
Type::compareToUDT(const UDT &) const
{
	return false;
}

bool
Type::compareToNamespace(const NamespaceUDT &) const
{
	return false;
}

bool
Type::compareToEnum(const EnumUDT &) const
{
	return false;
}

bool
Type::compareToTypedef(const TypedefUDT &) const
{
	return false;
}

bool
Type::compareToClasstype(const ClassType &) const
{
	return false;
}

bool
Type::compareToUnion(const UnionUDT &) const
{
	return false;
}

bool
Type::compareToClass(const ClassUDT &) const
{
	return false;
}

enum TypeKind
{
	TK_char,
	TK_short,
	TK_int,
	TK_long,
	TK_signed,
	TK_unsigned,
	TK_const,
	TK_volatile,
	TK_std_signed,
	TK_std_unsigned,

	/* the number of type kinds */
	TK_count
};

struct TypeWord
{
	const char *name;
	size_t nameLen;
	size_t bitWidth;
	TypeKind typeKind;
};

#define TYPE_ENTRY(name, type, typeKind) \
	{ (name), sizeof(name) - 1, 8 * sizeof(type), (typeKind) }

#define TYPE_QUAL(type, typeKind) \
	{ #type, sizeof(#type) - 1, 0, (typeKind) }

#define TYPE_WORD(type, typeKind) \
	{ #type, sizeof(#type) - 1, 8 * sizeof(type), (typeKind) }

static const TypeWord typeWords[] = {
	/* built-in types and modifiers */
	TYPE_WORD(char,     TK_char),
	TYPE_WORD(short,    TK_short),
	TYPE_WORD(int,      TK_int),
	TYPE_WORD(long,     TK_long),
	TYPE_WORD(signed,   TK_signed),
	TYPE_WORD(unsigned, TK_unsigned),
	TYPE_QUAL(const,    TK_const),
	TYPE_QUAL(volatile, TK_volatile),

	/* standard signed types */
	TYPE_WORD(int8_t,  TK_std_signed),
	TYPE_WORD(int16_t, TK_std_signed),
	TYPE_WORD(int32_t, TK_std_signed),
	TYPE_WORD(int64_t, TK_std_signed),

	/* standard unsigned types */
	TYPE_WORD(uint8_t,  TK_std_unsigned),
	TYPE_WORD(uint16_t, TK_std_unsigned),
	TYPE_WORD(uint32_t, TK_std_unsigned),
	TYPE_WORD(uint64_t, TK_std_unsigned),

	/* standard pointer types */
	TYPE_WORD(intptr_t,  TK_std_signed),
	TYPE_WORD(uintptr_t, TK_std_unsigned),

	/* other known signed types */
	TYPE_ENTRY("__int8_t",  int8_t,   TK_std_signed),
	TYPE_ENTRY("__int16_t", int16_t,  TK_std_signed),
	TYPE_ENTRY("__int32_t", int32_t,  TK_std_signed),
	TYPE_ENTRY("__int64_t", int64_t,  TK_std_signed),
	TYPE_ENTRY("I_8",       int8_t,   TK_std_signed),
	TYPE_ENTRY("I_16",      int16_t,  TK_std_signed),
	TYPE_ENTRY("I_32",      int32_t,  TK_std_signed),
	TYPE_ENTRY("I_64",      int64_t,  TK_std_signed),
/*  TYPE_ENTRY("I_128",     int128_t, TK_std_signed), */

	/* other known unsigned types */
	TYPE_ENTRY("__uint8_t",  uint8_t,   TK_std_unsigned),
	TYPE_ENTRY("__uint16_t", uint16_t,  TK_std_unsigned),
	TYPE_ENTRY("__uint32_t", uint32_t,  TK_std_unsigned),
	TYPE_ENTRY("__uint64_t", uint64_t,  TK_std_unsigned),
	TYPE_ENTRY("U_8",        uint8_t,   TK_std_unsigned),
	TYPE_ENTRY("U_16",       uint16_t,  TK_std_unsigned),
	TYPE_ENTRY("U_32",       uint32_t,  TK_std_unsigned),
	TYPE_ENTRY("U_64",       uint64_t,  TK_std_unsigned),
/*  TYPE_ENTRY("U_128",      uint128_t, TK_std_unsigned), */

	/* terminator */
	{ NULL, 0, false }
};

#undef TYPE_ENTRY
#undef TYPE_QUAL
#undef TYPE_WORD

bool
Type::isStandardType(const char *type, size_t typeLen, bool *isSigned, size_t *bitWidth)
{
	const char * const typeEnd = type + typeLen;
	size_t bits = 0;
	uint32_t num[TK_count];

	memset(num, 0, sizeof(num));

	/*
	 * C allows types and modifiers in any order, so we count the number of
	 * occurrences of each word to verify the combination is reasonable.
	 */
	for (const char * cursor = type; cursor < typeEnd;) {
		if (isspace(*cursor)) {
			cursor += 1;
			continue;
		}

		const char * const word = cursor;

		while ((cursor < typeEnd) && !isspace(*cursor)) {
			cursor += 1;
		}

		const size_t wordLen = (size_t)(cursor - word);

		for (const TypeWord *typeWord = typeWords;; ++typeWord) {
			if (NULL == typeWord->name) {
				/* unrecognized word */
				goto fail;
			}

			if (typeWord->nameLen != wordLen) {
				/* length mismatch */
				continue;
			}

			if (0 != strncmp(word, typeWord->name, wordLen)) {
				/* name mismatch */
				continue;
			}

			/* count the occurrence of this word */
			num[typeWord->typeKind] += 1;

			/* only 'long' can be repeated and then only twice */
			const size_t maxCount = (TK_long == typeWord->typeKind) ? 2 : 1;

			if (num[typeWord->typeKind] > maxCount) {
				/* too many occurrences of this word */
				goto fail;
			}

			/*
			 * Capture the width of a standard type and whether it is signed or
			 * unsigned. This also disallows the combination of a standard type
			 * and signed or unsigned.
			 */
			if (TK_std_signed == typeWord->typeKind) {
				bits = typeWord->bitWidth;
				num[TK_signed] += 1;
			} else if (TK_std_unsigned == typeWord->typeKind) {
				bits = typeWord->bitWidth;
				num[TK_unsigned] += 1;
			}

			break;
		}
	}

	if (num[TK_signed] + num[TK_unsigned] > 1) {
		/* at most one of 'signed' or 'unsigned' is allowed */
		goto fail;
	} else if (0 != num[TK_std_signed]) {
		/* at most one standard type is allowed */
		if (0 == num[TK_std_unsigned]) {
			/* standard types cannot be combined with built-in types */
			if (0 == (num[TK_char] + num[TK_short] + num[TK_int] + num[TK_long])) {
				goto pass;
			}
		}
	} else if (0 != num[TK_std_unsigned]) {
		/* standard types cannot be combined with built-in types */
		if (0 == (num[TK_char] + num[TK_short] + num[TK_int] + num[TK_long])) {
			goto pass;
		}
	} else if ((1 == num[TK_char]) && (0 == num[TK_short]) && (0 == num[TK_int]) && (0 == num[TK_long])) {
		/* char is unsigned unless explicitly 'signed' */
		if (0 == num[TK_signed]) {
			num[TK_unsigned] = 1;
		}
		bits = 8 * sizeof(char);
		goto pass;
	} else if ((0 == num[TK_char]) && (1 == num[TK_short]) && (1 >= num[TK_int]) && (0 == num[TK_long])) {
		bits = 8 * sizeof(short);
		goto pass;
	} else if ((0 == num[TK_char]) && (0 == num[TK_short]) && (1 == num[TK_int]) && (0 == num[TK_long])) {
		bits = 8 * sizeof(int);
		goto pass;
	} else if ((0 == num[TK_char]) && (0 == num[TK_short]) && (1 >= num[TK_int]) && (1 == num[TK_long])) {
		bits = 8 * sizeof(long);
		goto pass;
	} else if ((0 == num[TK_char]) && (0 == num[TK_short]) && (1 >= num[TK_int]) && (2 == num[TK_long])) {
		bits = 8 * sizeof(long long);
		goto pass;
	}

fail:
	/* we found an unreasonable combination of keywords */
	return false;

pass:
	/* types are signed unless explicitly 'unsigned' */
	*isSigned = (0 == num[TK_unsigned]) ? true : false;
	*bitWidth = bits;
	return true;
}
