/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * \file
 * \ingroup Port
 * \brief Dump formatting support internals unique to z/TPF
 */

#define ZTPF_NEEDS_GETCONTEXT

#include <tpf/tpf_limits.h>                /* For PATH_MAX            */
#include <tpf/i_ecb3.h>                    /* Page 3, ECB            */
#include <tpf/c_deri.h>                    /* SERRC info record    */
#include <sys/stat.h>                      /* For chmod().            */
#include <elf.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <tpf/c_cinfc.h>
#include <tpf/c_idsprg.h>
#include <tpf/tpfapi.h>
#include <atoe.h>

#include "portnls.h"
#include "omrosdump_helpers.h"
#include "safe_storage.h"

extern void translateInterruptContexts(args *argv) __attribute__((nonnull));
extern void masterSynchSignalHandler(int nbr, siginfo_t *siginfo,
		void *ucontext, uintptr_t bear);

#define    KEY0(a)    do { cinfc(CINFC_WRITE, CINFC_CMMJDB); } while(0)
#define    UNKEY(a) do { keyrc(); } while(0)

char javaPgmsToDump[][5] = { /* clean programs */ "DJHK", "DJNC", "DJVB", "DJCK", "DJHS",
		"DJPR", "DJVM", "DJDD", "DJHT", "DJRD", "DJZL", "DJDP", "DJHV", "DJSG",
		"D9VM", "DJDY", "DJHY", "DJSH", "DJ7B", "DJGC", "DJIT", "DJTH", "DJAR",
		"DJGK", "DJMT", "DJTR", "DJFF", "DJXT", "DJOR", /* dirty programs */ "DJVA",
		"DJRM", "DJNM", "DJAT", "DJAW", "DJAH", "DJDB", "DJDC", "DJDS", "DJFM",
		"DJHP", "DJIN", "DJAV", "DJCD", "DJJW", "DJWP", "DJLI", "DJPG", "DJSD",
		"DJSN", "DJKC", "DJLC", "DJMN", "DJML", "DJNT", "DJIO", "DJPT", "DJTK",
		"DJUN", "DJVR", "DJWR", "DJZP"};

/**
 * The ELF program header permission bits for read, write, and execute combined
 */
#define PT_RWX 7

/**
 * Size, in bytes, of the ELF NOTE region for z/Architecture machines
 */
#define NOTE_TABLE_SIZE 1020L

/*
 *      Non-exportable function prototypes
 */
static uint8_t* splitPathName(char *finalname, char *pathname);
static void buildELFHeader(Elf64_Ehdr *buffer);
static uint16_t buildELFPheaderBlk(Elf64_Phdr *buffer, DBFHDR *dbfHdr, uintptr_t phnum,
		uint64_t * numBytes);
static uintptr_t buildELFNoteArea(Elf64_Nhdr *buffer, DIB *dibPtr);
static void errMsg(args *argv, uint8_t *message, ...) __attribute__((nonnull(1,2)));
static uint64_t writeDumpFile(int32_t fd, uint8_t *buffer, uint64_t bytes);

/*
 * This function does the output to file, and returns the total count
 * of bytes written. It should be equal to the <bytes> argument.
 *
 * In the event of error, it will return ULONG_MAX.
 */
static uint64_t
writeDumpFile(int32_t fd, uint8_t *buffer, uint64_t bytes)
{
	int64_t bytesWrittenOnce = 0;
	uint64_t bytesWrittenTotal = 0;
	uint64_t bytesLeft = bytes;
	uint8_t *next = buffer;
	uint64_t chunkSize = PAGE_SIZE;

	while (bytesLeft) {
		if (bytesLeft < chunkSize) {
			chunkSize = bytesLeft;
		}
		bytesWrittenOnce = write(fd, next, chunkSize);
		if (-1 == bytesWrittenOnce) {
			return -1L;
		}
		next += bytesWrittenOnce;
		bytesWrittenTotal += (uint64_t) bytesWrittenOnce;
		bytesLeft -= (uint64_t) bytesWrittenOnce;
	}
	return bytesWrittenTotal;
}

static void
errMsg(args *argv, uint8_t *message, ...)
{
	va_list plist;

	va_start(plist, message);
	vsprintf((char *)workSpace(argv), (char *)message, plist);
	va_end(plist);
	dumpFlags(argv) |= J9TPF_ERRMSG_IN_WKSPC;
	return;
}

/**
 * \internal Calculate the distance between adjacent memory segments.
 *
 * \retval 0    These items are adjacent
 * \retval nz    These items are not adjacent, the distance in bytes between them.
 */
static void
calcGap(DBFITEM *curr, DBFITEM *next, uint64_t *gap)
{
	uint64_t distance = next->ijavsad - curr->ijavsad;
	*gap = distance - curr->ijavsiz;
	return;
}

/**
 * \internal Make sure the DBF table, which points to all the memory regions collected
 * by CCCPSE, is as compact as possible. The goal is to cut the number of ELF
 * Phdrs down as much as possible by merging adjoining memory regions. In order to do
 * this, it needs to mark each DBFITEM, which is in protected memory, SPK=0. This 
 * function is therefore in SPK=0 for its duration, and restores SPK1 prior to return.
 *
 * \param[in] dbfHdr Address of the Java dump buffer as returned from CCCPSE.
 *
 * \return
 *      The number of program headers after merge
 */
