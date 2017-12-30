/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * @internal @file
 * @ingroup Port
 * @brief Native language support helpers
 */


#include <windows.h>
#include <winbase.h>
#include <stdlib.h>
#include <LMCONS.H>
#include <direct.h>

#include "omrport.h"
#include "omrportpriv.h"

void nls_determine_locale(struct OMRPortLibrary *portLibrary);

/**
 * @internal
 * Set set locale.
 *
 * @param[in] portLibrary The port library
 */
void
nls_determine_locale(struct OMRPortLibrary *portLibrary)
{
	LCID localeID = LOCALE_USER_DEFAULT;
	LCTYPE infoType;
	int length;
#ifdef UNICODE
	int i;
#endif
	char lang[8];
	char country[8];
	J9NLSDataCache *nls = &portLibrary->portGlobals->nls_data;

	/* Get the language */
	infoType = LOCALE_SISO639LANGNAME;

	length = GetLocaleInfo(localeID, infoType, (LPTSTR)&lang[0], sizeof(lang));
	if (length < 2) {
		strncpy(nls->language, "en", 2);
	} else {

#ifdef UNICODE
		/* convert double byte to single byte */
		for (i = 0; i < length; i++) {
			lang[i] = (char)((short *)lang)[i];
		}
#endif

		_strlwr(lang);

		if (!strcmp(lang, "jp")) {
			// Not required for NT, Win32 gets it wrong
			strncpy(nls->language, "ja", 2);
		} else if (!strcmp(lang, "ch")) {
			/*[PR 104204] Pocket PC 2004 gives ch instead of zh for Chinese */
			strncpy(nls->language, "zh", 2);
		} else {
			strncpy(nls->language, lang, 3);
		}
	}

	/* Get the region */
	infoType = LOCALE_SISO3166CTRYNAME;

	length = GetLocaleInfo(localeID, infoType, (LPTSTR)&country[0], sizeof(country));
	if (length < 2) {
		strncpy(nls->region, "US", 2);
	} else {
#ifdef UNICODE
		// convert double byte to single byte
		for (i = 0; i < length; i++) {
			country[i] = (char)((short *)country)[i];
		}
#endif
		country[2] = 0; /* force null-terminator */
		strncpy(nls->region, country, 2);
	}

}


