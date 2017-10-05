/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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
 ******************************************************************************/

/**************************************************************
 * NOTE: THIS FILE INTENTIONALLY DOES NOT HAVE INCLUDE GUARDS *
 **************************************************************/


#ifndef ENUM_NAME
#error "ENUM_NAME must be defined as the name of the enum to be created."
#endif

#ifndef ENUM_FILE
#error "ENUM_FILE must be defined as the path (a quoted string) to the file containing the enum definition."
#endif

// enum definition

#define ENUM_V(name, value) name = value
#define ENUM(name) name

enum ENUM_NAME {
#include ENUM_FILE
};

#undef ENUM
#undef ENUM_V

// map definition

#ifndef TABLE_GETTER_NAME
#define GET_TABLE_GETTER_NAME_2(base) get ## base ## Table
#define GET_TABLE_GETTER_NAME(base) GET_TABLE_GETTER_NAME_2(base)
#define TABLE_GETTER_NAME GET_TABLE_GETTER_NAME(ENUM_NAME)
#endif

#ifndef TABLE_TYPENAME
#define GET_TABLE_TYPENAME_2(base) base ## _t
#define GET_TABLE_TYPENAME(base) GET_TABLE_TYPENAME_2(base)
#define TABLE_TYPENAME GET_TABLE_TYPENAME(TABLE_GETTER_NAME)
#endif

#define ENUM_V(name, value) { #name, ENUM_NAME::name }
#define ENUM(name) { #name, ENUM_NAME::name }

typedef std::map<std::string, uint32_t> TABLE_TYPENAME;

static const TABLE_TYPENAME& TABLE_GETTER_NAME()
   {
   static const auto enumTable = TABLE_TYPENAME
      {
#include ENUM_FILE
      };
   return enumTable;
   }

#undef ENUM
#undef ENUM_V

// map clean up

#ifdef GET_TABLE_GETTER_NAME_2
#undef GET_TABLE_GETTER_NAME_2
#endif
#ifdef GET_TABLE_GETTER_NAME
#undef GET_TABLE_GETTER_NAME
#endif
#ifdef GET_TABLE_TYPENAME_2
#undef GET_TABLE_TYPENAME_2
#endif
#ifdef GET_TABLE_TYPENAME
#undef GET_TABLE_TYPENAME
#endif

#undef TABLE_TYPENAME
#undef TABLE_GETTER_NAME

// other clean up

#undef ENUM_FILE
#undef ENUM_NAME
