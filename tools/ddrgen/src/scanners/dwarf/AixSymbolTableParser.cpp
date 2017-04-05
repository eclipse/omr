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
#include "AixSymbolTableParser.hpp"


AixSymbolTableParser::AixSymbolTableParser()
	: _fileCounter(0), _startNewFile(0), _fileID(0)
{
}

/**
 * Parses any inheritance (base classes) that have been used.
 *
 * @param[in]  data		   : EX: u0:11,u8:13
 * @param[out] classOptions: options vector to write out the class options
 *
 * @return: Options vector will always have, the ID of the inherited class
 *				followed by it's offset and accessibility
 *			EX: "inheritance","11"
 *				"offset","0"
 *				"accessibility","public"
 *				"inheritance","13"
 *				"offset","8"
 *				"accessibility","public"
 */
DDR_RC
AixSymbolTableParser::additionalClassOptions(const string data, const double fileID, options_vect *classOptions)
{
	DDR_RC ret = DDR_RC_ERROR;

	regex options_expression("("+OPT_BASE_SPEC+")+");
	regex option_attrs(BASE_CLASS_OFFSET+":"+CLASS_TYPEID);

	smatch m;

	string options_string = "";
	str_vect options;

	string tmp = "";
	str_vect tmpVect;

	string tmpName = "";
	string tmpAttrValue = "";

	/* STEPS:
	 * 1. Split data on ","
	 * 2. Split each vector from step 1 on ":"
	 *	  NOTE: Due to the second regex match, [0] will only
	 *			 contain the offset index.
	 *		[0] split vector from step 2 :
	 *			- will contain the offset
	 *		[1] split vector from step 2 :
	 *			- will contain the inherited class ID
	 */
	if (!data.empty() && (regex_search(data, m, options_expression))) {
		options_string = m[0]; /* get the options */
		options = split(options_string, ','); /* If there are multiple Options */

		for (str_vect::iterator it = options.begin(); it != options.end(); ++it) {
			if (regex_search(*it, m, option_attrs)) {
				tmp = m[0]; /* get index:classID */
				tmpVect = split(tmp, ':');

				classOptions->push_back(*(makeOptionInfoPair("inheritance", NUMERIC, "", fileID + extractTypeID(tmpVect[1], 0))));
				classOptions->push_back(*(makeOptionInfoPair("offset", NUMERIC, "", extractTypeID(tmpVect[0]))));

				tmp = m.prefix(); /* get the virtual and or access spec */
				for (int index = 0; index < tmp.length(); index++) {
					switch (tmp[index]) {
					case 'v' :
						tmpName = "virtuality";
						tmpAttrValue = "virtual";
						break;
					case 'i' :
						tmpName = "accessibility";
						tmpAttrValue = ACCESS_TYPES[0];
						break;
					case 'o' :
						tmpName = "accessibility";
						tmpAttrValue = ACCESS_TYPES[1];
						break;
					case 'u' :
						tmpName = "accessibility";
						tmpAttrValue = ACCESS_TYPES[2];
						break;
					}
					classOptions->push_back(*(makeOptionInfoPair(tmpName, ATTR_VALUE, tmpAttrValue, 0)));
				}
			}
		}
		ret = DDR_RC_OK;
	}
	return ret;
}

/**
 * Maps the access spec to their corresponding full names
 *
 * @param[in] data : should be of format (i|o|u)(.*)
 * @param[in] index: index to check for the type of access spec, default value is 0
 *
 * @return correspoinding full name value to the access spec as string
 */
string
AixSymbolTableParser::extractAccessSpec(const string data, const int index)
{
	string ret = "";

	if (!data.empty()) {
		switch (data[index]) {
		case 'i':
			ret = ACCESS_TYPES[0]; /* private */
			break;
		case 'o':
			ret = ACCESS_TYPES[1]; /* protected */
			break;
		case 'u':
			ret = ACCESS_TYPES[2]; /* public */
			break;
		}
	}

	return ret;
}

/**
 * Checks if the class declration of type union, struct or class
 *
 * @param[in] data: EX: Y32s < Would return structure
 *					EX: Y32u < Would return union
 *					EX: Y32c < Would return class
 *
 * @return string indicating if the data passed in defines a strucutre, union or a class
 */
string
AixSymbolTableParser::extractClassType(const string data)
{
	string ret = DECLARATION_TYPES[0];

	if (!data.empty()) {
		regex classStructure("Y"+NUM_BYTES+"s(.*)");
		regex classUnion("Y"+NUM_BYTES+"u(.*)");
		smatch m;

		ret = DECLARATION_TYPES[5];
		if (regex_search(data, m, classStructure)) {
			ret = DECLARATION_TYPES[8];
		} else if (regex_search(data, m, classUnion)) {
			ret = DECLARATION_TYPES[9];
		}
	}

	return ret;
}

/**
 * Parses the string to file ID. Extracts the integer betwen the square brackets.
 *
 * @param[in] data: string from which to extract ID from. The expected format is "[####]"
 *
 * @return file ID as double
 */
double
AixSymbolTableParser::extractFileID(const string data)
{
	stringstream ss;
	double ret = 0;
	ss << data.substr(1, data.length() - 1);
	ss >> ret;
	return ret;
}

/**
 * Extracts the typeID from the name string that's passed in
 *
 * @param[in] data: string that needs to be parsed for typeID EX: (T|t) INTEGER
 *
 * @return typeID as double
 */
double
AixSymbolTableParser::extractTypeDefID(const string data, const int index)
{
	return extractTypeID(data, index);
}

/**
 * Extracts the typeID from the name string that's passed in
 *
 * @param data: string that needs to be parsed for typeID EX: (T|t) INTEGER
 *
 * @return typeID as double
 */
double
AixSymbolTableParser::extractTypeID(const string data, const int index)
{
	stringstream ss;
	double ret = 0;
	ss << data.substr(index);
	ss >> ret;
	return ret;
}

/**
 * Looks for entry with the insertionKey
 *
 * @param[in] insertionKey: insertion key of the item to be looked up
 *
 * @return iterator pointing to the entry or parsedData.end() if entry is not found
 */
data_map::iterator
AixSymbolTableParser::findEntry(const double insertionKey)
{
	return parsedData.find(insertionKey);
}

/**
 * Generates typeID for a child based on it's parent ID and child offset
 *
 * @param[in] parentID   : ID of the parent
 * @param[in] childOffset: index number of the child, EX: if this is the first child it will be 1
 *
 * @return typeID for the child as double of format parentID.childOffset
 */
double
AixSymbolTableParser::generateChildTypeID(const double parentID, const double childOffset)
{
	return parentID + shiftDecimal(childOffset);
}

/**
 * Generates insertion key by offseting the typID by that of the parent
 *
 * @param[in] parentID: ID of the parent
 * @param[in] typeID  : ID of the item we need to compute a insertion key for
 *
 * @return insertion key as double
 */
double
AixSymbolTableParser::generateInsertionKey(const double parentID, const double typeID)
{
	return (parentID != 0 && typeID != 0) ? (parentID + typeID) : 0;
}

/**
 * Maps the integer typeID to the corresponding string description for integer typeID passed in the argument
 *
 * @param[in] typeID: ID for which string description is required. SHOULD BE NEGATIVE
 *
 * @return string description from built_type_descriptors array
 */
string
AixSymbolTableParser::getBuiltInTypeDesc(const int typeID)
 {
	string ret = "";

	if (29 > -typeID) {
		if (20 >= -typeID) {
			ret = built_type_descriptors[-typeID - 1];
		} else {
			ret = built_type_descriptors[-typeID - 1 - 4];
		}
	}

	return ret;
 }

/**
 * Maps the typeID to it's size
 *
 * @param[in] ID: typeID to retrieve size for. SHOULD BE NEGATIVE
 *
 * @return corresponding size to the typeID, as int
 */
int
AixSymbolTableParser::getBuiltInTypeSize(const int typeID)
{
	int ret = 0;

	if (29 > -typeID) {
		if (20 >= -typeID) {
			ret = built_type_descriptors_size[-typeID - 1];
		} else {
			ret = built_type_descriptors_size[-typeID - 1 - 4];
		}
	}

	return ret;
}

/**
 * Writes key of the next file in the map to fileID
 *
 * @param[out] fileID: key of the next file in the map
 *
 * @return DDR_RC_OK if not at the end of the _fileList, DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::getNextFile(double *fileID)
{
	DDR_RC ret = DDR_RC_ERROR;

	if (_fileCounter < _fileList.size()) {
		*fileID = _fileList[_fileCounter];
		_fileCounter += 1;
		ret = DDR_RC_OK;
	}

	return ret;
}

/**
 * Concatenates the declaration file ID and name. The reason this function was
 * created is in case someone wanted to change the format of declaration file
 * it will be easier to just modify this function instead of modifying every
 * assignment to declFile
 *
 * @param[in] insertionKey: insertion key of the file
 *
 * @return returns insertion key of the file
 */
double
AixSymbolTableParser::getDeclFile(const double insertionKey)
{
	/* OLD FORMAT
	 * toString(insertionKey) + "--" + getEntryName(insertionKey)
	 */
	return insertionKey;
}

/**
 * Gets the name of the entry with the insertion key passed as argument
 *
 * @param[in] insertionKey: insertion key for the entry in question
 *
 * @returns name of the entry as string
 */
