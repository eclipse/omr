/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
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

#include "ddr/error.hpp"

#include "ddr/scanner/dwarf/AixSymbolTableParser.hpp"
#include "ddr/scanner/dwarf/DwarfScanner.hpp"

/* Statics. */
static die_map createdDies;
static die_map builtInDies;
/* This vector contains the typeID of the nested class and parent class for populating nested classes. */
static vector<pair<int, Dwarf_Die> > nestedClassesToPopulate;
/* This vector contains the typeID of the type reference and type attribute to populate. */
static vector<pair<int, Dwarf_Attribute> > refsToPopulate;
/* This vector contains the Dwarf DIE with the bitfield attribute. */
static vector<Dwarf_Die> bitFieldsToCheck;
static Dwarf_Die _lastDie = NULL;
static Dwarf_Die _builtInTypeDie = NULL;
/* _startNewFile is a state counter. */
static int _startNewFile = 0;
/*
 * The accumulated (stabs) symbol table string.
 * The stabs format allows for entries to be split (a segment ends with '/' or '?')
 * in which case multiple lines must be read in the output of 'dump'.
 */
static string stabsString;
/* Start numbering DIE refs for refMap at one (zero will be for when no ref is found). */
static Dwarf_Off refNumber = 1;
/* This is used to initialize declLine to a unique value for each unique DIE every time it is required. */
static Dwarf_Off declarationLine = 1;
Dwarf_CU_Context * Dwarf_CU_Context::_firstCU;
Dwarf_CU_Context * Dwarf_CU_Context::_currentCU;

static void checkBitFields(Dwarf_Error *error);
static void deleteDie(Dwarf_Die die);
static int parseArray(const string & data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseBaseType(const string & data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseNamelessReference(const string & data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseStabstringDeclarationIntoDwarfDie(const string & data, Dwarf_Error *error);
static int parseSymbolTable(const char *line, Dwarf_Error *error);
static Dwarf_Half parseDeclarationLine(const string & data, int *typeID, declFormat *format, string *declarationData, string *name);
static int parseEnum(const string & data, string dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseField(const string & data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseCPPstruct(const string & data, const string & dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseCstruct(const string & data, const string & dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseTypeDef(const string & data, const string & dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int populateAttributeReferences();
static int populateBuiltInTypeDies(Dwarf_Error *error);
static void populateNestedClasses();
static void removeUselessStubs();
static int setDieAttributes(const string & dieName, unsigned int dieSize, Dwarf_Die currentDie, Dwarf_Error *error);

int
dwarf_finish(Dwarf_Debug dbg, Dwarf_Error *error)
{
	/* Free all Dies and Attributes created in dwarf_init(). */
	Dwarf_CU_Context::_currentCU = Dwarf_CU_Context::_firstCU;
	while (NULL != Dwarf_CU_Context::_currentCU) {
		Dwarf_CU_Context *nextCU = Dwarf_CU_Context::_currentCU->_nextCU;
		if (NULL != Dwarf_CU_Context::_currentCU->_die) {
			deleteDie(Dwarf_CU_Context::_currentCU->_die);
		}
		delete Dwarf_CU_Context::_currentCU;
		Dwarf_CU_Context::_currentCU = nextCU;
	}

	/* Delete the built-in type DIEs. */
	if (NULL != _builtInTypeDie) {
		deleteDie(_builtInTypeDie);
		_builtInTypeDie = NULL;
	}

	_lastDie = NULL;
	Dwarf_CU_Context::_fileId.clear();
	Dwarf_CU_Context::_currentCU = NULL;
	Dwarf_CU_Context::_firstCU = NULL;
	Dwarf_Die_s::refMap.clear();
	return DW_DLV_OK;
}

int
dwarf_init(int fd,
	Dwarf_Unsigned access,
	Dwarf_Handler errhand,
	Dwarf_Ptr errarg,
	Dwarf_Debug *dbg,
	Dwarf_Error *error)
{
	/* Initialize CU values */
	int ret = DW_DLV_OK;
	FILE *fp = NULL;
	char buffer[50000];
	Dwarf_CU_Context::_firstCU = NULL;
	Dwarf_CU_Context::_currentCU = NULL;
	refNumber = 1;

	/* Populate the built-in types. */
	ret = populateBuiltInTypeDies(error);

	if (DW_DLV_OK == ret) {
		ret = populateAttributeReferences();
	}

	/* Call dump to get the symbol table. */
	if (DW_DLV_OK == ret) {
		stringstream command;
		command << "dump -tvXany " << DwarfScanner::getScanFileName() << " 2>&1";
		fp = popen(command.str().c_str(), "r");
		if (NULL == fp) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_IOF);
		}
	}

	DEBUGPRINTF("Parsing through file.");
	/* Parse through the file. */
	while ((NULL != fgets(buffer, 50000, fp)) && (DW_DLV_OK == ret)) {
		ret = parseSymbolTable(buffer, error);
	}

	if (NULL != fp) {
		pclose(fp);
		fp = NULL;
	}

	/* Populate the references and nested classes of the last file to get parsed. */
	if (DW_DLV_OK == ret) {
		ret = populateAttributeReferences();
	}
	populateNestedClasses();
	removeUselessStubs();

	/* _currentCU must begin as NULL for dwarf_next_cu_header. */
	Dwarf_CU_Context::_currentCU = NULL;

	/* If the symbol table that we parsed contained no new files, create an empty placeholder generic CU and CU DIE. */
	if (NULL == Dwarf_CU_Context::_firstCU) {
		Dwarf_CU_Context *newCU = new Dwarf_CU_Context;
		Dwarf_Die newDie = new Dwarf_Die_s;
		Dwarf_Attribute name = new Dwarf_Attribute_s;

		if ((NULL == newCU) || (NULL == newDie) || (NULL == name)) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			newCU->_CUheaderLength = 0;
			newCU->_versionStamp = 0;
			newCU->_abbrevOffset = 0;
			newCU->_addressSize = 0;
			newCU->_nextCUheaderOffset = 0;
			newCU->_nextCU = NULL;
			newCU->_die = newDie;

			newDie->_tag = DW_TAG_compile_unit;
			newDie->_parent = NULL;
			newDie->_sibling = NULL;
			newDie->_previous = NULL;
			newDie->_child = NULL;
			newDie->_context = newCU;
			newDie->_attribute = name;

			name->_type = DW_AT_name;
			name->_nextAttr = NULL;
			name->_form = DW_FORM_string;
			name->_sdata = 0;
			name->_udata = 0;
			name->_stringdata = strdup("");
			name->_refdata = 0;
			name->_ref = NULL;

			Dwarf_CU_Context::_firstCU = newCU;
		}
	}

	/* Remove bit size attributes for members which don't need bit fields. */
	checkBitFields(error);

	/* Clear the maps after parsing. */
	refsToPopulate.clear();
	createdDies.clear();
	nestedClassesToPopulate.clear();
	builtInDies.clear();
	bitFieldsToCheck.clear();

	return ret;
}

static void
checkBitFields(Dwarf_Error *error)
{
	for (size_t i = 0; i < bitFieldsToCheck.size(); ++i) {
		/* Since this is a field, the type attribute is always the attribute after the first of the die passed in. */
		Dwarf_Attribute type = NULL;
		Dwarf_Attribute bitSize = NULL;
		Dwarf_Attribute tmpAttribute = bitFieldsToCheck[i]->_attribute;
		while (NULL != tmpAttribute) {
			if (DW_AT_bit_size == tmpAttribute->_type) {
				bitSize = tmpAttribute;
			} else if (DW_AT_type == tmpAttribute->_type) {
				type = tmpAttribute;
			}
			tmpAttribute = tmpAttribute->_nextAttr;
		}
		if ((NULL != type) && (NULL != bitSize) && (NULL != type->_ref)) {
			Dwarf_Die tmp = type->_ref;
			bool needsBitField = false;
			bool baseTypeHasBeenFound = false;
			while (!baseTypeHasBeenFound) {
				switch (tmp->_tag) {
				case DW_TAG_base_type:
					if (bitSize->_udata != tmp->_attribute->_nextAttr->_udata * 8) {
						needsBitField = true;
					}
					baseTypeHasBeenFound = true;
					break;
				case DW_TAG_const_type:
				case DW_TAG_typedef:
				case DW_TAG_volatile_type:
					/* Check if the type that this is referring to has been found before. */
					if (DW_DLV_OK == dwarf_attr(tmp, DW_AT_type, &tmpAttribute, error)) {
						tmp = tmpAttribute->_ref;
					} else {
						baseTypeHasBeenFound = true;
					}
					break;
				case DW_TAG_pointer_type:
					/* Compare the bit size with the pointer size for the current architecture. */
					if (PTR_SIZE * 8 != bitSize->_udata) {
						needsBitField = true;
					}
					baseTypeHasBeenFound = true;
					break;
				default:
					baseTypeHasBeenFound = true;
					break;
				}
			}
			if (!needsBitField) {
				/* Remove the bit size attribute if its not needed. */
				tmpAttribute = bitFieldsToCheck[i]->_attribute;
				while (DW_AT_bit_size != tmpAttribute->_nextAttr->_type) {
					tmpAttribute = tmpAttribute->_nextAttr;
				}
				if (NULL != tmpAttribute->_nextAttr->_stringdata) {
					free(tmpAttribute->_nextAttr->_stringdata);
				}
				delete tmpAttribute->_nextAttr;
				tmpAttribute->_nextAttr = NULL;
			}
		}
	}
}

static void
deleteDie(Dwarf_Die die)
{
	/* Delete the DIE's attributes. */
	Dwarf_Attribute attr = die->_attribute;
	while (NULL != attr) {
		Dwarf_Attribute next = attr->_nextAttr;
		if (NULL != attr->_stringdata) {
			free(attr->_stringdata);
		}
		delete attr;
		attr = next;
	}

	/* Delete the DIE's siblings and children. */
	if (NULL != die->_sibling) {
		deleteDie(die->_sibling);
	}
	if (NULL != die->_child) {
		deleteDie(die->_child);
	}
	delete die;
}

/**
 * Parses the string to file ID. Extracts the integer betwen the square brackets.
 *
 * @param[in] data: string from which to extract ID from. The expected format is "[####]"
 *
 * @return file ID as int
 */
int
extractFileID(const string & data)
{
	stringstream ss;
	int ret = 0;
	ss << data.substr(1, data.find_first_of(']') - 1);
	ss >> ret;
	return ret;
}

/**
 * Extracts the typeID from the name string that's passed in
 *
 * @param data: string that needs to be parsed for typeID EX: (T|t) INTEGER
 *
 * @return typeID as int
 */
int
extractTypeID(const string & data, int index)
{
	stringstream ss;
	int ret = 0;
	ss << data.substr(index);
	ss >> ret;
	return ret;
}

/**
 * Add a new (type) attribute to the list to resolve later.
 */
static void
addRefToPopulate(int typeId, Dwarf_Attribute attribute)
{
	if (0 == typeId) {
		ERRMSG("Bad type reference:\n%s\n", stabsString.c_str());
		exit(1);
	}
	refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)typeId, (Dwarf_Attribute)attribute));
}

/**
 * Parses data passed in as an array declaration
 * param[in] data: the data to be parsed, should be in the format ar(INDEX_ID);(LOWER_BOUND);(UPPER_BOUND);(ARRAY_TYPE)
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseArray(const string & data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (2 <= data.length()) {
		if ("ar" == data.substr(0, 2)) {
			str_vect arrayInfo = split(data, ';');
			/* arrayInfo is in the form: ar(INDEX_ID);(LOWER_BOUND);(UPPER_BOUND);(ARRAY_TYPE)
			 * arrayInfo[0] : ar + subrange type
			 * arrayInfo[1] : lower bound
			 * arrayInfo[2] : upper bound
			 * arrayInfo[3] : array type
			 */
			/* Populate array */
			Dwarf_Attribute type = new Dwarf_Attribute_s;

			/* Create a DIE to hold info about the subrange type. */
			Dwarf_Die newDie = new Dwarf_Die_s;
			Dwarf_Attribute subrangeType = new Dwarf_Attribute_s;
			Dwarf_Attribute subrangeUpperBound = new Dwarf_Attribute_s;
			if ((NULL == type) || (NULL == newDie) || (NULL == subrangeType) || (NULL == subrangeUpperBound)) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			} else {
				type->_type = DW_AT_type;
				type->_form = DW_FORM_ref1;
				type->_nextAttr = NULL;
				int ID = toInt(arrayInfo[3]);
				type->_udata = 0;
				type->_refdata = 0;
				type->_ref = NULL;

				/* Add the new attribute to the list of attributes to populate. */
				addRefToPopulate(ID, type);
				currentDie->_attribute = type;
				newDie->_tag = DW_TAG_subrange_type;
				newDie->_parent = currentDie;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;

				/* subrangeType is of the type sizetype, a built-in type. (-37)*/
				subrangeType->_type = DW_AT_type;
				subrangeType->_nextAttr = subrangeUpperBound;
				subrangeType->_form = DW_FORM_ref1;
				subrangeType->_udata = 0;
				subrangeType->_refdata = 0;
				subrangeType->_ref = NULL;

				/* Add subrangeType to the list of refs to populate in a second pass. */
				addRefToPopulate(-37, subrangeType);

				subrangeUpperBound->_type = DW_AT_upper_bound;
				subrangeUpperBound->_nextAttr = NULL;
				subrangeUpperBound->_form = DW_FORM_udata;
				subrangeUpperBound->_udata = toInt(arrayInfo[2]);
				subrangeUpperBound->_sdata = 0;
				subrangeUpperBound->_refdata = 0;
				subrangeUpperBound->_ref = NULL;
				newDie->_attribute = subrangeType;
				currentDie->_child = newDie;
			}
		} else {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_IA);
		}
	} else {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	}
	return ret;
}

