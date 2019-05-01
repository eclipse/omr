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
#include "codegen/ELFGenerator.hpp"

#if defined(LINUX)

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>
#include "env/CompilerEnv.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"

TR::ELFExecutableGenerator::ELFExecutableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            ELFGenerator(rawAllocator, codeStart, codeSize)
                            {
                                initialize();
                            }

TR::ELFRelocatableGenerator::ELFRelocatableGenerator(TR::RawAllocator rawAllocator,
                            uint8_t const * codeStart, size_t codeSize):
                            ELFGenerator(rawAllocator, codeStart, codeSize)
                            {
                                initialize();
                            }

void
TR::ELFGenerator::initializeELFHeaderForPlatform(void)
{
    _header->e_ident[EI_MAG0] = ELFMAG0;
    _header->e_ident[EI_MAG1] = ELFMAG1;
    _header->e_ident[EI_MAG2] = ELFMAG2;
    _header->e_ident[EI_MAG3] = ELFMAG3;
    _header->e_ident[EI_CLASS] = ELFClass;
    _header->e_ident[EI_VERSION] = EV_CURRENT;
    _header->e_ident[EI_ABIVERSION] = 0;
    _header->e_ident[EI_DATA] = TR::Compiler->target.cpu.isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;
    
    for (auto b = EI_PAD;b < EI_NIDENT;b++)
        _header->e_ident[b] = 0;
        _header->e_ident[EI_OSABI] = ELFOSABI_LINUX; // Current support for Linux only. AIX would use the macro ELFOSABI_AIX.
    
    if (TR::Compiler->target.cpu.isX86())
    {
        _header->e_machine = TR::Compiler->target.is64Bit() ? EM_X86_64 : EM_386;
    }
    else if (TR::Compiler->target.cpu.isPower())
    {
        _header->e_machine = TR::Compiler->target.is64Bit() ? EM_PPC64 : EM_PPC;
    }
    else if (TR::Compiler->target.cpu.isZ()){
        _header->e_machine = EM_S390;
    }
    else
    {
        TR_ASSERT(0, "Unrecognized architecture: Failed to initialize ELF Header!");
    }

    _header->e_version = EV_CURRENT;
    _header->e_flags = 0; //processor-specific flags associated with the file
    _header->e_ehsize = sizeof(ELFEHeader);
    _header->e_shentsize = sizeof(ELFSectionHeader);
}

void 
TR::ELFGenerator::initializeZeroSection()
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = 0;
    shdr->sh_type = 0;
    shdr->sh_flags = 0;
    shdr->sh_addr = 0;
    shdr->sh_offset = 0;
    shdr->sh_size = 0;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 0;
    shdr->sh_entsize = 0;

    _zeroSection = shdr;
    _zeroSectionName[0] = 0;
}
    
void 
TR::ELFGenerator::initializeTextSection(uint32_t shName, ELFAddress shAddress,
                                                 ELFOffset shOffset, uint32_t shSize)
{

    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_PROGBITS;
    shdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdr->sh_addr = shAddress;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 32;
    shdr->sh_entsize = 0;

    _textSection = shdr;
    strcpy(_textSectionName, ".text");
}
    
void
TR::ELFGenerator::initializeDynSymSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize, uint32_t shLink)
{
    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_SYMTAB; // SHT_DYNSYM
    shdr->sh_flags = 0; //SHF_ALLOC;
    shdr->sh_addr = 0; //// fake address because not continuous
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = shLink; // dynamic string table index
    shdr->sh_info = 1; // index of first non-local symbol: for now all symbols are global
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;
    shdr->sh_entsize = sizeof(ELFSymbol);

    _dynSymSection = shdr;
    strcpy(_dynSymSectionName, ".symtab");
}

void
TR::ELFGenerator::initializeStrTabSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{    
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));
    
    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = 0;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 1;
    shdr->sh_entsize = 0;

    _shStrTabSection = shdr;
    strcpy(_shStrTabSectionName, ".shstrtab");
}

void
TR::ELFGenerator::initializeDynStrSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_STRTAB;
    shdr->sh_flags = 0;
    shdr->sh_addr = 0;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 0;
    shdr->sh_info = 0;
    shdr->sh_addralign = 1;
    shdr->sh_entsize = 0;

    _dynStrSection = shdr;
    strcpy(_dynStrSectionName, ".dynstr");
}

