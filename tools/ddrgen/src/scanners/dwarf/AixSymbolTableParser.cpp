/*
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

#include "AixSymbolTableParser.hpp"

/* Statics to create: */
static die_map createdDies;
static die_map builtInDies;
static std::set<string> filesAdded;
/* This vector contains the typeID of the nested class, and parent class for populating nested classes */
static vector<pair<int, Dwarf_Die> > nestedClassesToPopulate;
/* This vector contains the typeID of the type reference, and type attribute to populate */
static vector<pair<int, Dwarf_Attribute> > refsToPopulate;
/* This vector contains the Dwarf DIE with the bitfield attribute, which may have to be removed if it is unneeded */
static vector<Dwarf_Die> bitFieldsToCheck;
static Dwarf_Die _lastDie = NULL;
static Dwarf_Die _builtInTypeDie = NULL;
static Dwarf_Die _undefinedRef = NULL;
/* _startNewFile is a state counter */
static int _startNewFile = 0;
/* Start numbering refs for refMap at one (zero will be for when no ref is found) */
static Dwarf_Off refNumber = 1;
/* This is used to initialize declLine to a unique value for each unique DIE, every time it is required */
static Dwarf_Off declarationLine = 1;
Dwarf_CU_Context * Dwarf_CU_Context::_firstCU;
Dwarf_CU_Context * Dwarf_CU_Context::_currentCU;