string
AixSymbolTableParser::getEntryName(const double insertionKey)
{
	return (findEntry(insertionKey))->second.name;
}

/**
 * Uses regex expressions to match the declaration type of the data string.
 *
 * @param[in] data: the string to be parsed.
 *
 * @return Type of declaration and writes out the name, type and member information to match_results
 * 			The declaration type can be of type def, constant, structure, union, class, enum or label
 */
string
AixSymbolTableParser::getInfoType(const string data, info_type_results *matchResults)
{
	string ret = DECLARATION_TYPES[0];
	if (!data.empty()) {

		/*								TYPE_DEF
		 * - INTEGER
		 * - c TYPE_ID; ...................................................... < Complex type TYPE_ID
		 * - d TYPE_ID ....................................................... < File of type TYPE_ID
		 * - e EnumSpec; ..................................................... < Enumerated type (default size, 32 bits)
		 * - g TYPE_ID; #NUM_BITS ............................................ < Floating-point type of size NumBits
		 * - D TYPE_ID; #NUM_BITS ............................................ < Decimal floating-point type of size NumBits
		 * - k TYPE_ID ....................................................... < C++ constatnt type
		 * - m OPT_VBASE_SPEC OPT_MULTI_BASE_SPEC TYPE_ID : TYPE_ID : TYPE_ID; < C++ pointer to member type; first INTEGER is member type; second INTEGER class type
		 * - w TYPE_ID ....................................................... < Wide character
		 * - M TYPE_ID; # BOUND .............................................. < Multiple instnace type of TYPE_ID with lengh indicated by BOUND (maybe needed)
		 * - * TYPE_ID ....................................................... < Pointer of type TYPE_ID
		 * - & TYPE_ID ....................................................... < C++ reference type
		 * - V TYPE_ID ....................................................... < C++ volatile type
		 * - Z ............................................................... < C++ ellipses parameter type (ignore)
		 *								Array Descriptions
		 * - a TYPE_ID; # TYPE_ID ............................................ < Array; First TypeID is index type
		 * - 	(INTEGER | c | d | e | g | D | k | m | w | M | \* | \& | V | Z | f | a )
		 *								Misc
		 * - f ............................................................... < function return type
		 */
		regex typeDefExpression ("(^):t"+INTEGER+"="+"(c|d|e|g|D|k|m|w|M|\\*|\\&|V|Z|f|a)?(.*)");

		/* Constant
		 *	NAME : CLASS
		 *			c = CONSTANTS
		 *				- b ORD_VALUE
		 *				- c ORD_VALUE
		 *				- i INTEGER
		 *				- r REAL
		 *				- s STRING
		 *				- C REAL, REAL
		 *				- S TYPE_ID, NUM_ELEMENTS, NUM_BITS, BIT_PATTERN
		 */
		regex constantExpression ("(^)"+NAME+":c="+"(b|c|i|r|s|C|S)?;(.*)");

		regex structureExpression ("=s"+NUM_BYTES);

		regex unionExpression ("=u"+NUM_BYTES);
		regex classExpression ("=Y"+NUM_BYTES+"(?:"+CLASS_KEY+")+(.*)(\\()");
		regex enumExpression ("=e"+INTEGER);
		regex labelExpression ("(^)"+NAME+":t(-)?[0-9]+="+INTEGER+"($)");

		smatch m;

		/* Useful information:
		 * - TYPE_DEF:
		 *		- EX: :t INTEGER = (TYPE_DEF) INTEGER
		 *			Name   : :t INTEGER
		 *			Type   : (TYPE_DEF) INTEGER
		 *			Members: none
		 * - TYPE_CONSTANT
		 *			Name   : NAME : c
		 *			Type   : CONSTANT (attributes)
		 *			Members: none
		 * - TYPE_STRUCTURE
		 *			Name   : NAME : t INTEGER
		 *			Type   : s NUM_BYTES
		 *			Members: NAME : TYPE_ID, BIT_OFFSET, NUM_BITS;NAME : TYPE_ID, BIT_OFFSET, NUM_BITS;
		 * - TYPE_UNION
		 *			Name   : NAME : t INTEGER
		 *			Type   : u NUM_BYTES
		 *			Members: NAME : TYPE_ID, BIT_OFFSET, NUM_BITS
		 * - TYPE_CLASS
		 *			Name   : NAME : T INTEGER
		 *			Type   : Y NUM_BYTES CLASS_KEY (.*)
		 *			Members: members
		 * - TYPE_ENUM
		 *			Name   : NAME : t INTEGER
		 *			Type   : e INTEGER
		 *			Members: members
		 * - TYPE_LABEL
		 *			Name   : NAME : t INTEGER
		 *			Type   : (TYPE_DEF) INTEGER
		 *			Members: none
		 */

		if (regex_search(data, m, typeDefExpression)) {

			ret = DECLARATION_TYPES[1]; /* type Def */

			str_vect tmp = split(data,'=');
			matchResults->name = tmp[0];
			matchResults->type = tmp[1];

		} else if (regex_search(data, m, constantExpression)) {

			ret = DECLARATION_TYPES[2]; /* constant */

			str_vect tmp = split(data,'=');
			matchResults->name = tmp[0];
			matchResults->type = tmp[1];

		} else if (regex_search(data, m, structureExpression)) {

			ret = DECLARATION_TYPES[3]; /* structure */

			matchResults->name = m.prefix();
			matchResults->type = m[0];
			matchResults->members = m.suffix();

			matchResults->type = stripLeading(matchResults->type, '=');
			matchResults->members = striptrailing(matchResults->members, ';');

		} else if (regex_search(data, m, unionExpression)) {

			ret = DECLARATION_TYPES[4]; /* union */

			matchResults->name = m.prefix();
			matchResults->type = m[0];
			matchResults->members = m.suffix();

			matchResults->type = stripLeading(matchResults->type, '=');
			matchResults->members = striptrailing(matchResults->members, ';');

		} else if (regex_search(data, m, classExpression)) {

			ret = DECLARATION_TYPES[5]; /* class */

			matchResults->name = m.prefix();
			matchResults->type = m[0];
			matchResults->members = m.suffix();

			matchResults->type = stripLeading(matchResults->type, '=');
			matchResults->type = striptrailing(matchResults->type, '(');
			matchResults->members = striptrailing(matchResults->members, ';');

		} else if (regex_search(data, m, enumExpression)) {

			ret = DECLARATION_TYPES[6]; /* enum */

			matchResults->name = m.prefix();
			matchResults->type = m[0];
			matchResults->members = m.suffix();

			matchResults->type = stripLeading(matchResults->type, '=');
			matchResults->members = stripLeading(matchResults->members, ':');
			matchResults->members = striptrailing(matchResults->members, ';');

		} else if (regex_search(data, m, labelExpression)) {

			ret = DECLARATION_TYPES[7]; /* label */

			str_vect tmp = split(data,'=');

			matchResults->name = tmp[0];
			matchResults->type = tmp[1];
		}
	}

	return ret;
}

/**
 * Uses the first index of the argument data to check if it is static,
 * virtual table pointer, virtual base pointer, or virtual base self pointer.
 *
 * @param[in] data: data member attribute string. Input is string, with MemberAttr
 * 					spec at the start, index 0.
 *
 * @return string representation of the attribute option
 */
string
AixSymbolTableParser::getMemberAttrsType(const string data)
{
	string ret = MEMBER_ATTRS[0];

	if (!data.empty()) {
		switch (data[0]){
		case 's':
			ret = MEMBER_ATTRS[1]; /* isStatic */
			break;
		case 'p':
			ret = MEMBER_ATTRS[2]; /* isVtblPtr */
			break;
		case 'b':
			ret = MEMBER_ATTRS[3]; /* vbase_pointer */
			break;
		case 'r':
			ret = MEMBER_ATTRS[4]; /* vbase_self-pointer */
			break;
		}
	}

	return ret;
}

/**
 * Checks if the declaration is for a built in type EX: int, uint ...
 *
 * @param[in]  data: string that need to be checked if it is built in or not
 * @param[out] ID  : pointer to the output variable
 *
 * @return boolean indicating if the declaration is for built in types
 * 			and the typeID of the declaration as int
 */
bool
AixSymbolTableParser::isBuiltIn(const string data, double *ID)
{
	bool ret = FALSE;

	regex expression ("(-)[0-9]+"); /* MUST HAVE THE NEGATIVE SIGN */
	smatch m;

	if (regex_search(data, m, expression)) {
		string raw_typeID(m[0]);
		*ID = extractTypeDefID(raw_typeID, (string (m[0])).find_first_of('-')); /* Retains the "-" */
		ret = TRUE;
	} else {
		*ID = 0;
		ret = FALSE;
	}

	return ret;
}

/**
 * Checks if the ID is that of a built in type
 *
 * @param[in] ID: id as integer EX: -3 < would return true
 *
 * @return TRUE if the arguemt is of type builtin (less than 0) and FALSE otherwise
 */
bool
AixSymbolTableParser::isBuiltIn(const int ID)
{
	return 0 > ID;
}

/**
 * Checks the first index of the field declaration string for compiler generated tag, GenSpec.
 *	Possible valid formats, that could potentially return TRUE are:
 *		GenSpec AccessSpec AnonSpec DataMember
 *		GenSpec VirtualSpec AccessSpec OptVirtualFuncIndex MemberFunction
 *
 * @param[in] data: EX: cup5__vfp:__vfp:23,0,32;uN222;uN223;vu1[di:__dt__Q3_3std7_LFS_ON10ctype_baseFv:2;; << This would return TRUE
 *
 * @return boolean value indicating if the declaration is compiler generated or not
 */
