/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#ifndef OMRCOMP_H
#define OMRCOMP_H

#include <stddef.h>

#if defined(__MINGW32__) || defined(__MINGW64__)
/* mingw is used in compilation on some windows platforms. The version used
 * does not support __thiscall, used in stdint.h on windows. Defining it to
 * nothing allows compilation and therefore allows us to use the types
 * defined within stdint.h.
 */
#define __thiscall
#endif /* __MINGW32__ || __MINGW64__ */

#include <stdint.h>
#include "omrcfg.h"
#if defined(__cplusplus) && (defined(__xlC__) || defined(J9ZOS390))
#include <builtins.h>
#endif

/* Platform specific VM "types":
	UDATA			unsigned data, can be used as an integer or pointer storage.
	intptr_t			signed data, can be used as an integer or pointer storage.
	U_64 / I_64		unsigned/signed 64 bits.
	U_32 / I_32		unsigned/signed 32 bits.
	U_16 / I_16		unsigned/signed 16 bits.
	U_8 / I_8		unsigned/signed 8 bits (bytes -- not to be confused with char)
	BOOLEAN			something that can be zero or non-zero.
*/
typedef uint64_t				U_64;
typedef int64_t					I_64;
typedef uintptr_t				UDATA;
typedef uint32_t				U_32;
typedef uint16_t				U_16;
typedef uint8_t					U_8;
typedef intptr_t				IDATA;
typedef int32_t					I_32;
typedef int16_t					I_16;
typedef int8_t					I_8;

#if defined(MVS)
typedef int32_t					BOOLEAN;
#elif defined(LINUX) || defined(RS6000) || defined(J9ZOS390) || defined(OSX) /* MVS */
typedef uint32_t					BOOLEAN;
#else /* MVS */
/* Don't typedef BOOLEAN since it's already defined on Windows */
#define BOOLEAN UDATA
#endif /* MVS */

/*
USE_PROTOTYPES:			Use full ANSI prototypes.

CLOCK_PRIMS:					We want the timer/clock prims to be used

LITTLE_ENDIAN:				This is for the intel machines or other
											little endian processors. Defaults to big endian.

NO_LVALUE_CASTING:	This is for compilers that don't like the left side
											of assigns to be cast.  It hacks around to do the
											right thing.

ATOMIC_FLOAT_ACCESS:	For the hp720 so that float operations will work.

LINKED_USER_PRIMITIVES:	Indicates that user primitives are statically linked
													with the VM executeable.

OLD_SPACE_SIZE_DIFF:	The 68k uses a different amount of old space.
											This "legitimizes" the change.

SIMPLE_SIGNAL:		For machines that don't use real signals in C.
									(eg: PC, 68k)

OS_NAME_LOOKUP:		Use nlist to lookup user primitive addresses.

SYS_FLOAT:	For the MPW C compiler on MACintosh. Most of the math functions
						there return extended type which has 80 or 96 bits depending on 68881 option.
						On All other platforms it is double

FLOAT_EXTENDED: If defined, the type name for extended precision floats.

PLATFORM_IS_ASCII: Must be defined if the platform is ASCII

EXE_EXTENSION_CHAR: the executable has a delimiter that we want to stop at as part of argv[0].

*/


/* Linux ANSI compiler (gcc) and OSX (clang). */
#if defined(LINUX) || defined (OSX)

/* NOTE: Linux supports different processors -- do not assume 386 */

#if defined(J9HAMMER) || defined(S39064) || defined(LINUXPPC64)

