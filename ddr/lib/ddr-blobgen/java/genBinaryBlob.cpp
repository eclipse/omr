/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#include "ddr/blobgen/java/genBinaryBlob.hpp"

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

class BlobBuildVisitor : public TypeVisitor
{
private:
	JavaBlobGenerator *_gen;
	bool _addFieldsOnly;
	string _prefix;

public:
	BlobBuildVisitor(JavaBlobGenerator *gen, bool addFieldsOnly, string prefix) : _gen(gen), _addFieldsOnly(addFieldsOnly), _prefix(prefix) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
};

class BlobEnumerateVisitor : public TypeVisitor
{
private:
	JavaBlobGenerator *_gen;
	bool _addFieldsOnly;

public:
	BlobEnumerateVisitor(JavaBlobGenerator *gen, bool addFieldsOnly) : _gen(gen), _addFieldsOnly(addFieldsOnly) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
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
	size_t length = strlen(s);
	uintptr_t hash = 0;
	uint8_t *data = (uint8_t *)s;
	const uint8_t *end = data + length;

	while (data < end) {
		uint16_t c;

		data += decodeUTF8Char(data, &c);
		hash = (hash << 5) - hash + c;
	}

	return hash;
}

static uintptr_t
stringTableEquals(void *leftKey, void *rightKey, void *userData)
{
	const char *left_s = ((StringTableEntry *)leftKey)->cString;
	const char *right_s = ((StringTableEntry *)rightKey)->cString;

	return (left_s == right_s) || (0 == strcmp(left_s, right_s));
}

void
JavaBlobGenerator::copyStringTable()
{
	uint8_t *stringData = _buildInfo.stringBuffer;
	J9HashTableState state;

	/* Iterate the table and copy each string to its already-assigned offset */
	StringTableEntry *entry = (StringTableEntry *)hashTableStartDo(_buildInfo.stringHash, &state);
	while (NULL != entry) {
		J9UTF8 *utf = (J9UTF8 *)(stringData + entry->offset);
		const char *cString = entry->cString;
		size_t len = strlen(cString);

		utf->length = (uint16_t)len;
		memcpy(utf->data, cString, len);
		free((char *)cString);

		entry = (StringTableEntry *)hashTableNextDo(&state);
	}

	/* TODO need to pad file length to 32-bits? */
#if 0
	/* Update cursor (stringDataSize is U_32 aligned already) */
	data->cursor = stringData + data->coreFileData->stringDataSize;
#endif
}