/**
 * Parses the data passed in and populates a DIE as a base type
 * param[in] data: the data to be parsed, it should consist of the following characters only: "1234567890-"
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseBaseType(const string & data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int index = toInt(data) * -1;

	/* Populate the DIE according to the base type specified. */
	if (!BUILT_IN_TYPES[index].empty()) {
		Dwarf_Attribute name = new Dwarf_Attribute_s;
		Dwarf_Attribute size = new Dwarf_Attribute_s;
		if ((NULL == name) || (NULL == size)) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			name->_type = DW_AT_name;
			name->_nextAttr = size;
			name->_form = DW_FORM_string;
			name->_sdata = 0;
			name->_udata = 0;
			name->_stringdata = strdup(BUILT_IN_TYPES[index].c_str());
			name->_refdata = 0;
			name->_ref = NULL;

			size->_type = DW_AT_byte_size;
			size->_nextAttr = NULL;
			size->_form = DW_FORM_udata;

			/* Divide size in bits by 8 bits/byte to get size in bytes. */
			size->_sdata = 0;
			size->_udata = BUILT_IN_TYPE_SIZES[index] / 8;
			size->_refdata = 0;
			size->_ref = NULL;

			currentDie->_attribute = name;
		}
	} else {
		/* It is a redundant built-in type; create a typedef instead. */
		string builtInType = "0";
		switch (index) {
		case 6:
		case 27:
			builtInType = "-2";
			break;
		case 9:
			builtInType = "-8";
			break;
		case 15:
		case 29:
			builtInType = "-1";
			break;
		case 17:
			builtInType = "-12";
			break;
		case 18:
			builtInType = "-13";
			break;
		case 20:
			builtInType = "-5";
			break;
		case 28:
			builtInType = "-3";
			break;
		case 30:
			builtInType = "-7";
			break;
		case 33:
			builtInType = "-32";
			break;
		case 34:
			builtInType = "-31";
			break;
		}
		ret = parseTypeDef(builtInType, BUILT_IN_TYPES_THAT_ARE_TYPEDEFS[index], currentDie, error);
	}
	return ret;
}

/*
 * Parses the data passed in and populates a DIE as a constant/pointer/volatile/subroutine type (all of those types have the same structure)
 * param[in] data: the data to be parsed, it should be of the form (*|k|V)(TYPE)
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseNamelessReference(const string & data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Give the DIE a type attribute. */
	Dwarf_Attribute type = new Dwarf_Attribute_s;
	if (NULL == type) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		type->_type = DW_AT_type;
		type->_form = DW_FORM_ref1;
		type->_nextAttr = NULL;
		const string substr = data.substr(1);
		int ID = toInt(substr.substr(0, substr.find_first_not_of("1234567890-")));
		type->_udata = 0;
		type->_refdata = 0;
		type->_ref = NULL;

		/* Add the attribute to the list of refs to be populated. */
		addRefToPopulate(ID, type);

		currentDie->_attribute = type;
	}
	return ret;
}

/**
 * Parses a line of data and returns the declaration type of the data string.
 *
 * @param[in]  data: the string to be parsed. data should be the portion of the declaration line after "debug     0    decl" and whitespace.
 * @param[out] tag: the type of the declaration of the data string
 * @param[out] typeID: the ID of the entity being declared
 * @param[out] format: the format of the declaration being parsed (used for structures, unions and classes)
 * @param[out] declarationData: members/type of the entity being declared
 * @param[out] name: the name of the entity being declared
 */