bool
AixSymbolTableParser::isCompilerGenerated(const string data)
{
	return data.empty() ? FALSE : ('c' == data[0]);
}

/**
 * Checks if the class is being passed by value i.e. if there is a "OptPBV"
 * option in the declaration string. This option will be there for C++ class type,
 * Y NumBytes ClassKey OptPBV OptBaseSpecList.
 *
 * @param[in] data: Should be of format Y[\d]+(c|s|u)(V) EX: Y4cV( << This will return TRUE
 *
 * @return boolean value indicating whether the class is passed by value or not
 */
bool
AixSymbolTableParser::isPassedByValue(const string data)
{
	bool ret = FALSE;

	if (!data.empty()) {
		int found = data.find("V");
		ret = string::npos != found;
	}

	return ret;
}

/**
 * Inserts the object passed in the second argument to the global parsedData map.
 *
 * @param[in] insetionKey: insertion key for the object
 * @param[in] object	 : object to be inserted
 *
 * @return: DDR_RC_OK on success and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::insertEntry(const double insetionKey, const Info object)
{
	DDR_RC ret = DDR_RC_ERROR;
	pair<data_map_itt, bool> result;
	/* Insert function returns an iterator to the entry that was inserted and
	 * a boolean value indicating if the insertion was successful or not
	 */
	result = parsedData.insert(make_pair<double,Info>((double)insetionKey, (Info)object));

	if (result.second) {
		ret = DDR_RC_OK;
	}

	return ret;
}

/**
 * Creates a info object for the file and inserts it in the map
 *
 * @param[in] fileName: name of the file that needs to be inserted in the map
 * @param[in] fileID  : Id of the file that needs to be inserted in the map
 *
 * @return DDR_RC_OK if the insert was successful and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::insertFileEntry(const string fileName, const double fileID)
{
	DDR_RC ret = DDR_RC_ERROR;

	if ((0 < fileID)
		&& (!fileName.empty())
		) {
		Info *tmp = new Info();

		tmp->name = fileName;
		tmp->typeID.ID = fileID;
		tmp->typeID.tag = "file";
		tmp->size.sizeType = SIZE_TYPES[4]; /* num_elements */

		ret = insertEntry(fileID, *tmp);
	}

	return ret;
}

/**
 * This is mainly for error checks. It tags any unwanted declaration as "Rejected"
 * and inserts them in the map. It tags them as "_!!REJECTED!!" if the declaration
 * was already in the map and "_REJECTED" otherwise.
 *
 * @param[in] insertionKey: insertion ID of the object that needs to be marked rejected
 * @param[in] object	  : rejected object that needs to inserted in the map
 *
 * @return DDR_RC_OK on success and DDR_RC_ERROR on error
 */
DDR_RC
AixSymbolTableParser::insertRejectedEntry(const double insertionKey, Info *object)
{
	DDR_RC ret = DDR_RC_ERROR;

	data_map::iterator existing_entry = findEntry(insertionKey);

	if (parsedData.end() != existing_entry) { /* Key already exists in the map */
		object->name += "_!!REJECTED!!";
		updateEntry(existing_entry, *object, TRUE);
	} else {
		object->name += "_REJECTED";
		insertEntry(insertionKey, *object);
		ret = DDR_RC_OK;
	}

	return ret;
}

/**
 * Checks if the type for which ID is passed is signed or not.
 * Returns 1 = Signed, 0 = Unsigned, 2 = Unspecified.
 *
 * @param[in] ID: ID to be matched agains the built in types EX: -1
 *
 * @return integer value indicating if the passed ID is signed or not.
 */
int
AixSymbolTableParser::isSigned(const int ID)
{
	int ret = 2;
	switch (ID) {
	case -1:
	case -3:
	case -4:
	case -6:
	case -15:
	case -27:
	case -28:
	case -29:
	case -31:
	case -34:
		ret = 1; break; /* Signed */
	case -5:
	case -7:
	case -8:
	case -9:
	case -10:
	case -17:
	case -20:
	case -32:
	case -33:
		ret = 0; break; /* Unsigned */
	case -12:
	case -13:
	case -14:
	case -16:
	case -18:
	case -25:
	case -26:
	case -30:
		ret = 2; break; /* Documentation doesn't specify */
	}
	return ret;
}

/**
 * Checks if the declaration string passed in is that of a valid data member.
 * If it is valid, it writes out the parsed info to dataMember.
 *
 * @param[in]  data	   : string should of format, "GenSpec AccessSpec AnonSpec DataMember"
 * @param[out] data_member: Object to store the parsed info in
 *
 * @return TRUE if it is a valid data member and FALSE otherwise
 */
bool
AixSymbolTableParser::isValidDataMember(const string data, Info *dataMember)
{
	bool ret = FALSE;

	/* GenSpec AccessSpec AnonSpec DataMember
	 *	c?		(i|u|o){1}	a?		( (s?) | (p INTEGER{1} NAME)? | (b|r)? ) : Field;
	 *	c? << SHOULDN'T BE PICKING UP ANYTHING WITH THIS ON
	 *			(i|u|o){1}
	 *						a?
	 *								(s?)
	 *								|(p INTEGER{1} NAME)?
	 *								|(b|r)?
	 *														: NAME
	 *																: [\d]+ , [\d]+ , [\d];
	 */
	regex expression ("^"+ACCESS_SPEC+"{1}a?((s?)|(p"+INTEGER+NAME+")?|(b|r)?):"+NAME+":"+INTEGER+","+BIT_OFFSET+","+NUM_BITS+"$");
	smatch m;

	/* Steps:
	 *	1. Get GenSpec, AccessSpec and AnonSpec
	 *		- data.substr(0,indexOfFirstColon)
	 *	2. If the data member is not compiler generated use isValidField to
	 *	   parse the data member name, typeID, bit offset and size
	 */
	if (regex_search(data, m, expression)) {
		int indexOfFirstColon = data.find_first_of(':');
		string attributes = data.substr(0, indexOfFirstColon);
		int index = 0; /* used to the next character index to check in the string */

		if ('c' != attributes[index]) { /* Proceed if the data member is not generated by compiler */
			options_vect memberOptions;
			memberOptions.push_back(*(makeOptionInfoPair("accessibility", ATTR_VALUE, extractAccessSpec(attributes),0)));
			index+=1; /* next we'll look at index 1 to check for anon spec */
			if ('a' == attributes[index]) {
				memberOptions.push_back(*(makeOptionInfoPair("anon_spec", ATTR_VALUE, "anonymous",0)));
				index+=1; /* if anon spec found the member attrs will start from index 2 */
			}

			string member_attr_type(getMemberAttrsType(attributes.substr(index)));
			if (MEMBER_ATTRS[0] != member_attr_type) {
				memberOptions.push_back(*(makeOptionInfoPair ("member_attr", ATTR_VALUE, member_attr_type,0)));
				if ("isVtblPtr" == member_attr_type) { /* get pointer integer and name */
					parseVtblPointerInfo(member_attr_type, &memberOptions);
				}
			}
			dataMember->options = memberOptions;
			ret = isValidField(data.substr(indexOfFirstColon + 1), dataMember);
		}
	}

	return ret;
}

/**
 * Checks if the data string passed in is of valid field format. If it is
 * parses out all the needed info and populates appropriate fields with it in field
 *
 * @param[in]  data	: should be of format "NAME : TYPE_ID, BIT_OFFSET, NUM_BITS"
 * @param[in]  parentID: type id of the parent
 * @param[in]  fieldID : id of the field (in most cases counter representing the number of fields before it plus 1)
 * @param[out] field   : object to write in the parsed info regarding the field
 *
 * @return TRUE if it is a valid field and FALSE otherwise
 */
bool
AixSymbolTableParser::isValidField(const string data, Info *field)
{
	regex expression (NAME+":"+INTEGER+","+BIT_OFFSET+","+NUM_BITS);
	smatch m;
	bool ret = FALSE;

	/* Steps:
	 *	1. Split data on ":"
	 *		- Split vector [0]
	 *			- Name of the field
	 *		- Split vector [1]
	 *			- Split on ","
	 *				- Split vector [0]
	 *					- field type
	 *				- Split vector [1]
	 *					- bit offset
	 *				- Split vector [2]
	 *					- size of the field
	 */
	if (regex_search(data, m, expression)) {

		str_vect field_info = split(data, ':'); /* Sample: numElements:-1,0,32 */
		field->name = field_info[0];
		str_vect field_attrs = split(field_info[1], ',');

		field->typeID.tag = "field";
		field->typeID.isChild = 1;

		if (!isBuiltIn(field_attrs[0], &(field->typeDef.typeID))) {
			/* getTypeID by default extracts ID starting from index 1, however,
			 * in this case since there's no prefix we want to start from index 0
			 */
			field->typeDef.typeID =  extractTypeID(field_attrs[0], 0);
		} else {
			field->typeDef.isBuiltIn = TRUE;
			field->typeDef.isSigned = isSigned(field->typeDef.typeID);
		}

		field->size.sizeValue = toSize(field_attrs[2]);
		field->size.sizeType = SIZE_TYPES[1];

		options_vect field_options;
		field_options.push_back(*(makeOptionInfoPair("bit_offset", NUMERIC, "", toDouble(field_attrs[1]))));

		/* In order to prevent overwriting any existing data, we use end range insertion.
		 * This will ensure that new data is always inserted at the end of the vector.
		 */
		field->options.insert(field->options.end(), field_options.begin(), field_options.end());
		field->rawData = data;

		ret = TRUE;
	}

	return ret;
}

