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
#include "config.hpp"

using std::getline;
using std::make_pair;
using std::pair;
using std::tr1::regex;
using std::tr1::regex_search;
using std::tr1::smatch;
using std::string;
using std::stringstream;
using std::tr1::tuple;
using std::tr1::unordered_map;
using std::vector;

const string NAME ("[^;:'\"]*");
const string INTEGER ("(-)?[0-9]+");
const string NON_NEG_INTEGER ("[0-9]+");
const string NUM_BYTES (NON_NEG_INTEGER);
const string NUM_BITS (NON_NEG_INTEGER);
const string BIT_OFFSET (INTEGER);
const string CLASS_TYPEID (NON_NEG_INTEGER);
const string ORD_VALUE (INTEGER);
const string CLASS_KEY ("(s|u|c)");
const string ACCESS_SPEC ("(i|o|u)"); /* MUST ALWAYS BE THERE */
const string ANON_SPEC ("a");
const string BASE_CLASS_OFFSET (NON_NEG_INTEGER);
const string VIRTUAL_ACCESS_SPEC ("(v?"+ACCESS_SPEC+"?)");
const string OPT_BASE_SPEC (VIRTUAL_ACCESS_SPEC+BASE_CLASS_OFFSET+"\\:"+CLASS_TYPEID+",?"); /* comma if we have multiple of this */

const string built_type_descriptors[29] = {"integer", "char", "short", "long", "char", "short",
											"integer", "unsigned", "long", "void", "float_single_precision",
											"long_double", "integer", "boolean", "short_real", "real",
											"stringptr", "character", "complex", "complex", "integer",
											"integer", "integer", "wchar", "long_long",
											"long_long", "logical"};
const int built_type_descriptors_size[29] = {32, 16, 32, 8, 8, 16,
											 32, 32, 32, 0, 32, 64,
											 64, 32, 32, 32, 64, 8,
											 64, 64, 8, 16, 32,
											 16, 64, 64, 64, 64};
const string MEMBER_ATTRS[5] = {"invalid_attr", "isStatic", "isVtblPtr", "vbase_pointer", "vbase_self-pointer"};
const string DECLARATION_TYPES[10] = {"invalid_declaration", "def_declaration", "constant_declaration",
									  "structure_declaration", "union_declaration", "class_declaration",
									  "enum_declaration", "label_declaration", "class_structure_declaration",
									  "class_union_declaration"};

const string SIZE_TYPES[5] = {"no_type", "num_bits", "num_bytes", "bound", "num_elements"};
const string ACCESS_TYPES[3] = {"private", "protected", "public"};

#define AIX_TYPE_array 0x01
#define AIX_TYPE_boolean 0x02
#define AIX_TYPE_character 0x03
#define AIX_TYPE_integer 0x04
#define AIX_TYPE_decimal 0x05
#define AIX_TYPE_string 0x06
#define AIX_TYPE_complex 0x07
#define AIX_TYPE_set 0x08
#define AIX_TYPE_file 0x09
#define AIX_TYPE_floating_point 0x0a
#define AIX_TYPE_decimal_floating_point 0x0b
#define AIX_TYPE_constant 0x0c
#define AIX_TYPE_ptr_to_member_type 0x0d
#define AIX_TYPE_wide_character 0x0e
#define AIX_TYPE_multiple_instance 0x0f
#define AIX_TYPE_pointer 0x10
#define AIX_TYPE_reference 0x11
#define AIX_TYPE_volatile 0x12
#define AIX_TYPE_ellipses 0x13

typedef vector<string> str_vect;
typedef vector<double> double_vect;
typedef enum option_info_type {NUMERIC, ATTR_VALUE, NOTHING} option_info_type;

struct info_type_results {
	string name;
	string type;
	string members;

	bool empty()
	{
		bool ret = FALSE;
		/* Only need to check type lenght because it's
		 * the only one that'll always be set
		 */
		if (0 == type.length()) {
			ret = TRUE;
		}

		return ret;
	}
};

struct option_info {
	string name;
	double ID;
	string attrValue;

	option_info_type type;

	option_info (string name_value = "", option_info_type type_value = NOTHING) : name (name_value), type (type_value) {}

	bool empty()
	{
		bool ret = FALSE;
		if ((0 == name.length() == 0) && (NOTHING == type)) {
			ret = TRUE;
		}
		return ret;
	}
};
typedef vector<option_info> options_vect;

struct obj_size {
	size_t sizeValue;
	string sizeType;

	obj_size (size_t value = 0, string type = "") : sizeValue (value), sizeType (type) {}
};

struct type {
	double ID;
	string tag;
	bool isChild;

	type (double ID = 0, string tag = "!") : ID (ID), tag (tag) {}

