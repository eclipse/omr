/*******************************************************************************
 * Copyright (c) 2015, 2021 IBM Corp. and others
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

#include "ddr/scanner/dwarf/DwarfFunctions.hpp"

/* Statics to create: */
static bool findTool(char **buffer, const char *command);
static void deleteDie(Dwarf_Die die);
static void cleanUnknownDiesInCU(Dwarf_CU_Context *cu);
static int parseDwarfInfo(char *line, Dwarf_Die *lastCreatedDie, Dwarf_Die *currentDie,
	size_t *lastIndent, unordered_map<Dwarf_Die *, Dwarf_Off> *refToPopulate, Dwarf_Error *error);
static int parseCompileUnit(char *line, size_t *lastIndent, Dwarf_Error *error);
static int createDwarfDie(Dwarf_Half tag, Dwarf_Die *lastCreatedDie,
	Dwarf_Die *currentDie, size_t *lastIndent, size_t spaces, Dwarf_Error *error);
static int parseDwarfDie(char *line, Dwarf_Die *lastCreatedDie,
	Dwarf_Die *currentDie, size_t *lastIndent, size_t spaces, Dwarf_Error *error);
static void parseTagString(char *string, size_t length, Dwarf_Half *tag);
static int parseAttribute(char *line, Dwarf_Die *lastCreatedDie,
	unordered_map<Dwarf_Die *, Dwarf_Off> *refToPopulate, Dwarf_Error *error);
static void parseAttrType(char *string, size_t length, Dwarf_Half *type, Dwarf_Half *form);
Dwarf_CU_Context * Dwarf_CU_Context::_firstCU;
Dwarf_CU_Context * Dwarf_CU_Context::_currentCU;

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
	Dwarf_CU_Context::_fileId.clear();
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
	Dwarf_CU_Context::_currentCU = NULL;
	Dwarf_CU_Context::_firstCU = NULL;

	/* Get the original file path of the opened file. */
	char filepath[PATH_MAX] = {0};
	if (0 > fcntl(fd, F_GETPATH, filepath)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_FNO);
	}

	/* Call the dwarfdump command on the file's dSYM bundle and read its output.
	 * The bundle must first be created by using dsymutil on an object or executable.
	 */
	FILE *fp = NULL;
	if (DW_DLV_OK == ret) {

		/* On some Mac machines, calling "dwarfdump" in shell calls a version named
		 * "llvm-dwarfdump" instead of the version of dwarfdump DDR uses, which is
		 * instead named "dwarfdump-classic" on those systems. Thus, we first try to
		 * find "dwarfdump-classic" directly in case "dwarfdump" is linked to
		 * "llvm-dwarfdump". If we don't find "dwarfdump-classic", then the machine
		 * we are on uses the correct version of dwarfdump.
		 */
		char *toolpath = NULL;
		if (findTool(&toolpath, "xcrun -f dwarfdump-classic 2>/dev/null")
		||  findTool(&toolpath, "xcrun -f dwarfdump 2>/dev/null")) {
			stringstream command;
			command << toolpath << " " << filepath << " 2>&1";
			fp = popen(command.str().c_str(), "r");
		}
		if (NULL != toolpath) {
			free(toolpath);
		}

		if (NULL == fp) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_IOF);
		}
	}

	/* Parse the output from dwarfdump to create the Die tree. */
	unordered_map<Dwarf_Die *, Dwarf_Off> refToPopulate;
	if (DW_DLV_OK == ret) {
		size_t len = 0;
		char *buffer = NULL;
		Dwarf_Die lastCreatedDie = NULL;
		Dwarf_Die currentDie = NULL;
		size_t lastIndent = 0;
		while ((-1 != getline(&buffer, &len, fp)) && (DW_DLV_OK == ret)) {
			if (0 == strncmp(buffer, "error", 5)) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_NOB);
			} else {
				ret = parseDwarfInfo(buffer, &lastCreatedDie, &currentDie, &lastIndent, &refToPopulate, error);
			}
		}
		free(buffer);
		cleanUnknownDiesInCU(Dwarf_CU_Context::_currentCU);
	}
	if (DW_DLV_OK == ret) {
		for (unordered_map<Dwarf_Die *, Dwarf_Off>::iterator it = refToPopulate.begin(); it != refToPopulate.end(); ++it) {
			(*it->first) = Dwarf_Die_s::refMap[it->second];
		}
		Dwarf_CU_Context::_currentCU = NULL;
	}

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
		delete attr;
		attr = next;
	}

	/* Free the Die's siblings and children. */
	if (NULL != die->_sibling) {
		deleteDie(die->_sibling);
	}
	if (NULL != die->_child) {
		deleteDie(die->_child);
	}
	delete die;
}

