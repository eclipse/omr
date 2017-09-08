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
 * \brief Dump formatting externals referenced from portLibrary
 */

#define ZTPF_NEEDS_GETCONTEXT

#include <signal.h>
#include <tpf/c_proc.h>
#include <tpf/c_cinfc.h>
#include <tpf/tpfapi.h>
#include <tpf/sysapi.h>
#include <tpf/c_idsprg.h>
#include <tpf/c_stck.h>
#include <sys/ucontext.h>
#include <pthread.h>
#define OSDUMP_SYMBOLS_ONLY

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <tpf/c_eb0eb.h>
#include <tpf/c_proc.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrosdump_helpers.h"
#include "omrsignal.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

typedef struct iproc IPROC;
#include "safe_storage.h"

/**
 * \fn Callback from CCCPSE: preemptible error.
 *
 * The z/TPF kernel will call us back at this function when it (the z/TPF
 * program interrupt handler) determines that the error is 'preemptible'
 * (i.e. is a 'soft' interrupt which is caused by division by zero or a
 * null-pointer reference).
 *
 */
static void __attribute__((used))
ztpf_preemptible_err_ep(void)
{
	// let the port library do the work.
	ztpf_preemptible_helper();
}

/**
 * \fn Callback from CCCPSE: non-preemptible error.
 *
 * The z/TPF kernel will call us back at this function when it (the z/TPF
 * program interrupt handler) determines that the error is 'non-preemptible'
 * (i.e. is a 'hard' interrupt) caused by any number of programming errors
 * considered serious enough to question the state of the JVM.
 */
static void
__attribute__((used))
ztpf_nonpreemptible_err_ep(void)
{
	// let the port library do the work.
	ztpf_nonpreemptible_helper();
}

/**
 * \fn  Create an ELF core dump file of the user space
 *
 *
 *        This function determines if there is a Java Dump Buffer available pertinent to this
 *        thread/process. If one is not, we know we were called from the system dump agent's
 *        executing a hook (such as -Xdump:system:events=vmstop) and arrange for one to be
 *        created right now. If a dump pertinent to us is available, we use it. We then call
 *        ztpfBuildCoreDump() in order to build an ELF core dump from its contents, then
 *        write it to file.
 *
 *        After we're done writing the file, zeroize the Java Dump Buffer pointer in the
 *        DIB block.
 *
 * \param[in] portLibrary       The port library.
 * \param[in,out] filename      Buffer for filename. On input, describes a requested filename.
 * \param[in] dumpType          Type of dump to perform. Ignored for z/TPF.
 * \param[in] userData          Implementation specific data. Ignored for z/TPF.
 * \return 0 on success, non-zero otherwise.
 *
 * \note <TT>filename</TT> buffer can not be NULL.
 * \note User must allocate and free <TT>filename</TT> buffer.
 * \note <TT>filename</TT> buffer length is platform dependent, assumed to be <TT>EsMaxPath</TT> or <TT>MAX_PATH</TT>
 * \note If <TT>filename</TT> buffer is empty, an error will be returned. Should it
 *        contain only a filename (no path), one will be chosen for it.
 */
