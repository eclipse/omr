/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#include "ddr/scanner/dwarf/DwarfScanner.hpp"

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Field.hpp"
#include "ddr/ir/NamespaceUDT.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/UnionUDT.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include "ddr/std/sstream.hpp"
#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <string.h>
#include <functional>
#include <utility>

class DwarfVisitor : public TypeVisitor
{
private:
	DwarfScanner * const _scanner;
	NamespaceUDT * const _outerUDT;
	Dwarf_Die const _die;

public:
	DwarfVisitor(DwarfScanner *scanner, NamespaceUDT *outerUDT, Dwarf_Die die)
		: _scanner(scanner)
		, _outerUDT(outerUDT)
		, _die(die)
	{
	}

	virtual DDR_RC visitType(Type *type) const;
	virtual DDR_RC visitClass(ClassUDT *type) const;
	virtual DDR_RC visitEnum(EnumUDT *type) const;
	virtual DDR_RC visitNamespace(NamespaceUDT *type) const;
	virtual DDR_RC visitTypedef(TypedefUDT *type) const;
	virtual DDR_RC visitUnion(UnionUDT *type) const;
};

DwarfScanner::DwarfScanner()
	: _fileNameCount(0), _fileNamesTable(NULL), _ir(NULL), _debug(NULL)
{
}

DDR_RC
DwarfScanner::getSourcelist(Dwarf_Die die)
{
	if (NULL != _fileNamesTable) {
		/* Free the memory from the previous CU, if allocated. */
		for (Dwarf_Signed i = 0; i < _fileNameCount; ++i) {
			free(_fileNamesTable[i]);
		}
		free(_fileNamesTable);
		_fileNamesTable = NULL;
		_fileNameCount = 0;
	}

	char **fileNamesTableConcat = NULL;
	char *compDir = NULL;
	Dwarf_Bool hasAttr = false;
	Dwarf_Error error = NULL;
	Dwarf_Attribute attr = NULL;

	if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_comp_dir, &hasAttr, &error)) {
		ERRMSG("Checking for compilation directory attribute: %s\n", dwarf_errmsg(error));
		goto Failed;
	}

	_fileNamesTable = NULL;
	_fileNameCount = 0;

	/* Get the dwarf source file names. */
	if (DW_DLV_ERROR == dwarf_srcfiles(die, &_fileNamesTable, &_fileNameCount, &error)) {
		ERRMSG("Failed to get list of source files: %s\n", dwarf_errmsg(error));
		goto Done;
	} else if (_fileNameCount < 0) {
		ERRMSG("List of source files has negative length: %lld\n", _fileNameCount);
		goto Done;
	}

	if (!hasAttr) {
		/* The DIE didn't have a compilationDirectory attribute so we can skip
		 * over getting the absolute paths from the relative paths.  AIX does
		 * not provide this attribute.
		 */
		goto Done;
	} else {
		/* Get the CU directory. */
		if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_comp_dir, &attr, &error)) {
			ERRMSG("Getting compilation directory attribute: %s\n", dwarf_errmsg(error));
			goto Failed;
		}

		if (DW_DLV_ERROR == dwarf_formstring(attr, &compDir, &error)) {
			ERRMSG("Getting compilation directory string: %s\n", dwarf_errmsg(error));
			goto Failed;
		}
	}

	/* Allocate a new file name table to hold the concatenated absolute paths. */
	fileNamesTableConcat = (char **)malloc(sizeof(char *) * _fileNameCount);
	if (NULL == fileNamesTableConcat) {
		ERRMSG("Failed to allocate file name table.\n");
		goto Failed;
	}
	memset(fileNamesTableConcat, 0, sizeof(char *) * _fileNameCount);
	for (Dwarf_Signed i = 0; i < _fileNameCount; ++i) {
		char *path = _fileNamesTable[i];
		if (0 == strncmp(path, "../", 3)) {
			/* For each file name beginning with "../", reformat the path as an absolute path. */
			fileNamesTableConcat[i] = (char *)malloc(strlen(compDir) + strlen(path) + 2);
			strcpy(fileNamesTableConcat[i], compDir);

			char *pathParent = path;
			while (NULL != pathParent) {
				pathParent = strstr(pathParent, "../");
				if (NULL != pathParent) {
					pathParent += 3;
					(*strrchr(fileNamesTableConcat[i], '/')) = '\0';
					path = pathParent;
				}
			}
			strcat(fileNamesTableConcat[i], "/");
			strcat(fileNamesTableConcat[i], path);
		} else if (0 == strncmp(path, "/", 1)) {
			fileNamesTableConcat[i] = (char *)malloc(strlen(path) + 1);
			strcpy(fileNamesTableConcat[i], path);
		} else {
			fileNamesTableConcat[i] = (char *)malloc(strlen(compDir) + strlen(path) + 2);
			strcpy(fileNamesTableConcat[i], compDir);
			strcat(fileNamesTableConcat[i], "/");
			strcat(fileNamesTableConcat[i], path);
		}
		dwarf_dealloc(_debug, _fileNamesTable[i], DW_DLA_STRING);
	}
	if (NULL != _fileNamesTable) {
		dwarf_dealloc(_debug, _fileNamesTable, DW_DLA_LIST);
	}
	dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	_fileNamesTable = fileNamesTableConcat;

Done:
	return DDR_RC_OK;

Failed:
	if (NULL != attr) {
		dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
	}
	if (NULL != _fileNamesTable) {
		for (Dwarf_Signed i = 0; i < _fileNameCount; ++i) {
			dwarf_dealloc(_debug, _fileNamesTable[i], DW_DLA_STRING);
		}
		dwarf_dealloc(_debug, _fileNamesTable, DW_DLA_LIST);
		_fileNamesTable = NULL;
		_fileNameCount = 0;
	}
	if (NULL != fileNamesTableConcat) {
		for (Dwarf_Signed i = 0; i < _fileNameCount; ++i) {
			if (NULL != fileNamesTableConcat[i]) {
				free(fileNamesTableConcat[i]);
			} else {
				break;
			}
		}
		free(fileNamesTableConcat);
	}
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return DDR_RC_ERROR;
}

