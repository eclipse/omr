/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "DwarfScanner.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>     /* For exit() */
#include <string.h>
#include <functional>
#include <utility>

#include "config.hpp"
#include "ClassUDT.hpp"
#include "EnumMember.hpp"
#include "EnumUDT.hpp"
#include "Field.hpp"
#include "NamespaceUDT.hpp"
#include "Symbol_IR.hpp"
#include "UnionUDT.hpp"

DwarfScanner::DwarfScanner()
	: _fileNameCount(0), _fileNamesTable(NULL), _ir(NULL), _debug(NULL)
{
}

DDR_RC
DwarfScanner::getBlacklist(Dwarf_Die die)
{
	if (_fileNameCount > 0) {
		/* Free the memory from the previous CU, if allocated. */
		for (int i = 0; i < _fileNameCount; i += 1) {
			free(_fileNamesTable[i]);
		}
		free(_fileNamesTable);
		_fileNamesTable = NULL;
		_fileNameCount = 0;
	}

	char **fileNamesTableConcat = NULL;
	char *compDir = NULL;
	Dwarf_Error error = NULL;
	Dwarf_Attribute attr = NULL;

	/* Get the dwarf source file names. */
	if (DW_DLV_ERROR == dwarf_srcfiles(die, &_fileNamesTable, &_fileNameCount, &error)) {
		ERRMSG("Failed to get list of source files: %s\n", dwarf_errmsg(error));
		_fileNamesTable = NULL;
		_fileNameCount = 0;
		goto Failed;
	}

	/* Get the CU directory. */
	if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_comp_dir, &attr, &error)) {
		ERRMSG("Getting compilation directory attribute: %s\n", dwarf_errmsg(error));
		goto Failed;
	}

	if (DW_DLV_ERROR == dwarf_formstring(attr, &compDir, &error)) {
		if (NULL == attr) {
			/* The DIE didn't have a compilationDirectory attribute (AIX)
			 * so we can skip over getting the absolute paths from the relative paths
			 */
			goto Done;
		} else {
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
	for (Dwarf_Signed i = 0; i < _fileNameCount; i += 1) {
		char *path = _fileNamesTable[i];
		if (0 == strncmp(_fileNamesTable[i], "../", 3)) {
			/* For each file name beginning with "../", reformat the path as an absolute path. */
			fileNamesTableConcat[i] = (char *)malloc(sizeof(char) * (strlen(compDir) + strlen(path) + 2));
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
		} else if (0 == strncmp(_fileNamesTable[i], "/", 1)) {
			fileNamesTableConcat[i] = (char *)malloc(sizeof(char) * (strlen(path) + 1));
			strcpy(fileNamesTableConcat[i], path);
		} else {
			fileNamesTableConcat[i] = (char *)malloc(sizeof(char) * (strlen(compDir) + strlen(path) + 2));
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
	if (_fileNameCount > 0) {
		for (Dwarf_Signed i = 0; i < _fileNameCount; i += 1) {
			dwarf_dealloc(_debug, _fileNamesTable[i], DW_DLA_STRING);
		}
		dwarf_dealloc(_debug, _fileNamesTable, DW_DLA_LIST);
		_fileNamesTable = NULL;
		_fileNameCount = 0;
	}
	if (NULL != fileNamesTableConcat) {
		for (Dwarf_Signed i = 0; i < _fileNameCount; i += 1) {
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
	*dieBlackListed = true;

	/* Check if the Die has the decl_file attribute. */
	Dwarf_Bool hasDeclFile;
	if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_decl_file, &hasDeclFile, &err)) {
		ERRMSG("Checking if die has attribute decl_file: %s\n", dwarf_errmsg(err));
		rc = DDR_RC_ERROR;
		goto Done;
	}
	if (hasDeclFile) {
		/* If the Die has the attribute, get the attribute and its value. */
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
		 */
		if ((NULL == strstr(_fileNamesTable[declFile - 1], "<built-in>"))
			&& (0 != strncmp(_fileNamesTable[declFile - 1], "/usr/", 5))
			&& (0 != strncmp(_fileNamesTable[declFile - 1], "/Applications/Xcode.app/", 24))
		) {
			*dieBlackListed = false;
		}
	} else {
		*dieBlackListed = false;
	}

Done:
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfScanner::getName(Dwarf_Die die, string *name)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Error err = NULL;
	char *dieName = NULL;
	int dwarfRC = dwarf_diename(die, &dieName, &err);

	if (DW_DLV_ERROR == dwarfRC) {
		ERRMSG("Error in dwarf_diename: %s\n", dwarf_errmsg(err));
		rc = DDR_RC_ERROR;
	} else if (DW_DLV_NO_ENTRY == dwarfRC) {
		DEBUGPRINTF("DW_DLV_NO_ENTRY %s", dieName);

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
#if defined(J9ZOS390)
			if (DW_DLV_ERROR == dwarf_offdie(_debug, offset, &die, &err)) {
#else /* defined(J9ZOS390) */
			if (DW_DLV_ERROR == dwarf_offdie_b(_debug, offset, 1, &die, &err)) {
#endif /* !defined(J9ZOS390) */
				ERRMSG("Getting die from specification offset: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto NameDone;
			}
			rc = getName(die, name);
			dwarf_dealloc(_debug, die, DW_DLA_DIE);
			goto NameDone;
		} else {
			/* If the Die has no specification attribute either, check for linkage_name. */
			if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_linkage_name, &hasAttr, &err)) {
				ERRMSG("Checking if die has linkage_name attribute: %s\n", dwarf_errmsg(err));
				rc = DDR_RC_ERROR;
				goto NameDone;
			}
			if (hasAttr) {
				/* Die's with a linkage_name and no name can get the name from the sibling die. */
				if (DDR_RC_OK != getNextSibling(&die)) {
					ERRMSG("Getting sibling Die: %s\n", dwarf_errmsg(err));
					rc = DDR_RC_ERROR;
					goto NameDone;
				}
				rc = getName(die, name);

				if (DDR_RC_OK != rc) {
					goto NameDone;
				}

				dwarf_dealloc(_debug, die, DW_DLA_DIE);
			}
		}
	}
	if (NULL != name) {
		if ((NULL == dieName) || (0 == strncmp(dieName, "<anonymous", 10))) {
			*name = "";
		} else {
			*name = string(dieName);
		}
		dwarf_dealloc(_debug, dieName, DW_DLA_STRING);
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
	Dwarf_Half tag = 0;
	Dwarf_Die typeDie = die;
	DDR_RC rc = DDR_RC_ERROR;
	bool done = false;
	bool foundTypedef = false;
	modifiers->_modifierFlags = Modifiers::NO_MOD;

	/* Get the bit field from the member Die before getting the type. */
	if (DDR_RC_OK == getBitField(typeDie, bitField)) {
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
						modifiers->_arrayLengths.push_back(upperBound + 1);
					} else {
						/* Arrays declared as "[]" have no size attributes. */
						modifiers->_arrayLengths.push_back(0);
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
				if (NULL != dieOut) {
					*dieOut = typeDie;
				}
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
					if (NULL != dieOut) {
						*dieOut = typeDie;
					}
				}

				/* Once a typedef is found, keep the name and do not append more pointer/array notation, but
				 * otherwise keep searching the type references for type information.
				 */
				if (DW_TAG_typedef == tag) {
					foundTypedef = true;
				} else {
					rc = DDR_RC_OK;
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
#if defined(J9ZOS390)
		/* z/OS dwarf library doesn't have dwarf_offdie_b, only dwarf_offdie*/
		if (DW_DLV_ERROR == dwarf_offdie(_debug, offset, &newDie, &err)) {
#else /* defined(J9ZOS390) */
		if (DW_DLV_ERROR == dwarf_offdie_b(_debug, offset, 1, &newDie, &err)) {
#endif /* !defined(J9ZOS390) */
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
DwarfScanner::addType(Dwarf_Die die, Dwarf_Half tag, bool ignoreFilter, NamespaceUDT *outerUDT, Type **type)
{
	DDR_RC rc = DDR_RC_ERROR;
	Type *newType = NULL;
	int typeNum = -1;
	bool dieBlackListed = true;
	bool isSubUDT = (NULL != outerUDT);

	if (!ignoreFilter) {
		rc = blackListedDie(die, &dieBlackListed);

		if (DDR_RC_OK != rc) {
			goto AddTypeDone;
		}

		if (dieBlackListed) {
			rc = DDR_RC_OK;
			goto AddTypeDone;
		}
	}

	rc = getType(die, tag, &newType, outerUDT, &typeNum);

	if(DDR_RC_OK == rc) {
		if (2 == typeNum) {
			rc = DDR_RC_OK;
			if (isSubUDT) {
				UDT *udt = dynamic_cast<UDT *>(newType);
				if ((NULL != udt) && (NULL == udt->_outerUDT)) {
					/* If this UDT has been added before when it was found as a field,
					 * there was no way to know its an inner type at that time and it
					 * would have gone into the main udt list.
					 */
					_ir->_types.erase(remove(_ir->_types.begin(), _ir->_types.end(), newType));

					/* Anonymous types would have been named incorrectly if they were
					 * found as a field before they were known to be an inner type.
					 */
					if ((newType->_name.length() > 9)
						&& ("Constants" == newType->_name.substr(newType->_name.length() - 9, 9))
					) {
						newType->_name = "";
						rc = getName(die, &newType->_name);
					}
				}
			}
		} else if ((0 == typeNum) || (1 == typeNum) || (3 == typeNum)) {
			/* Entry is for a type that has not already been found. */
			if ((DW_TAG_class_type == tag)
					|| (DW_TAG_structure_type == tag)
					|| (DW_TAG_union_type == tag)
					|| (DW_TAG_namespace == tag)
					|| (DW_TAG_enumeration_type == tag)
					|| (DW_TAG_typedef == tag)
			) {
				/* If the type was added as a stub with an outer type, such as is the case
				 * for types defined within namespaces, do not add it to the main list of types.
				 */
				isSubUDT = (isSubUDT) || (string::npos != newType->getFullName().find("::"));
				if (3 == typeNum) {
					rc = DDR_RC_OK;
				} else {
					rc = newType->scanChildInfo(this, die);
					DEBUGPRINTF("Done scanning child info");
				}
			} else {
				rc = DDR_RC_OK;
			}

			if (!isSubUDT && (0 == typeNum || 3 == typeNum)) {
				_ir->_types.push_back(newType);
			}
		} else {
			ERRMSG("getType failed\n");
		}
	}

AddTypeDone:
	if (NULL != type) {
		*type = newType;
	}

	return rc;
}

/* Given a Die for a type, return the associated Type object for it. The first time
 * called for each type, this function will create a Type object and add it to the map.
 * On subsequent calls for a given type, the existing Type object in the map will be
 * returned.
 *
 * The return code indicates if the returned type already had a full declaration, the
 * type had been found before but only as a stub without children, the type is a stub
 * and had not been found before, or the type is a full declaration and had not been
 * found before. The caller is responsible for adding the Type's properties/children
 * and adding it to the IR based on this return code.
 *
 * To keep track of duplicate Types, this function maintains two hash maps: one for
 * stub types and one for full declarations. Stub types only have a name, while full
 * declarations have a file name, line number, and possibly children/properties. A
 * type can be found first as a stub and later as a full declaration, as a full
 * declaration and later as a stub, as only a stub, or only as a full declaration.
 */
DDR_RC
DwarfScanner::getType(Dwarf_Die die, Dwarf_Half tag, Type **const newType, NamespaceUDT *outerUDT, int *typeNum)
{
	/* Set typeNum to -1 for error.
	 * Set typeNum to 0 for full types which have not been found before--add to types list and add children.
	 * Set typeNum to 1 for full types which have been found before as a stub--do not add to types list but add children.
	 * Set typeNum to 2 for full types which already exist--do not add to types list or add children.
	 * Set typeNum to 3 for stub types which have not been found before--add to types list but do not add children.
	 */
	DDR_RC rc = DDR_RC_OK;
	*typeNum = -1;

	string dieName = "";
	rc = getName(die, &dieName);

	if (DDR_RC_OK != rc) {
		ERRMSG("Couldn't get die name");
	} else {
		string stubKey = dieName + char(tag);
		unsigned int lineNumber = 0;
		string fileName = "";
		TypeKey key;
		/* createTypeKey() succeeds if the Die has a declaration file and line number.
		 * Use these as a map key to check for duplicates.
		 */
		if (DDR_RC_OK == createTypeKey(die, dieName, &key, &lineNumber, &fileName)) {
			if (dieName.empty() && (NULL == outerUDT) && (DW_TAG_enumeration_type == tag)) {
				/* Anonymous enums with no outer type are added to a constants structure named by file name. */
				size_t lastSlash = fileName.find_last_of("/");
				size_t lastDot = fileName.find_last_of(".");
				dieName = fileName.substr(lastSlash + 1, lastDot - lastSlash - 1) + "Constants";
				dieName[0] = toupper(dieName[0]);
			}
			/* If the Type is not in the map yet, add it. */
			bool isInTypeMap = (_typeMap.find(key) != _typeMap.end());
			if (isInTypeMap) {
				Type *existingType = _typeMap[key];
				UDT *udt = dynamic_cast<UDT *>(existingType);
				if (NULL != udt) {
					/* Type name, file name, line number is not a unique identifier for a type when macros change the name of the outer class */
					/* Will not work when macros change if it is an inner class or not */
					isInTypeMap = ((udt->_outerUDT == outerUDT) || (NULL == outerUDT) || (NULL == udt->_outerUDT));
				}
			}
			if (!isInTypeMap) {
				/* If the Type is not in the stub map either, or as a stub which has already
				 * been promoted to the map for types with declarations, create a new Type for this Die. */
				if ((_typeStubMap.find(stubKey) == _typeStubMap.end())
						|| ((DW_TAG_base_type != tag) && (0 != ((UDT *)_typeStubMap[stubKey])->_lineNumber))
				) {
					if (DDR_RC_OK == createType(die, tag, dieName, lineNumber, newType)) {
						_typeMap[key] = *newType;

						/* Anonymous types cannot also be a stub elsewhere and as such do not need
						 * to be added to the stub map.
						 */
						if (!dieName.empty()) {
							_typeStubMap[stubKey] = *newType;
						}
						*typeNum = 0;
					} else {
						ERRMSG("Error creating type.");
						rc = DDR_RC_ERROR;
					}
				} else {
					/* If the Type is already in the stub map, add it to the Type map. */
					*newType = _typeStubMap[stubKey];
					_typeMap[key] = *newType;
					/* If a stub type was created without a size, attempt to get it. */
					if (0 == (*newType)->_sizeOf) {
						size_t typeSize = 0;
						rc = getTypeSize(die, &typeSize);

						if (DDR_RC_OK == rc) {
							(*newType)->_sizeOf = typeSize;
						}
					}

					/* Add the line number for the stub type as well, now that it is found. */
					if (DDR_RC_OK == rc) {
						UDT *udt = dynamic_cast<UDT *>(*newType);
						if (NULL != udt) {
							udt->_lineNumber = lineNumber;
						}
					}
					*typeNum = 1;
				}
			} else {
				/* The Type already exists in the map; just return it */
				*newType = _typeMap[key];
				free(key.fileName);
				*typeNum = 2;
			}
		} else {
			if (dieName.empty()) {
				dieName = "void";
				tag = DW_TAG_base_type;
				stubKey = dieName + char(tag);
			}
			/* Die's without a declaration file and line number are remembered by die name and type instead. */
			if (_typeStubMap.find(stubKey) == _typeStubMap.end()) {
				if (DDR_RC_OK == createType(die, tag, dieName, lineNumber, newType)) {
					_typeStubMap[stubKey] = *newType;
					*typeNum = 3;
				}
			} else {
				/* The Type already exists as a stub and the declaration has not yet been found. */
				*newType = _typeStubMap[stubKey];
				*typeNum = 2;
			}
		}
	}

	if(-1 == *typeNum) {
		rc = DDR_RC_ERROR;
	}
	return rc;
}

/* Create and return a Type object of the correct subtype based on a dwarf tag. */
DDR_RC
DwarfScanner::createType(Dwarf_Die die, Dwarf_Half tag, string dieName, unsigned int lineNumber, Type **const newType)
{
	DDR_RC rc = DDR_RC_OK;
	size_t typeSize = 0;
	/* Get the UDT symbol type from the tag. */
	switch (tag) {
	case DW_TAG_union_type:
		DEBUGPRINTF("DW_TAG_union_type: '%s'", dieName.c_str());
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new UnionUDT(typeSize, lineNumber);
		break;
	case DW_TAG_enumeration_type:
		DEBUGPRINTF("DW_TAG_enumeration_type: '%s'", dieName.c_str());
		*newType = new EnumUDT(lineNumber);
		break;
	case DW_TAG_class_type:
		DEBUGPRINTF("DW_TAG_class_type: '%s'", dieName.c_str());
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new ClassUDT(typeSize, true, lineNumber);
		break;
	case DW_TAG_structure_type:
		DEBUGPRINTF("DW_TAG_structure_type: '%s'", dieName.c_str());
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new ClassUDT(typeSize, false, lineNumber);
		break;
	case DW_TAG_namespace:
		DEBUGPRINTF("DW_TAG_namespace: '%s'", dieName.c_str());
		*newType = new NamespaceUDT(lineNumber);
		break;
	case DW_TAG_typedef:
		DEBUGPRINTF("createType: DW_TAG_typedef: '%s'", dieName.c_str());
		*newType = new TypedefUDT(lineNumber);
		break;
	case DW_TAG_base_type:
		DEBUGPRINTF("DW_TAG_base_type: '%s'", dieName.c_str());
		rc = getTypeSize(die, &typeSize);
		if (DDR_RC_OK != rc) {
			break;
		}
		*newType = new Type(BASE, typeSize);
		break;
	default:
		ERRMSG("Unknown symbol type: %d\n", tag);
		rc = DDR_RC_ERROR;
	}
	if (DDR_RC_OK == rc) {
		(*newType)->_name = dieName;
	}
	return rc;
}

DDR_RC
DwarfScanner::dispatchScanChildInfo(TypedefUDT *newTypedef, void *data)
{
	Dwarf_Die die = (Dwarf_Die)data;
	Dwarf_Die typeDie = NULL;
	string typedefName = "";
	/* Find and add the typedef's modifiers. */
	DDR_RC rc = getTypeInfo(die, &typeDie, &typedefName, &newTypedef->_modifiers, &newTypedef->_sizeOf, NULL);

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
		rc = addType(typeDie, tag, true, NULL, &typedefType);
		dwarf_dealloc(_debug, typeDie, DW_DLA_DIE);
	}
	if (DDR_RC_OK == rc) {
		newTypedef->_type = typedefType;
		if (NULL != typedefType) {
			/* Set typedef's size to the size of its type. */
			newTypedef->_sizeOf = newTypedef->_modifiers.getSize(typedefType->_sizeOf);
			/* If the typedef's type has no associated name, let it assume the typedef's name. */
			if (typedefName.empty()) {
				typedefType->_name = newTypedef->_name;
			}
		}
	}
	return rc;
}

/* An enum Die should only have children Die's for enum members. This function
 * gets the enum members by processing the children.
 */
DDR_RC
DwarfScanner::dispatchScanChildInfo(EnumUDT *const newUDT, void *data)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Die die = (Dwarf_Die)data;
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
				if (DDR_RC_OK != addEnumMember(childDie, (EnumUDT *)newUDT)) {
					rc = DDR_RC_ERROR;
					break;
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

/* A Die for a class/struct/union has children Die's for all of its properties,
 * including member fields, sub types, functions, and super type. This function
 * processes those properties to populate a NamespaceUDT or ClassUDT.
 */
DDR_RC
DwarfScanner::dispatchScanChildInfo(NamespaceUDT *newClass, void *data)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Die die = (Dwarf_Die)data;
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
		 * Iterate over Child's siblings to get remainder of fields and repeat.
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
				if (DDR_RC_OK != addType(childDie, childTag, true, newClass, (Type **)&innerUDT)) {
					rc = DDR_RC_ERROR;
					break;
				} else if ((NULL != newClass) && (NULL != innerUDT)) {
					innerUDT->_outerUDT = newClass;
					newClass->_subUDTs.push_back(innerUDT);
				}
			} else if (DW_TAG_inheritance == childTag) {
				/* The child is a super type. */
				ClassUDT *classUDT = dynamic_cast<ClassUDT *>(newClass);
				if (NULL == classUDT || DDR_RC_OK != getSuperUDT(childDie, classUDT)) {
					rc = DDR_RC_ERROR;
					break;
				}
			} else if (DW_TAG_member == childTag) {
				/* The child is a member field. */
				string fieldName = "";
				rc = getName(childDie, &fieldName);
				DEBUGPRINTF("fieldName: %s", fieldName.c_str());
				if (DDR_RC_OK != addClassField(childDie, (ClassType *)newClass, fieldName)) {
					rc = DDR_RC_ERROR;
					break;
				}
			}
		} while (DDR_RC_OK == getNextSibling(&childDie));
		dwarf_dealloc(_debug, childDie, DW_DLA_DIE);

		/* If the class is not a stub and contained an anonymous enum not used as a field within the class, add
		 * the enum members to the class instead. */
		ClassType *ct = dynamic_cast<ClassType *>(newClass);
		if ((NULL != ct) && (0 != newClass->_lineNumber)) {
			for (vector<UDT *>::iterator it = ct->_subUDTs.begin(); it != ct->_subUDTs.end();) {
				EnumUDT *enumUDT = dynamic_cast<EnumUDT *>(*it);
				if ((NULL != enumUDT) && (*it)->isAnonymousType()) {
					bool usedAsField = false;
					for (vector<Field *>::iterator fit = ct->_fieldMembers.begin(); fit != ct->_fieldMembers.end(); fit += 1) {
						if ((*fit)->_fieldType == (*it)) {
							usedAsField = true;
							break;
						}
					}
					if (!usedAsField) {
						EnumUDT *eu = dynamic_cast<EnumUDT *>(*it);
						ct->_enumMembers.insert(ct->_enumMembers.end(), eu->_enumMembers.begin(), eu->_enumMembers.end());
						eu->_enumMembers.clear();
						delete(*it);
						it = ct->_subUDTs.erase(it);
					} else {
						it += 1;
					}
				} else {
					it += 1;
				}
			}
		}
	}

	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

DDR_RC
DwarfScanner::dispatchScanChildInfo(Type *newType, void *data)
{
	/* No-op: base types do not have children Die's to scan. */
	return DDR_RC_OK;
}

DDR_RC
DwarfScanner::dispatchScanChildInfo(ClassUDT *newType, void *data)
{
	return dispatchScanChildInfo((NamespaceUDT *)newType, data);
}

DDR_RC
DwarfScanner::dispatchScanChildInfo(UnionUDT *newType, void *data)
{
	return dispatchScanChildInfo((NamespaceUDT *)newType, data);
}

/* Add an enum member to an enum UDT from a Die. */
DDR_RC
DwarfScanner::addEnumMember(Dwarf_Die die, EnumUDT *const newEnum)
{
	Dwarf_Error error = NULL;
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Signed value = 0;
	Dwarf_Attribute attr = NULL;

	/* Create new enum member. */
	EnumMember *newEnumMember = new EnumMember();
	string enumName;
	rc = getName(die, &enumName);

	if (DDR_RC_OK != rc) {
		goto AddEnumMemberDone;
	}

	newEnumMember->_name = enumName;

	/* Get the value attribute of the enum. */
	if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_const_value, &attr, &error)) {
		ERRMSG("Getting value attribute of enum: %s\n", dwarf_errmsg(error));
		rc = DDR_RC_ERROR;
		goto AddEnumMemberDone;
	}

	/* Get the enum const value. */
	if (DW_DLV_ERROR == dwarf_formsdata(attr, &value, &error)) {
		ERRMSG("Getting const value of enum: %s\n", dwarf_errmsg(error));
		rc = DDR_RC_ERROR;
		goto AddEnumMemberDone;
	}
	newEnumMember->_value = value;
	newEnum->_enumMembers.push_back(newEnumMember);

AddEnumMemberDone:
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	return rc;
}

/* Add a field to a class UDT from a Die. */
DDR_RC
DwarfScanner::addClassField(Dwarf_Die die, ClassType *const newClass, string fieldName)
{
	DDR_RC rc = DDR_RC_ERROR;
	string typeName;
	Field *newField = new Field();
	size_t typeSize = 0;
	Dwarf_Error error = NULL;
	Dwarf_Die baseDie = NULL;

	if (DDR_RC_OK == getTypeInfo(die, &baseDie, &typeName, &newField->_modifiers, &typeSize, &newField->_bitField)) {
		DEBUGPRINTF("typeName: %s", typeName.c_str());

		/* Get the field UDT. */
		Dwarf_Half tag;
		if (DW_DLV_ERROR == dwarf_tag(baseDie, &tag, &error)) {
			ERRMSG("Getting field's tag: %s\n", dwarf_errmsg(error));
			goto AddUDTFieldDone;
		}
		Type *fieldType = NULL;
		if ((DDR_RC_OK != addType(baseDie, tag, true, NULL, &fieldType)) || (NULL == fieldType)) {
			goto AddUDTFieldDone;
		}
		newField->_fieldType = fieldType;
		newField->_name = fieldName;
		newField->_sizeOf = newField->_modifiers.getSize(typeSize);

		newClass->_fieldMembers.push_back(newField);
		rc = DDR_RC_OK;

		dwarf_dealloc(_debug, baseDie, DW_DLA_DIE);
	}
AddUDTFieldDone:
	if (NULL != error) {
		dwarf_dealloc(_debug, error, DW_DLA_ERROR);
	}
	if (DDR_RC_OK != rc) {
		delete(newField);
	}
	return rc;
}

/* Find and return the super type from a Die. */
DDR_RC
DwarfScanner::getSuperUDT(Dwarf_Die die, ClassUDT *const udt)
{
	DDR_RC rc = DDR_RC_ERROR;
	Dwarf_Half tag = 0;
	Dwarf_Die superTypeDie = NULL;

	if (DDR_RC_OK == getTypeTag(die, &superTypeDie, &tag)) {
		ClassUDT *superUDT = NULL;
		/* Get the super udt. */
		if ((DDR_RC_OK == addType(superTypeDie, tag, true, NULL, (Type **)&superUDT)) && (NULL != superUDT)) {
			rc = DDR_RC_OK;
			udt->_superClass = superUDT;
		}
		dwarf_dealloc(_debug, superTypeDie, DW_DLA_DIE);
	}
	return rc;
}

/* Traverse all compile unit (CU) Die's. */
DDR_RC
DwarfScanner::traverse_cu_in_debug_section(Symbol_IR *const ir)
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

		if (DDR_RC_OK != getBlacklist(cuDie)) {
			ERRMSG("Failed to get black list.\n");
			rc = DDR_RC_ERROR;
			break;
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
				if (DDR_RC_OK != addType(childDie, tag, false, NULL, NULL)) {
					ERRMSG("Failed to add type.\n");
					rc = DDR_RC_ERROR;
					break;
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
DwarfScanner::startScan(OMRPortLibrary *portLibrary, Symbol_IR *const ir, vector<string> *debugFiles)
{
	DDR_RC rc = DDR_RC_OK;
	DEBUGPRINTF("Init:");
	DEBUGPRINTF("Initializing libDwarf:");

	/* Read list of debug files to scan from the input file. */
	_ir = ir;
	for (vector<string>::iterator it = debugFiles->begin(); it != debugFiles->end(); ++it) {
		rc = scanFile(portLibrary, ir, it->c_str());
		if (DDR_RC_OK != rc) {
			break;
		}
	}

	return rc;
}

DDR_RC
DwarfScanner::scanFile(OMRPortLibrary *portLibrary, Symbol_IR *const ir, const char *filepath)
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
				ERRMSG("Failed to Initialize libDwarf! DW_DLV_ERROR: res: %s\nExiting...\n", dwarf_errmsg(error));
			} else if (DW_DLV_NO_ENTRY == res) {
				ERRMSG("Failed to Initialize libDwarf! DW_DLV_NO_ENTRY: res: %s\nExiting...\n", dwarf_errmsg(error));
			} else {
				ERRMSG("Failed to Initialize libDwarf! Unknown ERROR: res: %s\nExiting...\n", dwarf_errmsg(error));
			}
			if (NULL != error) {
				dwarf_dealloc(_debug, error, DW_DLA_ERROR);
			}
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		DEBUGPRINTF("Initialized libDwarf: Continuing...");

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
	for (unordered_map<TypeKey, Type *, KeyHasher>::iterator it = _typeMap.begin(); it != _typeMap.end(); it++) {
		free(it->first.fileName);
	}
	if (0 < _fileNameCount) {
		for (int i = 0; i < _fileNameCount; i += 1) {
			free(_fileNamesTable[i]);
		}
		free(_fileNamesTable);
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

DDR_RC
DwarfScanner::createTypeKey(Dwarf_Die die, string typeName, TypeKey *key, unsigned int *lineNumber, string *fileName)
{
	DDR_RC rc = DDR_RC_OK;
	Dwarf_Error err = NULL;

	Dwarf_Bool hasAttr = false;
	if (DW_DLV_ERROR == dwarf_hasattr(die, DW_AT_decl_file, &hasAttr, &err)) {
		ERRMSG("Checking if die has attr decl_file: %s\n", dwarf_errmsg(err));
		rc = DDR_RC_ERROR;
		goto Done;
	}
	/* If the Die has a file name and line number declaration, use it to check for duplicate types. */
	if (hasAttr) {
		Dwarf_Attribute attr = NULL;
		if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_decl_file, &attr, &err)) {
			ERRMSG("Getting attr decl_file: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		Dwarf_Unsigned declFile = 0;
		int ret = dwarf_formudata(attr, &declFile, &err);
		dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
		if (DW_DLV_ERROR == ret || 0 == declFile) {
			ERRMSG("Getting attr value decl_file: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		if (DW_DLV_ERROR == dwarf_attr(die, DW_AT_decl_line, &attr, &err)) {
			ERRMSG("Getting attr decl_file: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		Dwarf_Unsigned declLine = 0;
		ret = dwarf_formudata(attr, &declLine, &err);
		dwarf_dealloc(_debug, attr, DW_DLA_ATTR);
		if (DW_DLV_ERROR == ret) {
			ERRMSG("Getting attr value decl_file: %s\n", dwarf_errmsg(err));
			rc = DDR_RC_ERROR;
			goto Done;
		}
		if (NULL != lineNumber) {
			*lineNumber = declLine;
		}

		/* Copy the filename so it is still accessible after libdwarf frees and allocates
		 * a new file names table with each CU.
		 */
		key->fileName = (char *)malloc(sizeof(char) * (strlen(_fileNamesTable[declFile - 1]) + 1));
		strcpy(key->fileName, _fileNamesTable[declFile - 1]);
		*fileName = string(key->fileName);
		key->lineNumber = declLine;
		key->typeName = typeName;
	} else {
		/* If there is no file name and line number. */
		rc = DDR_RC_ERROR;
	}
Done:
	if (NULL != err) {
		dwarf_dealloc(_debug, err, DW_DLA_ERROR);
	}
	return rc;
}

bool
TypeKey::operator==(const TypeKey& other) const
{
	return (0 == strcmp(fileName, other.fileName))
		   && (typeName == other.typeName)
		   && (lineNumber == other.lineNumber);
}

size_t
KeyHasher::operator()(const TypeKey& key) const
{
	return ((hash<string>()(key.fileName) << 5)
			^ (hash<string>()(key.typeName) << 3)
			^ (hash<int>()(key.lineNumber)));
}