void
TR::ELFGenerator::initializeRelaSection(uint32_t shName, ELFOffset shOffset, uint32_t shSize)
{
    ELFSectionHeader * shdr = static_cast<ELFSectionHeader *>(_rawAllocator.allocate(sizeof(ELFSectionHeader)));

    shdr->sh_name = shName;
    shdr->sh_type = SHT_RELA;
    shdr->sh_flags = 0;
    shdr->sh_addr = 0;
    shdr->sh_offset = shOffset;
    shdr->sh_size = shSize;
    shdr->sh_link = 3; // dynsymSection index in the elf file. Kept this hardcoded for now as this shdr applies to relocatable only
    shdr->sh_info = 1;
    shdr->sh_addralign = TR::Compiler->target.is64Bit() ? 8 : 4;
    shdr->sh_entsize = sizeof(ELFRela);

    _relaSection = shdr;
    strcpy(_relaSectionName, ".rela.text");
}

bool
TR::ELFGenerator::emitELFFile(const char * filename)
{

    ::FILE *elfFile = fopen(filename, "wb");

    if (NULL == elfFile)
    {
        return false;
    }

    uint32_t size = 0;

    writeHeaderToFile(elfFile);

    if (_programHeader)
    {
        writeProgramHeaderToFile(elfFile);
    }

    writeCodeSegmentToFile(elfFile);

    writeSectionHeaderToFile(elfFile, _zeroSection);

    writeSectionHeaderToFile(elfFile, _textSection);

    if (_relaSection)
    {
        writeSectionHeaderToFile(elfFile, _relaSection);
    }

    writeSectionHeaderToFile(elfFile, _dynSymSection);

    writeSectionHeaderToFile(elfFile, _shStrTabSection);

    writeSectionHeaderToFile(elfFile, _dynStrSection);

    writeSectionNameToFile(elfFile, _zeroSectionName, sizeof(_zeroSectionName));

    writeSectionNameToFile(elfFile, _textSectionName, sizeof(_textSectionName));
    if (_relaSection)
    {
        writeSectionNameToFile(elfFile, _relaSectionName, sizeof(_relaSectionName));
    }
    writeSectionNameToFile(elfFile, _dynSymSectionName, sizeof(_dynSymSectionName));

    writeSectionNameToFile(elfFile, _shStrTabSectionName, sizeof(_shStrTabSectionName));

    writeSectionNameToFile(elfFile, _dynStrSectionName, sizeof(_dynStrSectionName));
    
    writeELFSymbolsToFile(elfFile);
    if(_relaSection)
    {
        writeRelaEntriesToFile(elfFile);
    }

    fclose(elfFile);
    
    return true;
}


void
TR::ELFGenerator::writeHeaderToFile(::FILE *fp)
{
    fwrite(_header, sizeof(uint8_t), sizeof(ELFEHeader), fp);
}

void 
TR::ELFGenerator::writeProgramHeaderToFile(::FILE *fp)
{
    fwrite(_programHeader, sizeof(uint8_t), sizeof(ELFProgramHeader), fp);
}

void 
TR::ELFGenerator::writeSectionHeaderToFile(::FILE *fp, ELFSectionHeader *shdr)
{
    fwrite(shdr, sizeof(uint8_t), sizeof(ELFSectionHeader), fp);
}

void 
TR::ELFGenerator::writeSectionNameToFile(::FILE *fp, char * name, uint32_t size)
{
    fwrite(name, sizeof(uint8_t), size, fp);
}

void 
TR::ELFGenerator::writeCodeSegmentToFile(::FILE *fp)
{
    fwrite(static_cast<const void *>(_codeStart), sizeof(uint8_t), _codeSize, fp);
}

