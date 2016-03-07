/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Dump formatting
 */

#include <sys/types.h>
#if defined(AIXPPC)
#include <sys/proc.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrosdump_helpers.h"

#if 0
#define DUMP_DBG
#endif

#if defined(OMRPORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>

#if defined(AIXPPC)
/* **********
These definitions were copied from /usr/include/sys/proc.h in AIX 6.1, because this functionality does not
exist on AIX 5.3. The code that uses these definitions does a runtime lookup to see if the
proc_setattr() function exists.
********* */
#ifndef PA_IGNORE
/*
 * proc_getattr and proc_setattr are APIs that allow a privileged
 * user to set/unset attributes of any process (including self)
 */
#define PA_IGNORE       0
#define PA_ENABLE       1
#define PA_DISABLE      2

typedef struct {
	uchar core_naming;  /* Unique core file name */
	uchar core_mmap;    /* Dump mmap'ed regions in core file */
	uchar core_shm;     /* Dump shared memory regions in core file */
	uchar aixthread_hrt;/* Enable high res timers */
} procattr_t;
#endif
#endif

static void unlimitCoreFileSize(struct OMRPortLibrary *portLibrary);

/**
 * Create a dump file of the OS state.
 *
 * @param[in] portLibrary The port library.
 * @param[in] filename Buffer for filename optionally containing the filename where dump is to be output.
 * @param[out] filename filename used for dump file or error message.
 * @param[in] dumpType Type of dump to perform.
 * @param[in] userData Implementation specific data.
 *
 * @return 0 on success, non-zero otherwise.
 *
 * @note filename buffer can not be NULL.
 * @note user allocates and frees filename buffer.
 * @note filename buffer length is platform dependent, assumed to be EsMaxPath/MAX_PATH
 *
 * @note if filename buffer is empty, a filename will be generated.
 * @note if J9UNIQUE_DUMPS is set, filename will be unique.
 */
uintptr_t
omrdump_create(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData)
{

#if !defined(J9OS_I5)
	char *lastSep;
	intptr_t pid;

#if defined(AIXPPC)
	int rc = -1;
	struct vario myvar;
	int sys_parmRC;

	/* check to see if full core dumps are enabled */
	sys_parmRC = sys_parm(SYSP_GET, SYSP_V_FULLCORE, &myvar);

	if ((sys_parmRC == 0) && (myvar.v.v_fullcore.value == 1)) {

		/* full core dumps are enabled, go ahead and use gencore */
		unlimitCoreFileSize(portLibrary);
		rc = genSystemCoreUsingGencore(portLibrary, filename);

		if (rc == 0) {
			/* we have successfully generated the system core file */
			return rc;
		}
	} else {
		portLibrary->tty_err_printf(portLibrary, "Note: \"Enable full CORE dump\" in smit is set to FALSE and as a result there will be limited threading information in core file.\n");
	}
#endif /* aix_ppc */

	lastSep = filename ? strrchr(filename, DIR_SEPARATOR) : NULL;

	/* fork a child process from which we'll dump a core file */
	if ((pid = fork()) == 0) {
		/* in the child process */

#if defined(LINUX)
		/*
		 * on Linux, shared library pages don't appear in core files by default.
		 * Mark all pages writable to force these pages to appear
		 */
		markAllPagesWritable(portLibrary);
#endif /* defined(LINUX) */

#ifdef AIXPPC
		/* On AIX we need to ask sigaction for full dumps */
		{
			struct sigaction act;
			act.sa_handler = SIG_DFL;
			act.sa_flags = SA_FULLDUMP;
			sigfillset(&act.sa_mask);
			sigaction(SIGIOT, &act, 0);
		}
#endif

		/*
		 * CMVC 95748: don't use abort() after fork() on Linux as this seems to upset certain levels of glibc
		 */
#if defined(LINUX) || defined(OSX)
#define J9_DUMP_SIGNAL  SIGSEGV
#else /* defined(LINUX) || defined(OSX) */
#define J9_DUMP_SIGNAL  SIGABRT
#endif /* defined(LINUX) || defined(OSX) */

		/* Ensure we get default action (core) - reset primary&app handlers */
		OMRSIG_SIGNAL(J9_DUMP_SIGNAL, SIG_DFL);

		/* Move to specified folder before dumping? */
		if (lastSep != NULL) {
			lastSep[1] = '\0';
			chdir(filename);
		}

		/*
		 * Ensure that ulimit doesn't get in our way
		 */
		unlimitCoreFileSize(portLibrary);

#if defined(LINUX) || defined(OSX)
		pthread_kill(pthread_self(), J9_DUMP_SIGNAL);
#endif /* defined(LINUX) || defined(OSX) */

		abort();
	} /* end of child process */

	/* We are now in the parent process. First check that the fork() worked OK (CMVC 130439) */
	if (pid < 0) {
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "insufficient system resources to generate dump, errno=%d \"%s\"", errno, strerror(errno));
		return 1;
	}

#if defined(LINUX) || defined(OSX)

	if (NULL != filename) {

		/* Wait for child process that is generating core file to finish */
		waitpid(pid, NULL, 0);

		return renameDump(portLibrary, filename, pid, J9_DUMP_SIGNAL);
	} else {
		return 1;
	}