uintptr_t
omrdump_create(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData)
{
	uint8_t workspace[PATH_MAX];
	char *errMsg = NULL;
	uint8_t *JVMfn = NULL;
	uint8_t holdkey = 0;

	DIB *pDIB = NULL;
	DBFHDR *pDBF = NULL;
	safeStorage *s = NULL;
	uintptr_t ptr;
	char *targetDirectory;
	void *oldPath;

	s = allocateDurableStorage();
	if (s == NULL)  {
		portLibrary->tty_err_printf(portLibrary, "Unable to to allocateDurableStorage().\n");
		return 1; /* Return Failure. */
	}
	s->argv.portLibrary = portLibrary;
	s->argv.OSFilename = (unsigned char *)filename;

	/*
	 * First, figure out whether or not we have to generate the dump buffer contents.
	 * If this routine was called as a result of a hooked event being hit; not a
	 * program check, then there is not a Java dump buffer (JDB) provided by the
	 * z/TPF Control Program (supervisor, kernel, whatever you want to call the
	 * permanently-memory-resident part of the system loaded by IPL).
	 *
	 * If we knew that this thread were the same thread which faulted, we could simply
	 * check ce2dib->dibjdb for non-zero content. Unfortunately, we don't have that
	 * assurance. Instead, the ce2dib address was stored by the synchronous master
	 * signal callback routine in the process block. Fetch it and see if it's non-zero.
	 * If so, a thread (the system dump agent) was dispatched here to create the dump
	 * file due to a synchronous signal (like SIGSEGV). If not, there was no signal
	 * and thus no existing data from which to write the core file.
	 *
	 * If there was no signal involved, we know that this call to omrdump_create()
	 * was issued by the system dump agent which fired because of a hooked event. In
	 * these cases, we presume that the user has a reason for wanting to see a system
	 * snapshot at the event, and we have to issue a SERRC 4006 in order to get the
	 * dump buffer created, even if it is quite a few microseconds after the call to
	 * the dump agents from the Dump() java class or a hooked event. We'll have to do
	 * this -- it's as close in time as we can get. Once the 4006 comes back, we can
	 * dispatch a thread to write the dump core file and wait on it. From that point, we
	 * will follow a very simple common process regardless of how the "OS core dump" file
	 * was created: we will rename it to whatever the system dump agent specified in the
	 * call and then we're done.
	 *
	 * It is crucial that the "OS core file" be written to the same path as the dump
	 * agent specified for the final filename. When we rename it, we will invoke no
	 * I/O overhead -- rename() simply alters the name field in the directory entry.
	 *
	 * A MFS (memory file system) output destination is recommended for speed's sake.
	 */
	/*
	 * ------------------------------------------------------------------------------
	 *  FUNCTION MAINLINE BEGINS
	 * ------------------------------------------------------------------------------
	 */
	/*
	 *        Go see if there's a current signal we're working now. If there is one,
	 *        we stuffed its DIB in the process block at a reserved location.
	 *        If it's not zero, that's where the faulting DIB is. If it is zero, we
	 *        aren't working a synchronous signal.
	 */
	if (s->pPROC->iproc_tdibptr) { /* If there's a DIB ptr stashed,  */
		pDIB = (DIB *) (s->pPROC->iproc_tdibptr); /* use it, then free the stash    */
		PROC_LOCK(&(s->pPROC->iproc_JVMLock), holdkey);
		s->pPROC->iproc_tdibptr = 0L; /* location ...                   */
		PROC_UNLOCK(&(s->pPROC->iproc_JVMLock), holdkey);
		pDBF = pDIB->dibjdb; /* and set our JDB pointer. */
	}

	/*
	 * If we have been entered from a non-interrupt generated event, go get the current
	 * system state parts that an ELF core file cares about, and base the core file
	 * on them.
	 */
	if (!(pDBF)) { /* Is there a JDB?              */
		serrc_op_ext(SERRC_RETURN, 0x4006, NULL, '\xc9', NULL); /* No, go create one.   */
		s->pDIB = ecbp2()->ce2dib; /* We now have a DIB pointer.   */
		pDIB = s->pDIB; /* Prep it, to pass along to coreBuilder Thread. JJ_UT*/
		translateInterruptContexts(&(s->argv)); /* Go build UNIX context blks.  */
	}

	s->argv.dibPtr = pDIB; /* Pass the DIB to the file writer      */

	/*
	 * Build the ELF core file. Wait on it to come back.
	 */
	JVMfn = ztpfBuildCoreFile(&(s->argv));

	/*
	 * Unless ztpfBuildCoreFile() spit up on us, we should have an absolute pathname to the file. Let's
	 * see what we got. If we took an error, the flags bits will generally
	 * tell us what the broader error was, and if a runtime service failed,
	 * errno will have been placed in argv.rc.
	 */
	if (!JVMfn) { /* Uh oh. There was an error. What to say?      */
		if (s->argv.flags & J9TPF_ERRMSG_IN_WKSPC) {
			errMsg = (char *)s->argv.wkSpace; /* The thread told us what to say ...           */
		} else { /* How do we come up with a message?            */
			switch (s->argv.flags & J9TPF_FLAGS_ERR_MASK) {
			case (J9TPF_JDUMPBUFFER_LOCKED + J9TPF_NO_JAVA_DUMPBUFFER):
			case J9TPF_JDUMPBUFFER_LOCKED:
				errMsg = "Java dump buffer is locked";
				break;
			case J9TPF_NO_JAVA_DUMPBUFFER:
				errMsg = "Java dump buffer is likely not for this process";
				break;
			case J9TPF_FILE_SYSTEM_ERROR:
				if (s->argv.rc) { /* If the thread left us an errno,      */
					errMsg = strerror(s->argv.rc); /*see if we can get a decent   */
				} /* reason out of strerror().    */
				else if (strlen((char *)s->argv.wkSpace)) { /* Otherwise, see if there's     */
					errMsg = (char *)s->argv.wkSpace; /*	 a message left by the dump writer	*/
				} /* If there is, use it.                 */
				break;
			case J9TPF_OUT_OF_BUFFERSPACE:
				errMsg =
						"insufficient free buffer space to write the core file";
				break;
			default:
				sprintf((const char *)workspace,
						"unantipated err flag value (0x%X). errno=%x.",
						s->argv.flags, s->argv.rc);
				errMsg = (char *)workspace;
				strcat(errMsg, "\nThere is no system dump to work with.");
				break;
			}
		}
		portLibrary->tty_err_printf(portLibrary,
				"unable to write ELF core dump: %s\n", errMsg);
		return 1; /* Report failure, get outta here.      */
	} else {
		s->argv.rc = rename((const char *)s->argv.wkSpace, (const char *)JVMfn); /* Success. Return.                     */
		if ((s->argv.rc < 0) && (errno == ENOENT)) {
			//Current working directory may be different from target Dump Directory.
			//In this scenario the dump is already in the target directory.
			//Therefore re-issue the command using an absolute path name for old
			//name.
			targetDirectory = dirname((char *)JVMfn);
			if (targetDirectory == NULL) {
				portLibrary->tty_err_printf(portLibrary,
						"Problem determining directory for %s\n", JVMfn);
			}
			// +2 for terminating Null and joining path seperator
			oldPath = alloca(
					strlen(targetDirectory) + strlen((const char *)s->argv.wkSpace) + 2);
			strcpy(oldPath, targetDirectory);
			strcat(oldPath, "/");
			strcat(oldPath, (const char *)s->argv.wkSpace);
			s->argv.rc = rename(oldPath, (const char *)JVMfn); /* Success. Return. */
			if (s->argv.rc < 0) {
				portLibrary->tty_err_printf(portLibrary,
						"Failed to rename: %s\n", oldPath);
			}
			//free response from dirname (using atoe wrapper library) because
			//untranslated dirname is static storage and atoe_dirname needs
			//to use malloc for response.
			free(targetDirectory);
		}

	}
	return 0;
}

