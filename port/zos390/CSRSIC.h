#ifndef __CSRSI
#define __CSRSI

/*********************************************************************
 * This header file was copied from SYS1.CSSLIB. It provides the
 * the C stubs and data structures for the CSRSI service. For 
 * details, see the "z/OS MVS Programming: Authorized Assembler 
 * Services Reference ALE-DYN"
 *
 * Update: Apr 2015
 * The original file can be found in 'SYS1.SAMPLIB(CSRSIC)'. This 
 * copy has been updated using stdint types so that we can safely
 * compile for both 31 and 64-bits without ending up with different
 * struct sizes.
 ********************************************************************/

#include <stdint.h>

/*********************************************************************
 *                                                                   *
 *  Name: CSRSIC                                                     *
 *                                                                   *
 *  Descriptive Name: Store System Information C declares            *
 *                                                                   */
 /*01* PROPRIETARY STATEMENT=                                        */
 /***PROPRIETARY_STATEMENT********************************************/
 /*                                                                  */
 /*                                                                  */
 /* LICENSED MATERIALS - PROPERTY OF IBM                             */
 /* 5694-A01 COPYRIGHT IBM CORP. 1999,2010                           */
 /*                                                                  */
 /* STATUS= HBB7770                                                  */
 /*                                                                  */
 /***END_OF_PROPRIETARY_STATEMENT*************************************/
 /*                                                                  */
 /*01* EXTERNAL CLASSIFICATION: PI                                   */
 /*01* END OF EXTERNAL CLASSIFICATION:                               */
 /*                                                                  */
/*  Function:                                                        *
 *    CSRSIC defines types, related constants, and function          *
 *    prototypes for the use of the CSRSI service                    *
 *    from the C language                                            *
 *                                                                   *
 *  Usage:                                                           *
 *    #include <CSRSIC.H>                                            *
 *                                                                   *
 *  Notes:                                                           *
 *   1. This member should be copied from SAMPLIB to the             *
 *      appropriate local C library.                                 *
 *                                                                   *
 *   2. CSRSI service does not use a null                            *
 *      character to terminate strings. The services expect the      *
 *      character operands to be a fixed-length type.                *
 *      Use memcpy to move into and from these fields.               *
 *                                                                   *
 *  Change Activity:                                                 *
 *$00=STSICSR,HBB6601, 990206, PDXB: OW38489 STSI                    *
 *$H1=STSICSR,HBB6601, 990206, PDXB: OW38489 STSI                    *
 *$L1=STSI   ,HBB7707, 011201, PDXB: si22v1alt                       *
 *$L2=GT16WAY,HBB7709, 021211, PDXB: si00PCCA_CPU_Address_Mask       *
 *$H2=STSICSR,HBB7709, 031105, PDXB: Model Capacity Identifier       *
 *$H3=IFA     HBB7709  031205  PDXB: IFA support                     *
 *$L3=ME05086 HBB7730  051115  PDXB: LPAR origin                     *
 *$01=OA21459 HBB7720  070614, PD00KD: Cleanup sequence numbers      *
 *$L4=ME18454 HBB7770  100210, PD00XB: Improve access to CVT         *
 *                                                                   *
 *********************************************************************/
/*********************************************************************
 *         Type Definitions for User Specified Parameters            *
 *********************************************************************/

/*  Type for Request operand of CSRSI                                */
typedef int32_t  CSRSIRequest;

/*  Type for InfoAreaLen operand of CSRSI                            */
typedef int32_t  CSRSIInfoAreaLen;

/*  Type for Return Code                                             */
typedef int32_t  CSRSIReturnCode;



/*********************************************************************
 *           Function Prototypes for Service Routines                *
 *********************************************************************/

#ifdef __cplusplus
   extern "OS" ??<
#else
  #pragma linkage(CSRSI_calltype,OS)
#endif
typedef void CSRSI_calltype(
   CSRSIRequest      __REQUEST,   /* Input  - request type           */
   CSRSIInfoAreaLen  __INFOAREALEN,  /* Input - length of infoarea   */
   void             *__INFOAREA,  /* Input  - info area              */
   CSRSIReturnCode  *__RC);       /* Output - return code            */

