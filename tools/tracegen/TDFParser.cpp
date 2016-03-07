/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EventTypes.hpp"
#include "FileUtils.hpp"
#include "StringUtils.hpp"
#include "TDFParser.hpp"

#define DEFAULT_DAT_FILE "c_TraceFormat.dat"

void
TDFParser::init(FileReader *reader, bool treatWarningAsError)
{
	_fileReader = reader;
	_treatWarningAsError = treatWarningAsError;
}

J9TDFFile *
TDFParser::parse()
{
	int type = -1;
	unsigned int id = 0;

	J9TDFFile *tdf = (J9TDFFile *)Port::omrmem_calloc(1, sizeof(J9TDFFile));
	if (NULL == tdf) {
		eprintf("Failed to allocate memory");
		goto failed;
	}

	while (_fileReader->hasNext()) {
		const char *line = _fileReader->next();
		/*
		 * Lines to parse (keywords are case insensitive):
		 *  executable=
		 *  datfilename=
		 *  submodules=
		 *  cfilesoutputdirectory= **UNUSED in J9**
		 *  auxiliary
		 *  // a commment line
		 * Trace* for trace point types:
		 *   TraceEvent
		 *   TraceException
		 *   TraceEntry
		 *   TraceEntry-Exception
		 *   TraceExit
		 *   TraceExit-Exception
		 *   TraceMem (Unused)
		 *   TraceMemException (Unused)
		 *   TraceDebug (Unused)
		 *   TraceDebug-Exception (Unused)
		 *   TracePerf (Unused)
		 *   TracePerf-Exception (Unused)
		 *   TraceAssert
		 */
		if (0 == strlen(line) || StringUtils::startsWithUpperLower(line, "//") || StringUtils::startsWithUpperLower(line, "\n") || StringUtils::startsWithUpperLower(line, "\r")) {
			/* skip */
		} else if (StringUtils::startsWithUpperLower(line, "Submodules=")) {
			tdf->header.submodules = strdup(strrchr(line, '=') + 1);
		} else if (StringUtils::startsWithUpperLower(line, "auxiliary")) {
			tdf->header.auxiliary = true;
		} else if (StringUtils::startsWithUpperLower(line, "executable=")) {
			tdf->header.executable = strdup(strrchr(line, '=') + 1);
		} else if (StringUtils::startsWithUpperLower(line, "datfilename=")) {
			tdf->header.datfilename = strdup(strrchr(line, '=') + 1);
		} else if (StringUtils::startsWithUpperLower(line, "Trace")) {
			/* This line is a trace point. */
			if (RC_OK != findTraceTypeIndex(line, &type)) {
				goto failed;
			}

			if (0 > type) {
				FileUtils::printError("Invalid tracepoint line: %s found\n", line);
				return NULL;
			}

			/* We've found a trace point. Add one to the list. */
			if (NULL == tdf->tracepoints) {
				tdf->tracepoints = (J9TDFTracepoint *)Port::omrmem_calloc(1, sizeof(J9TDFTracepoint));
				if (NULL == tdf->tracepoints) {
					eprintf("Failed to allocate memory");
					goto failed;
				}

				tdf->lasttp = tdf->tracepoints;
			} else {
				tdf->lasttp->nexttp = (J9TDFTracepoint *)Port::omrmem_calloc(1, sizeof(J9TDFTracepoint));
				if (NULL == tdf->lasttp->nexttp) {
					eprintf("Failed to allocate memory\n");
					goto failed;
				}

				tdf->lasttp = tdf->lasttp->nexttp;
			}
			tdf->lasttp->type = static_cast<TraceEventType>(type);

			if (0 != processTracePointDetail(line, tdf->lasttp, tdf->header.executable, id++, _fileReader->_fileName, _fileReader->_lineNumber)) {
				FileUtils::printError("Failed to parse trace line %s\n", line);
				Port::omrmem_free((void **)&line);
				goto failed;
			}
		} else {
			FileUtils::printError("Unsupported line: '%s' %s:%d\n", line, _fileReader->_fileName, _fileReader->_lineNumber);
		}
		Port::omrmem_free((void **)&line);
	}

	if (NULL == tdf->header.datfilename) {
		tdf->header.datfilename = (char *)Port::omrmem_calloc(1, strlen(DEFAULT_DAT_FILE) + 1);
		if (NULL == tdf->header.datfilename) {
			printf("Failed to allocate memory\n");
			goto failed;
		}

		strncpy(tdf->header.datfilename, DEFAULT_DAT_FILE, strlen(DEFAULT_DAT_FILE));
	}

	return tdf;

failed:
	return NULL;
}

