/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "ast.h"
#include "ast.hpp"
#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include <iterator>
#include <sstream>
#include <algorithm>
#include <limits>
#include <stdlib.h>

class Token {
    public:
        enum Type {
            ID = 0,
            STRING_,
            STRING_0,
            INT,
            WHITESPACE,
            AT,
            ZERO,
            HEXINT,
            QUOT,
            LPAREN,
            RPAREN,
            LPAREN_SQ,
            RPAREN_SQ,
            EQUALS,
            COMMA,
            COMMENT,
            DOUBLE_
    };

    private:
        Type type;
        std::string value;
    

    public:
        Token(Type type, std::string value):
	        type(type), value(value) {}
        Type getType() const { return type; }
        void setType(Token::Type t) {type = t ;}
        const std::string &getValue() const {return value ;}
};

class LexerFailure {
	std::string message;

public:
	LexerFailure(std::string message);
	const std::string &what() const;
};

LexerFailure::LexerFailure(std::string message) :
	message(message) {}

const std::string &LexerFailure::what() const { return message; }

class ParserFailure {
	std::string message;

public:
	ParserFailure(std::string message);
	const std::string &what() const;
};

ParserFailure::ParserFailure(std::string message) :
	message(message) {}

const std::string &ParserFailure::what() const { return message; }

bool isNotNewLine(char c) { return c != '\n'; }

bool isNotQuot(char c) { return c != '"'; }

bool isNonZeroDigit(char c) { return '1' <= c && c <= '9'; }

class ASTstates {
    public:
        enum State {
            ID = 0,
            STRING_,
            STRING_0,
            INT,
            WHITESPACE,
            AT,
            ZERO,
            HEXINT,
            QUOT,
            LPAREN,
            RPAREN,
            LPAREN_SQ,
            RPAREN_SQ,
            EQUALS,
            COMMA,
            DOUBLE_,

            START,
            FAIL,
            DOT,
            MINUS,
            COMMENT,
            LARGEST_STATE = COMMENT
        };
    
    private:
        std::vector<State> acceptingStates;
        std::vector<std::vector<State> > transitionFunction;
        Token::Type stateToType(State s) const {
            switch(s) {
                case ID: return Token::ID;
                case STRING_: return Token::STRING_;
                case INT: return Token::INT;
                case WHITESPACE: return Token::WHITESPACE;
                case DOUBLE_: return Token::DOUBLE_;
                case AT:  return Token::AT;
                case ZERO:  return Token::ZERO;
                case QUOT:  return Token::QUOT;
                case LPAREN:  return Token::LPAREN;
                case RPAREN:  return Token::RPAREN;
                case LPAREN_SQ:  return Token::LPAREN_SQ;
                case RPAREN_SQ:  return Token::RPAREN_SQ;
                case EQUALS: return Token::EQUALS;
                case COMMA: return Token::COMMA;
                case HEXINT:  return Token::HEXINT;
                case COMMENT: return Token::COMMENT;
                default: throw LexerFailure("Cannot convert state to Type");
            }
        }

    public:
    
        State start() const { return START; }

        bool failed(State state) const { return state == FAIL; }

        bool accept(State state) const {
            for (int i = 0; i < acceptingStates.size(); i++) {
                if (acceptingStates[i] == state) {
                    return true;
                }
            }
            return false;
        }
        
        void stateTransition(State oldState, const std::string &chars, State newState) {
            for (int i = 0; i < chars.size(); i++) {
                transitionFunction[oldState][chars[i]] = newState;
            }
        }

        void stateTransition(State oldState, int (*test)(int), State newState) {
            for (int c = 0; c < 256; ++c) {
                if (test(c)) {
                    transitionFunction[oldState][c] = newState;
                }
            }
        }

        void stateTransition(State oldState, bool (*test)(char), State newState) {
            for (int c = 0; c < 256; ++c) {
                if (test(c)) {
                    transitionFunction[oldState][c] = newState;
                }
            }
        }

