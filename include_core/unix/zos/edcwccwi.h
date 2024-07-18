/*******************************************************************************
 * Copyright IBM Corp. and others 1988
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
 * @ddr_namespace: default
 */

                   #ifndef __edcwccwi
                   #ifdef __COMPILER_VER__
                     #pragma filetag("IBM-1047")
                   #endif
                   #define __edcwccwi 1
                   #pragma nomargins nosequence
                   #pragma checkout(suspend)

  #if defined(__IBM_METAL__)

    #error Language Environment standard C headers \
cannot be used when METAL option is used. \
Correct your header search path.

  #endif /* __IBM_METAL__ */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __EDC_LE
  #define __EDC_LE 0x10000000
#endif


#if __TARGET_LIB__ >= __EDC_LE

#pragma map (__dsa_prev, "\174\174DSAPRV")
#pragma map (__ep_find, "\174\174EPFIND")
#pragma map (__fnwsa, "\174\174FNWSA")
#if !defined(_LP64)
#pragma map (__stack_info, "\174\174STACK\174")
#endif

#if defined(__XPLINK__) && !defined(_LP64)
#pragma map (__bldxfd, "\174\174BLDXFD")
#endif

#ifdef _LP64
  #pragma map (__set_laa_for_jit, "\174\174SETJIT")
  #define __bldxfd(fd) fd
#endif

#if !defined(__features_h) || defined(__inc_features)
  #include <features.h>
#endif

#ifndef __types
  #include <sys/types.h>
#endif

#if __EDC_TARGET >= 0x41080000
#pragma map (__static_reinit, "\174\174STATRE")
#define __STATIC_REINIT_FULL 1
#endif


/*
 *   =============================================================
 *   Function definitions for Language Environment C-Language CWIs
 *   =============================================================
 */

#ifdef _NO_PROTO

void * __dsa_prev();
void * __ep_find();
void * __fnwsa();
#if defined(__XPLINK__) && !defined(_LP64)
void * __bldxfd();
#endif

#else


/*
 *  __dsa_prev()  -- Do logical or physical DSA backchaining
 *  ============  ------------------------------------------
 *
 *  note: This routine may ABEND.  Caller may need to use CEE3SRP,
 *        CEEMRCE, etc. to handle ABENDs in this routine.
 *        (or use signal catcher, sigsetjmp(), and siglongjmp())
 */

void *                           /* pointer to prior DSA             */
__dsa_prev
(
  const void *                   /* pointer to current DSA           */

, int                            /* backchaining request type:
                                    1 = physical
                                    2 = logical                      */

, int                            /* starting DSA format:
                                    0 = up
                                    1 = down                         */

, void *                         /* called function returns a pointer
                                    to the fetched data (perhaps
                                    moved into this A/S)
                                    note: if callback function cannot
                                    return the requested data, it must
                                    not return -- i.e. it should
                                    longjump back to the user program
                                    that called __dsa_prev()         */

     (*)                         /* optional ptr to user-provided
                                    callback function that fetches
                                    data, possibly from other
                                    address space -- NULL = fetch
                                    data from current A/S            */
     (
       void *                    /* 1st input parm for callback fcn --
                                    starting address of data to be
                                    fetched (perhaps from other A/S) */
     , size_t                    /* 2nd input parm or callback fcn --
                                    length of data to be fetched
                                    (will not exceed 16 bytes)       */
     )

, const void *                   /* pointer to LE CAA -- required if
                                    4th argument is non-NULL.
                                    Note: this may be the address of
                                    CAA in the other A/S             */

, int  *                         /* optional ptr to output field
                                    where previous DSA stack format
                                    will be returned -- NULL = don't
                                    pass back stack format           */
, void **                        /* optional pointer to output field
                                    where address of physical callee
                                    DSA will be placed -- NULL =
                                    don't pass back physical callee
                                    address                          */
, int  *                         /* optional ptr to output field
                                    where physical callee stack format
                                    will be returned -- NULL = don't
                                    pass back stack format           */
);




/*
 *  __ep_find() -- find entry point of owner of passed-in DSA
 *  ===========    ------------------------------------------
 */

void *                           /* pointer to entry point -- NULL =
                                    could not find                   */