static Dwarf_Half
parseDeclarationLine(const string & data,
	int *typeID,
	declFormat *format,
	string *declarationData,
	string *name)
{
	Dwarf_Half ret = DW_TAG_unknown;

	/* 'data' is of the format: (DECLARATION_NAME):(c|t|T)(INTEGER_TYPE_ID)=(DECLARATION_TYPE_AND_SIZE)(FIELD_MEMBERS)
	 * Split the declaration into the name and the information about the declaration.
	 */
	size_t indexOfFirstColon = data.find_first_of(':');
	if (string::npos != indexOfFirstColon) {
		/* The data first includes the name of the declaration, then a ":", and then the rest of the data. */
		*declarationData = stripLeading(data.substr(indexOfFirstColon), ':');
		*name = data.substr(0, indexOfFirstColon);

		/* A "T" after the ":" indicates a type and a "t" after the ":" indicates a typedef.
		 * The number proceeding it is unique integer type ID for the type. This is followed by "=" and
		 * then the type of the declaration (union/struct/etc).
		 */
		if ((0 == (*name).size()) && ('t' == (*declarationData).at(0))) {
			/* This typedef declaration is nameless. */
			str_vect tmp = split(*declarationData, '=');
			*typeID = extractTypeID(tmp[0]);
			string type = tmp[1];

			/* Parse the data string to get the type of the expression. */
			switch (type[0]) {
			case '-': /* Built-in type */
				ret = DW_TAG_base_type;
				break;
			case 'k': /* Constant */
				ret = DW_TAG_const_type;
				break;
			case '*': /* Pointer */
				ret = DW_TAG_pointer_type;
				break;
			case 'V': /* Volatile */
				ret = DW_TAG_volatile_type;
				break;
			case 'a': /* Array */
				ret = DW_TAG_array_type;
				break;
			case 'f': /* Subprogram */
				ret = DW_TAG_subroutine_type;
				break;
			case '&': /* Reference */
				ret = DW_TAG_reference_type;
				break;
			case 'm': /* Pointer to member type */
				ret = DW_TAG_ptr_to_member_type;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				/* Typedef declaration */
				ret = DW_TAG_typedef;
				break;
			case 'Z': /* C++ ellipses parameter type */
				ret = DW_TAG_unspecified_type;
				break;
			default:
				ret = DW_TAG_unknown;
				break;
			}
		} else if ('T' == (*declarationData).at(0)) {
			size_t indexOfFirstEqualsSign = (*declarationData).find('=');
			/* After the "=", the declaration type and field members have two possible formats: C-style and C++-stype.
			 * C-style formatting is: (e/s/u)(DECLARED_TYPE_SIZE)(FIELD_MEMBERS)
			 * C++-style formatting is: Y(DECLARED_TYPE_SIZE)(c/e/s/u)((FIELD_MEMBERS))
			 */
			if (string::npos != indexOfFirstEqualsSign) {
				*typeID = extractTypeID((*declarationData).substr(0, indexOfFirstEqualsSign));
				string members = stripLeading((*declarationData).substr(indexOfFirstEqualsSign), '=');
				if (string::npos != (*declarationData).find('(')) {
					/* C++ type class/struct/union declaration */
					size_t indexOfFirstParenthesis = members.find('(');
					string sizeAndType = stripLeading(members.substr(0, indexOfFirstParenthesis), 'Y');
					size_t indexOfFirstNonInteger = sizeAndType.find_first_not_of("1234567890");
					members = stripLeading(members.substr(indexOfFirstParenthesis), '(');
					*format = CPP_FORMAT;
					/* Check if the declaration is for a class, structure, or union. */
					switch(sizeAndType[indexOfFirstNonInteger]) {
					case 'c':
						ret = DW_TAG_class_type;
						break;
					case 's':
						ret = DW_TAG_structure_type;
						break;
					case 'u':
						ret = DW_TAG_union_type;
						break;
					default:
						ret = DW_TAG_unknown;
						break;
					}
				} else {
					/* C type struct/union/enum declaration */
					*format = C_FORMAT;
					if ('e' == members[0]) {
						ret = DW_TAG_enumeration_type;
					} else if ('s' == members[0]) {
						ret = DW_TAG_structure_type;
					} else if ('u' == members[0]) {
						ret = DW_TAG_union_type;
					}
				}
			}
		} else if ('t' == (*declarationData).at(0)) {
			/* Typedef declaration */
			ret = DW_TAG_typedef;
			str_vect tmp = split(*declarationData, '=');
			*typeID = extractTypeID(tmp[0]);
		}
	}
	return ret;
}

/**
 * Takes in a declaration for an entity and creates a Dwarf DIE for it
 * param[in] line: declaration for an entity. It should be the portion of a declaration line after "^\\[.*\\].*m.*debug.*decl\\s+"
 */
static int
parseStabstringDeclarationIntoDwarfDie(const string & line, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int typeID = 0;
	declFormat format = NO_FORMAT;
	string declarationData;
	string name;

	/* We are parsing the section of debug info containing type declarations, comprised of lines beginning with "debug      0       decl".
	 * Each declaration line is of the format: (DECLARATION_NAME):(c|t|T)(INTEGER_TYPE_ID)=(DECLARATION_TYPE)(FIELD_MEMBERS)
	 * where DECLARATION_TYPE is one of: (*(INTEGER)|a|b|c|C|d|D|e|f|g|i|k|m|M|r|s|S|V|w|Z). See parseDeclarationLine() for more information.
	 * The format differs for constants, typedefs, structures, etc. First, determine the type of declaration. Then parse that specific format.
	 */

	/* Get the type of the data of the parsed line. */
	Dwarf_Half tag = parseDeclarationLine(line, &typeID, &format, &declarationData, &name);

	/* Parse the line further based on the type of the declaration. */
	if (DW_TAG_unknown == tag) {
		ERRMSG("Unknown stabs tag: '%s'", line.c_str());
		/*
		 * Older versions of the dump utility truncate very long lines.
		 * Sometimes that truncation erases a continuation marker leaving
		 * us trying to interpret a continuation as if it were the beginning
		 * of a new STABS record. Instead of signalling an error, which would
		 * break builds that use a broken dump utility, we assume that this
		 * is a continuation that doesn't include any vital information.
		 * Once all build machines have been updated with a corrected version
		 * of the dump utility, we should signal an error by un-commenting the
		 * line below.
		 */
		/* ret = DW_DLV_ERROR; */
	} else {
		Dwarf_Die newDie = NULL;

		/* Search for the DIE in createdDies before creating a new one. */
		die_map::iterator existingDieEntry = createdDies.find(typeID);
		if (createdDies.end() != existingDieEntry) {
			newDie = existingDieEntry->second.second;
		}

		/* Create a new DIE only if it wasn't previously created. */
		if (NULL == newDie) {
			newDie = new Dwarf_Die_s;
			if (NULL == newDie) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			} else {
				newDie->_tag = tag;
				newDie->_parent = Dwarf_CU_Context::_currentCU->_die;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;
				newDie->_attribute = NULL;

				/* Insert the new DIE into createdDies. */
				createdDies[typeID] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

				refNumber = refNumber + 1;

				/* If the current CU DIE doesn't have a child then make the first new new DIE the child. */
				if (NULL == Dwarf_CU_Context::_currentCU->_die->_child) {
					Dwarf_CU_Context::_currentCU->_die->_child = newDie;
				}

				/* Set the last created DIE's sibling to be this DIE. */
				if (NULL != _lastDie) {
					_lastDie->_sibling = newDie;
					newDie->_previous = _lastDie;
				}
				_lastDie = newDie;
			}
		} else if (DW_TAG_structure_type != tag) {
			/* Forward declarations of unions can be found as structs. Update them to union. */
			newDie->_tag = tag;
		}
		if (NULL == newDie) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			/* Do some preliminary parsing of the declarationData. */
			str_vect tmp = split(declarationData, '=');
			string members = tmp[1];

			/* Populate the dies. */
			switch (tag) {
			case DW_TAG_array_type:
				ret = parseArray(members, newDie, error);
				break;
			case DW_TAG_base_type:
				ret = parseBaseType(members, newDie, error);
				break;
			case DW_TAG_class_type:
				ret = parseCPPstruct(members, name, newDie, error);
				break;
			case DW_TAG_const_type:
			case DW_TAG_pointer_type:
			case DW_TAG_volatile_type:
			case DW_TAG_subroutine_type:
			case DW_TAG_reference_type:
				ret = parseNamelessReference(members, newDie, error);
				break;
			case DW_TAG_ptr_to_member_type:
				/* skip past optional 'virtual base' specification */
				if ('v' == members[1]) {
					members = members.substr(1);
				}
				/* skip past optional 'multiple base' specification */
				if ('m' == members[1]) {
					members = members.substr(1);
				}
				ret = parseNamelessReference(members, newDie, error);
				break;
			case DW_TAG_enumeration_type:
				ret = parseEnum(members, name, newDie, error);
				break;
			case DW_TAG_typedef:
				/* Note: AIX label is the same as DWARF typedef. */
				ret = parseTypeDef(members, name, newDie, error);
				break;
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				if (C_FORMAT == format) {
					ret = parseCstruct(members, name, newDie, error);
				} else if (CPP_FORMAT == format) {
					ret = parseCPPstruct(members, name, newDie, error);
				}
				break;
			default:
				/* No-op */
				break;
			}
		}
	}
	return ret;
}

