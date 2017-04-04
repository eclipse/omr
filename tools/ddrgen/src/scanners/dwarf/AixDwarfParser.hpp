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
#include "DwarfFunctions.hpp"

const string START_OF_FILE[3] = {"debug", "3", "FILE"};
const string FILE_NAME = "a0";
const string DECL_FILE[3]= {"debug", "0", "decl"};
const string END_OF_FILE = "**No Symbol**";

typedef enum declFormat {
	OLD_FORMAT, /* C format */
	NEW_FORMAT /* C++ format */
} declFormat;

double extractFileID(const string data);

double extractTypeID(const string data, const int index = 1);

bool isCompilerGenerated (const string data);

void split(const string data, char delimeters, str_vect *elements);

str_vect split(const string data, char delimeters);

string strip(const string str, const char chr);

string stripLeading(const string str, const char chr);

string stripTrailing(const string str, const char chr);

double shiftDecimal(const double value);

double toDouble(const string value);

int toInt(const string value);

size_t toSize(const string size);

string toString(const double value);
