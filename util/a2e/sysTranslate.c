/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