/**
 * Takes in and parses the AIX symbol table line-by-line, creating Dwarf DIEs and CU Contexts.
 * param[in] line: A line read from AIX symbol table
 */
static int
parseSymbolTable(const char *line, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	string data = line;
	string fileName = "";

	/* This state machine processes each stage of the input:
	 * 0: has not reached the start of a new file yet: skip junk data
	 * 1: reached the start of a new file: parse to get info about the file and populate Dwarf_CU_Context::_currentCU
	 * 2: inside of a file: skip the section listing the includes until the first type declaration is found
	 * 3: parse declarations: parse to get the information to populate dwarf DIEs and check if end of file reached
	 */
	if (3 != _startNewFile) {
		stabsString.clear();
	}
	if (0 == _startNewFile) {
		/* Ignore the information before the start of the debug information for a file.
		 * Search for the pattern "debug    3   FILE", indicating the start of a new file, to progress to step 2.
		 */
		size_t indexOfDebug = data.find(START_OF_FILE[0]);
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + START_OF_FILE[0].length());
			size_t indexOfNumber = dataAfterDebug.find(START_OF_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + START_OF_FILE[1].length());
				size_t indexOfFile = dataAfterNumber.find(START_OF_FILE[2]);
				if (string::npos != indexOfFile) {
					/* Verify that there are only whitespaces between the words. */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfFile == dataAfterNumber.find_first_not_of(' '))) {
						/* Populate the attribute references and nested classes from the previous file that we parsed. */
						ret = populateAttributeReferences();
						if (DW_DLV_OK == ret) {
							populateNestedClasses();

							/* Remove DIEs which have no children, no size, and are not referred to by any other DIE. */
							removeUselessStubs();

							/* Clear the unordered_maps we maintained from the last file. */
							createdDies.clear();
							nestedClassesToPopulate.clear();
							refsToPopulate.clear();
							/* Change state to expect the start of a section of debug info for a file next. */
							_startNewFile = 1;
						}
					}
				}
			}
		}
	} else if (1 == _startNewFile) {
		/* Verify that the section for a new file begins with a line starting with " a0 ". */
		if (string::npos != data.find(FILE_NAME)) {
			/* Get the name of the file this section of debug info is for. */
			size_t fileNameIndex = data.find(FILE_NAME) + FILE_NAME.size();
			size_t index = data.substr(fileNameIndex).find_first_not_of(' ');

			fileName = strip(data.substr(index + fileNameIndex), '\n');

			/* Verify that the file has not been parsed through before. */
			if (Dwarf_CU_Context::_fileId.end() == Dwarf_CU_Context::_fileId.find(fileName)) {
				/* Add the file to the list of files */
				Dwarf_CU_Context::_fileId.insert(make_pair(fileName, Dwarf_CU_Context::_fileId.size()));

				/* Create one compilation unit context per file. */
				Dwarf_CU_Context *newCU = new Dwarf_CU_Context;

				/* Create a root DIE for the DIE tree in CU. */
				Dwarf_Die newDie = new Dwarf_Die_s;

				/* Create name attribute for the DIE. */
				Dwarf_Attribute name = new Dwarf_Attribute_s;

				if ((NULL == newCU) || (NULL == newDie) || (NULL == name)) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
				} else {
					newCU->_CUheaderLength = 0;
					newCU->_versionStamp = 0;
					newCU->_abbrevOffset = 0;
					newCU->_addressSize = 0;
					newCU->_nextCUheaderOffset = 0;
					newCU->_nextCU = NULL;
					newCU->_die = newDie;

					newDie->_tag = DW_TAG_compile_unit;
					newDie->_parent = NULL;
					newDie->_sibling = NULL;
					newDie->_previous = NULL;
					newDie->_child = NULL;
					newDie->_context = newCU;
					newDie->_attribute = name;

					name->_type = DW_AT_name;
					name->_nextAttr = NULL;
					name->_form = DW_FORM_string;
					name->_sdata = 0;
					name->_udata = 0;
					name->_stringdata = strdup(fileName.c_str());
					name->_refdata = 0;
					name->_ref = NULL;

					if (NULL == Dwarf_CU_Context::_firstCU) {
						Dwarf_CU_Context::_firstCU = newCU;
						Dwarf_CU_Context::_currentCU = newCU;
					} else {
						Dwarf_CU_Context::_currentCU->_nextCU = newCU;
						Dwarf_CU_Context::_currentCU = newCU;
					}
					/* Change state to next process the debug info for this file. */
					_startNewFile = 2;
				}
			} else {
				/* The file was already parsed through previously, perhaps in another .so file. */
				/* Skip parsing this file. */
				_startNewFile = 0;
			}
		}
	} else if (2 == _startNewFile) {
		/* Skip lines of the format "debug      0       (b|e)incl" until finding a line of the format "debug    0       decl".
		 * These first lines in the file list includes before the section listing the type declarations.
		 */
		size_t indexOfDebug = data.find(DECL_FILE[0]);
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + DECL_FILE[0].length());
			size_t indexOfNumber = dataAfterDebug.find(DECL_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + DECL_FILE[1].length());
				size_t indexOfDecl = dataAfterNumber.find(DECL_FILE[2]);
				if (string::npos != indexOfDecl) {
					/* Verify that there are only whitespaces between the words. */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfDecl == dataAfterNumber.find_first_not_of(' '))) {
						/* Parse the data line to populate a DIE. */
						string dataAfterDecl = dataAfterNumber.substr(indexOfDecl + DECL_FILE[2].length());
						size_t index = dataAfterDecl.find_first_not_of(' ');
						if (string::npos != index) {
							stabsString.append(strip(dataAfterDecl.substr(index), '\n'));
							size_t stabsLength = stabsString.length();
							if (0 != stabsLength) {
								char lastChar = stabsString[stabsLength - 1];
								if (('?' == lastChar) || ('\\' == lastChar)) {
									/* Erase the last character which marks this entry as a continuation. */
									stabsString.erase(stabsLength - 1);
								} else {
									ret = parseStabstringDeclarationIntoDwarfDie(stabsString, error);
									stabsString.clear();
								}
							}
						}
						_startNewFile = 3;
					}
				}
			} else {
				/* Check if it is the declaration for the start of another file as the previous file may have contained no symbols */
				indexOfNumber = dataAfterDebug.find(START_OF_FILE[1]);
				if (string::npos != indexOfNumber) {
					string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + START_OF_FILE[1].length());
					size_t indexOfFile = dataAfterNumber.find(START_OF_FILE[2]);
					if (string::npos != indexOfFile) {
						/* Verify that there are only whitespaces between the words */
						if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfFile == dataAfterNumber.find_first_not_of(' '))) {
							_startNewFile = 1;
						}
					}
				}
			}
		}
	} else if (3 == _startNewFile) {
		/* Stop parsing whenever the end of the decl section is reached because all of the
		 * declarations in a file are guaranteed to be in a continuous block.
		 */
		size_t indexOfDebug = data.find(DECL_FILE[0]);
		/* Verify that the type declaration section has not ended by checking that the line begins with "debug      0       decl". */
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + DECL_FILE[0].length());
			size_t indexOfNumber = dataAfterDebug.find(DECL_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + DECL_FILE[1].length());
				size_t indexOfDecl = dataAfterNumber.find(DECL_FILE[2]);
				if (string::npos != indexOfDecl) {
					/* Verify that there are only whitespaces between the words/ */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfDecl == dataAfterNumber.find_first_not_of(' '))) {
						/* Parse the data line to populate a DIE. */
						string dataAfterDecl = dataAfterNumber.substr(indexOfDecl + DECL_FILE[2].length());
						size_t index = dataAfterDecl.find_first_not_of(' ');
						if (string::npos != index) {
							stabsString.append(strip(dataAfterDecl.substr(index), '\n'));
							size_t stabsLength = stabsString.length();
							if (0 != stabsLength) {
								char lastChar = stabsString[stabsLength - 1];
								if (('?' == lastChar) || ('\\' == lastChar)) {
									stabsString.erase(stabsLength - 1);
								} else {
									ret = parseStabstringDeclarationIntoDwarfDie(stabsString, error);
									stabsString.clear();
								}
							}
						}
					} else {
						_startNewFile = 0;
						_lastDie = NULL;
					}
				} else {
					_startNewFile = 0;
					_lastDie = NULL;
				}
			} else {
				_startNewFile = 0;
				_lastDie = NULL;
			}
		} else {
			_startNewFile = 0;
			_lastDie = NULL;
		}
	}
	return ret;
}

