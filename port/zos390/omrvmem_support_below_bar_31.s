***********************************************************************
*
* (c) Copyright IBM Corp. 2012, 2015
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
*    Multiple authors (IBM Corp.) - initial API and implementation
*    and/or initial documentation
***********************************************************************

         TITLE 'omrvmem_support_below_bar_32'

*** Please note: This file contains 2 Macros:
*
* NOTE: Each of these macro definitions start with "MACRO" and end
*       with "MEND"
*
* 1. MYPROLOG. This was needed for the METAL C compiler implementation
*       of j9allocate_large_pages and j9free_large_pages (implemented
*       at bottom).
* 2. MYEPILOG. See explanation for MYPROLOG
*
* This file also includes HLASM call to STORAGE OBTAIN HLASM
* 		macro
*		- These calls were generated using the METAL-C compiler
*		- See j9storage.c for details/instructions.
*
*
         MACRO                                                                 
&NAME    MYPROLOG                                                              
         GBLC  &CCN_PRCN                                                       
         GBLC  &CCN_LITN                                                       
         GBLC  &CCN_BEGIN                                                      
         GBLC  &CCN_ARCHLVL                                                    
         GBLA  &CCN_DSASZ                                                      
         GBLA  &CCN_RLOW                                                       
         GBLA  &CCN_RHIGH                                                      
         GBLB  &CCN_NAB                                                        
         GBLB  &CCN_LP64                                                       
         LARL  15,&CCN_LITN                                                    
         USING &CCN_LITN,15                                                    
         GBLA  &MY_DSASZ                                                       
&MY_DSASZ SETA 0                                                                
         AIF   (&CCN_LP64).LP64_1                                               
         STM   14,12,12(13)                                                     
         AGO   .NEXT_1                                                          
.LP64_1  ANOP                                                                   
         STMG  14,12,8(13)                                                      
.NEXT_1  ANOP                                                                   
         AIF   (&CCN_DSASZ LE 0).DROP                                           
&MY_DSASZ SETA &CCN_DSASZ                                                       
         AIF   (&CCN_DSASZ GT 32767).USELIT                                     
         AIF   (&CCN_LP64).LP64_2                                               
         LHI   0,&CCN_DSASZ                                                     
         AGO   .NEXT_2                                                          
.LP64_2  ANOP                                                                   
         LGHI  0,&CCN_DSASZ                                                     
         AGO   .NEXT_2                                                          
.USELIT  ANOP                                                                   
         AIF   (&CCN_LP64).LP64_3                                               
         L     0,=F'&CCN_DSASZ'                                                 
         AGO   .NEXT_2                                                          
.LP64_3  ANOP                                                                   
         LGF   0,=F'&CCN_DSASZ'                                                 
.NEXT_2  AIF   (NOT &CCN_NAB).GETDSA                                            
&MY_DSASZ SETA &MY_DSASZ+1048576                                                
         LA    1,1                                                              
         SLL   1,20                                                             
         AIF   (&CCN_LP64).LP64_4                                               
         AR    0,1                                                              
         AGO   .GETDSA                                                          
.LP64_4  ANOP                                                                   
         AGR   0,1                                                              
.GETDSA ANOP                                                                    
         STORAGE OBTAIN,LENGTH=(0),BNDRY=PAGE                                   
         AIF   (&CCN_LP64).LP64_5                                               
         LR    15,1                                                             
         ST    15,8(,13)                                                        
         L     1,24(,13)                                                        
         ST    13,4(,15)                                                        
         LR    13,15                                                            
         AGO   .DROP                                                            
.LP64_5  ANOP                                                                   
         LGR   15,1                                                             
         STG   15,136(,13)                                                      
         LG    1,32(,13)                                                        
         STG   13,128(,15)                                                      
         LGR   13,15                                                            
.DROP    ANOP                                                                   
         DROP  15                                                               
         MEND                                                                   
         MACRO                                                                  
&NAME    MYEPILOG                                                               
         GBLC  &CCN_PRCN                                                        
         GBLC  &CCN_LITN                                                        
         GBLC  &CCN_BEGIN                                                       
         GBLC  &CCN_ARCHLVL                                                     
         GBLA  &CCN_DSASZ                                                       
         GBLA  &CCN_RLOW                                                        
         GBLA  &CCN_RHIGH                                                       
         GBLB  &CCN_NAB                                                         
         GBLB  &CCN_LP64                                                        
         GBLA  &MY_DSASZ                                                        
         AIF   (&MY_DSASZ EQ 0).NEXT_1                                          
         AIF   (&CCN_LP64).LP64_1                                               
         LR    1,13                                                             
         AGO   .NEXT_1                                                          