DDR_RC
DwarfScanner::blackListedDie(Dwarf_Die die, bool *dieBlackListed)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Error err = NULL;
	bool blacklistedFile = true;
	bool blacklistedType = true;

	/* Check if the Die has the decl_file attribute. */
	if (_blacklistedFiles.empty()) {
		blacklistedFile = false;
	} else {
		Dwarf_Bool hasDeclFile = false;
		if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_decl_file, &hasDeclFile, &err)) {
			ERRMSG("Checking if die has attribute decl_file: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		if (!hasDeclFile) {
			blacklistedFile = false;
		} else {
			/* If the Die has the attribute, then get the attribute and its value. */
			Dwarf_Attribute attr = NULL;
			if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_decl_file, &attr, &err)) {
				ERRMSG("Getting attr decl_file: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto Done;
			}

			Dwarf_Unsigned declFile = 0;
			int ret = dwarf_formudata(attr, &declFile, &err);
			dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
			if (DW_DLV_ERROR == ret) {
				ERRMSG("Getting attr value decl_file: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto Done;
			}

			/* If the decl_file matches an entry in the file table not beginning with "/usr/",
			 * then we are interested in it. Also filter out decl file "<built-in>".
			 * Futhermore, don't filter out entries that have an unspecified declaring file.
			 */
			if (0 == declFile) {
				/* declFile can be 0 when no declaring file is specified */
				blacklistedFile = false;
			} else if (declFile <= (Dwarf_Unsigned)_fileNameCount) {
				const string fileName(_fileNamesTable[declFile - 1]);
				blacklistedFile = checkBlacklistedFile(fileName);
			} else {
				ERRMSG("Invalid decl_file value: %llu\n", declFile);
				rc = DDR_RC_ERROR;
				goto Done;
			}
		}
	}

	/* Check if the die's name is blacklisted. */
	if ((DDR_RC_OK == rc) && !blacklistedFile) {
		string dieName = "";
		rc = getName(die, &dieName);
		if (DDR_RC_OK == rc) {
			blacklistedType = checkBlacklistedType(dieName);
		}
	}

	*dieBlackListed = blacklistedType || blacklistedFile;

