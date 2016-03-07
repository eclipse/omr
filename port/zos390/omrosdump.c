/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <ctest.h>
#include "omrport.h"
#include "atoe.h"
#include "portnls.h"

static void convertToUpper(struct OMRPortLibrary *portLibrary, char *toConvert, uintptr_t len);
static void appendCoreName(struct OMRPortLibrary *portLibrary, char *corepath, intptr_t pathBufferLimit, intptr_t pid);
static uintptr_t tdump_wrapper(struct OMRPortLibrary *portLibrary, char *filename, char *dsnName);
static intptr_t tdump(struct OMRPortLibrary *portLibrary, char *asciiLabel, char *ebcdicLabel, uint32_t *returnCode, uint32_t *reasonCode);
static intptr_t ceedump(struct OMRPortLibrary *portLibrary, char *asciiLabel, char *ebcdicLabel);

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
	uintptr_t rc = 0;
	char *doIEATdump;
	char *dsnName = NULL;

	/* Handle J2SE and J9 controls */
	doIEATdump = strstr(dumpType, "IEATDUMP");

	if (doIEATdump) {
		/* do the IEATDUMP */
		rc = tdump_wrapper(portLibrary, filename, dsnName);
	} else {
		/* do the CEEDUMP */
		rc = ceedump(portLibrary, filename, NULL);
	}

	return rc;
}

/**
 * @internal
 * Converts any lower case characters in the the passed in string to uppercase characters
 *
 * @param[in,out] toConvert the string to convert
 * @param[in] len the number of characters in the string, not including any null terminator
 */
static void
convertToUpper(struct OMRPortLibrary *portLibrary, char *toConvert, uintptr_t len)
{

	uintptr_t i;

	if (NULL == toConvert) {
		return;
	}

	/* check each character to see if it is lowercase character, and if so, slam in the corresponding uppercase char */
	for (i = 0; i < len; i++) {
		if ((toConvert[i] >= 0x61) && (toConvert[i] <= 0x7a)) {
			toConvert[i] = toConvert[i] - 0x20;
		}
	}
}


/**
 * @internal @note corepath should be a buffer of at least EsMaxPath
 */
static void
appendCoreName(struct OMRPortLibrary *portLibrary, char *corepath, intptr_t pathBufferLimit, intptr_t pid)
{
	/* Name will be appended to the end of the path... */
	uintptr_t corepathPrefixLen = strlen(corepath);
	uintptr_t nameLimit = pathBufferLimit - corepathPrefixLen;
	char *base = corepath + corepathPrefixLen;
	char corename[64] = "CEEDUMP";
	time_t lastModTime = 0;

	struct stat attrBuf;
	char pidFilter[24];
	uintptr_t pidLen;

	DIR *folder = opendir(corepath);
	struct dirent *entry = NULL;

	/* Failsafe name */
	strncpy(base, "CEEDUMP", nameLimit);

	if (!folder) {
		return;
	}

	pidLen = portLibrary->str_printf(portLibrary, pidFilter, sizeof(pidFilter), ".%d", pid);

	while ((entry = readdir(folder))) {
		char *s = entry->d_name;

		/* Files beginning with "CEEDUMP." must end with ".<pid>" */
		if (strstr(s, "CEEDUMP.") == s) {
			if (strcmp(s + strlen(s) - pidLen, pidFilter) != 0) {
				continue;
			}
		}
		/* Otherwise keep searching */
		else {
			continue;
		}

		/* Temporarily append current name */
		strncpy(base, entry->d_name, nameLimit);

		/* Valid file? - need to use full path */
		if (stat(corepath, &attrBuf) == 0) {
			if (S_ISREG(attrBuf.st_mode) && attrBuf.st_mtime >= lastModTime) {
				/* Keep track of most recent regular matching file */
				strncpy(corename, entry->d_name, 63);
				corename[63] = '\0';
				lastModTime = attrBuf.st_mtime;
			}
		}
	}
	closedir(folder);

	/* Append most likely name */
	strncpy(base, corename, nameLimit);
}