__ep_find
(
  const void *                   /* pointer to DSA                   */

, int                            /* DSA format:
                                    0 = up
                                    1 = down                         */

, void *                         /* called function returns a pointer
                                    to the fetched data (perhaps
                                    moved into this A/S)
                                    note: if callback function cannot
                                    return the requested data, it must
                                    not return -- i.e. it should
                                    longjump back to the user program
                                    that called __dsa_prev()         */

     (*)                         /* optional ptr to user-provided
                                    callback function that fetches
                                    data, possibly from other
                                    address space -- NULL = fetch
                                    data from current A/S            */
     (
       void *                    /* 1st input parm for callback fcn --
                                    starting address of data to be
                                    fetched (perhaps from other A/S) */
     , size_t                    /* 2nd input parm or callback fcn --
                                    length of data to be fetched
                                    (will not exceed 16 bytes)       */
     )

);




/*
 *  __fnwsa() -- find the WSA associated with an executable module
 *  =========    containing the specified entry point
 *
 */

void *                           /* address of WSA (or NULL if
                                    module doesn't have a WSA) --
                                    -1 == executable module could
                                          not be found from ep       */
__fnwsa
(
  const void *                   /* pointer to entry point           */

, void *                         /* called function returns a pointer
                                    to the fetched data (perhaps
                                    moved into this A/S)
                                    note: if callback function cannot
                                    return the requested data, it must
                                    not return -- i.e. it should
                                    longjump back to the user program
                                    that called __fnwsa()            */

     (*)                         /* optional ptr to user-provided
                                    callback function that fetches
                                    data, possibly from other
                                    address space -- NULL = fetch
                                    data from current A/S            */
     (
       void *                    /* 1st input parm for callback fcn --
                                    starting address of data to be
                                    fetched (perhaps from other A/S) */
     , size_t                    /* 2nd input parm or callback fcn --
                                    length of data to be fetched
                                    (will not exceed 1K bytes)       */
     )

, const void *                   /* pointer to LE CAA -- required if
                                    2nd argument is non-NULL.
                                    Note: this may be the address of
                                    CAA in the other A/S             */

);


#if defined(__XPLINK__) && !defined(_LP64)
/*
 *  __bldxfd() -- build an XPLINK Compatibility Descriptor out of a
 *  =========     "function pointer" of unknown origin
 *
 */

char *                           /* address of storage contain
                                    XPLINK Compatibility Descriptor  */
__bldxfd
(
  void *                         /* pointer to entry point           */

);
#endif /* __XPLINK__ && !LP64 */

typedef
struct __jumpinfo_vr_ext
{
	short __ji_ve_version;
	char  __ji_ve_u[14];
	U_128 __ji_ve_savearea[32];
} __jumpinfo_vr_ext_t_;

#if ((!defined(_LP64) && defined(__XPLINK__))   ||         \
     (defined(_LP64)  && (__EDC_TARGET >= __EDC_LE4107)))
