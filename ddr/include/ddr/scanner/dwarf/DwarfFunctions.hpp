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
#ifndef DWARFFUNCTIONS_HPP
#define DWARFFUNCTIONS_HPP

#include "ddr/config.hpp"

#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ddr/std/sstream.hpp"
#include <tuple>
#include "ddr/std/unordered_map.hpp"
#include <vector>
#include <errno.h>

#if defined(OSX)
#include <sys/fcntl.h>
#endif /* OSX */

#if defined(AIXPPC)
#include <unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#endif /* AIXPPC */

using std::make_pair;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

#if defined(OMR_HAVE_CXX11)
using std::get;
using std::make_tuple;
using std::tuple;
#else /* OMR_HAVE_CXX11 */
using std::tr1::get;
using std::tr1::make_tuple;
using std::tr1::tuple;
#endif /* OMR_HAVE_CXX11 */

struct Dwarf_Attribute_s;
struct Dwarf_Debug_s;
struct Dwarf_Die_s;
struct Dwarf_Error_s;

typedef struct Dwarf_Debug_s *Dwarf_Debug;
typedef struct Dwarf_Die_s *Dwarf_Die;
typedef struct Dwarf_Error_s *Dwarf_Error;
typedef struct Dwarf_Attribute_s *Dwarf_Attribute;

typedef unsigned char Dwarf_Small;
typedef int Dwarf_Bool;
typedef unsigned long long Dwarf_Off;
typedef unsigned long long Dwarf_Unsigned;
typedef unsigned short Dwarf_Half;
typedef signed long long Dwarf_Signed;
typedef unsigned long long Dwarf_Addr;

struct Dwarf_Block
{
	Dwarf_Small *bl_data;
	Dwarf_Unsigned bl_len;
};

typedef void *Dwarf_Ptr;
typedef void (*Dwarf_Handler)(Dwarf_Error error, Dwarf_Ptr errarg);

typedef vector<string> str_vect;

#define DW_DLE_NE             0x00 /* No error */
#define DW_DLE_ATTR_FORM_BAD  0x01 /* Wrong form for attribute */
#define DW_DLE_BADOFF         0x02 /* Invalid offset */
#define DW_DLE_DIE_NULL       0x03 /* Die NULL */
#define DW_DLE_FNO            0x04 /* File not open */
#define DW_DLE_IA             0x05 /* Invalid argument */
#define DW_DLE_IOF            0x06 /* I/O failure */
#define DW_DLE_MAF            0x07 /* Memory allocation failure */
#define DW_DLE_NOB            0x08 /* Not an object file */
#define DW_DLE_VMM            0x09 /* Dwarf format/library version mismatch */

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
#define DW_AT_external 0x0c
#define DW_AT_data_member_location 0xd
#define DW_AT_data_bit_offset 0x0e
#define DW_AT_declaration 0x0f

#define DW_DLA_ATTR 0x01
#define DW_DLA_DIE 0x02
#define DW_DLA_ERROR 0x03
#define DW_DLA_LIST 0x04
#define DW_DLA_STRING 0x05
#define DW_DLA_BLOCK 0x06

#define DW_FORM_unknown 0x00
#define DW_FORM_ref1 0x01
#define DW_FORM_ref2 0x02
#define DW_FORM_ref4 0x03
#define DW_FORM_ref8 0x04
#define DW_FORM_udata 0x05
#define DW_FORM_sdata 0x06
#define DW_FORM_string 0x07
#define DW_FORM_block  0x08
#define DW_FORM_block1 0x09
#define DW_FORM_block2 0x0a
#define DW_FORM_block4 0x0b
#define DW_FORM_flag 0x0c
#define DW_FORM_exprloc 0x18

#define DW_OP_plus_uconst 0x23
#define DW_OP_lit0 0x30
#define DW_OP_lit1 0x31
#define DW_OP_lit2 0x32
#define DW_OP_lit3 0x33
#define DW_OP_lit4 0x34
#define DW_OP_lit5 0x35
#define DW_OP_lit6 0x36
#define DW_OP_lit7 0x37
#define DW_OP_lit8 0x38
#define DW_OP_lit9 0x39
#define DW_OP_lit10 0x3a
#define DW_OP_lit11 0x3b
#define DW_OP_lit12 0x3c
#define DW_OP_lit13 0x3d
#define DW_OP_lit14 0x3e
#define DW_OP_lit15 0x3f
#define DW_OP_lit16 0x40
#define DW_OP_lit17 0x41
#define DW_OP_lit18 0x42
#define DW_OP_lit19 0x43
#define DW_OP_lit20 0x44
#define DW_OP_lit21 0x45
#define DW_OP_lit22 0x46
#define DW_OP_lit23 0x47
#define DW_OP_lit24 0x48
#define DW_OP_lit25 0x49
#define DW_OP_lit26 0x4a
#define DW_OP_lit27 0x4b
#define DW_OP_lit28 0x4c
#define DW_OP_lit29 0x4d
#define DW_OP_lit30 0x4e
#define DW_OP_lit31 0x4f

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
#define DW_TAG_ptr_to_member_type 0x0c
#define DW_TAG_restrict_type 0x0d
#define DW_TAG_reference_type 0x0e
#define DW_TAG_shared_type 0x0f
#define DW_TAG_structure_type 0x10
#define DW_TAG_subprogram 0x11
#define DW_TAG_subrange_type 0x12
#define DW_TAG_subroutine_type 0x13
#define DW_TAG_typedef 0x14
#define DW_TAG_union_type 0x15
#define DW_TAG_volatile_type 0x16
#define DW_TAG_unspecified_type 0x17

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

	static unordered_map<string, size_t> _fileId; /* key: file name, value: order added indexed at 0 */
	static Dwarf_CU_Context *_firstCU;
	static Dwarf_CU_Context *_currentCU;
};

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

struct Dwarf_Attribute_s
{
	Dwarf_Half _type;
	Dwarf_Attribute_s *_nextAttr;
	Dwarf_Half _form;
	Dwarf_Bool _flag;
	Dwarf_Signed _sdata;
	Dwarf_Unsigned _udata;
	char *_stringdata;
	Dwarf_Off _refdata;
	Dwarf_Die_s *_ref;

	Dwarf_Attribute_s()
		: _type(DW_AT_unknown)
		, _nextAttr(NULL)
		, _form(DW_FORM_unknown)
		, _flag(false)
		, _sdata(0)
		, _udata(0)
		, _stringdata(NULL)
		, _refdata(0)
		, _ref(NULL)
	{
	}
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

int dwarf_hasattr(Dwarf_Die die, Dwarf_Half attr, Dwarf_Bool *returned_bool, Dwarf_Error *error);

int dwarf_formblock(Dwarf_Attribute attr, Dwarf_Block **returned_block, Dwarf_Error *error);

int dwarf_formflag(Dwarf_Attribute attr, Dwarf_Bool *returned_flag, Dwarf_Error *error);

int dwarf_formudata(Dwarf_Attribute attr, Dwarf_Unsigned *returned_val, Dwarf_Error *error);

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

int dwarf_formsdata(Dwarf_Attribute attr, Dwarf_Signed *returned_val, Dwarf_Error *error);

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

int dwarf_dieoffset(Dwarf_Die die, Dwarf_Off *dieOffset, Dwarf_Error *error);

int dwarf_get_TAG_name(Dwarf_Half tag, const char **name);

void setError(Dwarf_Error *error, Dwarf_Half num);

#endif /*DWARFFUNCTIONS_HPP*/
