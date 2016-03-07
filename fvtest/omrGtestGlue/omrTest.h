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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef OMR_TEST_FRAMEWORK_H_
#define OMR_TEST_FRAMEWORK_H_

#if defined(J9ZOS390)
/* Gtest invokes some functions (e.g., getcwd() ) before main() is invoked.
 * We need to invoke iconv_init() before gtest to ensure a2e is initialized.
 */
extern int iconv_initialization(void);
static int iconv_init_static_variable = iconv_initialization();
#endif


#if defined(J9ZOS390)
/*  Gtest invokes xlocale, which has function definition for tolower and toupper.
 * This causes compilation issue since the a2e macros (tolower and toupper) automatically replace the function definitions.
 * So we explicitly include <ctype.h> and undefine the macros for gtest, after gtest we then define back the macros.
 */
#include <ctype.h>
#undef toupper
#undef tolower

#include "gtest/gtest.h"

#define toupper(c)     (islower(c) ? (c & _XUPPER_ASCII) : c)
#define tolower(c)     (isupper(c) ? (c | _XLOWER_ASCII) : c)

#else
#include "gtest/gtest.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int testMain(int argc, char **argv, char **envp);

#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