typedef
struct __jumpinfo
{
                          /*Note - offsets are for 31-bit expansion*/
  char         __ji_u1[68];              /* +0   Reserved            */
  char         __ji_mask_saved;          /* +44  when non-zero
                                                indicates signal mask
                                                saved in __ji_sigmask*/
  char         __ji_u2[3];               /* +45  Reserved            */
  #ifdef __DOT1
  sigset_t  __ji_sigmask;                /* +48  signal mask         */
  #else
  unsigned int   __ji_u2a;               /* +48  Filler for non-posix*/
  unsigned int   __ji_u2b;               /* +4C  Filler for non-posix*/
  #endif
  char         __ji_u3[11];              /* +50  Reserved            */
  unsigned     __ji_fl_fp4           :1; /* +5B.0 4 FPRs valid       */
  unsigned     __ji_fl_fp16          :1; /* +5B.1 16 FPRs valid      */
  unsigned     __ji_fl_fpc           :1; /* +5B.2 FPC  valid         */
  unsigned     __ji_fl_res1a         :1; /* +5B.3 reserved           */
#ifndef _LP64
  unsigned     __ji_fl_hr            :1; /* +5B.4 Hi regs valid      */
#else
  unsigned     __ji_fl_res3          :1; /* +5B.4 Reserved           */
#endif
  unsigned     __ji_fl_res2          :1; /* +5B.5 Reserved           */
  unsigned     __ji_fl_exp           :1; /* +5B.6 explicit backchain */
  unsigned     __ji_fl_res2a         :1; /* +5B.7 Reserved           */
  char         __ji_u4[12];              /* +5C   Reserved       @D4C*/
  __jumpinfo_vr_ext_t_ *__ji_vr_ext;      /* +68   Pointer to VRs
                                                  save area      @D4A*/
#ifndef _LP64                            /*                      @D4A*/
  char         __ji_u7[4];               /* +6C   Reserved       @D4A*/
#endif                                   /*                      @D4A*/
  char         __ji_u8[16];              /* +70   Reserved       @D4A*/
  long         __ji_gr[16];              /* +80  resume registers    */
#ifndef _LP64
  long         __ji_hr[16];              /* +C0  hi regs             */
#endif
  int          __ji_u5[16];              /* +100 Reserved            */
  double       __ji_fpr[16];             /* +140 FP registers 0-15   */
  int          __ji_fpc;                 /* +1C0 Floating pt cntl reg*/
  char         __ji_u6[60];              /* +1C4 Reserved            */
                                          /* +200 End of Resume area */
} __jumpinfo_t;


/**********************************************************************
 *  __far_jump()  -- perform far jump                                 *
 **********************************************************************
 */
void
__far_jump
(
  struct __jumpinfo *            /* pointer to jump info buffer      */
);

#endif


#if !defined(_LP64)
#if defined(__XPLINK__)

/*******************************************************************
 *  __set_stack_softlimit() -- Set stack softlimit                 *
 *******************************************************************
 */

unsigned long                    /* returns the previous value of the
                                    softlimit (ULONG_MAX returned if
                                    first call)                      */
__set_stack_softlimit
(
  unsigned long                  /* soft limit stack size in bytes   */

);

#endif /* __XPLINK__  */
#else
  #define __set_stack_softlimit(a) \
    struct __no64bitSupport __set_stack_softlimit
#endif /* ! _LP64 */

#ifdef _LP64

typedef
struct __laa_jit_s {
 void *   user_data1;
 void *   user_data2;
} __laa_jit_t;
/**********************************************************************
 *  __set_laa_for_jit()  -- set 2 reserved fields in the LAA for JAVA *
 **********************************************************************
 */
int __set_laa_for_jit ( __laa_jit_t *laa_jit);

#endif /* _LP64 */

#if defined(__XPLINK__)