#define csrsi CSRSI
extern CSRSI_calltype CSRSI;


#ifdef __cplusplus
   ??>
#endif

#ifndef __cplusplus
#define csrsi_byaddr(Request, Flen, Fptr, Rcptr)                    \
??<                                                                 \
 ((struct CSRSI_PSA*) 0) ->                                         \
                   CSRSI_cvt->CSRSI_cvtcsrt->CSRSI_addr             \
           (Request,Flen,Fptr,Rcptr);                               \
??>;
#endif

struct CSRSI_CSRT ??<
   unsigned char CSRSI_csrt_filler1  ??(48??);
   CSRSI_calltype* CSRSI_addr;
??>;

struct CSRSI_CVT ??<
   unsigned char CSRSI_cvt_filler1  ??(116??);
  struct ??<
    int32_t CSRSI_cvtdcb_rsvd1 : 4;      /* Not needed                   */
    int32_t CSRSI_cvtosext : 1;          /* If on, indicates that the
                    CVTOSLVL fields are valid                        */
    int32_t CSRSI_cvtdcb_rsvd2 : 3;      /* Not needed                   */
         ??> CSRSI_cvtdcb;
   unsigned char CSRSI_cvt_filler2  ??(427??);
   struct CSRSI_CSRT * CSRSI_cvtcsrt;
   unsigned char CSRSI_cvt_filler3  ??(716??);
   unsigned char CSRSI_cvtoslv0;
   unsigned char CSRSI_cvtoslv1;
   unsigned char CSRSI_cvtoslv2;
   unsigned char CSRSI_cvtoslv3;
  struct ??<
    int32_t CSRSI_cvtcsrsi : 1;          /* If on, indicates that the
                                        CSRSI service is available   */
    int32_t CSRSI_cvtoslv1_rsvd1 : 7;    /* Not needed                   */
         ??> CSRSI_cvtoslv4;
   unsigned char CSRSI_cvt_filler4 ??(11??);        /*               */
??>;


struct CSRSI_PSA ??<
   char CSRSI_psa_filler??(16??);
   struct CSRSI_CVT* CSRSI_cvt;
??>;

/*  End of CSRSI Header                                              */

#endif

/*********************************************************************/
/* si11v1 represents the output for a V1 CPC when general CPC        */
/* information is requested                                          */
/*********************************************************************/

typedef struct ??<
  unsigned char  _filler1??(32??);  /* Reserved                  @H1A*/
  unsigned char  si11v1cpcmanufacturer??(16??); /*
                                       The 16-character (0-9
                                       or uppercase A-Z) EBCDIC name
                                       of the manufacturer of the V1
                                       CPC. The name is
                                       left-justified with trailing
                                       blank characters if necessary.
                                                                 @H1A*/
  unsigned char  si11v1cpctype??(4??); /* The 4-character (0-9) EBCDIC
                                       type identifier of the V1 CPC.
                                                                 @H1A*/
  unsigned char  _filler2??(12??);  /* Reserved                  @H1A*/
  unsigned char  si11v1cpcmodelcapident??(16??); /*
                                       The 16-character (0-9 or
                                       uppercase A-Z) EBCDIC model
                                       capacity identifier of the
                                       configuration. The identifier
                                       is left-justified with trailing
                                       blank characters if necessary.
                                       If the first word of
                                       si11v1cpcmodel1 is zero, this
                                       field also represents the
                                       model                     @H2C*/
  unsigned char  si11v1cpcsequencecode??(16??); /*
                                       The 16-character (0-9
                                       or uppercase A-Z) EBCDIC
                                       sequence code of the V1 CPC.
                                       The sequence code is
                                       right-justified with leading
                                       EBCDIC zeroes if necessary.
                                                                 @H1A*/
  unsigned char  si11v1cpcplantofmanufacture??(4??); /* The 4-character
                                       (0-9 or uppercase A-Z) EBCDIC
                                       plant code that identifies the
                                       plant of manufacture for the
                                       V1 CPC. The plant code is
                                       left-justified with trailing
                                       blank characters if necessary.
                                                                 @H1A*/
  unsigned char  si11v1cpcmodel1??(16??); /* The 16-character (0-9 or
                                       uppercase A-Z) EBCDIC model
                                       identifier of the configuration.
                                       The identifier is left-justified
                                       with trailing blank characters
                                       if necessary. Valid only when
                                       first word is not zero.
                                       Otherwise, the cpcmodelcapident
                                       field represents both the
                                       model-capacity identifier
                                       and the model.            @H2A*/
  unsigned char  _filler3??(3980??); /* Reserved                 @H1A*/