RCType
TDFParser::processTracePointDetail(const char *line, J9TDFTracepoint *tp, const char *module, unsigned int id, const char *fileName, unsigned int lineNumber)
{
	RCType rc = RC_FAILED;
	char *tok = NULL;
	tp->level = -1;
	tp->hasEnv = true;

	/* Save original string because strtok mangles the source string by placing \0 at delimiter */
	char *tokLine = strdup(line);

	/* read the first token */
	tok = strtok(tokLine, " \t");
	line = line + strlen(tok) + 1;
	while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
		line = line + 1;
	}

	tp->name = strdup(strchr(tok, '=') + 1);
	Port::omrmem_free((void **)&tokLine);

	/* now parse all of the optional fields */
	tokLine = strdup(line);

	tok = strtok(tokLine, " \t");
	while (NULL != tok) {
		if (StringUtils::startsWithUpperLower(tok, "Explicit")) {
			/* tp->isExplicit = true; */
			FileUtils::printError("WARNING : obsolete keyword 'Explicit' in %s:%u\n", fileName, lineNumber);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Suffix")) {
			/* tp->isSuffix = true; */
			FileUtils::printError("WARNING : obsolete keyword 'Suffix' in %s:%u\n", fileName, lineNumber);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Prefix")) {
			/* tp->isPrefix = true; */
			FileUtils::printError("WARNING : obsolete keyword 'Prefix' in %s:%u\n", fileName, lineNumber);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Exception")) {
			/* tp->isException = true; */
			FileUtils::printError("WARNING : obsolete keyword 'Exception' in %s:%u\n", fileName, lineNumber);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Private")) {
			/* tp->isPrivate = true; */
			FileUtils::printError("WARNING : obsolete keyword 'Private' in %s:%u\n", fileName, lineNumber);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Test")) {
			tp->test = true;
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "NoEnv")) {
			tp->hasEnv = false;
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Obsolete")) {
			tp->obsolete = true;
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			tok = strtok(NULL, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, (UT_ASSERT_TYPE == tp->type ? "Assert=" : "Template="))) {
			tp->format = getTemplate(line, (UT_ASSERT_TYPE == tp->type ? "Assert=" : "Template="));
			line = line + strlen((UT_ASSERT_TYPE == tp->type ? "Assert=" : "Template=")) + (strlen(tp->format) + 2 /* two quotes */);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			Port::omrmem_free((void **)&tokLine);
			tokLine = strdup(line);

			tok = strtok(tokLine, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Level=")) {
			if (RC_OK != StringUtils::getPositiveIntValue(tok, "Level=", &tp->level)) {
				goto failed;
			}
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			Port::omrmem_free((void **)&tokLine);
			tokLine = strdup(line);

			tok = strtok(tokLine, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Overhead=")) {
			if (RC_OK != StringUtils::getPositiveIntValue(tok, "Overhead=", &tp->overhead)) {
				goto failed;
			}
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			Port::omrmem_free((void **)&tokLine);
			tokLine = strdup(line);
			tok = strtok(tokLine, " \t");
		} else if (StringUtils::startsWithUpperLower(tok, "Group=")) {
			tp->groups = getGroups(tok);
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			Port::omrmem_free((void **)&tokLine);
			tokLine = strdup(line);

			tok = strtok(tokLine, " \t");
		} else {
			line = line + strlen(tok);
			while ((' ' == *line  || '\t' == *line) && '\0' != *line) {
				line = line + 1;
			}
			Port::omrmem_free((void **)&tokLine);
			tokLine = strdup(line);
			tok = strtok(tokLine, " \t");
		}
	}

	/* Assign default level */
	if ((unsigned int)-1 == tp->level) {
		tp->level = 1;
	}

	if (NULL == tp->format) {
		tp ->format = (char *)Port::omrmem_calloc(1, (strlen("null") + 1));
		strncpy(tp ->format,  "null", strlen("null"));
	}

	if (UT_ASSERT_TYPE == tp->type) {
		/* Assertion parameters are always \"\\377\\4\\377\" for "** ASSERTION FAILED ** at %s:%d: %s" */
		tp->parameters = NULL;
		rc = processAssertTemplate(tp->format, &tp->parmCount);
	} else {
		const char *key = "%";
		const char *pch = strpbrk(tp->format, key);
		unsigned int percentSignCount = 0;
		while (NULL != pch) {
			percentSignCount += 1;
			pch = strpbrk(pch + 1, key);
		}

		size_t len = 5 /* \\\"\\\"\0 when no parameters specified */ + (percentSignCount * (strlen(TRACE_DATA_TYPE_STRING) + strlen(TRACE_DATA_TYPE_PRECISION))) /* TRACE_DATA_TYPE_STRING is the longest parameter string + precision specifier*/;
		tp->parameters = (char *)Port::omrmem_calloc(1, len);
		if (NULL == tp->parameters) {
			FileUtils::printError("Failed to allocate memory\n");
			goto failed;
		}

		rc = processTemplate(tp->format, tp->parameters, &tp->parmCount, fileName, lineNumber);

		/* We pass NULL for no parameters not the empty string "". */
		if (0 == tp->parmCount) {
			Port::omrmem_free((void **)&tp->parameters);
			tp->parameters = (char *)Port::omrmem_calloc(1, strlen("NULL") + 1);
			strcpy(tp->parameters, "NULL");
		}
	}

	return rc;

failed:
	return rc;
}

