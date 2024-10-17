/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/*
 * ===========================================================================
 * Module Information:
 *
 * DESCRIPTION:
 * Generic ASCII to EBCDIC character conversion. This file defines
 * a number of "wrapper" functions to transparently convert between
 * ASCII and EBCDIC. Each function calls a platform specific function
 * (defined elsewhere) to perform the actual conversion.
 * ===========================================================================
 */

/*
 * ======================================================================
 * Disable the redefinition of the system IO functions, this
 * prevents to ATOE functions calling themselves.
 * ======================================================================
 */
#undef IBM_ATOE

/*
 * ======================================================================
 * Include all system header files.
 * ======================================================================
 */
#if defined(__open_xl__)
#define _EXT
#endif /* defined(__open_xl__) */
#define _OPEN_SYS_FILE_EXT  /* For SETCVTOFF */    /*ibm@57265*/
#include <unistd.h>
#include <fcntl.h>          /* <--SETCVTOFF in here */
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <netdb.h>
#include <dll.h>
#include <spawn.h>
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/ipc.h>
#include <strings.h>
#include <limits.h>              /*ibm@52465*/
#include <_Ccsid.h>
#include <sys/statvfs.h>

#if 1 /* msf - did not find this stuff in LE headers */
                                                   /*ibm@57265 start*/
int fcntl_init = 0;       /* Global flag that says the OS release    */
                          /* has been tested for.                    */
int use_fcntl  = 0;       /* Is SETCVTOFF flag supported in fcntl()? */
                          /* use_fcntl flag set TRUE if yes.         */
#define MIN_FCNTL_LEVEL  0x41020000  /* This is the lowest version   */
                                     /* of OS that supports the use  */
                                     /* of SETCVTOFF flag in fcntl().*/

/* Macros for checking and setting global flags for use of fcntl()   */
/* and turning autoconversion off.                                   */
#define check_fcntl_init()                                           \
    /* Has the use of fcntl been checked yet?  */                    \
    /* If not, check to see if we have the     */                    \
    /* right OS level that supports the use of */                    \
    /* SETCVTOFF flag to prevent autoconversion*/                    \
    /* of a file.                              */                    \
    if (!fcntl_init)                                                 \
      {                                                              \
        if (MIN_FCNTL_LEVEL <= __librel())                           \
          use_fcntl = 1;  /* Yes, SETCVTOFF supported */             \
        fcntl_init = 1;   /* Turn on global checked flag */          \
      }

#define set_SETCVTOFF()                                              \
	struct f_cnvrt std_cnvrt;                                    \
        std_cnvrt.cvtcmd = SETCVTOFF;                                \
        std_cnvrt.pccsid = 0;                                        \
        std_cnvrt.fccsid = 0;                                        \
        /* Turn of autoconversion for this file. */                  \
        fcntl( fd, F_CONTROL_CVT, &std_cnvrt);
                                                   /*ibm@57265 end*/
#else

#define check_fcntl_init() (1)
#define set_SETCVTOFF()
#define use_fcntl (0)

#endif

/* Whether file tagging enabled by user */
static int fileTaggingEnabled = 0;
/* CCSID for new files */
static __ccsid_t newFileCCSID = 0;


/*
 * ======================================================================
 * Trace logging?
 * ======================================================================
 */
#ifdef LOGGING
#include <dg_defs.h>
#else
#define Log(level,message)
#define Log1(level,message,x1)
#endif


/*
 * ======================================================================
 * ASCII<=>EBCDIC translate tables built using iconv()
 * ======================================================================
 */
#define BUFLEN 6144
#define CONV_TABLE_SIZE 256
char a2e_tab[CONV_TABLE_SIZE];
char e2a_tab[CONV_TABLE_SIZE];

/*
 * ======================================================================
 * Define ae2,e2a,a2e_string, e2a_string
 * ======================================================================
 */
#include "atoe.h"

int   atoe_vsnprintf(char *str, size_t count, const char *fmt, va_list args);
int   atoe_fprintf(FILE *, const char *, ...);
struct passwd *etoa_passwd(struct passwd *e_passwd);

/*
 * ======================================================================
 * A simple linked list of ascii environment variable entries;
 * each entry is added as a result of a getenv() or putenv() as
 * reflected through the EBCDIC environment maintained by the system.
 * This avoids storage leaks with repeated calls which refer to the
 * same variable.
 * ======================================================================
 */
struct ascii_envvar {
	struct ascii_envvar *next;
	char *name;
	char *value;
};
typedef struct ascii_envvar envvar_t;

/* Anchor entry */
envvar_t env_anchor = {NULL, NULL, NULL};

/* Mutex for single-threading list updates */
pthread_mutex_t env_mutex;


char *
sysTranslate(const char *source, int length, char *trtable, char *xlate_buf)
{
	return sysTranslateASM(source, length, trtable, xlate_buf);
}

/**************************************************************************
 * name        - updateEnvvar
 * description - Adds, updates, or deletes an entry to the envvar_t linked
 *               list, used to not leak memory for value we return when in
 *               atoe_getenv(). Concurrency is handled by locking env_mutex.
 * parameters  - name, ASCII environment variable name
 *               value, ASCII environment variable value
 * returns     - envvar_t entry that was added or updated, or NULL if the
 *               value parameter was NULL
 *************************************************************************/
static envvar_t *
updateEnvvar(char *name, char *value)
{
	envvar_t *prev;
	envvar_t *envvar;

	/* Grab the lock */
	pthread_mutex_lock(&env_mutex);

	/* Find an existing entry, if any. */
	prev = &env_anchor;
	envvar = env_anchor.next;
	while (NULL != envvar) {
		if (0 == strcmp(name, envvar->name)) {
			break;
		}
		prev = envvar;
		envvar = envvar->next;
	}

	if (NULL == value) {
		if (NULL != envvar) {
			/* Remove the old entry from the list. */
			prev->next = envvar->next;
			free(envvar->name);
			free(envvar->value);
			free(envvar);
			envvar = NULL;
		}
	} else if (NULL != envvar) {
		/* Update the existing entry */
		if (strlen(value) <= strlen(envvar->value)) {
			strcpy(envvar->value, value);
		} else {
			free(envvar->value);
			envvar->value = strdup(value);
		}
	} else {
		/* Add a new entry to the list */
		prev->next = (envvar_t *)malloc(sizeof(envvar_t));
		envvar = prev->next;
		envvar->next = NULL;
		envvar->name = strdup(name);
		envvar->value = strdup(value);
	}

	/* Release the lock */
	pthread_mutex_unlock(&env_mutex);

	return envvar;
}

/**************************************************************************
 * name        - a2e_func (ibm@1423)
 * description - Function implementation of a2e macro, for use by DLLs
 *               that don't want to have to link with sysTranslate.o
 * parameters  - str, ASCII string to convert
 *               len, Length of str
 * returns     - Converted EBCDIC string
 *************************************************************************/
char *
a2e_func(char *str, int len)
{
	return a2e(str, len);
}

/**************************************************************************
 * name        - e2a_func (ibm@3218)
 * description - Function implementation of e2a macro, for use by DLLs
 *               that don't want to have to link with sysTranslate.o
 * parameters  - str, EBCIDC string to convert
 *               len, Length of str
 * returns     - Converted ASCII string
 *************************************************************************/
char *
e2a_func(char *str, int len)
{
	return e2a(str, len);
}

/**************************************************************************
 * name        - atoe_getenv
 * description -
 *              Returns a pointer to the value of the environment variable
 *              specified by a_name. Looks up the EBCDIC value and converts
 *              it to ASCII.
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_getenv(const char *name)
{
	envvar_t *envvar;
	char *value, *e_name, *e_value;

	Log(1, "Entry: atoe_getenv\n");

	if (name == NULL) {
		return NULL;
	}

	/* Always get the EBCIDC value from the system (in case it may have changed). */
	e_name = a2e_string((char *)name);
	e_value = getenv(e_name);
	free(e_name);

	if (NULL == e_value) {
		updateEnvvar((char *)name, NULL);
		return NULL;
	}

	/* Convert the value and update the ASCII environment variable entry. */
	value = e2a_string(e_value);
	envvar = updateEnvvar((char *)name, value);
	free(value);

	return envvar->value;
}