/**
 * Checks if the data string passed in is of valid friend class format. If it is
 * parses out all the needed info and populates appropriate fields with it in friendClass
 *
 * @param[in]  data	   : should be of format "AnonSpec FriendClass"
 * @param[out] friendClass: object to write in the parsed info regarding the friend class
 *
 * @return TRUE if it is a valid friend class and FALSE otherwise
 */
bool
AixSymbolTableParser::isValidFriendClass(const string data, Info *friendClass)
{
	bool ret = FALSE;

	/* AnonSpec FriendClass#
	 * a?	   \\( TYPE_ID;
	 *
	 *	a?
	 * 		\\(
	 *			TYPE_ID;
	 */
	regex expression("a?\\("+NON_NEG_INTEGER);
	smatch m;

	/*Steps:
	 *	1. Split on "("
	 *		- Split vector [0]
	 * 			- Anon Spec
	 *		- Split vector [1]
	 *			- ID of the member
	 */
	if (regex_search(data, m, expression)) {
		str_vect tmp = split(data, '(');

		if ('a' == (tmp[0])[0]) {
			friendClass->options.push_back(*(makeOptionInfoPair("anon_spec", ATTR_VALUE, "anonymous",0)));
		}

		friendClass->typeID.tag = "friend_class_field";
		friendClass->typeID.isChild = 1;
		friendClass->typeDef.typeID =  extractTypeID(tmp[1],0);

		ret = TRUE;
	}

	return ret;
}

/**
 * Checks if the data string passed in is of valid nested class format.
 * If it is, writes out the parsed info to nestedClass.
 *
 * @param[in]  data	   : should be of format, "AccessSpec AnonSpec NestedClass"
 * @param[out] nestedClass: object to write in the parsed info regarding the nested class
 *
 * @return TRUE if it is a valid nested class and FALSE otherwise
 */
bool
AixSymbolTableParser::isValidNestedClass(const string data, Info *nestedClass)
{
	bool ret = FALSE;

	/* AccessSpec AnonSpec NestedClass#
	 * (i|u|o){1}   a?	   NTYPE_ID  ;
	 *
	 * (i|u|o){1}
	 * 				a?
	 * 						N
	 * 							TYPE_ID
	 */
	regex expression (ACCESS_SPEC+"{1}"+ANON_SPEC+"?N"+INTEGER+"$");
	/*Steps:
	 *	1. Check index 0 for access spec, call extractAccessSpec()
	 *	2. Check index 1 for anon spec
	 *	3. Get typeID, call getTypeID
	 *		- if there is a anonSpec we'll need to extract string starting from index 2
	 *		- else extract string
	 */
	smatch m;
	int next_index = 1;

	if (regex_search(data, m, expression)) {
		nestedClass->options.push_back(*(makeOptionInfoPair("accessibility", ATTR_VALUE, extractAccessSpec(data),0)));

		if ('a' == data[next_index]) {
			nestedClass->options.push_back(*(makeOptionInfoPair("anon_spec", ATTR_VALUE, "anonymous",0)));
			next_index++;
		}

		nestedClass->typeDef.typeID = extractTypeID(data.substr(next_index));
		nestedClass->typeID.tag = "nested_class_field";
		nestedClass->typeID.isChild = 1;

		ret = TRUE;
	}

	return ret;
}

/**
 * Creates an object of type option_info using the arguments and
 * returns it.
 *
 * @param[in] nameValue	 : name of the option you want to add
 * @param[in] typeValue	 : type of the value the option will have, ID or attribute
 * @param[in] attributeValue: option value of type string
 * @param[in] idValue	   : option value of type int
 *
 * @return makeOptionInfoPair populated with data from the arguments
 */
option_info*
AixSymbolTableParser::makeOptionInfoPair (const string nameValue, const option_info_type typeValue,
								 const string attributeValue, const int idValue)
{
	option_info *option = new option_info();

	option->name = nameValue;
	option->type = typeValue;

	switch (typeValue) {
	case NUMERIC:
		option->ID = idValue;
		break;
	case ATTR_VALUE:
		option->attrValue = attributeValue;
		break;
	default:
		option->attrValue = "NOTHING";
		break;
	}

	return option;
}


/**
 * Parses the intput data as array declaration. If the string is of valid
 * array declaration then writes the parsed information to arrayEntry. Only
 * picking up "a" at the moment.
 *
 * @param[in]  data	  : string containting array declaration
 * @param[out] arrayEntry: object to store the parsed info in
 *
 * @return DDR_RC_OK if data was a valid array declaration and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseArray(const string data, Info *arrayEntry)
{
	/*						   Array Descriptions
	 * - a TYPE_ID; # TYPE_ID ........ < Array; First TypeID is index type or of type ar INTEGER; INTEGER; INTEGER; INTEGER
	 * - A TYPE_ID ................... < Open Array
	 * - D INTEGER, TYPE_ID .......... < N-dimensional dynamic array of TypeID
	 * - E INTEGER, TYPE_ID .......... < N-dimensional dynamic subarray of TypeID
	 * - O INTEGER, TYPE_ID .......... < New open array
	 * - P INTEGER; # TYPE_ID ........ < Packed array
	 * -	 (a | A | D | E | O | P)
	 */
	DDR_RC ret = DDR_RC_OK;

	regex expression ("a(r"+INTEGER+"(.*)|"+INTEGER+");"+INTEGER);
	smatch m;

	if (regex_search(data, m, expression)) {
		arrayEntry->typeDef.type = AIX_TYPE_array;
		arrayEntry->typeDef.typeID = extractTypeDefID(data, data.find_last_of(';') + 1); /* type of array will always be at the end, after the last ":" */
		if (isBuiltIn(arrayEntry->typeDef.typeID)) {
			arrayEntry->typeDef.isBuiltIn = TRUE;
			arrayEntry->typeDef.isSigned = isSigned(arrayEntry->typeDef.typeID);
		}

		str_vect arrayInfo = split(data, ';');
		/* arrayInfo (subrange):
		 * - [0]: r[\d]+ subrange type EX: int, char ...
		 * - [1]: [\d]+  lower bound
		 * - [2]: [\d]+  upper bound
		 */
		if ('r' == data[1]) { /* index of subrage */
			/* To get the subrange type we extract it starting from index 2
			 * of the first index in arrayInfo vector because array
			 * declaration with subrange will be of
			 * arIndexID;lowerBound;UpperBound;ArrayType
			 */
			arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("subrange_type", NUMERIC, "", extractTypeDefID(arrayInfo[0],2))));

			/* There are two types of bounds that we are concerned with, INTEGER and INDETERMINABLE */
			ret = DDR_RC_OK; /* Set return to ok, it will be changed to ERROR if subrange format is not valid */
			string boundType;
			boundType = parseBound(arrayInfo[1]); /* Get bound type for lower bound */
			if ("indeterminable" == boundType) {
				arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("lower_bound", ATTR_VALUE, boundType, 0)));
			} else if ("integer" == boundType) {
				arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("lower_bound", NUMERIC, "", toInt(arrayInfo[1]))));
			} else {
				ret = DDR_RC_ERROR;
			}

			boundType = parseBound(arrayInfo[2]); /* Get bound type for upper bound */
			if ("indeterminable" == boundType) {
				arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("upper_bound", ATTR_VALUE, boundType, 0)));
			} else if ("integer" == boundType) {
				arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("upper_bound", NUMERIC, "", toInt(arrayInfo[2]))));
			} else {
				ret = DDR_RC_ERROR;
			}
		} else {
			/* Array declaration is of type "aTYPEID;TYPEID"
			 * in order to get the array index typeID we need
			 * the first type ID. That will at index 0 in arrayInfo
			 */
			arrayEntry->typeDef.options.push_back(*(makeOptionInfoPair("index_type", NUMERIC, "", extractTypeDefID(arrayInfo[0]))));
			ret = DDR_RC_OK;
		}
	}

	return ret;
}

/**
 * Returns type of bound
 *
 * @param[in] data: bound declaration string
 *
 * @return bound type based on the data string
 */
string
AixSymbolTableParser::parseBound(const string data)
{
	string ret = "INVALID";

	if ("J" == data) {
		ret = "indeterminable";
	} else if ('0' <= data[0] && '9' >= data[0]) {
		ret = "integer";
	}

	return ret;
}

/**
 * Takes in string will information about the members and parses them into individual enteries
 *
 * @param[in] data		 : string to be parsed for class members
 * @param[in] classID	  : insertion key of the class
 * @param[in] fileID	   : ID (would be the insertion key) of the file the declaation is from
 * @param[out] classMembers: vector of type double, used to return insertion keys of the members
 *
 * @return vector of type double with insertion keys of the class members
 */
