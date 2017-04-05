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

#ifndef DWARFSCANNER_HPP
#define DWARFSCANNER_HPP

#if defined(AIXPPC) || defined(J9ZOS390)
#define __IBMCPP_TR1__ 1
#include <unordered_map>
using std::tr1::unordered_map;
#undef __IBMCPP_TR1__
#else /* defined(AIXPPC) || defined(J9ZOS390) */
#include <unordered_map>
#endif /* !defined(AIXPPC) && !defined(J9ZOS390) */
#include <map>

#if defined(OSX) || defined(AIXPPC)
#include "DwarfFunctions.hpp"
#else /* defined(OSX) || defined(AIXPPC) */
#include <dwarf.h>
#include <libdwarf.h>
#endif /* defined(OSX) */

#include "ClassUDT.hpp"
#include "config.hpp"
#include "EnumUDT.hpp"
#include "../Scanner.hpp"
#include "Symbol_IR.hpp"
#include "TypedefUDT.hpp"

#if defined(AIXPPC) || defined(J9ZOS390)
using std::tr1::hash;
#else /* defined(AIXPPC) || defined(J9ZOS390) */
using std::hash;
#endif /* !defined(AIXPPC) && !defined(J9ZOS390) */
using std::map;
using std::string;
#if defined(AIXPPC) || defined(J9ZOS390)
using std::tr1::unordered_map;
#else /* defined(AIXPPC) || defined(J9ZOS390) */
using std::unordered_map;
#endif /* !defined(AIXPPC) && !defined(J9ZOS390) */

struct TypeKey {
	char *fileName;
	string typeName;
	int lineNumber;

	bool operator==(const TypeKey& other) const;
};

struct KeyHasher {
	size_t operator()(const TypeKey& key) const;
};

class DwarfScanner: public Scanner
{
public:
	DwarfScanner();
	~DwarfScanner();

	DDR_RC startScan(OMRPortLibrary *portLibrary, Symbol_IR *const ir, vector<string> *debugFiles);

	DDR_RC dispatchScanChildInfo(Type *newType, void *data);
	DDR_RC dispatchScanChildInfo(NamespaceUDT *newClass, void *data);
	DDR_RC dispatchScanChildInfo(EnumUDT *newEnum, void *data);
	DDR_RC dispatchScanChildInfo(TypedefUDT *newTypedef, void *data);
	DDR_RC dispatchScanChildInfo(ClassUDT *newTypedef, void *data);
	DDR_RC dispatchScanChildInfo(UnionUDT *newTypedef, void *data);

private:
	Dwarf_Signed _fileNameCount;
	char **_fileNamesTable;
	unordered_map<string, int> _anonymousEnumNames;
	unordered_map<TypeKey, Type *, KeyHasher> _typeMap;
	unordered_map<string, Type *> _typeStubMap;
	Symbol_IR *_ir;
	Dwarf_Debug _debug;

	DDR_RC scanFile(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *filepath);
	DDR_RC traverse_cu_in_debug_section(Symbol_IR *const ir);
	DDR_RC addType(Dwarf_Die die, Dwarf_Half tag, bool ignoreFilter, NamespaceUDT *outerUDT, Type **type);
	DDR_RC getType(Dwarf_Die die, Dwarf_Half tag, Type **const newUDT, NamespaceUDT *outerUDT, int *typeNum);
	DDR_RC createType(Dwarf_Die die, Dwarf_Half tag, string dieName, unsigned int lineNumber, Type **const newUDT);
	DDR_RC addEnumMember(Dwarf_Die die, EnumUDT *const udt);
	DDR_RC addClassField(Dwarf_Die die, ClassType *const newClass, string fieldName);
	DDR_RC getSuperUDT(Dwarf_Die die, ClassUDT *const udt);
	DDR_RC getTypeInfo(Dwarf_Die die, Dwarf_Die *dieout, string *typeName, Modifiers *modifiers, size_t *typeSize, size_t *bitField);
	DDR_RC getTypeSize(Dwarf_Die die, size_t *typeSize);
	DDR_RC getTypeTag(Dwarf_Die die, Dwarf_Die *typedie, Dwarf_Half *tag);
	DDR_RC getBlacklist(Dwarf_Die die);
	DDR_RC blackListedDie(Dwarf_Die die, bool *dieBlackListed);
	DDR_RC getName(Dwarf_Die die, string *name);
	DDR_RC createTypeKey(Dwarf_Die die, string typeName, TypeKey *key, unsigned int *lineNumber, string *fileName);
	DDR_RC getNextSibling(Dwarf_Die *die);
	DDR_RC getBitField(Dwarf_Die die, size_t *bitField);
};

#endif /* DWARFSCANNER_HPP */