.LP64_1  ANOP                                                                   
         LGR   1,13                                                             
.NEXT_1  ANOP                                                                   
         AIF   (&CCN_LP64).LP64_2                                               
         L     13,4(,13)                                                        
         AGO   .NEXT_2                                                          
.LP64_2  ANOP                                                                   
         LG    13,128(,13)                                                      
.NEXT_2  ANOP                                                                   
         AIF   (&MY_DSASZ EQ 0).NODSA                                           
         AIF   (&CCN_LP64).LP64_3                                               
         ST    15,16(,13)                                                       
         AGO   .NEXT_3                                                          
.LP64_3  ANOP                                                                   
         STG   15,16(,13)                                                       
.NEXT_3  ANOP                                                                   
         LARL  15,&CCN_LITN                                                     
         USING &CCN_LITN,15                                                     
         STORAGE RELEASE,LENGTH=&MY_DSASZ,ADDR=(1)                              
         AIF   (&CCN_LP64).LP64_4                                               
         L     15,16(,13)                                                       
         AGO   .NEXT_4                                                          
.LP64_4  ANOP                                                                   
         LG    15,16(,13)                                                       
.NEXT_4  ANOP                                                                   
.NODSA   ANOP                                                                   
         AIF   (&CCN_LP64).LP64_5                                               
         L     14,12(,13)                                                       
         LM    1,12,24(13)                                                      
         AGO   .NEXT_5                                                          
.LP64_5  ANOP                                                                   
         LG    14,8(,13)                                                        
         LMG   1,12,32(13)                                                      
.NEXT_5  ANOP                                                                   
         BR    14                                                              
         DROP  15                                                               
         MEND
*
**************************************************
* Insert contents of j9storage.s below
**************************************************
*
         ACONTROL AFPR                                                   000000
J9STORAGE CSECT                                                          000000
J9STORAGE AMODE 31                                                       000000
J9STORAGE RMODE ANY                                                      000000
         GBLA  &CCN_DSASZ              DSA size of the function          000000
         GBLA  &CCN_SASZ               Save Area Size of this function   000000
         GBLA  &CCN_ARGS               Number of fixed parameters        000000
         GBLA  &CCN_RLOW               High GPR on STM/STMG              000000
         GBLA  &CCN_RHIGH              Low GPR for STM/STMG              000000
         GBLB  &CCN_MAIN               True if function is main          000000
         GBLB  &CCN_LP64               True if compiled with LP64        000000
         GBLB  &CCN_NAB                True if NAB needed                000000
.* &CCN_NAB is to indicate if there are called functions that depend on  000000
.* stack space being pre-allocated. When &CCN_NAB is true you'll need    000000
.* to add a generous amount to the size set in &CCN_DSASZ when you       000000
.* obtain the stack space.                                               000000
         GBLB  &CCN_ALTGPR(16)         Altered GPRs by the function      000000
         GBLB  &CCN_SASIG              True to gen savearea signature    000000
         GBLC  &CCN_PRCN               Entry symbol of the function      000000
         GBLC  &CCN_CSECT              CSECT name of the file            000000
         GBLC  &CCN_LITN               Symbol name for LTORG             000000
         GBLC  &CCN_BEGIN              Symbol name for function body     000000
         GBLC  &CCN_ARCHLVL            n in ARCH(n) option               000000
         GBLC  &CCN_ASCM               A=AR mode P=Primary mode          000000
         GBLC  &CCN_NAB_OFFSET         Offset to NAB pointer in DSA      000000
         GBLB  &CCN_NAB_STORED         True if NAB pointer stored        000000
         GBLC  &CCN_PRCN_LONG          Full func name up to 1024 chars   000000
         GBLB  &CCN_STATIC             True if function is static        000000
         GBLB  &CCN_RENT               True if compiled with RENT        000000
         GBLB  &CCN_APARSE             True to parse OS PARM             000000
&CCN_SASIG SETB 1                                                        000000
&CCN_LP64 SETB 0                                                         000000
&CCN_RENT SETB 0                                                         000000
&CCN_APARSE SETB 1                                                       000000
&CCN_CSECT SETC 'J9STORAGE'                                              000000
&CCN_ARCHLVL SETC '7'                                                    000000
         SYSSTATE ARCHLVL=2                                              000000
         IEABRCX DEFINE                                                  000000