Done:
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfScanner::getName(Dwarf_Die die, string *name, Dwarf_Off *dieOffset)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Error err = NULL;

	if (NULL != dieOffset) {
		if (DW_DLV_ERROR == dwarf_dieoffset(die, dieOffset, &err)) {
			ERRMSG("Failed to get dwarf die offset: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
		}
	}

	if ((DDR_RC_OK == rc) && (NULL != name)) {
		char *dieName = NULL;
		int dwarfRC = dwarf_diename(die, &dieName, &err);

		if (DW_DLV_ERROR == dwarfRC) {
			ERRMSG("Error in dwarf_diename: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
		} else if (DW_DLV_NO_ENTRY == dwarfRC) {
			/* If the Die has no name attribute, check for specification reference instead. */
			Dwarf_Bool hasAttr = false;
			if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_specification, &hasAttr, &err)) {
				ERRMSG("Checking if die has specification attribute: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto NameDone;
			}
			if (hasAttr) {
				/* If it has a specification attribute, check the specification for a name recursively. */
				Dwarf_Attribute attr = NULL;
				if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_specification, &attr, &err)) {
					ERRMSG("Getting specification attribute: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
					goto NameDone;
				}
				Dwarf_Off offset = 0;
				int ret = dwarf_global_formref(attr, &offset, &err);
				dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
				if (DW_DLV_ERROR == ret) {
					ERRMSG("Getting formref of specification: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
					goto NameDone;
				}
				Dwarf_Die spec = NULL;
				if (
#if defined(J9ZOS390)
					DW_DLV_ERROR == dwarf_offdie(_debug, offset, &spec, &err)
#else /* defined(J9ZOS390) */
					DW_DLV_ERROR == dwarf_offdie_b(_debug, offset, 1, &spec, &err)
#endif /* !defined(J9ZOS390) */
				) {
					ERRMSG("Getting die from specification offset: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
					goto NameDone;
				}
				rc = getName(spec, name, dieOffset);
				dwarf_dealloc(_debug, spec, DW_DLA_DIE);
				goto NameDone;
			}
		}
		if ((NULL == dieName)
#if defined(LINUXPPC)
			|| ((0 == strncmp(dieName, "__", 2)) && (strlen(dieName + 2) == strspn(dieName + 2, "0123456789")))
#endif /* defined(LINUXPPC) */
			|| (0 == strncmp(dieName, "<anonymous", 10)))
		{
			*name = "";
		} else {
			*name = string(dieName);
		}
		dwarf_dealloc(_debug, dieName, DW_DLA_STRING);
		if (NULL != dieOffset) {
			DEBUGPRINTF("Name of %llx = %s", (long long)*dieOffset, name->c_str());
		} else {
			DEBUGPRINTF("Name = %s", name->c_str());
		}
	}
NameDone:
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfScanner::getBitField(Dwarf_Die die, size_t *bitField)
{
	DDR_RC rc = DDR_RC_OK;
	if (NULL != bitField) {
		*bitField = 0;
		Dwarf_Error err = NULL;
		Dwarf_Bool hasAttr = false;

		/* Get the bit field attribute from a member Die if it exists. */
		if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_bit_size, &hasAttr, &err)) {
			ERRMSG("Checking if die has bit size attribute: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		if (hasAttr) {
			Dwarf_Attribute attr = NULL;
			if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_bit_size, &attr, &err)) {
				ERRMSG("Getting bit size attribute: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto Done;
			}
			Dwarf_Off bitSize = 0;
			int ret = dwarf_formudata(attr, &bitSize, &err);
			dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
			if (DW_DLV_ERROR == ret) {
				ERRMSG("Getting formudata of bit size: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto Done;
			}
			*bitField = bitSize;
		}
	}
Done:
	return rc;
}

/* Get modifier, array, pointer, and type information for a Die. */
DDR_RC
DwarfScanner::getTypeInfo(Dwarf_Die die, Dwarf_Die *dieOut, string *typeName, Modifiers *modifiers, size_t *typeSize, size_t *bitField)
{
	Dwarf_Error err = NULL;
	DDR_RC rc = DDR_RC_ERROR;
	bool done = false;
	bool foundTypedef = false;
	modifiers->_modifierFlags = Modifiers::NO_MOD;

	/* Get the bit field from the member Die before getting the type. */
	if (DDR_RC_OK == getBitField(die, bitField)) {
		Dwarf_Die typeDie = die;
		Dwarf_Half tag = 0;
		/* Get all the field tags. */
		do {
			/* Get the tag for the die type if available. Enum members have no type.
			 * Failing to find a new type tag is not necessarily an error.
			 */
			if ((DDR_RC_OK != getTypeTag(typeDie, &typeDie, &tag)) || (NULL == typeDie)) {
				rc = DDR_RC_OK;
				break;
			}

			/* Get the type or modifier based on the tag. */
			if (DW_TAG_const_type == tag) {
				modifiers->_modifierFlags |= Modifiers::CONST_TYPE;
			} else if (DW_TAG_restrict_type == tag) {
				modifiers->_modifierFlags |= Modifiers::RESTRICT_TYPE;
			} else if (DW_TAG_volatile_type == tag) {
				modifiers->_modifierFlags |= Modifiers::VOLATILE_TYPE;
			} else if (DW_TAG_shared_type == tag) {
				modifiers->_modifierFlags |= Modifiers::SHARED_TYPE;
			} else if (DW_TAG_array_type == tag && !foundTypedef) {
				Dwarf_Die child = NULL;
				Dwarf_Unsigned upperBound = 0;

				/* Get the array size if it is an array. */
				if (DW_DLV_ERROR == dwarf_child(typeDie, &child, &err)) {
					ERRMSG("Error getting child Die of array type: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
					break;
				}

				do {
					Dwarf_Bool hasAttr = false;
					if (DW_DLV_ERROR == dwarf_hasattr(child, DW_AT_upper_bound, &hasAttr, &err)) {
						ERRMSG("Checking array for upper bound attribute: %s\n", dwarf_errmsg(err));
						rc = DDR_RC_ERROR;
						break;
					}
					if (hasAttr) {
						Dwarf_Attribute attr = NULL;
						if (DW_DLV_ERROR == dwarf_attr(child, DW_AT_upper_bound, &attr, &err)) {
							ERRMSG("Array upper bound attribute: %s\n", dwarf_errmsg(err));
							rc = DDR_RC_ERROR;
							break;
						}
						int ret = dwarf_formudata(attr, &upperBound, &err);
						dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
						if (DW_DLV_ERROR == ret) {
							ERRMSG("Getting array upper bound value: %s\n", dwarf_errmsg(err));
							rc = DDR_RC_ERROR;
							break;
						}
						/* New array length is upperBound + 1. */
						modifiers->addArrayDimension(upperBound + 1);
					} else {
						/* Arrays declared as "[]" have no size attributes. */
						modifiers->addArrayDimension(0);
					}
				} while (DDR_RC_OK == getNextSibling(&child));
				dwarf_dealloc(_debug, child, DW_DLA_DIE);
				if (NULL != err) {
					break;
				}
			} else if ((DW_TAG_pointer_type == tag) && !foundTypedef) {
				modifiers->_pointerCount += 1;
				/* A "void*" type has the pointer tag, but no base type to derive a name.
				 * Pointers of other types will override this name with the base type name.
				 */
				rc = DDR_RC_OK;
				if (NULL != typeName) {
					*typeName = "void";
				}
				/* Be careful not to create a typedef cycle. */
				if ((NULL != dieOut) && (die != typeDie)) {
					*dieOut = typeDie;
				}
			} else if (DW_TAG_reference_type == tag) {
				modifiers->_referenceCount += 1;
			} else {
				/* Once the base type is reached, get its name and exit the loop. */
				/* Get type size. */
				rc = getTypeSize(typeDie, typeSize);
				if (DDR_RC_OK != rc) {
					break;
				}

				/* Once a typedef has been found, keep the name and do not replace it with base type's name. */
				if (!foundTypedef) {
					rc = getName(typeDie, typeName);
					if (DDR_RC_OK != rc) {
						break;
					}

					/* Returned die should be the first type without continuing to the base type through typedefs. */
					/* Be careful not to create a typedef cycle. */
					if ((NULL != dieOut) && (die != typeDie)) {
						*dieOut = typeDie;
					}
				}

				/* Once a typedef is found, keep the name and do not append more pointer/array notation, but
				 * otherwise keep searching the type references for type information.
				 */
				if (DW_TAG_typedef == tag) {
					foundTypedef = true;
				} else {
					done = true;
				}
			}
		} while (!done);
	}

	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

/* Get the tag and type Die associated with a given Die. */
DDR_RC
DwarfScanner::getTypeTag(Dwarf_Die die, Dwarf_Die *typeDie, Dwarf_Half *tag)
{
	DDR_RC rc = DDR_RC_ERROR;
	Dwarf_Error err = NULL;

	/* First, check if the die has a type attribute. */
	Dwarf_Bool hasAttr = false;
	if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_type, &hasAttr, &err)) {
		ERRMSG("Checking if die has type attr: %s\n", dwarf_errmsg(err));
		goto getTypeDone;
	}
	if (hasAttr) {
		/* Get the type attribute. */
		Dwarf_Attribute attr = NULL;
		if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_type, &attr, &err)) {
			ERRMSG("Getting attr of type: %s\n", dwarf_errmsg(err));
			goto getTypeDone;
		}

		/* Check the form of the attribute. We only want reference forms. */
		Dwarf_Half form = 0;
		if (DW_DLV_ERROR == dwarf_whatform(attr, &form, &err)) {
			ERRMSG("Getting attr form: %s\n", dwarf_errmsg(err));
			dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
			goto getTypeDone;
		} else if ((DW_FORM_ref1 != form)
				&& (DW_FORM_ref2 != form)
				&& (DW_FORM_ref4 != form)
				&& (DW_FORM_ref8 != form)
		) {
			ERRMSG("Unexpected dwarf form %d\n", form);
			dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
			goto getTypeDone;
		}

		/* Get the type reference, which is an offset. */
		Dwarf_Off offset = 0;
		int ret = dwarf_global_formref(attr, &offset, &err);
		dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
		if (DW_DLV_ERROR == ret) {
			ERRMSG("Getting formref of type: %s\n", dwarf_errmsg(err));
			goto getTypeDone;
		}

		/* Use the offset to get the Die containing the type tag. */
		Dwarf_Die newDie = NULL;
		if (
#if defined(J9ZOS390)
			/* z/OS dwarf library doesn't have dwarf_offdie_b, only dwarf_offdie. */
			DW_DLV_ERROR == dwarf_offdie(_debug, offset, &newDie, &err)
#else /* defined(J9ZOS390) */
			DW_DLV_ERROR == dwarf_offdie_b(_debug, offset, 1, &newDie, &err)
#endif /* defined(J9ZOS390) */
		) {
			ERRMSG("Getting typedie from type offset: %s\n", dwarf_errmsg(err));
			goto getTypeDone;
		} else {
			*typeDie = newDie;
		}

		/* Get the tag for this type. */
		if (DW_DLV_ERROR == dwarf_tag(*typeDie, tag, &err)) {
			ERRMSG("Getting tag from type die: %s\n", dwarf_errmsg(err));
			goto getTypeDone;
		}
		rc = DDR_RC_OK;
	} else {
		*typeDie = NULL;
		rc = DDR_RC_OK;
	}
getTypeDone:
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