/**************************************************************************
 * name        - atoe_putenv
 * description -
 *              Issues putenv() to set the system EBCDIC environment
 *              then updates the ascii environment
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_putenv(const char *envstr)
{
	char *w_envstr;
	int result;

	Log(1, "Entry: atoe_putenv\n");

	/* Set the system EBCDIC environment */
	w_envstr = a2e_string((char *)envstr);

	/* note that this causes a leak - z/OS needs to have the memory, so we copy here. */
	result = putenv(w_envstr);

	/*
	 * Note: We don't add / update the entry in the envvar_t list, as that list is only
	 * to keep track of memory we allocated for getenv(), and is not used for lookups.
	 */

	return result;
}

/**************************************************************************
 * name        - atoe_perror
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
void
atoe_perror(const char *string)
{
	char *e;

	Log(1, "Entry: atoe_perror\n");

	e = a2e_string((char *)string);
	perror(e);

	free(e);

	return;
}

/**************************************************************************
 * name        - atoe_enableFileTagging
 * description - Enables tagging. CCSID is determined by
 *               atoe_determineFileTaggingCcsid()
 * parameters  - ccsid - Required CCSID for file tags
 * returns     - none
 *************************************************************************/
void
atoe_enableFileTagging(void)
{
	fileTaggingEnabled = 1;
}


/**************************************************************************
 * name        - atoe_enableFileTagging
 * description - Enables tagging of all new files to the specified ccsid
 * parameters  - ccsid - Required CCSID for file tags
 * returns     - none
 *************************************************************************/
void
atoe_setFileTaggingCcsid(void *pccsid)
{
	__ccsid_t ccsid = *(__ccsid_t *) pccsid;
	newFileCCSID = ccsid;

	if (fileTaggingEnabled) {
#pragma convlit(suspend)
		const char *dllname = "libwrappers.so";
		const char *fnname  = "atoe_enableFileTagging";
		dllhandle *libwrappersDll;
		void (*patoe_enableFileTagging)();

		/* Handle libwrappers.so:atoe_enableFileTagging */
		libwrappersDll = dllload(dllname);
		if (libwrappersDll == NULL) {
			perror(0);
			printf("atoe_enableFileTagging: %s not found\n", dllname);
		} else {
			patoe_enableFileTagging = (void (*)()) dllqueryfn(libwrappersDll, fnname);
			if (patoe_enableFileTagging == NULL) {
				perror(0);
				printf("atoe_enableFileTagging: %s not found\n", fnname);
			} else {
				patoe_enableFileTagging(newFileCCSID);
			}
		}
#pragma convlit(resume)
	}
}


/**************************************************************************
 * name        - fileTagRequired
 * description - Determines whether or not a specified file needs tagging
 * parameters  - filename (platform encoded)
 * returns     - 1 if file tagging enabled and file doesn't already exist.
 *               0 otherwise
 *************************************************************************/
static int
fileTagRequired(const char *filename)
{
	struct stat sbuf;
	if (fileTaggingEnabled && newFileCCSID != 0 &&
		(stat(filename, &sbuf) == -1 && errno == ENOENT)) {
		return 1;
	}
	return 0;
}

/**************************************************************************
 * name        - setFileTag
 * description - Sets the file tag to newFileCCSID for the specified file
 *               descriptor
 * parameters  - fd - open file descriptor of file for tagging
 * returns     - none
 *************************************************************************/
static void
setFileTag(int fd)
{
	struct file_tag tag;
	tag.ft_ccsid = newFileCCSID;
	tag.ft_txtflag = 1;
	tag.ft_deferred = 0;
	tag.ft_rsvflags = 0;
	fcntl(fd, F_SETTAG, &tag);
}

/**************************************************************************
 * name        - atoe___CSNameType
 * description - Wrapper for __CSNameType to return the encoding type for
 *               the specified codeset name
 * parameters  - codesetName - codeset name as an ASCII text string
 * returns     - codeset type: _CSTYPE_EBCDIC or _CSTYPE_ASCII
 *************************************************************************/
__csType
atoe___CSNameType(char *codesetName)
{
	char *estr;
	__csType ret;
	estr = a2e_string(codesetName);
	ret = __CSNameType(estr);
	free(estr);
	return ret;
}

/**************************************************************************
 * name        - atoe___toCcsid
 * description - Wrapper for __toCcsid to return the CCSID for the
 *               specified codeset name
 * parameters  - codesetName - codeset name as an ASCII text string
 * returns     - CCSID
 *************************************************************************/
__ccsid_t
atoe___toCcsid(char *codesetName)
{
	char *estr;
	__ccsid_t ret;
	estr = a2e_string(codesetName);
	ret = __toCcsid(estr);
	free(estr);
	return ret;
}


/**************************************************************************
 * name        - atoe_tagged_open
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_open(const char *fname, int options, ...)
{
	int fd;
	int mode;
	int tagFile;
	va_list args;
	char *f;

	Log(1, "Entry: atoe_open\n");

	check_fcntl_init();                            /*ibm@57265*/

	/* Convert filename to ebcdic before calling fileTagRequired() - CMVC 191919/199888 */
	f = a2e_string((char *)fname);

	/* See if the file needs to be tagged */
	tagFile = fileTagRequired(f);

	va_start(args, options);

	if (options & O_CREAT) {
		mode = va_arg(args, int);
	} else {
		mode = 0;
	}

	va_end(args);

	fd = open(f, options, mode);

	/*ibm@57265 start*/
	if ((fd != -1) &&                     /* file descriptor ok? */
		(use_fcntl)) {                    /* OS level is ok      */
		set_SETCVTOFF();
	}

	if (fd != -1) { /* file descriptor ok? */
		if (tagFile) {
			setFileTag(fd);
		}
	}
	/*ibm@57265 end*/

	free(f);

	return fd;
}

/**************************************************************************
 * name        - atoe_open_notag
 * description - Alternative version of atoe_open() which does not set the
 *               file tag - i.e does not call fcntl(fd,F_SETTAG..) Used for
 *               non-application files eg dumps, which are always in EBCDIC.
 * parameters  - fname - file name as ASCII string
 *             - options - file open options/flags
 * returns     - file descriptor
 *************************************************************************/
int
atoe_open_notag(const char *fname, int options, ...)
{
	int fd;
	int mode;
	va_list args;
	char *f;

	check_fcntl_init();

	f = a2e_string((char *)fname);

	va_start(args, options);
	if (options & O_CREAT) {
		mode = va_arg(args, int);
	} else {
		mode = 0;
	}
	va_end(args);

	fd = open(f, options, mode);

	if ((fd != -1) && (use_fcntl)) {
		set_SETCVTOFF();
	}

	free(f);
	return fd;
}

/**************************************************************************
 * name        - atoe_tempnam
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_tempnam(const char *dir, char *pfx)
{
	char *tempfn = 0;
	char *a = 0;
	char *d = a2e_string((char *)dir);
	char *p = a2e_string(pfx);

	Log(1, "Entry: atoe_tempnam\n");

	if ((tempfn = tempnam(d, p)) == 0) {
		atoe_fprintf(stderr, "Creation of temp file %s/%s failed.\n", dir, pfx);
	} else {
		a = e2a_string(tempfn);
		free(tempfn);
	}

	free(d);
	free(p);

	return a;
}


/**************************************************************************
 * name        - atoe_stat
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_stat(const char *pathname, struct stat *sbuf)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_stat\n");

	e = a2e_string((char *)pathname);
	rc = stat(e, sbuf);

	free(e);

	return rc;
}


/**************************************************************************
 * name        - atoe_statvfs
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_statvfs(const char *pathname, struct statvfs *buf)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_statvfs\n");

	e = a2e_string((char *)pathname);
	rc = statvfs(e, buf);

	free(e);

	return rc;
}

/**************************************************************************
 * name        - atoe_lstat
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_lstat(const char *pathname, struct stat *sbuf)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_stat\n");

	e = a2e_string((char *)pathname);
	rc = lstat(e, sbuf);

	free(e);

	return rc;
}


/**************************************************************************
 * name        - atoe_fopen
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
FILE *
atoe_fopen(const char *filename, char *mode)
{
	FILE *outfp;
	char *f, *m;
	int  fd = -1;         /* file descriptor */    /*ibm@57265*/

	Log(1, "Entry: atoe_fopen\n");

	check_fcntl_init();                            /*ibm@57265*/

	f = a2e_string((char *)filename);
	m = a2e_string(mode);
	outfp = fopen(f, m);

	/*ibm@57265 start*/
	if ((outfp != NULL) &&                        /* fopen() ok?    */
		(use_fcntl)    &&                         /* OS level is ok */
		((fd = fileno(outfp)) != -1)) {           /* have a file descriptor? */
		set_SETCVTOFF();
	}
	/*ibm@57265 end*/

	free(f);
	free(m);

	return outfp;
}