.* The HLASM GOFF option is needed to assemble this program              000000
@@CCN@19 ALIAS C'j9free_memory_below_bar'                                000000
@@CCN@11 ALIAS C'j9allocate_4K_pages_below_bar'                          000000
@@CCN@1  ALIAS C'j9allocate_1M_pageable_pages_below_bar'                 000000
* /********************************************************************  000001
*  *                                                                     000002
*  * (c) Copyright IBM Corp. 2001, 2015                                  000003
*  *                                                                     000004
*  *  This program and the accompanying materials are made available     000005
*  *  under the terms of the Eclipse Public License v1.0 and             000006
*  *  Apache License v2.0 which accompanies this distribution.           000007
*  *                                                                     000008
*  *      The Eclipse Public License is available at                     000009
*  *      http://www.eclipse.org/legal/epl-v10.html                      000010
*  *                                                                     000011
*  *      The Apache License v2.0 is available at                        000012
*  *      http://www.opensource.org/licenses/apache2.0.php               000013
*  *                                                                     000014
*  * Contributors:                                                       000015
*  *    Multiple authors (IBM Corp.) - initial API and implementation a  000016
*  ********************************************************************  000017
*                                                                        000018
* /*                                                                     000019
*  * This file is used to generate the HLASM corresponding to the C cal  000020
*  * that use the STORAGE macro in omrvmem.c                             000021
*  *                                                                     000022
*  * This file is compiled manually using the METAL-C compiler that was  000023
*  * introduced in z/OS V1R9. The generated output (j9storage.s) is the  000024
*  * inserted into omrvmem_support_below_bar_[31|64].s which is compile  000025
*  *                                                                     000026
*  * omrvmem_support_below_bar_[31|64].s indicates where to put the con  000027
*  * Search for:                                                         000028
*  *   Insert contents of j9storage.s below                              000029
*  *                                                                     000030
*  * *******                                                             000031
*  * NOTE!!!!! You must strip the line numbers from any pragma statemen  000032
*  *   (CMVC 140560)                                                     000033
*  * *******                                                             000034
*  *                                                                     000035
*  * It should be obvious, however, just to be clear be sure to omit th  000036
*  * first two lines from j9storage.s which will look something like:    000037
*  *                                                                     000038
*  *          TITLE '5694A01 V1.9 z/OS XL C                              000039
*  *                     ./j9storage.c'                                  000040
*  *                                                                     000041
*  * To compile for 64-bit use:                                          000042
*  *  xlc -S -qmetal -Wc,lp64 -qlongname j9storage.c                     000043
*  *                                                                     000044
*  * To compile for 31-bit use:                                          000045
*  *  xlc -S -qmetal -qlongname j9storage.c                              000046
*  *                                                                     000047
*  * z/OS V1R9 elements and features:                                    000048
*  *   http://www-03.ibm.com/systems/z/os/zos/bkserv/r9pdf/index.html    000049
*  *    - One stop shopping for all things z/OS.                         000050
*  *                                                                     000051
*  * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:      000052
*  *   http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf             000053
*  *                                                                     000054
*  */                                                                    000055
*                                                                        000056
* #pragma prolog(j9allocate_1M_pageable_pages_below_bar,"MYPROLOG")      000057
* #pragma epilog(j9allocate_1M_pageable_pages_below_bar,"MYEPILOG")      000058
*                                                                        000059
* /*                                                                     000060
*  * Allocate 1MB pageable pages below 2GB bar using STORAGE system mac  000061
*  * Memory allocated is freed using j9free_memory_below_bar().          000062
*  *                                                                     000063
*  * @params[in] numBytes Number of bytes to be allocated                000064
*  * @params[in] subpool subpool number to be used                       000065
*  *                                                                     000066
*  * @return pointer to memory allocated, NULL on failure.               000067
*  */                                                                    000068
* void *                                                                 000069
* j9allocate_1M_pageable_pages_below_bar(long *numBytes, int *subpool)   000070
* {                                                                      000071
*  long length;                                                          000072
*  long sp;                                                              000073
*  long addr;                                                            000074
*  int rc = 0;                                                           000075
*                                                                        000076
*  length = *numBytes;                                                   000077
*  sp = *subpool;                                                        000078
*                                                                        000079
*  __asm(" STORAGE OBTAIN,COND=YES,LOC=(31,PAGEFRAMESIZE1MB),"\          000080
*    "LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\                          000081
*    :"=r"(rc),"=r"(addr):"r"(length),"r"(sp));                          000082
*                                                                        000083
*  if (0 == rc) {                                                        000084
*   return (void *)addr;                                                 000085
*  } else {                                                              000086
*   return (void *)0;                                                    000087
*  }                                                                     000088
* }                                                                      000089
*                                                                        000090
* #pragma prolog(j9allocate_4K_pages_below_bar,"MYPROLOG")               000091
* #pragma epilog(j9allocate_4K_pages_below_bar,"MYEPILOG")               000092
*                                                                        000093
* /*                                                                     000094
*  * Allocate 4K pages below 2GB bar using STORAGE system macro.         000095
*  * Memory allocated is freed using j9free_memory_below_bar().          000096
*  *                                                                     000097
*  * @params[in] numBytes Number of bytes to be allocated                000098
*  * @params[in] subpool subpool number to be used                       000099
*  *                                                                     000100
*  * @return pointer to memory allocated, NULL on failure. Returned val  000101
*  */                                                                    000102
* void *                                                                 000103
* j9allocate_4K_pages_below_bar(long *numBytes, int *subpool)            000104
* {                                                                      000105
*  long length;                                                          000106
*  long sp;                                                              000107
*  long addr;                                                            000108
*  int rc = 0;                                                           000109
*                                                                        000110
*  length = *numBytes;                                                   000111
*  sp = *subpool;                                                        000112
*                                                                        000113
*  __asm(" STORAGE OBTAIN,COND=YES,LOC=(31,64),BNDRY=PAGE,"\             000114
*    "LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\                          000115
*    :"=r"(rc),"=r"(addr):"r"(length),"r"(sp));                          000116
*                                                                        000117
*  if (0 == rc) {                                                        000118
*   return (void *)addr;                                                 000119
*  } else {                                                              000120
*   return (void *)0;                                                    000121
*  }                                                                     000122
* }                                                                      000123
*                                                                        000124
* #pragma prolog(j9free_memory_below_bar,"MYPROLOG")                     000125
* #pragma epilog(j9free_memory_below_bar,"MYEPILOG")                     000126
*                                                                        000127
* /*                                                                     000128
*  * Free memory allocated using STORAGE system macro.                   000129
*  *                                                                     000130
*  * @params[in] address pointer to memory region to be freed            000131
*  *                                                                     000132
*  * @return non-zero if memory is not freed successfully, 0 otherwise.  000133
*  */                                                                    000134
* int                                                                    000135
* j9free_memory_below_bar(void *address, long *length, int *subpool)     000136
         J     @@CCN@19                                                  000136