??> si11v1;

  #define si11v1cpcmodel si11v1cpcmodelcapident

/*********************************************************************/
/* si22v1 represents the output for a V1 CPC when information        */
/* is requested about the set of CPUs                                */
/*********************************************************************/

typedef struct ??<
  uint32_t   si22v1format : 8;  /* A 1-byte value. When the
                                       value is 1, the ACCOffset field
                                       is valid                  @L1A*/
  uint32_t                : 8;  /* Reserved                  @L1A*/
  uint32_t   si22v1accoffset : 16; /* Alternate CPU Capability
                                       Offset. A 16-bit unsigned binary
                                       integer that specifies the
                                       offset in bytes of the
                                       alternate CPU capability
                                       area (which is physically
                                       within the SI22V1area, and is
                                       mapped by si22v1alt)
                                                                 @L1A*/
  unsigned char  _filler1??(24??);  /* Reserved                  @H3C*/
  unsigned char si22v1secondarycpucapability??(4??); /*
                                       An unsigned binary integer that,
                                       when not zero, specifies a
                                       secondary capability that may be
                                       applied to certain types of CPUs
                                       in the configuration.  There is
                                       no formal description of the
                                       algorithm used to generate this
                                       integer, except that it is the
                                       same algorithm used to generate
                                       the CPU capability. The integer
                                       is used as an indication of the
                                       capability of a CPU relative to
                                       the capability of other CPU
                                       models, and also relative to the
                                       capability of other CPU types
                                       within a model.  When the value
                                       is zero, all CPUs of any CPU
                                       type in the configuration have
                                       the same capability, as
                                       specified by the CPU capability.
                                                                 @H3A*/
  unsigned char  si22v1cpucapability??(4??); /*
                                       An unsigned binary integer
                                       that specifies the capability
                                       of one of the CPUs contained
                                       in the V1 CPC. It is used as
                                       an indication of the
                                       capability of the CPU relative
                                       to the capability of other CPU
                                       models.                   @H1A*/
  uint32_t   si22v1totalcpucount             : 16; /* A 2-byte
                                       unsigned integer
                                       that specifies the
                                       total number of CPUs contained
                                       in the V1 CPC. This number
                                       includes all CPUs in the
                                       configured state, the standby
                                       state, and the reserved state.
                                                                 @H1A*/
  uint32_t   si22v1configuredcpucount        : 16; /* A 2-byte
                                       unsigned binary
                                       integer that specifies
                                       the total number of CPUs that
                                       are in the configured state. A
                                       CPU is in the configured state
                                       when it is described in the
                                       V1-CPC configuration
                                       definition and is available to
                                       be used to execute programs.
                                                                 @H1A*/
  uint32_t   si22v1standbycpucount           : 16; /* A 2-byte
                                       unsigned integer
                                       that specifies the
                                       total number of CPUs that are
                                       in the standby state. A CPU is
                                       in the standby state when it
                                       is described in the V1-CPC
                                       configuration definition, is
                                       not available to be used to
                                       execute programs, but can be
                                       used to execute programs by
                                       issuing instructions to place
                                       it in the configured state.
                                                                 @H1A*/
  uint32_t   si22v1reservedcpucount          : 16; /* A 2-byte
                                       unsigned binary
                                       integer that specifies
                                       the total number of CPUs that
                                       are in the reserved state. A
                                       CPU is in the reserved state
                                       when it is described in the
                                       V1-CPC configuration
                                       definition, is not available
                                       to be used to execute
                                       programs, and cannot be made
                                       available to be used to
                                       execute programs by issuing
                                       instructions to place it in
                                       the configured state, but it
                                       may be possible to place it in
                                       the standby or configured
                                       state through manually
                                       initiated actions         @H1A*/
  struct ??<
    unsigned char  _si22v1mpcpucapaf??(2??); /* Each individual
                                       adjustment factor.        @H1A*/
    unsigned char  _filler2??(4050??);
  ??> si22v1mpcpucapafs;               /* This field is valid only
                                  when si22v1format is 0         @L1A*/