typedef
struct  __mcontext
{
                          /*Note - offsets are for 31-bit expansion*/
 char        __mc__1[68];              /* +0   Reserved            */
 char        __mc_mask_saved;          /* +44  when non-zero
                                              indicates signal mask
                                              saved in __mc_sigmask*/
 char        __mc__2[2];               /* +45  Reserved            */
 unsigned    __mc__3               :3; /* +47  Reserved            */
 unsigned    __mc_psw_flag         :1; /* +47.3 __mc_psw PSW valid */
#if __EDC_TARGET >= __EDC_LE4107 /* was '__EDC_LE410A' Modified for jit_freeSystemStackPointer */
 unsigned    __mc_int_dsa_flag     :1; /* +47.4 __mc_int_dsa
                                                is valid       @D2A*/
 unsigned    __mc_savstack_flag    :1; /* +47.5 __mc_int_dsa was
                                                saved from (must be
                                                restored to)
                                                CEECAA_SAVSTACK
                                                (31-bit) or
                                                CEELCA_SAVSTACK
                                                (64-bit)       @D2A*/
 unsigned    __mc_savstack_async_flag:1;/*+47.6 __mc_int_dsa was
                                                saved from (must be
                                                restored to) the
                                                field pointed to by
                                                CEECAA_SAVSTACK_ASYNC
                                                (31-bit) or
                                                CEELCA_SAVSTACK_ASYNC
                                                (64-bit)       @D2A*/
 unsigned    __mc__4               :1; /* +47.7 Reserved       @D2A*/
#else
 unsigned    __mc__4               :4; /* +47 Reserved             */
#endif  /* __EDC_TARGET >= __EDC_LE410A */
 #ifdef __DOT1
 sigset_t    __mc_sigmask;             /* +48 signal mask for posix*/
 #else
 unsigned int __mc__4a;                /* +48  Filler for non-posix*/
 unsigned int __mc__4b;                /* +4C  Filler for non-posix*/
 #endif
 char        __mc__5[11];              /* +50  Reserved            */
 unsigned    __mc_fl_fp4           :1; /* +5B.0 4 FPRs valid       */
 unsigned    __mc_fl_fp16          :1; /* +5B.1 16 FPRs valid      */
 unsigned    __mc_fl_fpc           :1; /* +5B.2 FPC valid          */
 unsigned    __mc_fl_res1a         :1; /* +5B.3 Reserved           */
 unsigned    __mc_fl_hr            :1; /* +5B.4 Hi regs valid      */
 unsigned    __mc_fl_res2          :1; /* +5B.5 Reserved           */
 unsigned    __mc_fl_exp           :1; /* +5B.6 Explicit backchain */
 unsigned    __mc_fl_res2a         :1; /* +5B.7 Reserved           */
/*#@                                                       BEGIN @D5A*/
 unsigned    __mc_fl_res2b         :2; /* +5C.0 Reserved             */
 unsigned    __mc_fl_vr            :1; /* +5C.2 Vector savearea valid*/
 unsigned    __mc_fl_res2c         :5; /* +5C.x Reserved             */
 char        __mc__6a[11];             /* +5D Reserved               */
 void       *__mc_vr_ext;              /* +68 ptr to vrs save area   */
 __pad31    (__mc__6b,4)               /* +6C 31-bit reserved        */
 char        __mc__6c[16];             /* +70 Reserved               */
/*#@                                                         END @D5A*/
 long        __mc_gr[16];              /* +80 resume register      */
#ifndef _LP64
 long        __mc_hr_[16];             /* +C0 hi regs              */
#endif
 int         __mc__7[16];              /* +100 Reserved            */
 double      __mc_fpr[16];             /* +140 FP registers 0-15   */
 int         __mc_fpc;                 /* +1C0 Floating pt cntl reg*/
 char        __mc__8[60];              /* +1C4 Reserved            */
 __pad31         (__mc__9,16)         /*      reserved  31-bit     */
 __pad64         (__mc__9,8)          /*      reserved  64-bit     */
 struct __mcontext  *__mc_aux_mcontext_ptr;/* +210 ptr __mcontext  */
 __pad31         (__mc__10,12)        /* +214 reserved  31-bit     */
 __pad64         (__mc__10,24)        /* +218 reserved  64-bit     */
 __pad31         (__mc_psw,8 )        /* 8-byte PSW for 31-bit     */
 __pad64         (__mc_psw,16)        /* 16-byte PSW for 64-bit    */
 /* Note- __mc_psw is valid only if __mc_psw_flag is set           */
#if __EDC_TARGET >= __EDC_LE410A
 void       *__mc_int_dsa;             /* +228 interrupt DSA   @D2A*/
 /* Note- __mc_int_dsa is only valid if __mc_int_dsa_flag is set   */
 __pad31         (__mc__11,84)         /* reserved  31-bit     @D2A*/
 __pad64         (__mc__11,104)        /* reserved  64-bit     @D2A*/
#else
 __pad31         (__mc__11,88)         /* reserved  31-bit         */
 __pad64         (__mc__11,112)        /* reserved  64-bit         */
#endif  /* __EDC_TARGET >= __EDC_LE410A */
                                       /* +280 End of Resume area  */
} __mcontext_t_;

#endif /* __XPLINK__ */

__new4108(int,__static_reinit,(int,void *));

#endif /* _NO_PROTO */


/*
 *  Constants for __dsa_prev() and __ep_find() and __stack_info()
 *  -------------------------------------------------------------
 */

#define __EDCWCCWI_PHYSICAL 1   /* do physical backchain             */
#define __EDCWCCWI_LOGICAL  2   /* do logical backchain              */

#define __EDCWCCWI_UP       0   /* stack format = UP                 */
#define __EDCWCCWI_DOWN     1   /* stack format = DOWN               */