        State getNewState(State state, char nextChar) const {
            return transitionFunction[state][nextChar];
        }

        /* Setting up transition rules using states */
        ASTstates() {
            acceptingStates.push_back(ID);
            acceptingStates.push_back(STRING_);
            acceptingStates.push_back(INT);
            acceptingStates.push_back(DOUBLE_);
            acceptingStates.push_back(WHITESPACE);
            acceptingStates.push_back(COMMENT);
            acceptingStates.push_back(ZERO);
            acceptingStates.push_back(LPAREN);
            acceptingStates.push_back(RPAREN);
            acceptingStates.push_back(LPAREN_SQ);
            acceptingStates.push_back(RPAREN_SQ);
            acceptingStates.push_back(EQUALS);
            acceptingStates.push_back(COMMA);
            acceptingStates.push_back(HEXINT);
            transitionFunction = std::vector<std::vector<State> > (LARGEST_STATE+1, std::vector<State> (256));
            // initialize the transitionFunction
            for (size_t i = 0; i < transitionFunction.size(); ++i) {
                for (size_t j = 0; j < transitionFunction[0].size(); ++j) {
                    transitionFunction[i][j] = FAIL;
                }
            }
            stateTransition(START, "@", AT);
            stateTransition(START, "-", MINUS);
            stateTransition(START, isNonZeroDigit, INT);
            stateTransition(START, ";", COMMENT);
            stateTransition(START, "-", MINUS);
            stateTransition(START, "\"", QUOT);
            stateTransition(START, isalpha, ID);
            stateTransition(START, isspace, WHITESPACE);
            stateTransition(START, "[", LPAREN_SQ);
            stateTransition(START, "]", RPAREN_SQ);
            stateTransition(START, "(", LPAREN);
            stateTransition(START, ")", RPAREN);
            stateTransition(START, "=", EQUALS);
            stateTransition(START, ",", COMMA);
            stateTransition(START, "0", ZERO);
		    stateTransition(WHITESPACE, isspace, WHITESPACE);
            stateTransition(AT, isalpha, ID);
            stateTransition(ID, isalpha, ID);
            stateTransition(ID, isdigit, ID);
            stateTransition(MINUS, "0", ZERO);
            stateTransition(MINUS, isNonZeroDigit, INT);
            stateTransition(MINUS, isalpha, ID);
            stateTransition(ZERO, isdigit, INT);
            stateTransition(ZERO, "x", HEXINT);
            stateTransition(HEXINT, isxdigit, HEXINT);
            stateTransition(INT, isdigit, INT);
            stateTransition(ZERO, ".", DOT);
            stateTransition(INT, ".", DOT);
            stateTransition(DOT, isdigit, DOUBLE_);
            stateTransition(DOUBLE_, isdigit, DOUBLE_);
            stateTransition(COMMENT, isNotNewLine, COMMENT);
            stateTransition(COMMENT, isNotNewLine, COMMENT);
            stateTransition(QUOT, isNotQuot, STRING_0);
            stateTransition(STRING_0, isdigit, STRING_0);
            stateTransition(STRING_0, isalpha, STRING_0);
            stateTransition(STRING_0, isNotQuot, STRING_0);
            stateTransition(STRING_0, "\"", STRING_);
        }

        /* Tokenize the given input following the transition rules until no non-error transitions available
         * If in an accepting state, then emit current token, and go back to the start state
         * Else if in the intermediate state (either accepting or failed state), keep reading input
         * Else throw LexerExecption (in failed state)
         * 
         * @param input The whole tril input string
         * @return The vector of Token after tokenizing the input
         */
        std::vector<Token> simplifiedMaximalMunch(const std::string &input) const {
            std::vector<Token> result;
            State state = start();
            std::string munchedInput;
            for (std::string::const_iterator inputPosn = input.begin(); inputPosn != input.end();) {
                State oldState = state;
                state = getNewState(state, *inputPosn);
                if (!failed(state)) {
                    munchedInput += *inputPosn;
                    oldState = state;
                    ++inputPosn;
                }
                if (inputPosn == input.end() || failed(state)) {
                    if (accept(oldState)) {
                        result.push_back(Token(stateToType(oldState), munchedInput));
                        munchedInput = "";
                        state = start();
                    } else {
                        if (failed(state)) {
                            munchedInput += *inputPosn;
                        }
                        throw LexerFailure("ERROR: Simplified maximal munch failed on input: " + munchedInput);
                    }
                }
            }
            return result;
        }
};

