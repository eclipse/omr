/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef DWARFSCANNER_HPP
#define DWARFSCANNER_HPP

#include "ddr/config.hpp"

#include "ddr/std/unordered_map.hpp"
#include <map>

#if defined(OSX) || defined(AIXPPC)
#include "ddr/scanner/dwarf/DwarfFunctions.hpp"
#else /* defined(OSX) || defined(AIXPPC) */

#if defined(HAVE_DWARF_H)
#include <dwarf.h>
#elif defined(HAVE_LIBDWARF_DWARF_H)
#include <libdwarf/dwarf.h>
#else
#error
#endif /* defined(HAVE_DWARF_H) */

#if defined(HAVE_LIBDWARF_H)
#include <libdwarf.h>
#elif defined(HAVE_LIBDWARF_LIBDWARF_H)
#include <libdwarf/libdwarf.h>
#else
#error
#endif

#endif /* defined(OSX) || defined(AIXPPX) */

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/config.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/scanner/Scanner.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TypedefUDT.hpp"

using std::make_pair;
using std::map;
using std::string;

#if defined(OMR_HAVE_TR1)
using std::tr1::hash;
#else
using std::hash;
#endif

class DwarfScanner: public Scanner
{
public:
	DwarfScanner();
	~DwarfScanner();

	DDR_RC startScan(OMRPortLibrary *portLibrary, Symbol_IR *const ir, vector<string> *debugFiles, string blacklistPath);

private:
	Dwarf_Signed _fileNameCount;
	char **_fileNamesTable;
	unordered_map<string, int> _anonymousEnumNames;
	unordered_map<Dwarf_Off, Type *> _typeOffsetMap;
	Symbol_IR *_ir;
	Dwarf_Debug _debug;

	DDR_RC scanFile(OMRPortLibrary *portLibrary, Symbol_IR *ir, const char *filepath);
	DDR_RC traverse_cu_in_debug_section(Symbol_IR *const ir);
	DDR_RC addDieToIR(Dwarf_Die die, Dwarf_Half tag, bool ignoreFilter, NamespaceUDT *outerUDT, Type **type);
	DDR_RC getOrCreateNewType(Dwarf_Die die, Dwarf_Half tag, Type **const newUDT, NamespaceUDT *outerUDT, bool *isNewType);
	DDR_RC createNewType(Dwarf_Die die, Dwarf_Half tag, string dieName, Type **const newUDT);
	DDR_RC scanClassChildren(NamespaceUDT *newClass, Dwarf_Die die, bool alreadyHadFields);
	DDR_RC addEnumMember(Dwarf_Die die, EnumUDT *const udt);
	DDR_RC addClassField(Dwarf_Die die, ClassType *const newClass, string fieldName);
	DDR_RC getSuperUDT(Dwarf_Die die, ClassUDT *const udt);
	DDR_RC getTypeInfo(Dwarf_Die die, Dwarf_Die *dieout, string *typeName, Modifiers *modifiers, size_t *typeSize, size_t *bitField);
	DDR_RC getTypeSize(Dwarf_Die die, size_t *typeSize);
	DDR_RC getTypeTag(Dwarf_Die die, Dwarf_Die *typedie, Dwarf_Half *tag);
	DDR_RC getSourcelist(Dwarf_Die die);
	DDR_RC blackListedDie(Dwarf_Die die, bool *dieBlackListed);
	DDR_RC getName(Dwarf_Die die, string *name, Dwarf_Off *dieOffset = NULL);
	DDR_RC getNextSibling(Dwarf_Die *die);
	DDR_RC getBitField(Dwarf_Die die, size_t *bitField);

	friend class DwarfVisitor;
};

#endif /* DWARFSCANNER_HPP */