@@PFD@@  DC    XL8'00C300C300D50000'   Prefix Data Marker                000136
         DC    CL8'20160229'           Compiled Date YYYYMMDD            000136
         DC    CL6'095330'             Compiled Time HHMMSS              000136
         DC    XL4'42010000'           Compiler Version                  000136
         DC    XL2'0000'               Reserved                          000136
         DC    BL1'00000000'           Flag Set 1                        000136
         DC    BL1'00000000'           Flag Set 2                        000136
         DC    BL1'00000000'           Flag Set 3                        000136
         DC    BL1'00000000'           Flag Set 4                        000136
         DC    XL4'00000000'           Reserved                          000136
         ENTRY @@CCN@19                                                  000136
@@CCN@19 AMODE 31                                                        000136
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000136
         DC    A(@@FPB@1-*+8)          Signed offset to FPB              000136
         DC    XL4'00000000'           Reserved                          000136
@@CCN@19 DS    0F                                                        000136
&CCN_PRCN SETC '@@CCN@19'                                                000136
&CCN_PRCN_LONG SETC 'j9free_memory_below_bar'                            000136
&CCN_LITN SETC '@@LIT@1'                                                 000136
&CCN_BEGIN SETC '@@BGN@1'                                                000136
&CCN_ASCM SETC 'P'                                                       000136
&CCN_DSASZ SETA 168                                                      000136
&CCN_SASZ SETA 72                                                        000136
&CCN_ARGS SETA 3                                                         000136
&CCN_RLOW SETA 14                                                        000136
&CCN_RHIGH SETA 6                                                        000136
&CCN_NAB SETB  0                                                         000136
&CCN_MAIN SETB 0                                                         000136
&CCN_NAB_STORED SETB 0                                                   000136
&CCN_STATIC SETB 0                                                       000136
&CCN_ALTGPR(1) SETB 1                                                    000136
&CCN_ALTGPR(2) SETB 1                                                    000136
&CCN_ALTGPR(3) SETB 1                                                    000136
&CCN_ALTGPR(4) SETB 1                                                    000136
&CCN_ALTGPR(5) SETB 1                                                    000136
&CCN_ALTGPR(6) SETB 1                                                    000136
&CCN_ALTGPR(7) SETB 1                                                    000136
&CCN_ALTGPR(8) SETB 0                                                    000136
&CCN_ALTGPR(9) SETB 0                                                    000136
&CCN_ALTGPR(10) SETB 0                                                   000136
&CCN_ALTGPR(11) SETB 0                                                   000136
&CCN_ALTGPR(12) SETB 0                                                   000136
&CCN_ALTGPR(13) SETB 0                                                   000136
&CCN_ALTGPR(14) SETB 1                                                   000136
&CCN_ALTGPR(15) SETB 1                                                   000136
&CCN_ALTGPR(16) SETB 1                                                   000136
         MYPROLOG                                                        000136