bool isCommentOrWhitespace(const Token token) {
    return (token.getType() == Token::COMMENT || token.getType() == Token::WHITESPACE);
}

/* @brief Scan the given input and filter useful tokens
 * 
 * @param input The whole tril input
 * @return The vector of useful tokens
 */
std::vector<Token> scan(const std::string &input) {
    static ASTstates rules;
    std::vector<Token> tokens;
    tokens = rules.simplifiedMaximalMunch(input);
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), isCommentOrWhitespace), tokens.end());
    return tokens;
}


std::ostream &operator<<(std::ostream &out, const Token &tok) {
    switch(tok.getType()) {
        case Token::ID: out << "ID"; break;
        case Token::STRING_: out << "STRING_"; break;
        case Token::INT: out << "INT"; break;
        case Token::DOUBLE_: out << "DOUBLE_"; break;
        case Token::AT:  out << "AT"; break;
        case Token::ZERO:  out << "ZERO"; break;
        case Token::QUOT:  out << "QUOT"; break;
        case Token::LPAREN:  out << "LPAREN"; break;
        case Token::RPAREN:  out << "RPAREN"; break;
        case Token::LPAREN_SQ:  out << "LPAREN_SQ"; break;
        case Token::RPAREN_SQ:  out << "RPAREN_SQ"; break;
        case Token::EQUALS: out << "EQUALS"; break;
        case Token::COMMA: out << "COMMA"; break;
        case Token::HEXINT:  out << "HEXINT"; break;
        default: throw LexerFailure("ERROR: Failed to determine input: " + tok.getValue());
    }
    out << " " << tok.getValue();
    return out;
}

class TokenIter {
    private:
        std::vector<Token>::iterator it;
        std::vector<Token>::iterator end;
    public:
        TokenIter(std::vector<Token>::iterator it, std::vector<Token>::iterator end)
            : it(it), end(end) {}
        TokenIter& operator++() {
            ++it;
            return *this;
        }
        bool isEnd() { return it == end; }
        Token peek() {
            auto temp = it;
            ++temp;
            return *temp;
        }
        Token& operator * () { return *it; }
        Token* operator -> () { return &*it; }
};

/* Grammer rules: (note: "." is being used to designate epsilon/null productions)
 * nodeList: nodeList node | .
 * node: '(' IDENTIFIER argList nodeList ')'
 * argList: argList arg | .
 * arg: value | IDENTIFIER '=' value | IDENTIFIER '=' '[' valueList ']'
 * valueList: value | valueList ',' value
 * value: INTEGER | DOUBLE | STRING | IDENTIFER
 *
 * INTEGER: [-]?[0-9]+ | [-]?0x[0-9a-fA-F]+
 * DOUBLE: [-]?[0-9]+[.][0-9]+
 * STRING: \"[^"]*\"
 * IDENTIFIER: [@-]?[a-zA-Z][a-zA-Z0-9]*
 */


/* @brief Check if the given token is a value
 *
 * @param tokenIt The iterator pointing to the input token
 * @return true if tokenIt is a value; false otherwise
 */
bool isValue(TokenIter tokenIt) {
    return (tokenIt->getType() == Token::DOUBLE_ || tokenIt->getType() == Token::INT ||
            tokenIt->getType() == Token::STRING_ || tokenIt->getType() == Token::HEXINT ||
            tokenIt->getType() == Token::ZERO || tokenIt->getType() == Token::ID);
}