/**
 * Parses data passed in as an enumerator and populates a DIE
 * param[in] data: the data to be parsed, should be in the format e(INTEGER):(NAME):(INTEGER),(NAME):(INTEGER),(...),;
 * param[in] dieName: the name of the enumerator
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseEnum(const string & data,
	string dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Parse the declaration line for the required fields. */
	string members = stripLeading(data, 'e');
	size_t indexOfFirstNonInteger = members.find_first_not_of("1234567890-");
	if (string::npos != indexOfFirstNonInteger) {
		members = stripLeading(members.substr(indexOfFirstNonInteger), ':');
		members = stripTrailing(members, ';');

		/* Check if the enum was a generated name by AIX, and if so, delete the generated name. */
		if ((0 == strcmp("__", dieName.substr(0, 2).c_str())) && (string::npos == dieName.substr(2).find_first_not_of("1234567890"))) {
			/* Generated names are of the form __(INTEGER). */
			dieName.assign("");
		}
		ret = setDieAttributes(dieName, 4, currentDie, error);

		if (DW_DLV_OK == ret) {
			/* Create new DIEs for each of the enumerators. */
			str_vect enumerators = split(members, ',');

			Dwarf_Die newDie = new Dwarf_Die_s;
			if (NULL == newDie) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			} else {
				newDie->_tag = DW_TAG_enumerator;
				newDie->_parent = currentDie;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_sibling = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;
				newDie->_attribute = NULL;

				currentDie->_child = newDie;
				for (unsigned int i = 1; i < enumerators.size(); ++i) {
					if (DW_DLV_OK == ret) {
						Dwarf_Die tempDie = new Dwarf_Die_s;
						if (NULL == tempDie) {
							ret = DW_DLV_ERROR;
							setError(error, DW_DLE_MAF);
						} else {
							tempDie->_tag = DW_TAG_enumerator;
							tempDie->_parent = currentDie;
							tempDie->_previous = newDie;
							tempDie->_sibling = NULL;
							tempDie->_child = NULL;
							tempDie->_context = Dwarf_CU_Context::_currentCU;
							tempDie->_attribute = NULL;

							newDie->_sibling = tempDie;
							newDie = tempDie;
						}
					}
				}
				if (DW_DLV_OK == ret) {
					/* Create name and value attributes and assign them to each DIE. */
					str_vect tmp;
					newDie = currentDie->_child;
					for (str_vect::iterator it = enumerators.begin(); it != enumerators.end(); ++it) {
						tmp = split(*it, ':');

						Dwarf_Attribute enumeratorName = new Dwarf_Attribute_s;
						Dwarf_Attribute enumeratorValue = new Dwarf_Attribute_s;

						if ((NULL == enumeratorName) || (NULL == enumeratorValue)) {
							ret = DW_DLV_ERROR;
							setError(error, DW_DLE_MAF);
						} else {
							enumeratorName->_type = DW_AT_name;
							enumeratorName->_nextAttr = enumeratorValue;
							enumeratorName->_form = DW_FORM_string;
							enumeratorName->_sdata = 0;
							enumeratorName->_udata = 0;
							enumeratorName->_stringdata = strdup(tmp.front().c_str());
							enumeratorName->_refdata = 0;
							enumeratorName->_ref = NULL;

							enumeratorValue->_type = DW_AT_const_value;
							enumeratorValue->_nextAttr = NULL;
							enumeratorValue->_form = DW_FORM_sdata;
							enumeratorValue->_sdata = strtoll(tmp.back().c_str(), NULL, 10);
							enumeratorValue->_udata = 0;
							enumeratorValue->_refdata = 0;
							enumeratorValue->_ref = NULL;

							newDie->_attribute = enumeratorName;

							newDie = newDie->_sibling;
						}
					}
				}
			}
		}
	} else {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	}
	return ret;
}

/**
 * Parses the data passed in as the fields for a member of a DIE declaration
 * param[in] data: the data to be parsed, should be in the format (NAME):(TYPE),(BIT_OFFSET),(NUM_BITS)
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseField(const string & data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* field[0] is the name and field[1] is field attributes. */
	str_vect field = split(data, ':');
	str_vect fieldAttributes = split(field[1], ',');

	/* Create attributes. */
	Dwarf_Attribute name = new Dwarf_Attribute_s;
	Dwarf_Attribute type = new Dwarf_Attribute_s;
	Dwarf_Attribute declFile = new Dwarf_Attribute_s;
	Dwarf_Attribute offset = new Dwarf_Attribute_s;

	if ((NULL == name) || (NULL == type) || (NULL == declFile) || (NULL == offset)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		name->_type = DW_AT_name;
		name->_nextAttr = type;
		name->_form = DW_FORM_string;
		name->_sdata = 0;
		name->_udata = 0;
		name->_stringdata = strdup(field[0].c_str());
		name->_refdata = 0;
		name->_ref = NULL;

		type->_type = DW_AT_type;
		type->_nextAttr = declFile;
		type->_form = DW_FORM_ref1;

		int ID = extractTypeID(fieldAttributes[0], 0);
		type->_udata = 0;
		type->_sdata = 0;
		type->_refdata = 0;
		type->_ref = NULL;

		/* Push the type into a vector to populate in a second pass. */
		addRefToPopulate(ID, type);
		declFile->_type = DW_AT_decl_file;
		declFile->_form = DW_FORM_udata;
		declFile->_nextAttr = offset;
		declFile->_sdata = 0;
		/* The declaration file number starts at 1. */
		declFile->_udata = Dwarf_CU_Context::_fileId.size();
		declFile->_refdata = 0;
		declFile->_ref = NULL;

		offset->_type = DW_AT_data_member_location;
		offset->_form = DW_FORM_udata;
		offset->_nextAttr = NULL;
		offset->_sdata = 0;
		offset->_udata = 0;
		offset->_refdata = 0;
		offset->_ref = NULL;
		if (fieldAttributes.size() >= 2) {
			offset->_udata = toInt(fieldAttributes[1]) / 8;
		}

		if (fieldAttributes.size() >= 3) {
			/* Do a preliminary check of if the field requires a bitSize or not. */
			bool bitSizeNeeded = true;
			if (0 > ID) {
				if (toInt(fieldAttributes[2]) == BUILT_IN_TYPE_SIZES[ID * -1]) {
					/* If the type is built-in and the sizes match, then bitSize is not needed. */
					bitSizeNeeded = false;
				}
			}
			if (bitSizeNeeded) {
				Dwarf_Attribute bitSize = new Dwarf_Attribute_s;
				offset->_nextAttr = bitSize;

				/* Set the bit size. */
				bitSize->_type = DW_AT_bit_size;
				bitSize->_form = DW_FORM_udata;
				bitSize->_nextAttr = NULL;
				bitSize->_sdata = 0;
				bitSize->_udata = toInt(fieldAttributes[2]);
				bitSize->_refdata = 0;
				bitSize->_ref = NULL;

				bitFieldsToCheck.push_back(currentDie);
			}
		}
		currentDie->_attribute = name;
	}
	return ret;
}