/******************************************************************/
/* define's for VHM events and structure required as input to the */
/* VHM event calls                                                */
/******************************************************************/
#if __EDC_TARGET >= __EDC_LE4103


#ifdef __cplusplus
   #ifndef _LP64
       #ifdef __XPLINK__
           extern "OS_UPSTACK" void __cee_heap_manager(int, void *);
       #else
           extern "OS" void __cee_heap_manager(int, void *);
       #endif
   #endif
#else
   void  __cee_heap_manager(int ,void *);
   #ifndef _LP64
       #ifdef __XPLINK__
           #pragma linkage(__cee_heap_manager, OS_UPSTACK)
       #else
           #pragma linkage(__cee_heap_manager, OS)
       #endif
   #endif
#endif

#define _VHM_INIT 1
#define _VHM_TERM 2
#define _VHM_CEEDUMP 3
#define _VHM_REPORT 4

/****** struct __event1_s ******/

struct __event1_s {
   void * __ev1_free;
   void * __ev1_malloc;
   void * __ev1_realloc;
   void * __ev1_calloc;
   void * __ev1_xp_free;
   void * __ev1_xp_malloc;
   void * __ev1_xp_realloc;
   void * __ev1_xp_calloc;
   unsigned int __ev1_le_xplink : 1,
                __ev1_le_reserved : 31;
   unsigned int __ev1_vhm_xplink : 1,
                __ev1_vhm_reserved : 31;
};

#if __EDC_TARGET >= __EDC_LE4106

   /*****************************************************************/
   /*                __vhm_event() PROTOTYPE                        */
   /*****************************************************************/
   #pragma map (__vhm_event, "\174\174VHMEVT")
   /*****************************************************************/
   /* NOTE !!! At this moment this API is just supporting to drive  */
   /*       the _VHM_REPORT as the event argument with VHM_CLEAR as */
   /*       the optional argument in MEMCHECK VHM. It could support */
   /*       more events and more event options in the future.       */
   /*****************************************************************/
   void __vhm_event(int, ...);

   /*****************************************************************/
   /*  __vhm_event() EVENT OPTIONS. Used in the optional argument.  */
   /*****************************************************************/
   #define _VHM_REPORT_CLEAR 0x01 /* Used by _VHM_REPORT event to   */
                                  /* free the resources previously  */
                                  /* logged by MEMCHECK.            */

#endif  /* __EDC_TARGET >= __EDC_LE4106 */

#endif  /* __EDC_TARGET >= __EDC_LE4103 */

  #if !defined(_LP64)
  #ifdef _OPEN_THREADS
  #include <pthread.h>

  typedef struct __segaddrs{
     void * __startaddr;
     void * __endaddr;
     int    __segtype;
     int    padding;
  }SegmentAddresses;

  typedef struct __stackinfo {
     int      __structsize;
     int      __numsegs;
     void *   __stacktop;
     void *   resrv;
     SegmentAddresses __segaddr[1];
  }STACKINFO;

  __new4102(int, __stack_info, (STACKINFO *, struct __thdq_ *));

  #endif
  #else
    #define __stack_info(a,b) \
      struct __no64bitSupport __stack_info
  #endif


#if __EDC_TARGET >= __EDC_LE4107 /* was '__EDC_LE410A' Modified for jit_freeSystemStackPointer */

  #ifndef _LP64
    #define __LE_SAVSTACK_ADDR ((void**)__gtca()+250)
    #define __LE_SAVSTACK_ASYNC_ADDR ((void**)__gtca()+253)
  #else
    #ifndef __LAA_P
        #define __LAA_P ((void *)*((void * __ptr32 *)0x04B8ul))
    #endif
    #define __LE_SAVSTACK_ADDR \
      (((void**)(*(((void**)(__LAA_P))+11)))+4)
    #define __LE_SAVSTACK_ASYNC_ADDR \
      (((void**)(*(((void**)(__LAA_P))+11)))+42)
  #endif

#endif  /* __EDC_TARGET >= __EDC_LE410A */

#endif  /* __TARGET_LIB__ >= __EDC_LE */


#ifdef __cplusplus
}
#endif

                   #pragma checkout(resume)
                   #endif /* __edcwccwi */