??> si22v1;

#define si22v1mpcpucapaf  si22v1mpcpucapafs._si22v1mpcpucapaf

/*********************************************************************/
/* si22v1alt maps the area located within the si22v1 area by the     */
/* si22v1accoffset field, when the si22v1format field has a value    */
/* of one.                                                           */
/*********************************************************************/

typedef struct ??<
  uint32_t   si22v1altcpucapability;  /* A 32-bit unsigned binary
                                  integer that specifies the announced
                                  capability of one of the CPUs in the
                                  configuration. There is no formal
                                  description of the algorithm used to
                                  generate this integer. The integer is
                                  used as an indication of the
                                  announced capability of the CPU
                                  relative to the announced capability
                                  of other CPU models.
                                  The alternate-capability value
                                  applies to each of the CPUs in the
                                  configuration. That is, all CPUs in
                                  the configuraiton have the same
                                  alternate capability.
                                                                 @L1A*/
  struct ??<
    unsigned char _si22v1altmpcpucapaf??(2??); /* Each individual
                                  adjustment factor. Note that the
                                  leading underscore in the name is
                                  to allow use of a #define that
                                  is below.                      @L1A*/
    unsigned char  _filler2??(4050??);
  ??> si22v1altmpcpucapafs;       /*
                                  A series of contiguous 2-byte fields,
                                  each containing a 16-bit unsigned
                                  binary integer which is an adjustment
                                  factor (percentage) for the value
                                  contained in the altternate-CPU-
                                  capability field. The number of
                                  alternate-adjustment-factor
                                  fields is one less than the number
                                  of CPUs specified in the
                                  total-CPU-count field. The alternate-
                                  adjustment-factor fields correspond
                                  to configurations
                                  with increasing numbers of CPUs in
                                  the configured state. The first
                                  alternate-adjustment-factor
                                  field corresponds to a configuration
                                  with two CPUs in the configured
                                  state. Each successive alternate-
                                  adjustment-factor field corresponds
                                  to a configuration with a number of
                                  CPUs in the configurd state that is
                                  more than that for the preceding
                                  field.                         @L1A*/
??> si22v1alt;                                                /* @L1A*/

#define si22v1altmpcpucapaf  si22v1altmpcpucapafs._si22v1altmpcpucapaf

/*********************************************************************/
/* si22v2 represents the output for a V2 CPC when information        */
/* is requested about the set of CPUs                                */
/*********************************************************************/