/* LinuxPPC64 is like AIX64 so we need direct function pointers */
#if defined(LINUXPPC64)
#if defined(OMR_ENV_LITTLE_ENDIAN)
/* LINUXPPC64LE has a new ABI that uses direct functions, not function descriptors */
#define TOC_UNWRAP_ADDRESS(wrappedPointer) ((void *) (wrappedPointer))
extern uintptr_t getTOC();
#define TOC_STORE_TOC(dest,wrappedPointer) ((dest) = getTOC())
#else /* OMR_ENV_LITTLE_ENDIAN */
#define TOC_UNWRAP_ADDRESS(wrappedPointer) (((void **)(wrappedPointer))[0])
#define TOC_STORE_TOC(dest,wrappedPointer) (dest = (((uintptr_t*)(wrappedPointer))[1]))
#endif /* OMR_ENV_LITTLE_ENDIAN */
#endif /* LINUXPPC64 */
#endif /* J9HAMMER || S39064 || LINUXPPC64 */

typedef double SYS_FLOAT;

#define J9CONST64(x) x##LL
#define J9CONST_I64(x) x##LL
#define J9CONST_U64(x) x##ULL

#define NO_LVALUE_CASTING
#define FLOAT_EXTENDED	long double
#define PLATFORM_IS_ASCII
#define PLATFORM_LINE_DELIMITER	"\012"
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_STR "/"

/* no priorities on Linux */
#define J9_PRIORITY_MAP {0,0,0,0,0,0,0,0,0,0,0,0}


#if (defined(LINUXPPC) && !defined(LINUXPPC64)) || defined(S390) || defined(J9HAMMER)
#define VA_PTR(valist) ((va_list*)&valist[0])
#endif

#if defined(__GNUC__)
#define VMINLINE_ALWAYS inline __attribute((always_inline))
/* If -O0 is in effect, define VMINLINE to be empty */
#if !defined(__OPTIMIZE__)
#define VMINLINE
#endif

#elif defined(__xlC__)
/*
 * xlC11 C++ compiler reportedly supports attributes before function names, but we've only tested xlC12.
 * The C compiler doesn't support it.
 */
#if defined(__cplusplus) && (__xlC__ >= 0xc00)
#define VMINLINE_ALWAYS inline __attribute__((always_inline))
#endif
#endif /* __xlC__ */

#ifndef VMINLINE_ALWAYS
#define VMINLINE_ALWAYS inline
#endif

#define HAS_BUILTIN_EXPECT

#endif /* defined(LINUX) || defined (OSX) */


/* MVS compiler */
#ifdef MVS
typedef double 					SYS_FLOAT;
typedef long double				FLOAT_EXTENDED;

#define J9CONST64(x) x##LL
#define J9CONST_I64(x) x##LL
#define J9CONST_U64(x) x##ULL

#define NO_LVALUE_CASTING
#define PLATFORM_LINE_DELIMITER	"\025"
#define DIR_SEPARATOR '.'
#define DIR_SEPARATOR_STR "."

#include "esmap.h"

#endif /* MVS */


#define GLOBAL_DATA(symbol) ((void*)&(symbol))
#define GLOBAL_TABLE(symbol) GLOBAL_DATA(symbol)

/* RS6000 */

/* The AIX platform has the define AIXPPC and RS6000,
	this means AIXPPC inherits from the RS6000.*/

#if defined(RS6000)
typedef double SYS_FLOAT;

#define J9CONST64(x) x##LL
#define J9CONST_I64(x) x##LL
#define J9CONST_U64(x) x##ULL

#define NO_LVALUE_CASTING
#define PLATFORM_LINE_DELIMITER	"\012"
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_STR "/"

#define TOC_UNWRAP_ADDRESS(wrappedPointer) (((void **)(wrappedPointer))[0])
#define TOC_STORE_TOC(dest,wrappedPointer) (dest = (((uintptr_t*)(wrappedPointer))[1]))

/*
 * Have to have priorities between 40 and 60 inclusive for AIX >=5.3
 * AIX 5.2 ignores them
 */

#if (defined(J9OS_I5) && !defined(J9OS_I5_V5R4)) /* i5/OS V6R1 or newer */
#define J9_PRIORITY_MAP  { 55,56,57,58,59,60,60,60,60,60,60,60 }
#define J9_PRIORITY_MAP_ALT  { 54,55,55,56,56,57,57,58,58,59,59,60 }

