/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef ELFGENERATOR_HPP
#define ELFGENERATOR_HPP

#if defined(LINUX)

#include <elf.h>
#include <string>
#include "env/TypedAllocator.hpp"
#include "env/RawAllocator.hpp"
#include "runtime/CodeCacheManager.hpp"

class TR_Memory;

namespace TR{

/**
 * ELFGenerator Abstract Base Class. This abstract base class provides
 * a way to share the commonalities between the building of different
 * kinds of ELF object files
*/
class ELFGenerator
{
public:
    /**
     * ELFGenerator constructor
     * @param[in] rawAllocator the TR::RawAllocator
     * @param[in] codeStart the code segment base
     * @param[in] codeSize the size of the code segment
    */
    ELFGenerator(TR::RawAllocator rawAllocator,
                 uint8_t const * codeStart, size_t codeSize):
                        _rawAllocator(rawAllocator),
                        _codeStart(codeStart),
                        _codeSize(codeSize),
                        _header(NULL),
                        _programHeader(NULL),
                        _zeroSection(NULL),
                        _textSection(NULL),
                        _dataSection(NULL),
                        _relaSection(NULL),
                        _dynSymSection(NULL),
                        _shStrTabSection(NULL),
                        _dynStrSection(NULL)
                        {
                        }
    
    /**
     * ELFGenerator destructor
    */
    ~ELFGenerator() throw(){};

protected:

#if defined(TR_TARGET_64BIT)
    typedef Elf64_Ehdr ELFEHeader;
    typedef Elf64_Shdr ELFSectionHeader;
    typedef Elf64_Phdr ELFProgramHeader;
    typedef Elf64_Addr ELFAddress;
    typedef Elf64_Sym  ELFSymbol;
    typedef Elf64_Rela ELFRela;
    typedef Elf64_Off  ELFOffset;
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define ELF_ST_VISIBILITY(visibility) ELF64_ST_VISIBILITY(visibility)
#define ELF_R_INFO(bind, type) ELF64_R_INFO(bind, type)
#define ELFClass ELFCLASS64;
#else
    typedef Elf32_Ehdr ELFEHeader;
    typedef Elf32_Shdr ELFSectionHeader;
    typedef Elf32_Phdr ELFProgramHeader;
    typedef Elf32_Addr ELFAddress;
    typedef Elf32_Sym  ELFSymbol;
    typedef Elf32_Rela ELFRela;
    typedef Elf32_Off  ELFOffset;
#define ELF_ST_INFO(bind, type) ELF32_ST_INFO(bind, type)
#define ELF_ST_VISIBILITY(visibility) ELF32_ST_VISIBILITY(visibility)
#define ELF_R_INFO(bind, type) ELF32_R_INFO(bind, type)
#define ELFClass ELFCLASS32;
#endif

    /**
     * Pure virtual function that should be implemented to handle how the header is set up
     * for different types of ELF objects
    */
    virtual void initialize(void) = 0;

    /**
     * Pure virtual function that should be implemented to initialize header members appropriately
    */
    virtual void initializeELFHeader(void) = 0;

    /**
     * Sets up arch and OS specific information, along with ELFEHeader's e_ident member
    */
    void initializeELFHeaderForPlatform(void);
    
    /**
     * This pure virtual method should be implemented by the derived classes to
     * correctly set up the trailer section and fill the CodeCacheSymbols to write
     * (plus other optional info such as CodeCacheRelocationInfo).
    */
    virtual void buildSectionHeaders(void) = 0;

    /**
     * Set up the trailer zero section
    */
    void initializeZeroSection();
    
    /**
     * Set up the trailer text section
     * @param[in] shName the section header name
     * @param[in] shAddress the section header address
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
    */
    void initializeTextSection(
                                uint32_t shName, 
                                ELFAddress shAddress,
                                ELFOffset shOffset, 
                                uint32_t shSize
                              );
    