static uintptr_t
adjustDBFTable(DBFHDR *dbfHdr)
{
	DBFITEM *dbfTbl = (DBFITEM *) dbfHdr + 1; /* DBTI[0] starts right after dbfHdr    */
	DBFITEM *pred = dbfTbl; /* Predecessor DBTI                        */
	/*
	 * For first pass only, set pred = curr. We never want to address zero.
	 * We'll use it only to test bitfields which aren't possible to be set
	 * for the very first item.
	 */
	DBFITEM *curr = dbfTbl; /* Current DBTI under examination        */
	DBFITEM *succ = pred + 1; /* Successor DBTI                        */
	DBFITEM *limit = dbfTbl + (dbfHdr->ijavcnt); /* DBTI[n+1] address                */
	uintptr_t phnum = 0; /* Number of expected Elf64_Phdrs        */
	uintptr_t wGap; /* ACCUMULATOR: total ibc gap            */
	void *wVstart; /* WORK: new p_vaddr                    */
	IDATA chunkSize = 0L; /* Semi-permanent per set ... size        */
	IDATA totalIBCSize = 0L; /* Total sizes of all writeable sets    */

	KEY0(); /* JDB's in SPK=0, get auth to write it.*/
	while (curr < limit) {
		memset(DBTITYPE, 0, sizeof(char) + (sizeof(DBTIGAP)));/* Clear DBTI tail            */
		calcGap(curr, succ, &wGap); /* Get GAP size.                        */

		if (0 == wGap) {
			/*
			 *    This pair qualifies to be combined
			 */
			if (PSETSTRT || PSETMIDL) {
				CSETMIDL = TRUE; /* It is either mid-set ...                    */
			} else { /*  ... or starts a new set.                */
				CSETSTRT = TRUE;
				wVstart = DBTIVADDR; /* If it's a start, hold the start vaddr.   */
				chunkSize += DBTISIZ;
				DBTIGAP = wGap;
			}
		} else {
			/*
			 *    This pair does not qualify to be combined. Is it last in set, or is its
			 *    predecessor not part of this one?
			 */
			if (PSETMIDL || PSETSTRT) { /*    It's the end of a set, so summarize        */
				CSETLAST = TRUE; /*     the set's start adrs & size in this    */
				chunkSize += DBTISIZ; /*     JDB index item, it's the one we'll        */
				DBTISIZ += chunkSize; /*     be building the Elf64_Phdr from.        */
				chunkSize = 0L;
				DBTIVADDR = wVstart; /*    Reset starting address & size            */
				wVstart = 0L; /*     accumulators.                            */
			} else {
				CSETSOLO = TRUE; /*    This one is a standalone item.            */
			}
			/*
			 * We will only write Elf64_Phdr items for those DBF items marked CSETLAST
			 * or CSETSOLO.
			 */
			if (CSETLAST || CSETSOLO) {
				phnum += 1; /* We will write a Phdr because of this item*/
			}
		}
		DBTIGAP = wGap; /* Save the gap for later analysis            */
		pred = curr; /* Advance our pair pointers                */
		curr = succ;
		succ += 1;
	}
	UNKEY(); /* Restore SPK=1                            */
	return phnum; /* Return revised count to caller.            */
}

/*
 * \internal   Calculate the path to where the dump should be written, 
 *               isolating it into the <TT>fullpath</TT> parameter, and
 *               placing the filename portion of the path string into the
 *               <TT>filename</TT> argument.
 *
 * \param[in]  finalname    Buffer containing full path of the final filename. (const)
 * \param[out] pathname        Ptr to user-acquired buffer of length PATH_MAX for the path.
 *
 * \retval        Address of full path, also copied into <TT>pathname</TT>.
 *                This value ends in a path separator.
 */
static uint8_t *
splitPathName(char *finalname, char *pathname)
{
	char tempPath[PATH_MAX];
	char *lastSep = finalname ? strrchr(finalname, DIR_SEPARATOR) : NULL;

	/*
	 *    The <fullpath> parameter is the final filename the JVM has selected
	 *    from the filled-out templated name that the user gave us. It also
	 *    has a path preceding the basename; which was selected from the search
	 *    hierarchy picked out by the dump agent according to its rules. Use
	 *    this path for the OS dump filename. Eventually, it will be renamed
	 *    to the final JVM-created name; but the idea is not to change paths -
	 *    this way, we are reassured that we never have to copy the file's
	 *    data between paths; we only change the directory entry's name.
	 */
	if (NULL != lastSep) {
		/*
		 * fullpath starts with a pathname: split it down to the directory name
		 * and the filename, making sure to preserve the trailing front slash.
		 * DO NOT WRITE ON THE PASSED ARGUMENT!
		 */
		size_t charsToCopy = lastSep - finalname + 1;
		strncpy(tempPath, finalname, charsToCopy);
		tempPath[charsToCopy] = '\0';
	} else {
		strcpy(&tempPath[0], "/tmp/");
	}
	strcpy(pathname, &tempPath[0]);
	return (uint8_t *)pathname;
}

/**
 * \internal Build the z/TPF ELF-format core dump from the JDB built by CCCPSE.
 *
 * First figure out the absolute file path (starts with a '/' and
 * ends with a file suffix) we are to write the dump to, then ensure we
 * have enough room left over in the Java dump buffer left for us by 
 * CCCPSE. If we do not, return an error. Otherwise, section the buffer
 * space off, building in order:
 *    - The ELF64 header
 *    - The ELF64 Program Header section
 *    - The ELF64 NOTE section
 *      
 * Finally, write all the above to the indicated filename, and follow it
 * with a copy of storage contents from the Java dump buffer, which is
 * laid out in ascending address order, with the ELF64_Phdrs created to
 * match the dump buffer. Expect a final file size measured in tens
 * of megabytes.
 *
 * As a matter of protocol, set the first 8 bytes of the Java Dump
 * Buffer (struct ijavdbf) to zeros so CCCPSE knows it's okay to 
 * write over its contents once we're done working with it.
 *
 * The arg block is used for the bi-directional passing of data. Those of its
 * members which are pointers (most of them, in fact) are used mainly for 
 * output or at least have an output role. The fields of <arg> which are
 * required are shown as input parameters below. The fields of <arg> which
 * are used for output are detailed as return values.
 *
 * Since the pthread_create() calls limits us to one pointer to type void as the
 * function's input, and the function is required to return a void pointer, this
 * function will take the address of the <arg> block as the input parameter, and
 * will return a pointer to type char which represents the file name which now
 * contains the full path name of the ELF-format core dump file. There are also 
 * fields in <arg> that will be updated as described below.
 *
 * \param[in][out]     arg   pointer to a user-created datatype 'args', declared
 *                           in header file j9osdump_helpers.h
 * \param[in]        arg->OSFilename        Pointer to scratch storage for a path +
 *                                        file name string. It will be presumed 
 *                                        to be at least PATH_MAX+1 in size.
 * \param[in]        arg->wkspcSize        32-bit quantity representing the size in
 *                                        bytes of the wkSpace field, following.
 * \param[in]        arg->wkSpace        64-bit pointer to scratch workspace for
 *                                        use by this function. It is recommended
 *                                        to make it equal to PATH_MAX, since file &
 *                                        path names will be built in this space.
 * \param[in][out]    arg->flags            Indicators as to what is present and what
 *                                        is not.
 * \param[in]        arg->sii            Pointer to scratch storage to be used 
 *                                        forever more as a siginfo_t block
 * \param[in]        arg->uct            Pointer to scratch storage to be used 
 *                                        forever more as a ucontext_t block
 * \param[in]        arg->sct            Pointer to scratch storage to be used 
 *                                        forever more as a sigcontext block
 * \param[in]        arg->portLibrary    Pointer to an initialized OMRPortLibrary
 *                                        block. If there isn't one at call time,
 *                                        leave this value NULL and set flag
 *                                        J9ZTPF_NO_PORT_LIBRARY
 * \param[in]        arg->dibPtr            Address of the DIB attached to the faulting
 *                                        UOW at post-interrupt time.
 * 
 * \returns    Pointer to a hz-terminated string representing the absolute path
 *               name at which the core dump file was written.
 *            
 * \returns    arg->sii                   Filled in.
 * \returns    arg->uct                   Filled in.
 * \returns    arg->sct                   Filled in.
 * \returns    arg->rc                   Final return code. Zero if successful
 *                                       (core file built), non-zero if not.
 * \returns    arg->OSFilename           Same as the function return value.
 */