/*
 * Find the template value from a tracepoint line.
 * Allocates a buffer to store it in. (Caller needs to free!)
 */
char *
TDFParser::getTemplate(const char *line, const char *key)
{
	char *format = NULL;
	size_t len = 0;

	const char *vpos = line;

	char *pos = NULL;
	vpos += strlen(key) + 1; /* Remove leading quote. */
	pos = (char *)vpos;

	/*
	 * There's actually only one quoted string allowed in a trace point definition,
	 * the template. So bypass clever quote handling by assuming that the quote at
	 * the other end of the line *must* belong to this template.
	 * Note that the template string should include the quotes as it will be included in
	 * #defines later on!
	 */
	while (0 != *pos) {
		pos++;
	}
	while ('\"' != *pos && pos != vpos) {
		pos--;
	}

	len = (pos - vpos);
	format = (char *)Port::omrmem_calloc(1, len + 1);
	if (NULL == format) {
		FileUtils::printError("Failed to allocate memory\n");
		goto failed;
	}
	strncpy(format, vpos, len);

	return format;

failed:
	return NULL;
}

/*
 * Find the groups value from a trace point line.
 * Note that a trace point can be in more than one group
 * so this function allocates and returns an array of strings.
 * (Which the caller must free later on.)
 */
char **
TDFParser::getGroups(const char *line)
{
	char *pos = NULL;
	char **values = NULL;
	unsigned int index = 0;
	unsigned int groupCount = 0;
	size_t len = strlen(line);

	const char *vpos = line;

	vpos += strlen("Group=");
	values = NULL;
	groupCount = 1;

	for (size_t i = 0; i < len; i++) {
		if (',' == *(line + i)) {
			groupCount += 1;
		}
	}

	values = (char **)Port::omrmem_calloc(groupCount + 1, sizeof(char *));
	if (NULL == values) {
		FileUtils::printError("Failed to allocate memory\n");
		goto failed;
	}

	values[0] = (char *)Port::omrmem_calloc(1, len + 1);
	if (NULL == values[0]) {
		FileUtils::printError("Failed to allocate memory\n");
		goto failed;
	}

	pos = values[0];
	strncpy(values[0], vpos, len);
	while (0 != *pos) {
		if (',' == *pos) {
			/* Null terminate this group name.
			 * Make the next group name the next
			 * element.
			 */
			*pos = 0;
			pos++;
			index += 1;
			values[index] = strdup(pos);
		} else {
			pos++;
		}
	}

	index++;
	values[index] = NULL; /* Null terminate our array */

	return values;

failed:
	return NULL;
}

