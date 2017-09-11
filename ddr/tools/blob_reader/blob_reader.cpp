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

#include <algorithm>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "ddr/error.hpp"

using std::vector;

typedef struct BlobHeaderV1 {
	uint32_t coreVersion;
	uint8_t sizeofBool;
	uint8_t sizeofUDATA;
	uint8_t bitfieldFormat;
	uint8_t padding;
	uint32_t structDataSize;
	uint32_t stringTableDataSize;
	uint32_t structureCount;
} BlobHeaderV1;

typedef struct BlobString {
	uint16_t length;
	char data[1]; /* flexible array member */
} BlobString;

typedef struct BlobStruct {
	uint32_t nameOffset;
	uint32_t superName;
	uint32_t sizeOf;
	uint32_t fieldCount;
	uint32_t constCount;
} BlobStruct;

typedef struct BlobField {
	uint32_t declaredName;
	uint32_t declaredType;
	uint32_t offset;
} BlobField;

typedef struct BlobConstant {
	uint32_t name;
	/* don't declare value as uint64_t to ensure that no padding is added between it and name */
	uint32_t value[2];
} BlobConstant;

struct Field;
struct Constant;

typedef struct Structure {
	char *name;
	uint32_t nameLength;
	char *superName;
	uint32_t superNameLength;
	uint32_t size;
	vector<Field *> fields;
	vector<Constant *> constants;
} Structure;

bool
compareStructs(const Structure *first, const Structure *second)
{
	return strcmp(first->name, second->name) < 0;
}

typedef struct Field {
	char *name;
	uint32_t nameLength;
	char *type;
	uint32_t typeLength;
	uint32_t offset;
} Field;