/**************************************************************************
 * name        - atoe_freopen
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
FILE *
atoe_freopen(const char *filename, char *mode, FILE *stream)
{
	FILE *outfp;
	char *f, *m;
	int  fd = -1;         /* file descriptor */    /*ibm@57265*/

	Log(1, "Entry: atoe_freopen\n");

	check_fcntl_init();                            /*ibm@57265*/

	f = a2e_string((char *)filename);
	m = a2e_string(mode);
	outfp = freopen(f, m, stream);

	/*ibm@57265 start*/
	if ((outfp != NULL) &&                        /* fopen() ok?    */
		(use_fcntl)    &&                         /* OS level is ok */
		((fd = fileno(outfp)) != -1)) {           /* have a file descriptor? */
		set_SETCVTOFF();
	}
	/*ibm@57265 end*/

	free(f);
	free(m);

	return outfp;
}


/**************************************************************************
 * name        - atoe_mkdir
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_mkdir(const char *pathname, mode_t mode)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_mkdir\n");

	e = a2e_string((char *)pathname);
	rc = mkdir(e, mode);

	free(e);

	return rc;
}

/**************************************************************************
 * name        - atoe_remove
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_remove(const char *pathname)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_remove\n");

	e = a2e_string((char *)pathname);
	rc = remove(e);

	free(e);

	return rc;
}

/**************************************************************************
 * name        - atoe_strerror
 * description -
 * parameters  -
 * returns     -
 * re-written by ibm@56159.1
 *************************************************************************/
#define ERRNO_CACHE_SIZE   250
typedef struct ascii_errno_t {
	int   errnum;
	char *ascii_msg;
} ascii_errno_t;
static ascii_errno_t errno_cache[ERRNO_CACHE_SIZE];
static int errno_cache_next = 0;
static pthread_mutex_t strerror_mutex = PTHREAD_MUTEX_INITIALIZER;
static int strerror_initialized = 0;
char *
atoe_strerror(int errnum)
{
	char *a, *e;
	int index;

	Log(1, "Entry: atoe_strerror\n");

	pthread_mutex_lock(&strerror_mutex);
	if (0 == strerror_initialized) {
		for (index = 0; index < ERRNO_CACHE_SIZE; index++) {
			errno_cache[index].errnum = -1;
			errno_cache[index].ascii_msg = 0;
		}
		strerror_initialized = 1;
	}

	for (index = 0;
		 index < ERRNO_CACHE_SIZE, errno_cache[index].errnum != -1;
		 index++) {
		if (errnum == errno_cache[index].errnum) {
			pthread_mutex_unlock(&strerror_mutex);
			return errno_cache[index].ascii_msg;
		}
	}

	e = strerror(errnum);
	a = e2a_string(e);

	errno_cache[errno_cache_next].errnum = errnum;
	if (errno_cache[errno_cache_next].ascii_msg) {
		free(errno_cache[errno_cache_next].ascii_msg);
	}
	errno_cache[errno_cache_next].ascii_msg = a;
	if (++errno_cache_next == ERRNO_CACHE_SIZE) {
		errno_cache_next = 0;
	}
	pthread_mutex_unlock(&strerror_mutex);

	return a;
}

/**************************************************************************
 * name        - atoe_getcwd
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_getcwd(char *buffer, size_t size)
{
	char *abuf;

	Log(1, "Entry: atoe_getcwd\n");

	if (getcwd(buffer, size) == NULL) {    /*ibm@9777*/
		buffer = NULL;
	} else {
		abuf = e2a(buffer, size);
		memcpy(buffer, abuf, size);
		free(abuf);
	}

	return buffer;
}

/**************************************************************************
 * name        - atoe_fgets
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_fgets(char *buffer, int num, FILE *file)
{
	char *abuf;

	Log(1, "Entry: atoe_fgets\n");

	if (fgets(buffer, num, file)) {
		int len = strlen(buffer);
		abuf = e2a(buffer, len);
		memcpy(buffer, abuf, len);

		free(abuf);
		return buffer;
	}

	return (char *)0;
}


/**************************************************************************
 * name        - atoe_unlink
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_unlink(const char *pathname)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_unlink\n");

	e = a2e_string((char *)pathname);
	rc = unlink(e);

	free(e);

	return rc;
}


/**************************************************************************
 * name        - atoe_unlink
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_rmdir(const char *pathname)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_rmdir\n");

	e = a2e_string((char *)pathname);
	rc = rmdir(e);

	free(e);

	return rc;
}


/**************************************************************************
 * name        - atoe_access
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_access(const char *pathname, int how)
{
	int rc;
	char *e;

	Log(1, "Entry: atoe_access\n");

	e = a2e_string((char *)pathname);
	rc = access(e, how);

	free(e);

	return rc;
}


/**************************************************************************
 * name        - atoe_opendir
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
DIR *
atoe_opendir(const char *dirname)
{
	DIR *dir;
	char *e;

	Log(1, "Entry: atoe_opendir\n");

	e = a2e_string((char *)dirname);
	dir = opendir(e);

	free(e);

	return dir;
}


/**************************************************************************
 * name        - atoe_readdir
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct dirent *
atoe_readdir(DIR *dir)
{
	struct dirent *d;
	char *a;

	Log(1, "Entry: atoe_readdir\n");

	d = readdir(dir);

	if (d == NULL) {
		return NULL;
	}

	a = e2a_string(d->d_name);

	strcpy(d->d_name, a);
	free(a);

	return d;
}

/**************************************************************************
 * name        - atoe_realpath
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_realpath(const char *file_name, char *resolved_name)
{
	char e_resolved_name[MAXPATHLEN];
	char *e_file_name, *p;

	Log(1, "Entry: atoe_realpath\n");

	e_file_name = a2e_string((char *)file_name);                   /*ibm@34803*/
	p = realpath(e_file_name, e_resolved_name);                    /*ibm@34803*/

	free(e_file_name);

	if (NULL == p) {
		return p;
	}

	p = e2a_string(e_resolved_name);
	strcpy(resolved_name, p);
	free(p);

	return resolved_name;
}

/**************************************************************************
 * name        - atoe_rename
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_rename(const char *oldname, const char *newname)
{
	int rc;
	char *o, *n;

	Log(1, "Entry: atoe_rename\n");

	o = a2e_string((char *)oldname);
	n = a2e_string(newname);
	rc = rename(o, n);

	free(o);
	free(n);

	return rc;
}


/**************************************************************************
 * name        - atoe_getpwuid
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct passwd *
atoe_getpwuid(uid_t uid)
{
	Log(1, "Entry: atoe_getpwuid\n");

	return etoa_passwd(getpwuid(uid));
}

/**************************************************************************
 * name        - atoe_getpwnam
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct passwd *
atoe_getpwnam(const char *name)
{
	struct passwd *e_passwd;
	char *e_name;

	Log(1, "Entry: atoe_getpwnam\n");

	e_name = a2e_string((char *)name);
	e_passwd = getpwnam(e_name);
	free(e_name);

	return etoa_passwd(e_passwd);
}

/**************************************************************************
 * name        - etoa_passwd
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct passwd *
etoa_passwd(struct passwd *e_passwd)
{
	struct passwd *a_passwd = NULL;

	if (e_passwd != NULL) {
		a_passwd = (struct passwd *)malloc(sizeof(struct passwd));

		if (a_passwd != NULL) {
			a_passwd->pw_name  = e2a_string(e_passwd->pw_name);
			a_passwd->pw_uid   = e_passwd->pw_uid;
			a_passwd->pw_gid   = e_passwd->pw_gid;
			a_passwd->pw_dir   = e2a_string(e_passwd->pw_dir);
			a_passwd->pw_shell = e2a_string(e_passwd->pw_shell);
		}
	}

	return a_passwd;
}

/**************************************************************************
 * name        - atoe_getlogin
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_getlogin(void)
{
	char *e_login;
	char *a_login = NULL;

	Log(1, "Entry: atoe_getlogin\n");

	e_login = getlogin();

	if (e_login != NULL) {
		a_login = e2a_string(e_login);
	}

	return a_login;
}

/**************************************************************************
 * name        - etoa_group
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct group *
e2a_group(struct group *e_group)
{
	struct group *a_group = NULL;

	if (e_group != NULL) {
		a_group = (struct group *)malloc(sizeof(struct group));

		if (a_group != NULL) {
			char **e_member = NULL;
			char **a_member = NULL;
			int arraySize = 0;
			a_group->gr_name  = e2a_string(e_group->gr_name);
			a_group->gr_gid   = e_group->gr_gid;
			for (e_member = e_group->gr_mem; *e_member != NULL; e_member++) {
				arraySize++;
			}
			a_group->gr_mem = (char **)malloc((arraySize + 1) * sizeof(char *));
			if (a_group->gr_mem != NULL) {
				for (e_member = e_group->gr_mem, a_member = a_group->gr_mem;
					 *e_member != NULL;
					 e_member++, a_member++) {
					*a_member = e2a_string(*e_member);
				}
				*a_member = NULL;
			}
		}
	}

	return a_group;
}

/**************************************************************************
 * name        - atoe_getgrgid
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct group *
atoe_getgrgid(gid_t gid)
{
	struct group *e_group;
	struct group *a_group = NULL;

	Log(1, "Entry: atoe_getgrgid\n");

	e_group = getgrgid(gid);

	if (e_group != NULL) {
		a_group = e2a_group(e_group);
	}

	return a_group;
}

/**************************************************************************
 * name        -
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_putchar(int ch)
{
	Log(1, "Entry: atoe_putchar\n");

	return putchar((int)a2e_tab[ch]);
}


/**************************************************************************
 * name        -
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_fprintf(FILE *file, const char *ascii_chars, ...)
{
	va_list args;
	char buf[BUFLEN];
	char *ebuf;
	int len;

	va_start(args, ascii_chars);

	len = atoe_vsnprintf(buf, BUFLEN, ascii_chars, args);

	/* Abort if failed... */
	if (len == -1) {
		return len;
	}

	ebuf = a2e(buf, len);
