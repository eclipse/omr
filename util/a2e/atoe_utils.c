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

#include <ctype.h>

/*
 * ======================================================================
 * Disable the redefinition of the system IO functions, this
 * prevents ATOE functions calling themselves.
 * ======================================================================
 */
#undef IBM_ATOE

/*
 * ======================================================================
 * Include all system header files.
 * ======================================================================
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>   /* for malloc() via e2a_string()/a2e_string() */
/*
 * ======================================================================
 * Define ae2,e2a,a2e_string, e2a_string
 * ======================================================================
 */
#include <atoe.h>

/*
 * ======================================================================
 * The following code has been taken form util.c
 * ======================================================================
 */

#define ERROR_RETVAL -1
#define SUCCESS 0
#define CheckRet(x) { if ((x) == ERROR_RETVAL) return ERROR_RETVAL; }

typedef struct InstanceData {
	char *buffer;
	char *end;
} InstanceData;

/**************************************************************************
 * name        - pchar
 * description - Print a charater to InstanceData buffer
 *               Taken from util.c used by fnumber and fstring.
 * parameters  - this   Structure holding the receiving buffer
 *               c      Character to add to buffer
 * returns     - int return code, 0 for success
 *************************************************************************/
static int
pchar(InstanceData *this, int c)
{
#if 0
	/*
	  We #if 0 this out as we have problem that shows up in following example
	  scenario.  The following code can be inserted in VM anywhere after the
	  call to iconv_init() is done:

	      char* str = "abc\177def";
	      char buf[1024];
	      sprintf(buf, "%s", str);
	      printf("str=%s, buf=%s, strlen(str)=%d, strlen(buf)=%d\n", str, buf, strlen(str), strlen(buf));

	  This causes the following output:

	      str=abc^_def, buf=abc^_def, strlen(str)=7, strlen(buf)=8

	  Note that strlen(str) != strlen(buf). This is a big problem, as strlen()
	  is used to determine how much buffer is needed need when we're using
	  sprintf(), etc..
	*/

	if (iscntrl(0xff & c) && c != '\n' && c != '\t') {
		c = '@' + (c & 0x1F);
		if (this->buffer >= this->end) {
			return ERROR_RETVAL;
		}
		*this->buffer++ = '^';
	}
#endif /* 0 */

	if (this->buffer >= this->end) {
		return ERROR_RETVAL;
	}
	*this->buffer++ = c;
	return SUCCESS;
}

/**************************************************************************
 * name        - fstring
 * description - Print a string to InstanceData buffer
 *               Taken from util.c used by atoe_vsnprintf.
 * parameters  - this           Structure holding the receiving buffer
 *               str            String to add to buffer
 *               left_justify   Left justify string flag
 *               min_width      Minimum width of string added to buffer
 *               precision
 * returns     - int return code, 0 for success
 *************************************************************************/
static int
fstring(InstanceData *this, char *str, int left_justify, int min_width,
		int precision)
{
	int pad_length;
	int width = 0;
	const char *p = str;

	if (NULL == str) {
		return ERROR_RETVAL;
	}

	for (width = 0; width < precision; width++) {
		if ('\0' == str[width]) {
			break;
		}
	}

	if (width < min_width) {
		pad_length = min_width - width;
	} else {
		pad_length = 0;
	}

	if (left_justify) {
		while (pad_length > 0) {
			CheckRet(pchar(this, ' '));
			pad_length -= 1;
		}
	}

	while (width-- > 0) {
		CheckRet(pchar(this, *p));
		p += 1;
	}

	if (!left_justify) {
		while (pad_length > 0) {
			CheckRet(pchar(this, ' '));
			pad_length -= 1;
		}
	}
	return SUCCESS;
}

#define MAX_DIGITS 32
typedef enum {
	FALSE = 0,
	TRUE = 1
} bool_t;

/**************************************************************************
 * name        - fnumber
 * description - Print an integer to InstanceData buffer
 *               Taken from util.c used by atoe_vsnprintf.
 * parameters  - this           Structure containing receiving buffer
 *               value          The value to format
 *               format_type    Character flag specifying format type
 *               left_justify   Left justify number flag
 *               min_width      Minimum number of charaters value will
 *                              occupy
 *               precision
 *               zero_pad       Pad number with zeros, flag
 * returns     - int return code, 0 for success
 *************************************************************************/