DDR_RC
AixSymbolTableParser::parseClassMembers(const string data, const double classID, const double fileID, double_vect *classMembers)
{
	DDR_RC ret = DDR_RC_ERROR;

	if (!data.empty() && 0 < classID && 0 < fileID) {

		/* Steps
		 * 1. Split vector on ";". This will seperate each member
		 * 2. Check each seperated member agains valid data member
		 *	, nested class and friend class syntax
		 * 3. If it matches each of them, then get the typeID. Store
		 *	the raw data and the declaration file
		 */
		str_vect preParsedMembers = split(data, ';');

		double insertion_key;
		int memberInsertionCounter = 1;

		for (str_vect::iterator itt = preParsedMembers.begin(); itt != preParsedMembers.end(); ++itt) {
			if (!(*itt).empty() && *itt != "\n") {

				/* Checks:
				 * 1. Is valid datamember
				 * 2. Is valid nested class
				 * 3. Is valid friend class
				 */

				Info *entry = new Info();

				if (isValidDataMember(*itt, entry)) {

					entry->typeID.ID = generateChildTypeID(classID, memberInsertionCounter);

					entry->rawData = *itt;
					entry->declFile = getDeclFile(fileID);

					insertion_key = generateInsertionKey(fileID, entry->typeID.ID);
					ret = insertEntry(insertion_key, *entry);

				} else if (isValidNestedClass(*itt, entry)) {

					entry->typeID.ID = generateChildTypeID(classID, memberInsertionCounter);

					entry->rawData = *itt;
					entry->declFile = getDeclFile(fileID);

					insertion_key = generateInsertionKey(fileID, entry->typeID.ID);
					ret = insertEntry(insertion_key, *entry);

				} else if (isValidFriendClass(*itt, entry)) {

					entry->typeID.ID = generateChildTypeID(classID, memberInsertionCounter);

					entry->rawData = *itt;
					entry->declFile = getDeclFile(fileID);

					insertion_key = generateInsertionKey(fileID, entry->typeID.ID);
					ret = insertEntry(insertion_key, *entry);
				}

				/* Insert the insertion key for the member into the member vector
				 * if it was successfully inserted in the map
				 * There's a check for insertion_key because insertion key varaiable
				 * will be updated to a valid valie, fileID + SOMETHING, if the member
				 * is one of the above 3 types and that's the only instance
				 */
				if (DDR_RC_OK == ret && fileID < insertion_key) {
					classMembers->push_back(insertion_key);
					memberInsertionCounter++;
					/* Set the insertion_key to 0 to indicate that
					 * the entry has been inserted
					 */
					insertion_key = 0;
				}
			}
		}
	}

	return ret;
}

/*
 * Takes in string will information about the constant declarations and parses it
 *
 * @param[in]  data		  : string containing the declaration EX: b32
 * @param[out] constantEntry: the info object to which to write info to
 *
 * @return DDR_RC_OK if the data string was of correct format and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseConstants(const string data, Info *constantEntry)
{
	/* c = Constants ; < Generic declaration format
	 *						  Constants
	 * - b ORD_VALUE ..................................................... < Boolean constant
	 * - c ORD_VALUE ..................................................... < Character constant
	 * - i INTEGER ....................................................... < Integer constant
	 * - r Real .......................................................... < Decimal or binary floating point constant
	 * - s String ........................................................ < String constant
	 * - C REAL, REAL .................................................... < Complex constant
	 * - S TYPE_ID, NUM_ELEMENTS, NUM_BITS, BIT_PATTEREN ................. < Set constant
	 */

	/* Steps:
	 * 1. Store the type of constant in typeDef type
	 * 2. Push other information related to the declaration to options
	 *
	 * NOTES: This function updates,
	 *			- typeDef.type
	 *			- typeDef.ID
	 *			- typeDef.options
	 *			- options
	 */
	DDR_RC ret = DDR_RC_ERROR;
	str_vect constant_entry_info;

	switch (data[0]) {
	case 'b':
		constantEntry->typeDef.type = AIX_TYPE_boolean;
		constantEntry->options.push_back(*(makeOptionInfoPair("ord_value", NUMERIC, "", toDouble(data.substr(1)))));
		ret = DDR_RC_OK;
		break;
	case 'c':
		constantEntry->typeDef.type = AIX_TYPE_character;
		constantEntry->options.push_back(*(makeOptionInfoPair( "ord_value", NUMERIC, "", toDouble(data.substr(1)))));
		ret = DDR_RC_OK;
		break;
	case 'i':
		constantEntry->typeDef.type = AIX_TYPE_integer;
		constantEntry->options.push_back(*(makeOptionInfoPair( "integer", NUMERIC, "", toDouble(data.substr(1)))));
		ret = DDR_RC_OK;
		break;
	case 'r':
		constantEntry->typeDef.type = AIX_TYPE_decimal;
		constantEntry->options.push_back(*(makeOptionInfoPair("real", ATTR_VALUE, data.substr(1),0)));
		ret = DDR_RC_OK;
		break;
	case 's':
		constantEntry->typeDef.type = AIX_TYPE_string;
		constantEntry->options.push_back(*(makeOptionInfoPair("string", ATTR_VALUE, data.substr(1),0)));
		ret = DDR_RC_OK;
		break;
	case 'C':
		constantEntry->typeDef.type = AIX_TYPE_complex;
		constant_entry_info = split(data, ',');
		constantEntry->options.push_back(*(makeOptionInfoPair("real", ATTR_VALUE, (constant_entry_info[0]).substr(1),0)));
		constantEntry->options.push_back(*(makeOptionInfoPair("real", ATTR_VALUE, constant_entry_info[1],0)));
		ret = DDR_RC_OK;
		break;
	case 'S':
		constantEntry->typeDef.type = AIX_TYPE_set;
		constant_entry_info = split(data, ',');
		constantEntry->typeDef.typeID = extractTypeDefID(constant_entry_info[0]);
		constantEntry->typeDef.options.push_back(*(makeOptionInfoPair("num_elements", NUMERIC, "", toInt(constant_entry_info[1]))));
		constantEntry->typeDef.options.push_back(*(makeOptionInfoPair("num_bits", NUMERIC, "", toInt(constant_entry_info[2]))));
		constantEntry->typeDef.options.push_back(*(makeOptionInfoPair("bit_pattern", NUMERIC, "", toInt(constant_entry_info[3]))));
		ret = DDR_RC_OK;
		break;
	}

	return ret;
}

/**
 * Parses out members from the string, inserts them into the data map and
 * returns their insertion key
 *
 * @param[in]  data	 : the members to be parsed EX: test:-1,0,32;
 * @param[in]  parentID : typeID of the parent
 * @param[in]  fileID   : insertion key of the declaration file
 * @param[out] memberIDs: to retrun insertion keys of valid fields, if any
 *
 * @return DDR_RC_OK if the data string was of correct format and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseFields(const string data, const double parentID, const double file_insertion_key, double_vect *memberIDs)
{
	DDR_RC ret = DDR_RC_ERROR;

	if (!data.empty()) {

		regex expression (NAME+":"+INTEGER+","+BIT_OFFSET+","+NUM_BITS);
		smatch m;

		/*Steps:
		 * 1. Split on ";"
		 * 2. Split vector [1]
		 * 		- get the name
		 * 3. Split vector [2]
		 * 		- Split on ","
		 *			- Split vector [0]
		 *				- ID of the member
		 *			- Split vector [1]
		 *				- options attribute
		 *			- Split vector [2]
		 *				- size of the member
		 */

		str_vect tmp = split(data, ';');
		double insertion_key = 0;

		for (int index = 1; index <= tmp.size(); index++) {

			Info *field = new Info();
			if (isValidField(tmp[index - 1], field)) {

				field->typeID.ID = generateChildTypeID(parentID, index);
				insertion_key = generateInsertionKey(file_insertion_key, field->typeID.ID);
				if (DDR_RC_OK == insertEntry(insertion_key, *field)) {
					memberIDs->push_back(insertion_key); /* Field is valid, insert it's insertion key */
					ret = DDR_RC_OK;
				}

			}
		}
	}
	return ret;
}

/**
 * parseVtblPointerInfo
 *
 * @param[in]  data	   : EX: p32sample
 * @param[out] pointerInfo: options vector to return the parsed information
 *
 * @return DDR_RC_OK if the data string was of correct format and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseVtblPointerInfo(const string data, options_vect *pointerInfo)
{
	DDR_RC ret = DDR_RC_ERROR;

	if (!data.empty()) {
		regex expression ("p"+INTEGER);
		smatch m;

		string tmp;

		if (regex_search(data, m, expression)) {
			/* - if the data string is of the correct format
			 * the integer value will be followed by the name.
			 * regex_search will places the integer in m[0]
			 * we will need to strip out the first index, letter p
			 * before we can extract the integer part.
			 * - Name will be in m.suffix()
			 */
			tmp = m[0];
			pointerInfo->push_back(*(makeOptionInfoPair("VtblPtr_INTEGER", NUMERIC,"",toDouble(tmp.substr(1)))));
			pointerInfo->push_back(*(makeOptionInfoPair("VtblPtr_NAME", ATTR_VALUE,  m.suffix(), 0)));
			ret = DDR_RC_OK;
		}
	}
	return ret;
}