#pragma convlit(suspend)
	len = fprintf(file, "%s", ebuf);
#pragma convlit(resume)
	free(ebuf);

	va_end(args);

	return len;
}


/**************************************************************************
 * name        -
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_printf(const char *ascii_chars, ...)
{
	va_list args;
	char buf[BUFLEN];
	char *ebuf;
	int len;

	va_start(args, ascii_chars);

	len = atoe_vsnprintf(buf, BUFLEN, ascii_chars, args);

	/* Abort if failed... */
	if (len == -1) {
		return len;
	}

	ebuf = a2e(buf, len);
#pragma convlit(suspend)
	len = printf("%s", ebuf);
#pragma convlit(resume)
	free(ebuf);

	va_end(args);

	return len;
}

/***************************************************************ibm@3753**
 * name        - std_sprintf
 * description -
 *              This function does not need to convert ascii -> EBCDIC
 *              as there is no IO
 * parameters  -
 * returns     -
 *************************************************************************/
int
std_sprintf(const char *buf, char *ascii_chars, ...)
{
	int len;
	va_list args;
	va_start(args, ascii_chars);

	len = sprintf((char *)buf, ascii_chars, args);

	va_end(args);

	return len;
}

/**************************************************************************
 * name        - atoe_sprintf
 * description -
 *              This function does not need to convert ascii -> EBCDIC
 *              as there is no IO
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_sprintf(char *buf, const char *ascii_chars, ...)
{
	int len;
	char wrkbuf[BUFLEN];

	va_list args;
	va_start(args, ascii_chars);

	len = atoe_vsnprintf(wrkbuf, BUFLEN, ascii_chars, args);

	va_end(args);
	if (-1 == len) {
		return len;
	}

	strcpy((char *)buf, wrkbuf);

	return len;
}

/**************************************************************************
 * name        - atoe_snprintf
 * description -
 *              This function does not need to convert ascii -> EBCDIC
 *              as there is no IO
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_snprintf(char *buf, size_t buflen, const char *ascii_chars, ...)
{
	int len;

	va_list args;
	va_start(args, ascii_chars);

	len = atoe_vsnprintf(buf, buflen, ascii_chars, args);

	va_end(args);
	if (-1 == len) {
		return len;
	}

	return len;
}

/**************************************************************************
 * name        - atoe_vfprintf
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_vfprintf(FILE *file, const char *ascii_chars, va_list args)
{
	char buf[BUFLEN];
	char *ebuf;
	int len;

	len = atoe_vsnprintf(buf, BUFLEN, ascii_chars, args);

	if (len == -1) {
		return len;
	}

	ebuf = a2e(buf, len);
#pragma convlit(suspend)
	len = fprintf(file, "%s", ebuf);
#pragma convlit(resume)

	free(ebuf);

	return len;
}

/**************************************************************************
 * name        - atoe_vprintf
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_vprintf(const char *ascii_chars, va_list args)
{
	return atoe_vfprintf(stdout, ascii_chars, args);
}

/**************************************************************************
 * name        - atoe_vsprintf                                     ibm@2580
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_vsprintf(char *target, const char *ascii_chars, va_list args)
{
	char buf[BUFLEN];                                     /*ibm@029013*/
	int  bsize = 0;                                       /*ibm@029013*/

	bsize = atoe_vsnprintf(buf, BUFLEN, ascii_chars, args); /*ibm@029013*/
	if (-1 == bsize) {
		return bsize;
	}
	strcpy(target, buf);                                  /*ibm@029013*/
	return bsize;                                         /*ibm@029013*/
}

/**************************************************************************
 * name        - atoe_sscanf                                      ibm@2609
 * description -
 * parameters  -
 * returns     -
 * RESTRICTIONS - At the time of writing, there is no requirement to support
 *                character or string formatting, so I have opted to avoid
 *                the pain of trawling the format string along with the
 *                argument list in order to convert characters/strings from
 *                EBCDIC back to ASCII after calling the RTL sscanf().
 *************************************************************************/
int
atoe_sscanf(const char *buffer, const char *format, va_list args)
{
	char *e_buffer = a2e_string((char *)buffer);
	char *e_format = a2e_string((char *)format);
	int len = sscanf((const char *)e_buffer, (const char *)e_format, args);
	free(e_buffer);
	free(e_format);
	return len;
}

/**************************************************************************
 * name        - atoe_strftime
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
size_t
atoe_strftime(char *buf, size_t buflen,
			  const char *format, const struct tm *timeptr)
{
	size_t num;
	char *e, *a;

	Log(1, "Entry: atoe_strftime\n");

	e = a2e_string((char *)format);
	num = strftime(buf, buflen, e, timeptr);
	a = e2a(buf, num);
	memcpy(buf, a, num);

	free(e);
	free(a);

	return num;
}

/**************************************************************************
 * name        - atoe_fwrite
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
size_t
atoe_fwrite(const void *buffer, size_t size, size_t count, FILE *stream)
{
	int numwritten;
	char *e;

	Log(1, "Entry: atoe_fwrite\n");

	e = a2e((void *)buffer, size * count);    /*ibm@2795*/
	numwritten = fwrite(e, size, count, stream);

	free(e);

	return numwritten;
}

/**************************************************************************
 * name        - atoe_fread
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
size_t
atoe_fread(void *buffer, size_t size, size_t count, FILE *stream)
{
	int numread;
	char *a;

	Log(1, "Entry: atoe_fread\n");

	numread = fread(buffer, size, count, stream);
	a = e2a((char *)buffer, numread);
	memcpy(buffer, a, numread);

	free(a);

	return numread;
}

/**************************************************************************
 * name        - atoe_system
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_system(char *cmd)
{
	char *eb;
	int result;

	Log(1, "Entry: atoe_system\n");

	eb = a2e_string(cmd);
	result = system(eb) ;

	free(eb);

	return result;
}

/**************************************************************************
 * name        - atoe_setlocale
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_setlocale(int category, const char *locale)
{
	char *eb, *result;

	Log(1, "Entry: atoe_setlocale\n");

	/* CMVC 143879: a2e_string converts a NULL pointer to an empty string */
	eb = (NULL == locale) ? NULL : a2e_string((char *)locale);

	result = setlocale(category, eb) ;

	free(eb);

	return e2a_string(result);
}