    /**
     * Set up the trailer data section
     * @param[in] shName the section header name
     * @param[in] shAddress the section header address
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
    */
    void initializeDataSection(
                                uint32_t shName, 
                                ELFAddress shAddress,
                                ELFOffset shOffset, 
                                uint32_t shSize
                              );
    
    /**
     * Set up the trailer dynamic symbol section
     * @param[in] shName the section header name
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
     * @param[in] shLink the section header link
    */
    void initializeDynSymSection(
                                uint32_t shName, 
                                ELFOffset shOffset, 
                                uint32_t shSize, 
                                uint32_t shLink
                                );

    /**
     * Set up the trailer string table section
     * @param[in] shName the section header name
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
    */
    void initializeStrTabSection( 
                                uint32_t shName, 
                                ELFOffset shOffset,  
                                uint32_t shSize
                                );

    /**
     * Set up the trailer dynamic string section
     * @param[in] shName the section header name
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
    */
    void initializeDynStrSection(
                                uint32_t shName, 
                                ELFOffset shOffset, 
                                uint32_t shSize
                                );

    /**
     * Set up the trailer Rela section
     * @param[in] shName the section header name
     * @param[in] shOffset the section header offset
     * @param[in] shSize the section header size
    */
    void initializeRelaSection( 
                                uint32_t shName, 
                                ELFOffset shOffset, 
                                uint32_t shSize
                              );
    
    /**
     * Write the header, program header (if applicable), the different
     * section headers, code segment, ELFSymbols, and Rela entries to
     * file (if applicable). Calls helper methods for the different
     * parts of the ELF file. It can build both executable and
     * relocatable ELF files, mainly by checking if the program
     * header and _relocations are non-NULL
     * @param[in] fp the name of the file to write to
     * @return bool indicating whether opening file to write to was successful
     */
    bool emitELFFile(const char * filename);

    /**
     * Write the ELFEheader field to file
     * @param[in] fp the file stream ptr
     */
    void writeHeaderToFile(::FILE *fp);

    /**
     * Write the ELFProgramHeader field to file
     * @param[in] fp the file stream ptr
     */
    void writeProgramHeaderToFile(::FILE *fp);

    /**
     * Write the Section header provided to file
     * @param[in] fp the file stream ptr
     * @param[in] shdr the section header to write to file
     */
    void writeSectionHeaderToFile(::FILE *fp, ELFSectionHeader *shdr);

    /**
     * Write the section name chars to file
     * @param[in] fp the file stream ptr
     * @param[in] name the ptr to the first element of the char array
     * @param[in] size the size of the char array, which includes the null terminator
     */
    void writeSectionNameToFile(::FILE *fp, char * name, uint32_t size);

    /**
     * Write the code segment to file
     * @param[in] fp the file stream ptr
     */
    void writeCodeSegmentToFile(::FILE *fp);

    /**
     * Write the code segment to file
     * @param[in] fp the file stream ptr
     */
    void writeDataSegmentToFile(::FILE *fp);

    /**
     * Write the ELFSymbols to file
     * @param[in] fp the file stream ptr
     */
    void writeELFSymbolsToFile(::FILE *fp);

    /**
     * Write the ELFRela entries to file, applicable for relocatable ELF
     * @param[in] fp the file stream ptr
     */
    void writeRelaEntriesToFile(::FILE *fp);

    TR::RawAllocator _rawAllocator; /**< the RawAllocator passed to the constructor */
    
    ELFEHeader       *_header;          /**< The ELFEHeader, required for all ELF files */
    ELFProgramHeader *_programHeader;   /**< The ELFProgramHeader, required for executable ELF */

    /**< The section headers and the sectionheader names */
    ELFSectionHeader *_zeroSection;         
    char              _zeroSectionName[1];
    ELFSectionHeader *_textSection;
    char              _textSectionName[6];
    ELFSectionHeader *_dataSection;
    char              _dataSectionName[6];     
    ELFSectionHeader *_relaSection;
    char              _relaSectionName[11];
    ELFSectionHeader *_dynSymSection;
    char              _dynSymSectionName[8];
    ELFSectionHeader *_shStrTabSection;
    char              _shStrTabSectionName[10];
    ELFSectionHeader *_dynStrSection;
    char              _dynStrSectionName[8];

