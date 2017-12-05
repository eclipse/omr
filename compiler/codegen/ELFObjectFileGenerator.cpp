/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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

#if defined(LINUX)

#include <stdio.h>
#include <string.h>
#include <unistd.h>                     // for getpid, pid_t
#include "codegen/ELFObjectFileGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

TR::ELFObjectFileGenerator::ELFObjectFileGenerator(TR::RawAllocator rawAllocator, const uint8_t * codeStart, size_t codeSize, const char * fileName) :
   _rawAllocator(rawAllocator),
   _codeStart(codeStart),
   _codeSize(codeSize),
   _fileNameLength(strlen(fileName)+1),
   _fileNameChars(static_cast<char *>(_rawAllocator.allocate(_fileNameLength))),
   _elfHeader(),
   _elfTrailer(NULL),
   _symbols(_rawAllocator),
   _relocations(_rawAllocator),
   _totalELFSymbolNamesLength(0)
   {
   strncpy(_fileNameChars, fileName, _fileNameLength);
   initializeELFHeader();
   }

TR::ELFObjectFileGenerator::~ELFObjectFileGenerator() throw()
   {
   _rawAllocator.deallocate(_fileNameChars);
   }

void
TR::ELFObjectFileGenerator::initializeELFHeader()
   {
   // main elf header
   _elfHeader.e_ident[EI_MAG0] = ELFMAG0;
   _elfHeader.e_ident[EI_MAG1] = ELFMAG1;
   _elfHeader.e_ident[EI_MAG2] = ELFMAG2;
   _elfHeader.e_ident[EI_MAG3] = ELFMAG3;
   _elfHeader.e_ident[EI_CLASS] = ELFClass;
   _elfHeader.e_ident[EI_VERSION] = EV_CURRENT;
   for (auto b = EI_PAD;b < EI_NIDENT;b++)
      _elfHeader.e_ident[b] = 0;

   initializeELFHeaderForPlatform();

   _elfHeader.e_type = ET_REL;
   _elfHeader.e_version = EV_CURRENT;

   _elfHeader.e_entry = 0;
   _elfHeader.e_phoff = 0;
   _elfHeader.e_shoff = _codeSize + sizeof(ELFHeader);
   _elfHeader.e_flags = 0;

   _elfHeader.e_ehsize = sizeof(ELFHeader);

   _elfHeader.e_phentsize = 0;
   _elfHeader.e_phnum = 0;
   _elfHeader.e_shentsize = sizeof(ELFSectionHeader);
   _elfHeader.e_shnum = 6; // number of sections in trailer
   _elfHeader.e_shstrndx = 4; // index to shared string table section in trailer
   }

void
TR::ELFObjectFileGenerator::initializeELFHeaderForPlatform()
   {
   _elfHeader.e_ident[EI_DATA] = TR::Compiler->host.cpu.isLittleEndian() ? ELFDATA2LSB : ELFDATA2MSB;

#if (HOST_OS == OMR_LINUX)
   _elfHeader.e_ident[EI_OSABI] = ELFOSABI_LINUX;
#elif (HOST_OS == OMR_AIX)
   _elfHeader.e_ident[EI_OSABI] = ELFOSABI_AIX;
#else
   TR_ASSERT(0, "unrecognized operating system: cannot initialize code cache elf header");
#endif

   _elfHeader.e_ident[EI_ABIVERSION] = 0;

#if (HOST_ARCH == ARCH_X86)
   #if defined(TR_TARGET_64BIT)
   _elfHeader.e_machine = EM_X86_64;
   #else
   _elfHeader.e_machine = EM_386;
   #endif
#elif (HOST_ARCH == ARCH_POWER)
   #if defined(TR_TARGET_64BIT)
   _elfHeader.e_machine = EM_PPC64;
   #else
   _elfHeader.e_machine = EM_PPC;
   #endif
#elif (HOST_ARCH == ARCH_ZARCH)
   _elfHeader.e_machine = EM_S390;
#elif (HOST_ARCH == ARCH_ARM)
   _elfHeader.e_machine = EM_ARM;
#else
   TR_ASSERT(0, "unrecognized architecture: cannot initialize code cache elf header");
#endif
   }