/**************************************************************************
 * name        - atoe_ctime
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
atoe_ctime(const time_t *t_in)
{
	char *eb;

	Log(1, "Entry: atoe_ctime\n");

	eb = ctime(t_in);

	return e2a_string(eb);
}

/**************************************************************************
 * name        - atoe_strtod
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
double
atoe_strtod(char *s, char **p)
{
	char *sebcdic;
	char *pebcdic;
	double d;

	Log(1, "Entry: atoe_strtod\n");

	sebcdic = a2e_string(s);
	d = strtod(sebcdic, &pebcdic);
	if (p != NULL) {
		*p = s + (pebcdic - sebcdic);
	}
	free(sebcdic);

	return d;
}

/**************************************************************************
 * name        - atoe_strtol                                       ibm@3139
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_strtol(const char *s, char **p, int b)
{
	char *sebcdic;
	char *pebcdic;
	int  i;

	Log(1, "Entry: atoe_strtol\n");

	sebcdic = a2e_string((char *)s);
	i = strtol((const char *)sebcdic, &pebcdic, b);
	if (p != NULL) {
		*p = (char *)s + (pebcdic - sebcdic);
	}
	free(sebcdic);

	return i;
}

/**************************************************************************
 * name        - atoe_strtoul                                      ibm@4968
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
unsigned long
atoe_strtoul(const char *s, char **p, int b)
{
	char *sebcdic;
	char *pebcdic;
	unsigned long  i;

	Log(1, "Entry: atoe_strtoul\n");

	sebcdic = a2e_string((char *)s);
	i = strtoul((const char *)sebcdic, &pebcdic, b);
	if (p != NULL) {
		*p = (char *)s + (pebcdic - sebcdic);
	}
	free(sebcdic);

	return i;
}

/**************************************************************************
 * name        - atoe_strtoull
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
unsigned long long
atoe_strtoull(const char *s, char **p, int b)
{
	char *sebcdic;
	char *pebcdic;
	unsigned long long i;

	Log(1, "Entry: atoe_strtoull\n");

	sebcdic = a2e_string((char *)s);
	i = strtoull((const char *)sebcdic, &pebcdic, b);
	if (p != NULL) {
		*p = (char *)s + (pebcdic - sebcdic);
	}
	free(sebcdic);

	return i;
}

/**************************************************************************
 * name        - atoe_strcasecmp
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_strcasecmp(const char *string1, const char *string2)
{
	int return_val;
	char *ebsstr1, *ebsstr2;

	Log(1, "Entry: atoe_strcasecmp\n");

	ebsstr1 = a2e_string((char *)string1);
	ebsstr2 = a2e_string((char *)string2);

	return_val = strcasecmp(ebsstr1, ebsstr2);

	free(ebsstr1);
	free(ebsstr2);

	return return_val;
}

/**************************************************************************
 * name        - atoe_strncasecmp
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_strncasecmp(const char *string1, const char *string2, int n)
{
	int return_val;
	char *ebsstr1, *ebsstr2;

	Log(1, "Entry: atoe_strncasecmp\n");

	ebsstr1 = a2e_string((char *)string1);
	ebsstr2 = a2e_string((char *)string2);

	return_val = strncasecmp(ebsstr1, ebsstr2, n);

	free(ebsstr1);
	free(ebsstr2);

	return return_val;
}

/*****************************************************************ibm@3817
 * name        - atoe_atof
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
double
atoe_atof(char *ascii_string)
{
	double rc;
	char *ebcdic_string;

	Log(1, "Entry: atoe_atof\n");

	ebcdic_string = a2e_string(ascii_string);
	rc = atof(ebcdic_string);
	free(ebcdic_string);

	return rc;
}

/**************************************************************************
 * name        - atoe_atoi
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_atoi(char *ascii_string)
{
	int rc;
	char *ebcdic_string;

	Log(1, "Entry: atoe_atoi\n");

	ebcdic_string = a2e_string(ascii_string);
	rc = atoi(ebcdic_string);
	free(ebcdic_string);

	return rc;
}

/*****************************************************************ibm@3817
 * name        - atoe_atol
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
long
atoe_atol(char *ascii_string)
{
	long rc;
	char *ebcdic_string;

	Log(1, "Entry: atoe_atol\n");

	ebcdic_string = a2e_string(ascii_string);
	rc = atol(ebcdic_string);
	free(ebcdic_string);

	return rc;
}

/**************************************************************************
 * name        - atoe_gethostname
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_gethostname(char *name, int namelen)
{
	int rc = gethostname(name, namelen);

	char *ascii_name = e2a(name, namelen);

	strcpy(name, ascii_name);
	free(ascii_name);

	Log1(1, "Entry: atoe_gethostname: %s\n", name);

	return rc;
}

/**************************************************************************
 * name        - atoe_gethostbyname                             ibm@38180.1
 * description -
 * parameters  -
 * returns     -
 * re-written by ibm@58845
 *************************************************************************/
#define HOSTBYNAME_CACHE_SIZE   50
typedef struct hostbyname_t {
	char *hostname;
	struct hostent h;
} hostbyname_t;
static hostbyname_t hostbyname_cache[HOSTBYNAME_CACHE_SIZE];
static int hostbyname_cache_next = 0;
static pthread_mutex_t hostbyname_mutex = PTHREAD_MUTEX_INITIALIZER;
static int hostbyname_initialized = 0;
struct hostent *
atoe_gethostbyname(const char *hostname)
{
	char *h;
	struct hostent *hostptr = NULL, *hostptr_a = NULL;
	int index;

	Log1(1, "Entry: atoe_gethostbyname: %s\n", hostname);

	pthread_mutex_lock(&hostbyname_mutex);
	if (0 == hostbyname_initialized) {
		/* initialize cache in static storage */
		for (index = 0; index < HOSTBYNAME_CACHE_SIZE; index++) {
			hostbyname_cache[index].hostname = NULL;
			memset(&hostbyname_cache[index].h, 0, sizeof(struct hostent));
		}
		hostbyname_initialized = 1;
	}

	/* see if we are already using a cache entry for this hostname */
	for (index = 0;
		 index < HOSTBYNAME_CACHE_SIZE, hostbyname_cache[index].hostname != NULL;
		 index++) {
		if (0 == strcmp(hostname, hostbyname_cache[index].hostname)) {
			hostptr_a = &hostbyname_cache[index].h;
			break;
		}
	}
	if (NULL == hostptr_a) {
		/* use next cache slot for this hostname */
		if (hostbyname_cache[hostbyname_cache_next].hostname) {
			free(hostbyname_cache[hostbyname_cache_next].hostname);
		}
		hostbyname_cache[hostbyname_cache_next].hostname = strdup(hostname);
		if (!hostbyname_cache[hostbyname_cache_next].hostname) {
			pthread_mutex_unlock(&hostbyname_mutex);
			return NULL;
		}
		hostptr_a = &hostbyname_cache[hostbyname_cache_next].h;
		if (++hostbyname_cache_next == HOSTBYNAME_CACHE_SIZE) {
			hostbyname_cache_next = 0;
		}
	}

	/* convert the input hostname to ebcdic */
	h = a2e_string(hostname);
	/* and issue the system call */
	hostptr = gethostbyname(h);
	/* don't need the ebcdic copy of hostname any more */
	free(h);

	/* if the system call worked save the results, otherwise return NULL */
	if (hostptr) {
		/* if we're re-using a cache slot, free any previously saved name */
		if (hostptr_a->h_name) {
			free(hostptr_a->h_name);
		}
		/* copy the system returned hostent into our cache */
		memcpy(hostptr_a, hostptr, sizeof(struct hostent));
		/* convert the system returned name to ascii and save it in our cache */
		hostptr_a->h_name = e2a_string(hostptr->h_name);
		/* if everything worked return the address of the cache entry */
		if (hostptr_a->h_name) {
			hostptr = hostptr_a;
		} else {
			hostptr = NULL;
		}
	}

	pthread_mutex_unlock(&hostbyname_mutex);

	return hostptr;
}

/**************************************************************************
 * name        - atoe_gethostbyname_r                           ibm@38180.1
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct hostent *
atoe_gethostbyname_r(char *hostname, struct hostent *hentptr,     /*ibm@38180.1*/
					 char *buf, int bufsize, int *h_error)
{
	char *h, *h_n;
	struct hostent *hostptr;

	Log1(1, "Entry: atoe_gethostbyname_r: %s\n", hostname);       /*ibm@38180.1*/

	h = a2e_string(hostname);

	if ((hostptr = gethostbyname(h)) == NULL) {                    /*ibm.7612*/
		hentptr = NULL;                                            /*ibm.7612*/
	} else if ((strlen(hostptr->h_name) + 1) > bufsize) {          /*ibm.7612*/
		*h_error = errno = ERANGE;                                 /*ibm.7612*/
		hentptr = NULL;                                            /*ibm.7612*/
	} else {                                                       /*ibm.7612*/
		memcpy(hentptr, hostptr, sizeof(struct hostent));          /*ibm.7612*/

		h_n = e2a_string(hostptr->h_name);
		strcpy(buf, h_n);                                          /*ibm.7612*/
		free(h_n);                                                 /*ibm.7612*/

		hentptr->h_name = buf;                                     /*ibm.7612*/
	}

	free(h);

	return hentptr;                                                /*ibm.7612*/
}