typedef struct ??<
  unsigned char  _filler1??(32??);  /* Reserved                  @H1A*/
  uint32_t   si22v2cpcnumber                 : 16; /* A 2-byte
                                       unsigned integer
                                       which is the number of
                                       this V2 CPC. This number
                                       distinguishes this V2 CPC from
                                       all other V2 CPCs provided by
                                       the same logical-partition
                                       hypervisor                @H1A*/
  unsigned char  _filler2;          /* Reserved                  @H1A*/
  struct ??<
    uint32_t   _si22v2lcpudedicated           : 1; /*
                                       When one, indicates that
                                       one or more of the logical
                                       CPUs for this V2 CPC are
                                       provided using V1 CPUs that
                                       are dedicated to this V2 CPC
                                       and are not used to provide
                                       logical CPUs for any other V2
                                       CPCs. The number of logical
                                       CPUs that are provided using
                                       dedicated V1 CPUs is specified
                                       by the dedicated-LCPU-count
                                       value. When zero, bit 0
                                       indicates that none of the
                                       logical CPUs for this V2 CPC
                                       are provided using V1 CPUs
                                       that are dedicated to this V2
                                       CPC.                      @H1A*/
    uint32_t   _si22v2lcpushared               : 1; /*
                                       When one, indicates that
                                       or more of the logical CPUs
                                       for this V2 CPC are provided
                                       using V1 CPUs that can be used
                                       to provide logical CPUs for
                                       other V2 CPCs. The number of
                                       logical CPUs that are provided
                                       using shared V1 CPUs is
                                       specified by the
                                       shared-LCPU-count value. When
                                       zero, it indicates that none
                                       of the logical CPUs for this
                                       V2 CPC are provided using
                                       shared V1 CPUs.           @H1A*/
    uint32_t   _si22v2lcpuulimit               : 1; /*
                                       Utilization limit. When one,
                                       indicates that the amount of
                                       use of the V1-CPC CPUs that
                                       are used to provide the
                                       logical CPUs for this V2 CPC
                                       is limited. When zero, it
                                       indicates that the amount of
                                       use of the V1-CPC CPUs that
                                       are used to provide the
                                       logical CPUs for this V2 CPC
                                       is unlimited.             @H1A*/
    uint32_t   _filler3                        : 5; /* Reserved
                                                                 @H1A*/
  ??> si22v2lcpuc;                  /* Characteristics           @H1A*/
  uint32_t   si22v2totallcpucount            : 16; /*
                                       A 2-byte unsigned
                                       integer that specifies the
                                       total number of logical CPUs
                                       that are provided for this V2
                                       CPC. This number includes all
                                       of the logical CPUs that are
                                       in the configured state, the
                                       standby state, and the
                                       reserved state.           @H1A*/
  uint32_t   si22v2configuredlcpucount       : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the total number of logical
                                       CPUs for this V2 CPC that are
                                       in the configured state. A
                                       logical CPU is in the
                                       configured state when it is
                                       described in the V2-CPC
                                       configuration definition and
                                       is available to be used to
                                       execute programs.         @H1A*/
  uint32_t   si22v2standbylcpucount          : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the total number of logical
                                       CPUs that are in the standby
                                       state. A logical CPU is in the
                                       standby state when it is
                                       described in the V2-CPC
                                       configuration definition, is
                                       not available to be used to
                                       execute programs, but can be
                                       used to execute programs by
                                       issuing instructions to place
                                       it in the configured state.
                                                                 @H1A*/
  uint32_t   si22v2reservedlcpucount         : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the total number of logical
                                       CPUs that are in the reserved
                                       state. A logical CPU is in the
                                       reserved state when it is
                                       described in the V2-CPC
                                       configuration definition, is
                                       not available to be used to
                                       execute programs, and cannot
                                       be made available to be used
                                       to execute programs by issuing
                                       instructions to place it in
                                       the configured state, but it
                                       may be possible to place it in
                                       the standby or configured
                                       state through manually
                                       initiated actions         @H1A*/
  unsigned char  si22v2cpcname??(8??);  /*
                                       The 8-character EBCDIC name of
                                       this V2 CPC. The name is
                                       left-justified with trailing
                                       blank characters if necessary.
                                                                 @H1A*/
  unsigned char  si22v2cpccapabilityaf??(4??); /* Capability Adjustment
                                       Factor (CAF). An unsigned
                                       binary integer of 1000 or
                                       less. The adjustment factor
                                       specifies the amount of the
                                       V1-CPC capability that is
                                       allowed to be used for this V2
                                       CPC by the logical-partition
                                       hypervisor. The fraction of
                                       V1-CPC capability is
                                       determined by dividing the CAF
                                       value by 1000.            @H1A*/
  unsigned char  si22v2lparorigin??(8??); /* A 64-bit unsigned binary
                                       integer, called a logical
                                       partition origin, which
                                       represents the relocation-zone
                                       origin of the logical
                                       partition.                @L3C*/
  unsigned char  _filler4??(8??);   /* Reserved                  @L3C*/
  uint32_t   si22v2dedicatedlcpucount        : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the number of configured-state
                                       logical CPUs for this V2 CPC
                                       that are provided using
                                       dedicated V1 CPUs. (See the
                                       description of bit
                                       si22v2lcpudedicated.)     @H1A*/
  uint32_t   si22v2sharedlcpucount           : 16; /*
                                       A 2-byte unsigned
                                       integer that specifies the
                                       number of configured-state
                                       logical CPUs for this V2 CPC
                                       that are provided using shared
                                       V1 CPUs. (See the description
                                       of bit si22v2lcpushared.)
                                                                 @H1A*/
  unsigned char  _filler5??(4020??); /* Reserved                 @H1A*/
