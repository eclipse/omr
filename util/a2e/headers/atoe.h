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
 * Generic ASCII to EBCDIC character conversion header file. This file defines
 * the base conversion functions used by the atoe_* functions, and AWT.
 * ===========================================================================
 */

#if !defined(_IBM_ATOE_H_)
    #define _IBM_ATOE_H_
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/* CMVC 162573 include the header file that includes the prototype for malloc used in functions/macros below*/
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/types.h>

    #pragma map(sysTranslateASM, "SYSXLATE")
    extern char* sysTranslateASM(const char *source, int length, char *trtable, char* xlate_buf);
    extern char* sysTranslate(const char *source, int length, char *trtable, char* xlate_buf);

    extern int iconv_init(void);
    extern void iconv_shutdown(void);

    #if !defined(MAXPATHLEN)
        #define MAXPATHLEN     1023+1
    #endif

    #if !defined(CONV_TABLE_SIZE)
        #define CONV_TABLE_SIZE 256
        extern char a2e_tab[CONV_TABLE_SIZE];
        extern char e2a_tab[CONV_TABLE_SIZE];
    #endif

      #define a2e(str, len) sysTranslate(str, abs(len), a2e_tab, (char *)malloc(abs(len)+1)) /*ibm@41269*/
      #define e2a(str, len) sysTranslate(str, abs(len), e2a_tab, (char *)malloc(abs(len)+1)) /*ibm@41269*/

      #define a2e_string(str) sysTranslate(str, strlen(str), a2e_tab, (char *)malloc(strlen(str)+1)) /*ibm@41269*/
      #define e2a_string(str) sysTranslate(str, strlen(str), e2a_tab, (char *)malloc(strlen(str)+1)) /*ibm@41269*/

    char *a2e_func(char *str, int len);    /*ibm@1423*/
    char *e2a_func(char *str, int len);    /*ibm@3218*/

    void atoe_enableFileTagging(void);
    void atoe_setFileTaggingCcsid(void *pccsid);
    int atoe_open_notag(const char *fname, int options, ...);
    struct passwd *etoa_passwd(struct passwd *e_passwd);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif  /* _IBM_ATOE_H_ */

/* END OF FILE */