/**************************************************************************
 * name        - atoe_gethostbyaddr
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct hostent *
atoe_gethostbyaddr(const void *address, int addrlen, int domain,
				   struct hostent *hentptr, char *buf, int bufsize,
				   int *h_error)
{
	char *h_n;
	struct hostent *hostptr;

	Log(1, "Entry: atoe_gethostbyaddr:\n");

	if ((hostptr = gethostbyaddr(address, addrlen, domain)) == NULL) { /*ibm.7612*/
		hentptr = NULL;                                            /*ibm.7612*/
	} else if ((strlen(hostptr->h_name) + 1) > bufsize) {          /*ibm.7612*/
		*h_error = errno = ERANGE;                                 /*ibm.7612*/
		hentptr = NULL;                                            /*ibm.7612*/
	} else {                                                       /*ibm.7612*/
		memcpy(hentptr, hostptr, sizeof(struct hostent));          /*ibm.7612*/

		h_n = e2a_string(hostptr->h_name);
		strcpy(buf, h_n);                                          /*ibm.7612*/
		free(h_n);                                                 /*ibm.7612*/

		hentptr->h_name = buf;                                     /*ibm.7612*/
	}

	return hentptr;                                                /*ibm.7612*/
}

/**************************************************************************
 * name        - atoe_getprotobyname
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
struct protoent *
atoe_getprotobyname(const char *name)
{
	struct protoent *pent;
	char *e_name, *p_n;

	Log1(1, "Entry: atoe_getprotobyname: %s\n", name);

	e_name = a2e_string((char *)name);

	if ((pent = getprotobyname(e_name)) != 0) {
		p_n = e2a_string(pent->p_name);
		pent->p_name = p_n;
	}

	free(e_name);

	return pent;
}

/**************************************************************************
 * name        - atoe_inet_addr
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
unsigned long
atoe_inet_addr(char *hostname)
{
	unsigned long iaddr = -1;
	char *h = a2e_string(hostname);

	Log1(1, "Entry: atoe_inet_addr: %s\n", hostname);

	iaddr = inet_addr(h);

	free(h);

	return iaddr;
}

/**************************************************************************
 * name        - atoe_getaddrinfo
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **result)
{
	int rc;
	char *e, *f;

	Log(1, "Entry: atoe_getaddrinfo\n");

	e = a2e_string((char *)nodename);
	f = a2e_string((char *)servname);
	rc = getaddrinfo(e, f, hints, result);

	free(e);
	free(f);

	return rc;
}

/**************************************************************************
 * name        - atoe_getaddrinfo
 * description - given a sockaddr, return the host and service name
 * parameters  -
 * returns     -
 *************************************************************************/
int
atoe_getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)
{
	int rc;
	char *host_ascii, *serv_ascii;

	Log(1, "Entry: atoe_getnameinfo\n");

	rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);

	if (0 != hostlen) {
		host_ascii = e2a_string(host);
		strncpy(host, host_ascii, hostlen);
		host[hostlen - 1] = '\0';
		free(host_ascii);
	}

	if (0 != servlen) {
		serv_ascii = e2a_string(serv);
		strncpy(serv, serv_ascii, servlen);
		serv[servlen - 1] = '\0';
		free(serv_ascii);
	}

	return rc;
}

#define _ISALNUM_ASCII  0x0001
#define _ISALPHA_ASCII  0x0002
#define _ISCNTRL_ASCII  0x0004
#define _ISDIGIT_ASCII  0x0008
#define _ISGRAPH_ASCII  0x0010
#define _ISLOWER_ASCII  0x0020
#define _ISPRINT_ASCII  0x0040
#define _ISPUNCT_ASCII  0x0080
#define _ISSPACE_ASCII  0x0100
#define _ISUPPER_ASCII  0x0200
#define _ISXDIGIT_ASCII 0x0400

#define _XUPPER_ASCII   0xdf                                        /*ibm@4345*/
#define _XLOWER_ASCII   0x20                                        /*ibm@4345*/

/* Check for iconv_init() already done   */
int iconv_initialized = 0;

/* Table for integer test macros      */
int _ascii_is_tab[CONV_TABLE_SIZE];

#pragma convlit(suspend)
/**************************************************************************
 * name        - iconv_init
 * description -
 *              Allocate code conversion descriptors for iconv()
 *              and build translate tables.
 * parameters  -
 * returns     -
 *************************************************************************/