/**
 * Parses the enumerator Data and returns insertion ID for each valid enumerator
 *
 * @param[in]  enumeratorData: enumerator data to be parsed (EX: E1:2,E2:3,E3:4)
 * @param[in]  enumID		: typeID of the enum (NOT THE INSETION KEY)
 * @param[in]  fileID		: typeID of the file the enum was declared in (also the insertion key)
 * @param[out] enumeratorIDs : used to return insertion keys of valid enumerators
 *
 * @return DDR_RC_OK if the data string was of correct format and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseEnumData(const string enumeratorData, const double enumID, const double fileID, double_vect *enumeratorIDs)
{

	DDR_RC ret = DDR_RC_ERROR;
	if (!enumeratorData.empty() && 0 < enumID && 0 < fileID) {

		str_vect enum_list = split(enumeratorData, ',');
		str_vect tmp;

		double enumeratorInsertionKey = 0;
		/* We use indexCounter because the symbol table doesn't provide an
		 * index to order the enumerators in.
		 * Since, the enumerator insertion key will be of format enumID.indexCounter
		 * we can't start from 0 because that will overlap with the enumID
		 */
		double indexCounter = 1;

		/* Steps
		 * 1. Split the enumeratorData string on ",". This will sepeate each enumerator.
		 * 2. Iteratoring over each vector index, split the string on ":".
		 *	  This will seperate the enumerator name and value.
		 * 3. - Get the name from first(0th) index of the second split vector, tmp.
		 *	- Use the indexCounter and enumID to generate child type ID.
		 *	  - Set the typeID isChild tag to true and typeID tag to ENUMERATOR
		 *	  - Push the enumerator value in the options vector
		 *	  - Get the declaration file using the fileID
		 *	  - Store the raw data for the enumerator
		 */
		for (str_vect::iterator iter = enum_list.begin(); iter != enum_list.end(); ++iter) {
			if ((";") != *iter) {
				tmp = split (*iter, ':');
				if (!tmp.empty() && tmp[0] != "\n") {
					Info *enum_list_item = new Info();
					enum_list_item->name = tmp[0];
					enum_list_item->typeID.ID = generateChildTypeID (enumID, indexCounter);
					enum_list_item->typeID.tag = "ENUMERATOR";
					enum_list_item->typeID.isChild = 1;
					enum_list_item->options.push_back(*(makeOptionInfoPair("enumerator_value", NUMERIC, "", toDouble(tmp[1]))));
					enum_list_item->declFile = getDeclFile(fileID);
					enum_list_item->rawData = *iter;

					enumeratorInsertionKey = generateInsertionKey(fileID, enum_list_item->typeID.ID);

					ret = insertEntry(enumeratorInsertionKey, *enum_list_item);
					if (DDR_RC_OK == ret) {
						enumeratorIDs->push_back(enumeratorInsertionKey);
						indexCounter++; /* used as ID for the enumerator */
					}
				}
			}
		}
	}

	return ret;
}

/**
 * Takes in raw data from the file and determins all the needed parts of it
 *
 * @param[in]  rawData : string containing the data to be parsed
 * @param[in]  fileID  : insertion key of the file the rawData is from
 * @param[out] memberID: returns a member ID of the parsed object
 *
 * @return DDR_RC_OK
 *		   - along with non zero member ID if the data string was of correct format
 *		   - along with zero for member ID if it has already been aded to the parsedData map
 *		   DDR_RC_ERROR
 *		   - along with non zero member ID if the data was of invalid format
 *			 and an entry for it already exists in the parsedData map
 *		   - along with zero for member ID if the data was of invalid format
 *			 and it hasn't been added to the parsedData map
 */
DDR_RC
AixSymbolTableParser::parseInfo(const string rawData,const double fileID, double *memberID)
{
	DDR_RC ret = DDR_RC_ERROR;
	if (!rawData.empty() && 0 < fileID) {
		Info *declData = new Info();

		info_type_results *matchResults = new info_type_results();
		string info_type_result = getInfoType(rawData, matchResults);

		str_vect tmp = split(matchResults->name, ':'); /* has the name and the typeID */

		/* Steps
		 * 1. Get the type of Info. This will also parse the
		 *	rawData into 3 main sections, name, type and members.
		 * 2. Split matchResults->name on ":". This will get us
		 *	  the name and typeID of the Info.
		 *		- Since this a not a member of any other declaratio,
		 *		  we'll set the isChild to false
		 * 3. Store the file it was declared in and it's rawData string
		 * 4. Parse further based on type of declaration
		 */
		declData->name = tmp[0];
		declData->typeID.ID = extractTypeID(tmp[1]);
		declData->typeID.isChild = 0;
		declData->size.sizeType = SIZE_TYPES[2];
		declData->declFile = getDeclFile(fileID);
		declData->rawData = rawData;

		double insertionKey = generateInsertionKey(fileID, declData->typeID.ID);

		if (DECLARATION_TYPES[1] == info_type_result) { /* type def declaration */

			declData->typeID.tag = info_type_result;
			ret = parseTypeDef(matchResults->type, declData);

		} else if (DECLARATION_TYPES[2] == info_type_result) { /* constant declaration */

			declData->typeID.tag = info_type_result;
			ret = parseConstants(matchResults->type, declData);

		} else if (DECLARATION_TYPES[3] == info_type_result) { /* structure declaration */

			declData->typeID.tag = info_type_result;
			declData->size.sizeValue = toSize((matchResults->type).substr(1));
			/* We are only concerned with the ret value of parseFields if there are any members.
			 * This is a safe gaurd for the case when there are no members to be parsed as parseFields
			 * would return DDR_RC_ERROR in that case but we may still want to insert the entry.
			 */
			if (!matchResults->members.empty()) {
				ret = parseFields(matchResults->members, declData->typeID.ID, fileID, &declData->members);
			} else {
				ret = DDR_RC_OK;
			}

		} else if (DECLARATION_TYPES[4] == info_type_result) { /* union declaration */

			declData->typeID.tag = info_type_result;
			declData->size.sizeValue = toSize((matchResults->type).substr(1));
			/* We are only concerned with the ret value of parseFields if there are any members.
			 * This is a safe gaurd for the case when there are no members to be parsed as parseFields
			 * would return DDR_RC_ERROR in that case but we may still want to insert the entry.
			 */
			if (!matchResults->members.empty()) {
				ret = parseFields(matchResults->members, declData->typeID.ID, fileID, &declData->members);
			} else {
				ret = DDR_RC_OK;
			}

		} else if (DECLARATION_TYPES[5] == info_type_result) { /* class declaration */
			int index = 1; /* Use this to keep track of the next string index to check */
			/* 1 (starting index) starts at SIZE
			 * iterates until we are at the classKey
			 * MOTIV: after the class key if there is OptPBV then we don't need to pick it up
			 */
			while (index != (matchResults->type).length()
					&& !isalpha((matchResults->type)[index])) {
				++index;
			}

			/* We don't want to pick up any class with compiler generated fields */
			if (0 == isCompilerGenerated(matchResults->members)) {

				ret = DDR_RC_OK;

				declData->typeID.tag = extractClassType(matchResults->type);
				declData->size.sizeValue = toSize((matchResults->type).substr(1, index - 1));

				if (!(matchResults->members).empty()) {
					ret = parseClassMembers(matchResults->members, declData->typeID.ID, fileID, &(declData->members));
				}

				additionalClassOptions((matchResults->type).substr(index + 1), fileID, &(declData->options));
			}

		} else if (DECLARATION_TYPES[6] == info_type_result) { /* enum declaration */
			declData->typeID.tag = info_type_result;
			ret = parseEnumData(matchResults->members, declData->typeID.ID, fileID, &(declData->members)); /* gets the keys of the members */
			declData->size.sizeValue = (declData->members).size(); /* get the number of enumerators */
			declData->size.sizeType = SIZE_TYPES[4]; /* num_elements */

			/* for anonymous enum
			 * if there is a label entry for the enum type
			 * then update the enum type to anonymousEnum
			 */

		} else if (DECLARATION_TYPES[7] == info_type_result) { /* label declaration */
			declData->typeID.tag = info_type_result;
			ret = DDR_RC_OK;

		} else if (DECLARATION_TYPES[0] == info_type_result) { /* invalid declaration */
			declData->typeID.tag = info_type_result;
		}

		if (DDR_RC_OK == ret) {
			/* WILL EITHER RETURN DDR_RC_OK or DDR_RC_ERROR OR POSITIVE INSERTION KEY VALUE */

			data_map::iterator existing_entry = findEntry(insertionKey);

			if (parsedData.end() != existing_entry) {
				if (existing_entry->second.declFile == declData->declFile) {
					DEBUGPRINTF("ERROR>>>>There was a overlap in insertion keys ('%f')", insertionKey);
				}
				ret = updateEntry(existing_entry, *declData); /* Will either be value of DDR_RC_OK or DDR_RC_ERROR */
				(*memberID) = 0;

			} else {
				ret = insertEntry(insertionKey, *declData);
				if (DDR_RC_OK == ret) {
					(*memberID) = insertionKey; /* Entery doesn't exist in the map, therefore, insert it and return the insertion key for it */
				}
			}
		} else { /* THIS HAS BEEN SET TO FALSE TO AVOID INSERTING REJECTED ENTRIES INTO THE MAP. SET IT TO TRUE WHEN DEBUGGING */
			/* If a entry didn't match any of the above criteria
			 * then it will be inserted as _REJECTED, provided there isn't
			 * a entry with the same name and ID in the map.
			 * If the entry is already in the map, it will be updated to
			 * reflect REJECTED status and it's insertKey will be removed from
			 * file_members vector
			 */
			ret = insertRejectedEntry(insertionKey, declData);
			if (DDR_RC_ERROR == ret && insertionKey > fileID) {
				(*memberID) = insertionKey;
			} else {
				ret = DDR_RC_ERROR; /* This is to indicate that the rawData was neither of types we are looking for */
				(*memberID) = 0;
			}
		}
	}

	return ret;

}