#elif (defined(J9OS_I5) && defined(J9OS_I5_V5R4)) /* i5/OS V5R4 */
#define J9_PRIORITY_MAP  { 54,55,55,56,56,57,57,58,58,59,59,60 }
#define J9_PRIORITY_MAP_ALT  { 55,56,57,58,59,60,60,60,60,60,60,60 }

#else /* not i5/OS */
#define J9_PRIORITY_MAP  { 40,41,43,45,47,49,51,53,55,57,59,60 }
#endif /* J9OS_I5 checks */


#if defined(__xlC__)
/*
 * xlC11 C++ compiler reportedly supports attributes before function names, but we've only tested xlC12.
 * The C compiler doesn't support it.
 */
#if defined(__cplusplus) && (__xlC__ >= 0xc00)
#define VMINLINE_ALWAYS inline __attribute__((always_inline))
#endif
#endif /* __xlC__ */

/* XLC doesn't support __attribute before a function name and doesn't support inline in the stdc89 language level */
#ifndef VMINLINE_ALWAYS
#define VMINLINE_ALWAYS __inline__
#endif

#define HAS_BUILTIN_EXPECT
#endif /* RS6000 */

#if defined(WIN32)
typedef double 					SYS_FLOAT;

#define NO_LVALUE_CASTING
#define EXE_EXTENSION_CHAR	'.'

#define DIR_SEPARATOR '\\'
#define DIR_SEPARATOR_STR "\\"

#define UNICODE_BUFFER_SIZE EsMaxPath
#define OS_ENCODING_CODE_PAGE CP_UTF8
#define OS_ENCODING_MB_FLAGS 0
#define OS_ENCODING_WC_FLAGS 0

#define J9_PRIORITY_MAP {	\
	THREAD_PRIORITY_IDLE,							/* 0 */\
	THREAD_PRIORITY_LOWEST,					/* 1 */\
	THREAD_PRIORITY_BELOW_NORMAL,	/* 2 */\
	THREAD_PRIORITY_BELOW_NORMAL,	/* 3 */\
	THREAD_PRIORITY_BELOW_NORMAL,	/* 4 */\
	THREAD_PRIORITY_NORMAL,						/* 5 */\
	THREAD_PRIORITY_ABOVE_NORMAL,		/* 6 */\
	THREAD_PRIORITY_ABOVE_NORMAL,		/* 7 */\
	THREAD_PRIORITY_ABOVE_NORMAL,		/* 8 */\
	THREAD_PRIORITY_ABOVE_NORMAL,		/* 9 */\
	THREAD_PRIORITY_HIGHEST,					/*10 */\
	THREAD_PRIORITY_TIME_CRITICAL			/*11 */}

#if defined(__GNUC__)
#define VMINLINE_ALWAYS inline __attribute((always_inline))
/* If -O0 is in effect, define VMINLINE to be empty */
#if !defined(__OPTIMIZE__)
#define VMINLINE
#endif
#define HAS_BUILTIN_EXPECT
#else /* __GNUC__ */
/* Only for use on static functions */
#define VMINLINE_ALWAYS __forceinline
#endif /* __GNUC__ */
#endif /* WIN32 */

#if defined(J9ZOS390)
typedef double				SYS_FLOAT;

#define J9CONST64(x) x##LL
#define J9CONST_I64(x) x##LL
#define J9CONST_U64(x) x##ULL

#define NO_LVALUE_CASTING
#define PLATFORM_LINE_DELIMITER	"\012"
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_STR "/"

#define VA_PTR(valist) ((va_list*)&valist[0])

typedef struct {
#if !defined(OMR_ENV_DATA64)
	char stuff[16];
#endif
	char *ada;
	void *rawFnAddress;
} J9FunctionDescriptor_T;

