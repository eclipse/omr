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

#include "AixDwarfParser.hpp"

/* Statics to create: */
static unordered_map<double, Dwarf_Die> createdDies;
static Dwarf_Die _lastDie = NULL;

static void deleteDie(Dwarf_Die die);
static int parseDwarfDie(const string data);
static int parseDwarfInfo(const char *line);
static Dwarf_Half parseDeclarationLine(string data, double *typeID);

/* _startNewFile is a state counter */
static int _startNewFile = 0;
static double _fileID = 0;

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
	Dwarf_CU_Context::_fileList.clear();
	Dwarf_CU_Context::_currentCU = NULL;
	Dwarf_CU_Context::_firstCU = NULL;
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
	int ret = DW_DLV_OK;
	FILE *fp = NULL;
	char *buffer = NULL;
	Dwarf_CU_Context::_firstCU = NULL;
	Dwarf_CU_Context::_currentCU = NULL;
	size_t len = 0;

	/* Initialize some values */

	/* Open up the input file. */
	fp = fopen("aixoutput", "r");

	/* Parse through the file */
	while ((-1 != getline(&buffer, &len, fp)) && (DW_DLV_OK == ret)) {
		ret = parseDwarfInfo(buffer);
	}
	free(buffer);

	if (NULL != fp) {
		pclose(fp);
	}

	return ret;
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
 * @return file ID as double
 */
double
extractFileID(const string data)
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
 * @param data: string that needs to be parsed for typeID EX: (T|t) INTEGER
 *
 * @return typeID as double
 */
double
extractTypeID(const string data, const int index)
{
	stringstream ss;
	double ret = 0;
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
 * Parses a line of data and returns the declaration type of the data string.
 *
 * @param[in]  data: the string to be parsed. data should be the portion of the declaration line after "debug     0    decl" and whitespace.
 * @param[out] tag: the type of the declaration of the data string
 * @param[out] typeID: the ID of the entity being declared
 */
static Dwarf_Half
parseDeclarationLine(const string data, double *typeID)
{
	Dwarf_Half ret = DW_TAG_unknown;

	/* split the declaration into the name and the information about the declaration */
	size_t indexOfFirstColon = data.find_first_of(':');

	/* Only proceed if the declaration has a colon in it. Otherwise, the line isn't a valid declaration and shouldn't be used */
	if (string::npos != indexOfFirstColon) {
		string declarationData = stripLeading(data.substr(indexOfFirstColon), ':');
		if ((0 == data.substr(0, indexOfFirstColon).size()) && ('t' == declarationData.at(0))) {
			/* Nameless typedef declaration */
			str_vect tmp = split(declarationData, '=');
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
			default:
				ret = DW_TAG_unknown;
				break;
			}
		} else if ('T' == declarationData.at(0)) {
			size_t indexOfFirstEqualsSign = declarationData.find('=');
			*typeID = extractTypeID(declarationData.substr(0, indexOfFirstEqualsSign));
			string members = stripLeading(declarationData.substr(indexOfFirstEqualsSign), '=');

			if (string::npos != declarationData.find('(')) {
				/* C++ type class/struct/union declaration */
				size_t indexOfFirstParenthesis = members.find('(');
				string sizeAndType = stripLeading(members.substr(0, indexOfFirstParenthesis), 'Y');
				size_t indexOfFirstNonInteger = sizeAndType.find_first_not_of("1234567890");
				members = stripLeading(members.substr(indexOfFirstParenthesis), '(');

				/* Check if the declaration is for a class, strucutre or union */
				if (0 == isCompilerGenerated(members)) {
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
					ret = DW_TAG_unknown;
				}
			} else if ('e' == members[0]) {
				ret = DW_TAG_enumeration_type;
			} else {
				/* C type struct/union declaration */
				if ('s' == members[0]) {
					ret = DW_TAG_structure_type;
				} else if ('u' == members[0]) {
					ret = DW_TAG_union_type;
				}
			}
		} else if ('t' == declarationData.at(0)) {
			/* Typedef declaration */
			ret = DW_TAG_typedef;
			str_vect tmp = split(declarationData, '=');
			*typeID = extractTypeID(tmp[0]);
		}
	}
	return ret;
}