static int
fnumber(InstanceData *this, long value, int format_type, int left_justify,
		int min_width, int precision, bool_t zero_pad)
{
	int sign_value = 0;
	unsigned long uvalue;
	char convert[MAX_DIGITS + 1];
	int place = 0;
	int pad_length = 0;
	static char digits[] = "0123456789abcdef";
	int base = 0;
	bool_t caps = FALSE;
	bool_t add_sign = FALSE;

	switch (format_type) {
	case 'o':
	case 'O':
		base = 8;
		break;
	case 'd':
	case 'D':
	case 'i':                                                 /*ibm@9929*/
	case 'I':                                                 /*ibm@9929*/
		add_sign = TRUE; /*FALLTHROUGH*/
	case 'u':
	case 'U':
		base = 10;
		break;
	case 'X':
		caps = TRUE; /*FALLTHROUGH*/
	case 'x':
		base = 16;
		break;
	case 'p':
		caps = TRUE;  /*FALLTHROUGH*/
		base = 16;
		break;
	}

	uvalue = value;
	if (add_sign) {
		if (value < 0) {
			sign_value = '-';
			uvalue = -value;
		}
	}

	do {
		convert[place] = digits[uvalue % (unsigned)base];
		if (caps) {
			convert[place] = toupper(convert[place]);
		}
		place++;
		uvalue = (uvalue / (unsigned)base);
		if (place > MAX_DIGITS) {
			return ERROR_RETVAL;
		}
	} while (uvalue);
	convert[place] = 0;

	pad_length = min_width - place;
	if (pad_length < 0) {
		pad_length = 0;
	}
	if (left_justify) {
		if (zero_pad && pad_length > 0) {
			if (sign_value) {
				CheckRet(pchar(this, sign_value));
				--pad_length;
				sign_value = 0;
			}
			while (pad_length > 0) {
				CheckRet(pchar(this, '0'));
				--pad_length;
			}
		} else {
			while (pad_length > 0) {
				CheckRet(pchar(this, ' '));
				--pad_length;
			}
		}
	}
	if (sign_value) {
		CheckRet(pchar(this, sign_value));
	}

	while (place > 0 && --precision >= 0) {
		CheckRet(pchar(this, convert[--place]));
	}

	if (!left_justify) {
		while (pad_length > 0) {
			CheckRet(pchar(this, ' '));
			--pad_length;
		}
	}
	return SUCCESS;
}

/*
 *=======================================================================
 * name        - flongnumber (ibm@9094)
 * description - Print an 64bit integer to InstanceData buffer.
 * parameters  - this          Structure holding receiving buffer
 *               value         Number to convert
 *               format_type   Character flag defining format
 *               left_justify  Left justify number flag
 *               min_width     Minimum number of charaters value will
 *                             occupy
 *               precision
 *               zero_pad      Pad number with zeros, flag
 * returns     - int return code,  0 for success
 *=======================================================================
 */
static int
flongnumber(InstanceData *this, signed long long value, int format_type, int left_justify,
			int min_width, int precision, bool_t zero_pad)
{
	int sign_value = 0;
	unsigned long long uvalue;
	char convert[MAX_DIGITS + 1];
	int place = 0;
	int pad_length = 0;
	static char digits[] = "0123456789abcdef";
	int base = 0;
	bool_t caps = FALSE;
	bool_t add_sign = FALSE;

	switch (format_type) {
	case 'o':
	case 'O':
		base = 8;
		break;
	case 'd':
	case 'D':
	case 'i':                                                 /*ibm@9929*/
	case 'I':                                                 /*ibm@9929*/
		add_sign = TRUE; /*FALLTHROUGH*/
	case 'u':
	case 'U':
		base = 10;
		break;
	case 'X':
		caps = TRUE; /*FALLTHROUGH*/
	case 'x':
		base = 16;
		break;
	case 'p':
		caps = TRUE;  /*FALLTHROUGH*/
		base = 16;
		break;
	}

	uvalue = value;
	if (add_sign) {
		if (value < 0) {
			sign_value = '-';
			uvalue = -(value);
		}
	}

	do {
		convert[place] = digits[(uvalue % (unsigned long long)base)];
		if (caps) {
			convert[place] = toupper(convert[place]);
		}
		place++;
		uvalue = (uvalue / (unsigned long long)base);
		if (place > MAX_DIGITS) {
			return ERROR_RETVAL;
		}
	} while (uvalue);
	convert[place] = 0;

	pad_length = min_width - place;
	if (pad_length < 0) {
		pad_length = 0;
	}
	if (left_justify) {
		if (zero_pad && pad_length > 0) {
			if (sign_value) {
				CheckRet(pchar(this, sign_value));
				--pad_length;
				sign_value = 0;
			}
			while (pad_length > 0) {
				CheckRet(pchar(this, '0'));
				--pad_length;
			}
		} else {
			while (pad_length > 0) {
				CheckRet(pchar(this, ' '));
				--pad_length;
			}
		}
	}
	if (sign_value) {
		CheckRet(pchar(this, sign_value));
	}

	while (place > 0 && --precision >= 0) {
		CheckRet(pchar(this, convert[--place]));
	}

	if (!left_justify) {
		while (pad_length > 0) {
			CheckRet(pchar(this, ' '));
			--pad_length;
		}
	}
	return SUCCESS;
}

/**************************************************************************
 * name        - atoe_vsnprintf
 * description - Based on jio_vsnprintf which is in libjvm, we can't use
 *               the jio function has it would result in circularity at
 *               link time.
 * parameters  - str    Receiving string buffer
 *               count  Maximum length of receiving buffer
 *               fmt    Format specifier string
 *               argc   Variable length argument list
 * returns     -
 *************************************************************************/
