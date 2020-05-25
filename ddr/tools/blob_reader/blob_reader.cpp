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

#include "ddr/config.hpp"

#include <algorithm>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
#include "atoe.h"
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

#include "omrport.h"
#include "thread_api.h"

using std::vector;

static void
swap_bytes(uint8_t *left, uint8_t *right)
{
	uint8_t temp = *left;

	*left = *right;
	*right = temp;
}

static void
swap_u16(uint16_t *value)
{
	uint8_t *bytes = (uint8_t *)value;

	swap_bytes(&bytes[0], &bytes[1]);
}

static void
swap_u32(uint32_t *value)
{
	uint8_t *bytes = (uint8_t *)value;

	swap_bytes(&bytes[0], &bytes[3]);
	swap_bytes(&bytes[1], &bytes[2]);
}

struct BlobHeaderV1 {
	uint32_t coreVersion;
	uint8_t sizeofBool;
	uint8_t sizeofUDATA;
	uint8_t bitfieldFormat;
	uint8_t padding;
	uint32_t structDataSize;
	uint32_t stringTableDataSize;
	uint32_t structureCount;

	void endian_swap()
	{
		swap_u32(&coreVersion);
		swap_u32(&structDataSize);
		swap_u32(&stringTableDataSize);
		swap_u32(&structureCount);
	}
};

struct BlobString {
	uint16_t length;
	char data[1]; /* flexible array member */

	void endian_swap()
	{
		swap_u16(&length);
	}
};

struct BlobStruct {
	uint32_t nameOffset;
	uint32_t superName;
	uint32_t sizeOf;
	uint32_t fieldCount;
	uint32_t constCount;

	void endian_swap()
	{
		swap_u32(&nameOffset);
		swap_u32(&superName);
		swap_u32(&sizeOf);
		swap_u32(&fieldCount);
		swap_u32(&constCount);
	}
};

struct BlobField {
	uint32_t declaredName;
	uint32_t declaredType;
	uint32_t offset;

	void endian_swap()
	{
		swap_u32(&declaredName);
		swap_u32(&declaredType);
		swap_u32(&offset);
	}
};

struct BlobConstant {
	uint32_t name;
	/* don't declare value as uint64_t to ensure that no padding is added between it and name */
	uint32_t value[2];

	void endian_swap()
	{
		swap_u32(&name);

		/* value may not be properly aligned to be treated as uint64_t - swap pairs of bytes */
		uint8_t *bytes = (uint8_t *)value;

		swap_bytes(&bytes[0], &bytes[7]);
		swap_bytes(&bytes[1], &bytes[6]);
		swap_bytes(&bytes[2], &bytes[5]);
		swap_bytes(&bytes[3], &bytes[4]);
	}
};

struct Constant {
	const char *name;
	uint32_t nameLength;
	uint64_t value;
};

struct Field {
	const char *name;
	uint32_t nameLength;
	const char *type;
	uint32_t typeLength;
	uint32_t offset;
};

struct Structure {
	const char *name;
	uint32_t nameLength;
	const char *superName;
	uint32_t superNameLength;
	uint32_t size;
	vector<Field *> fields;
	vector<Constant *> constants;

	Structure()
		: name(NULL)
		, nameLength(0)
		, superName(NULL)
		, superNameLength(0)
		, size(0)
		, fields()
		, constants()
	{
	}
};

static bool
compareStructs(const Structure *first, const Structure *second)
{
	return strcmp(first->name, second->name) < 0;
}