@@BGN@1  DS    0H                                                        000136
         USING @@AUTO@1,13                                               000136
         LARL  3,@@LIT@1                                                 000136
         USING @@LIT@1,3                                                 000136
         STMH  14,6,100(13)                                              000136
         ST    1,96(,13)               #SR_PARM_1                        000136
* {                                                                      000137
*  int rc = 0;                                                           000138
         LA    14,0                                                      000138
         ST    14,@24rc@11                                               000138
*  void *addr;                                                           000139
*  long len;                                                             000140
*  long sp;                                                              000141
*                                                                        000142
*  addr = address;                                                       000143
         L     14,96(,13)              #SR_PARM_1                        000143
         USING @@PARMD@1,14                                              000143
         L     14,@20address                                             000143
         ST    14,@25addr@12                                             000143
*  len = *length;                                                        000144
         L     14,96(,13)              #SR_PARM_1                        000144
         L     14,@21length@8                                            000144
         L     14,0(,14)               (*)long                           000144
         ST    14,@26len                                                 000144
*  sp = *subpool;                                                        000145
         L     14,96(,13)              #SR_PARM_1                        000145
         L     14,@22subpool@9                                           000145
         L     6,0(,14)                (*)int                            000145
         ST    6,@27sp@13                                                000145
*                                                                        000146
*  __asm(" STORAGE RELEASE,COND=YES,ADDR=(%1),LENGTH=(%2),SP=(%3),RTCD=  000147
         L     4,@25addr@12                                              000147
         L     5,@26len                                                  000147
         STORAGE RELEASE,COND=YES,ADDR=(4),LENGTH=(5),SP=(6),RTCD=(2)    000147
         LR    15,2                                                      000147
         ST    15,@24rc@11                                               000147
*    :"=r"(rc):"r"(addr),"r"(len),"r"(sp));                              000148
*                                                                        000149
*  return rc;                                                            000150
*                                                                        000151
* }                                                                      000152
@1L9     DS    0H                                                        000152
         LMH   14,6,100(13)                                              000152
         DROP                                                            000152
         MYEPILOG                                                        000152
J9STORAGE CSECT ,                                                        000152
         DS    0F                                                        000152
@@LIT@1  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@1  DS    0F                      Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111000000011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@1)      Signed Offset to Prefix Data      000000
         DC    BL1'00000000'           Flag Set 1                        000000
         DC    BL1'10000000'           Flag Set 2                        000000
         DC    BL1'01000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL2'0000'               Saved FPR Mask                    000000
         DC    BL2'1111111000000011'   Saved HGPR Mask                   000000
         DC    XL4'00000064'           HGPR Save Area Offset             000000
         DC    AL2(23)                                                   000000
         DC    C'j9free_memory_below_bar'                                000000
J9STORAGE LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@1 DSECT                                                           000000
         DS    42F                                                       000000
         ORG   @@AUTO@1                                                  000000
#GPR_SA_1 DS   18F                                                       000000
         DS    F                                                         000000
         ORG   @@AUTO@1+80                                               000000
@24rc@11 DS    F                                                         000000
         ORG   @@AUTO@1+84                                               000000
@25addr@12 DS  A                                                         000000
         ORG   @@AUTO@1+88                                               000000
@26len   DS    F                                                         000000
         ORG   @@AUTO@1+92                                               000000
@27sp@13 DS    F                                                         000000
         ORG   @@AUTO@1+96                                               000000
#SR_PARM_1 DS  XL4                                                       000000
@@PARMD@1 DSECT                                                          000000
         DS    XL12                                                      000000
         ORG   @@PARMD@1+0                                               000000
@20address DS  F                                                         000000
         ORG   @@PARMD@1+4                                               000000
@21length@8 DS F                                                         000000
         ORG   @@PARMD@1+8                                               000000
@22subpool@9 DS F                                                        000000
         EJECT                                                           000000