#define TOC_UNWRAP_ADDRESS(wrappedPointer) (wrappedPointer ? ((J9FunctionDescriptor_T *)(uintptr_t)(wrappedPointer))->rawFnAddress : NULL)


#if defined(__cplusplus)

#if defined(__MVS__) && defined(__COMPILER_VER__)
#if (__COMPILER_VER__ >= 0x410D0000)
#define VMINLINE_ALWAYS inline __attribute__((always_inline))
#endif
#endif /* __MVS__ && __COMPILER_VER__ */

#ifndef VMINLINE_ALWAYS
#define VMINLINE_ALWAYS inline
#endif

#endif /* __cplusplus */

#endif /* J9ZOS390 */

#ifndef J9CONST64
#define J9CONST64(x) x##L
#endif

#ifndef J9CONST_I64
#define J9CONST_I64(x) x##LL
#endif

#ifndef J9CONST_U64
#define J9CONST_U64(x) x##ULL
#endif

/*
 * MIN and MAX data types
 */
#define U_8_MAX ((uint8_t)-1)
#define I_8_MIN ((int8_t)1 << ((sizeof(int8_t) * 8) - 1))
#define I_8_MAX ((int8_t)((uint8_t)I_8_MIN - 1))

#define U_16_MAX ((uint16_t)-1)
#define I_16_MIN ((int16_t)1 << ((sizeof(int16_t) * 8) - 1))
#define I_16_MAX ((int16_t)((uint16_t)I_16_MIN - 1))

#define U_32_MAX ((uint32_t)-1)
#define I_32_MIN ((int32_t)1 << ((sizeof(int32_t) * 8) - 1))
#define I_32_MAX ((int32_t)((uint32_t)I_32_MIN - 1))

#define U_64_MAX ((uint64_t)-1)
#define I_64_MIN ((int64_t)1 << ((sizeof(int64_t) * 8) - 1))
#define I_64_MAX ((int64_t)((uint64_t)I_64_MIN - 1))

#define UDATA_MAX ((uintptr_t)-1)
#define IDATA_MIN ((intptr_t)1 << ((sizeof(intptr_t) * 8) - 1))
#define IDATA_MAX ((intptr_t)((uintptr_t)IDATA_MIN - 1))

#ifndef J9_DEFAULT_SCHED
/* by default, pthreads platforms use the SCHED_OTHER thread scheduling policy */
#define J9_DEFAULT_SCHED SCHED_OTHER
#endif

#ifndef J9_PRIORITY_MAP
/* if no priority map if provided, priorities will be determined algorithmically */
#endif

#ifndef	FALSE
#define	FALSE		((BOOLEAN) 0)

#ifndef TRUE
#define	TRUE		((BOOLEAN) (!FALSE))
#endif
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL    (0)
#else
#define NULL    ((void *)0)
#endif
#endif

#define USE_PROTOTYPES
#ifdef	USE_PROTOTYPES
#define	PROTOTYPE(x)	x
#define	VARARGS		, ...
#else
#define	PROTOTYPE(x)	()
#define	VARARGS
#endif

/* Assign the default line delimiter if it was not set */
#ifndef PLATFORM_LINE_DELIMITER
#define PLATFORM_LINE_DELIMITER	"\015\012"
#endif

/* Set the max path length if it was not set */
#ifndef MAX_IMAGE_PATH_LENGTH
#define MAX_IMAGE_PATH_LENGTH	(2048)
#endif

typedef	double	ESDOUBLE;
typedef	float		ESSINGLE;

typedef struct U_128 {
#if defined(OMR_ENV_LITTLE_ENDIAN)
	uint64_t low64;
	uint64_t high64;
#else /* OMR_ENV_LITTLE_ENDIAN */
	uint64_t high64;
	uint64_t low64;
#endif /* OMR_ENV_LITTLE_ENDIAN */
} U_128;

/* helpers for U_64s */
#define CLEAR_U64(u64)  (u64 = (uint64_t)0)