/**
 * parseTypeDef
 *
 * @param data: string of type TYPE_DEF
 *
 * @return : DDR_RC_OK if data was a valid typeDef DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::parseTypeDef(const string data, Info *typeDefEntry)
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
	 *	 -21 logical*1, ........ 8 bit type < FORTRAN
	 *	 -22 logical*2, ........ 16 bit type < FORTRAN
	 *	 -23 logical*4, ........ 32 bit type for boolean variables < FORTRAN
	 *	 -24 logical, .......... 32 bit type < FORTRAN
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

	str_vect tmp;
	DDR_RC ret = DDR_RC_ERROR;
	int typeID_index = 1;
	switch (data[0]) {
		case '-':
			if (!isBuiltIn(data, &(typeDefEntry->typeDef.typeID))) {
				ret = DDR_RC_ERROR;
			}
			ret = DDR_RC_OK;
			break;
		case 'c':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			tmp = split(data, ';');
			typeDefEntry->size.sizeValue = toSize(tmp[1]);
			typeDefEntry->size.sizeType = SIZE_TYPES[1];
			typeDefEntry->typeDef.type = AIX_TYPE_complex;
			ret = DDR_RC_OK;
			break;
		case 'd':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_file;
			ret = DDR_RC_OK;
			break;
		case 'g':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			tmp = split(data, ';');
			typeDefEntry->size.sizeValue = toSize(tmp[1]);
			typeDefEntry->size.sizeType = SIZE_TYPES[1];
			typeDefEntry->typeDef.type = AIX_TYPE_floating_point;
			ret = DDR_RC_OK;
			break;
		case 'D':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			tmp = split(data, ';');
			typeDefEntry->size.sizeValue = toSize(tmp[1]);
			typeDefEntry->size.sizeType = SIZE_TYPES[1];
			typeDefEntry->typeDef.type = AIX_TYPE_decimal_floating_point;
			ret = DDR_RC_OK;
			break;
		case 'k':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_constant;
			ret = DDR_RC_OK;
			break;
		case 'm':
			/* OPT_VBASE_SPEC
 			 * OPT_MULTI_BASE_SPEC
 			 * TYPE_ID < Member type
 			 * TYPE_ID < Class type < if there is no m, will there be no class type ID
 			 * TYPE_ID
			 */
			tmp = split(data, ':');
			typeDefEntry->typeDef.typeID = extractTypeDefID(tmp[2]);
			if ('v' == (tmp[0])[typeID_index]) {
				typeDefEntry->options.push_back(*(makeOptionInfoPair("has_virtual_base", ATTR_VALUE, "v",0)));
				typeID_index++;
			}
			if ('m' == (tmp[0])[typeID_index]) {
				typeDefEntry->options.push_back(*(makeOptionInfoPair("is_multi_based", ATTR_VALUE, "m",0)));
				typeID_index++;
			}
			typeDefEntry->options.push_back(*(makeOptionInfoPair( "member_type", NUMERIC, "", extractTypeID(tmp[0], typeID_index))));
			typeDefEntry->options.push_back(*(makeOptionInfoPair("class_type", NUMERIC, "", toDouble(tmp[1]))));
			typeDefEntry->typeDef.type = AIX_TYPE_ptr_to_member_type;
			ret = DDR_RC_OK;
			break;
		case 'n':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			tmp = split(data, ';');
			typeDefEntry->size.sizeValue = toSize(tmp[1]);
			typeDefEntry->size.sizeType = SIZE_TYPES[2];
			typeDefEntry->typeDef.type = AIX_TYPE_string;
			ret = DDR_RC_OK;
			break;
		case 'w':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_wide_character;
			ret = DDR_RC_OK;
			break;
		case 'M':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			tmp = split(data, ';');
			typeDefEntry->size.sizeValue = toSize(tmp[1]);
			typeDefEntry->size.sizeType = SIZE_TYPES[3];
			typeDefEntry->typeDef.type = AIX_TYPE_multiple_instance;
			ret = DDR_RC_OK;
			break;
		case '*':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_pointer;
			ret = DDR_RC_OK;
			break;
		case '&':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_reference;
			ret = DDR_RC_OK;
			break;
		case 'V':
			typeDefEntry->typeDef.typeID = extractTypeDefID(data);
			typeDefEntry->typeDef.type = AIX_TYPE_volatile;
			ret = DDR_RC_OK;
			break;
		case 'Z':
			typeDefEntry->typeDef.type = AIX_TYPE_ellipses;
			ret = DDR_RC_OK;
			break;
		case 'a':
			parseArray(data, typeDefEntry);
			ret = DDR_RC_OK;
			break;
	}

	if ((0 != typeDefEntry->typeDef.typeID)
		&& (isBuiltIn(typeDefEntry->typeDef.typeID))) {

		typeDefEntry->typeDef.isBuiltIn = 1;
		typeDefEntry->typeDef.isSigned = isSigned(typeDefEntry->typeDef.typeID);
		ret = DDR_RC_OK;
	}

	return ret;

}

/**
 * Gets the raw data and parses it. What you would in a 
 * loop to parse the output from dump comamnd.
 *
 * @param[in] data: raw data from the symbol table
 */
