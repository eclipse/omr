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
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <sys/fcntl.h>
#include <unordered_map>
#include <vector>

using std::get;
using std::make_pair;
using std::pair;
using std::runtime_error;
using std::string;
using std::stringstream;
using std::unordered_map;
using std::vector;

typedef /*struct Dwarf_Debug_s*/ void *Dwarf_Debug;
typedef struct Dwarf_Die_s *Dwarf_Die;
typedef struct Dwarf_Error_s *Dwarf_Error;
typedef struct Dwarf_Attribute_s *Dwarf_Attribute;

typedef int Dwarf_Bool;
typedef unsigned long long Dwarf_Off;
typedef unsigned long long Dwarf_Unsigned;
typedef unsigned short Dwarf_Half;
typedef signed long long Dwarf_Signed;
typedef unsigned long long Dwarf_Addr;
typedef signed long Dwarf_Sword;

typedef void *Dwarf_Ptr;
typedef void (*Dwarf_Handler)(Dwarf_Error error, Dwarf_Ptr errarg);

typedef vector<string> str_vect;

#define DW_DLE_NE				0x00 /* No error */
#define DW_DLE_ATTR_FORM_BAD	0x01 /* Wrong form for attribute */
#define DW_DLE_BADOFF			0x02 /* Invalid offset */
#define DW_DLE_DIE_NULL			0x03 /* Die NULL */
#define DW_DLE_FNO				0x04 /* File not open */
#define DW_DLE_IA				0x05 /* Invalid argument */
#define DW_DLE_IOF				0x06 /* I/O failure */
#define DW_DLE_MAF				0x07 /* Memory allocation failure */
#define DW_DLE_NOB				0x08 /* Not an object file */
#define DW_DLE_VMM				0x09 /* Dwarf format/library version mismatch */


#define DW_DLC_READ 0x01

#define DW_DLV_NO_ENTRY -1
#define DW_DLV_OK 0
#define DW_DLV_ERROR 1

#define DW_AT_unknown 0x00
#define DW_AT_byte_size 0x01
#define DW_AT_bit_size 0x02
#define DW_AT_comp_dir 0x03
#define DW_AT_const_value 0x04
#define DW_AT_decl_file 0x05
#define DW_AT_decl_line 0x06
#define DW_AT_linkage_name 0x07
#define DW_AT_name 0x08
#define DW_AT_specification 0x09
#define DW_AT_type 0x0a
#define DW_AT_upper_bound 0x0b

#define DW_DLA_ATTR 0x01
#define DW_DLA_DIE 0x02
#define DW_DLA_ERROR 0x03
#define DW_DLA_LIST 0x04
#define DW_DLA_STRING 0x05

#define DW_FORM_unknown 0x00
#define DW_FORM_ref1 0x01
#define DW_FORM_ref2 0x02
#define DW_FORM_ref4 0x03
#define DW_FORM_ref8 0x04
#define DW_FORM_udata 0x05
#define DW_FORM_sdata 0x06
#define DW_FORM_string 0x07

#define DW_TAG_unknown 0x00
#define DW_TAG_array_type 0x01
#define DW_TAG_base_type 0x02
#define DW_TAG_class_type 0x03
#define DW_TAG_compile_unit 0x04
#define DW_TAG_const_type 0x05
#define DW_TAG_enumeration_type 0x06
#define DW_TAG_enumerator 0x07
#define DW_TAG_inheritance 0x08
#define DW_TAG_member 0x09
#define DW_TAG_namespace 0x0a
#define DW_TAG_pointer_type 0x0b
#define DW_TAG_restrict_type 0x0c
#define DW_TAG_shared_type 0x0d
#define DW_TAG_structure_type 0x0e
#define DW_TAG_subprogram 0x0f
#define DW_TAG_subrange_type 0x10
#define DW_TAG_subroutine_type 0x11
#define DW_TAG_typedef 0x12
#define DW_TAG_union_type 0x13
#define DW_TAG_volatile_type 0x14

struct Dwarf_Error_s
{
	Dwarf_Half _errno;
};