J9STORAGE CSECT ,                                                        000000
* j9allocate_1M_pageable_pages_below_bar(long *numBytes, int *subpool)   000070
         ENTRY @@CCN@1                                                   000070
@@CCN@1  AMODE 31                                                        000070
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000070
         DC    A(@@FPB@3-*+8)          Signed offset to FPB              000070
         DC    XL4'00000000'           Reserved                          000070
@@CCN@1  DS    0F                                                        000070
&CCN_PRCN SETC '@@CCN@1'                                                 000070
&CCN_PRCN_LONG SETC 'j9allocate_1M_pageable_pages_below_bar'             000070
&CCN_LITN SETC '@@LIT@3'                                                 000070
&CCN_BEGIN SETC '@@BGN@3'                                                000070
&CCN_ASCM SETC 'P'                                                       000070
&CCN_DSASZ SETA 168                                                      000070
&CCN_SASZ SETA 72                                                        000070
&CCN_ARGS SETA 2                                                         000070
&CCN_RLOW SETA 14                                                        000070
&CCN_RHIGH SETA 6                                                        000070
&CCN_NAB SETB  0                                                         000070
&CCN_MAIN SETB 0                                                         000070
&CCN_NAB_STORED SETB 0                                                   000070
&CCN_STATIC SETB 0                                                       000070
&CCN_ALTGPR(1) SETB 1                                                    000070
&CCN_ALTGPR(2) SETB 1                                                    000070
&CCN_ALTGPR(3) SETB 1                                                    000070
&CCN_ALTGPR(4) SETB 1                                                    000070
&CCN_ALTGPR(5) SETB 1                                                    000070
&CCN_ALTGPR(6) SETB 1                                                    000070
&CCN_ALTGPR(7) SETB 1                                                    000070
&CCN_ALTGPR(8) SETB 0                                                    000070
&CCN_ALTGPR(9) SETB 0                                                    000070
&CCN_ALTGPR(10) SETB 0                                                   000070
&CCN_ALTGPR(11) SETB 0                                                   000070
&CCN_ALTGPR(12) SETB 0                                                   000070
&CCN_ALTGPR(13) SETB 0                                                   000070
&CCN_ALTGPR(14) SETB 1                                                   000070
&CCN_ALTGPR(15) SETB 1                                                   000070
&CCN_ALTGPR(16) SETB 1                                                   000070
         MYPROLOG                                                        000070
@@BGN@3  DS    0H                                                        000070
         USING @@AUTO@3,13                                               000070
         LARL  3,@@LIT@3                                                 000070
         USING @@LIT@3,3                                                 000070
         STMH  14,6,104(13)                                              000070
         ST    1,96(,13)               #SR_PARM_3                        000070
* {                                                                      000071
*  long length;                                                          000072
*  long sp;                                                              000073
*  long addr;                                                            000074
*  int rc = 0;                                                           000075
         LA    14,0                                                      000075
         ST    14,@5rc                                                   000075
*                                                                        000076
*  length = *numBytes;                                                   000077
         L     14,96(,13)              #SR_PARM_3                        000077
         USING @@PARMD@3,14                                              000077
         L     14,@2numBytes                                             000077
         L     14,0(,14)               (*)long                           000077
         ST    14,@7length                                               000077
*  sp = *subpool;                                                        000078
         L     14,96(,13)              #SR_PARM_3                        000078
         L     14,@3subpool                                              000078
         L     6,0(,14)                (*)int                            000078
         ST    6,@9sp                                                    000078
*                                                                        000079
*  __asm(" STORAGE OBTAIN,COND=YES,LOC=(31,PAGEFRAMESIZE1MB),"\          000080
         L     5,@7length                                                000080
         STORAGE OBTAIN,COND=YES,LOC=(31,PAGEFRAMESIZE1MB),LENGTH=(5),RX 000080
               TCD=(2),ADDR=(4),SP=(6)                                   000080
         LR    15,4                                                      000080
         LR    14,2                                                      000080
         ST    15,100(,13)             #wtemp_1                          000080
         ST    14,@5rc                                                   000080
         L     14,100(,13)             #wtemp_1                          000080
         ST    14,@10addr                                                000080
*    "LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\                          000081
*    :"=r"(rc),"=r"(addr):"r"(length),"r"(sp));                          000082
*                                                                        000083
*  if (0 == rc) {                                                        000084
         L     14,@5rc                                                   000084
         LTR   14,14                                                     000084
         BRNE  @3L5                                                      000084
*   return (void *)addr;                                                 000085
         L     15,@10addr                                                000085
         BRU   @3L7                                                      000085
@3L5     DS    0H                                                        000085
*  } else {                                                              000086
*   return (void *)0;                                                    000087
         LA    15,0                                                      000087
         BRU   @3L7                                                      000087
@3L6     DS    0H                                                        000087
*  }                                                                     000088
* }                                                                      000089
@3L7     DS    0H                                                        000089
         LMH   14,6,104(13)                                              000089
         DROP                                                            000089
         MYEPILOG                                                        000089