int
main(int argc, char *argv[])
{
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	/* Convert EBCDIC to UTF-8 (ASCII) */
	if (-1 == iconv_init()) {
		fprintf(stderr, "failed to initialize iconv\n");
		return -1;
	}

	/* translate argv strings to ASCII */
	for (int i = 0; i < argc; ++i) {
		argv[i] = e2a_string(argv[i]);
		if (NULL == argv[i]) {
			fprintf(stderr, "failed to convert argument #%d from EBCDIC to ASCII\n", i);
			return -1;
		}
	}
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */

	omrthread_attach(NULL);

	OMRPortLibrary portLibrary;

	if (0 != omrport_init_library(&portLibrary, sizeof(portLibrary))) {
		fprintf(stderr, "failed to initalize port library\n");
		return -1;
	}

	OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);
	int rc = -1;
	intptr_t fd = -1;
	size_t rb = 0;
	uint8_t *blobBuffer = NULL;
	BlobHeaderV1 blobHdrV1;

	memset(&blobHdrV1, 0, sizeof(blobHdrV1));

	if (argc < 2) {
		omrfile_printf(OMRPORT_TTY_ERR, "Please specify a blob filename.\n");
		omrfile_printf(OMRPORT_TTY_ERR, "Usage: %s <blobfile>\n", argv[0]);
		goto done;
	}

	fd = omrfile_open(argv[1], EsOpenRead, 0);
	if (fd < 0) {
		omrfile_printf(OMRPORT_TTY_ERR, "fopen failed: %s\n", strerror(errno));
		goto done;
	}

	/* read header */
	rb = omrfile_read(fd, &blobHdrV1, sizeof(blobHdrV1));
	if (sizeof(blobHdrV1) != rb) {
		omrfile_printf(OMRPORT_TTY_ERR, "read blob header returned %zu, expected %zu\n", rb, sizeof(blobHdrV1));
	} else {
		/* Data is written in the blob in the natural byte order of the originating system.
		 * All version numbers thus far are small: a large version number is interpreted as
		 * a mismatch between the byte order of this system and the originating system.
		 */
		const bool swapsNeeded = blobHdrV1.coreVersion > (uint32_t) 0xFFFF;

		if (swapsNeeded) {
			blobHdrV1.endian_swap();
		}

		omrfile_printf(OMRPORT_TTY_OUT, "Blob Header:\n"
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

		/* read the file into memory */
		const size_t blobLength = blobHdrV1.structDataSize + blobHdrV1.stringTableDataSize;
		blobBuffer = (uint8_t *)malloc(blobLength);
		if (NULL == blobBuffer) {
			omrfile_printf(OMRPORT_TTY_ERR, "malloc blobBuffer failed\n");
			goto done;
		}

		rb = omrfile_read(fd, blobBuffer, blobLength);
		if (blobLength != rb) {
			omrfile_printf(OMRPORT_TTY_ERR, "read blob data returned %zu, expected %zu\n", rb, blobLength);
			goto done;
		}

		/* read strings */
		{
			omrfile_printf(OMRPORT_TTY_OUT, "\n== STRINGS ==\n");

			const uint8_t * const stringDataStart = blobBuffer + blobHdrV1.structDataSize;
			const uint8_t * const stringDataEnd = stringDataStart + blobHdrV1.stringTableDataSize;
			const uint8_t *currentString = stringDataStart;

			for (uint32_t stringNum = 1; currentString < stringDataEnd; ++stringNum) {
				BlobString *blobString = (BlobString *)currentString;

				if (swapsNeeded) {
					blobString->endian_swap();
				}

				/* NOTE string lengths appear to be padded to an even length */
				const uint16_t padding = blobString->length & 1;

				/* The format of the printed list is:
				 * #: <offset in string data> [<string length>] <string data>
				 */
				omrfile_printf(OMRPORT_TTY_OUT, "%5u: %8zx [%zu] %.*s\n", stringNum, (uintptr_t)(currentString - stringDataStart), blobString->length, blobString->length, blobString->data);

				/* NOTE stringTableDataSize includes the space for the lengths */
				currentString += blobString->length + sizeof(blobString->length) + padding;
			}
		}

		/* read structuress */
		{
			const uint8_t * const stringDataStart = blobBuffer + blobHdrV1.structDataSize;
	#define BLOBSTRING_AT(offset) \
			((const BlobString *)(stringDataStart + (offset)))

			/* set offset to start of struct data */
			const uint8_t *currentStruct = blobBuffer;

			vector<Structure *> structs;
			for (uint32_t structCount = 1; structCount <= blobHdrV1.structureCount; ++structCount) {
				BlobStruct *blobStruct = (BlobStruct *)currentStruct;

				if (swapsNeeded) {
					blobStruct->endian_swap();
				}

				currentStruct += sizeof(BlobStruct);

				Structure *builtStruct = new Structure;

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

				for (uint32_t fieldCount = 1; fieldCount <= blobStruct->fieldCount; ++fieldCount) {
					BlobField *blobField = (BlobField *)currentStruct;

					if (swapsNeeded) {
						blobField->endian_swap();
					}

					currentStruct += sizeof(BlobField);

					Field *field = new Field;

					field->name = BLOBSTRING_AT(blobField->declaredName)->data;
					field->nameLength = BLOBSTRING_AT(blobField->declaredName)->length;
					field->type = BLOBSTRING_AT(blobField->declaredType)->data;
					field->typeLength = BLOBSTRING_AT(blobField->declaredType)->length;
					field->offset = blobField->offset;

					builtStruct->fields.push_back(field);
				}

				for (uint32_t constCount = 1; constCount <= blobStruct->constCount; ++constCount) {
					BlobConstant *blobConst = (BlobConstant *)currentStruct;

					if (swapsNeeded) {
						blobConst->endian_swap();
					}

					currentStruct += sizeof(BlobConstant);

					Constant *constant = new Constant;

					constant->name = BLOBSTRING_AT(blobConst->name)->data;
					constant->nameLength = BLOBSTRING_AT(blobConst->name)->length;
					constant->value = *(uint64_t *)blobConst->value;

					builtStruct->constants.push_back(constant);
				}

				structs.push_back(builtStruct);
			}
			sort(structs.begin(), structs.end(), compareStructs);

			omrfile_printf(OMRPORT_TTY_OUT, "\n== STRUCTS ==\n");
			for (size_t i = 0; i < structs.size(); ++i) {
				Structure *builtStruct = structs[i];
				omrfile_printf(OMRPORT_TTY_OUT, "\nStruct name: %.*s\n",
						builtStruct->nameLength,
						builtStruct->name);
				if (NULL == builtStruct->superName) {
					omrfile_printf(OMRPORT_TTY_OUT, " no superName\n");
				} else {
					omrfile_printf(OMRPORT_TTY_OUT, " superName: %.*s\n",
							builtStruct->superNameLength,
							builtStruct->superName);
				}
				omrfile_printf(OMRPORT_TTY_OUT, " sizeOf: %zu\n"
						" fieldCount: %zu\n"
						" constCount: %zu\n",
						builtStruct->size,
						builtStruct->fields.size(),
						builtStruct->constants.size());

				/* print fields in the structure */
				for (size_t j = 0; j < builtStruct->fields.size(); ++j) {
					Field *field = builtStruct->fields[j];

					omrfile_printf(OMRPORT_TTY_OUT, " Field declaredName: %.*s\n",
							field->nameLength,
							field->name);
					omrfile_printf(OMRPORT_TTY_OUT, "  declaredType: %.*s\n",
							field->typeLength,
							field->type);
					omrfile_printf(OMRPORT_TTY_OUT, "  offset: %u\n", field->offset);
					delete field;
				}

				/* print constants in the structure */
				for (size_t j = 0; j < builtStruct->constants.size(); ++j) {
					Constant *constant = builtStruct->constants[j];

					omrfile_printf(OMRPORT_TTY_OUT, " Constant name: %.*s\n",
							constant->nameLength,
							constant->name);
					omrfile_printf(OMRPORT_TTY_OUT, "  value: %llu\n", constant->value);
					delete constant;
				}
				delete builtStruct;
			}
	#undef BLOBSTRING_AT
		}
	}

	rc = 0;

done:
	if (fd >= 0) {
		omrfile_close(fd);
		fd = -1;
	}

	if (NULL != blobBuffer) {
		free(blobBuffer);
		blobBuffer = NULL;
	}

	return rc;
}