??> si22v2;

#define si22v2lcpudedicated       si22v2lcpuc._si22v2lcpudedicated
#define si22v2lcpushared          si22v2lcpuc._si22v2lcpushared
#define si22v2lcpuulimit          si22v2lcpuc._si22v2lcpuulimit

/*********************************************************************/
/* si22v3db is a description block that comprises part of the        */
/* si22v3 data.                                                      */
/*********************************************************************/

typedef struct ??<
  unsigned char  _filler1??(4??);   /* Reserved                  @H1A*/
  uint32_t   si22v3dbtotallcpucount            : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the total number of logical
                                       CPUs that are provided for
                                       this V3 CPC. This number
                                       includes all of the logical
                                       CPUs that are in the
                                       configured state, the standby
                                       state, and the reserved state.
                                                                 @H1A*/
  uint32_t   si22v3dbconfiguredlcpucount       : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the number of logical CPUs for
                                       this V3 CPC that are in the
                                       configured state. A logical
                                       CPU is in the configured state
                                       when it is described in the
                                       V3-CPC configuration
                                       definition and is available to
                                       be used to execute programs.
                                                                 @H1A*/
  uint32_t   si22v3dbstandbylcpucount          : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the number of logical CPUs for
                                       this V3 CPC that are in the
                                       standby state. A logical CPU
                                       is in the standby state when
                                       it is described in the V3-CPC
                                       configuration definition, is
                                       not available to be used to
                                       execute programs, but can be
                                       used to execute programs by
                                       issuing instructions to place
                                       it in the configured state.
                                                                 @H1A*/
  uint32_t   si22v3dbreservedlcpucount         : 16; /*
                                       A 2-byte unsigned
                                       binary integer that specifies
                                       the number of logical CPUs for
                                       this V3 CPC that are in the
                                       reserved state. A logical CPU
                                       is in the reserved state when
                                       it is described in the V2-CPC
                                       configuration definition, is
                                       not available to be used to
                                       execute programs, and cannot
                                       be made available to be used
                                       to execute programs by issuing
                                       instructions to place it in
                                       the configured state, but it
                                       may be possible to place it in
                                       the standby or configured
                                       state through manually
                                       initiated actions         @H1A*/
  unsigned char  si22v3dbcpcname??(8??); /* The 8-character EBCDIC name
                                       of this V3 CPC. The name is
                                       left-justified with trailing
                                       blank characters if necessary.
                                                                 @H1A*/
  unsigned char  si22v3dbcpccaf??(4??); /* A 4-byte unsigned binary
                                        integer that specifies an
                                        adjustment factor. The
                                        adjustment factor specifies
                                        the amount of the V1-CPC or
                                        V2-CPC capability that is
                                        allowed to be used for this V3
                                        CPC by the
                                        virtual-machine-hypervisor
                                        program.                 @H1A*/
  unsigned char  si22v3dbvmhpidentifier??(16??); /* The 16-character
                                       EBCDIC identifier of the
                                       virtual-machine-hypervisor
                                       program that provides this V3
                                       CPC. (This identifier may
                                       include qualifiers such as
                                       version number and release
                                       level). The identifier is
                                       left-justified with trailing
                                       blank characters if necessary.
                                                                 @H1A*/
  unsigned char  _filler2??(24??);  /* Reserved                  @H1A*/
??> si22v3db;
/*********************************************************************/
/* si22v3 represents the output for a V3 CPC when information        */
/* is requested about the set of CPUs                                */
/*********************************************************************/