struct Dwarf_CU_Context
{
	Dwarf_Die_s *_die;
	Dwarf_Unsigned _CUheaderLength;
	Dwarf_Half _versionStamp;
	Dwarf_Off _abbrevOffset;
	Dwarf_Half _addressSize;
	Dwarf_Unsigned _nextCUheaderOffset;
	Dwarf_CU_Context *_nextCU;
	
	static vector<string> _fileList;
	static Dwarf_CU_Context *_firstCU;
	static Dwarf_CU_Context *_currentCU;
};
Dwarf_CU_Context * Dwarf_CU_Context::_firstCU;
Dwarf_CU_Context * Dwarf_CU_Context::_currentCU;
vector<string> Dwarf_CU_Context::_fileList;
struct Dwarf_Die_s
{
	Dwarf_Half _tag;
	Dwarf_Die_s *_parent;
	Dwarf_Die_s *_sibling;
#if defined(AIXPPC)
	Dwarf_Die_s *_previous;
#endif /* defined (AIXPPC) */
	Dwarf_Die_s *_child;
	Dwarf_CU_Context *_context;
	Dwarf_Attribute_s *_attribute;
	
	static unordered_map<Dwarf_Off, Dwarf_Die> refMap;
};
unordered_map<Dwarf_Off,Dwarf_Die> Dwarf_Die_s::refMap;
struct Dwarf_Attribute_s
{
	Dwarf_Half _type;
	Dwarf_Attribute_s *_nextAttr;
	Dwarf_Half _form;
	Dwarf_Signed _sdata;
	Dwarf_Unsigned _udata;
	char *_stringdata;
	Dwarf_Off _refdata;
	Dwarf_Die_s *_ref;
};

int dwarf_srcfiles(Dwarf_Die die, char ***srcfiles, Dwarf_Signed *filecount, Dwarf_Error *error);

int dwarf_attr(
	Dwarf_Die die,
	Dwarf_Half attr,
	Dwarf_Attribute *return_attr,
	Dwarf_Error *error);

char* dwarf_errmsg(Dwarf_Error error);

int dwarf_formstring(Dwarf_Attribute attr, char **returned_string, Dwarf_Error *error);

void dwarf_dealloc(Dwarf_Debug dbg, void *space, Dwarf_Unsigned type);

int dwarf_hasattr(Dwarf_Die die, Dwarf_Half   attr, Dwarf_Bool *returned_bool, Dwarf_Error *error);

int dwarf_formudata(Dwarf_Attribute attr, Dwarf_Unsigned  *returned_val, Dwarf_Error *error);

int dwarf_diename(Dwarf_Die die, char **diename, Dwarf_Error *error);

int dwarf_global_formref(Dwarf_Attribute attr, Dwarf_Off *return_offset, Dwarf_Error *error);

int dwarf_offdie_b(Dwarf_Debug dbg,
    Dwarf_Off offset,
    Dwarf_Bool is_info,
    Dwarf_Die *return_die,
    Dwarf_Error *error);

int dwarf_child(Dwarf_Die die, Dwarf_Die *return_childdie, Dwarf_Error *error);

int dwarf_whatform(Dwarf_Attribute attr, Dwarf_Half *returned_final_form, Dwarf_Error *error);

int dwarf_tag(Dwarf_Die die, Dwarf_Half *return_tag, Dwarf_Error *error);

int dwarf_formsdata(Dwarf_Attribute attr, Dwarf_Signed  *returned_val, Dwarf_Error *error);

int dwarf_next_cu_header(Dwarf_Debug dbg,
    Dwarf_Unsigned *cu_header_length,
    Dwarf_Half *version_stamp,
    Dwarf_Off *abbrev_offset,
    Dwarf_Half *address_size,
    Dwarf_Unsigned *next_cu_header_offset,
    Dwarf_Error *error);

int dwarf_siblingof(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Die *dieOut, Dwarf_Error *error);

int dwarf_finish(Dwarf_Debug dbg, Dwarf_Error *error);

int dwarf_init(int fd,
    Dwarf_Unsigned access,
    Dwarf_Handler errhand,
    Dwarf_Ptr errarg,
    Dwarf_Debug *dbg,
    Dwarf_Error *error);

void setError(Dwarf_Error *error, Dwarf_Half num);
