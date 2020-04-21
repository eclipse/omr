/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

/* Statics to define */
static const char * const _errmsgs[] = {
	"No error (0)",
	"DW_DLE_ATTR_FORM_BAD (1)",
	"DW_DLE_BADOFF (2) Invalid offset",
	"DW_DLE_DIE_NULL (3)",
	"DW_DLE_FNO (4) file not open",
	"DW_DLE_IA (5) invalid argument",
	"DW_DLE_IOF (6) I/O failure",
	"DW_DLE_MAF (7) memory allocation failure",
	"DW_DLE_NOB (8) not an object file or dSYM bundle not found",
	"DW_DLE_VMM (9) dwarf format/library version mismatch",
};

unordered_map<string, size_t> Dwarf_CU_Context::_fileId;
unordered_map<Dwarf_Off, Dwarf_Die> Dwarf_Die_s::refMap;

int
dwarf_srcfiles(Dwarf_Die die, char ***srcfiles, Dwarf_Signed *filecount, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else if ((NULL == filecount) || (NULL == srcfiles)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		/* Copy the list of file names found during dwarf_init.
		 * This implementation maintains the entire list through
		 * all CU's, rather than just the current CU, for simplicity.
		 */
		*filecount = Dwarf_CU_Context::_fileId.size();
		*srcfiles = new char *[*filecount];
		if (NULL == *srcfiles) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			for (unordered_map<string, size_t>::const_iterator it = Dwarf_CU_Context::_fileId.begin(); it != Dwarf_CU_Context::_fileId.end(); ++it) {
				(*srcfiles)[it->second] = strdup(it->first.c_str());
				if (NULL == (*srcfiles)[it->second]) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
					break;
				}
			}
		}
	}
	return ret;
}

int
dwarf_attr(
	Dwarf_Die die,
	Dwarf_Half attr,
	Dwarf_Attribute *return_attr,
	Dwarf_Error *error)
{
	int ret = DW_DLV_NO_ENTRY;
	if (NULL == die) {
		setError(error, DW_DLE_DIE_NULL);
		ret = DW_DLV_ERROR;
	} else if (NULL == return_attr) {
		setError(error, DW_DLE_DIE_NULL);
		ret = DW_DLE_IA;
	} else {
		/* Iterate the Die's attributes to look for one with
		 * a type matching 'attr' and return it.
		 */
		*return_attr = NULL;
		Dwarf_Attribute attrCurrent = die->_attribute;
		while (NULL != attrCurrent) {
			if (attr == attrCurrent->_type) {
				*return_attr = attrCurrent;
				ret = DW_DLV_OK;
				break;
			}
			attrCurrent = attrCurrent->_nextAttr;
		}
	}
	return ret;
}

char *
dwarf_errmsg(Dwarf_Error error)
{
	return (char *)_errmsgs[error->_errno];
}

int
dwarf_formstring(Dwarf_Attribute attr, char **returned_string, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == returned_string)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (DW_FORM_string != attr->_form) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_ATTR_FORM_BAD);
	} else {
		/* Return attribute data. */
		*returned_string = attr->_stringdata;
	}
	return ret;
}

void
dwarf_dealloc(Dwarf_Debug dbg, void *space, Dwarf_Unsigned type)
{
	/* Since only the dwarf_init() function allocates anything,
	 * all memory is freed in dwarf_finish(). Freeing any data
	 * in part could lead to not being able to free other data
	 * late and would prevent using the Die's that rely on the
	 * freed data. Just as in the dwarf library, dwarf_dealloc()
	 * is a no-op for Dies and Attributes.
	 */
	if (DW_DLA_STRING == type) {
		free((char *)space);
	} else if (DW_DLA_LIST == type) {
		delete[] (void **)space;
	} else if (DW_DLA_ERROR == type) {
		delete (Dwarf_Error)space;
	}
}

int
dwarf_hasattr(Dwarf_Die die, Dwarf_Half attr, Dwarf_Bool *returned_bool, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else if (NULL == returned_bool) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		/* Iterate the Die's attributes to check if it has an
		 * attribute of a given type.
		 */
		*returned_bool = false;
		Dwarf_Attribute attrCurrent = die->_attribute;
		while (NULL != attrCurrent) {
			if (attr == attrCurrent->_type) {
				*returned_bool = true;
				break;
			}
			attrCurrent = attrCurrent->_nextAttr;
		}
	}
	return ret;
}