static void
cleanUnknownDies(Dwarf_Die die)
{
	Dwarf_Die previousSibling = NULL;
	while (NULL != die) {
		if (DW_TAG_unknown == die->_tag) {
			Dwarf_Die toDelete = die;
			Dwarf_Die nextDie = NULL;
			if (NULL != die->_child) {
				Dwarf_Die child = die->_child;
				for (; NULL != child->_sibling; child = child->_sibling) {
					child->_parent = toDelete->_parent;
				}
				child->_parent = toDelete->_parent;
				child->_sibling = toDelete->_sibling;
				nextDie = toDelete->_child;
			} else {
				nextDie = toDelete->_sibling;
			}
			if (NULL == previousSibling) {
				toDelete->_parent->_child = nextDie;
			} else {
				previousSibling->_sibling = nextDie;
			}
			die = nextDie;
			toDelete->_child = NULL;
			toDelete->_sibling = NULL;
			deleteDie(toDelete);
		} else {
			cleanUnknownDies(die->_child);
			previousSibling = die;
			die = die->_sibling;
		}
	}
}

static void
cleanUnknownDiesInCU(Dwarf_CU_Context *cu)
{
	if (NULL != cu) {
		cleanUnknownDies(cu->_die);
	}
}

static int
parseDwarfInfo(char *line, Dwarf_Die *lastCreatedDie, Dwarf_Die *currentDie,
	size_t *lastIndent, unordered_map<Dwarf_Die *, Dwarf_Off> *refToPopulate,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	/* There are two possible formats for a line. Every second line
	 * is blank. Non-blank lines either begin with
	 * "0x[address]: [whitespace...] TAG_*" to represent a new Die org/legal/epl-v10
	 * "[whitespae...] AT_*( {[optional address]} ( [value] ) ) for a die attribute.
	 * The amount of whitespace varies to indicate parent-child relationships
	 * between Die's.
	 */
	if (0 == strncmp("0x", line, 2)) {
		/* Find the end of the address and number of spaces. Amount of indentation
		 * represents parent-child relationship between Dwarf_Die's.
		 */
		char *endOfAddress = NULL;
		Dwarf_Off address = strtoul(line, &endOfAddress, 16);
		size_t spaces = 0;
		if (NULL != endOfAddress) {
			endOfAddress += 1;
			spaces = strspn(endOfAddress, " ");
		}
		/* The string "TAG_[tag]" or "Compile Unit:" or "NULL" should be next. */
		if (0 == strncmp(endOfAddress + spaces, "Compile Unit: ", 14)) {
			cleanUnknownDiesInCU(Dwarf_CU_Context::_currentCU);
			ret = parseCompileUnit(endOfAddress + spaces + 14, lastIndent, error);
			lastCreatedDie = NULL;
		} else if (0 == strncmp(endOfAddress + spaces, "TAG_", 4)) {
			ret = parseDwarfDie(endOfAddress + spaces + 4, lastCreatedDie, currentDie, lastIndent, spaces, error);
			if (DW_DLV_OK == ret && DW_TAG_unknown != (*lastCreatedDie)->_tag) {
				Dwarf_Die_s::refMap[address] = *lastCreatedDie;
			}
		} else if (0 == strncmp(endOfAddress + spaces, "Unknown DW_TAG constant", 23)) {
			/* Create a placeholder DIE for a tag that dwarfdump-classic doesn't understand. */
			ret = createDwarfDie(DW_TAG_unknown, lastCreatedDie, currentDie, lastIndent, spaces, error);
		} else if (0 == strncmp(endOfAddress + spaces, "NULL", 4)) {
			/* A "NULL" line indicates going up one level in the Die tree.
			 * Process it only if the current level contained a Die that was
			 * processed.
			 */
			if (spaces <= *lastIndent) {
				*lastCreatedDie = (*lastCreatedDie)->_parent;
			}
		} else {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_VMM);
		}
	} else if ((0 == strncmp("  ", line, 2)) && (NULL != *currentDie)) {
		/* Die attribute lines begin with "[whitespace...] AT_". To belong
		 * to the last Die to be created, it must contain 12 spaces more
		 * than the last indentation (For the size of the Die offset text).
		 */
		size_t spaces = strspn(line, " ");
		if ((spaces == *lastIndent + 12) && (0 == strncmp(line + spaces, "AT_", 3))) {
			ret = parseAttribute(line + spaces + 3, lastCreatedDie, refToPopulate, error);
		}
	}
	return ret;
}

