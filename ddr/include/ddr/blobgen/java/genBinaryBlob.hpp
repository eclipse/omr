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

#ifndef GENBINARYBLOB_HPP
#define GENBINARYBLOB_HPP

#include "hashtable_api.h"
#include "omrutil.h"
#include "omrport.h"

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/config.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Field.hpp"
#include "ddr/ir/Macro.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TypedefUDT.hpp"
#include "ddr/ir/UnionUDT.hpp"

using std::pair;

class JavaBlobGenerator: public BlobGenerator
{
private:
	typedef struct BitField {
		uint32_t x1 : 1;
		uint32_t x2 : 15;
		uint32_t x3 : 6;
		uint32_t x4 : 10;

		uint32_t x5 : 7;
		uint32_t x6 : 9;
		uint32_t x7 : 4;
		uint32_t x8 : 12;
	} BitField;

	typedef struct BlobHeader {
		uint32_t version;
		uint8_t sizeofBool;
		uint8_t sizeofUDATA;
		uint8_t bitfieldFormat;
		uint8_t unused;
		uint32_t structDataSize;
		uint32_t stringDataSize;
		uint32_t structureCount;
	} BlobHeader;

	typedef struct BlobStruct {
		uint32_t nameOffset;
		uint32_t superOffset;
		uint32_t structSize;
		uint32_t fieldCount;
		uint32_t constantCount;
	} BlobStruct;

	typedef struct BlobField {
		uint32_t nameOffset;
		uint32_t typeOffset;
		uint32_t offset;
	} BlobField;

	typedef struct BlobConstant {
		uint32_t nameOffset;
		uint32_t value[2];
	} BlobConstant;

	typedef struct J9UTF8 {
		uint16_t length;
		uint8_t data[2];
	} J9UTF8;

	class BuildBlobInfo
	{
		/* Data members */
	public:
		BlobHeader header;
		J9HashTable *stringHash;
		uint32_t fieldCount;
		uint32_t constCount;
		BlobStruct *blobStructs;
		BlobStruct *curBlobStruct;
		BlobField *blobFields;
		BlobField *curBlobField;
		BlobConstant *blobConsts;
		BlobConstant *curBlobConst;
		uint8_t *stringBuffer;
		intptr_t fd;

		BuildBlobInfo() :
			stringHash(NULL),
			fieldCount(0),
			constCount(0),
			blobStructs(NULL),
			curBlobStruct(NULL),
			blobFields(NULL),
			curBlobField(NULL),
			blobConsts(NULL),
			curBlobConst(NULL),
			stringBuffer(NULL),
			fd(-1)
		{
			initBlobHeader();
		}

		void initBlobHeader();

	private:
		DDR_RC initializeBitfieldFormat(uint8_t *bitfieldFormat);
	};

	static pair<string, unsigned long long> cLimits[];

	BuildBlobInfo _buildInfo;
	bool _printEmptyTypes;

	void copyStringTable();
	DDR_RC stringTableOffset(BlobHeader *blobHeader, J9HashTable *stringTable, const char *cString, uint32_t *offset);
	DDR_RC enumerateCLimits();
	DDR_RC addCLimits();
	DDR_RC countStructsAndStrings(Symbol_IR *const ir);
	DDR_RC addFieldAndConstCount(bool addStructureCount, size_t fieldCount, size_t constCount);
	DDR_RC buildBlobData(OMRPortLibrary *portLibrary, Symbol_IR *const ir);
	DDR_RC addBlobField(Field *f, uint32_t *fieldCount, uint32_t *constCount);
	DDR_RC addBlobField(Field *f, uint32_t *fieldCount, uint32_t *constCount, string prefix);
	DDR_RC addBlobConst(string name, long long value, uint32_t *constCount);
	DDR_RC addBlobStruct(string name, string superName, uint32_t constCount, uint32_t fieldCount, uint32_t size);
	DDR_RC formatFieldType(Field *f, string *fieldType);

	friend class BlobBuildVisitor;
	friend class BlobEnumerateVisitor;

public:
	JavaBlobGenerator(bool printEmptyTypes) : _printEmptyTypes(printEmptyTypes) {}
	DDR_RC genBinaryBlob(struct OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *blobFile);
};

#endif /* GENBINARYBLOB_HPP */