J9STORAGE CSECT ,                                                        000089
         DS    0F                                                        000089
@@LIT@3  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@3  DS    0F                      Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111000000011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@3)      Signed Offset to Prefix Data      000000
         DC    BL1'00000000'           Flag Set 1                        000000
         DC    BL1'10000000'           Flag Set 2                        000000
         DC    BL1'01000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL2'0000'               Saved FPR Mask                    000000
         DC    BL2'1111111000000011'   Saved HGPR Mask                   000000
         DC    XL4'00000068'           HGPR Save Area Offset             000000
         DC    AL2(38)                                                   000000
         DC    C'j9allocate_1M_pageable_pages_below_bar'                 000000
J9STORAGE LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@3 DSECT                                                           000000
         DS    42F                                                       000000
         ORG   @@AUTO@3                                                  000000
#GPR_SA_3 DS   18F                                                       000000
         DS    F                                                         000000
         ORG   @@AUTO@3+80                                               000000
@7length DS    F                                                         000000
         ORG   @@AUTO@3+84                                               000000
@9sp     DS    F                                                         000000
         ORG   @@AUTO@3+88                                               000000
@10addr  DS    F                                                         000000
         ORG   @@AUTO@3+92                                               000000
@5rc     DS    F                                                         000000
         ORG   @@AUTO@3+96                                               000000
#SR_PARM_3 DS  XL4                                                       000000
@@PARMD@3 DSECT                                                          000000
         DS    XL8                                                       000000
         ORG   @@PARMD@3+0                                               000000
@2numBytes DS  F                                                         000000
         ORG   @@PARMD@3+4                                               000000
@3subpool DS   F                                                         000000
         EJECT                                                           000000
J9STORAGE CSECT ,                                                        000000
* j9allocate_4K_pages_below_bar(long *numBytes, int *subpool)            000104
         ENTRY @@CCN@11                                                  000104
@@CCN@11 AMODE 31                                                        000104
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000104
         DC    A(@@FPB@2-*+8)          Signed offset to FPB              000104
         DC    XL4'00000000'           Reserved                          000104
@@CCN@11 DS    0F                                                        000104
&CCN_PRCN SETC '@@CCN@11'                                                000104
&CCN_PRCN_LONG SETC 'j9allocate_4K_pages_below_bar'                      000104
&CCN_LITN SETC '@@LIT@2'                                                 000104
&CCN_BEGIN SETC '@@BGN@2'                                                000104
&CCN_ASCM SETC 'P'                                                       000104
&CCN_DSASZ SETA 168                                                      000104
&CCN_SASZ SETA 72                                                        000104
&CCN_ARGS SETA 2                                                         000104
&CCN_RLOW SETA 14                                                        000104
&CCN_RHIGH SETA 6                                                        000104
&CCN_NAB SETB  0                                                         000104
&CCN_MAIN SETB 0                                                         000104
&CCN_NAB_STORED SETB 0                                                   000104
&CCN_STATIC SETB 0                                                       000104
&CCN_ALTGPR(1) SETB 1                                                    000104
&CCN_ALTGPR(2) SETB 1                                                    000104
&CCN_ALTGPR(3) SETB 1                                                    000104
&CCN_ALTGPR(4) SETB 1                                                    000104
&CCN_ALTGPR(5) SETB 1                                                    000104
&CCN_ALTGPR(6) SETB 1                                                    000104
&CCN_ALTGPR(7) SETB 1                                                    000104
&CCN_ALTGPR(8) SETB 0                                                    000104
&CCN_ALTGPR(9) SETB 0                                                    000104
&CCN_ALTGPR(10) SETB 0                                                   000104
&CCN_ALTGPR(11) SETB 0                                                   000104
&CCN_ALTGPR(12) SETB 0                                                   000104
&CCN_ALTGPR(13) SETB 0                                                   000104
&CCN_ALTGPR(14) SETB 1                                                   000104
&CCN_ALTGPR(15) SETB 1                                                   000104
&CCN_ALTGPR(16) SETB 1                                                   000104
         MYPROLOG                                                        000104