#ifdef	OMR_ENV_LITTLE_ENDIAN
#define	LOW_LONG(l)	(*((uint32_t *) &(l)))
#define	HIGH_LONG(l)	(*(((uint32_t *) &(l)) + 1))
#else
#define	HIGH_LONG(l)	(*((uint32_t *) &(l)))
#define	LOW_LONG(l)	(*(((uint32_t *) &(l)) + 1))
#endif

#define	I8(x)			((int8_t) (x))
#define	I8P(x)			((int8_t *) (x))
#define	U16(x)			((uint16_t) (x))
#define	I16(x)			((int16_t) (x))
#define	I16P(x)			((int16_t *) (x))
#define	U32(x)			((uint32_t) (x))
#define	I32(x)			((int32_t) (x))
#define	I32P(x)			((int32_t *) (x))
#define	U16P(x)			((uint16_t *) (x))
#define	U32P(x)			((uint32_t *) (x))
#define	BYTEP(x)		((BYTE *) (x))

/* Test - was conflicting with OS2.h */
#define	ESCHAR(x)		((CHARACTER) (x))
#define	FLT(x)			((FLOAT) x)
#define	FLTP(x)			((FLOAT *) (x))

#ifdef	NO_LVALUE_CASTING
#define	LI8(x)			(*((int8_t *) &(x)))
#define	LI8P(x)			(*((int8_t **) &(x)))
#define	LU16(x)			(*((uint16_t *) &(x)))
#define	LI16(x)			(*((int16_t *) &(x)))
#define	LU32(x)			(*((uint32_t *) &(x)))
#define	LI32(x)			(*((int32_t *) &(x)))
#define	LI32P(x)		(*((int32_t **) &(x)))
#define	LU16P(x)		(*((uint16_t **) &(x)))
#define	LU32P(x)		(*((uint32_t **) &(x)))
#define	LBYTEP(x)		(*((BYTE **) &(x)))
#define	LCHAR(x)		(*((CHARACTER) &(x)))
#define	LFLT(x)			(*((FLOAT) &x))
#define	LFLTP(x)		(*((FLOAT *) &(x)))
#else
#define	LI8(x)			I8((x))
#define	LI8P(x)			I8P((x))
#define	LU16(x)			U16((x))
#define	LI16(x)			I16((x))
#define	LU32(x)			U32((x))
#define	LI32(x)			I32((x))
#define	LI32P(x)		I32P((x))
#define	LU16P(x)		U16P((x))
#define	LU32P(x)		U32P((x))
#define	LBYTEP(x)		BYTEP((x))
#define	LCHAR(x)		CHAR((x))
#define	LFLT(x)			FLT((x))
#define	LFLTP(x)		FLTP((x))
#endif

/* Macros for converting between words and longs and accessing bits */

#define	HIGH_WORD(x)	U16(U32((x)) >> 16)
#define	LOW_WORD(x)		U16(U32((x)) & 0xFFFF)
#define	MAKE_32(h, l)	((U32((h)) << 16) | U32((l)))
#define	MAKE_64(h, l)	((((int64_t)(h)) << 32) | (l))

#ifdef __cplusplus
#define J9_CFUNC "C"
#define J9_CDATA "C"
#else
#define J9_CFUNC
#define J9_CDATA
#endif

/* Macros for tagging functions which read/write the vm thread */

#define READSVMTHREAD
#define WRITESVMTHREAD
#define REQUIRESSTACKFRAME

/* macro for tagging functions which never return */
#ifdef __GNUC__
/* on GCC, we can actually pass this information on to the compiler */
#define OMRNORETURN __attribute__((noreturn))
#else
#define OMRNORETURN
#endif

/* on some systems (e.g. LinuxPPC) va_list is an array type.  This is probably in
 * violation of the ANSI C spec, but it's not entirely clear.  Because of this, we end
 * up with an undesired extra level of indirection if we take the address of a
 * va_list argument.
 *
 * To get it right ,always use the VA_PTR macro
 */