typedef struct Constant {
	char *name;
	uint32_t nameLength;
	uint64_t value;
} Constant;

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		ERRMSG("Please specify a blob filename.\n");
		ERRMSG("Usage: %s <blobfile>\n", argv[0]);
		return -1;
	}

	FILE *fd = fopen(argv[1], "rb");
	if (NULL == fd) {
		ERRMSG("fopen failed: %s", strerror(errno));
		return -1;
	}

	/* Read header */
	BlobHeaderV1 blobHdrV1;
	memset(&blobHdrV1, 0, sizeof(blobHdrV1));

	size_t rb = fread(&blobHdrV1, 1, sizeof(blobHdrV1), fd);
	if (sizeof(blobHdrV1) != rb) {
		ERRMSG("read blob header returned %lu, expected %lu\n", (unsigned long)rb, (unsigned long)sizeof(blobHdrV1));
		return -1;
	}

	/* NOTE Data appears to be written into the Blob using platform-dependent
	 * integer endianness. bitfieldFormat is probably used to interpret the
	 * values.
	 */

	printf("Blob Header:\n"
		   " coreVersion: %u\n"
		   " sizeofBool: %u\n"
		   " sizeofUDATA: %u\n"
		   " bitfieldFormat: %u\n" /* see initializeBitfieldEncoding in j9ddr.c */
		   " structDataSize: %u\n"
		   " stringTableDataSize: %u\n"
		   " structureCount: %u\n",
		   blobHdrV1.coreVersion,
		   blobHdrV1.sizeofBool,
		   blobHdrV1.sizeofUDATA,
		   blobHdrV1.bitfieldFormat,
		   blobHdrV1.structDataSize,
		   blobHdrV1.stringTableDataSize,
		   blobHdrV1.structureCount);

	/* Copy the file into memory */
	size_t blobLength = sizeof(blobHdrV1) + blobHdrV1.structDataSize + blobHdrV1.stringTableDataSize;
	uint8_t *blobBuffer = (uint8_t *)malloc(blobLength);
	if (NULL == blobBuffer) {
		ERRMSG("malloc blobBuffer failed\n");
		return -1;
	}

	rewind(fd);
	rb = fread(blobBuffer, 1, blobLength, fd);
	if (blobLength != rb) {
		ERRMSG("read blob data returned %lu, expected %lu\n", (unsigned long)rb, (unsigned long)blobLength);
		return -1;
	}
	fclose(fd);


	/* Read strings */
	{
		printf("\n== STRINGS ==\n");

		const uint8_t *const stringDataStart = blobBuffer + sizeof(blobHdrV1) + blobHdrV1.structDataSize;
		const uint8_t *const stringDataEnd = stringDataStart + blobHdrV1.stringTableDataSize;
		const uint8_t *currentString = stringDataStart;
		unsigned int stringNum = 1; /* Numbers the strings in the printed list */

		while (currentString < stringDataEnd) {
			const BlobString *blobString = (const BlobString *)currentString;

			/* NOTE string lengths appear to be padded to an even length */
			const uint16_t padding = blobString->length & 1;

			/* The format of the printed list is:
			 * [#]: <offset in string data> [<string length>] <string data>
			 */
			printf("%5u: %8lx [%u] %.*s\n", stringNum, (long unsigned int)(currentString - stringDataStart), blobString->length, blobString->length, blobString->data);

			/* NOTE stringTableDataSize includes the space for the lengths */
			currentString += blobString->length + sizeof(blobString->length) + padding;
			stringNum += 1;
		}
	}

	/* NOTE DDRGen uses a hash table to uniquify the string data while building
	 * the structure information.
	 */

	/* Read structs */
	{
		const uint8_t *const stringDataStart = blobBuffer + sizeof(blobHdrV1) + blobHdrV1.structDataSize;
#define BLOBSTRING_AT(offset) \
		((BlobString *)(stringDataStart + (offset)))

		/* set offset to start of struct data */
		const uint8_t *currentStruct = blobBuffer + sizeof(blobHdrV1);

		uint32_t structCount = 1;
		vector<Structure *> structs;
		while (structCount <= blobHdrV1.structureCount) {
			const BlobStruct *blobStruct = (const BlobStruct *)currentStruct;
			currentStruct += sizeof(BlobStruct);

			Structure *builtStruct = (Structure *)malloc(sizeof(Structure));
			memset(builtStruct, 0, sizeof(Structure));

			builtStruct->name = BLOBSTRING_AT(blobStruct->nameOffset)->data;
			builtStruct->nameLength = BLOBSTRING_AT(blobStruct->nameOffset)->length;
			if ((uint32_t)-1 == blobStruct->superName) {
				builtStruct->superName = NULL;
				builtStruct->superNameLength = 0;
			} else {
				builtStruct->superName = BLOBSTRING_AT(blobStruct->superName)->data;
				builtStruct->superNameLength = BLOBSTRING_AT(blobStruct->superName)->length;
			}
			builtStruct->size = blobStruct->sizeOf;

			uint32_t fieldCount = 1;
			while (fieldCount <= blobStruct->fieldCount) {
				const BlobField *blobField = (const BlobField *)currentStruct;
				currentStruct += sizeof(BlobField);

				Field *field = (Field *)malloc(sizeof(Field));
				field->name = BLOBSTRING_AT(blobField->declaredName)->data;
				field->nameLength = BLOBSTRING_AT(blobField->declaredName)->length;
				field->type = BLOBSTRING_AT(blobField->declaredType)->data;
				field->typeLength = BLOBSTRING_AT(blobField->declaredType)->length;
				field->offset = blobField->offset;

				builtStruct->fields.push_back(field);
				fieldCount += 1;
			}

			uint32_t constCount = 1;
			while (constCount <= blobStruct->constCount) {
				const BlobConstant *blobConst = (const BlobConstant *)currentStruct;
				currentStruct += sizeof(BlobConstant);

				Constant *constant = (Constant *)malloc(sizeof(Constant));
				constant->name = BLOBSTRING_AT(blobConst->name)->data;
				constant->nameLength = BLOBSTRING_AT(blobConst->name)->length;
				constant->value = *(uint64_t *)blobConst->value;

				builtStruct->constants.push_back(constant);
				constCount += 1;
			}

			structs.push_back(builtStruct);
			structCount += 1;
		}
		sort(structs.begin(), structs.end(), compareStructs);

		printf("\n== STRUCTS ==\n");
		for (size_t i = 0; i < structs.size(); i += 1) {
			Structure *builtStruct = structs[i];
			printf("\nStruct name: %.*s\n",
				   builtStruct->nameLength,
				   builtStruct->name);
			if (NULL == builtStruct->superName) {
				printf(" no superName\n");
			} else {
				printf(" superName: %.*s\n",
					   builtStruct->superNameLength,
					   builtStruct->superName);
			}
			printf(" sizeOf: %u\n"
				   " fieldCount: %lu\n"
				   " constCount: %lu\n",
				   builtStruct->size,
				   (unsigned long int)builtStruct->fields.size(),
				   (unsigned long int)builtStruct->constants.size());

			/* Print fields in the struct */
			for (size_t j = 0; j < builtStruct->fields.size(); j += 1) {
				Field *field = builtStruct->fields[j];

				printf(" Field declaredName: %.*s\n",
					   field->nameLength,
					   field->name);
				printf("  declaredType: %.*s\n",
					   field->typeLength,
					   field->type);
				printf("  offset: %u\n", field->offset);
				free(field);
			}

			/* Print consts in the struct */
			for (size_t j = 0; j < builtStruct->constants.size(); j += 1) {
				Constant *constant = builtStruct->constants[j];

				printf(" Constant name: %.*s\n",
					   constant->nameLength,
					   constant->name);
				printf("  value: %lld\n", (long long unsigned int)constant->value);
				free(constant);
			}
			free(builtStruct);
		}
#undef BLOBSTRING_AT
	}

	/* free the blob buffer */
	free(blobBuffer);
	blobBuffer = NULL;
	return 0;
}