uintptr_t
tdump_wrapper(struct OMRPortLibrary *portLibrary, char *filename, char *dsnName)
{
	uintptr_t retVal = 0;
	BOOLEAN filenameSpecified = TRUE;
	uint32_t returnCode;
	uint32_t reasonCode;
	intptr_t err;

	if (NULL == filename) {
		return 1;
	}

	/* Use default dataset name */
	if (filename[0] == '\0') {

		char *loginID = NULL;
#define USERNAME_STACK_BUFFER_SIZE 128
		char userNameStack[USERNAME_STACK_BUFFER_SIZE];
		time_t now = time(NULL);
		struct passwd *userDescription = NULL;
		intptr_t result = -1;
#define J9_MAX_JOBNAME 16
		char jobname[J9_MAX_JOBNAME];
		uintptr_t fileNamePrefixLength;

		result = portLibrary->sysinfo_get_username(portLibrary, userNameStack, USERNAME_STACK_BUFFER_SIZE);
		if (-1 == result) {
			/* we tried to get the username, but failed, use ANON instead */
			portLibrary->str_printf(portLibrary, userNameStack, USERNAME_STACK_BUFFER_SIZE, "%s", "ANON");
		}

		/* fetch the current jobname */
		if (jobname) {
			omrget_jobname(portLibrary, jobname, J9_MAX_JOBNAME);
		}

#if defined(J9ZOS39064) /* Remove 'TDUMP' to alleviate the 44 chars constraint on dataset names */
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "%s.JVM.%s.", userNameStack, jobname);
#else /* 31 bits */
		portLibrary->str_printf(portLibrary, filename, EsMaxPath, "%s.JVM.TDUMP.%s.", userNameStack, jobname);
#endif

		fileNamePrefixLength = strlen(filename);

		/* Tack on current timestamp */
		strftime(filename + fileNamePrefixLength, EsMaxPath - fileNamePrefixLength, "D%y%m%d.T%H" "%M" "%S", localtime(&now));

		/* Set filenameSpecified = FALSE as we're now using the default template */
		filenameSpecified = FALSE;
#undef USERNAME_STACK_BUFFER_SIZE

	}

#if defined(J9ZOS39064)
	/* Appending X&DS token if the TDUMP dataset pattern does not have it already.
	 * &DS token is introduced in zOS V1R10. It is backported to R7, R8 and R9.
	 * OA24232 shipped the solution to earlier releases - PTF List:
	 * Release 720   : UA43245 available 08/09/17 (F809) (R7)
	 * Release 730   : UA43246 available 08/09/17 (F809) (R8)
	 * Release 740   : UA43247 available 08/09/17 (F809) (R9)
	 * Release 750   : UA43248 available 08/09/17 (F809) (R10)
	 * zOS machines without these PTF will fail to generate TDUMP due to invalid dataset
	 * name (unable to interpret &DS token).
	 */
	if (filenameSpecified) {
		/* append X&DS to the user specified pattern if needed */
		if (strstr(filename, "&DS") == NULL) {
			/* may need to terminate a trailing zOS token here */
			char *token = strrchr(filename, '&');
			if (token && strstr(token, ".") == NULL) {
				strcat(filename, "..X&DS");
			} else {
				strcat(filename, ".X&DS");
			}
			portLibrary->nls_printf(portLibrary, J9NLS_INFO | J9NLS_STDERR, J9NLS_PORT_ZOS_64_APPENDING_XDS);
		}
	} else { /* append X&DS to the default dump pattern */
		strcat(filename, ".X&DS");
	}
#endif

	/* the filename must be entirely uppercase for IEATDUMP requests */
	convertToUpper(portLibrary, filename, strlen(filename));

	/* Convert filename into EBCDIC... */
	dsnName = a2e_func(filename, strlen(filename) + 1);
	err = tdump(portLibrary, filename, dsnName, &returnCode, &reasonCode);
	free(dsnName);

	if (0 == err) {
		retVal = returnCode;
		/* We have a couple of conditions we can work around */
		if (0x8 == returnCode && 0x22 == reasonCode) {
			/* Name was too long */
			if (filenameSpecified) {
				portLibrary->nls_printf(portLibrary, J9NLS_WARNING | J9NLS_STDERR, J9NLS_PORT_IEATDUMP_NAME_TOO_LONG);
				filename[0] = '\0';
				/* Convert filename into EBCDIC... */
				dsnName = a2e_func(filename, strlen(filename) + 1);
				retVal = tdump_wrapper(portLibrary, filename, dsnName);
				free(dsnName);
			}
		} else if (0x8 == returnCode && 0x26 == reasonCode) {
			/* Couldn't allocate data set (disk full) */
			portLibrary->nls_printf(portLibrary, J9NLS_ERROR | J9NLS_STDERR, J9NLS_PORT_IEATDUMP_DISK_FULL);
		}
	} else {
		/* If err is non-zero, we didn't call TDUMP because of an internal error*/
		retVal = err;
	}

	return retVal;
}

/**
 * @internal
 * Generate an IEATDUMP
 *
 * @param[in] portLibrary the port library
 * @param[in,out] asciiLabel the filename to use in ascii. Lowercase letters are _not_ allowed
 * @param[in,out] ebcdicLabel the filename to use in ebcdic. Lowercase letters are _not_ allowed
 * @param[out] returnCode the zOS return code from the IEATDUMP call
 * @param[out] reasonCode the zOS reason code from the IEATDUMP call
 *  *
 * @return 0 if IEATDUMP was actually called, non-zero if there was an error that prevented us
 *  		getting that far
 *
 * If we return 0, it's the callers responsibility to check returnCode to find out if the TDUMP was
 * actually successful
 *
 * Note that asciiLabel and ebcdicLabel should represent the same
 *  filename as both are used in this function
 */