/* Find the size of a type from a Die. */
DDR_RC
DwarfScanner::getTypeSize(Dwarf_Die die, size_t *typeSize)
{
	DDR_RC rc = DDR_RC_OK;
	if (NULL != typeSize) {
		Dwarf_Error err = NULL;
		Dwarf_Bool hasAttr = false;
		*typeSize = 0;

		/* Get and return the byte_size attribute from the Die if available. */
		if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_byte_size, &hasAttr, &err)) {
			ERRMSG("Checking if die has byte size attribute: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
		} else if (hasAttr) {
			Dwarf_Attribute attr = NULL;
			if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_byte_size, &attr, &err)) {
				ERRMSG("Getting size attr of type: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
			} else {
				Dwarf_Unsigned byteSize = 0;
				int ret = dwarf_formudata(attr, &byteSize, &err);
				dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
				if (DW_DLV_ERROR == ret) {
					ERRMSG("Getting size attr value: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
				} else {
					*typeSize = byteSize;
				}
			}
		} else {
			/* If the Die does not have a size attribute, get its type and repeat. */
			Dwarf_Half tag = 0;
			Dwarf_Die typeDie = NULL;
			if (DDR_RC_OK == getTypeTag(die, &typeDie, &tag)) {
				if (NULL != typeDie) {
					rc = getTypeSize(typeDie, typeSize);
					dwarf_dealloc(_debug, typeDie, DW_DLA_DIE);
				}
			} else {
				rc = DDR_RC_ERROR;
			}
		}

		if (NULL != err) {
			dwarf_dealloc(_debug, err, DW_DLA_ERROR);
		}
	}
	return rc;
}

/* Given a Die for a enum/struct/class/union/typedef, get its type and add it to
 * a map of all types if it has not been found before. The type is added to the IR
 * the first time it is discovered. The associated Type object is returned.
 */
DDR_RC
DwarfScanner::addDieToIR(Dwarf_Die die, Dwarf_Half tag, NamespaceUDT *outerUDT, Type **type)
{
	DDR_RC rc = DDR_RC_OK;
	Type *newType = NULL;
	bool dieBlackListed = false;
	bool isSubUDT = (NULL != outerUDT);
	bool isNewType = false;

	rc = getOrCreateNewType(die, tag, &newType, outerUDT, &isNewType);

	if ((DDR_RC_OK == rc) && isNewType && (NULL == outerUDT)) {
		rc = blackListedDie(die, &dieBlackListed);
	}

	if (DDR_RC_OK == rc) {
		if (!isNewType) {
			/* Entry is for a type that has already been found. */
		} else if (dieBlackListed) {
			newType->_blacklisted = true;
		} else if ((DW_TAG_class_type == tag)
				|| (DW_TAG_structure_type == tag)
				|| (DW_TAG_union_type == tag)
				|| (DW_TAG_namespace == tag)
				|| (DW_TAG_enumeration_type == tag)
				|| (DW_TAG_typedef == tag)
		) {
			/* If the type was added as a stub with an outer type, such as is the case
			 * for types defined within namespaces, do not add it to the main list of types.
			 */
			isSubUDT = isSubUDT || (string::npos != newType->getFullName().find("::"));
			rc = newType->acceptVisitor(DwarfVisitor(this, outerUDT, die));
			DEBUGPRINTF("Done scanning child info for %s", newType->_name.c_str());
		}

		if (isSubUDT) {
			/* When the type is a stub that was found as a stub before, it might be found as a
			 * subUDT now, so we check if it is, and remove it from the main types list if it is.
			 */
			if (NULL == newType->getNamespace()) {
				_ir->_types.erase(remove(_ir->_types.begin(), _ir->_types.end(), newType), _ir->_types.end());

				/* Anonymous types would have been named incorrectly if they were
				 * found as a field before they were known to be an inner type.
				 */
				size_t nameLen = newType->_name.length();
				if ((nameLen > 9) && (0 == newType->_name.compare(nameLen - 9, 9, "Constants"))) {
					string newName = "";
					rc = getName(die, &newName);
					newType->_name = newName;
				}
			}
		} else if (isNewType && (NULL == newType->getNamespace())) {
			_ir->_types.push_back(newType);
		}
	}

	if (NULL != type) {
		*type = newType;
	}

	return rc;
}

/* Given a Die for a type, return the associated Type object for it. The first time
 * called for each type, this function will create a Type object and add it to the map.
 * On subsequent calls for a given type, the existing Type object in the map will be
 * returned.
 */
DDR_RC
DwarfScanner::getOrCreateNewType(Dwarf_Die die, Dwarf_Half tag, Type **newType, NamespaceUDT *outerUDT, bool *isNewType)
{
	string dieName = "";
	Dwarf_Off dieOffset = 0;
	DDR_RC rc = getName(die, &dieName, &dieOffset);

	if (DDR_RC_OK == rc) {
		unordered_map<Dwarf_Off, Type *>::const_iterator it = _typeOffsetMap.find(dieOffset);
		if (_typeOffsetMap.end() != it) {
			*newType = it->second;
			*isNewType = false;
		} else {
			*newType = NULL;
			rc = createNewType(die, tag, dieName.c_str(), newType);
			if (DDR_RC_OK == rc) {
				_typeOffsetMap[dieOffset] = *newType;
				*isNewType = true;
			}
		}
	}

	return rc;
}

/* Create and return a Type object of the correct subtype based on a dwarf tag. */
DDR_RC
DwarfScanner::createNewType(Dwarf_Die die, Dwarf_Half tag, const char *dieName, Type **newType)
{
	DDR_RC rc = DDR_RC_OK;
	size_t typeSize = 0;
	/* Get the UDT symbol type from the tag. */
	switch (tag) {
	case DW_TAG_union_type:
		DEBUGPRINTF("DW_TAG_union_type: '%s'", dieName);
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new UnionUDT(typeSize, 0);
		break;
	case DW_TAG_enumeration_type:
		DEBUGPRINTF("DW_TAG_enumeration_type: '%s'", dieName);
		*newType = new EnumUDT;
		break;
	case DW_TAG_class_type:
		DEBUGPRINTF("DW_TAG_class_type: '%s'", dieName);
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new ClassUDT(typeSize, true, 0);
		break;
	case DW_TAG_structure_type:
		DEBUGPRINTF("DW_TAG_structure_type: '%s'", dieName);
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new ClassUDT(typeSize, false, 0);
		break;
	case DW_TAG_namespace:
		DEBUGPRINTF("DW_TAG_namespace: '%s'", dieName);
		*newType = new NamespaceUDT(0);
		break;
	case DW_TAG_typedef:
		DEBUGPRINTF("createType: DW_TAG_typedef: '%s'", dieName);
		*newType = new TypedefUDT(0);
		break;
	case DW_TAG_base_type:
		DEBUGPRINTF("DW_TAG_base_type: '%s'", dieName);
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new Type(typeSize);
		break;
	/* Void types and function pointers: */
	case DW_TAG_subroutine_type:
	case DW_TAG_pointer_type:
	case DW_TAG_ptr_to_member_type:
		*newType = new Type(0);
		dieName = "void";
		break;
	default:
		{
			const char *tagName = NULL;
#if !defined(J9ZOS390)
			dwarf_get_TAG_name(tag, &tagName);
#endif /* !defined(J9ZOS390) */
			ERRMSG("Symbol with name '%s' has unknown symbol type: %s (%d)\n", dieName, tagName, tag);
			rc = DDR_RC_ERROR;
		}
		break;
	}
	if ((DDR_RC_OK == rc) && (NULL != *newType)) {
		(*newType)->_name = dieName;
	}
	return rc;
}