typedef struct ??<
  unsigned char  _filler1??(28??);  /* Reserved                  @H1A*/
  unsigned char  _filler2??(3??);   /* Reserved                  @H1A*/
  struct ??<
      uint32_t   _filler3                : 4; /* Reserved
                                                                 @H1A*/
      uint32_t   _si22v3dbcount          : 4; /*
                                       Description Block Count. A
                                       4-bit unsigned binary integer
                                       that indicates the number (up
                                       to 8) of V3-CPC description
                                       blocks that are stored in the
                                       si22v3dbe array.          @H1A*/
  ??> si22v3dbcountfield;           /*                           @H1A*/
  si22v3db  si22v3dbe??(8??);    /* Array of entries. Only the number
                                    indicated by si22v3dbcount
                                    are valid                    @H1A*/
  unsigned char  _filler5??(3552??); /* Reserved                 @H1A*/
??> si22v3;

#define si22v3dbcount      si22v3dbcountfield._si22v3dbcount


/*********************************************************************/
/* SI00 represents the "starter" information. This structure is      */
/* part of the information returned on every CSRSI request.          */
/*********************************************************************/

typedef struct ??<
  char           si00cpcvariety;    /* SI00CPCVariety_V1CPC_MACHINE,
                                       SI00CPCVariety_V2CPC_LPAR, or
                                       SI00CPCVariety_V3CPC_VM   @H1A*/
    struct ??<
               int32_t   _si00validsi11v1  : 1; /* si11v1 was requested and
                                   the information returned is valid
                                                                 @H1A*/
               int32_t   _si00validsi22v1  : 1; /* si22v2 was requested and
                                   the information returned is valid
                                                                 @H1A*/
               int32_t   _si00validsi22v2  : 1; /* si22v2 was requested and
                                   the information returned is valid
                                                                 @H1A*/
               int32_t   _si00validsi22v3  : 1; /* si22v3 was requested and
                                   the information returned is valid
                                                                 @H1A*/
               int32_t   _filler1          : 4; /* Reserved          @H1A*/
    ??> si00validityflags;
  unsigned char  _filler2??(2??);   /* Reserved                  @H1A*/
  unsigned char  si00pccacpid??(12??); /* PCCACPID value for this CPU
                                                                 @H1A*/
  unsigned char  si00pccacpua??(2??); /* PCCACPUA value for this CPU
                                                                 @H1A*/
  unsigned char  si00pccacafm??(2??); /* PCCACAFM value for this CPU.
                                         This has information only
                                         about CPUs 0-15         @L2C*/
  unsigned char  _filler3??(4??);   /* Reserved                  @H1A*/
  unsigned char  si00lastupdatetimestamp??(8??); /* Time of last STSI
                                       update, via STCK          @H1A*/
  unsigned char  si00pcca_cpu_address_mask??(8??); /*
                            PCCA_CPU_Address_Mask value for this CPU
                                                                 @L2A*/
  unsigned char  _filler4??(24??);  /* Reserved                  @L2C*/
??> si00;

#define si00validsi11v1         si00validityflags._si00validsi11v1
#define si00validsi22v1         si00validityflags._si00validsi22v1
#define si00validsi22v2         si00validityflags._si00validsi22v2
#define si00validsi22v3         si00validityflags._si00validsi22v3

/*********************************************************************/
/* siv1 represents the information returned when V1CPC_MACHINE       */
/* data is requested                                                 */
/*********************************************************************/

typedef struct ??<
  si00 siv1si00;                                    /* Area mapped by
                                       struct si00               @H1A*/
  si11v1 siv1si11v1;                                    /* Area
                                       mapped by struct si11v1   @H1A*/
  si22v1 siv1si22v1;                                    /* Area
                                       mapped by struct si22v1   @H1A*/
??> siv1;

/*********************************************************************/
/* siv1v2 represents the information returned when V1CPC_MACHINE     */
/* data and V2CPC_LPAR data is requested                             */
/*********************************************************************/

