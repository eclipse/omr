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

#ifndef GENSUPERSET_HPP
#define GENSUPERSET_HPP

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

#include <set>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "ClassType.hpp"
#include "EnumMember.hpp"
#include "Field.hpp"
#include "Symbol_IR.hpp"

using std::string;
using std::set;
using std::stringstream;
#if defined(AIXPPC) || defined(J9ZOS390)
using std::tr1::unordered_map;
#else /* defined(AIXPPC) || defined(J9ZOS390) */
using std::unordered_map;
#endif /* !defined(AIXPPC) && !defined(J9ZOS390) */

class JavaSupersetGenerator: public SupersetGenerator
{
private:
	set<string> _baseTypedefSet; /* Set of types renamed to "[U/I][SIZE]" */
	unordered_map<string, string> _baseTypedefMap; /* Types remapped for assembled type names. */
	unordered_map<string, string> _baseTypedefReplace; /* Type names which are replaced everywhere. */
	set<string> _baseTypedefIgnore; /* Set of types to not rename when found as a typedef */
#if defined(AIXPPC)
	/* Keep a set of names of structures already printed, to avoid printing duplicates in the superset. 
	 * Currently, only use this approach for AIX, where removeDuplicates() runs too slowly. Using this
	 * method on other platforms has not been tested yet. There may still be issues related to differences
	 * in the DWARF/intermediate representation structures between platforms which may be revealed by this
	 * approach. Consequently, I have put the alternative duplicate removal code in #if defined statements.
	 */
	set<string> _printedTypes;
#endif /* defined(AIXPPC) */
	intptr_t _file;
	OMRPortLibrary *_portLibrary;

	void initBaseTypedefSet();
	void convertJ9BaseTypedef(Type *type, string *name);
	void replaceBaseTypedef(Type *type, string *name);
	DDR_RC getFieldType(Field *f, string *assembledTypeName, string *simpleTypeName);
	DDR_RC getTypeName(Field *f, string *typeName);
	string getUDTname(Type *type);
	DDR_RC printFieldMember(Field *field, string prefix);
	void printConstantMember(string name);

	string replace(string str, string subStr, string newStr);

public:
	JavaSupersetGenerator();

	DDR_RC printSuperset(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *supersetFile);

	DDR_RC dispatchPrintToSuperset(Type *type, bool addFieldsOnly, string prefix);
	DDR_RC dispatchPrintToSuperset(EnumUDT *type, bool addFieldsOnly, string prefix);
	DDR_RC dispatchPrintToSuperset(NamespaceUDT *type, bool addFieldsOnly, string prefix);
	DDR_RC dispatchPrintToSuperset(TypedefUDT *type, bool addFieldsOnly, string prefix);
	DDR_RC dispatchPrintToSuperset(ClassUDT *type, bool addFieldsOnly, string prefix);
	DDR_RC dispatchPrintToSuperset(UnionUDT *type, bool addFieldsOnly, string prefix);
};

#endif /* GENSUPERSET_HPP */