DDR_RC
DwarfVisitor::visitTypedef(TypedefUDT *newTypedef) const
{
	DDR_RC rc = DDR_RC_OK;
	if (NULL == newTypedef->_aliasedType) {
		Dwarf_Die typeDie = NULL;
		string typedefName = "";
		/* Find and add the typedef's modifiers. */
		rc = _scanner->getTypeInfo(_die, &typeDie, &typedefName, &newTypedef->_modifiers, &newTypedef->_sizeOf, NULL);

		/* Add a type for the typedef's type. */
		Type *typedefType = NULL;
		Dwarf_Half tag = 0;
		if ((DDR_RC_OK == rc) && (NULL != typeDie)) {
			/* Get the tag for this type. */
			Dwarf_Error err = NULL;
			if (DW_DLV_ERROR == dwarf_tag(typeDie, &tag, &err)) {
				ERRMSG("Getting tag from type die: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
			}
		}
		if ((DDR_RC_OK == rc) && (NULL != typeDie)) {
			rc = _scanner->addDieToIR(typeDie, tag, NULL, &typedefType);
			dwarf_dealloc(_scanner->_debug, typeDie, DW_DLA_DIE);
		}
		if (DDR_RC_OK == rc) {
			if ((NULL == typedefType) || (newTypedef != typedefType)) {
				newTypedef->_aliasedType = typedefType;
			} else {
				newTypedef->_aliasedType = NULL;
			}

			if (NULL != typedefType) {
				/* Set typedef's size to the size of its type. */
				newTypedef->_sizeOf = newTypedef->_modifiers.getSize(typedefType->_sizeOf);
				/* If the typedef's type has no associated name, let it assume the typedef's name. */
				if (typedefName.empty() && typedefType->_name.empty()) {
					typedefType->_name = newTypedef->_name;
				}
			}
		}
	}
	return rc;
}

/* An enum Die should only have children Die's for enum members. This function
 * gets the enum members by processing the children.
 */
DDR_RC
DwarfVisitor::visitEnum(EnumUDT *newUDT) const
{
	DDR_RC rc = DDR_RC_OK;

	if (newUDT->_enumMembers.empty()) {
		Dwarf_Die childDie = NULL;
		Dwarf_Error error = NULL;
		int dwRc = dwarf_child(_die, &childDie, &error);

		if (DW_DLV_ERROR == dwRc) {
			ERRMSG("Getting child of CU DIE: %s\n", dwarf_errmsg(error));
			rc = DDR_RC_ERROR;
		} else if (DW_DLV_NO_ENTRY == dwRc) {
			/* No Children. Do nothing. */
		} else if (DW_DLV_OK == dwRc) {
			/* Get child die and get it's get field name, type and type name.
			 * Iterate over Child's siblings to get remainder of fields and repeat.
			 */
			do {
				Dwarf_Half childTag = 0;
				if (DW_DLV_ERROR == dwarf_tag(childDie, &childTag, &error)) {
					ERRMSG("Getting tag from UDT child: %s\n", dwarf_errmsg(error));
					rc = DDR_RC_ERROR;
					break;
				}

				if (DW_TAG_enumerator == childTag) {
					/* The child is an enumerator member. */
					if (DDR_RC_OK != _scanner->addEnumMember(childDie, _outerUDT, newUDT)) {
						rc = DDR_RC_ERROR;
						break;
					}
				}
			} while (DDR_RC_OK == _scanner->getNextSibling(&childDie));
			dwarf_dealloc(_scanner->_debug, childDie, DW_DLA_DIE);
		}

		if (NULL != error) {
			dwarf_dealloc(_scanner->_debug, error, DW_DLA_ERROR);
		}
	}
	return rc;
}

DDR_RC
DwarfVisitor::visitNamespace(NamespaceUDT *newClass) const
{
	return _scanner->scanClassChildren(newClass, _die);
}

/*
 * Answer whether the given field name holds the compiler-generated virtual
 * function table pointer.
 */
static bool
isVtablePointer(const char *fieldName)
{
#if defined(__GNUC__)
	return 0 == strncmp(fieldName, "_vptr.", 6);
#elif defined(AIXPPC) || defined(LINUXPPC)
	return 0 == strcmp(fieldName, "__vfp");
#else
	return false;
#endif /* __GNUC__ */
}

/* A Die for a class/struct/union has children Die's for all of its properties,
 * including member fields, sub types, functions, and super type. This function
 * processes those properties to populate a NamespaceUDT or ClassUDT.
 */
DDR_RC
DwarfScanner::scanClassChildren(NamespaceUDT *newClass, Dwarf_Die die)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Die childDie = NULL;
	Dwarf_Error error = NULL;
	int dwRc = dwarf_child(die, &childDie, &error);

	if (DW_DLV_ERROR == dwRc) {
		ERRMSG("Getting child of CU DIE: %s\n", dwarf_errmsg(error));
		rc = DDR_RC_ERROR;
	} else if (DW_DLV_NO_ENTRY == dwRc) {
		/* No Children. Do nothing. */
	} else if (DW_DLV_OK == dwRc) {
		/* Get child die and get it's get field name, type and type name.
		 * Iterate over child's siblings to get remainder of fields and repeat.
		 */
		do {
			Dwarf_Half childTag = 0;
			if (DW_DLV_ERROR == dwarf_tag(childDie, &childTag, &error)) {
				ERRMSG("Getting tag from UDT child: %s\n", dwarf_errmsg(error));
				rc = DDR_RC_ERROR;
				break;
			}

			if (DW_TAG_subprogram == childTag) {
				/* Skip class functions. */
				continue;
			} else if ((DW_TAG_class_type == childTag)
					|| (DW_TAG_union_type == childTag)
					|| (DW_TAG_structure_type == childTag)
					|| (DW_TAG_enumeration_type == childTag)
					|| (DW_TAG_namespace == childTag)
			) {
				/* The child is an inner type. */
				UDT *innerUDT = NULL;
				if (DDR_RC_OK != addDieToIR(childDie, childTag, newClass, (Type **)&innerUDT)) {
					rc = DDR_RC_ERROR;
					break;
				} else if ((NULL != newClass) && (NULL != innerUDT) && (NULL == innerUDT->getNamespace())) {
					/* We only add it to list of subUDTs if innerUDT's _outerNamespace is NULL, because there should only be one outer UDT per inner UDT. */
					/* Check that the subUDT wasn't already added when the type was found as a stub type */
					if (newClass->_subUDTs.end() == std::find(newClass->_subUDTs.begin(), newClass->_subUDTs.end(), innerUDT)) {
						innerUDT->_outerNamespace = newClass;
						newClass->_subUDTs.push_back(innerUDT);
					}
				}
			} else if (DW_TAG_inheritance == childTag) {
				/* The child is a super type. */
				if (NULL == ((ClassUDT *)newClass)->_superClass) {
					rc = getSuperUDT(childDie, (ClassUDT *)newClass);
				}
			} else if (DW_TAG_member == childTag) {
				/* The child is a member field. */
				string fieldName = "";
				rc = getName(childDie, &fieldName);
				if (!isVtablePointer(fieldName.c_str())) {
					DEBUGPRINTF("fieldName: %s", fieldName.c_str());
					if (DDR_RC_OK != addClassField(childDie, (ClassType *)newClass, fieldName)) {
						rc = DDR_RC_ERROR;
						break;
					}
				}
			}
		} while (DDR_RC_OK == getNextSibling(&childDie));
		dwarf_dealloc(_debug, childDie, DW_DLA_DIE);
	}

	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfVisitor::visitType(Type *newType) const
{
	/* No-op: base types do not have children Die's to scan. */
	return DDR_RC_OK;
}

DDR_RC
DwarfVisitor::visitClass(ClassUDT *newType) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!newType->_isComplete) {
		newType->_isComplete = true;
		rc = _scanner->scanClassChildren(newType, _die);
	}
	return rc;
}