/**
 * Parses the data passed in as a C-style structure/union
 * param[in] data: data to be parsed, in the format (s|u)(INTEGER)(NAME):(TYPE),(BIT_OFFSET),(NUM_BITS);(NAME):(TYPE),(BIT_OFFSET),(NUM_BITS);(...);;
 * param[in] dieName: the name of the structure/union
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseCstruct(const string & data,
	const string & dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Get rid of the first index of data. It only specifies if the declaration is for a struct or union. */
	string unparsedMembers = data.substr(1);
	size_t indexOfFirstNonInteger = unparsedMembers.find_first_not_of("1234567890");
	if (string::npos != indexOfFirstNonInteger) {
		int size = toInt(unparsedMembers.substr(0, indexOfFirstNonInteger));
		unparsedMembers = unparsedMembers.substr(indexOfFirstNonInteger);

		/* Set the DIE attributes. */
		ret = setDieAttributes(dieName, size, currentDie, error);

		if (DW_DLV_OK == ret) {
			/* Get all of the members of the structure. */
			str_vect members = split(unparsedMembers, ';');

			/* Get the last child of the DIE. */
			Dwarf_Die lastChild = currentDie->_child;
			if (NULL != lastChild) {
				while (NULL != lastChild->_sibling) {
					lastChild = lastChild->_sibling;
				}
			}

			for (unsigned int i = 0; i < members.size(); ++i) {
				Dwarf_Die newDie = new Dwarf_Die_s;
				if (NULL == newDie) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
					break;
				}
				newDie->_tag = DW_TAG_member;
				newDie->_parent = currentDie;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;
				newDie->_attribute = NULL;

				ret = parseField(members[i], newDie, error);

				if (DW_DLV_OK == ret) {
					/* Set the member to be a child of the parent DIE. */
					if (NULL == lastChild) {
						currentDie->_child = newDie;
					} else {
						lastChild->_sibling = newDie;
						newDie->_previous = lastChild;
					}
					lastChild = newDie;
				} else {
					deleteDie(newDie);
					break;
				}
			}
		}
	} else {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	}
	return ret;
}

/**
 * Parses data passed in as a C++ style class/union/struct
 * param[in] data: the data to be parsed, in the format Y(TYPE)(c|u|s){1}(V)?:(TYPE)((MEMBERS);;
 * param[in] dieName: the name of the class/structure/union
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseCPPstruct(const string & data,
	const string & dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	size_t indexOfFirstParenthesis = data.find('(');
	if (string::npos != indexOfFirstParenthesis) {
		string sizeTypeAndInheritance = stripLeading(data.substr(0, indexOfFirstParenthesis), 'Y');
		size_t indexOfFirstNonInteger = sizeTypeAndInheritance.find_first_not_of("1234567890");
		if (string::npos != indexOfFirstNonInteger) {
			string unparsedMembers = stripLeading(data.substr(indexOfFirstParenthesis), '(');

			unsigned int size = toInt(sizeTypeAndInheritance.substr(0, indexOfFirstNonInteger));

			/* If there is a parent class, create a DIE with tag DW_TAG_inheritance.
			 * The numbers after the colon denote the ID of the parent class this class inherits from.
			 */
			size_t indexOfColon = sizeTypeAndInheritance.find(':');
			if (string::npos != indexOfColon) {
				/* A parent class exists. */
				int parentClassID = toInt(stripLeading(sizeTypeAndInheritance.substr(indexOfColon), ':'));

				/* Create a DIE to record the inheritance. */
				Dwarf_Die inheritanceDie = new Dwarf_Die_s;
				Dwarf_Attribute type = new Dwarf_Attribute_s;

				if ((NULL == type) || (NULL == inheritanceDie)) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
				} else {
					inheritanceDie->_tag = DW_TAG_inheritance;
					inheritanceDie->_parent = currentDie;
					inheritanceDie->_sibling = NULL;
					inheritanceDie->_previous = NULL;
					inheritanceDie->_child = NULL;
					inheritanceDie->_context = Dwarf_CU_Context::_currentCU;
					inheritanceDie->_attribute = type;

					type->_type = DW_AT_type;
					type->_nextAttr = NULL;
					type->_form = DW_FORM_ref1;
					type->_udata = 0;
					type->_sdata = 0;
					type->_refdata = 0;
					type->_ref = NULL;

					/* Push the type ref into a vector to populate in a second pass. */
					addRefToPopulate(parentClassID, type);

					if (NULL == currentDie->_child) {
						currentDie->_child = inheritanceDie;
					} else {
						Dwarf_Die lastChild = currentDie->_child;
						while (NULL != lastChild->_sibling) {
							lastChild = lastChild->_sibling;
						}
						lastChild->_sibling = inheritanceDie;
					}
				}
			}

			if (DW_DLV_OK == ret) {
				/* Set DIE attributes. */
				ret = setDieAttributes(dieName, size, currentDie, error);
			}

			if (DW_DLV_OK == ret) {
				/* Parse the members. */
				str_vect members = split(unparsedMembers, ';');

				Dwarf_Die lastChild = currentDie->_child;
				if (NULL != lastChild) {
					while (NULL != lastChild->_sibling) {
						lastChild = lastChild->_sibling;
					}
				}
				for (unsigned int i = 0; i < members.size(); ++i) {
					if (2 <= members[i].size()) {
						size_t indexOfFirstColon = members[i].find_first_of(':');
						if (string::npos != members[i].find_first_of('[')) {
							/* Ignore member function. */
						} else if (string::npos != members[i].find_first_of(']')) {
							/* Ignore friend function. */
						} else if (string::npos != members[i].find_first_of(':')) {
							/* Check to see if the data member is valid. */
							if ((indexOfFirstColon > 0) && ('s' == members[i].at(indexOfFirstColon - 1))) {
								/* Ignore static field. */
							} else {
								Dwarf_Die newDie = new Dwarf_Die_s;
								if (NULL == newDie) {
									ret = DW_DLV_ERROR;
									setError(error, DW_DLE_MAF);
									break;
								}
								newDie->_tag = DW_TAG_member;
								newDie->_parent = currentDie;
								newDie->_sibling = NULL;
								newDie->_previous = NULL;
								newDie->_child = NULL;
								newDie->_context = Dwarf_CU_Context::_currentCU;
								newDie->_attribute = NULL;

								ret = parseField(members[i].substr(indexOfFirstColon + 1), newDie, error);
								if (DW_DLV_OK == ret) {
									/* Set the member as a child of the parent die. */
									if (NULL == lastChild) {
										currentDie->_child = newDie;
									} else {
										lastChild->_sibling = newDie;
										newDie->_previous = lastChild;
									}
									lastChild = newDie;
								} else {
									deleteDie(newDie);
									break;
								}
							}
						} else if ('N' == members[i].at(1)) {
							/* Verify that the nested class is valid. */
							/* Get the ID of the nested class from tmp[1]. */
							str_vect tmp = split(members[i], 'N');
							int ID = toInt(tmp[1]);
							nestedClassesToPopulate.push_back(make_pair<int, Dwarf_Die>((int)ID, (Dwarf_Die)currentDie));
						}
					}
				}
			}
		} else {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_IA);
		}
	} else {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	}
	return ret;
}

/**
 * Parses data passed in as a typedef.
 * param[in] data: The data to parse, should be in the format (TYPE)
 * param[in] dieName: The name of the typedef
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseTypeDef(const string & data,
	const string & dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int ID = 0;

	/* Set attributes for name, type, and declaring file. */
	Dwarf_Attribute type = new Dwarf_Attribute_s;
	Dwarf_Attribute name = new Dwarf_Attribute_s;
	Dwarf_Attribute declFile = new Dwarf_Attribute_s;
	Dwarf_Attribute declLine = new Dwarf_Attribute_s;

	if ((NULL == type) || (NULL == name) || (NULL == declFile) || (NULL == declLine)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		type->_type = DW_AT_type;
		type->_form = DW_FORM_ref1;
		type->_nextAttr = name;

		/* AIX classifies int64_t and uint64_t as intptr_t (IDATA) and uintptr_t (UDATA) respectively. */
		if ((0 == strcmp("int64_t", dieName.c_str())) || (0 == strcmp("uint64_t", dieName.c_str()))) {
			if (-36 == toInt(data)) {
				ID = -32;
			} else if (-35 == toInt(data)) {
				ID = -31;
			} else {
				ID = toInt(data);
			}
		} else {
			ID = toInt(data);
		}
		type->_udata = 0;
		type->_sdata = 0;
		type->_refdata = 0;
		type->_ref = NULL;

		/* Add the new type attribute to the list of refs to populate on a second pass. */
		addRefToPopulate(ID, type);
		name->_type = DW_AT_name;
		name->_form = DW_FORM_string;
		name->_nextAttr = declLine;
		name->_sdata = 0;
		name->_udata = 0;
		name->_stringdata = strdup(dieName.c_str());
		name->_refdata = 0;
		name->_ref = NULL;

		declLine->_type = DW_AT_decl_line;
		declLine->_nextAttr = declFile;
		declLine->_form = DW_FORM_udata;
		declLine->_sdata = 0;
		declLine->_udata = declarationLine;
		declLine->_refdata = 0;
		declLine->_ref = NULL;

		declarationLine = declarationLine + 1;

		declFile->_type = DW_AT_decl_file;
		declFile->_form = DW_FORM_udata;
		declFile->_nextAttr = NULL;
		declFile->_sdata = 0;
		declFile->_refdata = 0;
		declFile->_ref = NULL;
		if (!Dwarf_CU_Context::_fileId.empty()) {
			/* The declaration file number starts at 1. */
			declFile->_udata = Dwarf_CU_Context::_fileId.size();
		} else {
			/* If there is no declaring file then delete the attributes for the declFile and declLine. */
			name->_nextAttr = NULL;
			delete declFile;
			delete declLine;
		}

		currentDie->_attribute = type;
	}
	return ret;
}