DDR_RC
JavaBlobGenerator::stringTableOffset(BlobHeader *blobHeader, J9HashTable *stringTable, const char *cString, uint32_t *offset)
{
	DDR_RC rc = DDR_RC_OK;
	StringTableEntry exemplar;
	StringTableEntry *entry;
	*offset = UINT32_MAX;

	exemplar.cString = cString;
	exemplar.offset = UINT32_MAX;

	/* Add will return an existing entry if one matches, NULL on allocation failure, or the new node */
	entry = (StringTableEntry *)hashTableAdd(stringTable, &exemplar);

	/* NULL means the string was new, but memory allocation failed */
	if (NULL == entry) {
		ERRMSG("Unable to allocate memory for string table entry: %s", cString);
		rc = DDR_RC_ERROR;
	} else {
		/* If the offset is UINT32_MAX, this indicates that a new entry was added */
		if (UINT32_MAX == entry->offset) {
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
		} else {
			*offset = entry->offset;
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::genBinaryBlob(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *blobFile)
{
	DDR_RC rc = DDR_RC_OK;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* iterate ir:
	 *  - count structs - update blob header
	 *  - build string hash table
	 */
	rc = countStructsAndStrings(ir);

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
		for (unsigned int s_ix = 0; s_ix < _buildInfo.header.structureCount; s_ix++) {
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

		if ((unsigned long)amountWritten != _buildInfo.header.structDataSize) {
			ERRMSG("Written: %d fields, %d constants, %d structs.\nCounted: %d fields, %d constants, %d structures.\n",
				   f_ix, c_ix, (int)(_buildInfo.curBlobStruct - _buildInfo.blobStructs),
				   _buildInfo.fieldCount, _buildInfo.constCount, _buildInfo.header.structureCount);
			ERRMSG("Expected %d to be written, but %d was written.\n", (int)_buildInfo.header.structDataSize, (int)amountWritten);
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

pair<string, unsigned long long> JavaBlobGenerator::cLimits[] = {
		make_pair("CHAR_MAX", CHAR_MAX),
		make_pair("CHAR_MIN", CHAR_MIN),
		make_pair("INT_MAX", INT_MAX),
		make_pair("INT_MIN", INT_MIN),
		make_pair("LONG_MAX", LONG_MAX),
		make_pair("LONG_MIN", LONG_MIN),
		make_pair("SCHAR_MAX", SCHAR_MAX),
		make_pair("SCHAR_MIN", SCHAR_MIN),
		make_pair("SHRT_MAX", SHRT_MAX),
		make_pair("SHRT_MIN", SHRT_MIN),
		make_pair("UCHAR_MAX", UCHAR_MAX),
		make_pair("UINT_MAX", UINT_MAX),
		make_pair("ULONG_MAX", ULONG_MAX),
		make_pair("USHRT_MAX", USHRT_MAX)
};

DDR_RC
JavaBlobGenerator::enumerateCLimits()
{
	DDR_RC rc = DDR_RC_OK;
	size_t fieldCount = 0;
	size_t constCount = sizeof(cLimits) / sizeof(cLimits[0]);
	size_t structDataSize = sizeof(BlobField) * fieldCount + sizeof(BlobConstant) * constCount + sizeof(BlobStruct);

	_buildInfo.header.structureCount += 1;
	_buildInfo.header.structDataSize += (uint32_t)structDataSize;
	_buildInfo.fieldCount += (uint32_t)fieldCount;
	_buildInfo.constCount += (uint32_t)constCount;
	return rc;
}

DDR_RC
JavaBlobGenerator::addCLimits()
{
	DDR_RC rc = DDR_RC_OK;
	size_t fieldCount = 0;
	size_t constCount = sizeof(cLimits) / sizeof(cLimits[0]);

	for (size_t i = 0; i < constCount; i += 1) {
		rc = addBlobConst(cLimits[i].first, cLimits[i].second, NULL);
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	if (DDR_RC_OK == rc) {
		rc = addBlobStruct("CLimits", "", (uint32_t)constCount, (uint32_t)fieldCount, 0);
	}

	return rc;
}

/* iterate ir:
 *  - count structs - update blob header
 *  - build string hash table
 */
DDR_RC
JavaBlobGenerator::countStructsAndStrings(Symbol_IR *const ir)
{
	DDR_RC rc = DDR_RC_OK;
	/* just count everything */
	for (vector<Type *>::iterator v = (ir->_types).begin(); v != (ir->_types).end(); ++v) {
		rc = (*v)->acceptVisitor(BlobEnumerateVisitor(this, false));
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	/* Add CLimits to the count. */
	if (DDR_RC_OK == rc) {
		rc = enumerateCLimits();
	}
	return rc;
}

DDR_RC
JavaBlobGenerator::buildBlobData(OMRPortLibrary *portLibrary, Symbol_IR *const ir)
{
	DDR_RC rc = DDR_RC_OK;
	/* allocate hashtable */
	_buildInfo.stringHash =
		hashTableNew(portLibrary, OMR_GET_CALLSITE(), 0,
					 sizeof(StringTableEntry), 0, 0, OMRMEM_CATEGORY_UNKNOWN,
					 stringTableHash, stringTableEquals, NULL, NULL);

	/* allocate class, field, and const structures */
	_buildInfo.blobStructs = (BlobStruct *)malloc(_buildInfo.header.structureCount * sizeof(BlobStruct));
	_buildInfo.curBlobStruct = _buildInfo.blobStructs;
	_buildInfo.blobFields = (BlobField *)malloc(_buildInfo.fieldCount * sizeof(BlobField));
	_buildInfo.curBlobField = _buildInfo.blobFields;
	_buildInfo.blobConsts = (BlobConstant *)malloc(_buildInfo.constCount * sizeof(BlobConstant));
	_buildInfo.curBlobConst = _buildInfo.blobConsts;
	_buildInfo.curBlobStruct->constantCount = 0;
	_buildInfo.curBlobStruct->fieldCount = 0;

	for (vector<Type *>::iterator v = ir->_types.begin(); v != ir->_types.end(); ++v) {
		rc = (*v)->acceptVisitor(BlobBuildVisitor(this, false, ""));
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	if (DDR_RC_OK == rc) {
		/* Add CLimits. */
		rc = addCLimits();
	}

	if (DDR_RC_OK == rc) {
		_buildInfo.stringBuffer = (uint8_t *)malloc(_buildInfo.header.stringDataSize);
		memset(_buildInfo.stringBuffer, 0, _buildInfo.header.stringDataSize);
		copyStringTable();
	}
	return rc;
}

DDR_RC
BlobBuildVisitor::visitType(EnumUDT *e) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!e->_isDuplicate) {
		/* Do not add anonymous inner types as their own type. */
		if ((!e->isAnonymousType() || _addFieldsOnly)
			&& ((e->_enumMembers.size() > 0) || _gen->_printEmptyTypes)) {
			/* Format class name */
			string nameFormatted = e->_name;
			if (NULL != e->_outerNamespace) {
				nameFormatted = e->_outerNamespace->_name + "__" + nameFormatted;
			}

			uint32_t constCount = 0;
			for (vector<EnumMember *>::iterator m = (e->_enumMembers).begin(); m != (e->_enumMembers).end(); ++m) {
				rc = _gen->addBlobConst((*m)->_name, (*m)->_value, &constCount);
				if (DDR_RC_OK != rc) {
					break;
				}
			}

			if (DDR_RC_OK == rc) {
				if (_addFieldsOnly) {
					_gen->_buildInfo.curBlobStruct->constantCount += constCount;
				} else {
					rc = _gen->addBlobStruct(nameFormatted, "", constCount, 0, 0);
				}
			}
		}
	}
	return rc;
}

DDR_RC
BlobBuildVisitor::visitType(UnionUDT *u) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!u->_isDuplicate) {
		/* Do not add anonymous inner types as their own type. */
		if ((!u->isAnonymousType() || _addFieldsOnly)
			&& ((u->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			/* Format class name */
			string nameFormatted = u->_name;
			if (NULL != u->_outerNamespace) {
				nameFormatted = u->_outerNamespace->_name + "__" + nameFormatted;
			}

			uint32_t fieldCount = 0;
			uint32_t constCount = 0;
			for (vector<Field *>::iterator m = (u->_fieldMembers).begin(); m != (u->_fieldMembers).end(); ++m) {
				rc = _gen->addBlobField(*m, &fieldCount, &constCount, _prefix);
				if (DDR_RC_OK != rc) {
					break;
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<EnumMember *>::iterator m = (u->_enumMembers).begin(); m != (u->_enumMembers).end(); ++m) {
					rc = _gen->addBlobConst((*m)->_name, (*m)->_value, &constCount);
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<Macro>::iterator m = (u->_macros).begin(); m != (u->_macros).end(); ++m) {
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
					rc = _gen->addBlobStruct(nameFormatted, "", constCount, fieldCount, (uint32_t)u->_sizeOf);
				}
			}
		}
	}
	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		for (vector<UDT *>::iterator v = u->_subUDTs.begin(); v != u->_subUDTs.end(); ++v) {
			rc = (*v)->acceptVisitor(BlobBuildVisitor(_gen, false, ""));
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}
	return rc;
}

DDR_RC
BlobBuildVisitor::visitType(ClassUDT *cu) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!cu->_isDuplicate) {
		/* Do not add anonymous inner types as their own type. */
		if ((!cu->isAnonymousType() || _addFieldsOnly)
			&& ((cu->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			/* Format class name */
			string nameFormatted = cu->_name;
			if (NULL != cu->_outerNamespace) {
				nameFormatted = cu->_outerNamespace->_name + "__" + nameFormatted;
			}

			/* Find super class name offset */
			string superName;
			if (NULL != cu->_superClass) {
				superName = cu->_superClass->_name;
				if (NULL != cu->_superClass->_outerNamespace) {
					superName = cu->_superClass->_outerNamespace->_name + "__" + superName;
				}
			}

			uint32_t fieldCount = 0;
			uint32_t constCount = 0;
			for (vector<Field *>::iterator m = (cu->_fieldMembers).begin(); m != (cu->_fieldMembers).end(); ++m) {
				rc = _gen->addBlobField(*m, &fieldCount, &constCount, _prefix);
				if (DDR_RC_OK != rc) {
					break;
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<EnumMember *>::iterator m = (cu->_enumMembers).begin(); m != (cu->_enumMembers).end(); ++m) {
					rc = _gen->addBlobConst((*m)->_name, (*m)->_value, &constCount);
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}

			if (DDR_RC_OK == rc) {
				for (vector<Macro>::iterator m = (cu->_macros).begin(); m != (cu->_macros).end(); ++m) {
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
					rc = _gen->addBlobStruct(nameFormatted, superName, constCount, fieldCount, (uint32_t)cu->_sizeOf);
				}
			}
		}
	}

	/* Anonymous sub udt's not used as fields are to have their fields added to this struct.*/
	if ((DDR_RC_OK == rc) && !_addFieldsOnly && !cu->_isDuplicate) {
		for (vector<UDT *>::iterator it = cu->_subUDTs.begin(); it != cu->_subUDTs.end(); ++it) {
			if ((*it)->isAnonymousType()) {
				bool isUsedAsField = false;
				for (vector<Field *>::iterator fit = cu->_fieldMembers.begin(); fit != cu->_fieldMembers.end(); fit += 1) {
					if ((*fit)->_fieldType == (*it)) {
						isUsedAsField = true;
						break;
					}
				}
				if (!isUsedAsField) {
					rc = (*it)->acceptVisitor(BlobBuildVisitor(_gen, true, ""));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
	}
	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
		for (vector<UDT *>::iterator it = cu->_subUDTs.begin(); it != cu->_subUDTs.end(); ++it) {
			bool isUsedAsField = false;
			if ((*it)->isAnonymousType()) {
				for (vector<Field *>::iterator fit = cu->_fieldMembers.begin(); fit != cu->_fieldMembers.end(); fit += 1) {
					if ((*fit)->_fieldType == (*it)) {
						isUsedAsField = true;
						break;
					}
				}
			}
			if (!(*it)->isAnonymousType() || isUsedAsField) {
				rc = (*it)->acceptVisitor(BlobBuildVisitor(_gen, false, ""));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}
	return rc;
}

DDR_RC
BlobBuildVisitor::visitType(NamespaceUDT *ns) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!ns->_isDuplicate) {
		uint32_t constCount = 0;
		for (vector<Macro>::iterator m = (ns->_macros).begin(); m != (ns->_macros).end(); ++m) {
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
				rc = _gen->addBlobStruct(ns->_name, "", constCount, 0, 0);
			}
		}

		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			/* Anonymous sub udt's are to have their fields added to this struct.*/
			for (vector<UDT *>::iterator v = ns->_subUDTs.begin(); v != ns->_subUDTs.end(); ++v) {
				if ((*v)->isAnonymousType()) {
					rc = (*v)->acceptVisitor(BlobBuildVisitor(_gen, true, ""));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<UDT *>::iterator v = ns->_subUDTs.begin(); v != ns->_subUDTs.end(); ++v) {
				if (!(*v)->isAnonymousType()) {
					rc = (*v)->acceptVisitor(BlobBuildVisitor(_gen, false, ""));
					if (DDR_RC_OK != rc) {
						break;
					}
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
BlobBuildVisitor::visitType(TypedefUDT *type) const
{
	/* No op: base types are not added to the blob at this time. */
	return DDR_RC_OK;
}

DDR_RC
JavaBlobGenerator::addBlobField(Field *f, uint32_t *fieldCount, uint32_t *constCount)
{
	return addBlobField(f, fieldCount, constCount, "");
}

DDR_RC
JavaBlobGenerator::addBlobField(Field *f, uint32_t *fieldCount, uint32_t *constCount, string prefix)
{
	DDR_RC rc = DDR_RC_OK;

	if (!f->_isStatic) {
		/* Anonymous classes, structs, and unions add their fields directly to the outer type. */
		if ((NULL != f->_fieldType) && f->_fieldType->isAnonymousType()) {
			f->_fieldType->acceptVisitor(BlobBuildVisitor(this, true, prefix + f->_name + "."));
		} else {
			*fieldCount += 1;
			string typeName;
			rc = formatFieldType(f, &typeName);

			if (DDR_RC_OK == rc) {
				string fieldName = prefix + f->_name;
				if (DDR_RC_OK == rc) {
					uint32_t offset = UINT32_MAX;
					rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, fieldName.c_str(), &offset);

					if (DDR_RC_OK == rc) {
						_buildInfo.curBlobField->nameOffset = offset;

						offset = UINT32_MAX;
						rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, typeName.c_str(), &offset);

						if (DDR_RC_OK == rc) {
							_buildInfo.curBlobField->typeOffset = offset;

							_buildInfo.curBlobField->offset = (uint32_t)f->_offset;
							_buildInfo.curBlobField += 1;
						}
					}
				}
			}
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addBlobConst(string name, long long value, uint32_t *constCount)
{
	DDR_RC rc = DDR_RC_OK;
	uint32_t offset = UINT32_MAX;

	rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, name.c_str(), &offset);
	if (DDR_RC_OK == rc) {
		_buildInfo.curBlobConst->nameOffset = offset;

		memset(_buildInfo.curBlobConst->value, 0, sizeof(_buildInfo.curBlobConst->value));
		memcpy(_buildInfo.curBlobConst->value, &value, sizeof(value));
		_buildInfo.curBlobConst += 1;
		if (NULL != constCount) {
			*constCount += 1;
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addBlobStruct(string name, string superName, uint32_t constCount, uint32_t fieldCount, uint32_t size)
{
	DDR_RC rc = DDR_RC_OK;
	uint32_t offset = UINT32_MAX;

	rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, name.c_str(), &offset);

	if (DDR_RC_OK == rc) {
		_buildInfo.curBlobStruct->nameOffset = offset;
		if (superName.empty()) {
			_buildInfo.curBlobStruct->superOffset = 0xffffffff;
		} else {
			offset = UINT32_MAX;
			rc = stringTableOffset(&_buildInfo.header, _buildInfo.stringHash, superName.c_str(), &offset);

			if (DDR_RC_OK == rc) {
				_buildInfo.curBlobStruct->superOffset = offset;
			}
		}

		_buildInfo.curBlobStruct->constantCount += constCount;
		_buildInfo.curBlobStruct->fieldCount += fieldCount;
		_buildInfo.curBlobStruct->structSize = size;
		if ((uintptr_t)(_buildInfo.curBlobStruct - _buildInfo.blobStructs) < (uintptr_t)_buildInfo.header.structureCount - 1) {
			_buildInfo.curBlobStruct += 1;
			_buildInfo.curBlobStruct->constantCount = 0;
			_buildInfo.curBlobStruct->fieldCount = 0;
		}
	}

	return rc;
}

class BlobFieldVisitor : public TypeVisitor
{
private:
	string *_fieldString;

public:
	BlobFieldVisitor(string *fieldString) : _fieldString(fieldString) {}

	DDR_RC visitType(Type *type) const;
	DDR_RC visitType(NamespaceUDT *type) const;
	DDR_RC visitType(EnumUDT *type) const;
	DDR_RC visitType(TypedefUDT *type) const;
	DDR_RC visitType(ClassUDT *type) const;
	DDR_RC visitType(UnionUDT *type) const;
};

DDR_RC
BlobFieldVisitor::visitType(Type *type) const
{
	*_fieldString += type->_name;
	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitType(NamespaceUDT *type) const
{
	*_fieldString += type->getSymbolKindName() + " " + type->getFullName();
	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitType(EnumUDT *type) const
{
	*_fieldString += type->getSymbolKindName() + " " + type->getFullName();
	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitType(TypedefUDT *type) const
{
	/* If typedef is void*, or otherwise known as a function pointer, return name as void*. */
	if ((NULL != type->_aliasedType) && ("void" == type->_aliasedType->_name) && (1 == type->_modifiers._pointerCount)) {
		*_fieldString += "void*";
	} else {
		string prefix = type->getSymbolKindName();
		*_fieldString += (prefix.empty() ? "" : prefix + " ") + type->getFullName();
	}

	return DDR_RC_OK;
}

DDR_RC
BlobFieldVisitor::visitType(ClassUDT *type) const
{
	return visitType((NamespaceUDT *)type);
}

DDR_RC
BlobFieldVisitor::visitType(UnionUDT *type) const
{
	return visitType((NamespaceUDT *)type);
}

DDR_RC
JavaBlobGenerator::formatFieldType(Field *field, string *fieldType)
{
	DDR_RC rc = DDR_RC_OK;
	*fieldType = "";

	if (0 != (field->_modifiers._modifierFlags & ~Modifiers::MODIFIER_FLAGS)) {
		ERRMSG("Unhandled field modifer flags: %d", field->_modifiers._modifierFlags);
		rc = DDR_RC_ERROR;
	} else {
		*fieldType =  field->_modifiers.getModifierNames();
	}

	if (DDR_RC_OK == rc) {
		if (NULL == field->_fieldType) {
			*fieldType = "void";
		} else {
			rc = field->_fieldType->acceptVisitor(BlobFieldVisitor(fieldType));
		}
	}

	if (DDR_RC_OK == rc) {
		for (size_t i = 0; i < field->_modifiers._pointerCount; i++) {
			*fieldType += "*";
		}

		if (field->_modifiers.isArray()) {
			stringstream stream;
			for (size_t i = 0; i < field->_modifiers.getArrayDimensions(); i++) {
				stream << "[";
				if (field->_modifiers.getArrayLength(i) > 0) {
					stream << field->_modifiers.getArrayLength(i);
				}
				stream << "]";
			}
			*fieldType += stream.str();
		}
	}

	return rc;
}

DDR_RC
JavaBlobGenerator::addFieldAndConstCount(bool addFieldsOnly, size_t fieldCount, size_t constCount)
{
	DDR_RC rc = DDR_RC_OK;
	size_t structDataSize = sizeof(BlobField) * fieldCount + sizeof(BlobConstant) * constCount;
	if (!addFieldsOnly) {
		structDataSize += sizeof(BlobStruct);
	}

	if (structDataSize < ((uint32_t)-1)) {
		if (!addFieldsOnly) {
			_buildInfo.header.structureCount += 1;
		}
		_buildInfo.header.structDataSize += (uint32_t)structDataSize;
		_buildInfo.fieldCount += (uint32_t)fieldCount;
		_buildInfo.constCount += (uint32_t)constCount;
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
BlobEnumerateVisitor::visitType(NamespaceUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		size_t constCount = 0;
		/* Add only integer constants to the blob. */
		for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); it += 1) {
			if (DDR_RC_OK == it->getNumeric(NULL)) {
				constCount += 1;
			}
		}

		if ((DDR_RC_OK == rc) && (!type->isAnonymousType() || _addFieldsOnly)) {
			rc = _gen->addFieldAndConstCount(_addFieldsOnly, 0, constCount);
		}
		/* When adding the fields from a field with an anonymous type, do not add subUDTs twice. */
		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
				if ((*it)->isAnonymousType()) {
					rc = (*it)->acceptVisitor(BlobEnumerateVisitor(_gen, true));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
		if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
			for (vector<UDT *>::iterator it = type->_subUDTs.begin(); it != type->_subUDTs.end(); ++it) {
				if (!(*it)->isAnonymousType()) {
					rc = (*it)->acceptVisitor(BlobEnumerateVisitor(_gen, false));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
	}
	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitType(EnumUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		if ((!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_enumMembers.size() > 0) || _gen->_printEmptyTypes)) {
			rc = _gen->addFieldAndConstCount(_addFieldsOnly, 0, type->_enumMembers.size());
		}
	}
	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitType(TypedefUDT *type) const
{
	/* No op: typedefs are not added to the blob at this time. */
	return DDR_RC_OK;
}

DDR_RC
BlobEnumerateVisitor::visitType(ClassUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		size_t fieldCount = 0;
		size_t constCount = type->_enumMembers.size();

		if ((DDR_RC_OK == rc) && (!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			for (vector<Field *>::iterator v = type->_fieldMembers.begin(); v != type->_fieldMembers.end(); ++v) {
				if (!(*v)->_isStatic) {
					/* Anonymous type members are added to the struct and not counted as a field themselves. */
					if ((NULL != (*v)->_fieldType) && (*v)->_fieldType->isAnonymousType()) {
						rc = (*v)->_fieldType->acceptVisitor(BlobEnumerateVisitor(_gen, true));
						if (DDR_RC_OK != rc) {
							break;
						}
					} else {
						fieldCount += 1;
					}
				}
			}
		}

		if (DDR_RC_OK == rc) {
			/* Add only integer constants to the blob. */
			for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); it += 1) {
				if (DDR_RC_OK == it->getNumeric(NULL)) {
					constCount += 1;
				}
			}
		}
		if ((DDR_RC_OK == rc) && (!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			rc = _gen->addFieldAndConstCount(_addFieldsOnly, fieldCount, constCount);
		}
	}

	/* Anonymous sub udt's not used as fields are to have their fields added to this struct. */
	if ((DDR_RC_OK == rc) && !_addFieldsOnly && !type->_isDuplicate) {
		/* When adding the fields from a field with an anonymous type, do not add subUDTs twice. */
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
					rc = (*it)->acceptVisitor(BlobEnumerateVisitor(_gen, true));
					if (DDR_RC_OK != rc) {
						break;
					}
				}
			}
		}
	}
	if ((DDR_RC_OK == rc) && !_addFieldsOnly) {
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
				rc = (*it)->acceptVisitor(BlobEnumerateVisitor(_gen, false));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}
	return rc;
}

DDR_RC
BlobEnumerateVisitor::visitType(UnionUDT *type) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!type->_isDuplicate) {
		size_t fieldCount = 0;
		size_t constCount = type->_enumMembers.size();

		if ((DDR_RC_OK == rc) && (!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			for (vector<Field *>::iterator v = type->_fieldMembers.begin(); v != type->_fieldMembers.end(); ++v) {
				if (!(*v)->_isStatic) {
					/* Anonymous type members are added to the struct and not counted as a field themselves. */
					if ((NULL != (*v)->_fieldType) && (*v)->_fieldType->isAnonymousType()) {
						rc = (*v)->_fieldType->acceptVisitor(BlobEnumerateVisitor(_gen, true));
						if (DDR_RC_OK != rc) {
							break;
						}
					} else {
						fieldCount += 1;
					}
				}
			}
		}

		if (DDR_RC_OK == rc) {
			/* Add only integer constants to the blob. */
			for (vector<Macro>::iterator it = type->_macros.begin(); it != type->_macros.end(); it += 1) {
				if (DDR_RC_OK == it->getNumeric(NULL)) {
					constCount += 1;
				}
			}
		}
		if ((DDR_RC_OK == rc) && (!type->isAnonymousType() || _addFieldsOnly)
			&& ((type->_fieldMembers.size() > 0) || _gen->_printEmptyTypes)) {
			rc = _gen->addFieldAndConstCount(_addFieldsOnly, fieldCount, constCount);
		}
	}
	if (DDR_RC_OK == rc) {
		/* When adding the fields from a field with an anonymous type, do not add subUDTs twice. */
		if (!_addFieldsOnly) {
			for (vector<UDT *>::iterator v = type->_subUDTs.begin(); v != type->_subUDTs.end(); ++v) {
				rc = (*v)->acceptVisitor(BlobEnumerateVisitor(_gen, false));
				if (DDR_RC_OK != rc) {
					break;
				}
			}
		}
	}
	return rc;
}