DDR_RC
DwarfVisitor::visitUnion(UnionUDT *newType) const
{
	DDR_RC rc = DDR_RC_OK;
	if (!newType->_isComplete) {
		newType->_isComplete = true;
		rc = _scanner->scanClassChildren(newType, _die);
	}
	return rc;
}

/* Add an enum member to an enum UDT from a Die. */
DDR_RC
DwarfScanner::addEnumMember(Dwarf_Die die, NamespaceUDT *outerUDT, EnumUDT *newEnum)
{
	Dwarf_Error error = NULL;
	DDR_RC rc = DDR_RC_OK;
	vector<EnumMember *> *members = NULL;

	/* literals of a nested anonymous enum are collected in the enclosing namespace */
	if ((NULL != outerUDT) && newEnum->isAnonymousType()) {
		members = &outerUDT->_enumMembers;
	} else {
		members = &newEnum->_enumMembers;
	}

	/* Check if an enum member with this name is already present. */
	string enumName = "";
	rc = getName(die, &enumName);

	bool memberAlreadyExists = false;
	for (vector<EnumMember *>::iterator m = members->begin(); m != members->end(); ++m) {
		if ((*m)->_name == enumName) {
			memberAlreadyExists = true;
			break;
		}
	}

	if (!memberAlreadyExists) {
		/* Create new enum member. */
		EnumMember *newEnumMember = new EnumMember;

		if (DDR_RC_OK != rc) {
			goto AddEnumMemberDone;
		}
		newEnumMember->_name = enumName;

		/* Get the value attribute of the enum. */
		Dwarf_Attribute attr = NULL;
		if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_const_value, &attr, &error)) {
			ERRMSG("Getting value attribute of enum: %s\n", dwarf_errmsg(error));
			rc = DDR_RC_ERROR;
			goto AddEnumMemberDone;
		}

		/* Get the enum const value. */
		Dwarf_Signed value = 0;
		if ((NULL != attr) && (DW_DLV_ERROR == dwarf_formsdata(attr, &value, &error))) {
			ERRMSG("Getting const value of enum: %s\n", dwarf_errmsg(error));
			rc = DDR_RC_ERROR;
			goto AddEnumMemberDone;
		}
		newEnumMember->_value = value;
		members->push_back(newEnumMember);
	}

AddEnumMemberDone:
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

/* Add a field to a class UDT from a Die. */
DDR_RC
DwarfScanner::addClassField(Dwarf_Die die, ClassType *newClass, const string &fieldName)
{
	DDR_RC rc = DDR_RC_ERROR;
	string typeName = "";
	size_t typeSize = 0;
	Dwarf_Error error = NULL;
	Dwarf_Die baseDie = NULL;
	Modifiers fieldModifiers = Modifiers();
	size_t bitField = 0;

	/* Follow the die through any type modifiers (pointer, volatile, etc) to
	 * get the field type.
	 */
	if (DDR_RC_OK == getTypeInfo(die, &baseDie, &typeName, &fieldModifiers, &typeSize, &bitField)
			&& ((NULL != baseDie) || !fieldName.empty())) {
		/* Get the field UDT. */
		Dwarf_Half tag = 0;
		if (NULL == baseDie) {
			ERRMSG("Missing field type for field: %s. Possible missing or corrupt dwarf info.\n", fieldName.c_str());
			typeName = "void";
			tag = DW_TAG_base_type;
		} else {
			if (DW_DLV_ERROR == dwarf_tag(baseDie, &tag, &error)) {
				ERRMSG("Getting field's tag: %s\n", dwarf_errmsg(error));
				goto AddUDTFieldDone;
			}
		}
		DEBUGPRINTF("typeName: %s", typeName.c_str());

		Field *newField = new Field;
		newClass->_fieldMembers.push_back(newField);
		newField->_modifiers = fieldModifiers;
		newField->_bitField = bitField;

		/* check if field is static (DW_AT_external=yes) */
		{
			Dwarf_Bool hasAttr = false;

			/* Get the external attribute from a member Die if it exists. */
			if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_external, &hasAttr, &error)) {
				ERRMSG("Checking if die has external attribute: %s\n", dwarf_errmsg(error));
				goto AddUDTFieldDone;
			}
			if (hasAttr) {
				Dwarf_Attribute attr = NULL;
				if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_external, &attr, &error)) {
					ERRMSG("Getting external attribute: %s\n", dwarf_errmsg(error));
					goto AddUDTFieldDone;
				}
				Dwarf_Bool isExternal = false;
				int ret = dwarf_formflag(attr, &isExternal, &error);
				dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
				if (DW_DLV_ERROR == ret) {
					ERRMSG("Getting formflag of external attribute: %s\n", dwarf_errmsg(error));
					goto AddUDTFieldDone;
				}
				if (isExternal) {
					newField->_isStatic = true;
				}
			}
		}

		if (!newField->_isStatic) {
			/* Get the offset (member_location) attribute. */
			Dwarf_Bool hasAttrBytes = false;
			if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_data_member_location, &hasAttrBytes, &error)) {
				ERRMSG("Checking if die has offset attribute: %s\n", dwarf_errmsg(error));
				goto AddUDTFieldDone;
			}
			Dwarf_Bool hasAttrBits = false;
			if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_data_bit_offset, &hasAttrBits, &error)) {
				ERRMSG("Checking if die has bit offset attribute: %s\n", dwarf_errmsg(error));
				goto AddUDTFieldDone;
			}
			if (hasAttrBytes || hasAttrBits) {
				Dwarf_Attribute attr = NULL;
				Dwarf_Unsigned offset = 0;
				Dwarf_Half attrType = hasAttrBytes ? DW_AT_data_member_location : DW_AT_data_bit_offset;
				if (DW_DLV_ERROR == dwarf_attr(die, attrType, &attr, &error)) {
					ERRMSG("Getting offset attribute: %s\n", dwarf_errmsg(error));
					goto AddUDTFieldDone;
				}
				int ret = dwarf_formudata(attr, &offset, &error);
				dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
				if (DW_DLV_ERROR == ret) {
					ERRMSG("Getting value of offset attribute: %s\n", dwarf_errmsg(error));
					goto AddUDTFieldDone;
				}
				newField->_offset = hasAttrBytes ? offset : (offset / 8);
			} else if ("union" != newClass->getSymbolKindName()) {
				ERRMSG("Missing offset attribute for %s.%s\n", newClass->_name.c_str(), fieldName.c_str());
				goto AddUDTFieldDone;
			}
		}

		if ((typeName == newClass->_name) && (!newClass->_name.empty())) {
			newField->_fieldType = newClass;
		} else if (NULL != baseDie) {
			if ((DDR_RC_OK != addDieToIR(baseDie, tag, NULL, &newField->_fieldType))
				|| (NULL == newField->_fieldType)) {
				goto AddUDTFieldDone;
			}
		}
		newField->_name = fieldName;
		rc = DDR_RC_OK;
		dwarf_dealloc(_debug, baseDie, DW_DLA_DIE);
	}