static void checkBitFields(Dwarf_Error *error);
static void deleteDie(Dwarf_Die die);
static int parseArray(const string data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseBaseType(const string data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseNamelessReference(const string data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseStabstringDeclarationIntoDwarfDie(const string data, Dwarf_Error *error);
static int parseSymbolTable(const char *line, Dwarf_Error *error);
static Dwarf_Half parseDeclarationLine(string data, int *typeID, declFormat *format, string *declarationData, string *name);
static int parseEnum(const string data, string dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseFields(const string data, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseNewFormat(const string data, const string dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseOldFormat(const string data, const string dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static int parseTypeDef(const string data, const string dieName, Dwarf_Die currentDie, Dwarf_Error *error);
static void populateAttributeReferences();
static int populateBuiltInTypeDies(Dwarf_Error *error);
static void populateNestedClasses();
static void removeUselessStubs();
static int setDieAttributes(const string dieName, const unsigned int dieSize, Dwarf_Die currentDie, Dwarf_Error *error);

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
		delete(Dwarf_CU_Context::_currentCU);
		Dwarf_CU_Context::_currentCU = nextCU;
	}

	/* Delete the built-in type DIEs */
	if (NULL != _builtInTypeDie) {
		deleteDie(_builtInTypeDie);
		_builtInTypeDie = NULL;
	}

	/* Delete the undefined type reference DIE */
	if (NULL != _undefinedRef) {
		deleteDie(_undefinedRef);
		_undefinedRef = NULL;
	}

	_lastDie = NULL;
	Dwarf_CU_Context::_fileList.clear();
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
	/* Initialize some values */
	int ret = DW_DLV_OK;
	FILE *fp = NULL;
	char *buffer = NULL;
	char filepath[100] = {'\0'};
	Dwarf_CU_Context::_firstCU = NULL;
	Dwarf_CU_Context::_currentCU = NULL;
	refNumber = 1;
	size_t len = 0;

	/* Populate the built-in types */
	ret = populateBuiltInTypeDies(error);
	populateAttributeReferences();

	/* Get the path of the file descriptor */
	sprintf(filepath, "/proc/%d/fd/%d", getpid(), fd);

	/* Call dump to get the symbol table */
	if (DW_DLV_OK == ret) {
		stringstream ss;
		ss << "dump -tvXany " << filepath << " 2>&1";
		fp = popen(ss.str().c_str(), "r");
		if (NULL == fp) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_IOF);
		}
	}

	DEBUGPRINTF("Parsing through file.");
	/* Parse through the file */
	while ((-1 != getline(&buffer, &len, fp)) && (DW_DLV_OK == ret)) {
		ret = parseSymbolTable(buffer, error);
	}
	free(buffer);

	if (NULL != fp) {
		pclose(fp);
	}

	/* Populate the references and nested classes of the last file to get parsed */
	populateAttributeReferences();
	populateNestedClasses();
	removeUselessStubs();

	/* Must set _currentCU to null for dwarf_next_cu_header to work */
	Dwarf_CU_Context::_currentCU = NULL;

	/* If the symbol table that we parsed contained no new files, create an empty generic CU and CU die to make sure DwarfScanner doesn't fail */
	if (NULL == Dwarf_CU_Context::_firstCU) {
		Dwarf_CU_Context *newCU = new Dwarf_CU_Context();
		Dwarf_Die newDie = new Dwarf_Die_s();
		Dwarf_Attribute name = new Dwarf_Attribute_s();

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

	/* Create a DIE for undefined references */
	_undefinedRef = new Dwarf_Die_s;
	_undefinedRef->_tag = DW_TAG_unknown;
	_undefinedRef->_parent = NULL;
	_undefinedRef->_sibling = NULL;
	_undefinedRef->_previous = NULL;
	_undefinedRef->_child = NULL;
	_undefinedRef->_context = NULL;
	_undefinedRef->_attribute = NULL;
	Dwarf_Die_s::refMap.insert(make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)0, (Dwarf_Die)_undefinedRef));

	/* Remove bit size attributes for members that don't need bit fields */
	checkBitFields(error);

	/* Clear the unordered_maps that were required during parsing */
	refsToPopulate.clear();
	createdDies.clear();
	nestedClassesToPopulate.clear();
	builtInDies.clear();
	bitFieldsToCheck.clear();

	return ret;
}

static void
checkBitFields(Dwarf_Error *error) {
	for (int i = 0; i < bitFieldsToCheck.size(); i++) {
		/* Since this is a field, the type attribute is guaranteed to be the attribute after the first of the die passed in */
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
		Dwarf_Die tmp = type->_ref;
		if ((NULL != type) && (NULL != bitSize)) {
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
					/* Check the type that this is referring to */

					if (DW_DLV_OK == dwarf_attr(tmp, DW_AT_type, &tmpAttribute, error)) {
						tmp = tmpAttribute->_ref;
					} else {
						baseTypeHasBeenFound = true;
					}
					break;
				case DW_TAG_pointer_type:
					/* Compare the bit size with the pointer size for the current architecture */
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
				/* Remove the bit size attribute */
				tmpAttribute = bitFieldsToCheck[i]->_attribute;
				while (DW_AT_bit_size != tmpAttribute->_nextAttr->_type) {
					tmpAttribute = tmpAttribute->_nextAttr;
				}
				if (NULL != tmpAttribute->_nextAttr->_stringdata) {
					free(tmpAttribute->_nextAttr->_stringdata);
				}
				delete(tmpAttribute->_nextAttr);
				tmpAttribute->_nextAttr = NULL;
			}
		}
	}
}

static void
deleteDie(Dwarf_Die die)
{
	/* Free the Die's attributes. */
	Dwarf_Attribute attr = die->_attribute;
	while (NULL != attr) {
		Dwarf_Attribute next = attr->_nextAttr;
		if (NULL != attr->_stringdata) {
			free(attr->_stringdata);
		}
		delete(attr);
		attr = next;
	}

	/* Free the Die's siblings and children. */
	if (NULL != die->_sibling) {
		deleteDie(die->_sibling);
	}
	if (NULL != die->_child) {
		deleteDie(die->_child);
	}
	delete(die);
}


/**
 * Parses the string to file ID. Extracts the integer betwen the square brackets.
 *
 * @param[in] data: string from which to extract ID from. The expected format is "[####]"
 *
 * @return file ID as int
 */
int
extractFileID(const string data)
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
extractTypeID(const string data, const int index)
{
	stringstream ss;
	int ret = 0;
	ss << data.substr(index);
	ss >> ret;
	return ret;
}

/**
 * Checks the first index of the field declaration string for compiler generated tag, GenSpec.
 *	Possible valid formats, that could potentially return true are:
 *		GenSpec AccessSpec AnonSpec DataMember
 *		GenSpec VirtualSpec AccessSpec OptVirtualFuncIndex MemberFunction
 *
 * @param[in] data: EX: cup5__vfp:__vfp:23,0,32;uN222;uN223;vu1[di:__dt__Q3_3std7_LFS_ON10ctype_baseFv:2;; << This would return true
 *
 * @return boolean value indicating if the declaration is compiler generated or not
 */
bool
isCompilerGenerated(const string data)
{
	return data.empty() ? false : ('c' == data[0]);
}
/**
 * Parses data passed in as an array declaration
 * param[in] data: the data to be parsed, should be in the format ar(INDEX_ID);(LOWER_BOUND);(UPPER_BOUND);(ARRAY_TYPE)
 * param[out] currentDie: the DIE to populate with the parsed data.
 */
static int
parseArray(const string data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (2 <= data.length()) {
		if ("ar" == data.substr(0, 2)) {
			str_vect arrayInfo = split(data, ';');
			/*
			 * arrayInfo will be in the form: ar(INDEX_ID);(LOWER_BOUND);(UPPER_BOUND);(ARRAY_TYPE)
			 * arrayInfo[0] : ar + subrange type
			 * arrayInfo[1] : lower bound
			 * arrayInfo[2] : upper bound
			 * arrayInfo[3] : array type
			 */
			/* Populate array */
			Dwarf_Attribute type = new Dwarf_Attribute_s();

			/* Create a DIE to hold info about the subrange type */
			Dwarf_Die newDie = new Dwarf_Die_s();
			Dwarf_Attribute subrangeType = new Dwarf_Attribute_s();
			Dwarf_Attribute subrangeUpperBound = new Dwarf_Attribute_s();
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

				/* Add it to the list of attributes to populate */
				refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)ID, (Dwarf_Attribute)type));
				currentDie->_attribute = type;
				newDie->_tag = DW_TAG_subrange_type;
				newDie->_parent = currentDie;
				newDie->_sibling = NULL;
				newDie->_previous = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;

				/* subrangeType is of the type sizetype, which is a built-in type (-37)*/
				subrangeType->_type = DW_AT_type;
				subrangeType->_nextAttr = subrangeUpperBound;
				subrangeType->_form = DW_FORM_ref1;
				subrangeType->_udata = 0;
				subrangeType->_refdata = 0;
				subrangeType->_ref = NULL;

				/* Add subrangeType to the list of refs to populate in a second pass */
				refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)-37, (Dwarf_Attribute)subrangeType));

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
parseBaseType(const string data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int index = toInt(data) * -1;

	/* Populate the die according to the base type specified */
	if (!BUILT_IN_TYPES[index].empty()) {
	Dwarf_Attribute name = new Dwarf_Attribute_s();
	Dwarf_Attribute size = new Dwarf_Attribute_s();
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

			/* Divide size in bits by 8bits/byte to get size in bytes */
			size->_sdata = 0;
			size->_udata = BUILT_IN_TYPE_SIZES[index] / 8;
			size->_refdata = 0;
			size->_ref = NULL;

			currentDie->_attribute = name;
		}
	} else {
		/* It is a redundant built-in type, so we create a typedef instead */
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
parseNamelessReference(const string data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Give the DIE a type attribute */
	Dwarf_Attribute type = new Dwarf_Attribute_s();
	if (NULL == type) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		type->_type = DW_AT_type;
		type->_form = DW_FORM_ref1;
		type->_nextAttr = NULL;
		int ID = toInt(data.substr(1));
		type->_udata = 0;
		type->_refdata = 0;
		type->_ref = NULL;

		/* Add the attribute to the list of refs to be populated */
		refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)ID, (Dwarf_Attribute)type));

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
parseDeclarationLine(const string data,
	int *typeID,
	declFormat *format,
	string *declarationData,
	string *name)
{
	Dwarf_Half ret = DW_TAG_unknown;

	/* split the declaration into the name and the information about the declaration */
	size_t indexOfFirstColon = data.find_first_of(':');
	if (string::npos != indexOfFirstColon) {
		*declarationData = stripLeading(data.substr(indexOfFirstColon), ':');
		*name = data.substr(0, indexOfFirstColon);

		if ((0 == (*name).size()) && ('t' == (*declarationData).at(0))) {
			/* Nameless typedef declaration */
			str_vect tmp = split(*declarationData, '=');
			*typeID = extractTypeID(tmp[0]);
			string type = tmp[1];

			/* Parse the data string to get the type of the expression */
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
				ret =  DW_TAG_subroutine_type;
				break;
			default:
				ret = DW_TAG_unknown;
				break;
			}
		} else if ('T' == (*declarationData).at(0)) {
			size_t indexOfFirstEqualsSign = (*declarationData).find('=');
			if (string::npos != indexOfFirstEqualsSign) {
				*typeID = extractTypeID((*declarationData).substr(0, indexOfFirstEqualsSign));
				string members = stripLeading((*declarationData).substr(indexOfFirstEqualsSign), '=');
				if (string::npos != (*declarationData).find('(')) {
					/* C++ type class/struct/union declaration */
					size_t indexOfFirstParenthesis = members.find('(');
					string sizeAndType = stripLeading(members.substr(0, indexOfFirstParenthesis), 'Y');
					size_t indexOfFirstNonInteger = sizeAndType.find_first_not_of("1234567890");
					members = stripLeading(members.substr(indexOfFirstParenthesis), '(');
					*format = NEW_FORMAT;
					/* Check if the declaration is for a class, structure or union */
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
				} else if ('e' == members[0]) {
					ret = DW_TAG_enumeration_type;
				} else {
					/* C type struct/union declaration */
					*format = OLD_FORMAT;
					if ('s' == members[0]) {
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
parseStabstringDeclarationIntoDwarfDie(const string line, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int typeID = 0;
	declFormat format = NO_FORMAT;
	string declarationData;
	string name;
	string members;

	/* get the type of the data of the line being parsed */
	Dwarf_Half tag = parseDeclarationLine(line, &typeID, &format, &declarationData, &name);

	/* Parse the line further based on the type of declaration */
	if (DW_TAG_unknown != tag) {
		Dwarf_Die newDie = NULL;

		/* Search for the die in createdDies before creating a new one */
		die_map::iterator existingDieEntry = createdDies.find(typeID);
		if (createdDies.end() != existingDieEntry) {
			newDie = existingDieEntry->second.second;
		}

		/* Create a new die, if it wasn't previously created */
		if (NULL == newDie) {
			newDie = new Dwarf_Die_s();
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

				/* Insert the DIE into createdDies */
				createdDies[typeID] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

				refNumber = refNumber + 1;

				/* If the current CU die doesn't have a child, make the first new newDie the child */
				if (NULL == Dwarf_CU_Context::_currentCU->_die->_child) {
					Dwarf_CU_Context::_currentCU->_die->_child = newDie;
				}

				/* Set the last created die's sibling to be this die*/
				if (NULL != _lastDie) {
					_lastDie->_sibling = newDie;
					newDie->_previous = _lastDie;
				}
				_lastDie = newDie;
			}
		}
		if (NULL == newDie) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else if (DW_TAG_unknown != tag) {
			/* Do some preliminary parsing of the declarationData */
			str_vect tmp = split(declarationData, '=');
			members = tmp[1];

			/* Populate the dies */
			switch (tag) {
			case DW_TAG_array_type:
				ret = parseArray(members, newDie, error);
				break;
			case DW_TAG_base_type:
				ret = parseBaseType(members, newDie, error);
				break;
			case DW_TAG_class_type:
				ret = parseNewFormat(members, name, newDie, error);
				break;
			case DW_TAG_const_type:
			case DW_TAG_pointer_type:
			case DW_TAG_volatile_type:
			case DW_TAG_subroutine_type:
				ret = parseNamelessReference(members, newDie, error);
				break;
			case DW_TAG_enumeration_type:
				ret = parseEnum(members, name, newDie, error);
				break;
			case DW_TAG_typedef:
				/* Note: AIX label is the same as DWARF typedef */
				ret = parseTypeDef(members, name, newDie, error);
				break;
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				if (OLD_FORMAT == format) {
					ret = parseOldFormat(members, name, newDie, error);
				} else if (NEW_FORMAT == format) {
					ret = parseNewFormat(members, name, newDie, error);
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
	string validData = "";
	string fileName = "";

	/* state machine processes each stage of the input
	 * 0: has not reached the start of a new file yet
	 * 1: reached the start of a new file, parse to get info about the file, populate Dwarf_CU_Context::_currentCU
 	 * 2: inside of a file, parse to get information to populate dies, check if end of file reached
 	 */
	if (0 == _startNewFile) {
		size_t indexOfDebug = data.find(START_OF_FILE[0]);
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + START_OF_FILE[0].length());
			size_t indexOfNumber  = dataAfterDebug.find(START_OF_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + START_OF_FILE[1].length());
				size_t indexOfFile = dataAfterNumber.find(START_OF_FILE[2]);
				if (string::npos != indexOfFile) {
					/* Check that there are only whitespaces between the words */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfFile == dataAfterNumber.find_first_not_of(' '))) {
						/* Populate the attribute references and nested classes from the previous file that we parsed */
						populateAttributeReferences();
						populateNestedClasses();

						/* Remove DIEs which have no children, no size, and are not referred to by any other DIE */
						removeUselessStubs();

						/* Clear the unordered_maps we maintained from the last file */
						createdDies.clear();
						nestedClassesToPopulate.clear();
						refsToPopulate.clear();
						_startNewFile = 1;
					}
				}
			}
		}
	} else if (1 == _startNewFile) {
		if (string::npos != data.find(FILE_NAME)) {
			size_t fileNameIndex = data.find(FILE_NAME) + FILE_NAME.size();
			size_t index = data.substr(fileNameIndex).find_first_not_of(' ');

			fileName = strip(data.substr(index+fileNameIndex), '\n');

			/* Check the file has not been parsed through before */
			if (filesAdded.end() == filesAdded.find(fileName)) {
				/* Add the file to the list of files */
				Dwarf_CU_Context::_fileList.push_back(fileName);

				filesAdded.insert(fileName);

				/* Create one compilation unit context per file */
				Dwarf_CU_Context *newCU = new Dwarf_CU_Context();

				/* Create a DIE for the CU */
				Dwarf_Die newDie = new Dwarf_Die_s();

				/* Create name attribute for the DIE */
				Dwarf_Attribute name = new Dwarf_Attribute_s();

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
					name->_stringdata = strdup(Dwarf_CU_Context::_fileList.back().c_str());
					name->_refdata = 0;
					name->_ref = NULL;


					if (NULL == Dwarf_CU_Context::_firstCU) {
						Dwarf_CU_Context::_firstCU = newCU;
						Dwarf_CU_Context::_currentCU = newCU;
					} else {
						Dwarf_CU_Context::_currentCU->_nextCU = newCU;
						Dwarf_CU_Context::_currentCU = newCU;
					}
					_startNewFile = 2;
				}
			} else {
				/* The file was already parsed through previously, perhaps in another .so file */
				/* Skip parsing this file */
				_startNewFile = 0;
			}
		}
	} else if (2 == _startNewFile) {
		size_t indexOfDebug = data.find(DECL_FILE[0]);
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + DECL_FILE[0].length());
			size_t indexOfNumber = dataAfterDebug.find(DECL_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + DECL_FILE[1].length());
				size_t indexOfDecl = dataAfterNumber.find(DECL_FILE[2]);
				if (string::npos != indexOfDecl) {
					/* Check that there are only whitespaces between the words */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfDecl == dataAfterNumber.find_first_not_of(' '))) {
						/* Parse the data line to populate a die */
						string dataAfterDecl = dataAfterNumber.substr(indexOfDecl + DECL_FILE[2].length());
						size_t index = dataAfterDecl.find_first_not_of(' ');
						validData.assign(strip(dataAfterDecl.substr(index), '\n'));
						ret = parseStabstringDeclarationIntoDwarfDie(validData, error);
						_startNewFile = 3;
					}
				}
			} else {
				/* Check if it is the declaration for the start of another file, as the previous file may have contained no symbols */
				indexOfNumber = dataAfterDebug.find(START_OF_FILE[1]);
				if (string::npos != indexOfNumber) {
					string dataAfterNumber = dataAfterDebug.substr(indexOfNumber+START_OF_FILE[1].length());
					size_t indexOfFile = dataAfterNumber.find(START_OF_FILE[2]);
					if (string::npos != indexOfFile) {
						/* Check that there are only whitespaces between the words */
						if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfFile == dataAfterNumber.find_first_not_of(' '))) {
							_startNewFile = 1;
						}
					}
				}
			}
		}
	} else if (3 == _startNewFile) {
		/* Stops parsing whenever the end of the decl section is reached, since all the declarations in a file are guaranteed to be in a continuous block */
		size_t indexOfDebug = data.find(DECL_FILE[0]);
		if (string::npos != indexOfDebug) {
			string dataAfterDebug = data.substr(indexOfDebug + DECL_FILE[0].length());
			size_t indexOfNumber = dataAfterDebug.find(DECL_FILE[1]);
			if (string::npos != indexOfNumber) {
				string dataAfterNumber = dataAfterDebug.substr(indexOfNumber + DECL_FILE[1].length());
				size_t indexOfDecl = dataAfterNumber.find(DECL_FILE[2]);
				if (string::npos != indexOfDecl) {
					/* Check that there are only whitespaces between the words */
					if ((indexOfNumber == dataAfterDebug.find_first_not_of(' ')) && (indexOfDecl == dataAfterNumber.find_first_not_of(' '))) {
						/* Parse the data line to populate a die */
						string dataAfterDecl = dataAfterNumber.substr(indexOfDecl + DECL_FILE[2].length());
						size_t index = dataAfterDecl.find_first_not_of(' ');
						validData.assign(strip(dataAfterDecl.substr(index), '\n'));
						ret = parseStabstringDeclarationIntoDwarfDie(validData, error);
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
parseEnum(const string data,
	string dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Parse the declaration line for the required fields */
	string members = stripLeading(data, 'e');
	size_t indexOfFirstNonInteger = members.find_first_not_of("123456790-");
	if (string::npos != indexOfFirstNonInteger) {
		members = stripLeading(members.substr(indexOfFirstNonInteger), ':');
		members = stripTrailing(members, ';');

		/* Check if the enum was generated a name by AIX, and if so, delete the generated name */
		if ((0 == strcmp("__", dieName.substr(0, 2).c_str())) && (string::npos == dieName.substr(2).find_first_not_of("1234567890"))) {
			/* generated names are of the form __(INTEGER) */
			dieName.assign("");
		}
		ret = setDieAttributes(dieName, 4, currentDie, error);

		if (DW_DLV_OK == ret) {
			/* Create new dies for each of the enumerators */
			str_vect enumerators = split(members, ',');

			Dwarf_Die newDie = new Dwarf_Die_s();
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
				for (unsigned int i = 1; i < enumerators.size(); i++) {
					if (DW_DLV_OK == ret) {
						Dwarf_Die tempDie = new Dwarf_Die_s();
						if (NULL == tempDie) {
							ret = DW_DLV_ERROR;
							setError(error, DW_DLE_MAF);
						} else {
							tempDie->_tag = DW_TAG_enumerator;
							tempDie->_parent = currentDie;
							tempDie->_previous = newDie;
							tempDie->_child = NULL;
							tempDie->_context = Dwarf_CU_Context::_currentCU;
							tempDie->_attribute = NULL;

							newDie->_sibling = tempDie;
							newDie = tempDie;
						}
					}
				}
				if (DW_DLV_OK == ret) {
					/* Create name and value attributes and assign them to each die */
					str_vect tmp;
					newDie = currentDie->_child;
					for (str_vect::iterator it = enumerators.begin(); it != enumerators.end(); ++it) {
						tmp = split(*it, ':');

						Dwarf_Attribute enumeratorName = new Dwarf_Attribute_s();
						Dwarf_Attribute enumeratorValue = new Dwarf_Attribute_s();

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
							enumeratorValue->_sdata = strtoll(tmp.back().c_str(), NULL, 16);
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
	}	else {
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
parseFields(const string data, Dwarf_Die currentDie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* field[0] will be the name, field[1] will be field attributes */
	str_vect field = split(data, ':');
	str_vect fieldAttributes = split(field[1], ',');

	/* Create attributes */
	Dwarf_Attribute name = new Dwarf_Attribute_s();
	Dwarf_Attribute type = new Dwarf_Attribute_s();
	Dwarf_Attribute declFile = new Dwarf_Attribute_s();

	if ((NULL == name) || (NULL == type) || (NULL == declFile)) {
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

		int ID = extractTypeID(fieldAttributes[0],0);
		type->_udata = 0;
		type->_sdata = 0;
		type->_refdata = 0;
		type->_ref = NULL;

		/* push the type into a vector to populate in a second pass */
		refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)ID, (Dwarf_Attribute)type));
		declFile->_type = DW_AT_decl_file;
		declFile->_form = DW_FORM_udata;
		declFile->_nextAttr = NULL;
		declFile->_sdata = 0;
		/* The declaration file number cannot be zero, as DwarfScanner subtracts by one to get the index to the filename string */
		declFile->_udata = Dwarf_CU_Context::_fileList.size();
		declFile->_refdata = 0;
		declFile->_ref = NULL;

		/* Do a preliminary check if the field requires a bitSize or not */
		bool bitSizeNeeded = true;
		if (0 > ID) {
			if (toInt(fieldAttributes[2]) == BUILT_IN_TYPE_SIZES[ID * -1]) {
				/* If the type is built-in and sizes match, then bitSize is not needed */
				bitSizeNeeded = false;
			}
		}
		if (bitSizeNeeded) {
			Dwarf_Attribute bitSize = new Dwarf_Attribute_s();
			declFile->_nextAttr = bitSize;

			/* Set the bit size */
			bitSize->_type = DW_AT_bit_size;
			bitSize->_form = DW_FORM_udata;
			bitSize->_nextAttr = NULL;
			bitSize->_sdata = 0;
			bitSize->_udata = toInt(fieldAttributes[2]);
			bitSize->_refdata = 0;
			bitSize->_ref = NULL;

			bitFieldsToCheck.push_back(currentDie);
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
parseOldFormat(const string data,
	const string dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* Get rid of the first index of data, as it specifies only if declaration is for struct or union */
	string unparsedMembers = data.substr(1);
	size_t indexOfFirstNonInteger = unparsedMembers.find_first_not_of("1234567890");
	if (string::npos != indexOfFirstNonInteger) {
		int size = toInt(unparsedMembers.substr(0, indexOfFirstNonInteger));
		unparsedMembers = unparsedMembers.substr(indexOfFirstNonInteger);

		/* Set DIE attributes */
		ret = setDieAttributes(dieName, size, currentDie, error);

		if (DW_DLV_OK == ret) {
			/* Get all the members of the structure */
			str_vect members = split(unparsedMembers, ';');

			/* Get the last child of the die */
			Dwarf_Die lastChild = currentDie->_child;
			if (NULL != lastChild) {
				while (NULL != lastChild->_sibling) {
					lastChild = lastChild->_sibling;
				}
			}

			for (unsigned int i = 0; i < members.size(); i++) {
				if (DW_DLV_OK == ret) {
					Dwarf_Die newDie = new Dwarf_Die_s();
					if (NULL == newDie) {
						ret = DW_DLV_ERROR;
						setError(error, DW_DLE_MAF);
					} else {
						newDie->_tag = DW_TAG_member;
						newDie->_parent = currentDie;
						newDie->_sibling = NULL;
						newDie->_previous = NULL;
						newDie->_child = NULL;
						newDie->_context = Dwarf_CU_Context::_currentCU;
						newDie->_attribute = NULL;

						ret = parseFields(members[i], newDie, error);

						if (DW_DLV_OK == ret) {
							/* Set the member as a child of the parent die */
							if (NULL == lastChild) {
								currentDie->_child = newDie;
							} else {
								lastChild->_sibling = newDie;
								newDie->_previous = lastChild;
							}
							lastChild = newDie;
						} else {
							/* Delete the die to make sure there's no memory leak */
							deleteDie(newDie);
						}
					}
				}
			}
		}
	}	else {
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
parseNewFormat(const string data,
	const string dieName,
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

			/* If there is a parent class, create a DW_TAG_inheritance DIE. The numbers after the colon denote the ID of the parent class this class inherits from */
			size_t indexOfColon = sizeTypeAndInheritance.find(':');
			if (string::npos != indexOfColon) {
				/* A parent class exists */
				int parentClassID = toInt(stripLeading(sizeTypeAndInheritance.substr(indexOfColon), ':'));

				/* Create a Dwarf Die to record the inheritance */
				Dwarf_Die inheritanceDie = new Dwarf_Die_s();
				Dwarf_Attribute type = new Dwarf_Attribute_s();

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

					/* push the type into a vector to populate in a second pass */
					refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)parentClassID, (Dwarf_Attribute)type));

					if (NULL == currentDie->_child) {
					currentDie->_child = inheritanceDie;
					} else {
						Dwarf_Die lastChild = currentDie->_child;
						while (NULL != lastChild) {
							lastChild = lastChild->_sibling;
						}
					}
				}
			}

			if (DW_DLV_OK == ret) {
			/* Set DIE attributes */
			ret = setDieAttributes(dieName, size, currentDie, error);
			}

			if (DW_DLV_OK == ret) {
				/* Parse the members */
				str_vect members = split(unparsedMembers, ';');

				Dwarf_Die lastChild = currentDie->_child;
				if (NULL != lastChild) {
					while (NULL !=lastChild->_sibling) {
						lastChild = lastChild->_sibling;
					}
				}
				for (unsigned int i = 0; i < members.size(); i++) {
					if (DW_DLV_OK == ret) {
						Dwarf_Die newDie = new Dwarf_Die_s();
						if (NULL == newDie) {
							ret = DW_DLV_ERROR;
							setError(error, DW_DLE_MAF);
						} else {
							newDie->_tag = DW_TAG_member;
							newDie->_parent = currentDie;
							newDie->_sibling = NULL;
							newDie->_previous = NULL;
							newDie->_child = NULL;
							newDie->_context = Dwarf_CU_Context::_currentCU;
							newDie->_attribute = NULL;
							if (2 <= members[i].size()) {
								/* Note: currently doesn't account for ACCESS_SPEC GEN_SPEC, but it's only dropping compiler generated fields so maybe it's ok */
								if (':' == members[i].at(1)) {
									/* Check to see if valid data member */
									if (!isCompilerGenerated(members[i])) {
										size_t indexOfFirstColon  = members[i].find_first_of(':');
										if (string::npos != indexOfFirstColon) {
											ret = parseFields(members[i].substr(indexOfFirstColon+1), newDie, error);
											if (DW_DLV_OK == ret) {
												/* Set the member as a child of the parent die */
												if (NULL == lastChild) {
													currentDie->_child = newDie;
												} else {
													lastChild->_sibling = newDie;
													newDie->_previous = lastChild;
												}
												lastChild = newDie;
											} else {
												/* To prevent a memory leak, delete the new die */
												deleteDie(newDie);
											}
										} else {
											/* It's not a valid data member */
											deleteDie(newDie);
										}
									} else {
										/* If it's compiler generated, don't add the member */
										deleteDie(newDie);
									}
								} else if ('N' == members[i].at(1)) {
									/* Check to see if valid nested class */
									/* Get the ID of the nested class, which will be in tmp[1] */
									str_vect tmp = split(members[i], 'N');
									int ID = toInt(tmp[1]);
									nestedClassesToPopulate.push_back(make_pair<int, Dwarf_Die>((int)ID, (Dwarf_Die)currentDie));

									/* To prevent a memory leak, delete the new die */
									deleteDie(newDie);
								} else {
									/* delete the new die, to prevent a memory leak */
									deleteDie(newDie);
								}
							}
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
parseTypeDef(const string data,
	const string dieName,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	int ID = 0;

	/* Set attributes for name, type, declaring file */
	Dwarf_Attribute type = new Dwarf_Attribute_s();
	Dwarf_Attribute name  = new Dwarf_Attribute_s();
	Dwarf_Attribute declFile = new Dwarf_Attribute_s();
	Dwarf_Attribute declLine = new Dwarf_Attribute_s();

	if ((NULL == type) || (NULL == name) || (NULL == declFile) || (NULL == declLine)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		type->_type = DW_AT_type;
		type->_form = DW_FORM_ref1;
		type->_nextAttr = name;

		/* AIX classifies int64_t and uint64_t as intptr_t (IDATA) and uintptr_t (UDATA) respectively */
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

		/* Add type to the list of refs to populate on a second pass */
		refsToPopulate.push_back(make_pair<int, Dwarf_Attribute>((int)ID, (Dwarf_Attribute)type));
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
		/* DwarfScanner uses declLine+declFile to determine Type uniqueness */
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
		if (!Dwarf_CU_Context::_fileList.empty()) {
			/* The declaration file number cannot be zero, as DwarfScanner subtracts by one to get the index to the filename string */
			declFile->_udata = Dwarf_CU_Context::_fileList.size();
		} else {
			/* If there is no declaring file, delete declFile and declLine */
			name->_nextAttr = NULL;
			delete(declFile);
			delete(declLine);
		}

		currentDie->_attribute = type;
	}
	return ret;
}

/**
 * Looks up and populates the reference attributes which were added while populating Dwarf DIEs
 */
static void
populateAttributeReferences()
{
	/* Iterate through the vector containing all attributes to be populated*/
	for (unsigned int i = 0; i < refsToPopulate.size(); i++) {
		Dwarf_Attribute tmp = refsToPopulate[i].second;

		/* If the reference is to a built-in type, search the unordered_map containing built-in types */
		if (0 > refsToPopulate[i].first) {
			/* Search for the reference in the die map */
			die_map::iterator existingDieEntry = builtInDies.find(refsToPopulate[i].first);
			if (builtInDies.end() != existingDieEntry) {
				tmp->_ref = existingDieEntry->second.second;

				/* Set _refdata to the correct value and insert the reference into refMap*/
				tmp->_refdata = existingDieEntry->second.first;
				Dwarf_Die_s::refMap.insert(make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)existingDieEntry->second.first, (Dwarf_Die)existingDieEntry->second.second));
			}
		} else {
			/* Search for the reference in the die map */
			die_map::iterator existingDieEntry = createdDies.find(refsToPopulate[i].first);
			if (createdDies.end() != existingDieEntry) {
				tmp->_ref = existingDieEntry->second.second;

				/* Set _refdata to the correct value and insert the reference into refMap*/
				tmp->_refdata = existingDieEntry->second.first;
				Dwarf_Die_s::refMap.insert(make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)existingDieEntry->second.first, (Dwarf_Die)existingDieEntry->second.second));
			}
		}
	}
}

/**
 * Creates DIEs for built-in types
 */
static int
populateBuiltInTypeDies(Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	Dwarf_Die previousDie = NULL;

	/* Populate the base types */
	for (unsigned int i = 1; i <= NUM_BUILT_IN_TYPES; i++) {
		if ((DW_DLV_OK == ret) && (!BUILT_IN_TYPES[i].empty())) {
			Dwarf_Die newDie = new Dwarf_Die_s();
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

				Dwarf_Attribute name = new Dwarf_Attribute_s();
				Dwarf_Attribute size = new Dwarf_Attribute_s();

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

					/* Divide size in bits by 8bits/byte to get size in bytes */
					size->_udata = BUILT_IN_TYPE_SIZES[i] / 8;
					size->_refdata = 0;
					size->_ref = NULL;

					newDie->_attribute = name;
					if (NULL == _builtInTypeDie) {
						_builtInTypeDie = newDie;
					}
					if (NULL !=  previousDie) {
						previousDie->_sibling = newDie;
						newDie->_previous = previousDie;
					}
					previousDie = newDie;

					/* Insert the newly created DIE into the unordered map for use later on */
					builtInDies[i * -1] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

					/* Increment the refNumber */
					refNumber = refNumber + 1;
				}
			}
		}
	}
	/* For redundant types, create typedefs that refer to the base types created above */
	for (unsigned int i = 0; i <= NUM_BUILT_IN_TYPES; i++) {
		if ((DW_DLV_OK == ret) && (!BUILT_IN_TYPES_THAT_ARE_TYPEDEFS[i].empty())) {
			Dwarf_Die newDie = new Dwarf_Die_s();
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
					/* Add to the linked list of DIEs */
					if (NULL !=  previousDie) {
						previousDie->_sibling = newDie;
						newDie->_previous = previousDie;
					}
					previousDie = newDie;

					/* Insert the typedef into the unordered map for use later on */
					builtInDies[i * -1] = make_pair<Dwarf_Off, Dwarf_Die>((Dwarf_Off)refNumber, (Dwarf_Die)newDie);

					/* Increment the refNumber */
					refNumber = refNumber + 1;
				} else {
					/* Delete to prevent a memory leak */
					delete(newDie);
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
	/* Iterate through all the nested classes found */
	for (unsigned int i = 0; i < nestedClassesToPopulate.size(); i++) {
		Dwarf_Die parent = nestedClassesToPopulate[i].second;
		Dwarf_Die nestedClass = NULL;
		if (NULL != parent) {
			die_map::iterator existingDieEntry = createdDies.find(nestedClassesToPopulate[i].first);
			if (createdDies.end() != existingDieEntry) {
				nestedClass = existingDieEntry->second.second;
				if (NULL != nestedClass) {
					/* Get the last child of the die */
					Dwarf_Die lastChild = parent->_child;
					if (NULL != lastChild) {
						while (NULL != lastChild->_sibling) {
							lastChild = lastChild->_sibling;
						}
					}

					/* In case the current parent of nestedClass has nestedClass as the child, set it to be the sibling of nestedClass */
					if (nestedClass == nestedClass->_parent->_child) {
						nestedClass->_parent->_child = nestedClass->_sibling;
					}

					/* Modify the linked list of DIEs to remove the nested class */
					if (NULL != nestedClass->_previous) {
						nestedClass->_previous->_sibling = nestedClass->_sibling;
					}
					if (NULL != nestedClass->_sibling) {
						nestedClass->_sibling->_previous = nestedClass->_previous;
					}
					nestedClass->_sibling = NULL;
					nestedClass->_previous = NULL;

					/* Set the nested class DIE to be a child of the parent */
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
			/* For now, only get rid of empty struct/union/class DIEs */
			if ((DW_TAG_class_type == dieToCheck->_tag) || (DW_TAG_union_type == dieToCheck->_tag) || (DW_TAG_structure_type == dieToCheck->_tag)) {
				if ((Dwarf_CU_Context::_currentCU->_die == dieToCheck->_parent) && (NULL == dieToCheck->_child)) {
					/* If the DIE has a size of zero, has no children, and is not being referred to by any other DIE, it's useless */
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
						/* Remove the DIE and delete it */
						if (dieToCheck->_parent->_child == dieToCheck) {
							dieToCheck->_parent->_child = dieToCheck->_sibling;
						}
						if (NULL != dieToCheck->_previous) {
							dieToCheck->_previous->_sibling = dieToCheck->_sibling;
						}
						if (NULL != dieToCheck->_sibling) {
							dieToCheck->_sibling->_previous = dieToCheck->_previous;
						}
						delete(dieToCheck);
					}
				}
			}
		}

		/* Have to increment iterator this way or it won't compile on AIX */
		it++;
	}
}

/**
 * Sets the name, size and declaring file attributes for a DIE that is passed in
 * param[in] dieName: The name of the entity
 * param[in] dieSize: The size of the entity (in bytes)
 * param[out] currentDie: the DIE to populate with the attributes.
 */
static int
setDieAttributes(const string dieName,
	const unsigned int dieSize,
	Dwarf_Die currentDie,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* If the die already had attributes, update them */
	if (NULL != currentDie->_attribute) {
		Dwarf_Attribute tmp = currentDie->_attribute;
		while (NULL != tmp) {
			switch (tmp->_type) {
			case DW_AT_byte_size:
				tmp->_udata = dieSize;
				break;
			case DW_AT_decl_file:
				/* The declaration file number cannot be zero, as DwarfScanner subtracts by one to get the index to the filename string */
				tmp->_udata = Dwarf_CU_Context::_fileList.size();
				break;
			}
			tmp = tmp->_nextAttr;
		}
	} else {
		/* Create attributes for name, size, declaring file. */
		Dwarf_Attribute name  = new Dwarf_Attribute_s();
		Dwarf_Attribute size = new Dwarf_Attribute_s();
		Dwarf_Attribute declFile = new Dwarf_Attribute_s();
		Dwarf_Attribute declLine = new Dwarf_Attribute_s();

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
			/* The declaration file number cannot be zero, as DwarfScanner subtracts by one to get the index to the filename string */
			declFile->_udata = Dwarf_CU_Context::_fileList.size();
			declFile->_refdata = 0;
			declFile->_ref = NULL;

			declLine->_type = DW_AT_decl_line;
			declLine->_nextAttr = NULL;
			declLine->_form = DW_FORM_udata;
			declLine->_sdata = 0;
			/* DwarfScanner uses declLine+declFile to determine Type uniqueness */
			declLine->_udata = declarationLine;
			declLine->_refdata = 0;
			declLine->_ref = NULL;
			currentDie->_attribute = name;

			/* Increment the number of the declaration line */
			declarationLine = declarationLine + 1;
		}
	}
	return ret;
}

/**
 * Splits the string s on delimeters in argument delim and writes the output to
 * elements.
 *
 * @param[in]  data	  : string that needs to be split
 * @param[in]  delimeters: what the string needs to be seperated on
 * @param[out] elements  : string vector to write out the strings after doing the split to
 */
void
split(const string data, char delimeters, str_vect *elements)
{
	stringstream ss;
	ss.str(data);
	string item;
	while (getline(ss, item, delimeters)) {
		if (!item.empty()) {
			elements->push_back(item);
		}
	}
}

/**
 * Splits the string data on delimeters in argument delimeters and writes the output to
 * elements.
 *
 * @param[in]  data	  : string that needs to be split
 * @param[in]  delimeters: what the string needs to be seperated on
 *
 * @return string vector containing the strings after resulting from the split operation
 */
str_vect
split(const string data, char delimeters)
{
	str_vect elements;
	split(data, delimeters, &elements);
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
strip(const string str, const char chr)
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
stripLeading(const string str, const char chr)
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
stripTrailing(const string str, const char chr)
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
 * Converts value to 0.0value
 *
 * @param[in] value: double value that you want to move the decimal point for
 *
 * @return double with decimal point shifted left by length of the argument plus 1
 */
double
shiftDecimal(const double value)
{
	double ret = value;
	if (ret >= 1) {
		ret = shiftDecimal(value/100);
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
toDouble(const string value)
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
toInt(const string value)
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
toSize(const string size)
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
toString(const int value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}