static int
parseCompileUnit(char *line, size_t *lastIndent, Dwarf_Error *error)
{
	/* Compile unit lines are of the format:
	 * 0x00000000: Compile Unit: length = 0x[offset]  version = 0x[version]  abbr_offset = 0x[offset]  addr_size = 0x[size]  (next CU at 0x[offset])
	 */
	int ret = DW_DLV_OK;
	Dwarf_Unsigned CUheaderLength = 0;
	Dwarf_Half versionStamp = 0;
	Dwarf_Off abbrevOffset = 0;
	Dwarf_Half addressSize = 0;
	Dwarf_Unsigned nextCUheaderOffset = 0;

	char *valueString = strstr(line, "length = ");
	if (NULL == valueString) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_VMM);
	} else {
		CUheaderLength = (Dwarf_Unsigned)strtoul(valueString + 9, &valueString, 16);
	}

	if (DW_DLV_OK == ret) {
		valueString = strstr(valueString, "version = ");
		if (NULL == valueString) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_VMM);
		} else {
			versionStamp = (Dwarf_Half)strtoul(valueString + 10, &valueString, 16);
		}
	}

	if (DW_DLV_OK == ret) {
		valueString = strstr(valueString, "abbr_offset = ");
		if (NULL == valueString) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_VMM);
		} else {
			abbrevOffset = (Dwarf_Off)strtoul(valueString + 14, &valueString, 16);
		}
	}

	if (DW_DLV_OK == ret) {
		valueString = strstr(valueString, "addr_size = ");
		if (NULL == valueString) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_VMM);
		} else {
			addressSize = (Dwarf_Half)strtoul(valueString + 12, &valueString, 16);
		}
	}

	if (DW_DLV_OK == ret) {
		valueString = strstr(valueString, "(next CU at ");
		if (NULL == valueString) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_VMM);
		} else {
			nextCUheaderOffset = (Dwarf_Unsigned)strtoul(valueString + 12, &valueString, 16);
		}
	}

	if (DW_DLV_OK == ret) {
		Dwarf_CU_Context *newCU = new Dwarf_CU_Context;
		if (NULL == newCU) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			newCU->_CUheaderLength = CUheaderLength;
			newCU->_versionStamp = versionStamp;
			newCU->_abbrevOffset = abbrevOffset;
			newCU->_addressSize = addressSize;
			newCU->_nextCUheaderOffset = nextCUheaderOffset;
			newCU->_nextCU = NULL;
			newCU->_die = NULL;
			if (NULL == Dwarf_CU_Context::_firstCU) {
				Dwarf_CU_Context::_firstCU = newCU;
				Dwarf_CU_Context::_currentCU = Dwarf_CU_Context::_firstCU;
			} else {
				Dwarf_CU_Context::_currentCU->_nextCU = newCU;
				Dwarf_CU_Context::_currentCU = newCU;
			}
			*lastIndent = 0;
		}
	}
	return ret;
}

static int
createDwarfDie(Dwarf_Half tag, Dwarf_Die *lastCreatedDie,
	Dwarf_Die *currentDie, size_t *lastIndent, size_t spaces, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	Dwarf_Die newDie = new Dwarf_Die_s;

	if (NULL == newDie) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_MAF);
	} else {
		newDie->_tag = tag;
		newDie->_parent = NULL;
		newDie->_sibling = NULL;
#if defined(AIXPPC)
		newDie->_previous = NULL;
#endif /* defined (AIXPPC) */
		newDie->_child = NULL;
		newDie->_context = Dwarf_CU_Context::_currentCU;
		newDie->_attribute = NULL;

		if (0 == *lastIndent) {
			/* No last indent indicates that this Die is at the start of a CU. */
			Dwarf_CU_Context::_currentCU->_die = newDie;
		} else if (spaces > *lastIndent) {
			/* If the Die is indented farther than the last Die, it is a first child. */
			newDie->_parent = *lastCreatedDie;
			(*lastCreatedDie)->_child = newDie;
		} else if (spaces <= *lastIndent) {
			/* If the Die is indented equally to the last Die, it is a
			* sibling Die. If the indentation has decreased, the last
			* Die would have been updated to one level up the tree when
			* a "NULL" line indicated there were no more siblings.
			*/
			newDie->_parent = (*lastCreatedDie)->_parent;
			(*lastCreatedDie)->_sibling = newDie;
		}
		*lastCreatedDie = newDie;
		*currentDie = newDie;
		*lastIndent = spaces;
	}
	return ret;
}

