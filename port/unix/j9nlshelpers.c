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
 * @internal @file
 * @ingroup Port
 * @brief Native language support helpers
 */


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "omrutil.h"

void nls_determine_locale(struct OMRPortLibrary *portLibrary);


static char
nls_tolower(char toConvert)
{
	char lower;

#if !defined(J9ZOS390)
	lower = j9_ascii_tolower(toConvert);
#else
	/* check to see if it is an uppercase character, and if so, return the corresponding lowercase char */
	if ((0x41 <=  toConvert) && (toConvert <= 0x5a)) {
		lower = toConvert + 0x20;
	} else {
		lower = toConvert;
	}
#endif

	return lower;
}


/**
 * @internal
 * Set set locale.
 *
 * @param[in] portLibrary The port library
 */
void
nls_determine_locale(struct OMRPortLibrary *portLibrary)
{
	J9NLSDataCache *nls = &portLibrary->portGlobals->nls_data;
	char languageProp[4] = "en\0";
	char countryProp[3] = "US";
	char *lang = NULL;
	int langlen = 0;
#if defined(LINUX) || defined(OSX)
	intptr_t result;
	char langProp[24];
#endif /* defined(LINUX) || defined(OSX) */
	intptr_t countryStart = 2;

	/* Get the language */

	/* Set locale, returns NULL in case locale data cannot be initialized. This may indicate
	 * that the locale is not installed OR not selected & generated (see /etc/locale.gen) */
	setlocale(LC_ALL, "");

#if defined(LINUX) || defined(OSX)
	lang = setlocale(LC_CTYPE, NULL);
	/* set lang for later usage, we cannot use the return of setlocale(LC_ALL, "") as its not
	 * the correct interface to retrieve it (lang/region) */

	/*[PR 104520] Use LANG environment variable when locale cannot be obtained */
	if (!lang || !strcmp(lang, "C") || !strcmp(lang, "POSIX")) {
		result = omrsysinfo_get_env(portLibrary, "LANG", langProp, sizeof(langProp));
		if (!result && !strcmp(langProp, "ja")) {
			strcat(langProp, "_JP");
			lang = langProp;
		}
	}
#else /* defined(LINUX) || defined(OSX) */
	/* Query locale data */
	lang = setlocale(LC_CTYPE, NULL);
#if defined (J9ZOS390)
	if (NULL != lang) {
		/* CMVC 107274:  z/OS sometimes returns the HFS path to a "locale object" so carve it up to make it look like the corresponding locale name */
		char *lastSlash = strrchr(lang, '/');

		if (NULL != lastSlash) {
			lang = lastSlash + 1;
		}
	}
#endif /* defined (J9ZOS390) */
#endif  /* defined(LINUX) || defined(OSX) */
	if (lang != NULL && strcmp(lang, "POSIX") && strcmp(lang, "C"))
		if (lang != NULL && (langlen = strlen(lang)) >= 2) {
			/* copy the language, stopping at '_'
			 * CMVC 145188 - language locale must be lowercase
			 */
			languageProp[0] = nls_tolower(lang[0]);
			languageProp[1] = nls_tolower(lang[1]);
			if (lang[2] != '_') {
				languageProp[2] = nls_tolower(lang[2]);
				countryStart++;
			}
		}
	if (!strcmp(languageProp, "jp")) {
		languageProp[1] = 'a';
	}
	strncpy(nls->language, languageProp, 3);

	/* Get the region */
	if (langlen >= (3 + countryStart) && lang[countryStart] == '_') {
		countryProp[0] = lang[countryStart + 1];
		countryProp[1] = lang[countryStart + 2];
	}
	strncpy(nls->region, countryProp, 2);
}