void *
ztpfBuildCoreFile(void *argv_block)
{
#define  MOUNT_TFS    4  /* TPF Filesystem equate from imount.h */

	args *arg = (args *) argv_block;
	uint8_t *buffer, *endBuffer;
	DBFHDR *dbfptr; /* Ptr to JDB's index header                */
	DIB *dibPtr = dibAddr(arg); /* Ptr to the Dump I'chg Block    */
	Elf64_Ehdr *ehdrp; /* Pointer to Elf64 file header                */
	Elf64_Phdr *phdrp; /* Pointer to Elf64_Phdr block                */
	Elf64_Nhdr *narea; /* Pointer to the ELF NOTE data                */
	char pathName[PATH_MAX]; /* Working buffer for core.* fname.*/
	char *ofn = (char *)dumpFilename(arg); /* Output dump file path    */
	uint32_t ofd; /* File descriptor for output dump            */
	uintptr_t rc; /* Working return code d.o.                    */
	uintptr_t wPtr; /* Working byte-sized pointer d.o.            */
	uint64_t phCount; /* Counter of Elf64_Phdrs required            */
	uint8_t *imageBuffer; /* Start of the JDB's ICB            */
	uint64_t imageBufferSize; /* Size of data in the ICB            */
	uint64_t octetsToWrite = 0UL; /* Count of bytes to write        */
	uint64_t octetsWritten = 0UL; /* Count of bytes written        */
	uint64_t spcAvailable;

	/*
	 *    If there is a Java dump buffer belonging to this process, then
	 *    convert its contents into an Elf64 core dump file; otherwise
	 *    return failure.
	 */
	if (!(dibPtr->dibjdb)) { /* No dump buffer? Not possible.*/
		dumpFlags(arg) |= J9TPF_NO_JAVA_DUMPBUFFER;
		returnCode (arg) = -1; /* Set error flags & bad RC ...    */
		return NULL; /* 'Bye.                        */
	}
	dbfptr = dibPtr->dibjdb; /* Pick up the dump buffer ptr    */
	if (0L == dbfptr->ijavcnt) { /* Did CCCPSE write us one?        */
		returnCode (arg) = -1; /* Nope. JDB is locked...        */
		dumpFlags(arg) |= J9TPF_JDUMPBUFFER_LOCKED;
		return NULL; /* See ya next time.            */
	}
	/*
	 *     Calculate the start, end, and net length of the output buffer we'll
	 *     use for the ELF-dictated start of the file content. The start address
	 *     should, as a matter of good practice, start on a paragraph boundary.
	 */
	buffer = (uint8_t *) (dbfptr->ijavbfp); /* Get buffer start as uint8_t ptr    */
	buffer = (uint8_t *) NEXT_PARA(buffer); /* Start it on next paragraph    */
	/*    boundary.                    */
	endBuffer = buffer + dbfptr->ijavbfl; /* Get bytes left in buffer        */
	spcAvailable = endBuffer - buffer; /* Get corrected count of bytes    */

	phCount = dbfptr->ijavcnt;

	int numJavaPgms = sizeof(javaPgmsToDump) / 5;
	octetsToWrite = sizeof(Elf64_Ehdr)
			+ ((phCount + 1 + numJavaPgms) * sizeof(Elf64_Phdr)) +
			NOTE_TABLE_SIZE;
	/*
	 *     Uh oh. Not enough free space remaining in the dump buffer. Now
	 *     we've gotta go to the heap and see if there's enough there. Not
	 *     much hope; but we have to try it.
	 */
	if (octetsToWrite > spcAvailable) { /* Check for available memory,    */
		buffer = malloc64(octetsToWrite); /*  anywhere. We're desperate.    */
		if (!buffer) { /* If we can't buffer our I/Os,    */
			returnCode (arg) = -1; /*  we are done. Indicate error */
			errMsg(arg, (uint8_t *)"Cannot buffer dump file, out of memory");
			dumpFlags(arg) |= J9TPF_OUT_OF_BUFFERSPACE;
			KEY0();
			dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
			UNKEY();
			return NULL; /* See ya 'round.                */
		}
	}
	/*
	 *    We're ready to write the file. First, we need a full path + filename
	 *    to represent the "OS filename" we're going to write. Get that path
	 *    from the final file name passed in the <tt>args</tt> block, then
	 *    follow that with "core.%X.%d" with the TOD in the second node and
	 *    the PID number in the third. In this way, we can identify the OS
	 *    filename later.
	 *
	 *    Next, we'll try to open it in CREATE+WRITE_ONLY mode. If it fails,
	 *    halt; else write away!
	 */
	splitPathName(ofn, pathName);
	/*
	 *    The "OS filename" will look like ${path}/core.${TOD_in_hex}.${pid}
	 */
	sprintf((char *)workSpace(arg), "core.%lX.%d", dibPtr->dstckf,
			dumpSiginfo(arg)->si_pid);
	{
		int fsystype = pathconf(pathName, _TPF_PC_FS_TYPE);
		if (fsystype == MOUNT_TFS) {
			errMsg(arg, (uint8_t *)"Cannot use the tfs for java dumps: path used='%s'\n",
					(uint8_t *)pathName);
			returnCode (arg) = -1;
			KEY0();
			dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
			UNKEY();
			return NULL;
			//dir_path_name is within the TFS
		}
	}
	strcat(pathName, (const char *)workSpace(arg));
	ofd = open(pathName, O_WRONLY | O_CREAT | O_EXCL);

	if (-1 == ofd) {
		errMsg(arg, (uint8_t *)"Cannot open() filename %s: %s\n", strerror(errno));
		returnCode (arg) = -1;
		KEY0();
		dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
		UNKEY();
		return NULL;
	}
	/*
	 *    The file is named and opened. Now we have to go build its ELF-dictated
	 *    parts. Set up pointers to the start of each sub-section, and then go
	 *    build them. When that's complete, write the file in two parts: first,
	 *    the ELF-dictated start, and then follow that with the data in the
	 *    ICB.
	 */
	wPtr = (uintptr_t) buffer; /*    Calc section addresses with single    */
	                           /*     byte arithmetic                    */
	ehdrp = (Elf64_Ehdr *) wPtr; /*    Elf64_Ehdr pointer                    */
	phdrp = (Elf64_Phdr *) (ehdrp + 1); /*    Elf64_Phdr pointer                    */
	buildELFHeader(ehdrp);
	uint64_t numBytes = 0;
	uint64_t pCount = buildELFPheaderBlk(phdrp, dbfptr, phCount, &numBytes);
	KEY0(); /* Get into SPK 0, then write the        */
	ehdrp->e_phnum = pCount; /*  absolutely final Phdr count and     */
	ehdrp->e_phoff = 0x40; /*    offset of its table into place in    */
	UNKEY(); /*  the Ehdr, then get back to SPK 1.    */
	wPtr = (uintptr_t) phdrp; /* Calculate the end address        */
	wPtr += ((phCount + 1 + numJavaPgms) * sizeof(Elf64_Phdr)); /*    of the Elf64_Phdr section        */
	narea = (Elf64_Nhdr *) wPtr; /* Calculate the address of NOTE sec,    */
	buildELFNoteArea(narea, dibPtr); /*    and go write it there.                */
	/*
	 *    Write the ELF-dictated portion of the file first.
	 */
	octetsWritten = writeDumpFile(ofd, buffer, octetsToWrite);
	if (-1 == octetsWritten) {
		errMsg(arg, (uint8_t *)"Error writing dump file %s: %s", ofn, strerror(errno));
		dumpFlags(arg) |= J9TPF_FILE_SYSTEM_ERROR;
		KEY0();
		dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
		UNKEY();
		return NULL;
	}
	/*
	 *    Finish the output with the ICB portion of the Java Dump Buffer
	 */
	imageBuffer = (uint8_t *) cinfc_fast(CINFC_CMMJDB);
	octetsWritten = writeDumpFile(ofd, imageBuffer, numBytes);
	if (-1 == octetsWritten) {
		errMsg(arg, (uint8_t *)"Error writing dump file %s: %s", ofn, strerror(errno));
		dumpFlags(arg) |= J9TPF_FILE_SYSTEM_ERROR;
		KEY0();
		dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
		UNKEY();
		return NULL;
	}
	int i = 0;
	for (i = 0; i < numJavaPgms; i++) {
		struct pat * pgmpat = progc(javaPgmsToDump[i], PROGC_PBI);
		struct ifetch *pgmbase = pgmpat->patgca;
		if (pgmbase != NULL) {
			int offset = pgmbase->_iftch_txt_off + pgmbase->_iftch_txt_size
					+ 0x1000;
			char * text = ((char*) pgmbase) + offset;
			uint64_t size = pgmpat->patpsize - offset;
			octetsWritten = writeDumpFile(ofd, (uint8_t *)text, size);
			if (-1 == octetsWritten) {
				errMsg(arg, (uint8_t *)"Error writing dump file %s: %s", ofn,
						strerror(errno));
				dumpFlags(arg) |= J9TPF_FILE_SYSTEM_ERROR;
				KEY0();
				dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
				UNKEY();
				return NULL;
			}
		}
	}
	/*
	 *    That's all folks. Close the file and return the so-called "OS Filename"
	 *    to the caller as if the OS had written it.
	 */
	rc = close(ofd); /* Only try to close() the file once    */
	if (-1 == rc) {
		errMsg(arg, (uint8_t *)"I/O error attempting to close %s:%s\n", ofn,
				strerror(errno));
		KEY0();
		dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
		UNKEY();
		return NULL;
	}
	/*
	 * Make sure we have a recognizable IPC permission on the OS filename,
	 *  then make sure the JDB buffer is unlocked in case CPSE needs it again.
	 */
	rc = chmod((const char *)workSpace(arg), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	KEY0();
	dbfptr->ijavcnt = 0L; /* Unlock the JDB so CCCPSE can reuse it*/
	UNKEY();
	/*
	 * Return the filename to the caller. Remember that the buffer containing
	 * it is non-JVM-managed heap space and should be free()d back to it to
	 * avoid a mem leak.
	 */
	strcpy((char *)dumpFilename(arg), ofn); /* Store it in the args block so the */
	return (void *) ofn; /*  caller always has it, and goback.*/
}

static char *elf64header_constant =
		"\x7f\x45\x4c\x46\x02\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x04\x00\x16\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
				"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

/**
 * \internal Build the ELF64 header. This is really simple, just overlay the input buffer
 * region with a set of constants. We will worry about the number of Program Headers
 * later.
 *
 * \param[in]  buffer    Address of writeable memory large enough to hold the ELF64_Ehdr structure.
 * \param[out] buffer    As above, filled in for the sizeof(Elf64_Ehdr).
 *
 * \return
 *      Address of <TT>buffer</TT> as given.
 */
static void
buildELFHeader(Elf64_Ehdr *buffer)
{
	Elf64_Ehdr *ePtr = buffer;
	KEY0();
	memcpy(buffer, elf64header_constant, sizeof(*buffer));
	ePtr->e_type = ET_CORE;
	ePtr->e_ehsize = sizeof(*ePtr);
	ePtr->e_phentsize = sizeof(Elf64_Phdr);
	UNKEY();
}

/**
 * \internal
 *
 * Build the ELF64 program header block in memory.
 *
 * The Elf64_Phdr looks like this:<b><tt> 
 * 0000 0004    p_type        Type of memory this Phdr represents.<b>
 * 0004 0004    p_flags        Phdr flags (permissions +OS_specific bits)<b>
 * 0008 0008    p_offset    Offset in file (0-relative) of memory image<b>
 * 0010 0008    p_vaddr        Virtual address of memory image @ run time<b>
 * 0018 0008    p_addr        Physical adress of memory image @ run time<b>
 * 0020 0008    p_filesz    Size of memory image in file<b>
 * 0028 0008    p_memsz        Size of memory image in memory<b>
 * 0030    0008    p_align        Boundary to align to when loaded<b>
 * ---- ----<b>
 * 0038 0038    Total length (56 decimal), natural alignment/size.<b></tt>
 *
 *    p_type should be obvious. Unlike relocatable/absolute ELF forms, the 
 *    The 0th element does not have to be a PT_NULL. In fact, observed core
 *    files often are of type PT_NOTE; we'll follow that pattern. The 2nd
 *    through last elements are PT_LOAD.
 *
 *    p_flags are just the permissions. For PT_NULL, they're zero. For PT_NOTE,
 *    they're zero (not loaded to memory), and the PT_LOADs are rwx (7).
 *
 *    p_offset is the offset from zero in the file at which the image starts.
 *    Each file-resident image runs continuously & sequentially for p_filesz. 
 *    We'll have to calculate the offset value and keep a running total, starting
 *    from p_offset for the PT_NOTE segment, then incrementing it by each
 *    p_filesz value.
 *    
 *    p_vaddr is the virtual address at which the segment resides. It's truly 
 *    meant more for executables & shared objects, its content of of little 
 *    consequence; but we'll take a policy of making it the physical address. 
 *    Same for p_addr, which is meant to describe the physical address. App-
 *    ropriate for a core file, not so much for a typical executable/DSO.
 *
 *    p_memsz is the image's size when loaded to memory. For a core file, 
 *    p_filesz = p_memsz. This would make a difference for an executable or
 *    DSO (this is how the .bss segment, for example, is described: memsz as
 *    required, 0 filesz.
 *
 *    p_align is the required alignment, if any, for the memory image described
 *    by this phdr. A value of 0 or 1 means no alignment required. Otherwise,
 *    the value should be modulo of a page size. For a core file, just use 1.
 *
 * 
 * \param[in]  buffer    Address of writeable memory large enough to house all 
 *                        required Phdr elements (phnum * sizeof(Elf64_Phdr)).
 * \param[in]  dbfHdr    Address of the Java dump buffer given to us by CCCPSE
 * \param[in]  phnum    The number of program headers to write
 *
 * \returns
 *      Count of program headers written.
 */
static uint16_t
buildELFPheaderBlk(Elf64_Phdr *buffer, DBFHDR *dbfHdr,
		uint64_t phnum, uint64_t * numBytes)
{
	Elf64_Phdr *phdrPtr = buffer;
	DBFITEM *curr = (DBFITEM *) (dbfHdr + 1);
	uint64_t dbfiCount = dbfHdr->ijavcnt;
	uint16_t newPhdrCount = 0;
	int numJavaPgms = sizeof(javaPgmsToDump) / 5;
	uint64_t netOffset = sizeof(Elf64_Ehdr)
			+ ((phnum + 1 + numJavaPgms) * sizeof(Elf64_Phdr));

	/*
	 * First we set up one constant Phdr ... a PT_NOTE. The rest come from DBF
	 * table items as adjusted.
	 */
	KEY0();
	memset(phdrPtr, 0, sizeof(Elf64_Phdr)); /* Set up 1 zero Phdr        */
	phdrPtr->p_type = PT_NOTE; /* Type = NOTE.                */
	phdrPtr->p_offset = netOffset; /* NOTE segment offset        */
	phdrPtr->p_filesz = NOTE_TABLE_SIZE; /* Size is a known constant    */
	/*
	 * Now write a Phdr for every useable DBF table item
	 */
	netOffset += NOTE_TABLE_SIZE; /* First value of p_offset    */
	phdrPtr += 1; /* Start with next phdr    slot*/

	while (dbfiCount > 0) {
		memset(phdrPtr, 0, sizeof(Elf64_Phdr)); /* Zeroize all 56 bytes.        */
		phdrPtr->p_type = PT_LOAD; /* Normal virtual memory content, loadable*/
		phdrPtr->p_flags = PT_RWX; /* Flags = rwx                            */
		phdrPtr->p_offset = netOffset; /* Fill in offset into file ...            */
		phdrPtr->p_vaddr = (uintptr_t) DBTIVADDR; /*  ... the virtual memory address        */
		phdrPtr->p_filesz = phdrPtr->p_memsz = DBTISIZ; /*  ... and memory sizes.        */
		phdrPtr->p_align = 1; /* Align all images to 1                */
		/*
		 *    Set up for next write.
		 */
		netOffset += DBTISIZ; /* Set next offset and                    */
		phdrPtr += 1; /*    next phdr pointer.                    */
		newPhdrCount += 1; /* Increment actual phnum                */
		/*
		 * Set up for next read
		 */
		curr += 1; /* Set next DBF table item ptr              */
		dbfiCount -= 1; /* One less DBF table item to work ....   */
	}
	// return number of bytes to write from the dump buffer
	*numBytes = netOffset
			- (sizeof(Elf64_Ehdr)
					+ ((phnum + 1 + numJavaPgms) * sizeof(Elf64_Phdr))
					+ NOTE_TABLE_SIZE);
	int i = 0;
	for (i = 0; i < numJavaPgms; i++) {
		struct pat * pgmpat = progc(javaPgmsToDump[i], PROGC_PBI);
		if (pgmpat != NULL) {
			struct ifetch *pgmbase = pgmpat->patgca;
			if (pgmbase != NULL) {
				int offset = pgmbase->_iftch_txt_off + pgmbase->_iftch_txt_size
						+ 0x1000;
				char * text = ((char*) pgmbase) + offset;
				uint64_t size = pgmpat->patpsize - offset;
				memset(phdrPtr, 0, sizeof(Elf64_Phdr)); /* Zeroize all 56 bytes.        */
				phdrPtr->p_type = PT_LOAD; /* Normal virtual memory content, loadable*/
				phdrPtr->p_flags = PT_RWX; /* Flags = rwx                            */
				phdrPtr->p_offset = netOffset; /* Fill in offset into file ...            */
				phdrPtr->p_vaddr = (uintptr_t) text;
				phdrPtr->p_filesz = phdrPtr->p_memsz = size;
				phdrPtr->p_align = 1; /* Align all images to 1                */
				netOffset += size; /* Set next offset and                    */
				phdrPtr += 1; /*    next phdr pointer.                    */
				newPhdrCount += 1; /* Increment actual phnum                */
			}
		}
	}
	UNKEY();
	return newPhdrCount; /* We're done                              */
}

#define    NOTE_DATA_OFFSET(p)    ((uintptr_t)p+0x14)

/**
 * \internal
 *      
 * \details Build the data (in the user-passed <TT>buffer</TT> address)
 * block to which an Elf64_Phdr will point. Its role is to extract 
 * information passed to it in CE2DIB and IJAVDBF and put them in the
 * proper places. Sometimes these values require translation to put 
 * them into proper contextual meaning, for example, a hardware PIC code
 * can infer a signal value. As long as we get close enough, it's good.
 * The contents of the NOTE section are very poorly documented.
 *
 * \note The final sub-section, tagged <i><tt>NT_ZTPF_SERRC</tt></i>, is invented 
 * for this routine's purposes: its role is to express the SE number associated 
 * with the already-written traditional SERRC dump as requested during design. Its 
 * existence may require addition of a method to the Java class which processes 
 * ELF NOTEs in <b><tt>jdmpview</tt></b> - there is no ELF NOTE tag otherwise 
 * suited for this purpose. Otherwise, this information stays buried.
 *
 * This function returns the total size of the buffer as its Elf64_Phdr
 * must report it.
 *
 * \param[in]  buffer    Writeable memory large enough to house the NOTE
 *                        section (see \ref NOTE_TABLE_SIZE, above).
 * \param[in]  dibPtr    Address of the CPSE<->JVM Dump Interchange Block
 *                        belonging to the faulting or requesting Unit Of Work.
 * \returns
 *         Size, in bytes, of the NOTE table section as written.
 */
static uintptr_t
buildELFNoteArea(Elf64_Nhdr *buffer, DIB *dibPtr)
{
	uintptr_t tableSize = NOTE_TABLE_SIZE; /* Return value                    */

	/* Pointers to the different required NOTE subsections, in sequential
	 * order of appearance: PRSTATUS, PRPSINFO, AUXV, FPREGSET, and
	 * S390_LAST_BREAK. The NOTE subsections are a little bizarre: each has a
	 * header of 0x0c bytes in length, followed by a displayable label of
	 * what it is. In general, these headers end up being 0x14 bytes in length
	 * because their data needs to be fullword-aligned. So, in general, the ptr
	 * prefixed with "nsh_" is the lower of the two addresses, being the
	 * Elf64_Nhdr pointer, and the un-prefixed pointer is where the data goes.
	 * Kinda screwy, but so is the NOTE section itself when there are a number
	 * of subsections in sequence.
	 */
	Elf64_Nhdr *nsh_prstat; /* NT_PRSTATUS NOTE header        */
	prstatus_t *prstat; /* PRSTATUS NOTE data ptr        */
	Elf64_Nhdr *nsh_prpsin; /* NT_PRPSINFO NOTE header        */
	prpsinfo_t *prinfo; /* PRPSINFO NOTE data ptr        */
	Elf64_Nhdr *nsh_auxvxx; /* NT_AUXV NOTE header            */
	AUXV *auxv; /* NT_AUXV NOTE data ptr        */
	Elf64_Nhdr *nsh_fpregs; /* NT_FPREGSET NOTE header        */
	fpregset_t *fpregs; /* NT_FPREGSET data ptr            */
	Elf64_Nhdr *nsh_lastbr; /* NT_S390_LAST_BREAK NOTE hdr    */
	uint16_t signalNumber;
	siginfo_t wSiginfo;

	DIB *holdDibPtr;
	DBFHDR *dbfPtr = dibPtr->dibjdb;
	/*
	 *    The ERI sits as the first JDB index item. We have to bump its pointer
	 *    past the DBFHDR, which just so happens to be the same length as a DBFITEM
	 *    itself.
	 */
	DBFITEM *eriRef = (DBFITEM *) dbfPtr + 1;
	struct cderi *eriPtr = (struct cderi *) (eriRef->ijavsbuf);
	/*
	 * z/TPF keeps its process information in a unique IPROC block, address it.
	 */
	struct iproc *pIPROC = iproc_process; /* Ptr to this PID's IPROC block */
	struct iproc *pPIPROC; /* Ptr to mom's PID IPROC block    */
	/*
	 *     The datatypes inside each NOTE section and subsection are so diverse
	 *     as to make the commonly-used typecasting approach cumbersome and difficult
	 *     to understand. Often working pointer fields which have a length of 1 for
	 *     address arithmetic uses are the easiest or best way to do this yet still
	 *     have legible code. We use all three of the following data objects in that
	 *     vein.
	 */
	uint8_t *wSrc; /* General uint8_t address pointer    */
	uint8_t *wDest; /* General uint8_t address pointer    */
	uint8_t *wPtr; /* General uint8_t address pointer    */

	/*
	 *     Build the NT_PRSTATUS subsection first
	 */
	KEY0();
	memset(buffer, 0, tableSize); /* Clear the entire buffer        */
	nsh_prstat = buffer; /* Start the data sec hdr ptr.    */
	prstat = (prstatus_t *) NOTE_DATA_OFFSET(nsh_prstat); /* Point at its data*/
	nsh_prstat->n_namesz = 5; /* Size of "CORE" = 5            */
	nsh_prstat->n_descsz = sizeof(prstatus_t);
	nsh_prstat->n_type = NT_PRSTATUS; /* Type = 1                        */
	memcpy(nsh_prstat + 1, "CORE", 5); /* Label this one "CORE".        */
	/*
	 *    The "current signal" value is inferred from the h/w PIC value lookup.
	 *    Unfortunately, ztpfDeriveSiginfo() is coded to use the DIB hung off
	 *    page 2 of the ECB, not the passed DIB. So hold on to the real
	 *    DIB address for this ECB in a safe place, and stuff the passed dibPtr
	 *    into page 2 of the ECB before the call, then replace it after the
	 *    call.
	 */
	holdDibPtr = ecbp2()->ce2dib; /* Substitute the passed DIB    */
	ecbp2()->ce2dib = dibPtr; /*  for the real one, then ...    */
	UNKEY(); /*  reset the SPK to prob state */
	ztpfDeriveSiginfo(&wSiginfo); /* While we go build a long        */
	KEY0(); /*  siginfo_t from which to        */
	prstat->info.si_signo = wSiginfo.si_signo; /*  feed the short one. After    */
	prstat->info.si_code = wSiginfo.si_code; /*  we've returned, get back    */
	prstat->info.si_errno = wSiginfo.si_errno; /*  into supv state prot key.    */
	prstat->cursig = wSiginfo.si_errno;
	ecbp2()->ce2dib = holdDibPtr; /* Replace the real DIB pointer    */
	/*
	 *    The relative PID values come from the process block .... Just read
	 *  them dirty; don't bother with a lock at first, see if it works before
	 *  we take a chance on losing ctl with a lock
	 */
	prstat->pid = pIPROC->iproc_pid; /* My PID                    */
	prstat->pgid = pIPROC->iproc_pgid; /* My PGID                    */
	prstat->ppid = pIPROC->iproc_pptr->iproc_pid; /* Mom's PID                */
	/* No concept of a session ID    */
	/*
	 * The System z TOD clock ticks once every 244 picoseconds. To get to accurate
	 * nanoseconds, we'd need to divide its value by 4.09836. Since we can't
	 * divide by a fractional quantity without going to floating point (and
	 * thereby introducing approximation error & burning more cycles), we
	 * usually trade precision for speed by shifting right 2 bits (dividing by 4)
	 * to yield nanoseconds from a little less than a quarter of a ns. That's
	 * considered close enough. But now that we need _micro_second (ns * 1000)
	 * precision, we'd be amplifying the error. And since we have to divide by 1000
	 * anyway, we might as well divide by 4098 (would shifting right 12 bits, for
	 * 4096, be close enough? No. That would cause us to lose a full millisecond in
	 * precision error!) to be accurate. So that's what we're doing below, since
	 * we have to eat the cycles in the division operation anyway -- might as well
	 * use the right divisor. Never mind rounding up with the remainder; we're
	 * already generating a larger result than reality because we can't go
	 * fractional. Does it really matter? Nah. But if you're gonna do it, you might
	 * as well do it right ....
	 */
	prstat->stime.tv_usec = *((unsigned long long *)(tpf_get_ecb_field(CE1ISTIM_FIELD)))/4098;
	wDest = (uint8_t *) &(prstat->reg); /* Point at offset +0 of reg fld    */
	/*
	 *     The PGM/O PSW is followed by a 16-wide array of ULONGs which
	 *     represent the values of all 16 general registers, 0 through 15,
	 *     as they were at the time the PGM/N PSW fired. Prepare to copy
	 *     them into the NT_PRSTATUS section, lth=128+16 (144).
	 */
	wSrc = (unsigned char *) (dibPtr->dispw); /* Point @ PSW & general regs        */
	memcpy(wDest, wSrc, 144); /* Preserve all that data.            */
	prstat->fpvalid = TRUE; /* Yes, we have floating point        */
	/*
	 * NT_PRPSINFO is next
	 */
	nsh_prpsin = (Elf64_Nhdr *) (prstat + 1); /* Point to next NOTE hdr    */
	prinfo = (prpsinfo_t *) NOTE_DATA_OFFSET(nsh_prpsin);
	nsh_prpsin->n_namesz = 5; /* Size of "CORE" = 5        */
	nsh_prpsin->n_descsz = sizeof(prpsinfo_t);
	nsh_prpsin->n_type = NT_PRPSINFO; /* Type = 3                    */
	memcpy(nsh_prpsin + 1, "CORE", 5); /* Set "CORE" name ...        */
	prinfo->char_state = 'R'; /* We were 'R'unning.        */
	prinfo->flag = 0x00400440L; /* Flags are constant        */
	prinfo->uid = pIPROC->iproc_uid; /* User id &                */
	prinfo->gid = pIPROC->iproc_gid; /*  group id.                */
	memcpy(&prinfo->pid, &prstat->pid, 16); /* Process ID set again        */
	strcpy((char *)prinfo->fname, "main"); /* Just like Linux does it    */
	wSrc = (unsigned char *)getenv("IBM_JAVA_COMMAND_LINE");
	if (wSrc) {
		strncpy((char *)prinfo->psargs, (const char *)wSrc, (size_t)PRARGSZ);
		prinfo->psargs[PRARGSZ - 1] = '\0';
	}
	/*
	 * NT_AUXV after that ... we fake it (we have no ELF protocols involved
	 * in task switching and thus have no AUXV structure or meaning).
	 */
	nsh_auxvxx = (Elf64_Nhdr *) (prinfo + 1); /* Next NOTE header            */
	auxv = (AUXV *) NOTE_DATA_OFFSET(nsh_auxvxx); /* and data region ptr.    */
	nsh_auxvxx->n_namesz = 5; /* Size of "CORE" = 5        */
	nsh_auxvxx->n_descsz = sizeof(AUXV);
	nsh_auxvxx->n_type = NT_AUXV; /* Type = 6                    */
	memcpy(nsh_auxvxx + 1, "CORE", 5); /* Set "CORE" name ...        */
	memset(auxv, 0, sizeof(AUXV)); /* Fill it out.                */
	/*
	 * ... then NT_FPREGSET
	 */
	nsh_fpregs = (Elf64_Nhdr *) (auxv + 1); /* Next NOTE header            */
	fpregs = (fpregset_t *) NOTE_DATA_OFFSET(nsh_fpregs); /* and data ptr    */
	nsh_fpregs->n_namesz = 5; /* Size of "CORE" = 5        */
	nsh_fpregs->n_descsz = sizeof(fpregset_t);
	nsh_fpregs->n_type = NT_FPREGSET; /* Type = 2                    */
	memcpy(nsh_fpregs + 1, "CORE", 5); /* Set "CORE" name ...        */
	/*
	 * We don't care what the real FPCR (fpc) contained. We're setting the mask
	 * only to state that 16 fprs exist. The real FPCR is too hard to get,
	 * anyway.
	 */
	fpregs->fpc = 0x00800000;
	wSrc = (uint8_t *) &(dibPtr->dfreg); /* Get the adrs of the fp    */
	wDest = (U_8 *)&(fpregs->fprs);                /*  regs, then copy them     */
	memcpy( wDest, wSrc, sizeof(fpregs->fprs) );   /*  into the note area.      */
	/*
	 * ... then NT_S390_LAST_BREAK
	 */
	nsh_lastbr = (Elf64_Nhdr *) (fpregs + 1);
	nsh_lastbr->n_namesz = 6; /* Size of "LINUX" = 6        */
	nsh_lastbr->n_descsz = 8; /* Always 0x008 bytes.        */
	nsh_lastbr->n_type = 0x306; /* Type = NT_S390_LAST_BR    */
	wDest = (uint8_t *) NOTE_DATA_OFFSET(nsh_lastbr);
	memcpy(nsh_lastbr + 1, "LINUX", 6); /* Set "LINUX" name            */
	wSrc = (unsigned char *) (dibPtr->dbrevn); /* Get NOTE data source ptr */
	memcpy(wDest, wSrc, 8); /*  and move it in.            */
	UNKEY();
	return tableSize; /* We're done.                */
	/*
	 * ... and finally, NT_ZTPF_SERRC (made up; we'll need to add the Java
	 * class code to display this)
	 */
	wPtr += 8;
	nsh_lastbr = (Elf64_Nhdr *) (wDest + 8);
	nsh_lastbr->n_namesz = 5; /* Size of "ZTPF" = 5        */
	nsh_lastbr->n_descsz = 68; /* Always 0x044 bytes.        */
	nsh_lastbr->n_type = 0x500; /* Type = 500                */
	memcpy(nsh_lastbr + 1, "ZTPF", 5); /* Set "ZTPF" name ...        */
	wDest += 0x14; /*  and point at data        */
	sprintf((char *)wDest, "SE-%4.4d OPR-%c%6.6X PGM-%4.4s %5.5c %8.8s", eriPtr->eriseq,
			eriPtr->eridpfx, eriPtr->eridnum, eriPtr->eridate, eriPtr->eritime);
	UNKEY();
	return tableSize; /* We're done.                */
}

void
ztpf_preemptible_helper(void)
{
	safeStorage *s = allocateDurableStorage();
	translateInterruptContexts(&(s->argv));

	s->argv.flags = J9TPF_NO_PORT_LIBRARY;
	s->pDIB = ecbp2()->ce2dib;
	s->argv.dibPtr = s->pDIB;
	masterSynchSignalHandler(s->siginfo.si_signo, &(s->siginfo), &(s->ucontext),
			s->pDIB->dbrevn);
	/*
	 * Like its "harder" cousin, below, if the signal handler returns here, it is
	 * acknowledging that it wants to exit. Otherwise, don't expect a return. In
	 * the normal case, the faulting thread gets returned to the J9 thread pool
	 * so there's no chance of an accidental return here.
	 */
	exit(0);
	/* UNREACHABLE */
}

void
ztpf_nonpreemptible_helper(void)
{
	safeStorage *s = allocateDurableStorage();
	uint8_t holdkey;
	sigset_t newmask;
	sigset_t oldmask;

	/*
	 *    Go translate the TPF-ish data available at interrupt time into UNIX-ish
	 *    formatted blocks in preparation to call the master synchronous signal
	 *    handler for the JVM's structured signal handling architecture. This
	 *    storage comes from a static storage segment in DJPR.
	 */
	s->pDIB = (DIB *) (ecbp2()->ce2dib);
	s->argv.dibPtr = s->pDIB;
	translateInterruptContexts(&(s->argv));

	/*
	 *    Only proceed if (1) the signal is currently NOT blocked, and (2)
	 *    if no other interrupt has queued itself. In the interim, if
	 *    applicable, sleep for 50 ms. IOW, serialize all synchronous
	 *    signals -- we have to, we only have one JDB.
	 */
	sigemptyset(&newmask); /* Ensure our 'constant' mask word is zeros    */
	sigaddset(&newmask, s->siginfo.si_signo); /* Build a constant signal mask word        */

	do {
		PROC_LOCK(&(s->pPROC->iproc_JVMLock), holdkey);
		if (s->pPROC->iproc_tdibptr) { /* Are we blocked because we're already    working    */
			PROC_UNLOCK(&(s->pPROC->iproc_JVMLock), holdkey); /* another signal? If so,unlock    */
			usleep(50000); /* and sleep for 50 ms.            */
		}
		pthread_sigmask(SIG_BLOCK, NULL, &oldmask); /* Test the signal mask again.    */
	} while ((newmask & oldmask) || (s->pPROC->iproc_tdibptr));
	/*
	 *    We are released to go process this signal. Do it. The lock we also hold
	 *    on our little subsection of the PROC block is also still in effect.
	 *    But first, stash the DIB pointer in the IPROC block, then unlock it.
	 */
	s->pPROC->iproc_tdibptr = (uint64_t) s->pDIB; /*    Stash the faulting DIB pointer            */
	PROC_UNLOCK(&(s->pPROC->iproc_JVMLock), holdkey);
	/*
	 * Call the master structured signal handler for synchronous signals.
	 * It's not possible that an async signal can get here. The signal handler will
	 * either hand off execution to the JIT, or will run the dump agents. Do not
	 * expect a return; else the normal SIG_DFL (bye, bye) rule applies.
	 */
	masterSynchSignalHandler(s->siginfo.si_signo, &(s->siginfo), &(s->ucontext),
			s->pDIB->dbrevn);
	/*
	 * If execution returns to this point, the signal handler returned
	 * here. This means that the thread accepts the consequences of a
	 * return from the SIG_DFL treatment of a fatal signal, which means
	 * an end to the process.
	 */
	exit(0);
	/* UNREACHABLE */
}