void
AixSymbolTableParser::parseDumpOutput(const string data)
{
	regex fileSection (".*\\s*m.*debug\\s*3*\\s*FILE\\s*");
	regex fileInfoRow ("\\s(a0)\\s*");
	regex sectionStart ("^\\[.*\\].*m.*debug.*decl\\s+");
	regex checkSymbol (".*\\s(m)*\\s*.text\\s*(1)\\s*(unamex)\\s*(\\*\\*No Symbol\\*\\*)");

	smatch m_s;
	string validData = "";
	string fileName = "";
	double member_id = 0;

	if (0 == _startNewFile && regex_search(data, m_s, fileSection)) {
		_startNewFile = 1;
	} else if (1 == _startNewFile) {

		if (regex_search(data, m_s, fileInfoRow)) { /* Get file name */
			fileName = m_s.suffix();
			_fileID = extractFileID(m_s.prefix());

			fileName = strip(fileName, '\n');
			insertFileEntry(fileName, _fileID);

			_fileEntry = findEntry(_fileID);
			_startNewFile = 2;
			DEBUGPRINTF("\n\nFile Name: %s",fileName.c_str());
		}
	} else if (2 == _startNewFile) {
		/* Get the file structure info */

		if (regex_search (data, m_s, sectionStart)) {
			validData.assign (m_s.suffix());

			DDR_RC result = parseInfo(strip(validData, '\n'), _fileID, &member_id);

			if (DDR_RC_OK == result && _fileID < member_id) { /* Valid member, store it's insertion key */
				_fileEntry->second.members.push_back(member_id);
				_fileEntry->second.size.sizeValue += 1;
			} else if (DDR_RC_ERROR == result && _fileID < member_id) { /* Invalid member, must remove it's insertion key */

				double_vect::iterator itt = find(_fileEntry->second.members.begin(), _fileEntry->second.members.end(), member_id);
				if (_fileEntry->second.members.end() != itt) {
					_fileEntry->second.members.erase(itt);
					_fileEntry->second.size.sizeValue -= 1;
				}
			}

		} else if (regex_search (data, m_s, checkSymbol)) {

			_startNewFile = 0;

			if (0 < _fileEntry->second.size.sizeValue) {
				_fileList.push_back(_fileID);
				string output  = beautifyInfo(_fileEntry->second,_fileEntry->first);
				DEBUGPRINTF("\n%s",output.c_str());
			}
		}
	}
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
AixSymbolTableParser::split(const string data, char delimeters, str_vect *elements)
{
	stringstream ss;
	ss.str(data);
	string item;

	while (getline(ss, item, delimeters)) {
		elements->push_back(item);
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
AixSymbolTableParser::split(const string data, char delimeters)
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
AixSymbolTableParser::strip(const string str, const char chr)
{
	string ret = str;
	if (!ret.empty()) {
		ret = stripLeading(ret, chr);
		ret = striptrailing(ret, chr);
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
AixSymbolTableParser::stripLeading(const string str, const char chr)
{
	string ret = str;
	if (!ret.empty()){
		if (chr == str[0]){
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
AixSymbolTableParser::striptrailing(const string str, const char chr)
{
	string ret = str;
	if (!ret.empty()){

		int strLength = str.length();

		if (chr == ret[strLength - 1]){
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
AixSymbolTableParser::shiftDecimal(const double value)
{
	double ret = value;
	if (ret >= 1) {
		ret = shiftDecimal (value/100);
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
AixSymbolTableParser::toDouble(const string value)
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
AixSymbolTableParser::toInt(const string value)
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
AixSymbolTableParser::toSize(const string size)
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
AixSymbolTableParser::toString(const double value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}

/**
 * Maps the value from the argument to it's corresponding string description
 *
 * @param[in] value: value that needs to be mapped to string
 *
 * @return argument value as string
 */
string 
AixSymbolTableParser::getStringDescription(const unsigned short value)
{
	return AIX_TYPES[value];
}

/**
 * Updates the entry already in the map with the new updated values.
 * Only updates - size
 * 				- members
 * 				- options
 * 				- typeDef
 * 				- rawData
 *
 * @param[in] update: Contains the info that needs to be updated
 * @param[in] entry : Old entry from the data map
 *
 * @return DDR_RC_OK on successful update and DDR_RC_ERROR otherwise
 */
DDR_RC
AixSymbolTableParser::updateEntry(data_map::iterator entry, Info update, bool overRideChecks)
{
	DDR_RC ret = DDR_RC_ERROR;

	if (!overRideChecks) {
		if (((0 == entry->second.name.compare(update.name))
				&& (entry->second.typeID.ID == update.typeID.ID)
				&& (entry->second.declFile == update.declFile)
				)
			) {

			entry->second.size = update.size;

			if (!update.members.empty()) {
				entry->second.members = update.members;
			}

			if (!update.options.empty()) {
				entry->second.options = update.options;
			}

			entry->second.typeDef = update.typeDef;
			entry->second.rawData = update.rawData;

			ret = DDR_RC_OK;
		}
	} else {
		entry->second  = update;
		ret = DDR_RC_OK;
	}

	return ret;
}

/**
 * Parses the information in the vector to a format easily readable by the human eye
 *
 * @param[in] data	: the vector of type double that needs to be beautified
 * @param[in] location: an identifier to seperate each call to this function from the others
 * @param[in] level   : indicator of the indentation need, default value is 1
 *
 * @return the vector passed as argument in a human friendly format as a string
 */
string
AixSymbolTableParser::beautifyVector(const vector<double> data, const string location,const int level)
{
	string padding = getPadding(level);
	string output = "";
	string delimeter = " ---- ";
	string outputDelimeter = "------------\n";

	output += padding + outputDelimeter ;
	output += padding + "Location: " + location + "\n";
	for (int i = 0; i < data.size(); i++) {
		output += padding + toString(data[i]) + delimeter;
	}
	output += "\n" + padding + outputDelimeter;

	return output;
}

string
AixSymbolTableParser::beautifyVector(const str_vect data, const string location,const int level)
{
	string padding = getPadding(level);
	string output = "";
	string delimeter = " ---- ";
	string outputDelimeter = "------------\n";

	output += padding + outputDelimeter ;
	output += padding + "Location: " + location + "\n";
	for (int i = 0; i < data.size(); i++) {
		output += padding + data[i].c_str() + delimeter;
	}
	output += "\n" + padding + outputDelimeter;

	return output;
}

/**
 * Returns padding(spaces) based on the level value.
 * Level 1 is zero padding. After that every level is padded by 4 spaces.
 *
 * @param[in] level: integer indicating the level for which padding is required
 *
 * @return string with level appropriate spaces
 */
string
AixSymbolTableParser::getPadding(const int level)
{
	string levelUp = "	";
	string retPadding = "";
	for(int index = 2; index <= level; index++) {
		retPadding += levelUp;
	}

	return retPadding;
}

/**
 * Parses the information in the info struct to a format easily readable by the human eye
 *
 * @param[in] data			 : info struct that needs to be beautified
 * @param[in] key			  : insertion key of the info struct
 * @param[in] overrideChildTag : boolean value indicating if the info struct is a child or not
 * @param[in] level			: indentation level, default value is 1
 *
 * @return the info struct passed as argument in a human friendly format as a string
 */
string
AixSymbolTableParser::beautifyInfo(const Info data, const double key, const bool overrideChildTag, const int level)
{
	string padding = "";
	padding = getPadding(level);
	int subLevel = level + 1;
	string subLevelPadding = getPadding(subLevel);
	int subSubLevel = subLevel + 1;
	string subSubLevelPadding = getPadding(subLevel + 1);

	string output = "";
	string outputDelimeter = "------------\n";

	/* Prints the child record only if there is an override */
	if ((overrideChildTag) || (0 == data.typeID.isChild)) {

		output += padding + outputDelimeter;
		output += padding + "Key  : " + toString(key) + "\n";
		output += padding + "Info : \n";
		output += subLevelPadding + "name	  : " + data.name + "\n";
		output += subLevelPadding + "typeID	  : " + toString(data.typeID.ID) + "--" + data.typeID.tag + "\n";
		output += subLevelPadding + "size	  : " + toString(data.size.sizeValue) + "--" + data.size.sizeType + "\n";

		output += subLevelPadding + "members   : \n";
		/* List the members */
		if ((!data.typeID.isChild)
			&& (!data.members.empty())
			) {
			/* && (data.typeID.tag != "file") ) {
			 * Add this if you want to call printMapContents this will prevent
			 * it from printing all the members of every info. Which will be pointless
			 * because we'll be printing all the contents of the map anyways
			 */

			output += beautifyVector(data.members, "",subLevel);
			for (vector<double>::const_iterator it = data.members.begin(); it != data.members.end(); ++it) {
				data_map::iterator result;
				result = parsedData.find (*it);
				output += beautifyInfo(result->second, result->first, TRUE, subLevel);
			}

		} else {
			output += subLevelPadding + "/*EMPTY*/" + "\n";
		}

		output += subLevelPadding + "options   : \n";
		if (!data.options.empty()) {
			output += beautifyOptionsVector(data.options, "", subLevel);
		} else {
			output += subLevelPadding + "/*EMPTY*/" + "\n";
		}

		output += subLevelPadding + "typeDef  : \n";
		output += subSubLevelPadding + "type	  : " + getStringDescription(data.typeDef.type) + "\n";
		output += subSubLevelPadding + "isBuiltIn : " + toString(data.typeDef.isBuiltIn) + "\n";
		output += subSubLevelPadding + "isSigned  : " + toString(data.typeDef.isSigned) + "\n";
		output += subSubLevelPadding + "typeID	  : " + toString(data.typeDef.typeID) + "\n";

		output += subSubLevelPadding + "options   : \n";
		if (!data.typeDef.options.empty()) {
			output += beautifyOptionsVector(data.typeDef.options, "Options:", subSubLevel);
		} else {
			output += subSubLevelPadding + "/*EMPTY*/" + "\n";
		}

		output += subLevelPadding + "declFile : " + toString(data.declFile) + "\n";
		output += subLevelPadding + "rawData  : " + data.rawData + "\n";
		output += padding + outputDelimeter;
	}
	return output;
}

/**
 * Prints the contents of the map in a human friendly format
 */
void
AixSymbolTableParser::printMapContents()
{
	DEBUGPRINTF("map_dump");

	data_map::iterator it;
	string output = "";

	for (it = parsedData.begin(); it != parsedData.end(); ++it) {
		output = beautifyInfo (it->second, it->first);
		DEBUGPRINTF("%s",output.c_str());
	}
}

/**
 * Parses the information in the regex_search to a format easily readable by the human eye
 *
 * @param[in] m	   : smatch variable that needs to be beautified
 * @param[in] location: an identifier to seperate each call to this function from the others
 *
 * @return the smatch passed as argument in a human friendly format as a string
 */
string
AixSymbolTableParser::beautifyMatchResults(const smatch m, const string location)
{
	string output = "";
	string outputDelimeter = "------------\n";

	output += outputDelimeter;
	output += "Location: " + location;
	output += "--------\n";
	string prefix = m.prefix();
	string middle = m[0];
	string suffix = m.suffix();
	output += "Prefix  : " + prefix + "\n";
	output += "[0]	 : " + middle + "\n";
	output += "Suffix  : " + suffix + "\n" ;
	output += outputDelimeter;

	return output;
}

/**
 * Parses the information in the options vector to a format easily readable by the human eye
 *
 * @param[in] data	: option vector that needs to be beautified
 * @param[in] location: an identifier to seperate each call to this function from the others
 * @param[in] level   : indentation level, default level is 1
 *
 * @return the options_vect passed as argument in a human friendly format as a string
 */
string
AixSymbolTableParser::beautifyOptionsVector(const options_vect data, const string location, const int level)
{
	string padding = getPadding(level);
	string output = "";
	string delimeter = " ---- ";
	string outputDelimeter = "------------\n";

	output += padding + outputDelimeter;
	output += padding + "Location: " + location + "\n";
	for (int i = 0; i < data.size(); i++) {
		output += padding + data[i].name + " = ";
		if (NUMERIC == data[i].type) {
			output += toString(data[i].ID);
		} else if (ATTR_VALUE == data[i].type) {
			output += data[i].attrValue;
		}
		output += delimeter + "\n";
	}
	output += padding + outputDelimeter;

	return output;
}

/**
 * Parses the information in the output from getInfoType to a format easily readable by the human eye
 *
 * @param[in] results : output of getInfoType
 * @param[in] location: an identifier to seperate each call to this function from the otherse
 *
 * @return the info_type_results passed as argument in a human friendly format as a string
 */
string
AixSymbolTableParser::beautifyInfoTypeResults(const info_type_results results, const string location)
{
	string output = "";
	string outputDelimeter = "------------\n";

	output += outputDelimeter;
	output += "Location: " + location + "\n";
	output += "match_results:\n";
	output += "	name   : " + results.name + "\n";
	output += "	type   : " + results.type + "\n";
	output += "	members: " + results.members + "\n";
	output += outputDelimeter;

	return output;
}