void
TR::ELFObjectFileGenerator::initializeELFTrailer()
   {
   _totalELFSymbolNamesLength = (_totalELFSymbolNamesLength+(sizeof(void*)-1)) & ~(sizeof(void*) - 1);
   _elfTrailerSize = sizeof(ELFTrailer) +
                     numELFSymbols() * sizeof(ELFSymbol) + // NOTE: ELFTrailer includes 1 ELFSymbol: UNDEF
                     _totalELFSymbolNamesLength +
                     numELFRelocations() * sizeof(ELFRela);
   ELFTrailer *trlr = static_cast<ELFTrailer *>(_rawAllocator.allocate(_elfTrailerSize));

   size_t trailerStartOffset = sizeof(ELFHeader) + _codeSize;
   size_t symbolsStartOffset = trailerStartOffset + offsetof(ELFTrailer, symbols);
   size_t symbolNamesStartOffset = symbolsStartOffset + (numELFSymbols()+1) * sizeof(ELFSymbol);
   size_t relaStartOffset = symbolNamesStartOffset + _totalELFSymbolNamesLength;

   trlr->zeroSection.sh_name = 0;
   trlr->zeroSection.sh_type = 0;
   trlr->zeroSection.sh_flags = 0;
   trlr->zeroSection.sh_addr = 0;
   trlr->zeroSection.sh_offset = 0;
   trlr->zeroSection.sh_size = 0;
   trlr->zeroSection.sh_link = 0;
   trlr->zeroSection.sh_info = 0;
   trlr->zeroSection.sh_addralign = 0;
   trlr->zeroSection.sh_entsize = 0;

   trlr->textSection.sh_name = trlr->textSectionName - trlr->zeroSectionName;
   trlr->textSection.sh_type = SHT_PROGBITS;
   trlr->textSection.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
   trlr->textSection.sh_addr = 0;
   trlr->textSection.sh_offset = sizeof(ELFHeader);
   trlr->textSection.sh_size = _codeSize;
   trlr->textSection.sh_link = 0;
   trlr->textSection.sh_info = 0;
   trlr->textSection.sh_addralign = 32; // code cache alignment?
   trlr->textSection.sh_entsize = 0;

   trlr->relaSection.sh_name = trlr->relaSectionName - trlr->zeroSectionName;
   trlr->relaSection.sh_type = SHT_RELA;
   trlr->relaSection.sh_flags = 0;
   trlr->relaSection.sh_addr = 0;
   trlr->relaSection.sh_offset = relaStartOffset;
   trlr->relaSection.sh_size = numELFRelocations() * sizeof(ELFRela);
   trlr->relaSection.sh_link = 3; // dynsymSection
   trlr->relaSection.sh_info = 1;
   trlr->relaSection.sh_addralign = 8;
   trlr->relaSection.sh_entsize = sizeof(ELFRela);

   trlr->dynsymSection.sh_name = trlr->dynsymSectionName - trlr->zeroSectionName;
   trlr->dynsymSection.sh_type = SHT_SYMTAB; // SHT_DYNSYM
   trlr->dynsymSection.sh_flags = 0; //SHF_ALLOC;
   trlr->dynsymSection.sh_addr = 0; //(ELFAddress) &((uint8_t *)_elfHeader + symbolStartOffset); // fake address because not continuous
   trlr->dynsymSection.sh_offset = symbolsStartOffset;
   trlr->dynsymSection.sh_size = (numELFSymbols() + 1)*sizeof(ELFSymbol);
   trlr->dynsymSection.sh_link = 5; // dynamic string table index
   trlr->dynsymSection.sh_info = 1; // index of first non-local symbol: for now all symbols are global
   trlr->dynsymSection.sh_addralign = 8;
   trlr->dynsymSection.sh_entsize = sizeof(ELFSymbol);

   trlr->shstrtabSection.sh_name = trlr->shstrtabSectionName - trlr->zeroSectionName;
   trlr->shstrtabSection.sh_type = SHT_STRTAB;
   trlr->shstrtabSection.sh_flags = 0;
   trlr->shstrtabSection.sh_addr = 0;
   trlr->shstrtabSection.sh_offset = trailerStartOffset + offsetof(ELFTrailer, zeroSectionName);
   trlr->shstrtabSection.sh_size = sizeof(trlr->zeroSectionName) +
                                   sizeof(trlr->shstrtabSectionName) +
                                   sizeof(trlr->textSectionName) +
                                   sizeof(trlr->relaSectionName) +
                                   sizeof(trlr->dynsymSectionName) +
                                   sizeof(trlr->dynstrSectionName);
   trlr->shstrtabSection.sh_link = 0;
   trlr->shstrtabSection.sh_info = 0;
   trlr->shstrtabSection.sh_addralign = 1;
   trlr->shstrtabSection.sh_entsize = 0;

   trlr->dynstrSection.sh_name = trlr->dynstrSectionName - trlr->zeroSectionName;
   trlr->dynstrSection.sh_type = SHT_STRTAB;
   trlr->dynstrSection.sh_flags = 0;
   trlr->dynstrSection.sh_addr = 0;
   trlr->dynstrSection.sh_offset = symbolNamesStartOffset;
   trlr->dynstrSection.sh_size = _totalELFSymbolNamesLength;
   trlr->dynstrSection.sh_link = 0;
   trlr->dynstrSection.sh_info = 0;
   trlr->dynstrSection.sh_addralign = 1;
   trlr->dynstrSection.sh_entsize = 0;

   trlr->zeroSectionName[0] = 0;
   strcpy(trlr->shstrtabSectionName, ".shstrtab");
   strcpy(trlr->textSectionName, ".text");
   strcpy(trlr->relaSectionName, ".rela.text");
   strcpy(trlr->dynsymSectionName, ".symtab");
   strcpy(trlr->dynstrSectionName, ".strtab");

   // now walk list of compiled code symbols building up the symbol names and filling in array of ELFSymbol structures
   ELFSymbol *elfSymbols = trlr->symbols + 0;
   char *elfSymbolNames = pointer_cast<char *>(elfSymbols + (numELFSymbols()+1));

   // first symbol is UNDEF symbol: all zeros, even name is zero-terminated empty string
   elfSymbolNames[0] = 0;
   elfSymbols[0].st_name = 0;
   elfSymbols[0].st_info = ELF_ST_INFO(0,STT_NOTYPE);
   elfSymbols[0].st_other = 0;
   elfSymbols[0].st_shndx = 0;
   elfSymbols[0].st_value = 0;
   elfSymbols[0].st_size = 0;

   ELFSymbol *elfSym = elfSymbols + 1;
   char *names = elfSymbolNames + 1;
   for (auto it = _symbols.begin(); it != _symbols.end(); ++it)
      {
      if (TR::Options::getCmdLineOptions()->getOption(TR_TraceObjectFileGeneration))
         {
         fprintf(stderr, "Writing elf symbol %d, name(%d) = %s\n", (elfSym - elfSymbols), it->_nameLength, it->_name);
         }
      memcpy(names, it->_name, it->_nameLength);

      elfSym->st_name = names - elfSymbolNames;
      elfSym->st_info = ELF_ST_INFO(STB_GLOBAL,STT_FUNC);
      elfSym->st_other = ELF64_ST_VISIBILITY(STV_DEFAULT);
      elfSym->st_shndx = it->_start ? 1 : SHN_UNDEF; // text section
      elfSym->st_value = it->_start ? static_cast<ELFAddress>(it->_start - _codeStart) : 0;
      elfSym->st_size = it->_size;

      names += it->_nameLength;
      elfSym++;
      }


   ELFRela *elfRela = pointer_cast<ELFRela *>(elfSymbolNames + _totalELFSymbolNamesLength);
   for (auto it = _relocations.begin(); it != _relocations.end(); ++it)
      {
      elfRela->r_offset = static_cast<ELFAddress>(it->_location - _codeStart);
      elfRela->r_info = ELF64_R_INFO(it->_symbol + 1, it->_type);
      elfRela->r_addend = 0;
      ++elfRela;
      }
   _elfTrailer = trlr;
   }