bool
TDFParser::isNoEnv(const char *line)
{
	return NULL != StringUtils::containsUpperLower((char *)line, " NoEnv ");
}

bool
TDFParser::isObsolete(const char *line)
{
	return NULL != StringUtils::containsUpperLower((char *)line, " Obsolete ");
}

bool
TDFParser::isTest(const char *line)
{
	return NULL != StringUtils::containsUpperLower((char *)line, " Test ");
}

/*
 * Params:
 * formatTemplate  - the printf style format string
 * str - a malloc'd buffer to hold the result
 */
RCType
TDFParser::processAssertTemplate(const char *formatTemplate, unsigned int *count)
{
	RCType rc = RC_OK;
	char *pos = (char *)formatTemplate;
	char *numStart = NULL;
	unsigned int max = 0;
	unsigned int num = 0;

	while ('\0' != *pos) {
		if ('P' == *pos) {
			pos++;
			numStart = pos;
			while (isdigit(*pos)) {
				pos++;
			}
			num = atoi(numStart);
			max = max > num ? max : num;
		}
		pos++;
	}

	*count = max;
	return rc;
}

/****
 * Port of processTemplate from original java code.
 * Params:
 * formatTemplate  - the printf style format string
 * str - a malloc'd buffer to hold the result
 */
RCType
TDFParser::processTemplate(const char *formatTemplate, char *str, unsigned int *count, const char *fileName, unsigned int lineNumber)
{
	char *pos = (char *)formatTemplate;
	RCType rc = RC_OK;
	char *modifier = NULL;
	bool precision;
	size_t templateLength = strlen(formatTemplate);
	unsigned int parmCount = 0;

	/*
	 * The following bitmasks indicate the allowed state transitions when
	 * processing a '%' type specifier. The states are noted in comments at
	 * the point they become active. This seemingly complex technique is
	 * required because we need to catch syntax errors now, rather than at
	 * trace formatting time.
	 */
	char allowFromStates[] = { (char) 0xBE, /*
											 * State 0 allows entry from
											 * states 0, 2 - 6
											 */
							   (char) 0x80, /* State 1 allows entry from states 0 */
							   (char) 0xE0, /* State 2 allows entry from states 1,2 */
							   (char) 0xF0, /* State 3 allows entry from states 1,2,3 */
							   (char) 0xF0, /* State 4 allows entry from states 1,2,3 */
							   (char) 0x0c, /* State 5 allows entry from states 4,5 */
							   (char) 0xF4, /* State 6 allows entry from states 1,2,3,5 */
							   (char) 0xF6, /* State 7 allows entry from states 1,2,3,5,6 */
							   (char) 0x41 /* State 8 allows entry from states 1,7 */
							 };

	unsigned int state;
	/*
	 * State number to bit lookup table.
	 */
	char stateBits[] = { (char) 0x80, (char) 0x40, (char) 0x20,
						 (char) 0x10, (char) 0x08, (char) 0x04, (char) 0x02, (char) 0x01
					   };

	strcat(str, "\"");

	while ((NULL != (pos = strchr(pos, '%'))) && (RC_OK == rc)) {
		/* printf("Found a possible format specifier"); */
		modifier = (char *)-1;
		precision = false;

		/*
		 * Enter state 0 (Parsing specifier)
		 */
		state = 0;
		while ((++pos < (formatTemplate + templateLength))
			&& (0 != (stateBits[state] & allowFromStates[0]))
			&& (RC_OK == rc)
		) {
			/* printf("Parsing started for format specifier\n"); */
			switch (*pos) {
			/*
			 * Enter state 1 (Found another %)
			 */
			case '%':
				if (0 != (stateBits[state] & allowFromStates[1])) {
					state = 1;
				} else {
					rc = RC_FAILED;
				}
				break;
			/*
			 * Enter state 2 (Found a flag)
			 */
			case '+':
			case '-':
			case ' ':
			case '#':
				if (0 == (stateBits[state] & allowFromStates[2])) {
					rc = RC_FAILED;
				}
				state = 2;
				break;
			case '0':
				/*
				 * 0 can be a width, a precision, or a request for left
				 * padding with 0.
				 */
				if (formatTemplate + templateLength > pos) {
					/*
					 * For a valid string, there must always be a follow on
					 * type specifier, so this code runs in all valid cases
					 */
					char peekAhead = *(pos + 1);
					if (3 == state) {
						/* This 0 is a continuation of a width specifier */
					} else if ('.' == peekAhead) {
						/*
						 * This 0 is part of a width specifier if the next
						 * char is a .
						 */
						if (0 == (stateBits[state] & allowFromStates[3])) {
							rc = RC_FAILED;
						}
						state = 3;
					} else if (isdigit(peekAhead)) {
						/*
						 * A digit follows, could be in the middle of a big
						 * width/precision number, or could be that this 0
						 * is for 0 padding and the next number is the
						 * beginning of a width specifier. Would need to
						 * completely rewrite with a tokeniser to be
						 * certain, so naively take 0 padding case for now.
						 * %.1024s for example may not work with this
						 * assumption.
						 */
						if (0 == (stateBits[state] & allowFromStates[2])) {
							rc = RC_FAILED;
						}
						state = 2;
					} else if (5 == state) {
						/*
						 * We are in the precision specifier, so this can't
						 * be 0 padding
						 */
						if (0 == (stateBits[state] & allowFromStates[5])) {
							rc = RC_FAILED;
						}
						state = 5;
					} else {
						/*
						 * This 0 will be interpreted as 0 padding, reached
						 * in instances where peekAhead is a type specifier
						 */
						if (0 == (stateBits[state] & allowFromStates[2])) {
							rc = RC_FAILED;
						}
					}
				} else {
					rc = RC_FAILED;
				}
				break;

			/*
			 * Enter state 3 or 5 (Found a width or precision)
			 */
			case '*':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (0 != (stateBits[state] & allowFromStates[3])) {
					state = 3;
				} else if (0 != (stateBits[state] & allowFromStates[5])) {
					state = 5;
					if ('*' == *pos) {
						precision = true;
					}
				} else {
					rc = RC_FAILED;
				}
				break;

			/*
			 * Enter state 4 (Found a period)
			 */
			case '.':
				if (0 == (stateBits[state] & allowFromStates[4])) {
					rc = RC_FAILED;
				}
				state = 5;
				break;
			/*
			 * Enter state 6 (Found a size specifier)
			 */
			case 'h':
			case 'L':
			case 'z':
			case 'P': /* The P size specifier indicates an integer that can contain a pointer */
				if (0 == (stateBits[state] & allowFromStates[6])) {
					rc = RC_FAILED;
				} else {
					modifier = pos;
					state = 6;
				}
				break;
			case 'l':
				if (0 == (stateBits[state] & allowFromStates[6])) {
					rc = RC_FAILED;
				} else {
					modifier = pos;
					state = 6;
					/* Check for a long long */
					if (formatTemplate + templateLength > pos + 1) {
						switch (*(pos + 1)) {
						case 'l':
							pos++;
							break;
						/*
						 * %ld, %li, %lx, %lX and %lu are not valid
						 * modifiers.
						 */
						case 'd':
						case 'i':
						case 'x':
						case 'X':
						case 'u':
							if (_treatWarningAsError) {
								eprintf("ERROR : %%l is not valid modifier. (parameter %u in mutated format string %s). %s:%u", parmCount, formatTemplate, fileName, lineNumber);
								return RC_FAILED;
							} else {
								eprintf("WARNING : %%l is not valid modifier. (parameter %u in mutated format string %s). %s:%u", parmCount, formatTemplate, fileName, lineNumber);
							}
						}
					}
				}
				break;
			/*
			 * Enter state 7 (Found a type)
			 */
			case 'c':
			case 'd':
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
			case 'i':
			case 'o':
			case 'p':
			case 'x':
			case 'X':
			case 'u':
			case 'J':
				if (precision) {
					rc = RC_FAILED;
					break;
				}
				/* Fall through */
			case 's':
				if (0 == (stateBits[state] & allowFromStates[7])) {
					rc = RC_FAILED;
				}

				switch (*pos) {
				case 'c':
					strcat(str, TRACE_DATA_TYPE_CHAR);
					break;
				case 'X':
					/* Fall through */
				case 'x':
					/* Fall through */
				case 'd':
				case 'i':
				case 'o':
				case 'u':
					if ((char *)-1 == modifier) {
						strcat(str, TRACE_DATA_TYPE_INT32);
						break;
					}
					switch (*modifier) {

					case 'h':
						strcat(str, TRACE_DATA_TYPE_SHORT);
						break;

					case 'l':
					case 'L':
						strcat(str, TRACE_DATA_TYPE_INT64);
						break;
					case 'z':
					case 'P':
						strcat(str, TRACE_DATA_TYPE_POINTER);
						break;

					default:
						flagTdfSyntaxError("Invalid length modifier and type", parmCount, formatTemplate, fileName, lineNumber);
						rc = RC_FAILED;
					}
					break;
				case 'J': /* Java object reference. */
					/* Fall through */
				case 'p':
					strcat(str, TRACE_DATA_TYPE_POINTER);
					break;
				case 'e':
				case 'E':
				case 'f':
				case 'g':
				case 'G':
					/*
					 * CMVC 164940 All %f tracepoints are internally
					 * promoted to double. Affects: TraceFormat
					 * com/ibm/jvm/format/Message.java TraceFormat
					 * com/ibm/jvm/trace/format/api/Message.java VM_Common
					 * ute/ut_trace.c TraceGen OldTrace.java
					 */

					if ((char *)-1 == modifier) {
						strcat(str, TRACE_DATA_TYPE_DOUBLE);
					} else if ('L' == *modifier) {
						strcat(str, TRACE_DATA_TYPE_LONG_DOUBLE);
					} else {
						flagTdfSyntaxError("Invalid length modifier and type", parmCount, formatTemplate, fileName, lineNumber);
						rc = RC_FAILED;
					}
					break;
				case 's':
					if (precision) {
						strcat(str, TRACE_DATA_TYPE_PRECISION);
						parmCount++;
					}
					strcat(str, TRACE_DATA_TYPE_STRING);
					break;
				}
				parmCount++;
				state = 7;
				break;
			default:
				rc = RC_FAILED;
				break;
			}
		}
		if ((RC_OK != rc) || (0 == (stateBits[state] & allowFromStates[8]))) {
			flagTdfSyntaxError("Invalid template specifier", parmCount, formatTemplate, fileName, lineNumber);
			rc = RC_FAILED;
		} else {
			state = 1;
		}
	}
	strcat(str, "\"");
	*count = parmCount;
	return rc;
}

