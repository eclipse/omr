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
	#include <stdlib.h>

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
    int  atoe_open_notag(const char *fname, int options, ...);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif  /* _IBM_ATOE_H_ */

/* END OF FILE */

