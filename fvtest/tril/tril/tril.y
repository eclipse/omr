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
    #include <stdio.h>

    static ASTNode* trees;
%}

%union {
    ASTNode* node;
    ASTNodeArg* arg;
    char* str;
    uint64_t integer;
    double f64;
    ASTValue* value;
};

%token <str> IDENTIFIER STRING
%token <integer> INTEGER
%token <f64> DOUBLE

%type <node> node nodeList
%type <arg> arg argList
%type <value> value valueList

%%

nodeList:
    node
        {
            //printf("Generating tree\n");
            $$ = trees = $1;
        }
    | nodeList node
        {
            //printf("Generating tree list\n");
            appendSiblingNode($1, $2);
            $$ = trees = $1;
        }
    ;

node:
    '(' IDENTIFIER ')'
        {
            //printf("Generating leaf node")
            $$ = createNode($2, NULL, NULL, NULL);
        }
    | '(' IDENTIFIER nodeList ')'
        {
            //printf("Generating tree node")
            $$ = createNode($2, NULL, $3, NULL);
        }
    | '(' IDENTIFIER argList ')'
        {
            //printf("Generating node with argument")
            $$ = createNode($2, $3, NULL, NULL);
        }
    | '(' IDENTIFIER argList nodeList ')'
        {
            //printf("Generating tree node with argument")
            $$ = createNode($2, $3, $4, NULL);
        }
    ;

argList:
    arg
        {
            $$ = $1;
        }
    | argList arg
        {
            appendSiblingArg($1, $2);
            $$ = $1;
        }
    ;

arg:
    value
        {
            $$ = createNodeArg("", $1, NULL);
        }
    | IDENTIFIER '=' value
        {
            $$ = createNodeArg($1, $3, NULL);
        }
    | IDENTIFIER '=' '[' valueList ']'
        {
            $$ = createNodeArg($1, $4, NULL);
        }
    ;

valueList:
    value
        {
            $$ = $1;
        }
    | valueList ',' value
        {
            appendSiblingValue($1, $3);
            $$ = $1;
        }
    ;

value:
    INTEGER
        {
            //printf("Generating value %d\n", $1);
            $$ = createIntegerValue($1);
        }
    | DOUBLE
        {
            $$ = createFloatingPointValue($1);
        }
    | STRING
        {
            //printf("Generating value \"%s\"\n", $1);
            $$ = createStrValue($1);
        }
    | IDENTIFIER
        {
            //printf("Generating value \"%s\"\n", $1);
            $$ = createStrValue($1);
        }
    ;

%%

ASTNode* parseFile(FILE* in) {
    set_input_file(in);
    yyparse();
    return trees;
}

ASTNode* parseString(const char* in) {
    FILE* pf = tmpfile();
    if (pf != NULL) {
        if(fputs(in, pf) >= 0) {
            fflush(pf);
            fseek(pf, 0, SEEK_SET);
            parseFile(pf);
        } else {
	    fprintf(stderr, "Tril parser error: Could not write to temporary file \n");
	}
    } else {
	    fprintf(stderr, "Tril parser error: Could not create temporary file for writing \n");
	}
    fclose(pf);
    return trees;
}