void 
TR::ELFGenerator::writeELFSymbolsToFile(::FILE *fp)
{
    
    ELFSymbol * elfSym = static_cast<ELFSymbol*>(_rawAllocator.allocate(sizeof(ELFSymbol)));
    char ELFSymbolNames[_totalELFSymbolNamesLength];
    
    /* Writing the UNDEF symbol*/
    elfSym->st_name = 0;
    elfSym->st_info = 0;
    elfSym->st_other = 0;
    elfSym->st_shndx = 0;
    elfSym->st_value = 0;
    elfSym->st_size = 0;
    fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
    
    ELFSymbolNames[0] = 0; //first bit needs to be 0, corresponding to the name of the UNDEF symbol
    char * names = ELFSymbolNames + 1; //the rest of the array will contain method names
    TR::CodeCacheSymbol *sym = _symbols; 
    

    const uint8_t* rangeStart; //relocatable elf files need symbol offset from segment base
    if (_relaSection)
    {
        rangeStart = _codeStart;    
    } 
    else 
    {
        rangeStart = 0;
    }
    
    //values that are unchanged are being kept out of the while loop
    elfSym->st_other = ELF_ST_VISIBILITY(STV_DEFAULT);
    elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
    /* this while loop re-uses the ELFSymbol and writes
       CodeCacheSymbol info into file */
    while (sym)
    {
        memcpy(names, sym->_name, sym->_nameLength);
        elfSym->st_name = names - ELFSymbolNames; 
        elfSym->st_shndx = sym->_start ? 1 : SHN_UNDEF; // text section
        elfSym->st_value = sym->_start ? (ELFAddress)(sym->_start - rangeStart) : 0;
        elfSym->st_size = sym->_size;
        
        fwrite(elfSym, sizeof(uint8_t), sizeof(ELFSymbol), fp);
        
        names += sym->_nameLength;
        sym = sym->_next;
    }
    /* Finally, write the symbol names to file */
    fwrite(ELFSymbolNames, sizeof(uint8_t), _totalELFSymbolNamesLength, fp);

    _rawAllocator.deallocate(elfSym);
}

void 
TR::ELFGenerator::writeRelaEntriesToFile(::FILE *fp)
{
    if(_numRelocations > 0)
    {
        ELFRela * elfRelas = 
            static_cast<ELFRela*>(_rawAllocator.allocate(sizeof(ELFRela)));
        elfRelas->r_addend = 0; //addends are always 0, so it is kept out of the while loop
        TR::CodeCacheRelocationInfo *reloc = _relocations;
        /* this while loop re-uses the ELFRela and writes
            CodeCacheSymbol info into file */
        while (reloc)
            {
            elfRelas->r_offset = (ELFAddress)(reloc->_location - _codeStart);
            elfRelas->r_info = ELF_R_INFO(reloc->_symbol+1, reloc->_type);
            
            fwrite(elfRelas, sizeof(uint8_t), sizeof(ELFRela), fp);
            
            reloc = reloc->_next;
            }
        
        _rawAllocator.deallocate(elfRelas);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void
TR::ELFExecutableGenerator::initialize(void)
{
    ELFEHeader *hdr =
        static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
        std::nothrow));
        _header = hdr;
    
    ELFProgramHeader *phdr =
        static_cast<ELFProgramHeader *>(_rawAllocator.allocate(sizeof(ELFProgramHeader),
        std::nothrow));
    
    _programHeader = phdr;

    initializeELFHeader();

    initializeELFHeaderForPlatform();

    initializePHdr();
}

void
TR::ELFExecutableGenerator::initializeELFHeader(void)
{
    _header->e_type = ET_EXEC;
    _header->e_entry = (ELFAddress) _codeStart; //virtual address to which the system first transfers control
    _header->e_phoff = sizeof(ELFEHeader); //program header offset
    _header->e_shoff = sizeof(ELFEHeader) + sizeof(ELFProgramHeader) + _codeSize; //section header offset
    _header->e_phentsize = sizeof(ELFProgramHeader);
    _header->e_phnum = 1; // number of ELFProgramHeaders
    _header->e_shnum = 5; // number of sections in trailer
    _header->e_shstrndx = 3; // index of section header string table in trailer
}

void
TR::ELFExecutableGenerator::initializePHdr(void)
{
    _programHeader->p_type = PT_LOAD; //should be loaded in memory
    _programHeader->p_offset = sizeof(ELFEHeader); //offset of program header from the first byte of file to be loaded
    _programHeader->p_vaddr = (ELFAddress) _codeStart; //virtual address to load into
    _programHeader->p_paddr = (ELFAddress) _codeStart; //physical address to load into
    _programHeader->p_filesz = _codeSize; //in-file size
    _programHeader->p_memsz = _codeSize; //in-memory size
    _programHeader->p_flags = PF_X | PF_R; // should add PF_W if we get around to loading patchable code
    _programHeader->p_align = 0x1000;
}