	bool empty()
	{
		bool ret = FALSE;
		if (0 == ID) {
			ret = TRUE;
		}
		return ret;
	}
};
struct obj_type{
	unsigned short type;
	bool isBuiltIn;
	double typeID;
	int isSigned;		/* Needs to be int because we can have 0 (Not signed), 1 (Signed) and 2 (Don't know) */
	options_vect options;	
};

struct Info {
	string name;
	type typeID;
	obj_size size;
	double_vect members;
	options_vect options;
	obj_type typeDef;
	string rawData;
	string declFile;

	bool empty()
	{
		bool ret = FALSE;
		if (0 == typeID.ID) {
			ret = TRUE;
		}
		return ret;
	}
};

typedef unordered_map<double, Info> data_map;
typedef data_map::iterator data_map_itt;

class AixSymbolTableParser
{
public:
	AixSymbolTableParser() : _fileCounter(0), _startNewFile(0), _fileID(0);
	~AixSymbolTableParser();

	int parseDumpOutput(const string data);
	data_map_itt findEntry(const double insertionKey);
	
	string getBuiltInTypeDesc(const int typeID);
	int getBuiltInTypeSize(const int typeID);
	DDR_RC getNextFile(double *fileID);

	string beautifyVector(const str_vect data, const string location, const int level = 1);
	string beautifyVector(const vector<double> data, const string location, const int level = 1);
	string getPadding(const int level);
	string beautifyInfo(const Info data, const double key, const bool overrideChildTag = FALSE, const int level = 1);
	void printMapContents();
	string beautifyMatchResults(const smatch m, const string location);
	string beautifyOptionsVector(const options_vect data, const string location, const int level = 1);
	string beautifyInfoTypeResults(const info_type_results results, const string location);

private:
	/* Unorderd map for global access to the data */
	data_map _parsedData;

	double_vect _fileList;
	int _fileCounter;

	/* Global variables required by parseDumpOutput */
	int _startNewFile;
	double _fileID;
	data_map::iterator _fileEntry;

	DDR_RC additionalClassOptions(const string data, const double fileID, options_vect *classOptions);

	string extractAccessSpec(const string data, const int index = 0);
	string extractClassType(const string data);
	double extractFileID(const string data);
	double extractTypeDefID(const string data, const int index = 1);
	double extractTypeID(const string data, const int index = 1);

	double generateChildTypeID(const double parentID, const double childOffset);
	double generateInsertionKey(const double parentID, const double typeID);
	string getDeclFile(const double insertionKey);
	string getEntryName(const double insertionKey);
	string getInfoType(const string data, info_type_results *matchResults);
	string getMemberAttrsType(const string data);
	
	bool isBuiltIn(const string data, double *ID);
	bool isBuiltIn(const int ID);
	bool isCompilerGenerated(const string data);
	bool isPassedByValue(const string data);
	DDR_RC insertEntry(const double insetionKey, const Info object);
	DDR_RC insertFileEntry(const string fileName, const double fileID);
	DDR_RC insertRejectedEntry(const double insertionKey, Info *object);
	int isSigned(const int ID);
	bool isValidDataMember(const string data, Info *dataMember);
	bool isValidField(const string data, Info *field);
	bool isValidFriendClass(const string data, Info *friendClass);
	bool isValidNestedClass(const string data, Info *nestedClass);
	
	option_info *makeOptionInfoPair(const string nameValue, const option_info_type typeValue, const string attributeValue, const int idValue);
	
	DDR_RC parseArray(const string data, Info *arrayEntry);
	string parseBound(const string data);
	DDR_RC parseClassMembers(const string data, const double classID, const double fileID, double_vect *classMembers);
	DDR_RC parseConstants(const string data, Info *constantEntry);
	DDR_RC parseFields(const string data, const double parentID, const double file_insertion_key, double_vect *memberIDs);
	DDR_RC parseVtblPointerInfo(const string data, options_vect *pointerInfo);
	DDR_RC parseEnumData(const string enumeratorData, const double enumID, const double fileID, double_vect *enumeratorIDs);
	DDR_RC parseInfo(const string rawData, const double fileID, double *memberID);
	DDR_RC parseTypeDef(const string data, Info *typeDefEntry);
	
	void split(const string data, char delimeters, str_vect *elements);
	str_vect split(const string data, char delimeters);
	string strip(const string str, const char chr);
	string stripLeading(const string str, const char chr);
	string striptrailing(const string str, const char chr);
	double shiftDecimal(const double value);
	
	double toDouble(const string value);
	int toInt(const string value);
	size_t toSize(const string size);
	string toString(const double value);
	string getStringDescription(const unsigned short value);

	DDR_RC updateEntry(data_map::iterator entry, Info update, bool overRideChecks = FALSE);
}