@@BGN@2  DS    0H                                                        000104
         USING @@AUTO@2,13                                               000104
         LARL  3,@@LIT@2                                                 000104
         USING @@LIT@2,3                                                 000104
         STMH  14,6,104(13)                                              000104
         ST    1,96(,13)               #SR_PARM_2                        000104
* {                                                                      000105
*  long length;                                                          000106
*  long sp;                                                              000107
*  long addr;                                                            000108
*  int rc = 0;                                                           000109
         LA    14,0                                                      000109
         ST    14,@15rc@7                                                000109
*                                                                        000110
*  length = *numBytes;                                                   000111
         L     14,96(,13)              #SR_PARM_2                        000111
         USING @@PARMD@2,14                                              000111
         L     14,@12numBytes@1                                          000111
         L     14,0(,14)               (*)long                           000111
         ST    14,@16length@4                                            000111
*  sp = *subpool;                                                        000112
         L     14,96(,13)              #SR_PARM_2                        000112
         L     14,@13subpool@2                                           000112
         L     6,0(,14)                (*)int                            000112
         ST    6,@17sp@5                                                 000112
*                                                                        000113
*  __asm(" STORAGE OBTAIN,COND=YES,LOC=(31,64),BNDRY=PAGE,"\             000114
         L     5,@16length@4                                             000114
         STORAGE OBTAIN,COND=YES,LOC=(31,64),BNDRY=PAGE,LENGTH=(5),RTCDX 000114
               =(2),ADDR=(4),SP=(6)                                      000114
         LR    15,4                                                      000114
         LR    14,2                                                      000114
         ST    15,100(,13)             #wtemp_2                          000114
         ST    14,@15rc@7                                                000114
         L     14,100(,13)             #wtemp_2                          000114
         ST    14,@18addr@6                                              000114
*    "LENGTH=(%2),RTCD=(%0),ADDR=(%1),SP=(%3)"\                          000115
*    :"=r"(rc),"=r"(addr):"r"(length),"r"(sp));                          000116
*                                                                        000117
*  if (0 == rc) {                                                        000118
         L     14,@15rc@7                                                000118
         LTR   14,14                                                     000118
         BRNE  @2L2                                                      000118
*   return (void *)addr;                                                 000119
         L     15,@18addr@6                                              000119
         BRU   @2L8                                                      000119
@2L2     DS    0H                                                        000119
*  } else {                                                              000120
*   return (void *)0;                                                    000121
         LA    15,0                                                      000121
         BRU   @2L8                                                      000121
@2L3     DS    0H                                                        000121
*  }                                                                     000122
* }                                                                      000123
@2L8     DS    0H                                                        000123
         LMH   14,6,104(13)                                              000123
         DROP                                                            000123
         MYEPILOG                                                        000123
J9STORAGE CSECT ,                                                        000123
         DS    0F                                                        000123
@@LIT@2  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@2  DS    0F                      Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111000000011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@2)      Signed Offset to Prefix Data      000000
         DC    BL1'00000000'           Flag Set 1                        000000
         DC    BL1'10000000'           Flag Set 2                        000000
         DC    BL1'01000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL2'0000'               Saved FPR Mask                    000000
         DC    BL2'1111111000000011'   Saved HGPR Mask                   000000
         DC    XL4'00000068'           HGPR Save Area Offset             000000
         DC    AL2(29)                                                   000000
         DC    C'j9allocate_4K_pages_below_bar'                          000000
J9STORAGE LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@2 DSECT                                                           000000
         DS    42F                                                       000000
         ORG   @@AUTO@2                                                  000000
#GPR_SA_2 DS   18F                                                       000000
         DS    F                                                         000000
         ORG   @@AUTO@2+80                                               000000
@16length@4 DS F                                                         000000
         ORG   @@AUTO@2+84                                               000000
@17sp@5  DS    F                                                         000000
         ORG   @@AUTO@2+88                                               000000
@18addr@6 DS   F                                                         000000
         ORG   @@AUTO@2+92                                               000000
@15rc@7  DS    F                                                         000000
         ORG   @@AUTO@2+96                                               000000
#SR_PARM_2 DS  XL4                                                       000000
@@PARMD@2 DSECT                                                          000000
         DS    XL8                                                       000000
         ORG   @@PARMD@2+0                                               000000
@12numBytes@1 DS F                                                       000000
         ORG   @@PARMD@2+4                                               000000
@13subpool@2 DS F                                                        000000
*                                                                        000153
         END   ,(5650ZOS   ,2100,16060)                                  000000