static intptr_t
tdump(struct OMRPortLibrary *portLibrary, char *asciiLabel, char *ebcdicLabel, uint32_t *returnCode, uint32_t *reasonCode)
{
	struct ioparms_t {
		uint64_t plist[256];
		uint32_t retcode;
		uint32_t rsncode;
		uint32_t retval;
	} *ioParms31;

	struct dsn_pattern_t {
		unsigned char len;
		char dsn[256];
	} *dsnPattern31;

	/* _TDUMP subroutine expects 31 bit addresses */
	ioParms31 = __malloc31(sizeof(*ioParms31));
	if (ioParms31 == NULL) {
		omrtty_err_printf(portLibrary, "__malloc31 failed to allocate buffer for ioParms31\n");
		return 1;
	}
	memset(ioParms31, 0, sizeof(*ioParms31));

	dsnPattern31 = __malloc31(sizeof(*dsnPattern31));
	if (dsnPattern31 == NULL) {
		omrtty_err_printf(portLibrary, "__malloc31 failed to allocate buffer for dsnPattern31\n");
		free(ioParms31);
		return 2;
	}
	memset(dsnPattern31, 0, sizeof(*dsnPattern31));

	strcpy(dsnPattern31->dsn, ebcdicLabel);
	dsnPattern31->len = strlen(ebcdicLabel);

	/* Note: the actual IEATDUMP options are specified on the assembler macro call in j9generate_ieat_dumps.s. The
	 * message below needs to be kept consistent with the options set on the macro call.
	 */
	omrtty_err_printf(portLibrary, "IEATDUMP in progress with options SDATA=(LPA,GRSQ,LSQA,NUC,PSA,RGN,SQA,SUM,SWA,TRT)\n");

	_TDUMP(ioParms31, dsnPattern31);

	if (returnCode) {
		*returnCode = ioParms31->retcode;
	}

	if (reasonCode) {
		*reasonCode = ioParms31->rsncode;
	}

	if (ioParms31->retcode) {
		/*
		 * Please refer to VMDESIGN 1459 about IEATDUMP failure recovery
		 */
		char errmsg[1024];

		omrtty_err_printf(portLibrary, "IEATDUMP failure for DSN='%s' RC=0x%08X RSN=0x%08X\n", asciiLabel, ioParms31->retcode, ioParms31->rsncode);

		memset(errmsg, 0, sizeof errmsg);

		omrstr_printf(portLibrary, errmsg, sizeof errmsg, "JVMDMP025I IEATDUMP failed RC=0x%08X RSN=0x%08X DSN=%s", ioParms31->retcode, ioParms31->rsncode, asciiLabel);
		omrsyslog_write(portLibrary, 0, errmsg);
	} else {
		omrtty_err_printf(portLibrary, "IEATDUMP success for DSN='%s'\n", asciiLabel);
	}

	free(dsnPattern31);
	free(ioParms31);

	return 0;
}

static intptr_t
ceedump(struct OMRPortLibrary *portLibrary, char *asciiLabel, char *ebcdicLabel)
{
	/* Optional dump location hint */
	const char *location = getenv("_CEE_DMPTARG");
	char corepath[EsMaxPath] = ".";
	struct stat attrBuf;
	int rc;

	omrtty_err_printf(portLibrary, "CEEDUMP in progress - Please Wait.\n");

	/* replace call to __cdump() with call to csnap() as this does not lock up. see CMVC 134311 */
	/* original line : rc = cdump(ebcdicLabel); */
	rc = csnap(ebcdicLabel);

	if (location) {
		strncpy(corepath, location, EsMaxPath);
		corepath[EsMaxPath - 1] = '\0';
	} else {
		getcwd(corepath, EsMaxPath);
	}

	/* Normalize path */
	if (corepath[strlen(corepath) - 1] != '/') {
		strcat(corepath, "/");
	}

	/* Search for most likely CEEDUMP file */
	appendCoreName(portLibrary, corepath, EsMaxPath, getpid());

	/* __cdump may return 1 even on success, so check that the file exists
	 *  This from the V1R9 C/C++ Runtime Library Reference
	 *  	The output of the dump is directed to the CEESNAP data set.
	 *  	The DD definition for CEESNAP is as follows: //CEESNAP DD SYSOUT= *
	 *  	If the data set is not defined, or is not usable for any reason, cdump() returns a failure code of 1.
	 *  	This occurs even if the call to CEE3DMP is successful.
	 *  	For more information see "Debugging C/C++ Routines" in "z/OS Language Environment Debugging Guide"
	 */
	if (1 == rc) {
		if (stat(corepath, &attrBuf)) {
			rc = errno;
		} else {
			rc = 0;
		}
	}

	if (rc) {
		omrtty_err_printf(portLibrary, "CEEDUMP failure for FILE='%s' RC=0x%08X\n", corepath, rc);
	} else {
		if (asciiLabel != NULL) {
			strncpy(asciiLabel, corepath, EsMaxPath);
		}
		omrtty_err_printf(portLibrary, "CEEDUMP success for FILE='%s'\n", corepath);
	}

	return rc;
}

int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary)
{
	/* noop */
	return 0;
}

void
omrdump_shutdown(struct OMRPortLibrary *portLibrary)
{
	/* noop */
}