/**
 * Looks up and populates the reference attributes which were added while populating Dwarf DIEs
 */
static int
populateAttributeReferences()
{
	int ret = DW_DLV_OK;

	/* Iterate through the vector containing all attributes to be populated. */
	vector<pair<int, Dwarf_Attribute> >::const_iterator fixup = refsToPopulate.begin();
	for (; refsToPopulate.end() != fixup; ++fixup) {
		int id = fixup->first;

		/* Negative identifiers refer to built-in types, others refer to explicit types. */
		die_map *dieMap = (id < 0) ? &builtInDies : &createdDies;

		/* Search for the reference in the DIE map. */
		die_map::const_iterator existingDieEntry = dieMap->find(id);

		if (dieMap->end() == existingDieEntry) {
			ERRMSG("Cannot resolve reference to type %d\n", id);
			ret = DW_DLV_ERROR;
		} else {
			Dwarf_Attribute attr = fixup->second;
			Dwarf_Off offset = existingDieEntry->second.first;
			Dwarf_Die target = existingDieEntry->second.second;

			/* Set _ref and _refdata to the correct value and insert the reference into refMap. */
			attr->_ref = target;
			attr->_refdata = offset;
			Dwarf_Die_s::refMap.insert(make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)offset, (Dwarf_Die)target));
		}
	}

	return ret;
}

/**
 * Creates DIEs for built-in types
 */
static int
populateBuiltInTypeDies(Dwarf_Error *error)
{
	/* Built in types
	 * -1  int, .............. 32 bit signed integeral type
	 * -5  char, ............. 8 bit < AIX specific
	 * -3  short. ............ 16 bit signed integral type
	 * -4  long, ............. 32 bit signed intergraal type
	 * -5  unsigned char, .... 8 bit unsigned integral type
	 * -6  signed char, ...... 8 bit signed integral type
	 * -7  unsigned short, ... 16 bit unsigned integral type
	 * -8  unsigned int, ..... 32 bit unsigned integral type
	 * -9  unsigned, ......... 32 bit unsigned integral type
	 * -10 unsigned long, .... 32 bit unsigned integral type
	 * -11 void, ............. type indicating the lack of value
	 * -12 float, ............ IEEE single precision
	 * -13 double, ........... IEEE double precision
	 * -14 long double, ...... IEEE double precision
	 * -15 integer, .......... 32 bit signed integral type
	 * -16 boolean, .......... 32 bit type < 0 false and 1 true, others unspecified meaning
	 * -17 short real, ....... IEEE single precision
	 * -18 real, ............. IEEE double precision
	 * -19 stringptr
	 * -20 character, ........ 8 bit unsigned character type
	 * -21 logical*1, ........ 8 bit type < FORTRAN
	 * -22 logical*2, ........ 16 bit type < FORTRAN
	 * -23 logical*4, ........ 32 bit type for boolean variables < FORTRAN
	 * -24 logical, .......... 32 bit type < FORTRAN
	 * -25 complex, .......... consisting of two IEEE single-precision floating point values
	 * -26 complex, .......... consisting of two IEEE double-precision floating point values
	 * -27 integer*1, ........ 8 bit signed integral type
	 * -28 integer*2, ........ 16 bit signed integral type
	 * -29 integer*4, ........ 32 bit signed integral type
	 * -30 wchar, ............ wide character 16 bit wide
	 * -31 logn long, ........ 64 bit unsigned intergral type
	 * -32 unsigned long long, 64 bit unsigned integral type
	 * -33 logical*8, ........ 64 bit unsigned integral type
	 * -34 integer*8, ........ 64 bit singned integral type
	 */
	int ret = DW_DLV_OK;
	Dwarf_Die previousDie = NULL;

	/* Populate the base types. */
	for (unsigned int i = 1; i <= NUM_BUILT_IN_TYPES; ++i) {
		if ((DW_DLV_OK == ret) && (!BUILT_IN_TYPES[i].empty())) {
			Dwarf_Die newDie = new Dwarf_Die_s;
			if (NULL == newDie) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			} else {
				newDie->_tag = DW_TAG_base_type;
				newDie->_parent = NULL;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = NULL;

				Dwarf_Attribute name = new Dwarf_Attribute_s;
				Dwarf_Attribute size = new Dwarf_Attribute_s;

				if ((NULL == name)||(NULL == size)) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
				} else {
					name->_type = DW_AT_name;
					name->_form = DW_FORM_string;
					name->_nextAttr = size;
					name->_sdata = 0;
					name->_udata = 0;
					name->_stringdata = strdup(BUILT_IN_TYPES[i].c_str());
					name->_refdata = 0;
					name->_ref = NULL;

					size->_type = DW_AT_byte_size;
					size->_form = DW_FORM_udata;
					size->_nextAttr = NULL;
					size->_sdata = 0;

					/* Divide the size in bits by 8 bits per byte to get the size in bytes */
					size->_udata = BUILT_IN_TYPE_SIZES[i] / 8;
					size->_refdata = 0;
					size->_ref = NULL;

					newDie->_attribute = name;
					if (NULL == _builtInTypeDie) {
						_builtInTypeDie = newDie;
					}
					if (NULL != previousDie) {
						previousDie->_sibling = newDie;
						newDie->_previous = previousDie;
					}
					previousDie = newDie;

					/* Insert the newly created DIE into the unordered_map for lookup by offset later on. */
					builtInDies[i * -1] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

					/* Increment the refNumber. */
					refNumber = refNumber + 1;
				}
			}
		}
	}

	/* For redundant types, create typedefs that refer to the base types created above. */
	for (unsigned int i = 0; i <= NUM_BUILT_IN_TYPES; ++i) {
		if ((DW_DLV_OK == ret) && (!BUILT_IN_TYPES_THAT_ARE_TYPEDEFS[i].empty())) {
			Dwarf_Die newDie = new Dwarf_Die_s;
			if (NULL == newDie) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			} else {
				newDie->_tag = DW_TAG_typedef;
				newDie->_parent = NULL;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = NULL;

				string data = "0";
				switch (i) {
				case 6:
				case 27:
					data = "-2";
					break;
				case 9:
					data = "-8";
					break;
				case 15:
				case 29:
					data = "-1";
					break;
				case 17:
					data = "-12";
					break;
				case 18:
					data = "-13";
					break;
				case 20:
					data = "-5";
					break;
				case 28:
					data = "-3";
					break;
				case 30:
					data = "-7";
					break;
				case 33:
					data = "-32";
					break;
				case 34:
					data = "-31";
					break;
				}
				ret = parseTypeDef(data, BUILT_IN_TYPES_THAT_ARE_TYPEDEFS[i], newDie, error);

				if (DW_DLV_OK == ret) {
					/* Add the new child to the linked list of DIEs. */
					if (NULL != previousDie) {
						previousDie->_sibling = newDie;
						newDie->_previous = previousDie;
					}
					previousDie = newDie;

					/* Insert the typedef into the unordered_map for lookup by offset later on. */
					builtInDies[i * -1] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

					/* Increment the refNumber to assign associate a unique number with each DIE. */
					refNumber = refNumber + 1;
				} else {
					delete newDie;
				}
			}
		}
	}
	return ret;
}

/**
 * Finds nested classes, and moves them to their appropriate locations as children of the DIEs that they are a member of
 */
