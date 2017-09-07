/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#pragma map(sysTranslate, "SYSXLATE")
#include <builtins.h>

char *
sysTranslate(const char *source, int length, char *trtable, char *xlate_buf)
{
	int i;

	memcpy(xlate_buf, source, length);  /* copy source to target */

	i = 0;

#if __COMPILER_VER >= 0x41050000
	/* If the compiler level supports the fast builtin for translation, use it
	 * on the first n*256 bytes of the string
	 */
	for (; length > 255; i += 256, length -= 256) {
		__tr(xlate_buf + i, trtable, 255);

	}
#endif

	for (; length > 0; i++, length--) {      /* translate */
		xlate_buf[i] = trtable[source[i]];
	}

	xlate_buf[i] = '\0';                /* null terminate */
	return xlate_buf;
}