int
atoe_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
{
	char *strvalue;
	const char *pattern;                                       /*ibm@8665*/
	long value;
	InstanceData this;
	bool_t left_justify, zero_pad;
	bool_t long_flag, long_long_flag;                         /*ibm@9094*/
	bool_t fPrecision;
	int min_width, precision, ch;
	static char NULLCHARSTRING[] = "[null]";                /*ibm@029013*/
	/*ibm@029013*/
	if (fmt == NULL) {                                      /*ibm@029013*/
		fmt = NULLCHARSTRING;                               /*ibm@029013*/
	}                                                       /*ibm@029013*/
	if (str == NULL) {
		return ERROR_RETVAL;
	}
	str[0] = '\0';

	this.buffer = str;
	this.end = str + count - 1;
	*this.end = '\0';          /* ensure null-termination in case of failure */

	while ((ch = *fmt++) != 0) {
		if (ch == '%') {
			zero_pad = FALSE;
			long_flag = FALSE;
			long_long_flag = FALSE;                            /*ibm@9094*/
			fPrecision = FALSE;
			pattern = fmt - 1;                                 /*ibm@8665*/
			left_justify = TRUE;
			min_width = 0;
			precision = this.end - this.buffer;

next_char:
			ch = *fmt++;
			switch (ch) {
			case 0:
				return ERROR_RETVAL;
			case '-':
				left_justify = FALSE;
				goto next_char;
			case '+':                                                                                       /*ibm@8665*/
				left_justify = TRUE;                             /*ibm@8665*/
				goto next_char;                                  /*ibm@8665*/
			case '0':            /* set zero padding if min_width not set */
				if (min_width == 0) {
					zero_pad = TRUE;
				}
				/*FALLTHROUGH*/
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (fPrecision == TRUE) {
					precision = precision * 10 + (ch - '0');
				} else {
					min_width = min_width * 10 + (ch - '0');
				}
				goto next_char;
			case '.':
				fPrecision = TRUE;
				precision = 0;
				goto next_char;
			case '*': {
				int temp_precision = va_arg(args, int);
				if (fPrecision == TRUE) {
					precision = temp_precision;
				} else {
					min_width = temp_precision;
				}
				goto next_char;
			}
			case 'l':
				if (long_flag) {
					long_long_flag = TRUE;
					long_flag = FALSE;
				} else {
					long_flag = TRUE;
				}
				goto next_char;
			case 's':
				strvalue = va_arg(args, char *);
				CheckRet(fstring(&this, strvalue, left_justify,
								 min_width, precision));
				break;
			case 'c':
				ch = va_arg(args, int);
				CheckRet(pchar(&this, ch));
				break;
			case '%':
				CheckRet(pchar(&this, '%'));
#if 0 /* J9 CMVC defect 74726 */
				CheckRet(pchar(&this, '%'));    /*ibm@5203 */
#endif
				break;
			case 'd':
			case 'D':
			case 'i':                                                 /*ibm@9929*/
			case 'I':                                                 /*ibm@9929*/
			case 'u':
			case 'U':
			case 'o':
			case 'O':
			case 'x':
			case 'X':
				if (long_long_flag) {                                      /*ibm@9094*/
					signed long long value64 = va_arg(args, signed long long);               /*ibm@9094*/ /* j9@72429 */

					CheckRet(flongnumber(&this, value64, ch, left_justify, /*ibm@9094*/
										 min_width, precision, zero_pad));     /*ibm@9094*/
				} else {
					value = long_flag ? va_arg(args, long) : va_arg(args, int);
					CheckRet(fnumber(&this, value, ch, left_justify,
									 min_width, precision, zero_pad));
				}
				break;
			case 'p':
				value = (long) va_arg(args, char *);
				CheckRet(fnumber(&this, value, ch, left_justify,
								 min_width, precision, zero_pad));
				break;
			case 'e':
			case 'E':
			case 'f':
			case 'F':
			case 'g':
			case 'G':
				{
					/* ibm@8665
					 * Add floating point support for dbl2str & flt2str
					 */
					char *b;
					int len;

					b = a2e((char *)pattern, fmt - pattern);

					/* Extract a double from args, this works for both doubles
					 * and floats,
					 * NB if we use float for a single precision floating
					 * point number the result is wrong.
					 */
					len = sprintf(this.buffer, b, va_arg(args, double));
					free(b);
					b = e2a_string(this.buffer);
					strcpy(this.buffer, b);
					free(b);
					this.buffer += len;
				}
				break;
			default:
				/* ibm@9094
				 * If all we got was "%ll" assume
				 * there should be a d on the end
				 */
				if (long_long_flag) {
					signed long long value64 = va_arg(args, signed long long);

					CheckRet(flongnumber(&this, value64, 'd', left_justify,
										 min_width, precision, zero_pad));

					fmt--; /*backup so we don't lose the current char */
					break;
				}

				return ERROR_RETVAL;
			}
		} else {
			CheckRet(pchar(&this, ch));
		}
	}
	*this.buffer = '\0';
	return strlen(str);
}

/* END OF FILE */