AddUDTFieldDone:
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

/* Find and return the super type from a Die. */
DDR_RC
DwarfScanner::getSuperUDT(Dwarf_Die die, ClassUDT *udt)
{
	DDR_RC rc = DDR_RC_ERROR;
	Dwarf_Half tag = 0;
	Dwarf_Die superTypeDie = NULL;

	if (DDR_RC_OK == getTypeTag(die, &superTypeDie, &tag)) {
		ClassUDT *superUDT = NULL;
		/* Get the super udt. */
		if ((DDR_RC_OK == addDieToIR(superTypeDie, tag, NULL, (Type **)&superUDT))
				&& (NULL != superUDT)) {
			rc = DDR_RC_OK;
			udt->_superClass = superUDT;
		}
		dwarf_dealloc(_debug, superTypeDie, DW_DLA_DIE);
	}
	return rc;
}

/* Traverse all compile unit (CU) Die's. */
DDR_RC
DwarfScanner::traverse_cu_in_debug_section(Symbol_IR *ir)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Unsigned cuHeaderLength = 0;
	Dwarf_Half versionStamp = 0;
	Dwarf_Unsigned abbrevOffset = 0;
	Dwarf_Half addressSize = 0;
	Dwarf_Unsigned nextCUheader = 0;
	Dwarf_Error error = NULL;

	/* Go over each cu header. */
	while (DDR_RC_OK == rc) {
		Symbol_IR newIR(ir);
		_typeOffsetMap.clear();
		_ir = &newIR;

		int ret = dwarf_next_cu_header(_debug, &cuHeaderLength, &versionStamp, &abbrevOffset, &addressSize, &nextCUheader, &error);
		if (DW_DLV_ERROR == ret) {
			ERRMSG("Failed to get next dwarf CU header.");
			rc = DDR_RC_ERROR;
			break;
		} else if (DW_DLV_OK != ret) {
			DEBUGPRINTF("No entry");
			/* No more CU's. */
			break;
		}
		Dwarf_Die cuDie = NULL;
		Dwarf_Die childDie = NULL;

		/* Expect the CU to have a single sibling - a DIE */
		if (DW_DLV_ERROR == dwarf_siblingof(_debug, NULL, &cuDie, &error)) {
			ERRMSG("Getting sibling of CU: %s\n", dwarf_errmsg(error));
			rc = DDR_RC_ERROR;
			break;
		}

		if (!_blacklistedFiles.empty()) {
			if (DDR_RC_OK != getSourcelist(cuDie)) {
				ERRMSG("Failed to get source file list.\n");
				rc = DDR_RC_ERROR;
				break;
			}
		}

		/* Expect the CU DIE to have children */
		ret = dwarf_child(cuDie, &childDie, &error);
		dwarf_dealloc(_debug, cuDie, DW_DLA_DIE);
		if (DW_DLV_ERROR == ret) {
			ERRMSG("Getting child of CU Die: %s\n", dwarf_errmsg(error));
			rc = DDR_RC_ERROR;
			break;
		}
		if (NULL == childDie) {
			continue;
		}

		/* Now go over all children DIEs */
		do {
			DEBUGPRINTF("Going over child die");
			Dwarf_Half tag;
			if (DW_DLV_ERROR == dwarf_tag(childDie, &tag, &error)) {
				ERRMSG("In dwarf_tag: %s\n", dwarf_errmsg(error));
				rc = DDR_RC_ERROR;
				break;
			}

			if ((tag == DW_TAG_structure_type)
				|| (tag == DW_TAG_class_type)
				|| (tag == DW_TAG_enumeration_type)
				|| (tag == DW_TAG_union_type)
				|| (tag == DW_TAG_namespace)
				|| (tag == DW_TAG_typedef)
			) {
				if (DDR_RC_OK != addDieToIR(childDie, tag, NULL, NULL)) {
					ERRMSG("Failed to add type.\n");
					rc = DDR_RC_ERROR;
					break;
				}
			}
		} while (DDR_RC_OK == getNextSibling(&childDie));
		dwarf_dealloc(_debug, childDie, DW_DLA_DIE);

		if (DDR_RC_OK == rc) {
			rc = ir->mergeIR(&newIR);
		}
	}
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfScanner::startScan(OMRPortLibrary *portLibrary, Symbol_IR *ir, vector<string> *debugFiles, const char *blacklistPath)
{
	DEBUGPRINTF("Init:");
	DEBUGPRINTF("Initializing libDwarf:");

	DDR_RC rc = loadBlacklist(blacklistPath);

	map<Type *, set<string> > typeToContainingFiles;
	map<string, set<Type *> > fileToContainedTypes;

	/* Read list of debug files to scan from the input file. */
	for (vector<string>::iterator it = debugFiles->begin(); it != debugFiles->end(); ++it) {
		const string & debugFile = *it;
		Symbol_IR newIR(ir);
		rc = scanFile(portLibrary, &newIR, debugFile.c_str());
		if (DDR_RC_OK != rc) {
			break;
		}
		for (vector<Type *>::iterator it2 = newIR._types.begin(); it2 != newIR._types.end(); ++it2) {
			Type *type = *it2;
			typeToContainingFiles[type].insert(debugFile);
			fileToContainedTypes[debugFile].insert(type);
			vector<UDT *> *subTypes = type->getSubUDTS();
			if (NULL != subTypes) {
				for (vector<UDT *>::iterator it3 = subTypes->begin(); it3 != subTypes->end(); ++it3) {
					UDT *subType = *it3;
					typeToContainingFiles[subType].insert(debugFile);
					fileToContainedTypes[debugFile].insert(subType);
				}
			}
		}
		ir->mergeIR(&newIR);
	}

	if (DDR_RC_OK == rc) {
		/* Create a set of all files and remove from that set any files that are necessary. */
		set<string> unnecessaryFiles;
		unnecessaryFiles.insert(debugFiles->begin(), debugFiles->end());

		/* Any type scanned in only one file is necessary, so remove them first. */
		for (map<Type *, set<string> >::iterator it = typeToContainingFiles.begin(); it != typeToContainingFiles.end() && !unnecessaryFiles.empty();) {
			if (1 == it->second.size()) {
				string fileName = *it->second.begin();
				set<string>::iterator uselessFile = unnecessaryFiles.find(fileName);
				if (unnecessaryFiles.end() != uselessFile) {
					unnecessaryFiles.erase(uselessFile);
				}
				fileToContainedTypes[fileName].erase(fileToContainedTypes[fileName].find(it->first));
				map<Type *, set<string> >::iterator previous = it;
				++it;
				typeToContainingFiles.erase(previous);
			} else {
				++it;
			}
		}

		while (!typeToContainingFiles.empty()) {
			/* Next, count how many unmarked types are contained in each file.
			 * Keep the file which contains the most until no unmarked types remain.
			 */
			string fileWithMostTypes = "";
			size_t greatestNumberOfTypes = 0;
			for (map<string, set<Type *> >::iterator it = fileToContainedTypes.begin(); it != fileToContainedTypes.end(); ++it) {
				if ((it->second.size() > greatestNumberOfTypes) && (unnecessaryFiles.end() != unnecessaryFiles.find(it->first))) {
					greatestNumberOfTypes = it->second.size();
					fileWithMostTypes = it->first;
				}
			}
			if (greatestNumberOfTypes > 0) {
				/* Consider the file that adds the most new types necessary and remove its types from the maps. */
				unnecessaryFiles.erase(unnecessaryFiles.find(fileWithMostTypes));
				for (set<Type *>::iterator it = fileToContainedTypes[fileWithMostTypes].begin(); it != fileToContainedTypes[fileWithMostTypes].end(); ++ it) {
					Type *typeFromFile = *it;
					/* For each type contained in the file, remove it from the type set of each file it is in. */
					for (set<string>::iterator it2 = typeToContainingFiles[typeFromFile].begin(); it2 != typeToContainingFiles[typeFromFile].end(); ++ it2) {
						string fileContainingType = *it2;
						fileToContainedTypes[fileContainingType].erase(fileToContainedTypes[fileContainingType].find(typeFromFile));
					}
					typeToContainingFiles.erase(typeToContainingFiles.find(typeFromFile));
				}
				fileToContainedTypes.erase(fileToContainedTypes.find(fileWithMostTypes));
			} else {
				break;
			}
		}

		/* Afterwards, print the files that are found to be uneccessary for a complete superset. */
		if (!unnecessaryFiles.empty()) {
			printf("These files can be ommited from scanning without missing any data:\n");
			for (set<string>::iterator it = unnecessaryFiles.begin(); it != unnecessaryFiles.end(); ++ it) {
				printf("%s,\n", it->c_str());
			}
		}
	}

	return rc;
}