static int
parseDwarfDie(const string line)
{
	int ret = DW_DLV_OK;
	double typeID;
	/* get the type of the data of the line being parsed */
	Dwarf_Half tag = parseDeclarationLine(line, &typeID);

	/* Parse the line further based on the type of declaration */
	if (DW_TAG_unknown != tag) {
		Dwarf_Die newDie = NULL;
		double ID = _fileID + typeID;

		/* Search for the die in the createdDies before creating a new one */
		unordered_map<double, Dwarf_Die>::iterator existingEntry = createdDies.find(ID);

		if (createdDies.end() != existingEntry) {
			newDie = existingEntry->second;
		} else {
			newDie = new Dwarf_Die_s();
			if (NULL == newDie) {
				ret = DW_DLV_ERROR;
			} else {
				newDie->_tag = tag;
				newDie->_parent = Dwarf_CU_Context::_currentCU->_die;
				newDie->_sibling = NULL;
				newDie->_child = NULL;
				newDie->_context = Dwarf_CU_Context::_currentCU;
				newDie->_attribute = NULL;

				pair<double, Dwarf_Die> result;
				result = make_pair<double, Dwarf_Die>((double)ID, (Dwarf_Die)newDie);
				createdDies.insert(result);

				/* If the current CU die doesn't have a child, make the first new newDie the child */
				if (NULL == Dwarf_CU_Context::_currentCU->_die->_child) {
					Dwarf_CU_Context::_currentCU->_die->_child = newDie;
				}

				/* Set the last created die's sibling to be this die*/
				if (NULL != _lastDie) {
					_lastDie->_sibling = newDie;
				}
				_lastDie = newDie;
			}
		}
		if (NULL == newDie) {
			ret = DW_DLV_ERROR;
		} else {
			switch (tag) {
			case DW_TAG_class_type:
				break;
			case DW_TAG_const_type:
				break;
			case DW_TAG_enumeration_type:
				break;
			case DW_TAG_typedef:
				break;
			case DW_TAG_structure_type:
			case DW_TAG_union_type:
				break;
			default:
				/* No-op */
				break;
			}
		}
	}
	return ret;
}

static int
parseDwarfInfo(const char *line)
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
						/* Create one compilation unit context per file */
						Dwarf_CU_Context *newCU = new Dwarf_CU_Context();

						/* Create a DIE for the CU */
						Dwarf_Die newDie = new Dwarf_Die_s();

						if ((NULL == newCU) || (NULL == newDie)) {
							ret = DW_DLV_ERROR;
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
							newDie->_attribute = NULL;

							if (NULL == Dwarf_CU_Context::_firstCU) {
								Dwarf_CU_Context::_firstCU = newCU;
								Dwarf_CU_Context::_currentCU = Dwarf_CU_Context::_firstCU;
							} else {
								Dwarf_CU_Context::_currentCU->_nextCU = newCU;
								Dwarf_CU_Context::_currentCU->_die->_sibling = newDie;
								newDie->_previous = Dwarf_CU_Context::_currentCU->_die;
								Dwarf_CU_Context::_currentCU = newCU;
							}
							_startNewFile = 1;
						}
					}
				}
			}
		}
	} else if (1 == _startNewFile) {
		if (string::npos != data.find(FILE_NAME)) {
			size_t fileNameIndex = data.find(FILE_NAME) + FILE_NAME.size();
			size_t index = data.substr(fileNameIndex).find_first_not_of(' ');
			/* Update the _fileID */
			_fileID = extractFileID(data);

			/* Add the file to the list of files */
			Dwarf_CU_Context::_fileList.push_back(strip(data.substr(index+fileNameIndex), '\n'));

			Dwarf_Attribute name = new Dwarf_Attribute_s();
			if (NULL == name) {
				ret = DW_DLV_ERROR;
			} else {
				name->_type = DW_AT_name;
				name->_nextAttr = NULL;
				name->_form = DW_FORM_string;
				name->_sdata = 0;
				name->_udata = 0;
				name->_stringdata = strdup(Dwarf_CU_Context::_fileList.back().c_str());
				name->_refdata = 0;
				name->_ref = NULL;
				Dwarf_CU_Context::_currentCU->_die->_attribute = name;
				_startNewFile = 2;
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
						ret = parseDwarfDie(validData);
					}
				}
			}
		}
		else if (string::npos != data.find(END_OF_FILE)) {
			/* end of file has been reached, look for the next start of a file */
			_startNewFile = 0;

			/* Set _lastDie to null so that children of different CUs won't be siblings */
			_lastDie = NULL;
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
toString(const double value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}