int
dwarf_formblock(Dwarf_Attribute attr, Dwarf_Block **returned_block, Dwarf_Error *error)
{
	/* No attributes are generated with block forms on OSX. */
	if ((NULL == attr) || (NULL == returned_block)) {
		setError(error, DW_DLE_IA);
	} else {
		setError(error, DW_DLE_ATTR_FORM_BAD);
	}
	return DW_DLV_ERROR;
}

int
dwarf_formflag(Dwarf_Attribute attr, Dwarf_Bool *returned_flag, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == returned_flag)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (DW_FORM_flag != attr->_form) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_ATTR_FORM_BAD);
	} else {
		/* Return attribute flag. */
		*returned_flag = attr->_flag;
	}
	return ret;
}

int
dwarf_formudata(Dwarf_Attribute attr, Dwarf_Unsigned  *returned_val, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == returned_val)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (DW_FORM_udata != attr->_form) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_ATTR_FORM_BAD);
	} else {
		/* Return attribute data. */
		*returned_val = attr->_udata;
	}
	return ret;
}

int
dwarf_diename(Dwarf_Die die, char **diename, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else if (NULL == diename) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		/* Get the name attribute. */
		Dwarf_Attribute attr = NULL;
		ret = dwarf_attr(die, DW_AT_name, &attr, error);
		char *name = NULL;
		if (DW_DLV_OK == ret) {
			ret = dwarf_formstring(attr, &name, error);
		}
		/* If the Die has a name attribute, copy the name string. */
		if (DW_DLV_OK == ret) {
			*diename = strdup(name);
			if (NULL == *diename) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			}
		} else if (DW_DLV_NO_ENTRY == ret) {
			*diename = strdup("");
			if (NULL == *diename) {
				ret = DW_DLV_ERROR;
				setError(error, DW_DLE_MAF);
			}
		}
	}
	return ret;
}

int
dwarf_global_formref(Dwarf_Attribute attr, Dwarf_Off *return_offset, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == return_offset)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if ((DW_FORM_ref1 > attr->_form) || (DW_FORM_ref8 < attr->_form)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_ATTR_FORM_BAD);
	} else {
		/* Return attribute data. */
		*return_offset = attr->_refdata;
	}
	return ret;
}

int
dwarf_offdie_b(Dwarf_Debug dbg,
	Dwarf_Off offset,
	Dwarf_Bool is_info,
	Dwarf_Die *return_die,
	Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == return_die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		unordered_map<Dwarf_Off, Dwarf_Die>::const_iterator iter = Dwarf_Die_s::refMap.find(offset);
		if ((Dwarf_Die_s::refMap.end() != iter) && (NULL != iter->second)) {
			/* Return any Die from its global offset. */
			*return_die = iter->second;
		} else {
			ret = DW_DLV_NO_ENTRY;
			setError(error, DW_DLE_BADOFF);
		}
	}
	return ret;
}

int
dwarf_child(Dwarf_Die die, Dwarf_Die *return_childdie, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else if (NULL == return_childdie) {
		ret = DW_DLE_IA;
		setError(error, DW_DLE_DIE_NULL);
	} else if (NULL == die->_child) {
		ret = DW_DLV_NO_ENTRY;
		*return_childdie = NULL;
	} else {
		/* Return the Die's first child. */
		*return_childdie = die->_child;
	}
	return ret;
}

int
dwarf_whatform(Dwarf_Attribute attr, Dwarf_Half *returned_final_form, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == returned_final_form)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		/* Get the data form of an attribute. */
		*returned_final_form = attr->_form;
	}
	return ret;
}

int
dwarf_tag(Dwarf_Die die, Dwarf_Half *return_tag, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else if (NULL == return_tag) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else {
		/* Get a Die's tag, indicating what type of Die it is. */
		*return_tag = die->_tag;
	}
	return ret;
}

int
dwarf_formsdata(Dwarf_Attribute attr, Dwarf_Signed *returned_val, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if ((NULL == attr) || (NULL == returned_val)) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (DW_FORM_sdata != attr->_form) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_ATTR_FORM_BAD);
	} else {
		/* Return attribute data. */
		*returned_val = attr->_sdata;
	}
	return ret;
}