int
iconv_init(void)
{
	char *asciiSet = "ISO8859-1";
	char *defaultSet = "IBM-1047";
	char *ebcdicSet, *inptr, *outptr;
	size_t inleft, outleft;
	iconv_t atoe_cd = (iconv_t)(-1);                         /*ibm@33935*/
	iconv_t etoa_cd = (iconv_t)(-1);                         /*ibm@33935*/
	char init_tab[CONV_TABLE_SIZE];
	int i;

	/* Need to save start locale setting                            ibm@38796 */
	char *slc = NULL;
	/* Get the current locale setting                               ibm@38796 */
	char *clc = setlocale(LC_ALL, NULL);

	/* Handle initialization of libwrappers.so iconv as well */
	const char *dllname = "libwrappers.so";
	const char *fnname  = "iconv_init";
	dllhandle *libwrappersDll;
	int (*piconv_init)();

	if (iconv_initialized) {
		return 0;
	}

	/*
	 * Test whether we're running with an DBCS codepage
	 * If so, use default IBM-1047 EBCDIC SBCS codepage to build translate table.
	 */
	{
		char a[] = {97, 0}; /* ASCII "a" */

		/* Copy the start locale setting to reset it later          ibm@38796 */
		slc = strdup(clc);
		if (!slc) {
			return -1;
		}

		/* Set the locale to whatever is set by the env. vars       ibm@38796 */
		setlocale(LC_ALL, "") ;

		if ((__atoe(a) < 1) && (errno == EINVAL)) {
			/* The current locale does not describe a single-byte character set.*/
			/* This test does not always give the correct result - see below ibm@12464*/
			ebcdicSet = defaultSet;
		} else {
			char *encoding = NULL;                                    /*ibm@12741*/
			char *lc;
			char *p;
			int i;

			lc = getenv("LC_ALL");
			if (lc == NULL) {
				lc = getenv("LC_CTYPE");
				if (lc == NULL) {
					lc = getenv("LANG");
				}
			}

			if (lc != NULL) {

				/* We now have a string with the format
				 * language_region.encoding@variant any part of which
				 * may be missing. We need to extract the encoding part
				 */

				lc = strdup(lc);
				if ((p = strchr(lc, '.')) != NULL) {
					encoding = p + 1;                                 /*ibm@12741*/
				}
				if ((p = strchr(lc, '@')) != NULL) {
					*p = '\0';
				}
			}

			/* Translate alias to encoding */
#if 0 /* msf - the encoding_names code can not be pulled into here - need to write this yourself if needed */

			if (encoding == NULL) {                                   /*ibm@12741*/
				if (lc != NULL) {
					/* lc is in EBCDIC, encoding_names is in ASCII */
					char *temp, *ae_lc;
					ae_lc = strdup(lc);
					__etoa(ae_lc);
					for (i = 0; strcmp(encoding_names[i], ""); i += 2) {
						if (!strcasecmp(ae_lc, encoding_names[i])) {
							ae_lc = strdup(encoding_names[i + 1]);            /*ibm@56369.1*/
							break;
						}
					}
					__atoe(ae_lc);
					/* ae_lc has the Java name, get the equivalent IBM name */
					temp = strstr(ae_lc, "Cp");
					if (temp != NULL) {
						temp = strdup(ae_lc + 2);
						ae_lc = malloc(strlen("IBM-") + strlen(temp) + 1);
						strcat(ae_lc, "IBM-");
						strcat(ae_lc, temp);
						free(temp);
					}
					lc = ae_lc;
				}
			}

			else
#endif
			{
				lc = encoding;
			}                                                         /*ibm@12741*/

			/* If lc is still NULL or we have "C" encoding
			 * use the default encoding.
			 */
			if (lc == NULL || !strcmp(lc, "C")) {
				ebcdicSet = defaultSet;
			} else {
				ebcdicSet = lc;
			}
		}
	}

	/* reset the locale to start setting                            ibm@38796 */
	clc = setlocale(LC_ALL, slc);
	free(slc);

	if (((etoa_cd = iconv_open(asciiSet, ebcdicSet)) == (iconv_t)(-1)) ||
		((atoe_cd = iconv_open(ebcdicSet, asciiSet)) == (iconv_t)(-1))) {
		/* Retry with default ebcdicSet                                 ibm@12741*/
		if (strcmp(ebcdicSet, defaultSet) == 0) {
			return -1;
		}
		/* Close conversion descriptors just in case one of them succeeded */
		if (etoa_cd != (iconv_t)(-1)) {                               /*ibm@33935*/
			iconv_close(etoa_cd);                                     /*ibm@33935*/
		}                                                             /*ibm@33935*/

		if (atoe_cd != (iconv_t)(-1)) {                               /*ibm@33935*/
			iconv_close(atoe_cd);                                     /*ibm@33935*/
		}                                                             /*ibm@33935*/

		if (((etoa_cd = iconv_open(asciiSet, defaultSet)) == (iconv_t)(-1)) ||
			((atoe_cd = iconv_open(defaultSet, asciiSet)) == (iconv_t)(-1))) {
			return -1;
		}                                                             /*ibm@12741*/
	}

	/* Build initial table x'00' - x'ff'   */
	for (i = 0; i < CONV_TABLE_SIZE; i++) {
		init_tab[i] = i;
	}

	/* Create conversion table for ASCII=>EBCDIC */
	inptr = init_tab;
	outptr = a2e_tab;
	inleft = outleft = CONV_TABLE_SIZE;
	if (iconv(atoe_cd, &inptr, &inleft, &outptr, &outleft) == -1) {
		/* atoe conversion failed */
		if (errno == E2BIG) {                                        /*ibm@12464*/
			/* The EBCDIC codepage is probably DBCS */
			/* Close conversion descriptors */
			iconv_close(atoe_cd);
			iconv_close(etoa_cd);
			/* Retry with default IBM-1047 */
			if (((etoa_cd = iconv_open(asciiSet, defaultSet)) == (iconv_t)(-1)) ||
				((atoe_cd = iconv_open(defaultSet, asciiSet)) == (iconv_t)(-1))) {
				return -1;
			}
			inptr = init_tab;
			outptr = a2e_tab;
			inleft = outleft = CONV_TABLE_SIZE;
			if (iconv(atoe_cd, &inptr, &inleft, &outptr, &outleft) == -1) {
				return -1;
			}
		}                                                            /*ibm@12464*/
	}

	/* Create conversion table for EBCDIC=>ASCII */
	inptr = init_tab;
	outptr = e2a_tab;
	inleft = outleft = CONV_TABLE_SIZE;

	/* ibm@54240 start */
	/* Try to create a complete translate table for this codepage */
	/* if an EILSEQ return is received for a codepoint bypass and */
	/* move on to the next one.                                   */
	while (inleft > 0) {
		size_t ret;
		ret = iconv(etoa_cd, &inptr, &inleft, &outptr, &outleft);
		if (ret != (size_t)-1) {
			break;
		}
		if (errno == EILSEQ) {
			inptr  += 1;
			inleft -= 1;
			outptr  += 1;
			outleft -= 1;
		} else {
			/* etoa conversion failed */
			return -1;
		}
	}
	/* ibm@54240 end */

	/* Close conversion descriptors */
	iconv_close(atoe_cd);
	iconv_close(etoa_cd);

	/* Build integer test macros flag table */
	for (i = 0; i < CONV_TABLE_SIZE; i++) {
		_ascii_is_tab[i] = 0;
		if (isalnum(a2e_tab[i])) _ascii_is_tab[i] |=  _ISALNUM_ASCII;
		if (isalpha(a2e_tab[i])) _ascii_is_tab[i] |=  _ISALPHA_ASCII;
		if (iscntrl(a2e_tab[i])) _ascii_is_tab[i] |=  _ISCNTRL_ASCII;
		if (isdigit(a2e_tab[i])) _ascii_is_tab[i] |=  _ISDIGIT_ASCII;
		if (isgraph(a2e_tab[i])) _ascii_is_tab[i] |=  _ISGRAPH_ASCII;
		if (islower(a2e_tab[i])) _ascii_is_tab[i] |=  _ISLOWER_ASCII;
		if (isprint(a2e_tab[i])) _ascii_is_tab[i] |=  _ISPRINT_ASCII;
		if (ispunct(a2e_tab[i])) _ascii_is_tab[i] |=  _ISPUNCT_ASCII;
		if (isspace(a2e_tab[i])) _ascii_is_tab[i] |=  _ISSPACE_ASCII;
		if (isupper(a2e_tab[i])) _ascii_is_tab[i] |=  _ISUPPER_ASCII;
		if (isxdigit(a2e_tab[i])) _ascii_is_tab[i] |=  _ISXDIGIT_ASCII;
	}

	/* Now we handle libwrappers.so:iconv_init as well */
	libwrappersDll = dllload(dllname);
	if (libwrappersDll == NULL) {
		perror(0);
		printf("iconv_init: %s not found\n", dllname);
	} else {
		piconv_init = (int (*)()) dllqueryfn(libwrappersDll, fnname);
		if (piconv_init == NULL) {
			perror(0);
			printf("iconv_init: libwrappers.so:%s not found\n", fnname);
		} else {
			if (piconv_init() == -1) {
				printf("iconv_init: libwrappers.so:iconv_init() failed\n");
			}
		}
	}

	/* initialize the mutex used by putEnv and getEnv */
	pthread_mutex_init(&env_mutex, NULL);

	/* If we get here, then we are initialized.                    /*ibm@54240*/
	iconv_initialized = 1;                                         /*ibm@54240*/

	return 0;
}
#pragma convlit(resume)

/**************************************************************************
 * name        - atoe_dllload
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
dllhandle *
atoe_dllload(const char *dllName)
{
	dllhandle *handle;
	char *d = a2e(dllName, strlen(dllName));
	char buf[280];                                                /*ibm@29859*/

	Log1(1, "Entry: atoe_dllload: %s\n", dllName);

/*
 * TBN 2003/06/05 - Removed perror() because it does not make
 * any sense to spew out error messages when it's not supposed to!!
 * It is supposed to keep the same behaviour as dllload() and NOT
 * try to do more than what it's supposed to do!!
 */
/*
	if ((handle = dllload(d)) == 0)
	{
*/
/*        atoe_sprintf(buf,"Could not load dll : %s \n",dllName); */  /*ibm@29859*/
/*        atoe_perror(buf);                                       */  /*ibm@29859*/
/*
	}
*/
	handle = dllload(d);
	free(d);
	return handle;
}

void *
atoe_dllqueryvar(dllhandle *dllHandle, char *varName)
{
	char *n = a2e_string(varName);
	void *r = dllqueryvar(dllHandle, n);

	Log1(1, "Entry: atoe_dllqueryvar: %s\n", varName);

	free(n);

	return r;
}


void (*atoe_dllqueryfn(dllhandle *dllHandle, const char *funcName))()
{
	char *n = a2e_string(funcName);
	void (*r)() = dllqueryfn(dllHandle, n);

	Log1(1, "Entry: atoe_dllqueryfn: %s\n", funcName);

	free(n);

	return r;
}

/**************************************************************************
 * name        - atoe_spawnp
 * description -
 *              argv are ASCII (note: envp are EBCDIC) so must convert. If
 *              successful, return the value of the process ID of the child
 *              process. If unsuccessful, return -1.
 * parameters  -
 * returns     -
 *************************************************************************/