static int
parseDwarfDie(char *line, Dwarf_Die *lastCreatedDie,
	Dwarf_Die *currentDie, size_t *lastIndent, size_t spaces, Dwarf_Error *error)
{
	Dwarf_Half tag = DW_TAG_unknown;
	size_t span = strcspn(line, " ");
	parseTagString(line, span, &tag);

	return createDwarfDie(tag, lastCreatedDie, currentDie, lastIndent, spaces, error);
}

static const pair<const char *, Dwarf_Half> tagStrings[] = {
	make_pair("array_type", DW_TAG_array_type),
	make_pair("base_type", DW_TAG_base_type),
	make_pair("class_type", DW_TAG_class_type),
	make_pair("compile_unit", DW_TAG_compile_unit),
	make_pair("const_type", DW_TAG_const_type),
	make_pair("enumeration_type", DW_TAG_enumeration_type),
	make_pair("enumerator", DW_TAG_enumerator),
	make_pair("inheritance", DW_TAG_inheritance),
	make_pair("member", DW_TAG_member),
	make_pair("namespace", DW_TAG_namespace),
	make_pair("pointer_type", DW_TAG_pointer_type),
	make_pair("ptr_to_member_type", DW_TAG_ptr_to_member_type),
	make_pair("restrict_type", DW_TAG_restrict_type),
	make_pair("shared_type", DW_TAG_shared_type),
	make_pair("structure_type", DW_TAG_structure_type),
	make_pair("subprogram", DW_TAG_subprogram),
	make_pair("subrange_type", DW_TAG_subrange_type),
	make_pair("subroutine_type", DW_TAG_subroutine_type),
	make_pair("typedef", DW_TAG_typedef),
	make_pair("union_type", DW_TAG_union_type),
	make_pair("volatile_type", DW_TAG_volatile_type),
};

static void
parseTagString(char *string, size_t length, Dwarf_Half *tag)
{
	size_t options = sizeof(tagStrings) / sizeof(tagStrings[0]);
	for (size_t i = 0; i < options; ++i) {
		if (0 == strncmp(string, tagStrings[i].first, length)) {
			*tag = tagStrings[i].second;
			break;
		}
	}
}

