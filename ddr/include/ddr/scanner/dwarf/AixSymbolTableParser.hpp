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

#ifndef AixSymbolTableParser_HPP
#define AixSymbolTableParser_HPP

#include <sys/types.h>
#include <sys/errno.h>
#include "ddr/config.hpp"
#include "ddr/scanner/dwarf/DwarfFunctions.hpp"
#include <set>

typedef enum declFormat {
	NO_FORMAT, /* Default initial value */
	C_FORMAT,
	CPP_FORMAT
} declFormat;

typedef unordered_map<int, pair<Dwarf_Off, Dwarf_Die> > die_map;

/* PTR_SIZE varies for 32bit, 64bit */
const int PTR_SIZE = 8;

/* Number of built in types */
const unsigned int NUM_BUILT_IN_TYPES = 37;

const string BUILT_IN_TYPES[NUM_BUILT_IN_TYPES + 1] = {"", "int", "char", "short int", "long int", "unsigned char",
"", "unsigned short int", "unsigned int", "", "long unsigned int", "void", 
"float", "double", "long double", "", "bool", "", "", "stringptr",
"", "", "", "", "", "", "", "",
"", "", "", "long long int", "long long unsigned int", "", "",
"intptr_t","uintptr_t", "sizetype"};
const string BUILT_IN_TYPES_THAT_ARE_TYPEDEFS[NUM_BUILT_IN_TYPES + 1] = {"", "", "", "", "", "", "signed char",
"", "", "unsigned", "", "", "", "", "", "integer", "", "short real", "real", "", "character", "", "", "", "",
"", "", "integer*1", "integer*2", "integer*4", "wchar", "", "", "logical*8", "integer*8", "", "", ""
};
const int BUILT_IN_TYPE_SIZES[NUM_BUILT_IN_TYPES + 1] = {0, 32, 8, 16, 32, 8, 8, 16, 32, 32,
32, 0, 32, 64, 128, 32, 8, 32, 64, PTR_SIZE, 8, 8, 16, 32, 32, 64, 128,
8, 16, 32, 16, 64, 64, 64, 64, 64, 64, 64};

const string START_OF_FILE[3] = {"debug", "3", "FILE"};
const string FILE_NAME = "a0";
const string DECL_FILE[3] = {"debug", "0", "decl"};

int extractFileID(const string & data);
int extractTypeID(const string & data, int index = 1);
void split(const string & data, char delimeters, str_vect *elements);
str_vect split(const string & data, char delimeters);
string strip(const string & str, char chr);
string stripLeading(const string & str, char chr);
string stripTrailing(const string & str, char chr);
double toDouble(const string & value);
int toInt(const string & value);
size_t toSize(const string & size);
string toString(int value);

#endif /* AixSymbolTableParser_HPP */
