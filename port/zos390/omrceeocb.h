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
 * The following struct maps the first part of the CEEOCB and then
 * one entry of the fixed portion.  The fixed portion is identical
 * for each runtime option.  For runtime options with suboptions,
 * use the address of the CEEOCB plus the ceeocb_opt_suopts_offset
 * for the desired option to obtain the address of the suboption
 * data.  Then use the option specific suboption structure to look
 * at the suboption information.
 */
struct ceeocb {                                  
  char                 ceeocb_eyecatcher[8];     
  short                ceeocb_version_release;   
  short                ceeocb_length;            
  unsigned int         ceeocb_ev_ptr;            
  unsigned int         ceeocb_format     :8,     
                       ___011___         :24;    
  struct {                                       
    unsigned int       ceeocb_opt_on         :1, 
                       ceeocb_opt_nooverride :1, 
                       ceeocb_opt_rsvd1      :5, 
                       ceeocb_opt_on_v       :1; 
    char               ceeocb_opt_rsvd2;         
    short              ceeocb_opt_where_set;     
    unsigned int       ceeocb_opt_subopts_offset;
  } ceeocb_opt[1];                               
};

/*
 * This enum is provided for easy access to the fixed portion of
 * the CEEOCB for each runtime option.
 */

enum ceeocb_options {
  __rsvd1,
  __aixbld,
  __all31,
  __belowheap,
  __check,
  __plitaskcount,
  __abtermenc,
  __country,
  __debug,
  __errcount,
  __filehist,
  __envar,
  __flowc,
  __heap,
  __inqpcopn,
  __interrupt,
  __libstack,
  __msgq,
#if 0
  /* __msgfile is already defined in stdio.h
   * we don't use __msgfile so replace it
   * with a dummy  */
   __msgfile,
#else
  __j9dummy,
#endif
  __natlang,
  __errunit,
  __ocstatus,
  __posix,
  __rptstg,
  __rtereus,
  __simvrd,
  __stack,
  __storage,
  __autotask,
  __trace,
  __threadheap,
  __test,
  __threadstack,
  __trap,
  __upsi,
  __vctrsave,
  __prtunit,
  __xuflow,
  __cblopts,
  __noniptstack,
  __rptopts,
  __anyheap,
  __abperc,
  __termthdact,
  __depthcondlmt,
  __cblpshpop,
  __cblqda,
  __pununit,
  __rdrunit,
  __recpad,
  __usrhdlr,
  __namelist,
  __pc,
  __library,
  __version,
  __rtls,
  __heapchk,
  __profile,
  __heappools,
  __infomsgfilter,
  __xplink,
  __filetag,
  __heap64,
  __heappools64,
  __ioheap64,
  __libheap64,
  __stack64,
  __threadstack64,
  __dyndump,
  __ceedump
};

/*
 * TRAP SUBOPTIONS
 */

struct ceeocb_trap_sub_options {
  unsigned int         ceeocb_trap_spie_v : 1,
                       ___000_1___        : 31;
  unsigned int         ceeocb_trap_spie   : 1,  /*1=SPIE             */
                                                /*0=NOSPIE           */
                       ___004_1__         : 31;
};

/** CEEEDB **/

#if defined(_LP64)
struct ceeedb {
  char                 ceeedbeye[8];
  char                 ___008___[248];
  unsigned int         ceeedbmaini          : 1,
                       ___100_1___          : 1,
                       ceeedbactiv          : 1,
                       ceeedbtip            : 1,
                       ___100_4___          : 1,
                       ceeedb_posix         : 1,
                       ceeedbmultithread    : 1,
                       ceeedb_omvs_dubbed   : 1,
                       ceeedbipm            : 8,
                       ceeedb_creator_id    : 8,
                       ___103___            : 8;
  char                 ___104___[4];
  void                *ceeedbmembr;
  struct ceeocb       *ceeedboptcb;
};
#else
struct ceeedb {
  char                 ceeedbeye[8];
  unsigned int         ceeedbmaini          : 1,
                       ceeedb_initial_amode : 1,
                       ceeedbactiv          : 1,
                       ceeedbtip            : 1,
                       ceeedbpici           : 1,
                       ceeedb_posix         : 1,
                       ceeedbmultithread    : 1,
                       ceeedb_omvs_dubbed   : 1,
                       ceeedbipm            : 8,
                       ceeedbpm             : 8,
                       ceeedb_creator_id    : 8;
  void                *ceeedbmembr;
  struct ceeocb       *ceeedboptcb;
};
#endif

/** CEECAA **/

#if defined(_LP64)
struct ceecaa {
  char                 ___000___[0x388];
  struct ceeedb       *ceecaaedb;
};
#else
struct ceecaa {
  char                 ___000___[0x2F0];
  struct ceeedb       *ceecaaedb;
};
#endif

/** ACCESS TO R12 WHICH CONTAINS CEECAA ADDRESS **/

#ifndef __gtca
  #define __gtca() _gtca()
  #ifdef __cplusplus
    extern "builtin"
  #else
    #pragma linkage(_gtca,builtin)
  #endif
  const void *_gtca(void);
#endif

/** POINTER TO CEECAA **/

#ifndef ceecaa
  #define ceecaa() ( (struct ceecaa *)__gtca() )
#endif

/** POINTER TO CEEEDB **/

#ifndef ceeedb
  #define ceeedb() ( (struct ceeedb *)(ceecaa()->ceecaaedb) )
#endif

/** POINTER TO CEEOCB **/

#ifndef ceeocb
  #define ceeocb() ( (struct ceeocb *)(ceeedb()->ceeedboptcb) )
#endif