/**
 *       \deprecated Still referenced from portLibrary, must be kept as is.
 */
int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary)
{

	char prevSPK;
	IPROC *pPROC = (IPROC *) iproc_process;
	struct ifetch *fblk;
	int rc;
	PROC_LOCK(&(pPROC->iprocloc), prevSPK);
	memset(pPROC->iproc_nppsw, 0,
			(sizeof(pPROC->iproc_nppsw) + sizeof(pPROC->iproc_pepsw)
					+ sizeof(pPROC->iproc_byteinterp)
					+ sizeof(pPROC->iproc_tdibptr)
					+ sizeof(pPROC->iproc_JVMLock)));
	pPROC->iproc_flags |= IPROC_JVMPROCESS;
	PROC_UNLOCK(&(pPROC->iprocloc), prevSPK);

	/*  The byte interpreter is _this_ module, DJPR. Its name needs to be passed in EBCDIC.
	 *  The #define following is that constant in EBCDIC.
	 */
#define OMRZTPF_BYTE_INTERP "\xC4\xF9\xE5\xD4"
	/*              J9ZTPF_BYTE_INTERP "D9VM" */
	/*
	 *      z/TPF handles JVM-associated processes a little differently than all
	 *      others. We need to mark the process block to inform the kernel that
	 *      this is such a process so h/w-detected errors can come back to us instead
	 *      of going to exit. There are no such things as kernel-space signals.
	 */

	/*
	 *      First, get the address range of the byte interpreter module.
	 *      The kernel needs this to detect what's faulting: native or Java code
	 */
	rc = getpc( OMRZTPF_BYTE_INTERP, GETPC_LOCK, NO_LVL, &fblk, GETPC_NODUMP,
			GETPC_PBI, NULL);
	/*
	 *      Next, get a lock on the JVM's sub-area of the process block
	 *      with the PROC_LOCK() macro, which also puts us in KEY 0 so
	 *      we can write on it.
	 */
	PROC_LOCK(&(pPROC->iproc_JVMLock), prevSPK);
	/*
	 * Calculate & store the address range of the byte interpreter module
	 */
	pPROC->iproc_byteinterp[0] = fblk->_iftch_entry + fblk->_iftch_txt_off;
	pPROC->iproc_byteinterp[1] = pPROC->iproc_byteinterp[0]
			+ fblk->_iftch_txt_size - 1;
	/*
	 * Give CCCPSE a pair of callback addresses. These are in PSW format.
	 */
	pPROC->iproc_nppsw[0] = 0;
	pPROC->iproc_pepsw[0] = 0;
	pPROC->iproc_nppsw[1] = (unsigned long int) &ztpf_nonpreemptible_err_ep;
	pPROC->iproc_pepsw[1] = (unsigned long int) &ztpf_preemptible_err_ep;
	/*
	 * Unlock the process block and put us back in the normal problem
	 * state key.
	 */
	PROC_UNLOCK(&(pPROC->iproc_JVMLock), prevSPK);

	return 0;
}

/**
 *              \deprecated Still referenced from portLibrary, must be kept as is.
 */
void
omrdump_shutdown(struct OMRPortLibrary *portLibrary)
{
	/* noop */
}
