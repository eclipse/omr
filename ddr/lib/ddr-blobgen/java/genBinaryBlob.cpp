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

#include "ddr/blobgen/java/genBinaryBlob.hpp"
#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Field.hpp"
#include "ddr/ir/Macro.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TypeVisitor.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"
#include "ddr/std/sstream.hpp"

#undef NDEBUG

#include <assert.h>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

using std::string;
using std::pair;
using std::make_pair;
using std::vector;
using std::stringstream;

#define INVALID_OFFSET (~(uint32_t)0)

class BlobBuildVisitor : public TypeVisitor
{
private:
	JavaBlobGenerator * const _gen;
	const bool _addFieldsOnly;
	const size_t _baseOffset;
	const string _prefix;

public:
	explicit BlobBuildVisitor(JavaBlobGenerator *gen)
		: _gen(gen)
		, _addFieldsOnly(false)
		, _baseOffset(0)
		, _prefix()
	{
	}

	BlobBuildVisitor(JavaBlobGenerator *gen, size_t baseOffset, const string &prefix)
		: _gen(gen)
		, _addFieldsOnly(true)
		, _baseOffset(baseOffset)
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

class BlobEnumerateVisitor : public TypeVisitor
{
private:
	JavaBlobGenerator * const _gen;
	const bool _addFieldsOnly;

public:
	BlobEnumerateVisitor(JavaBlobGenerator *gen, bool addFieldsOnly)
		: _gen(gen), _addFieldsOnly(addFieldsOnly)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;
};

void
JavaBlobGenerator::BuildBlobInfo::initBlobHeader()
{
	memset(&header, 0, sizeof(header));

	header.version = 1;
	header.sizeofBool = sizeof(bool); /* TODO what if cross-compiled? */
	header.sizeofUDATA = sizeof(UDATA); /* TODO what if cross-compiled? */
	initializeBitfieldFormat(&header.bitfieldFormat);
	header.structDataSize = 0;
	header.stringDataSize = 0;
	header.structureCount = 0;
}

DDR_RC
JavaBlobGenerator::BuildBlobInfo::initializeBitfieldFormat(uint8_t *bitfieldFormat)
{
	BitField bf;
	DDR_RC rc = DDR_RC_OK;
	uint32_t slot0;
	uint32_t slot1;

	memset(&bf, 0, sizeof(bf));

	bf.x1 = 1;			/* 1 */
	bf.x2 = 31768;		/* 111110000011000 */
	bf.x3 = 3;			/* 000011 */
	bf.x4 = 777;		/* 1100001001 */

	bf.x5 = 43;			/* 0101011 */
	bf.x6 = 298;		/* 100101010 */
	bf.x7 = 12;			/* 1100 */
	bf.x8 = 1654;		/* 011001110110 */

	slot0 = ((uint32_t *)&bf)[0];
	slot1 = ((uint32_t *)&bf)[1];
	if ((slot0 == 0xC243F831) && (slot1 == 0x676C952B)) {
		*bitfieldFormat = 1;
	} else if ((slot0 == 0xFC180F09) && (slot1 == 0x572AC676)) {
		*bitfieldFormat = 2;
	} else {
		ERRMSG("Unable to determine bitfield format from %08X %08X\n", slot0, slot1);
		rc = DDR_RC_ERROR;
	}

	return rc;
}

typedef struct StringTableEntry {
	const char *cString;
	uint32_t offset;
} StringTableEntry;

static uintptr_t
stringTableHash(void *key, void *userData)
{
	const char *s = ((StringTableEntry *)key)->cString;
	const size_t length = strlen(s);
	uintptr_t hash = 0;
	const uint8_t *data = (uint8_t *)s;
	const uint8_t *end = data + length;

	while (data < end) {
		uint16_t c = 0;

		data += decodeUTF8Char(data, &c);
		hash = (hash << 5) - hash + c;
	}

	return hash;
}

static uintptr_t
stringTableEquals(void *leftKey, void *rightKey, void *userData)
{
	const char *left_s = ((const StringTableEntry *)leftKey)->cString;
	const char *right_s = ((const StringTableEntry *)rightKey)->cString;

	return (left_s == right_s) || (0 == strcmp(left_s, right_s));
}

static string
getBlobFullName(Type *type)
{
	string blobName = type->getFullName();

	for (size_t i = 0;;) {
		i = blobName.find("::", i);
		if (string::npos == i) {
			break;
		}
		blobName.replace(i, 2, "__");
	}

	return blobName;
}

void
JavaBlobGenerator::copyStringTable()
{
	uint8_t * const stringData = _buildInfo.stringBuffer;
	J9HashTableState state;

	/* Iterate the table and copy each string to its already-assigned offset. */
	StringTableEntry *entry = (StringTableEntry *)hashTableStartDo(_buildInfo.stringHash, &state);
	while (NULL != entry) {
		J9UTF8 *utf = (J9UTF8 *)(stringData + entry->offset);
		const char *cString = entry->cString;
		const size_t len = strlen(cString);

		utf->length = (uint16_t)len;
		memcpy(utf->data, cString, len);
		free((char *)cString);

		entry = (StringTableEntry *)hashTableNextDo(&state);
	}
}

DDR_RC
JavaBlobGenerator::stringTableOffset(BlobHeader *blobHeader, J9HashTable *stringTable, const char *cString, uint32_t *offset)
{
	DDR_RC rc = DDR_RC_OK;
	StringTableEntry exemplar;

	*offset = INVALID_OFFSET;

	exemplar.cString = cString;
	exemplar.offset = INVALID_OFFSET;

	/* Add will return an existing entry if one matches, NULL on allocation failure, or the new node */
	StringTableEntry *entry = (StringTableEntry *)hashTableAdd(stringTable, &exemplar);

	if (NULL == entry) {
		/* the string was new, but memory allocation failed */
		ERRMSG("Unable to allocate memory for string table entry: %s", cString);
		rc = DDR_RC_ERROR;
	} else if (INVALID_OFFSET != entry->offset) {
		/* an existing entry was found */
		*offset = entry->offset;
	} else {
		/* the offset is INVALID_OFFSET which indicates that a new entry was added */
		entry->offset = blobHeader->stringDataSize;

		/* For new string entries, allocate memory to hold the string */
		char *buffer = (char *)malloc(strlen(cString) + 1);
		if (NULL == buffer) {
			ERRMSG("Unable to allocate memory for string table entry: %s", cString);
			rc = DDR_RC_ERROR;
		} else {
			strcpy(buffer, cString);
			entry->cString = buffer;

			/* Reserve space for the uint16_t size, the string data, and the padding byte, if necessary */
			blobHeader->stringDataSize += (uint32_t)((sizeof(uint16_t) + strlen(cString) + 1) & ~(size_t)1);
			*offset = entry->offset;
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::genBinaryBlob(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *blobFile)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* iterate ir:
	 * - count structs - update blob header
	 * - build string hash table
	 */
	DDR_RC rc = countStructsAndStrings(ir);

	if (DDR_RC_OK == rc) {
		/* compute offsets for each entry of string hash table
		 * compute size of string data - update blob header
		 */
		rc = buildBlobData(portLibrary, ir);
	}

	intptr_t fd = -1;
	if (DDR_RC_OK == rc) {
		/* open blob file */
		fd = omrfile_open(blobFile, EsOpenCreate | EsOpenWrite | EsOpenAppend | EsOpenTruncate, 0644);
		_buildInfo.fd = fd;
		if (-1 == fd) {
			ERRMSG("Failed to open file blob.dat for writing.\n");
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		/* write blob header */
		intptr_t wb = omrfile_write(fd, &_buildInfo.header, sizeof(_buildInfo.header));
		assert(wb == sizeof(_buildInfo.header));

		/* write structs */
		uint32_t f_ix = 0;
		uint32_t c_ix = 0;
		intptr_t amountWritten = 0;
		for (uint32_t s_ix = 0; s_ix < _buildInfo.header.structureCount; ++s_ix) {
			wb = omrfile_write(fd, &_buildInfo.blobStructs[s_ix], sizeof(BlobStruct));
			amountWritten += wb;

			/* write fields */
			wb = omrfile_write(fd, &_buildInfo.blobFields[f_ix], sizeof(BlobField) * _buildInfo.blobStructs[s_ix].fieldCount);
			amountWritten += wb;
			f_ix += _buildInfo.blobStructs[s_ix].fieldCount;

			/* write constants */
			wb = omrfile_write(fd, &_buildInfo.blobConsts[c_ix], sizeof(BlobConstant) * _buildInfo.blobStructs[s_ix].constantCount);
			amountWritten += wb;
			c_ix += _buildInfo.blobStructs[s_ix].constantCount;
		}

		if (amountWritten != (intptr_t)_buildInfo.header.structDataSize) {
			ERRMSG("Wrote %u fields, %u constants, %u structs.\nCounted %u fields, %u constants, %u structures.\n",
				   f_ix, c_ix, (uint32_t)(_buildInfo.curBlobStruct - _buildInfo.blobStructs),
				   _buildInfo.fieldCount, _buildInfo.constCount, _buildInfo.header.structureCount);
			ERRMSG("Expected %u to be written, but %u was written.\n",
					(uint32_t)_buildInfo.header.structDataSize, (uint32_t)amountWritten);
		}

		/* write string data */
		wb = omrfile_write(fd, _buildInfo.stringBuffer, _buildInfo.header.stringDataSize);

		/* close blob file */
		omrfile_close(fd);

		free(_buildInfo.blobStructs);
		free(_buildInfo.blobFields);
		free(_buildInfo.blobConsts);
		free(_buildInfo.stringBuffer);
		hashTableFree(_buildInfo.stringHash);
	}

	return rc;
}

/* iterate ir:
 * - count structs - update blob header
 * - build string hash table
 */
DDR_RC
JavaBlobGenerator::countStructsAndStrings(Symbol_IR *ir)
{
	DDR_RC rc = DDR_RC_OK;
	const BlobEnumerateVisitor enumerator(this, false);

	/* just count everything */
	for (vector<Type *>::iterator v = ir->_types.begin(); v != ir->_types.end(); ++v) {
		Type *type = *v;
		if (!type->_blacklisted) {
			rc = type->acceptVisitor(enumerator);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::buildBlobData(OMRPortLibrary *portLibrary, Symbol_IR *ir)
{
	DDR_RC rc = DDR_RC_OK;

	/* allocate hashtable */
	_buildInfo.stringHash =
		hashTableNew(portLibrary, OMR_GET_CALLSITE(), 0,
					 sizeof(StringTableEntry), 0, 0, OMRMEM_CATEGORY_UNKNOWN,
					 stringTableHash, stringTableEquals, NULL, NULL);

	/* Allocate room for one extra class, field and constant
	 * so our cursors always point at valid locations.
	 */
	_buildInfo.blobStructs = (BlobStruct *)malloc((_buildInfo.header.structureCount + 1) * sizeof(BlobStruct));
	_buildInfo.curBlobStruct = _buildInfo.blobStructs;
	_buildInfo.blobFields = (BlobField *)malloc((_buildInfo.fieldCount + 1) * sizeof(BlobField));
	_buildInfo.curBlobField = _buildInfo.blobFields;
	_buildInfo.blobConsts = (BlobConstant *)malloc((_buildInfo.constCount + 1) * sizeof(BlobConstant));
	_buildInfo.curBlobConst = _buildInfo.blobConsts;
	_buildInfo.curBlobStruct->constantCount = 0;
	_buildInfo.curBlobStruct->fieldCount = 0;

	const BlobBuildVisitor builder(this);

	for (vector<Type *>::iterator v = ir->_types.begin(); v != ir->_types.end(); ++v) {
		Type *type = *v;
		if (!type->_blacklisted) {
			rc = type->acceptVisitor(builder);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}

	if (DDR_RC_OK == rc) {
		_buildInfo.stringBuffer = (uint8_t *)malloc(_buildInfo.header.stringDataSize);
		memset(_buildInfo.stringBuffer, 0, _buildInfo.header.stringDataSize);
		copyStringTable();
	}

	return rc;
}

static bool
isNeededBy(UDT *type, const vector<Field *> &fields)
{
	bool referenced = true;

	if (type->_blacklisted) {
		referenced = false;
	} else if (type->isAnonymousType()) {
		referenced = false;
		for (vector<Field *>::const_iterator it = fields.begin(); it != fields.end(); ++it) {
			if ((*it)->_fieldType == type) {
				referenced = true;
				break;
			}
		}
	}

	return referenced;
}

DDR_RC
BlobBuildVisitor::visitEnum(EnumUDT *e) const
{
	DDR_RC rc = DDR_RC_OK;

	/* Do not add anonymous inner types as their own type. */
	if (_addFieldsOnly || !e->isAnonymousType()) {
		uint32_t constCount = 0;

		if (!_addFieldsOnly) {
			for (vector<EnumMember *>::iterator m = e->_enumMembers.begin(); m != e->_enumMembers.end(); ++m) {
				EnumMember *literal = *m;

				rc = _gen->addBlobConst(literal->_name, literal->_value, &constCount);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}

		if (DDR_RC_OK == rc) {
			if (_addFieldsOnly) {
				_gen->_buildInfo.curBlobStruct->constantCount += constCount;
			} else {
				rc = _gen->addBlobStruct(getBlobFullName(e), "", constCount, 0, (uint32_t)e->_sizeOf);
			}
		}
	}

	return rc;
}

DDR_RC
BlobBuildVisitor::visitUnion(UnionUDT *u) const
{
	DDR_RC rc = DDR_RC_OK;

	/* Do not add anonymous inner types as their own type. */
	if (_addFieldsOnly || !u->isAnonymousType()) {
		uint32_t fieldCount = 0;
		uint32_t constCount = 0;

		for (vector<Field *>::iterator m = u->_fieldMembers.begin(); m != u->_fieldMembers.end(); ++m) {
			rc = _gen->addBlobField(*m, &fieldCount, _baseOffset, _prefix);
			if (DDR_RC_OK != rc) {
				break;
			}
		}

		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<EnumMember *>::iterator m = u->_enumMembers.begin(); m != u->_enumMembers.end(); ++m) {
				EnumMember *literal = *m;

				rc = _gen->addBlobConst(literal->_name, literal->_value, &constCount);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}

		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<Macro>::iterator m = u->_macros.begin(); m != u->_macros.end(); ++m) {
				long long value = 0;

				if (DDR_RC_OK == m->getNumeric(&value)) {
					rc = _gen->addBlobConst(m->_name, value, &constCount);
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}

		if (DDR_RC_OK == rc) {
			if (_addFieldsOnly) {
				_gen->_buildInfo.curBlobStruct->constantCount += constCount;
				_gen->_buildInfo.curBlobStruct->fieldCount += fieldCount;
			} else {
				rc = _gen->addBlobStruct(getBlobFullName(u), "", constCount, fieldCount, (uint32_t)u->_sizeOf);
			}
		}
	}

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		const BlobBuildVisitor builder(_gen);

		for (vector<UDT *>::const_iterator it = u->_subUDTs.begin(); it != u->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (isNeededBy(nested, u->_fieldMembers)) {
				rc = nested->acceptVisitor(builder);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}

DDR_RC
BlobBuildVisitor::visitClass(ClassUDT *cu) const
{
	DDR_RC rc = DDR_RC_OK;

	/* Do not add anonymous inner types as their own type. */
	if (_addFieldsOnly || !cu->isAnonymousType()) {
		uint32_t fieldCount = 0;
		uint32_t constCount = 0;

		for (vector<Field *>::iterator m = cu->_fieldMembers.begin(); m != cu->_fieldMembers.end(); ++m) {
			rc = _gen->addBlobField(*m, &fieldCount, _baseOffset, _prefix);
			if (DDR_RC_OK != rc) {
				break;
			}
		}

		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<EnumMember *>::iterator m = cu->_enumMembers.begin(); m != cu->_enumMembers.end(); ++m) {
				EnumMember *literal = *m;

				rc = _gen->addBlobConst(literal->_name, literal->_value, &constCount);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}

		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<Macro>::iterator m = cu->_macros.begin(); m != cu->_macros.end(); ++m) {
				long long value = 0;

				if (DDR_RC_OK == m->getNumeric(&value)) {
					rc = _gen->addBlobConst(m->_name, value, &constCount);
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}

		if (DDR_RC_OK == rc) {
			if (_addFieldsOnly) {
				_gen->_buildInfo.curBlobStruct->constantCount += constCount;
				_gen->_buildInfo.curBlobStruct->fieldCount += fieldCount;
			} else {
				string nameFormatted = getBlobFullName(cu);
				string superName;

				if (NULL != cu->_superClass) {
					superName = getBlobFullName(cu->_superClass);
				}

				rc = _gen->addBlobStruct(nameFormatted, superName, constCount, fieldCount, (uint32_t)cu->_sizeOf);
			}
		}
	}

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		const BlobBuildVisitor builder(_gen);

		for (vector<UDT *>::const_iterator it = cu->_subUDTs.begin(); it != cu->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (isNeededBy(nested, cu->_fieldMembers)) {
				rc = nested->acceptVisitor(builder);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}

DDR_RC
BlobBuildVisitor::visitNamespace(NamespaceUDT *ns) const
{
	DDR_RC rc = DDR_RC_OK;
	uint32_t constCount = 0;

	for (vector<Macro>::iterator m = ns->_macros.begin(); m != ns->_macros.end(); ++m) {
		long long value = 0;

		if (DDR_RC_OK == m->getNumeric(&value)) {
			rc = _gen->addBlobConst(m->_name, value, &constCount);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}

	if (DDR_RC_OK == rc) {
		if (_addFieldsOnly) {
			_gen->_buildInfo.curBlobStruct->constantCount += constCount;
		} else {
			rc = _gen->addBlobStruct(getBlobFullName(ns), "", constCount, 0, 0);
		}
	}

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		const BlobBuildVisitor builder(_gen);

		for (vector<UDT *>::const_iterator it = ns->_subUDTs.begin(); it != ns->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (!nested->_blacklisted && !nested->isAnonymousType()) {
				rc = nested->acceptVisitor(builder);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}

DDR_RC
BlobBuildVisitor::visitType(Type *type) const
{
	/* No op: base types are not added to the blob at this time. */
	return DDR_RC_OK;
}

DDR_RC
BlobBuildVisitor::visitTypedef(TypedefUDT *type) const
{
	/* No need to write this typedef: references are expanded. */
	return DDR_RC_OK;
}

DDR_RC
JavaBlobGenerator::addBlobField(Field *field, uint32_t *fieldCount, size_t baseOffset, const string &prefix)
{
	DDR_RC rc = DDR_RC_OK;
	const size_t adjustedOffset = baseOffset + field->_offset;
	Type *fieldType = field->_fieldType;

	if (field->_isStatic) {
		/* omit static fields */
	} else if (NULL == fieldType) {
		/* omit fields without a type */
	} else if (fieldType->isAnonymousType()) {
		const string adjustedPrefix = prefix + field->_name + ".";
		rc = fieldType->acceptVisitor(BlobBuildVisitor(this, adjustedOffset, adjustedPrefix));
	} else {
		uint32_t nameOffset = INVALID_OFFSET;
		uint32_t typeOffset = INVALID_OFFSET;
		string typeName;

		rc = formatFieldType(field, &typeName);

		if (DDR_RC_OK == rc) {
			string fieldName = prefix + field->_name;

			rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, fieldName.c_str(), &nameOffset);
		}

		if (DDR_RC_OK == rc) {
			rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, typeName.c_str(), &typeOffset);
		}

		if (DDR_RC_OK == rc) {
			*fieldCount += 1;
			_buildInfo.curBlobField->nameOffset = nameOffset;
			_buildInfo.curBlobField->typeOffset = typeOffset;
			_buildInfo.curBlobField->offset = (uint32_t)adjustedOffset;

			if ((uintptr_t)(_buildInfo.curBlobField - _buildInfo.blobFields) < (uintptr_t)_buildInfo.fieldCount) {
				_buildInfo.curBlobField += 1;
			} else {
				ERRMSG("Adding more fields than enumerated");
				rc = DDR_RC_ERROR;
			}
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addBlobConst(const string &name, long long value, uint32_t *constCount)
{
	uint32_t offset = INVALID_OFFSET;
	DDR_RC rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, name.c_str(), &offset);

	if (DDR_RC_OK == rc) {
		_buildInfo.curBlobConst->nameOffset = offset;

		memset(_buildInfo.curBlobConst->value, 0, sizeof(_buildInfo.curBlobConst->value));
		memcpy(_buildInfo.curBlobConst->value, &value, sizeof(value));

		if ((uintptr_t)(_buildInfo.curBlobConst - _buildInfo.blobConsts) < (uintptr_t)_buildInfo.constCount) {
			_buildInfo.curBlobConst += 1;
			if (NULL != constCount) {
				*constCount += 1;
			}
		} else {
			ERRMSG("Adding more constants than enumerated");
			rc = DDR_RC_ERROR;
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addBlobStruct(const string &name, const string &superName, uint32_t constCount, uint32_t fieldCount, uint32_t size)
{
	if (!_printEmptyTypes && (0 == constCount) && (0 == fieldCount)) {
		return DDR_RC_OK;
	}

	uint32_t nameOffset = INVALID_OFFSET;
	uint32_t superOffset = INVALID_OFFSET;
	DDR_RC rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, name.c_str(), &nameOffset);

	if ((DDR_RC_OK == rc) && !superName.empty()) {
		rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, superName.c_str(), &superOffset);
	}

	if (DDR_RC_OK == rc) {
		_buildInfo.curBlobStruct->nameOffset = nameOffset;
		_buildInfo.curBlobStruct->superOffset = superOffset;
		_buildInfo.curBlobStruct->constantCount += constCount;
		_buildInfo.curBlobStruct->fieldCount += fieldCount;
		_buildInfo.curBlobStruct->structSize = size;

		if ((uintptr_t)(_buildInfo.curBlobStruct - _buildInfo.blobStructs) < (uintptr_t)_buildInfo.header.structureCount) {
			_buildInfo.curBlobStruct += 1;
			_buildInfo.curBlobStruct->constantCount = 0;
			_buildInfo.curBlobStruct->fieldCount = 0;
		} else {
			ERRMSG("Adding more types than enumerated");
			rc = DDR_RC_ERROR;
		}
	}

	return rc;
}

class BlobFieldVisitor : public TypeVisitor
{
private:
	string * const _typePrefix;
	string * const _typeSuffix;

public:
	explicit BlobFieldVisitor(string *prefix, string *suffix)
		: _typePrefix(prefix)
		, _typeSuffix(suffix)
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
BlobFieldVisitor::visitType(Type *type) const
{
	const string & typeName = type->_name;
	bool isSigned = false;
	size_t bitWidth = 0;

	if (Type::isStandardType(typeName.c_str(), (size_t)typeName.length(), &isSigned, &bitWidth)) {
		stringstream newType;

		newType << (isSigned ? "I" : "U") << bitWidth;

		*_typePrefix += newType.str();
	} else {
		*_typePrefix += typeName;
	}

	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitNamespace(NamespaceUDT *type) const
{
	string prefix = type->getSymbolKindName();

	/* prefix "union" should not be included */
	if (prefix.empty() || ("union" == prefix)) {
		*_typePrefix += type->getFullName();
	} else {
		*_typePrefix += prefix + " " + type->getFullName();
	}

	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitEnum(EnumUDT *type) const
{
	*_typePrefix += type->getSymbolKindName() + " " + type->getFullName();
	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitTypedef(TypedefUDT *type) const
{
	/* If typedef is void*, or otherwise known as a function pointer, return name as void*. */
	Type *opaqueType = type->getOpaqueType();

	if ((NULL != opaqueType) && ("void" == opaqueType->_name) && (0 != type->_modifiers._pointerCount)) {
		*_typePrefix += "void";
	} else {
		string prefix = opaqueType->getSymbolKindName();
		string fullName = opaqueType->getFullName();

		/* prefix "union" should not be included */
		if (prefix.empty() || ("union" == prefix)) {
			bool isSigned = false;
			size_t bitWidth = 0;

			if (Type::isStandardType(fullName.c_str(), (size_t)fullName.length(), &isSigned, &bitWidth)) {
				stringstream newType;

				newType << (isSigned ? "I" : "U") << bitWidth;

				*_typePrefix += newType.str();
			} else {
				*_typePrefix += fullName;
			}
		} else {
			*_typePrefix += prefix + " " + fullName;
		}
	}

	for (size_t i = type->getPointerCount(); i > 0; --i) {
		*_typeSuffix = "*" + *_typeSuffix;
	}

	for (size_t i = type->getArrayDimensions(); i > 0; --i) {
		*_typeSuffix = *_typeSuffix + "[]";
	}

	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitClass(ClassUDT *type) const
{
	return visitNamespace(type);
}

DDR_RC
BlobFieldVisitor::visitUnion(UnionUDT *type) const
{
	return visitNamespace(type);
}

DDR_RC
JavaBlobGenerator::formatFieldType(Field *field, string *fieldType)
{
	DDR_RC rc = DDR_RC_OK;
	Type *type = field->_fieldType;
	string typePrefix;
	string typeSuffix;

	*fieldType = "";

	if (0 != (field->_modifiers._modifierFlags & ~Modifiers::MODIFIER_FLAGS)) {
		ERRMSG("Unhandled field modifer flags: %d", field->_modifiers._modifierFlags);
		rc = DDR_RC_ERROR;
	} else {
		typeSuffix = field->_modifiers.getPointerType();

		if ((NULL == type) || type->_blacklisted || (field->_modifiers._pointerCount > 1)) {
			typePrefix = "void";
		} else {
			typePrefix = field->_modifiers.getModifierNames();

			rc = type->acceptVisitor(BlobFieldVisitor(&typePrefix, &typeSuffix));
		}
	}

	if ((DDR_RC_OK == rc) && (NULL != type)) {
		stringstream bits;

		if (0 != field->_bitField) {
			bits << ":" << field->_bitField;
		}

		*fieldType = typePrefix + typeSuffix + bits.str();
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addFieldAndConstCount(bool addFieldsOnly, size_t fieldCount, size_t constCount)
{
	DDR_RC rc = DDR_RC_OK;
	size_t structDataSize = sizeof(BlobField) * fieldCount + sizeof(BlobConstant) * constCount;

	if (!addFieldsOnly && (_printEmptyTypes || (0 != structDataSize))) {
		structDataSize += sizeof(BlobStruct);
	}

	if (structDataSize < ((uint32_t)-1)) {
		_buildInfo.header.structDataSize += (uint32_t)structDataSize;
		_buildInfo.fieldCount += (uint32_t)fieldCount;
		_buildInfo.constCount += (uint32_t)constCount;

		if (addFieldsOnly) {
			/* just count fields and constants */
		} else if (_printEmptyTypes || (0 != structDataSize)) {
			_buildInfo.header.structureCount += 1;
		}
	} else {
		ERRMSG("structDataSize is too large to fit in header.structDataSize");
		rc = DDR_RC_ERROR;
	}

	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitType(Type *type) const
{
	/* No op: base types are not added to the blob at this time. */
	return DDR_RC_OK;
}

DDR_RC
BlobEnumerateVisitor::visitNamespace(NamespaceUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;

	if (_addFieldsOnly || !type->isAnonymousType()) {
		size_t constCount = 0;

		if (!_addFieldsOnly) {
			for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); ++it) {
				/* Add only integer constants to the blob. */
				if (DDR_RC_OK == it->getNumeric(NULL)) {
					constCount += 1;
				}
			}
		}

		rc = _gen->addFieldAndConstCount(_addFieldsOnly, 0, constCount);
	}

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		for (vector<UDT *>::const_iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (!nested->_blacklisted && !nested->isAnonymousType()) {
				rc = nested->acceptVisitor(*this);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitEnum(EnumUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	size_t constCount = _addFieldsOnly ? 0 : type->_enumMembers.size();

	if (_addFieldsOnly || !type->isAnonymousType()) {
		rc = _gen->addFieldAndConstCount(_addFieldsOnly, 0, constCount);
	}

	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitTypedef(TypedefUDT *type) const
{
	/* No need to write this typedef: references are expanded. */
	return DDR_RC_OK;
}

DDR_RC
BlobEnumerateVisitor::visitClass(ClassUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	size_t fieldCount = 0;

	if (_addFieldsOnly || !type->isAnonymousType()) {
		const BlobEnumerateVisitor fieldsOnly(_gen, true);

		for (vector<Field *>::iterator v = type->_fieldMembers.begin(); v != type->_fieldMembers.end(); ++v) {
			Field *field = *v;
			Type *fieldType = field->_fieldType;

			if (field->_isStatic) {
				/* skip static fields */
			} else if (NULL == fieldType) {
				/* skip fields without a type */
			} else if (fieldType->isAnonymousType()) {
				/* Anonymous type members are added to the struct and not counted as fields themselves. */
				rc = fieldType->acceptVisitor(fieldsOnly);
				if (DDR_RC_OK != rc) {
					break;
				}
			} else {
				fieldCount += 1;
			}
		}
	}

	size_t constCount = 0;

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		constCount += type->_enumMembers.size();

		for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); ++it) {
			/* Only add integer constants to the blob. */
			if (DDR_RC_OK == it->getNumeric(NULL)) {
				constCount += 1;
			}
		}
	}

	if ((DDR_RC_OK == rc) && (_addFieldsOnly || !type->isAnonymousType())) {
		rc = _gen->addFieldAndConstCount(_addFieldsOnly, fieldCount, constCount);
	}

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		for (vector<UDT *>::const_iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (isNeededBy(nested, type->_fieldMembers)) {
				rc = nested->acceptVisitor(*this);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitUnion(UnionUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	size_t fieldCount = 0;

	if (_addFieldsOnly || !type->isAnonymousType()) {
		const BlobEnumerateVisitor fieldsOnly(_gen, true);

		for (vector<Field *>::iterator v = type->_fieldMembers.begin(); v != type->_fieldMembers.end(); ++v) {
			Field *field = *v;
			Type *fieldType = field->_fieldType;

			if (field->_isStatic) {
				/* skip static fields */
			} else if (NULL == fieldType) {
				/* skip fields without a type */
			} else if (fieldType->isAnonymousType()) {
				/* Anonymous type members are added to the struct and not counted as fields themselves. */
				rc = fieldType->acceptVisitor(fieldsOnly);
				if (DDR_RC_OK != rc) {
					break;
				}
			} else {
				fieldCount += 1;
			}
		}
	}

	size_t constCount = 0;

	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		constCount += type->_enumMembers.size();

		for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); ++it) {
			/* Only add integer constants to the blob. */
			if (DDR_RC_OK == it->getNumeric(NULL)) {
				constCount += 1;
			}
		}
	}

	if ((DDR_RC_OK == rc) && (_addFieldsOnly || !type->isAnonymousType())) {
		rc = _gen->addFieldAndConstCount(_addFieldsOnly, fieldCount, constCount);
	}

	/* When adding the fields from a field with an anonymous type, do not add subUDTs twice. */
	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		for (vector<UDT *>::const_iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
			UDT *nested = *it;

			if (isNeededBy(nested, type->_fieldMembers)) {
				rc = nested->acceptVisitor(*this);
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}

	return rc;
}