static void
populateNestedClasses()
{
	/* Iterate through all the nested classes found. */
	for (unsigned int i = 0; i < nestedClassesToPopulate.size(); ++i) {
		Dwarf_Die parent = nestedClassesToPopulate[i].second;
		Dwarf_Die nestedClass = NULL;
		if (NULL != parent) {
			die_map::iterator existingDieEntry = createdDies.find(nestedClassesToPopulate[i].first);
			if (createdDies.end() != existingDieEntry) {
				nestedClass = existingDieEntry->second.second;
				if (NULL != nestedClass) {
					/* Get the last child of the DIE. */
					Dwarf_Die lastChild = parent->_child;
					if (NULL != lastChild) {
						while (NULL != lastChild->_sibling) {
							lastChild = lastChild->_sibling;
						}
					}

					/* In case the current parent of nestedClass has nestedClass as the child, set it to be the sibling of nestedClass. */
					if (nestedClass == nestedClass->_parent->_child) {
						nestedClass->_parent->_child = nestedClass->_sibling;
					}

					/* Remove the nested class from the linked list of DIEs. */
					if (NULL != nestedClass->_previous) {
						nestedClass->_previous->_sibling = nestedClass->_sibling;
					}
					if (NULL != nestedClass->_sibling) {
						nestedClass->_sibling->_previous = nestedClass->_previous;
					}
					nestedClass->_sibling = NULL;
					nestedClass->_previous = NULL;

					/* Set the nested class DIE to be a child of its parent. */
					if (NULL == lastChild) {
						parent->_child = nestedClass;
					} else {
						lastChild->_sibling = nestedClass;
						nestedClass->_previous = lastChild;
					}
					nestedClass->_parent = parent;
				}
			}
		}
	}
}

static void
removeUselessStubs()
{
	die_map::iterator it = createdDies.begin();
	while (it != createdDies.end()) {
		Dwarf_Die dieToCheck = it->second.second;
		Dwarf_Off refNumberToCheck = it->second.first;
		if (Dwarf_Die_s::refMap.end() == Dwarf_Die_s::refMap.find(refNumberToCheck)) {
			/* Get rid of empty struct/union/class DIEs. */
			if ((DW_TAG_class_type == dieToCheck->_tag) || (DW_TAG_union_type == dieToCheck->_tag) || (DW_TAG_structure_type == dieToCheck->_tag)) {
				if ((Dwarf_CU_Context::_currentCU->_die == dieToCheck->_parent) && (NULL == dieToCheck->_child)) {
					/* If the DIE has a size of zero, has no children, and is not being referred to by any other DIE then it's useless. */
					bool zeroSize = true;
					Dwarf_Attribute attributeToCheck = dieToCheck->_attribute;
					while (NULL != attributeToCheck) {
						if (DW_AT_byte_size == attributeToCheck->_type) {
							if (0 != attributeToCheck->_udata) {
								zeroSize = false;
								break;
							}
						}
						attributeToCheck = attributeToCheck->_nextAttr;
					}
					if (zeroSize) {
						/* Remove the DIE and delete it. */
						if (dieToCheck->_parent->_child == dieToCheck) {
							dieToCheck->_parent->_child = dieToCheck->_sibling;
						}
						if (NULL != dieToCheck->_previous) {
							dieToCheck->_previous->_sibling = dieToCheck->_sibling;
						}
						if (NULL != dieToCheck->_sibling) {
							dieToCheck->_sibling->_previous = dieToCheck->_previous;
						}
						delete dieToCheck;
					}
				}
			}
		}
		++it;
	}
}

/**
 * Sets the name, size and declaring file attributes for a DIE that is passed in
 * param[in] dieName: The name of the entity
 * param[in] dieSize: The size of the entity (in bytes)
 * param[out] currentDie: the DIE to populate with the attributes.
 */
static int
setDieAttributes(const string & dieName,
	unsigned int dieSize,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* If the die already had attributes then update them. */
	if (NULL != currentDie->_attribute) {
		Dwarf_Attribute tmp = currentDie->_attribute;
		while (NULL != tmp) {
			switch (tmp->_type) {
			case DW_AT_byte_size:
				tmp->_udata = dieSize;
				break;
			case DW_AT_decl_file:
				/* The declaration file number starts at 1. */
				tmp->_udata = Dwarf_CU_Context::_fileId.size();
				break;
			}
			tmp = tmp->_nextAttr;
		}
	} else {
		/* Create attributes for name, size, and declaring file. */
		Dwarf_Attribute name = new Dwarf_Attribute_s;
		Dwarf_Attribute size = new Dwarf_Attribute_s;
		Dwarf_Attribute declFile = new Dwarf_Attribute_s;
		Dwarf_Attribute declLine = new Dwarf_Attribute_s;

		if ((NULL == name) || (NULL == size) || (NULL == declFile) || (NULL == declLine)) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			name->_type = DW_AT_name;
			name->_nextAttr = size;
			name->_form = DW_FORM_string;
			name->_sdata = 0;
			name->_udata = 0;
			name->_stringdata = strdup(dieName.c_str());
			name->_refdata = 0;
			name->_ref = NULL;

			size->_type = DW_AT_byte_size;
			size->_nextAttr = declFile;
			size->_form = DW_FORM_udata;
			size->_sdata = 0;
			size->_udata = dieSize;
			size->_refdata = 0;
			size->_ref = NULL;

			declFile->_type = DW_AT_decl_file;
			declFile->_nextAttr = declLine;
			declFile->_form = DW_FORM_udata;
			declFile->_sdata = 0;
			/* The declaration file number starts at 1. */
			declFile->_udata = Dwarf_CU_Context::_fileId.size();
			declFile->_refdata = 0;
			declFile->_ref = NULL;

			declLine->_type = DW_AT_decl_line;
			declLine->_nextAttr = NULL;
			declLine->_form = DW_FORM_udata;
			declLine->_sdata = 0;
			declLine->_udata = declarationLine;
			declLine->_refdata = 0;
			declLine->_ref = NULL;
			currentDie->_attribute = name;

			/* Increment the number of the declaration line. */
			declarationLine = declarationLine + 1;
		}
	}
	return ret;
}

/**
 * Splits the string 'data' on delimiter in argument 'delim' and writes non-empty
 * segments to elements.
 *
 * @param[in]  data : string that needs to be split
 * @param[in]  delim : what the string needs to be separated on
 * @param[out] elements : vector to receive the non-empty segments
 */
void
split(const string & data, char delim, str_vect *elements)
{
	for (size_t start = 0; start < data.length();) {
		size_t end = data.find(delim, start);

		if (string::npos != end) {
			if (start < end) {
				elements->push_back(data.substr(start, end - start));
			}
			start = end + 1;
		} else {
			elements->push_back(data.substr(start));
			break;
		}
	}
}

/**
 * Splits the string 'data' on delimiter in argument 'delim' and returns a vector
 * of non-empty segments.
 *
 * @param[in]  data : string that needs to be split
 * @param[in]  delim : what the string needs to be separated on
 *
 * @return vector containing the strings resulting from the split operation
 */
str_vect
split(const string & data, char delim)
{
	str_vect elements;
	split(data, delim, &elements);
	return elements;
}

/**
 * Strips chr from the start and end of the string
 *
 * @param[in] str: the string to be
 * @param[in] chr: the character to be removed from str
 *
 * @return string after trimming char from front and back of the string
 */
string
strip(const string & str, char chr)
{
	string ret = str;
	if (!ret.empty()) {
		ret = stripLeading(ret, chr);
		ret = stripTrailing(ret, chr);
	}
	return ret;
}

/**
 * Strips chr from the start the string
 *
 * @param[in] str: string to be stripped
 * @param[in] chr: the character to be removed from str
 *
 * @return string after trimming char from the start of the string
 */
string
stripLeading(const string & str, char chr)
{
	string ret = str;
	if (!ret.empty()) {
		if (chr == str[0]) {
			ret.erase(0, 1);
		}
	}
	return ret;
}

/**
 * Strips chr from the start the string
 *
 * @param[in] str: string to be stripped
 * @param[in] chr: the character to be removed from str
 *
 * @return string after trimming char from the end of the string
 */
string
stripTrailing(const string & str, char chr)
{
	string ret = str;
	if (!ret.empty()) {
		int strLength = str.length();
		if (chr == ret[strLength - 1]) {
			ret.erase(strLength - 1, 1);
		}
	}
	return ret;
}

/**
 * Converts value from string to double
 *
 * @param[in] value: string value that needs to be converted to double
 *
 * @return double equivalent of the string value
 */
double
toDouble(const string & value)
{
	stringstream ss;
	double ret = 0;
	ss << value;
	ss >> ret;
	return ret;
}

/**
 * Converts value from string to int
 *
 * @param[in] value: string value that needs to be converted to int
 *
 * @return int equivalent of the string value
 */
int
toInt(const string & value)
{
	return (int)toDouble(value);
}

/**
 * Converts string to size_t
 *
 * @param[in] size: string that needs to be parsed as size
 *
 * @return size_t
 */
size_t
toSize(const string & size)
{
	stringstream ss;
	size_t ret = 0;
	ss << size;
	ss >> ret;
	return ret;
}

/**
 * Converts double to string. Used for conversions in the debugging functions.
 *
 * @param[in] value: value that needs to be converted to string
 *
 * @return argument value as string
 */
string
toString(int value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}
