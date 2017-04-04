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

#include "DwarfFunctions.hpp"

/* Statics to define */
static const char *_errmsgs[] = {
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
		*filecount = Dwarf_CU_Context::_fileList.size();
		*srcfiles = new char *[*filecount];
		if (NULL == *srcfiles) {
			ret = DW_DLV_ERROR;
			setError(error, DW_DLE_MAF);
		} else {
			size_t index = 0;
			for (str_vect::iterator it = Dwarf_CU_Context::_fileList.begin(); it != Dwarf_CU_Context::_fileList.end(); it ++) {
				(*srcfiles)[index] = strdup(it->c_str());
				if (NULL == (*srcfiles)[index]) {
					ret = DW_DLV_ERROR;
					setError(error, DW_DLE_MAF);
					break;
				}
				index += 1;
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
		 delete [] ((char **)space);
	 } else if (DW_DLA_ERROR == type) {
		 delete((Dwarf_Error)space);
	 }
}

int
dwarf_hasattr(Dwarf_Die die, Dwarf_Half attr, Dwarf_Bool *returned_bool, Dwarf_Error *error)
{
	int ret = DW_DLV_OK;
	if (NULL == die) {
		ret = DW_DLV_ERROR;
		setError(error, DW_DLE_DIE_NULL);
	} if (NULL == returned_bool) {
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
	} else if (Dwarf_Die_s::refMap.end() != Dwarf_Die_s::refMap.find(offset)) {
		/* Return any Die from its global offset. */
		*return_die = Dwarf_Die_s::refMap[offset];
	} else {
		ret = DW_DLV_NO_ENTRY;
		setError(error, DW_DLE_BADOFF);
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

void
setError(Dwarf_Error *error, Dwarf_Half num)
{
	if (NULL != error) {
		*error = new Dwarf_Error_s();
		if (NULL != *error) {
			(*error)->_errno = num;
		}
	}
}