DDR_RC
DwarfScanner::scanFile(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *filepath)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	DDR_RC rc = DDR_RC_OK;
	int res = DW_DLV_ERROR;
	Dwarf_Error error = NULL;
	intptr_t fd = omrfile_open(filepath, EsOpenRead, 0);
	if (fd < 0) {
		ERRMSG("Failure attempting to open %s\nExiting...\n", filepath);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		Dwarf_Unsigned access = DW_DLC_READ;
		Dwarf_Handler errhand = 0;
		Dwarf_Ptr errarg = NULL;
		res = dwarf_init(fd, access, errhand, errarg, &_debug, &error);
		if (DW_DLV_OK != res) {
			if (DW_DLV_ERROR == res) {
				ERRMSG("Failed to Initialize libDwarf scanning %s! DW_DLV_ERROR: res: %s\nExiting...\n", filepath, dwarf_errmsg(error));
			} else if (DW_DLV_NO_ENTRY == res) {
				ERRMSG("Failed to Initialize libDwarf scanning %s! DW_DLV_ERROR: res: %s\nExiting...\n", filepath, dwarf_errmsg(error));
			} else {
				ERRMSG("Failed to Initialize libDwarf scanning %s! DW_DLV_ERROR: res: %s\nExiting...\n", filepath, dwarf_errmsg(error));
			}
			if (NULL != error) {
				dwarf_dealloc(_debug, error, DW_DLA_ERROR);
			}
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		DEBUGPRINTF("Initialized libDwarf: Continuing...");

		DEBUGPRINTF("%s", filepath);
		/* Get info from dbg - Dwarf Info structure.*/
		rc = traverse_cu_in_debug_section(ir);
	}

	if (DDR_RC_OK == rc) {
		DEBUGPRINTF("Unloading libDwarf");

		res = dwarf_finish(_debug, &error);
		if (DW_DLV_OK != res) {
			ERRMSG("Failed to Unload libDwarf! res: %s\nExiting...\n", dwarf_errmsg(error));
			if (NULL != error) {
				dwarf_dealloc(_debug, error, DW_DLA_ERROR);
			}
			rc = DDR_RC_ERROR;
		}
	}

	if (fd >= 0) {
		DEBUGPRINTF("Closing file: fd");

		omrfile_close(fd);
	}

	DEBUGPRINTF("Start Scan Finished: Returning...");

	return rc;
}

DwarfScanner::~DwarfScanner()
{
	if (NULL != _fileNamesTable) {
		for (Dwarf_Signed i = 0; i < _fileNameCount; ++i) {
			free(_fileNamesTable[i]);
		}
		free(_fileNamesTable);
		_fileNamesTable = NULL;
	}
}

DDR_RC
DwarfScanner::getNextSibling(Dwarf_Die *die)
{
	DDR_RC rc = DDR_RC_ERROR;
	Dwarf_Die nextSibling = NULL;
	Dwarf_Error err = NULL;

	/* Get the next sibling and free the previous one if successful. */
	int ret = dwarf_siblingof(_debug, *die, &nextSibling, &err);
	if (DW_DLV_ERROR == ret) {
		ERRMSG("Getting sibling of die:%s\n", dwarf_errmsg(err));
	} else if (DW_DLV_OK == ret) {
		rc = DDR_RC_OK;
		*die = nextSibling;
	}
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLV_ERROR);
	}
	return rc;
}