    uint32_t _elfTrailerSize;               /**< Size of the region of the ELF file that would be occupied by the trailer segment */
    uint32_t _elfHeaderSize;                /**< Size of the region of the ELF file that would be occupied by the header segment */
    uint32_t _totalELFSymbolNamesLength;    /**< Total length of the symbol names that would be written to file  */
    struct TR::CodeCacheSymbol *_symbols;   /**< Chain of CodeCacheSymbol structures to be written */
    uint32_t _numSymbols;                   /**< Number of symbols to be written */
    uint8_t const * const _codeStart;       /**< base of code segment */
    uint32_t _codeSize;                     /**< Size of the code segment */

    struct TR::CodeCacheRelocationInfo *_relocations; /**< Struct containing relocation info to be written */
    uint32_t _numRelocations;                         /**< Number of relocations to write to file */

}; //class ELFGenerator


/**
 * ELFExecutableGenerator class. Used for generating executable ELF objects
*/
class ELFExecutableGenerator : public ELFGenerator
{
public:

    /**
     * ELFExecutableGenerator constructor
     * @param[in] rawAllocator 
     * @param[in] codeStart the codeStart is the base of the code segment
     * @param[in] codeSize the size of the region of the code segment
    */
    ELFExecutableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize);

    /**
     * ELFExecutableGenerator destructor
    */
    ~ELFExecutableGenerator() throw()
    {
    }

protected:
    /**
     * Initializes header for executable ELF and calls helper methods
    */
    virtual void initialize(void);
    
    /**
     * Initializes ELF header struct members
    */
    virtual void initializeELFHeader(void);

    /**
     * Initializes ELF Program Header, required for executable ELF
    */
    virtual void initializePHdr(void);
    
    virtual void buildSectionHeaders(void);

public:

    /**
     * This function is called when it is time to write symbols to file.
     * This function calls helper functions that initializes section headers and
     * writes to file.
     * @param[in] filename the name of the file to write to
     * @param[in] symbols the TR::CodeCacheSymbol*
     * @param[in] numSymbols the number of symbols not including UNDEF
     * @param[in] totalELFSymbolNamesLength the sum of symbol name lengths + 1 for UNDEF
     * @return bool whether emitting ELF file succeeded
    */
    bool emitELF(const char * filename,
                CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength);

}; //class ELFExecutableGenerator


class ELFRelocatableGenerator : public ELFGenerator
{
public:
    ELFRelocatableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize);

    ~ELFRelocatableGenerator() throw()
    {
    }

protected:

    /**
     * Initializes header for relocatable ELF file
    */
    virtual void initialize(void);
    
    /**
     * Initializes header struct members for relocatable ELF
    */
    virtual void initializeELFHeader(void);

    /**
     * Initializes ELF Trailer struct members, with calls to helper methods
     * implemented by the parent class and then lays out the
     * symbols to be written in memory
    */
    virtual void buildSectionHeaders(void);

public:

    /**
     * This function is called when it is time to write symbols to file.
     * This function calls helper functions that initializes section headers and
     * writes to file.
     * @param[in] filename the name of the file to write to
     * @param[in] symbols the TR::CodeCacheSymbol*
     * @param[in] numSymbols the number of symbols not including UNDEF
     * @param[in] totalELFSymbolNamesLength the sum of symbol name lengths + 1 for UNDEF
     * @param[in] relocations the TR::CodeCacheRelocationInfo
     * @param[in] numRelocations the total number of relocations
     * @return bool whether emitting ELF file succeeded
    */
    bool emitELF(const char * filename,
                TR::CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength,
                TR::CodeCacheRelocationInfo *relocations,
                uint32_t numRelocations);

}; //class ELFRelocatableGenerator

} //namespace TR

#endif //LINUX

#endif //ifndef ELFGENERATOR_HPP