void
TR::ELFExecutableGenerator::buildSectionHeaders(void)
{
    uint32_t shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName);
    
    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + sizeof(ELFProgramHeader) +
                                  _codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset + 
                                  (sizeof(ELFSectionHeader) * /* #shdr */ 5) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset +
                                      ((_numSymbols + /* UNDEF */ 1) * sizeof(ELFSymbol));
    uint32_t shNameOffset = 0;

    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeTextSection(shNameOffset,
                          (ELFAddress) _codeStart,
                          sizeof(ELFEHeader) + sizeof(ELFProgramHeader), 
                          _codeSize);
    shNameOffset += sizeof(_textSectionName);

    initializeDynSymSection(shNameOffset,
                            symbolsStartOffset, 
                            symbolNamesStartOffset - symbolsStartOffset,
                            /* Index of dynStrTab */ 4);
    shNameOffset += sizeof(_dynSymSectionName);
    
    initializeStrTabSection(shNameOffset, 
                            symbolsStartOffset - shStrTabNameLength, 
                            shStrTabNameLength);
    shNameOffset += sizeof(_shStrTabSectionName);

    initializeDynStrSection(shNameOffset, 
                            symbolNamesStartOffset, 
                            _totalELFSymbolNamesLength);
    shNameOffset += sizeof(_dynStrSectionName);
}

bool
TR::ELFExecutableGenerator::emitELF(const char * filename,
                CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength)
{
    _symbols = symbols;
    _numSymbols = numSymbols;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;
    
    buildSectionHeaders();
    return emitELFFile(filename);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void
TR::ELFRelocatableGenerator::initialize(void)
{
    ELFEHeader *hdr =
        static_cast<ELFEHeader *>(_rawAllocator.allocate(sizeof(ELFEHeader),
        std::nothrow));
    _header = hdr;
    
    initializeELFHeader();
    
    initializeELFHeaderForPlatform();
}

void
TR::ELFRelocatableGenerator::initializeELFHeader(void)
{
    _header->e_type = ET_REL;           
    _header->e_entry = 0; //no associated entry point for relocatable ELF files
    _header->e_phoff = 0; //no program header for relocatable files
    _header->e_shoff = sizeof(ELFEHeader) + _codeSize; //start of the section header table in bytes from the first byte of the ELF file
    _header->e_phentsize = 0; //no program headers in relocatable elf
    _header->e_phnum = 0;
    _header->e_shnum = 6;
    _header->e_shstrndx = 4; //index of section header string table
}

void
TR::ELFRelocatableGenerator::buildSectionHeaders(void)
{    
    uint32_t shStrTabNameLength = sizeof(_zeroSectionName) +
                                  sizeof(_shStrTabSectionName) +
                                  sizeof(_textSectionName) +
                                  sizeof(_relaSectionName) +
                                  sizeof(_dynSymSectionName) +
                                  sizeof(_dynStrSectionName);

    /* offset calculations */
    uint32_t trailerStartOffset = sizeof(ELFEHeader) + _codeSize;
    uint32_t symbolsStartOffset = trailerStartOffset +
                                  (sizeof(ELFSectionHeader) * /* # shdr */ 6) +
                                  shStrTabNameLength;
    uint32_t symbolNamesStartOffset = symbolsStartOffset + 
                                      (_numSymbols + /* UNDEF */ 1) * sizeof(ELFSymbol);
    uint32_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;
    uint32_t shNameOffset = 0;

    initializeZeroSection();
    shNameOffset += sizeof(_zeroSectionName);

    initializeTextSection(shNameOffset,
                          /*sh_addr*/ 0,
                          sizeof(ELFEHeader),
                          _codeSize);
    shNameOffset += sizeof(_textSectionName);

    initializeRelaSection(shNameOffset,
                          relaStartOffset, 
                          _numRelocations * sizeof(ELFRela));
    shNameOffset += sizeof(_relaSectionName);

    initializeDynSymSection(shNameOffset, 
                            symbolsStartOffset,
                            symbolNamesStartOffset - symbolsStartOffset,
                            /*Index of dynStrTab*/ 5);
    shNameOffset += sizeof(_dynSymSectionName);

    initializeStrTabSection(shNameOffset, 
                            symbolsStartOffset - shStrTabNameLength, 
                            shStrTabNameLength);
    shNameOffset += sizeof(_shStrTabSectionName);

    initializeDynStrSection(shNameOffset,
                            symbolNamesStartOffset, 
                            _totalELFSymbolNamesLength);
    shNameOffset += sizeof(_dynStrSectionName);
}

bool 
TR::ELFRelocatableGenerator::emitELF(const char * filename,
                CodeCacheSymbol *symbols, uint32_t numSymbols,
                uint32_t totalELFSymbolNamesLength,
                CodeCacheRelocationInfo *relocations,
                uint32_t numRelocations)
{
    _symbols = symbols;
    _relocations = relocations;
    _numSymbols = numSymbols;
    _numRelocations = numRelocations;
    _totalELFSymbolNamesLength = totalELFSymbolNamesLength;

    buildSectionHeaders();
    
    return emitELFFile(filename);
}

#endif //LINUX