/* @brief Build ASTValue from a value token
 * Exceptions can be thrown when the token type is not valid ASTValue type
 * 
 * @param token The token to be build an ASTValue from
 * @return The single ASTValue
 */
ASTValue* buildNodeValue(Token token) {
    Token::Type tokenType = token.getType();
    std::string tokenValue = token.getValue();
    if (tokenValue.size() > 1 && tokenType == Token::STRING_) { 
        tokenValue = tokenValue.substr(1, tokenValue.size() - 2);
    }
    char * cstr = new char[tokenValue.size() + 1];
    strcpy(cstr, tokenValue.c_str());
    if (tokenType == Token::STRING_) {
       return createStrValue(cstr);
    } else if (tokenType == Token::ID) {
        if        (tokenValue ==  "inf") {
           return createFloatingPointValue( std::numeric_limits<double>::infinity());
        } else if (tokenValue == "-inf") {
           return createFloatingPointValue(-std::numeric_limits<double>::infinity());
        } else if (tokenValue ==  "nan") {
           return createFloatingPointValue( std::numeric_limits<double>::quiet_NaN());
        } else if (tokenValue == "-nan") {
           return createFloatingPointValue(-std::numeric_limits<double>::quiet_NaN());
        } else if (tokenValue ==  "INF") {
           return createFloatingPointValue( std::numeric_limits<double>::infinity());
        } else if (tokenValue == "-INF") {
           return createFloatingPointValue(-std::numeric_limits<double>::infinity());
        } else if (tokenValue ==  "NAN") {
           return createFloatingPointValue( std::numeric_limits<double>::quiet_NaN());
        } else if (tokenValue == "-NAN") {
           return createFloatingPointValue(-std::numeric_limits<double>::quiet_NaN());
        } else if (tokenValue ==  "NaNQ") {
           return createFloatingPointValue( std::numeric_limits<double>::quiet_NaN());
        } else if (tokenValue == "-NaNQ") {
           return createFloatingPointValue(-std::numeric_limits<double>::quiet_NaN());
        } else {
           return createStrValue(cstr);
        }
    } else if (tokenType == Token::DOUBLE_) {
        double tmpdouble = std::atof(cstr);
        return createFloatingPointValue(tmpdouble);
    } else if (tokenType == Token::INT || tokenType == Token::ZERO) {
        return createIntegerValue(strtoull(cstr, NULL, 10));
    } else if (tokenType == Token::HEXINT) {
        return createIntegerValue(strtoull(cstr, NULL, 16));
    } else {
        throw ParserFailure("Cannot build ASTValue from non-value token: " + tokenValue);
    }
}

/* @brief Parse and build a list of node values
 * When the function returns, the token iterator points to the first ']' token of the parsed expression
 * 
 * @param tokenIt The token iterator pointing to node value tokens
 * @return The ASTValue linked list representing the node values
 */
ASTValue* parseValueList(TokenIter &tokenIt) {
    ASTValue* currentValue = NULL;
    while (tokenIt->getType() != Token::RPAREN_SQ) {
        if (currentValue == NULL && tokenIt->getType() != Token::COMMA) {
            currentValue = buildNodeValue(*tokenIt);
        } else if (currentValue && tokenIt->getType() != Token::COMMA) {
            appendSiblingValue(currentValue, buildNodeValue(*tokenIt));
        }
        ++tokenIt; // consume the node value
    }
    return currentValue;
}

/* @brief Parse and build a list of the node arguments.
 * When the function returns, the token iterator points to the first '(' or ')' token of the parsed expression.
 * Exceptions can be thrown when the parsed node argument grammer is NOT one of the following:
 *   1. ID EQUALS
 *   2. a valid ASTValue type
 *
 * @param tokenIt The tokens contains the node argument info
 * @return The node argument linked list
 */