typedef struct ??<
  si00 siv1v2si00;                                  /* Area mapped by
                                       by struct si00            @H1A*/
  si11v1 siv1v2si11v1;                                    /* Area
                                       mapped by struct si11v1   @H1A*/
  si22v1 siv1v2si22v1;                                    /* Area
                                       mapped by struct si22v2   @H1A*/
  si22v2 siv1v2si22v2;                                    /* Area
                                       mapped by struct si22v2   @H1A*/
??> siv1v2;

/*********************************************************************/
/* siv1v2v3 represents the information returned when V1CPC_MACHINE   */
/* data, V2CPC_LPAR data and V3CPC_VM data is requested              */
/*********************************************************************/

typedef struct ??<
  si00 siv1v2v3si00;                                    /* Area
                                       mapped by struct si00     @H1A*/
  si11v1 siv1v2v3si11v1;                                    /* Area
                                       mapped by struct si11v1   @H1A*/
  si22v1 siv1v2v3si22v1;                                    /* Area
                                       mapped by struct si22v1   @H1A*/
  si22v2 siv1v2v3si22v2;                                    /* Area
                                       mapped by struct si22v2   @H1A*/
  si22v3 siv1v2v3si22v3;                                    /* Area
                                       mapped by struct si22v3   @H1A*/
??> siv1v2v3;

/*********************************************************************/
/* siv1v3 represents the information returned when V1CPC_MACHINE     */
/* data and V3CPC_VM data is requested                               */
/*********************************************************************/

typedef struct ??<
  si00 siv1v3si00;                                    /* Area mapped
                                       by struct si00            @H1A*/
  si11v1 siv1v3si11v1;                                    /* Area
                                       mapped by struct si11v1   @H1A*/
  si22v1 siv1v3si22v1;                                    /* Area
                                       mapped by struct si22v1   @H1A*/
  si22v3 siv1v3si22v3;                                    /* Area
                                       mapped by struct si22v3   @H1A*/
??> siv1v3;

/*********************************************************************/
/* siv2 represents the information returned when V2CPC_LPAR          */
/* data is requested                                                 */
/*********************************************************************/

typedef struct ??<
  si00 siv2si00;                    /* Area mapped by
                                       struct si00               @H1A*/
  si22v2 siv2si22v2;                /* Area
                                       mapped by struct si22v2   @H1A*/
??> siv2;

/*********************************************************************/
/* siv2v3 represents the information returned when V2CPC_LPAR        */
/* and V3CPC_VM data is requested                                    */
/*********************************************************************/

typedef struct ??<
  si00 siv2v3si00;                  /* Area mapped
                                       by struct si00            @H1A*/
  si22v2 siv2v3si22v2;              /* Area
                                       mapped by struct si22v2   @H1A*/
  si22v3 siv2v3si22v3;              /* Area
                                       mapped by struct si22v3   @H1A*/
??> siv2v3;

/*********************************************************************/
/* siv3 represents the information returned when V3CPC_VM            */
/* data is requested                                                 */
/*********************************************************************/

typedef struct ??<
  si00 siv3si00;                    /* Area mapped by
                                       struct si00               @H1A*/
  si22v3 siv3si22v3;                /* Area
                                       mapped by struct si22v3   @H1A*/
??> siv3;


/*********************************************************************
 *       Fixed Service Parameter and Return Code Defines             *
 *********************************************************************/

/*  SI00 Constants                                                   */

#define SI00CPCVARIETY_V1CPC_MACHINE   1
#define SI00CPCVARIETY_V2CPC_LPAR      2
#define SI00CPCVARIETY_V3CPC_VM        3

/*  CSRSI Constants                                                  */

#define CSRSI_REQUEST_V1CPC_MACHINE    1
#define CSRSI_REQUEST_V2CPC_LPAR       2
#define CSRSI_REQUEST_V3CPC_VM         4

/*  CSRSI Return codes                                               */

#define CSRSI_SUCCESS                  0
#define CSRSI_STSINOTAVAILABLE         4
#define CSRSI_SERVICENOTAVAILABLE      8
#define CSRSI_BADREQUEST               12
#define CSRSI_BADINFOAREALEN           16
#define CSRSI_BADLOCK                  20