#elif defined(AIXPPC)

	if (filename && filename[0] != '\0') {
		char corepath[EsMaxPath] = "";

		/* Wait for child process that is generating core file to finish */
		waitpid(pid, NULL, 0);

		/* Copy path and remove basename */
		if (lastSep != NULL) {
			strcpy(corepath, filename);
			corepath[1 + lastSep - filename] = '\0';
		}

		findOSPreferredCoreDir(portLibrary, corepath);

		/* Search for expected names in the directory <corepath>
		 * and get the absolute path for the generated core file */
		appendCoreName(portLibrary, corepath, pid);

		/* check if the generated core filename ends in .Z (which indicates core compression was on) */
		if (strstr(corepath, ".Z") == (corepath + strlen(corepath) - strlen(".Z"))) {
			/* append the filename with ".Z" */
			if ((strlen(filename) + strlen(".Z") + 1) < EsMaxPath) {
				strcat(filename, ".Z");
			}
		}

		/* Apply suggested name */
		if (rename(corepath, filename)) {
			portLibrary->str_printf(portLibrary, filename, EsMaxPath, "cannot find core file: \"%s\". check \"ulimit -Hc\" is set high enough", strerror(errno));
			return 1;
		}

		return 0;
	}

	portLibrary->tty_err_printf(portLibrary, "Note: dump may be truncated if \"ulimit -c\" is set too low\n");

	/* guess typical filename */
	if (lastSep != NULL) {
		lastSep[1] = '\0';
		strcat(filename, "{default OS core name}");
	} else if (filename != NULL) {
		strcpy(filename, "{default OS core name}");
	}

	return 0;

#else

#error "This platform doesn't have an implementation of omrdump_create"

#endif /* #if defined(AIX) */

#else /* J9OS_I5 */

	if (getenv("I5_JVM_SUPPLIED_DUMP") == 0) {
		char corefile[EsMaxPath + 1] = "";
		intptr_t i5Pid = getpid();

		/* Ensure that ulimit doesn't get in our way */
		unlimitCoreFileSize(portLibrary);

		if (filename && filename[0] != '\0') {
			if (getenv("J9NO_DUMP_RENAMING")) {
				char *lastSep = filename ? strrchr(filename, DIR_SEPARATOR) : NULL;
				if (lastSep != NULL) {
					lastSep[1] = '\0';
					strcat(filename, "core");
				}
				strcpy(corefile, filename);
			} else {
				strcpy(corefile, filename);
			}
		} else {
			strcpy(corefile, "core");
			strcpy(filename, "{default OS core name}");
		}

		/* Generate the core file. On I5 gencore will always generate a full core dump */
		struct coredumpinfo coredumpinfop;
		memset(&coredumpinfop, 0, sizeof(coredumpinfop));

		coredumpinfop.length = strlen(corefile) + 1;
		coredumpinfop.name = corefile;
		coredumpinfop.pid = i5Pid;
		coredumpinfop.flags = GENCORE_VERSION_1;
		return gencore(&coredumpinfop);
	}
#endif /* J9OS_I5 */
}

static void
unlimitCoreFileSize(struct OMRPortLibrary *portLibrary)
{

	struct rlimit limit;

	/* read the current value (to initialize limit.rlim_max) */
	getrlimit(RLIMIT_CORE, &limit);

	/* ask for the most we're allowed to, up to the hard limit */
	limit.rlim_cur = limit.rlim_max;

	setrlimit(RLIMIT_CORE, &limit);
}

int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary)
{

#if defined(AIXPPC)
	uintptr_t handle;

	portLibrary->error_set_last_error(portLibrary, 0, 0);
	portLibrary->portGlobals->control.aix_proc_attr = 1;

	if (0 == portLibrary->sl_open_shared_library(portLibrary, NULL, &handle, 0)) {
		int (*setattr)(pid_t pid, procattr_t * attr, uint32_t size);

		if (0 == portLibrary->sl_lookup_name(portLibrary, handle, "proc_setattr", (uintptr_t *)&setattr, "IPLj")) {
			procattr_t attr;
			int rc;

			memset(&attr, PA_IGNORE, sizeof(procattr_t));
			/* enable the flags that are required for including non-anonymous mmap regions and shared memory in core file */
			attr.core_naming = PA_ENABLE;
			attr.core_mmap = PA_ENABLE;
			attr.core_shm = PA_ENABLE;

			rc = setattr(-1, &attr, sizeof(procattr_t));
			if (rc != 0) {
				if (ENOSYS == errno) {
					/* CMVC 176613: we can get here if the machine was updated from 6100-04 to 6100-05 without a subsequent reboot,
					 * in which case, behave the same as if we're running on an AIX version which does not support proc_setattr() */
				} else {
					int32_t error = errno; /* Save errno for past closing the shared library. */
					portLibrary->sl_close_shared_library(portLibrary, handle);
					portLibrary->error_set_last_error(portLibrary, error, OMRPORT_ERROR_STARTUP_AIX_PROC_ATTR);
					return OMRPORT_ERROR_STARTUP_AIX_PROC_ATTR;
				}
			} else {
				/* successfully set the process attributes */
				portLibrary->portGlobals->control.aix_proc_attr = 0;
			}
		} else {
			/* do not return error as we may be running on an AIX version which does not support proc_setattr() */
		}
		portLibrary->sl_close_shared_library(portLibrary, handle);
	}
#endif

	/* We can only get here omrdump_startup completed successfully */

	return 0;
}

void
omrdump_shutdown(struct OMRPortLibrary *portLibrary)
{
}