ASTNodeArg* parseArgList(TokenIter &tokenIt) {
    ASTNodeArg* currentArg = NULL;
    ASTNodeArg* argList = NULL;

    while (tokenIt->getType() != Token::LPAREN && tokenIt->getType() != Token::RPAREN) {
        if (tokenIt->getType() == Token::ID && tokenIt.peek().getType() == Token::EQUALS) {
            char * node_args_name = new char[tokenIt->getValue().size() + 1];
            strcpy(node_args_name, tokenIt->getValue().c_str());
            ++tokenIt; // consume ID
            ++tokenIt; // consume EQUALS
            if (tokenIt->getType() == Token::LPAREN_SQ) {
                ++tokenIt; // consume LPAREN_SQ
                currentArg = createNodeArg(node_args_name, parseValueList(tokenIt), NULL);
            } else {
                currentArg = createNodeArg(node_args_name, buildNodeValue(*tokenIt), NULL);
            }
        } else if (isValue(tokenIt)) {
            currentArg = createNodeArg("", buildNodeValue(*tokenIt), NULL);
        } else {
            throw ParserFailure("Expecting value token but got " + tokenIt->getValue());
        }
        if (argList == NULL) {
            argList = currentArg;
        } else {
            appendSiblingArg(argList, currentArg);
        }
        ++tokenIt; // consume LPAREN_SQ or value
    }
    return argList;
}

/* @brief Parse and build the AST. 
 * When the function returns, the token iterator points to the token immediately
 * after the ')' of the parsed s-expression.
 * Exceptions can be thrown when the parsed string is incorrect in the following situation:
 *   1. LPAREN followed by end of node
 *   2. LPAREN followed by none ID type
 *   3. Missing RPAREN at the end of node
 *
 * @param tokenIt The tokens contains the AST info
 * @return The root of the AST
 */
ASTNode* buildAST(TokenIter &tokenIt) {
    ASTNode* siblingsList = NULL;
    while (!tokenIt.isEnd() && tokenIt->getType() == Token::LPAREN) {
        ASTNodeArg* argList = NULL;
        ASTNode* childrenList = NULL;
        ++tokenIt; // consume LPAREN
        if (tokenIt.isEnd()) {
            throw ParserFailure("Token stream ended unexpectedly after LPAREN");
        } else if (tokenIt->getType() != Token::ID) {
            throw ParserFailure("The next token of LPAREN should be IDENTIFIER, while current value is " +  tokenIt->getValue());
        }
        char* node_name = new char[tokenIt->getValue().size() + 1];
        strcpy(node_name, tokenIt->getValue().c_str());
        ++tokenIt; // consume ID
        if (isValue(tokenIt)) {
            argList = parseArgList(tokenIt);
        }
        childrenList = buildAST(tokenIt);
        if (tokenIt.isEnd() || tokenIt->getType() != Token::RPAREN) {
            throw ParserFailure("Missing RPAREN, while current value is " + tokenIt->getValue());
        }
        ++tokenIt; // consume to be the end or RPAREN

        if (siblingsList == NULL) {
            siblingsList = createNode(node_name, argList, childrenList, NULL);
        } else {
            appendSiblingNode(siblingsList, createNode(node_name, argList, childrenList, NULL));
        }
    }
    return siblingsList;
}

/* @brief Parse the file to build AST
 * 
 * @param in The tril input file
 * @return The root of the AST
 */
ASTNode* parseFile(FILE *in) {
    std::string result;
    while(!feof(in)) {
        char temp[100];
        fgets(temp, 100, in);
        result += temp;
    }
    std::vector<Token> scanToken = scan(result);
    TokenIter token = TokenIter(scanToken.begin(), scanToken.end());
    return buildAST(token);
}

/* @brief Parse the string to build AST
 * 
 * @param in The whole tril input
 * @return The root of the AST
 */
ASTNode* parseString(const char* in) {
    std::string in_str(in);
    std::vector<Token> tokenLine = scan(in_str);
    TokenIter token = TokenIter(tokenLine.begin(), tokenLine.end());
    return buildAST(token);
}