void
TDFParser::flagTdfSyntaxError(const char *message, unsigned int parmCount, const char *formatTemplate, const char *fileName, unsigned int lineNumber)
{
	/* Caller has not yet incremented parmCount to include the invalid parameter. */
	eprintf("%s: in parameter %u of format string at %s:%u\n\tformatString: \"%s\"", message, parmCount + 1, fileName, lineNumber, formatTemplate);
}

RCType
TDFParser::findTraceTypeIndex(const char *traceLine, int *result)
{
	RCType rc = RC_FAILED;
	const char *TRACE_TYPES[] = {
		"TraceEvent",
		"TraceException",
		"TraceEntry",
		"TraceEntry-Exception",
		"TraceExit",
		"TraceExit-Exception",
		"TraceMem",
		"TraceMemException",
		"TraceDebug",
		"TraceDebug-Exception",
		"TracePerf",
		"TracePerf-Exception",
		"TraceAssert"
	};

	for (int type = 0; UT_MAX_TYPES > type; type++) {
		const char *separator = strchr(traceLine, '=');
		size_t len = (separator - traceLine);
		if (strlen(TRACE_TYPES[type]) > len) {
			len = strlen(TRACE_TYPES[type]);
		}
		if (0 == strncmp(TRACE_TYPES[type], traceLine, len)) {
			*result = type;
			rc = RC_OK;
		}
	}

	return rc;
}