void
TR::ELFObjectFileGenerator::registerCompiledMethod(const char *sig, uint8_t *startPC, uint32_t codeSize)
   {
   size_t nameLength = strlen(sig) + 1;
   char *name = static_cast<char *>(_rawAllocator.allocate(nameLength * sizeof(char)));
   strncpy(name, sig, nameLength);
   _symbols.push_back(ELFObjectFileSymbol(name, nameLength, startPC, codeSize));
   _totalELFSymbolNamesLength += nameLength;
   }

void
TR::ELFObjectFileGenerator::registerStaticRelocation(const TR::StaticRelocation &relocation)
   {
   const char * const symbolName(relocation.symbol());
   size_t nameLength = strlen(symbolName) + 1;
   char *name = static_cast<char *>(_rawAllocator.allocate(nameLength * sizeof(char)));
   strncpy(name, symbolName, nameLength);
   _symbols.push_back(ELFObjectFileSymbol(name, nameLength, 0, 0));
   _totalELFSymbolNamesLength += nameLength;
   uint32_t symbolNumber = static_cast<uint32_t>(_symbols.size() - 1);
   uint32_t relocationType = _resolver.resolveRelocationType(relocation);
   _relocations.push_back(ELFObjectFileRelocation(relocation.location(), relocationType, symbolNumber));
   }

void
TR::ELFObjectFileGenerator::emitObjectFile()
   {
   initializeELFTrailer();

   ::FILE *elfFile = fopen(_fileNameChars, "wb");

   if (TR::Options::getCmdLineOptions()->getOption(TR_TraceObjectFileGeneration))
      {
      fprintf(stderr, "Writing ELF header at offset: %lu\n", ftell(elfFile));
      }

   fwrite(&_elfHeader, sizeof(uint8_t), sizeof(ELFHeader), elfFile);

   if (TR::Options::getCmdLineOptions()->getOption(TR_TraceObjectFileGeneration))
      {
      fprintf(stderr, "Writing ELF code area at offset: %lu\n", ftell(elfFile));
      }

   fwrite(static_cast<const void *>(_codeStart), sizeof(uint8_t), _codeSize, elfFile);

   if (TR::Options::getCmdLineOptions()->getOption(TR_TraceObjectFileGeneration))
      {
      fprintf(stderr, "Writing ELF trailer at offset: %lu\n", ftell(elfFile));
      }

   fwrite(_elfTrailer, sizeof(uint8_t), _elfTrailerSize, elfFile);

   fclose(elfFile);
   }

size_t
TR::ELFObjectFileGenerator::numELFSymbols()
   {
   return _symbols.size();
   }

size_t
TR::ELFObjectFileGenerator::numELFRelocations()
   {
   return _relocations.size();
   }

#endif /* defined(LINUX) */