pid_t
atoe_spawnp(const char *filename, const int fd_cnt, const int *fd_map,
			const struct inheritance *inherit, char **argv, const char **envp)
{
	char **e_argv;                                                              /*ibm@52465*/
	int  validArgs = 1;                                                         /*ibm@52465*/
	char *e_filename;                                              /*ibm.7677*/
	int   i = 0;
	pid_t pid = -1;

	Log1(1, "Entry: atoe_spawnp: %s\n", filename);

	/* Determine the number of arguments. Note that the max number of      */   /*ibm@52465 ...*/
	/* bytes in the argument list to the spawn command is _POSIX_ARG_MAX   */
	/* hence it is not possible to have more than _POSIX_ARG_MAX arguments */
	for (i = 0; argv[i] != NULL && i < _POSIX_ARG_MAX; i++);
	e_argv = (char **)malloc(i * sizeof(char *) + 1);
	if (e_argv == NULL) {
		atoe_fprintf(stderr,
					 "atoe_spawnp insufficient memory for command arguments\n");
		return -1;
	}                                                                           /*... ibm@52465*/

	e_filename = a2e((char *)filename, strlen(filename));          /*ibm.7677*/

	for (i = 0; argv[i] != NULL && i < _POSIX_ARG_MAX; i++) {      /*ibm.7677*/ /*ibm@52465*/
		e_argv[i] = a2e(argv[i], strlen(argv[i]));                 /*ibm.7677*/
		if (e_argv[i] == NULL) {                                                /*ibm@52465 ...*/
			atoe_fprintf(stderr,
						 "atoe_spawnp insufficient memory for command argument %d\n", i);
			validArgs = 0;
			break;
		}                                                                       /*... ibm@52465*/
	}
	e_argv[i] = NULL;  /* ensure null terminated char array */     /*ibm.7677*/

	if (validArgs) {                                                            /*ibm@52465*/
		pid = spawnp((const char *)e_filename, fd_cnt, fd_map, inherit, /*ibm.7677*/
					 (const char **)e_argv, envp);

		if (pid == -1) {
			atoe_fprintf(stderr, "spawn of \"%s\" failed\n", filename);
		}
	}                                                                           /*ibm@52465*/

	free(e_filename);
	for (i = 0; e_argv[i] != NULL; i++) {
		free(e_argv[i]);
	}
	free(e_argv);                                                               /*ibm@52465*/

	return pid;
}

/**************************************************************************
 * name        - atoe_spawnpe
 * description -
 *              Spawn may be called using the parent's environment variables
 *              or with specified ev's. If the ev's are inherited from the
 *              parent then they will be EBCDIC, if they are specified to
 *              spawn they will be ASCII. This code deals with this
 *              latter case and just converts the ev's to EBCDIC before
 *              calling atoe_spawnp.
 * parameters  -
 * returns     -
 *************************************************************************/
pid_t
atoe_spawnpe(const char *filename, const int fd_cnt, const int *fd_map,
			 const struct inheritance *inherit, char **argv, char **envp, int envlen)
{
	char *e_envp[1025];
	int i, j;
	pid_t pid;

	Log(1, "Entry: atoe_spawnpe\n");

	i = j = (envlen < 1025) ? envlen : 1025;
	while (i--) {
		e_envp[i] = a2e(envp[i], strlen(envp[i]));
	}

	e_envp[envlen] = NULL; /* ensure null terminated */

	pid = atoe_spawnp(filename, fd_cnt, fd_map, inherit, argv, e_envp);

	while (j--) {
		free(e_envp[j]);
	}

	return pid;
}

/**************************************************************************
 * name        - etoa_uname                                 ibm@3752
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
etoa_uname(struct utsname *a)
{
	struct utsname e;
	int rc = 0;

	Log(1, "Entry: etoa_uname\n");

	rc = uname(&e);
	if (rc == 0 && a != NULL) {
		char *temp = NULL;
		temp  = e2a_string(e.sysname);
		strcpy(a->sysname, temp);
		free(temp);
		temp  = e2a_string(e.nodename);
		strcpy(a->nodename, temp);
		free(temp);
		temp  = e2a_string(e.release);
		strcpy(a->release, temp);
		free(temp);
		temp  = e2a_string(e.version);
		strcpy(a->version, temp);
		free(temp);
		temp  = e2a_string(e.machine);
		strcpy(a->machine, temp);
		free(temp);
	}

	return rc;
}

/**************************************************************************
 * name        - etoa_nl_langinfo                           ibm@3752
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
char *
etoa_nl_langinfo(int nl_item)
{

	char *retVal = NULL;
	retVal = nl_langinfo(nl_item);
	return (retVal != NULL) ? e2a_string(retVal) : NULL;
}

/*********************************************************************/
/* name        - atoe_utimes                                ibm@11596*/
/*                                                                   */
/* description - provide a version of the utimes() system function   */
/*               that converts the specified path from an ascii to   */
/*               an ebcdic string.                                   */
/*                                                                   */
/* parameters  - path:  ascii string representing the path name      */
/*               times: times to be used in setting the modification */
/*                      times of the specified file                  */
/*                                                                   */
/* returns     - int: 0 (success) or -1 (failure)                    */
/*********************************************************************/

int
atoe_utimes(const char *path, const struct timeval *times)
{
	char *ep;
	int rc;

	Log(1, "Entry: atoe_utimes\n");

	ep = a2e_string(path);

	rc = utimes(ep, times);

	free(ep);

	return rc;
}

/*********************************************************************/
/* name        - atoe_chmod                                 ibm@11596*/
/*                                                                   */
/* description - provide a version of the chmod() system function    */
/*               that converts the specified path from an ascii to   */
/*               an ebcdic string.                                   */
/*                                                                   */
/* parameters  - path: ascii string representing the path name       */
/*               mode: bit set representing mode to be set on the    */
/*                     specified file                                */
/*                                                                   */
/* returns     - int: 0 (success) or -1 (failure)                    */
/*********************************************************************/

int
atoe_chmod(const char *path, mode_t mode)
{
	char *ep;
	int rc;

	Log(1, "Entry: atoe_chmod\n");

	ep = a2e_string(path);

	rc = chmod(ep, mode);

	free(ep);

	return rc;
}

/*********************************************************************/
/* name        - atoe_chdir                                 ibm@28845*/
/*                                                                   */
/* description - provide a version of the chdir() system function    */
/*               that converts the specified path from an ascii to   */
/*               an ebcdic string.                                   */
/*                                                                   */
/* parameters  - path: ascii string representing the path name       */
/*                                                                   */
/* returns     - int: 0 (success) or -1 (failure)                    */
/*********************************************************************/

int
atoe_chdir(const char *path)
{
	int  rc;
	char *ep;

	Log(1, "Entry: atoe_chdir\n");

	ep = a2e_string(path);

	rc = chdir(ep);

	free(ep);

	return rc;
}

/*********************************************************************/
/* name        - atoe_chown                                          */
/*                                                                   */
/* description - provide a version of the chown() system function    */
/*               that converts the specified path from an ascii to   */
/*               an ebcdic string.                                   */
/*                                                                   */
/* parameters  - path: ascii string representing the path name       */
/*               uid: new user id (-1 for no change)                 */
/*               gid: new group id (-1 for no change)                */
/*                                                                   */
/* returns     - int: 0 (success) or -1 (failure)                    */
/*********************************************************************/

int
atoe_chown(const char *path, uid_t uid, gid_t gid)
{
	int  rc;
	char *ep;

	Log(1, "Entry: atoe_chown\n");

	ep = a2e_string(path);

	rc = chown(ep, uid, gid);

	free(ep);

	return rc;
}

/**************************************************************************
 * name        - atoe_ftok
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/

key_t
atoe_ftok(const char *pathname, int id)
{
	key_t key;
	char  *e;

	Log(1, "Entry: atoe_ftok\n");

	e   = a2e_string((char *) pathname);
	key = ftok(e, id);

	free(e);

	return key;
}

/**************************************************************************
 * name        - etoa___osname                                 ibm@27407
 * description -
 * parameters  -
 * returns     -
 *************************************************************************/
int
etoa___osname(struct utsname *a)
{
	struct utsname e;
	int rc = 0;

	Log(1, "Entry: etoa___osname\n");

	rc = __osname(&e);
	if (rc == 0 && a != NULL) {
		char *temp = NULL;
		temp = e2a_string(e.sysname);
		strcpy(a->sysname, temp);
		free(temp);

		temp = e2a_string(e.nodename);
		strcpy(a->nodename, temp);
		free(temp);

		temp = e2a_string(e.release);
		strcpy(a->release, temp);
		free(temp);

		temp = e2a_string(e.version);
		strcpy(a->version, temp);
		free(temp);

		temp = e2a_string(e.machine);
		strcpy(a->machine, temp);
		free(temp);
	}

	return rc;
}


/* END OF FILE */
