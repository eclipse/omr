/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

%{
    #include "ast.h"
    #include "tril.parser.h"
    #include <stdio.h>
%}

%%

@?[a-zA-Z][a-zA-Z0-9]* {
                           char* name = (char*)malloc(yyleng*sizeof(char) + 1);
                           snprintf(name, yyleng + 1, "%s", yytext);
                           yylval.str = name;
                           return IDENTIFIER;
                       };

[-]?[0-9]+             {
                           yylval.integer = strtoull(yytext, NULL, 10);
                           return INTEGER;
                       };

[-]?0x[0-9a-fA-F]+     {
                           yylval.integer = strtoull(yytext, NULL, 16);
                           return INTEGER;
                       };

[-]?[0-9]+[.][0-9]+    {
                           yylval.f64 = strtod(yytext, NULL);
                           return DOUBLE;
                       };

;[^\n]*                ; /* ignore comment */

\"[^"]*\"              {
                           char* val = (char*)malloc(yyleng*sizeof(char) + 1);
                           snprintf(val, yyleng - 1, "%s", yytext + 1);
                           yylval.str = val;
                           return STRING;
                       };

[()[\]=,]              return *yytext;

[ \t\n]                ; /* ignore white space */

.                      fprintf(stderr, "Invalid character '%s'\n", yytext);

%%

void set_input_file(FILE* in) {
    yyin = in;
}

int yyerror(char *s, ...) {
    fprintf(stderr, "Tril lexer error on line %d: %s\n", yylineno, s);
	return 1;
}

int yywrap(void) {
    return 1;
}