int
dwarf_next_cu_header(Dwarf_Debug dbg,
	Dwarf_Unsigned *cu_header_length,
	Dwarf_Half *version_stamp,
	Dwarf_Off *abbrev_offset,
	Dwarf_Half *address_size,
	Dwarf_Unsigned *next_cu_header_offset,
	Dwarf_Error *error)
{
	/* Advance the current CU and return its properties. */
	int ret = DW_DLV_OK;
	if (NULL == Dwarf_CU_Context::_currentCU) {
		Dwarf_CU_Context::_currentCU = Dwarf_CU_Context::_firstCU;
	} else if (NULL == Dwarf_CU_Context::_currentCU->_nextCU) {
		ret = DW_DLV_NO_ENTRY;
	} else {
		Dwarf_CU_Context::_currentCU = Dwarf_CU_Context::_currentCU->_nextCU;
	}
	if (DW_DLV_OK == ret) {
		*cu_header_length = Dwarf_CU_Context::_currentCU->_CUheaderLength;
		*version_stamp = Dwarf_CU_Context::_currentCU->_versionStamp;
		*abbrev_offset = Dwarf_CU_Context::_currentCU->_abbrevOffset;
		*address_size = Dwarf_CU_Context::_currentCU->_addressSize;
		*next_cu_header_offset = Dwarf_CU_Context::_currentCU->_nextCUheaderOffset;
	}
	return ret;
}

int
dwarf_siblingof(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die *dieOut, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == dieOut) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (NULL == die) {
		*dieOut = Dwarf_CU_Context::_currentCU->_die;
	} else if (NULL == die->_sibling) {
		*dieOut = NULL;
		ret = DW_DLV_NO_ENTRY;
	} else {
		/* Get the next sibling of a Die. */
		*dieOut = die->_sibling;
	}
	return ret;
}

int
dwarf_dieoffset(Dwarf_Die die, Dwarf_Off *dieOffset, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == dieOffset) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_IA);
	} else if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} else {
		/* Get the address of a Die. */
		/* Note that the returned offset is not reversible using dwarf_offdie_b,
		 * but just needs to be unique per Die.
		 */
		*dieOffset = (Dwarf_Off)die;
	}
	return ret;
}

int
dwarf_get_TAG_name(Dwarf_Half tag, const char **name)
{
	int ret = DW_DLV_OK;
	if (NULL == name) {
		ret = DW_DLV_ERROR;
	} else {
		switch (tag) {
		case DW_TAG_unknown:
			*name = "DW_TAG_unknown";
			break;
		case DW_TAG_array_type:
			*name = "DW_TAG_array_type";
			break;
		case DW_TAG_base_type:
			*name = "DW_TAG_base_type";
			break;
		case DW_TAG_class_type:
			*name = "DW_TAG_class_type";
			break;
		case DW_TAG_compile_unit:
			*name = "DW_TAG_compile_unit";
			break;
		case DW_TAG_const_type:
			*name = "DW_TAG_const_type";
			break;
		case DW_TAG_enumeration_type:
			*name = "DW_TAG_enumeration_type";
			break;
		case DW_TAG_enumerator:
			*name = "DW_TAG_enumerator";
			break;
		case DW_TAG_inheritance:
			*name = "DW_TAG_inheritance";
			break;
		case DW_TAG_member:
			*name = "DW_TAG_member";
			break;
		case DW_TAG_namespace:
			*name = "DW_TAG_namespace";
			break;
		case DW_TAG_pointer_type:
			*name = "DW_TAG_pointer_type";
			break;
		case DW_TAG_ptr_to_member_type:
			*name = "DW_TAG_ptr_to_member_type";
			break;
		case DW_TAG_restrict_type:
			*name = "DW_TAG_restrict_type";
			break;
		case DW_TAG_reference_type:
			*name = "DW_TAG_reference_type";
			break;
		case DW_TAG_shared_type:
			*name = "DW_TAG_shared_type";
			break;
		case DW_TAG_structure_type:
			*name = "DW_TAG_structure_type";
			break;
		case DW_TAG_subprogram:
			*name = "DW_TAG_subprogram";
			break;
		case DW_TAG_subrange_type:
			*name = "DW_TAG_subrange_type";
			break;
		case DW_TAG_subroutine_type:
			*name = "DW_TAG_subroutine_type";
			break;
		case DW_TAG_typedef:
			*name = "DW_TAG_typedef";
			break;
		case DW_TAG_union_type:
			*name = "DW_TAG_union_type";
			break;
		case DW_TAG_volatile_type:
			*name = "DW_TAG_volatile_type";
			break;
		default:
			*name = "unknown";
			break;
		}
	}
	return ret;
}

void
setError(Dwarf_Error *error, Dwarf_Half num)
{
	if (NULL != error) {
		*error = new Dwarf_Error_s;
		if (NULL != *error) {
			(*error)->_errno = num;
		}
	}
}