#ifndef VA_PTR
#define VA_PTR(valist) (&valist)
#endif

/* Macros used on RS6000 to manipulate wrapped function pointers */
#ifndef TOC_UNWRAP_ADDRESS
#define TOC_UNWRAP_ADDRESS(wrappedPointer) ((void *) (wrappedPointer))
#endif
#ifndef TOC_STORE_TOC
#define TOC_STORE_TOC(dest,wrappedPointer)
#endif

/* Macros for accessing int64_t values */
#ifdef ATOMIC_LONG_ACCESS
#define PTR_LONG_STORE(dstPtr, aLongPtr) ((*U32P(dstPtr) = *U32P(aLongPtr)), (*(U32P(dstPtr)+1) = *(U32P(aLongPtr)+1)))
#define PTR_LONG_VALUE(dstPtr, aLongPtr) ((*U32P(aLongPtr) = *U32P(dstPtr)), (*(U32P(aLongPtr)+1) = *(U32P(dstPtr)+1)))
#else
#define PTR_LONG_STORE(dstPtr, aLongPtr) (*(dstPtr) = *(aLongPtr))
#define PTR_LONG_VALUE(dstPtr, aLongPtr) (*(aLongPtr) = *(dstPtr))
#endif

/* Macro used when declaring tables which require relocations. */
#ifndef J9CONST_TABLE
#define J9CONST_TABLE const
#endif

/* ANSI qsort is not always available */
#ifndef J9_SORT
#define J9_SORT(base, nmemb, size, compare) qsort((base), (nmemb), (size), (compare))
#endif

#if !defined(VMINLINE_ALWAYS)
#define VMINLINE_ALWAYS
#endif
#if !defined(VMINLINE)
#define VMINLINE VMINLINE_ALWAYS
#endif
#if defined(DEBUG)
#undef VMINLINE
#define VMINLINE
#endif

/* DDR cannot parse __builtin_expect */
#if defined(TYPESTUBS_H)
#undef HAS_BUILTIN_EXPECT
#endif

#if defined(HAS_BUILTIN_EXPECT)
#undef HAS_BUILTIN_EXPECT
#define J9_EXPECTED(e)		__builtin_expect(!!(e), 1)
#define J9_UNEXPECTED(e)	__builtin_expect(!!(e), 0)
#else
#define J9_EXPECTED(e) (e)
#define J9_UNEXPECTED(e) (e)
#endif

#define OMR_MAX(a,b) (((a) > (b)) ? (a) : (b))
#define OMR_MIN(a,b) (((a) < (b)) ? (a) : (b))

/* Provide macros which can be used for bit testing */
#define J9_ARE_ANY_BITS_SET(value, bits) (0 != ((value) & (bits)))
#define J9_ARE_ALL_BITS_SET(value, bits) ((bits) == ((value) & (bits)))
#define J9_ARE_NO_BITS_SET(value, bits) (!J9_ARE_ANY_BITS_SET(value, bits))

/* Workaround for gcc -Wunused-result, which was added in 4.5.4 */
#if defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5)))
#define J9_IGNORE_RETURNVAL(funcCall) \
	do { \
		UDATA _ignoredRc = (UDATA)(funcCall); \
		_ignoredRc = _ignoredRc; \
	} while (0)
#else /* defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5))) */
#define J9_IGNORE_RETURNVAL(funcCall) (funcCall)
#endif /* defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5))) */

#define J9_STR_(x) #x
#define J9_STR(x) J9_STR_(x)
#define OMR_GET_CALLSITE() __FILE__ ":" J9_STR(__LINE__)

/* Legacy defines - remove once code cleanup is complete */
#define J9VM_ENV_DIRECT_FUNCTION_POINTERS
#define J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING

#endif /* OMRCOMP_H */