static int
parseAttribute(char *line, Dwarf_Die *lastCreatedDie,
	unordered_map<Dwarf_Die *, Dwarf_Off> *refToPopulate, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;

	/* Get the type and form of the attribute. */
	Dwarf_Half type = DW_TAG_unknown;
	Dwarf_Half form = DW_FORM_unknown;
	size_t span = strcspn(line, "(");
	parseAttrType(line, span, &type, &form);

	if ((DW_AT_unknown != type) && (DW_FORM_unknown != form)) {
		Dwarf_Attribute newAttr = new Dwarf_Attribute_s;
		if (NULL == newAttr) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			newAttr->_nextAttr = NULL;
			newAttr->_stringdata = NULL;
			newAttr->_type = type;
			newAttr->_form = form;
			newAttr->_ref = NULL;

			/* Parse the value of the attribute based on its form. */
			if (DW_AT_decl_file == type) {
				char *valueStart = line + span + 3;
				size_t valueLength = strchr(valueStart, '"') - valueStart;
				newAttr->_stringdata = strndup(valueStart, valueLength);
				if (NULL == newAttr->_stringdata) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
					newAttr->_udata = 0;
				} else if (DW_TAG_unknown != (*lastCreatedDie)->_tag) {
					string fileName = string(newAttr->_stringdata);
					unordered_map<string, size_t>::const_iterator insertIt = Dwarf_CU_Context::_fileId.insert(make_pair(fileName, Dwarf_CU_Context::_fileId.size())).first;
					/* whether the insert succeeded or not due to duplicates, the pair at the iterator will have the right index value */
					newAttr->_udata = insertIt->second + 1; /* since this attribute is indexed at 1 */


				}
			} else if (DW_FORM_string == form) {
				char *valueStart = line + span + 3;
				size_t valueLength = strchr(valueStart, '"') - valueStart;
				newAttr->_stringdata = strndup(valueStart, valueLength);
				if (NULL == newAttr->_stringdata) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
				}
			} else if (DW_FORM_ref1 == form) {
				newAttr->_refdata = strtoul(line + span + 3, NULL, 16);
				if (DW_TAG_unknown != (*lastCreatedDie)->_tag) {
					refToPopulate->emplace(&newAttr->_ref, newAttr->_refdata);
				}
			} else if (DW_FORM_udata == form) {
				newAttr->_udata = strtoul(line + span + 1, NULL, 0);
			} else if (DW_FORM_sdata == form) {
				newAttr->_sdata = strtol(line + span + 1, NULL, 0);
			} else if (DW_FORM_flag == form) {
				size_t whitespace = strspn(line + span + 1, " \t") + span + 1;
				if (0 == strncmp(line + whitespace, "true", 4)) {
					newAttr->_flag = 1;
				} else if (0 == strncmp(line + whitespace, "false", 5)) {
					newAttr->_flag = 0;
				} else {
					newAttr->_flag = 0 != strtol(line + whitespace, NULL, 0);
				}
			} else {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_VMM);
			}
			if (DW_DLV_OK == ret) {
				/* Add the attribute to the most recently created Die. */
				if (NULL == (*lastCreatedDie)->_attribute) {
					(*lastCreatedDie)->_attribute = newAttr;
				} else {
					Dwarf_Attribute attr = (*lastCreatedDie)->_attribute;
					while (NULL != attr->_nextAttr) {
						attr = attr->_nextAttr;
					}
					attr->_nextAttr = newAttr;
				}
			} else {
				delete newAttr;
			}
		}
	}
	return ret;
}

static const tuple<const char *, Dwarf_Half, Dwarf_Half> attrStrings[] = {
	make_tuple("byte_size", DW_AT_byte_size, DW_FORM_udata),
	make_tuple("bit_size", DW_AT_bit_size, DW_FORM_udata),
	make_tuple("comp_dir", DW_AT_comp_dir, DW_FORM_string),
	make_tuple("const_value", DW_AT_const_value, DW_FORM_sdata),
	make_tuple("data_bit_offset", DW_AT_data_bit_offset, DW_FORM_udata),
	make_tuple("data_member_location", DW_AT_data_member_location, DW_FORM_udata),
	make_tuple("decl_file", DW_AT_decl_file, DW_FORM_udata),
	make_tuple("decl_line", DW_AT_decl_line, DW_FORM_udata),
	make_tuple("external", DW_AT_external, DW_FORM_flag),
	make_tuple("linkage_name", DW_AT_linkage_name, DW_FORM_string),
	make_tuple("name", DW_AT_name, DW_FORM_string),
	make_tuple("specification", DW_AT_specification, DW_FORM_ref1),
	make_tuple("type", DW_AT_type, DW_FORM_ref1),
	make_tuple("upper_bound", DW_AT_upper_bound, DW_FORM_udata),
};

static void
parseAttrType(char *string, size_t length, Dwarf_Half *type, Dwarf_Half *form)
{
	size_t options = sizeof(attrStrings) / sizeof(attrStrings[0]);
	for (size_t i = 0; i < options; ++i) {
		if (0 == strncmp(string, get<0>(attrStrings[i]), length)) {
			*type = get<1>(attrStrings[i]);
			*form = get<2>(attrStrings[i]);
			break;
		}
	}
}

/* Runs a shell command to get the path for a tool DDR needs (dwarfdump)
 * If successful, returns true and stores the path in 'buffer' variable.
 * If not, returns false.
 */
static bool
findTool(char **buffer, const char *command)
{
	bool found = false;
	FILE *fp = popen(command, "r");
	if (NULL != fp) {
		size_t cap = 0;
		ssize_t len = getline(buffer, &cap, fp);
		/* if xcrun fails to find the tool, then returned length would be 0 */
		if ((len > 0) &&  ('/' == (*buffer)[0])) {
			/* remove the newline consumed by getline */
			if ('\n' == (*buffer)[len - 1]) {
				(*buffer)[len - 1] = '\0';
			}
			found = true;
		}
		pclose(fp);
	}
	return found;
}
