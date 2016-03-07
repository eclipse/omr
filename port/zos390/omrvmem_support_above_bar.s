***********************************************************************
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
*    Multiple authors (IBM Corp.) - initial API and implementation
*    and/or initial documentation
***********************************************************************

         TITLE 'omrvmem_support_above_bar'

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
*** This file also includes multiple HLASM calls to IARV64 HLASM 
* 		macro
*		- These calls were generated using the METAL-C compiler
*		- See j9iarv64.c for details/instructions.
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
* Insert contents of j9iarv64.s below
**************************************************
*
         ACONTROL AFPR                                                   000000
J9IARV64 CSECT                                                           000000
J9IARV64 AMODE 64                                                        000000
J9IARV64 RMODE ANY                                                       000000
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
&CCN_LP64 SETB 1                                                         000000
&CCN_RENT SETB 0                                                         000000
&CCN_APARSE SETB 1                                                       000000
&CCN_CSECT SETC 'J9IARV64'                                               000000
&CCN_ARCHLVL SETC '7'                                                    000000
         SYSSTATE ARCHLVL=2,AMODE64=YES                                  000000
         IEABRCX DEFINE                                                  000000
.* The HLASM GOFF option is needed to assemble this program              000000
@@CCN@62 ALIAS C'j9discard_data'                                         000000
@@CCN@54 ALIAS C'j9free_memory_above_bar'                                000000
@@CCN@45 ALIAS C'j9allocate_4K_pages_above_bar'                          000000
@@CCN@36 ALIAS C'j9allocate_4K_pages_in_2to32G_area'                     000000
@@CCN@25 ALIAS C'j9allocate_2G_pages'                                    000000
@@CCN@14 ALIAS C'j9allocate_1M_pageable_pages_above_bar'                 000000
@@CCN@2  ALIAS C'j9allocate_1M_fixed_pages'                              000000
* /********************************************************************  000001
*  *                                                                     000002
*  * (c) Copyright IBM Corp. 1991, 2016                                  000003
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
*  * that use the IARV64 macro in omrvmem.c                              000021
*  *                                                                     000022
*  * This file is compiled manually using the METAL-C compiler that was  000023
*  * introduced in z/OS V1R9. The generated output (j9iarv64.s) is then  000024
*  * inserted into omrvmem_support_above_bar.s which is compiled by our  000025
*  *                                                                     000026
*  * omrvmem_support_above_bar.s indicates where to put the contents of  000027
*  * Search for:                                                         000028
*  *   Insert contents of j9iarv64.s below                               000029
*  *                                                                     000030
*  * *******                                                             000031
*  * NOTE!!!!! You must strip the line numbers from any pragma statemen  000032
*  * *******                                                             000033
*  *                                                                     000034
*  * It should be obvious, however, just to be clear be sure to omit th  000035
*  * first two lines from j9iarv64.s which will look something like:     000036
*  *                                                                     000037
*  *          TITLE '5694A01 V1.9 z/OS XL C                              000038
*  *                     ./j9iarv64.c'                                   000039
*  *                                                                     000040
*  * To compile:                                                         000041
*  *  xlc -S -qmetal -Wc,lp64 -qlongname j9iarv64.c                      000042
*  *                                                                     000043
*  * z/OS V1R9 z/OS V1R9.0 Metal C Programming Guide and Reference:      000044
*  *   http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf             000045
*  */                                                                    000046
*                                                                        000047
* #pragma prolog(j9allocate_1M_fixed_pages,"MYPROLOG")                   000048
* #pragma epilog(j9allocate_1M_fixed_pages,"MYEPILOG")                   000049
*                                                                        000050
* __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));          000051
*                                                                        000052
* /*                                                                     000053
*  * Allocate 1MB fixed pages using IARV64 system macro.                 000054
*  * Memory allocated is freed using j9free_memory_above_bar().          000055
*  *                                                                     000056
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000057
*  * @params[in] use2To32GArea 1 if memory request in for 2G-32G range,  000058
*  *                                                                     000059
*  * @return pointer to memory allocated, NULL on failure.               000060
*  */                                                                    000061
* void * j9allocate_1M_fixed_pages(int *numMBSegments, int *use2To32GAr  000062
*  long segments;                                                        000063
*  long origin;                                                          000064
*  long use2To32G = *use2To32GArea;                                      000065
*  int  iarv64_rc = 0;                                                   000066
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000067
*                                                                        000068
*  segments = *numMBSegments;                                            000069
*  wgetstor = lgetstor;                                                  000070
*                                                                        000071
*  if (0 == use2To32G) {                                                 000072
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000073
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000074
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000075
*  } else {                                                              000076
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000077
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000078
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000079
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000080
*  }                                                                     000081
*  if (0 != iarv64_rc) {                                                 000082
*   return (void *)0;                                                    000083
*  }                                                                     000084
*  return (void *)origin;                                                000085
* }                                                                      000086
*                                                                        000087
* #pragma prolog(j9allocate_1M_pageable_pages_above_bar,"MYPROLOG")      000088
* #pragma epilog(j9allocate_1M_pageable_pages_above_bar,"MYEPILOG")      000089
*                                                                        000090
* __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));          000091
*                                                                        000092
* /*                                                                     000093
*  * Allocate 1MB pageable pages above 2GB bar using IARV64 system macr  000094
*  * Memory allocated is freed using j9free_memory_above_bar().          000095
*  *                                                                     000096
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000097
*  * @params[in] use2To32GArea 1 if memory request in for 2G-32G range,  000098
*  *                                                                     000099
*  * @return pointer to memory allocated, NULL on failure.               000100
*  */                                                                    000101
* void * j9allocate_1M_pageable_pages_above_bar(int *numMBSegments, int  000102
*  long segments;                                                        000103
*  long origin;                                                          000104
*  long use2To32G = *use2To32GArea;                                      000105
*  int  iarv64_rc = 0;                                                   000106
*                                                                        000107
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000108
*                                                                        000109
*  segments = *numMBSegments;                                            000110
*  wgetstor = ngetstor;                                                  000111
*                                                                        000112
*  if (0 == use2To32G) {                                                 000113
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000114
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000115
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000116
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000117
*  } else {                                                              000118
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000119
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000120
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000121
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000122
*  }                                                                     000123
*                                                                        000124
*  if (0 != iarv64_rc) {                                                 000125
*   return (void *)0;                                                    000126
*  }                                                                     000127
*  return (void *)origin;                                                000128
* }                                                                      000129
*                                                                        000130
* #pragma prolog(j9allocate_2G_pages,"MYPROLOG")                         000131
* #pragma epilog(j9allocate_2G_pages,"MYEPILOG")                         000132
*                                                                        000133
* __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));          000134
*                                                                        000135
* /*                                                                     000136
*  * Allocate 2GB fixed pages using IARV64 system macro.                 000137
*  * Memory allocated is freed using j9free_memory_above_bar().          000138
*  *                                                                     000139
*  * @params[in] num2GBUnits Number of 2GB units to be allocated         000140
*  * @params[in] use2To32GArea 1 if memory request in for 2G-32G range,  000141
*  *                                                                     000142
*  * @return pointer to memory allocated, NULL on failure.               000143
*  */                                                                    000144
* void * j9allocate_2G_pages(int *num2GBUnits, int *use2To32GArea, cons  000145
*  long units;                                                           000146
*  long origin;                                                          000147
*  long use2To32G = *use2To32GArea;                                      000148
*  int  iarv64_rc = 0;                                                   000149
*                                                                        000150
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000151
*                                                                        000152
*  units = *num2GBUnits;                                                 000153
*  wgetstor = ogetstor;                                                  000154
*                                                                        000155
*  if (0 == use2To32G) {                                                 000156
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000157
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000158
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000159
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000160
*  } else {                                                              000161
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000162
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000163
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000164
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000165
*  }                                                                     000166
*                                                                        000167
*  if (0 != iarv64_rc) {                                                 000168
*   return (void *)0;                                                    000169
*  }                                                                     000170
*  return (void *)origin;                                                000171
* }                                                                      000172
*                                                                        000173
* #pragma prolog(j9allocate_4K_pages_in_2to32G_area,"MYPROLOG")          000174
* #pragma epilog(j9allocate_4K_pages_in_2to32G_area,"MYEPILOG")          000175
*                                                                        000176
* __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));          000177
*                                                                        000178
* /*                                                                     000179
*  * Allocate 4KB pages in 2G-32G range using IARV64 system macro.       000180
*  * Memory allocated is freed using j9free_memory_above_bar().          000181
*  *                                                                     000182
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000183
*  *                                                                     000184
*  * @return pointer to memory allocated, NULL on failure.               000185
*  */                                                                    000186
* void * j9allocate_4K_pages_in_2to32G_area(int *numMBSegments, const c  000187
*  long segments;                                                        000188
*  long origin;                                                          000189
*  int  iarv64_rc = 0;                                                   000190
*                                                                        000191
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000192
*                                                                        000193
*  segments = *numMBSegments;                                            000194
*  wgetstor = mgetstor;                                                  000195
*                                                                        000196
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\    000197
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000198
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000199
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000200
*                                                                        000201
*  if (0 != iarv64_rc) {                                                 000202
*   return (void *)0;                                                    000203
*  }                                                                     000204
*  return (void *)origin;                                                000205
* }                                                                      000206
*                                                                        000207
* #pragma prolog(j9allocate_4K_pages_above_bar,"MYPROLOG")               000208
* #pragma epilog(j9allocate_4K_pages_above_bar,"MYEPILOG")               000209
*                                                                        000210
* __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));          000211
*                                                                        000212
* /*                                                                     000213
*  * Allocate 4KB pages using IARV64 system macro.                       000214
*  * Memory allocated is freed using j9free_memory_above_bar().          000215
*  *                                                                     000216
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000217
*  *                                                                     000218
*  * @return pointer to memory allocated, NULL on failure.               000219
*  */                                                                    000220
* void * j9allocate_4K_pages_above_bar(int *numMBSegments, const char *  000221
*  long segments;                                                        000222
*  long origin;                                                          000223
*  int  iarv64_rc = 0;                                                   000224
*                                                                        000225
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000226
*                                                                        000227
*  segments = *numMBSegments;                                            000228
*  wgetstor = rgetstor;                                                  000229
*                                                                        000230
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000231
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000232
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000233
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000234
*                                                                        000235
*  if (0 != iarv64_rc) {                                                 000236
*   return (void *)0;                                                    000237
*  }                                                                     000238
*  return (void *)origin;                                                000239
* }                                                                      000240
*                                                                        000241
* #pragma prolog(j9free_memory_above_bar,"MYPROLOG")                     000242
* #pragma epilog(j9free_memory_above_bar,"MYEPILOG")                     000243
*                                                                        000244
* __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));          000245
*                                                                        000246
* /*                                                                     000247
*  * Free memory allocated using IARV64 system macro.                    000248
*  *                                                                     000249
*  * @params[in] address pointer to memory region to be freed            000250
*  *                                                                     000251
*  * @return non-zero if memory is not freed successfully, 0 otherwise.  000252
*  */                                                                    000253
* int j9free_memory_above_bar(void *address, const char * ttkn){         000254
*  void * xmemobjstart;                                                  000255
*  int  iarv64_rc = 0;                                                   000256
*                                                                        000257
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000258
*                                                                        000259
*  xmemobjstart = address;                                               000260
*  wgetstor = pgetstor;                                                  000261
*                                                                        000262
*  __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),  000263
*    ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));      000264
*  return iarv64_rc;                                                     000265
* }                                                                      000266
*                                                                        000267
* #pragma prolog(j9discard_data,"MYPROLOG")                              000268
* #pragma epilog(j9discard_data,"MYEPILOG")                              000269
*                                                                        000270
* __asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));          000271
*                                                                        000272
* /* Used to pass parameters to IARV64 DISCARDDATA in j9discard_data().  000273
* struct rangeList {                                                     000274
*  void *startAddr;                                                      000275
*  long pageCount;                                                       000276
* };                                                                     000277
*                                                                        000278
* /*                                                                     000279
*  * Discard memory allocated using IARV64 system macro.                 000280
*  *                                                                     000281
*  * @params[in] address pointer to memory region to be discarded        000282
*  * @params[in] numFrames number of frames to be discarded. Frame size  000283
*  *                                                                     000284
*  * @return non-zero if memory is not discarded successfully, 0 otherw  000285
*  */                                                                    000286
* int j9discard_data(void *address, int *numFrames) {                    000287
         J     @@CCN@62                                                  000287
@@PFD@@  DC    XL8'00C300C300D50000'   Prefix Data Marker                000287
         DC    CL8'20160229'           Compiled Date YYYYMMDD            000287
         DC    CL6'100017'             Compiled Time HHMMSS              000287
         DC    XL4'42010000'           Compiler Version                  000287
         DC    XL2'0000'               Reserved                          000287
         DC    BL1'00000000'           Flag Set 1                        000287
         DC    BL1'00000000'           Flag Set 2                        000287
         DC    BL1'00000000'           Flag Set 3                        000287
         DC    BL1'00000000'           Flag Set 4                        000287
         DC    XL4'00000000'           Reserved                          000287
         ENTRY @@CCN@62                                                  000287
@@CCN@62 AMODE 64                                                        000287
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000287
         DC    A(@@FPB@1-*+8)          Signed offset to FPB              000287
         DC    XL4'00000000'           Reserved                          000287
@@CCN@62 DS    0FD                                                       000287
&CCN_PRCN SETC '@@CCN@62'                                                000287
&CCN_PRCN_LONG SETC 'j9discard_data'                                     000287
&CCN_LITN SETC '@@LIT@1'                                                 000287
&CCN_BEGIN SETC '@@BGN@1'                                                000287
&CCN_ASCM SETC 'P'                                                       000287
&CCN_DSASZ SETA 472                                                      000287
&CCN_SASZ SETA 144                                                       000287
&CCN_ARGS SETA 2                                                         000287
&CCN_RLOW SETA 14                                                        000287
&CCN_RHIGH SETA 10                                                       000287
&CCN_NAB SETB  0                                                         000287
&CCN_MAIN SETB 0                                                         000287
&CCN_NAB_STORED SETB 0                                                   000287
&CCN_STATIC SETB 0                                                       000287
&CCN_ALTGPR(1) SETB 1                                                    000287
&CCN_ALTGPR(2) SETB 1                                                    000287
&CCN_ALTGPR(3) SETB 1                                                    000287
&CCN_ALTGPR(4) SETB 1                                                    000287
&CCN_ALTGPR(5) SETB 1                                                    000287
&CCN_ALTGPR(6) SETB 0                                                    000287
&CCN_ALTGPR(7) SETB 0                                                    000287
&CCN_ALTGPR(8) SETB 0                                                    000287
&CCN_ALTGPR(9) SETB 0                                                    000287
&CCN_ALTGPR(10) SETB 0                                                   000287
&CCN_ALTGPR(11) SETB 1                                                   000287
&CCN_ALTGPR(12) SETB 0                                                   000287
&CCN_ALTGPR(13) SETB 0                                                   000287
&CCN_ALTGPR(14) SETB 1                                                   000287
&CCN_ALTGPR(15) SETB 1                                                   000287
&CCN_ALTGPR(16) SETB 1                                                   000287
         MYPROLOG                                                        000287
@@BGN@1  DS    0H                                                        000287
         AIF   (NOT &CCN_SASIG).@@NOSIG1                                 000287
         LLILH 9,X'C6F4'                                                 000287
         OILL  9,X'E2C1'                                                 000287
         ST    9,4(,13)                                                  000287
.@@NOSIG1 ANOP                                                           000287
         USING @@AUTO@1,13                                               000287
         LARL  3,@@LIT@1                                                 000287
         USING @@LIT@1,3                                                 000287
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_1                        000287
*  struct rangeList range;                                               000288
*  void *rangePtr;                                                       000289
*  int iarv64_rc = 0;                                                    000290
         LGHI  14,0                                                      000290
         ST    14,@66iarv64_rc@32                                        000290
*                                                                        000291
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000292
*                                                                        000293
*  range.startAddr = address;                                            000294
         LG    14,464(0,13)            #SR_PARM_1                        000294
         USING @@PARMD@1,14                                              000294
         LG    14,@63address@30                                          000294
         STG   14,176(0,13)            range_rangeList_startAddr         000294
*  range.pageCount = *numFrames;                                         000295
         LG    14,464(0,13)            #SR_PARM_1                        000295
         LG    14,@64numFrames                                           000295
         LGF   14,0(0,14)              (*)int                            000295
         STG   14,184(0,13)            range_rangeList_pageCount         000295
*  rangePtr = (void *)&range;                                            000296
         LA    14,@68range                                               000296
         DROP  10                                                        000296
         STG   14,@71rangePtr                                            000296
*  wgetstor = qgetstor;                                                  000297
         MVC   @67wgetstor,1536(10)                                      000297
*                                                                        000298
*  __asm(" IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,"\                     000299
         LA    2,@71rangePtr                                             000299
         LA    4,@67wgetstor                                             000299
         IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,RANGLIST=(2),RETCODE=20X 000299
               0(13),MF=(E,(4))                                          000299
*    "RANGLIST=(%1),RETCODE=%0,MF=(E,(%2))"\                             000300
*    ::"m"(iarv64_rc),"r"(&rangePtr),"r"(&wgetstor));                    000301
*                                                                        000302
*  return iarv64_rc;                                                     000303
         LGF   15,@66iarv64_rc@32                                        000303
* }                                                                      000304
@1L24    DS    0H                                                        000304
         DROP                                                            000304
         MYEPILOG                                                        000304
J9IARV64 CSECT ,                                                         000304
         DS    0FD                                                       000304
@@LIT@1  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@1  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@1)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(14)                                                   000000
         DC    C'j9discard_data'                                         000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@1 DSECT                                                           000000
         DS    59FD                                                      000000
         ORG   @@AUTO@1                                                  000000
#GPR_SA_1 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@1+176                                              000000
@68range DS    XL16                                                      000000
         ORG   @@AUTO@1+192                                              000000
@71rangePtr DS AD                                                        000000
         ORG   @@AUTO@1+200                                              000000
@66iarv64_rc@32 DS F                                                     000000
         ORG   @@AUTO@1+208                                              000000
@67wgetstor DS XL256                                                     000000
         ORG   @@AUTO@1+464                                              000000
#SR_PARM_1 DS  XL8                                                       000000
@@PARMD@1 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@1+0                                               000000
@63address@30 DS FD                                                      000000
         ORG   @@PARMD@1+8                                               000000
@64numFrames DS FD                                                       000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* void * j9allocate_1M_fixed_pages(int *numMBSegments, int *use2To32GAr  000062
         ENTRY @@CCN@2                                                   000062
@@CCN@2  AMODE 64                                                        000062
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000062
         DC    A(@@FPB@7-*+8)          Signed offset to FPB              000062
         DC    XL4'00000000'           Reserved                          000062
@@CCN@2  DS    0FD                                                       000062
&CCN_PRCN SETC '@@CCN@2'                                                 000062
&CCN_PRCN_LONG SETC 'j9allocate_1M_fixed_pages'                          000062
&CCN_LITN SETC '@@LIT@7'                                                 000062
&CCN_BEGIN SETC '@@BGN@7'                                                000062
&CCN_ASCM SETC 'P'                                                       000062
&CCN_DSASZ SETA 472                                                      000062
&CCN_SASZ SETA 144                                                       000062
&CCN_ARGS SETA 3                                                         000062
&CCN_RLOW SETA 14                                                        000062
&CCN_RHIGH SETA 10                                                       000062
&CCN_NAB SETB  0                                                         000062
&CCN_MAIN SETB 0                                                         000062
&CCN_NAB_STORED SETB 0                                                   000062
&CCN_STATIC SETB 0                                                       000062
&CCN_ALTGPR(1) SETB 1                                                    000062
&CCN_ALTGPR(2) SETB 1                                                    000062
&CCN_ALTGPR(3) SETB 1                                                    000062
&CCN_ALTGPR(4) SETB 1                                                    000062
&CCN_ALTGPR(5) SETB 1                                                    000062
&CCN_ALTGPR(6) SETB 1                                                    000062
&CCN_ALTGPR(7) SETB 1                                                    000062
&CCN_ALTGPR(8) SETB 0                                                    000062
&CCN_ALTGPR(9) SETB 0                                                    000062
&CCN_ALTGPR(10) SETB 0                                                   000062
&CCN_ALTGPR(11) SETB 1                                                   000062
&CCN_ALTGPR(12) SETB 0                                                   000062
&CCN_ALTGPR(13) SETB 0                                                   000062
&CCN_ALTGPR(14) SETB 1                                                   000062
&CCN_ALTGPR(15) SETB 1                                                   000062
&CCN_ALTGPR(16) SETB 1                                                   000062
         MYPROLOG                                                        000062
@@BGN@7  DS    0H                                                        000062
         AIF   (NOT &CCN_SASIG).@@NOSIG7                                 000062
         LLILH 9,X'C6F4'                                                 000062
         OILL  9,X'E2C1'                                                 000062
         ST    9,4(,13)                                                  000062
.@@NOSIG7 ANOP                                                           000062
         USING @@AUTO@7,13                                               000062
         LARL  3,@@LIT@7                                                 000062
         USING @@LIT@7,3                                                 000062
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_7                        000062
*  long segments;                                                        000063
*  long origin;                                                          000064
*  long use2To32G = *use2To32GArea;                                      000065
         LG    14,464(0,13)            #SR_PARM_7                        000065
         USING @@PARMD@7,14                                              000065
         LG    14,@4use2To32GArea                                        000065
         LGF   14,0(0,14)              (*)int                            000065
         STG   14,@7use2To32G                                            000065
*  int  iarv64_rc = 0;                                                   000066
         LGHI  14,0                                                      000066
         ST    14,@9iarv64_rc                                            000066
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000067
*                                                                        000068
*  segments = *numMBSegments;                                            000069
         LG    14,464(0,13)            #SR_PARM_7                        000069
         LG    14,@3numMBSegments                                        000069
         LGF   14,0(0,14)              (*)int                            000069
         STG   14,@11segments                                            000069
*  wgetstor = lgetstor;                                                  000070
         MVC   @10wgetstor,@1lgetstor                                    000070
*                                                                        000071
*  if (0 == use2To32G) {                                                 000072
         LG    14,@7use2To32G                                            000072
         LTGR  14,14                                                     000072
         BRNE  @7L15                                                     000072
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000073
         LA    2,@12origin                                               000073
         DROP  10                                                        000073
         LA    4,@11segments                                             000073
         LA    5,@10wgetstor                                             000073
         LG    14,464(0,13)            #SR_PARM_7                        000073
         LG    6,@5ttkn                                                  000073
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000073
               AMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=X 000073
               200(13),MF=(E,(5))                                        000073
         BRU   @7L16                                                     000073
@7L15    DS    0H                                                        000073
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000074
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000075
*  } else {                                                              000076
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000077
         LA    2,@12origin                                               000077
         LA    4,@11segments                                             000077
         LA    5,@10wgetstor                                             000077
         LG    14,464(0,13)            #SR_PARM_7                        000077
         LG    6,@5ttkn                                                  000077
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000077
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000077
               EN=(6),RETCODE=200(13),MF=(E,(5))                         000077
@7L16    DS    0H                                                        000077
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000078
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000079
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000080
*  }                                                                     000081
*  if (0 != iarv64_rc) {                                                 000082
         LGF   14,@9iarv64_rc                                            000082
         LTR   14,14                                                     000082
         BRE   @7L17                                                     000082
*   return (void *)0;                                                    000083
         LGHI  15,0                                                      000083
         BRU   @7L18                                                     000083
@7L17    DS    0H                                                        000083
*  }                                                                     000084
*  return (void *)origin;                                                000085
         LG    15,@12origin                                              000085
* }                                                                      000086
@7L18    DS    0H                                                        000086
         DROP                                                            000086
         MYEPILOG                                                        000086
J9IARV64 CSECT ,                                                         000086
         DS    0FD                                                       000086
@@LIT@7  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@7  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@7)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(25)                                                   000000
         DC    C'j9allocate_1M_fixed_pages'                              000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@7 DSECT                                                           000000
         DS    59FD                                                      000000
         ORG   @@AUTO@7                                                  000000
#GPR_SA_7 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@7+176                                              000000
@11segments DS FD                                                        000000
         ORG   @@AUTO@7+184                                              000000
@12origin DS   FD                                                        000000
         ORG   @@AUTO@7+192                                              000000
@7use2To32G DS FD                                                        000000
         ORG   @@AUTO@7+200                                              000000
@9iarv64_rc DS F                                                         000000
         ORG   @@AUTO@7+208                                              000000
@10wgetstor DS XL256                                                     000000
         ORG   @@AUTO@7+464                                              000000
#SR_PARM_7 DS  XL8                                                       000000
@@PARMD@7 DSECT                                                          000000
         DS    XL24                                                      000000
         ORG   @@PARMD@7+0                                               000000
@3numMBSegments DS FD                                                    000000
         ORG   @@PARMD@7+8                                               000000
@4use2To32GArea DS FD                                                    000000
         ORG   @@PARMD@7+16                                              000000
@5ttkn   DS    FD                                                        000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* void * j9allocate_1M_pageable_pages_above_bar(int *numMBSegments, int  000102
         ENTRY @@CCN@14                                                  000102
@@CCN@14 AMODE 64                                                        000102
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000102
         DC    A(@@FPB@6-*+8)          Signed offset to FPB              000102
         DC    XL4'00000000'           Reserved                          000102
@@CCN@14 DS    0FD                                                       000102
&CCN_PRCN SETC '@@CCN@14'                                                000102
&CCN_PRCN_LONG SETC 'j9allocate_1M_pageable_pages_above_bar'             000102
&CCN_LITN SETC '@@LIT@6'                                                 000102
&CCN_BEGIN SETC '@@BGN@6'                                                000102
&CCN_ASCM SETC 'P'                                                       000102
&CCN_DSASZ SETA 472                                                      000102
&CCN_SASZ SETA 144                                                       000102
&CCN_ARGS SETA 3                                                         000102
&CCN_RLOW SETA 14                                                        000102
&CCN_RHIGH SETA 10                                                       000102
&CCN_NAB SETB  0                                                         000102
&CCN_MAIN SETB 0                                                         000102
&CCN_NAB_STORED SETB 0                                                   000102
&CCN_STATIC SETB 0                                                       000102
&CCN_ALTGPR(1) SETB 1                                                    000102
&CCN_ALTGPR(2) SETB 1                                                    000102
&CCN_ALTGPR(3) SETB 1                                                    000102
&CCN_ALTGPR(4) SETB 1                                                    000102
&CCN_ALTGPR(5) SETB 1                                                    000102
&CCN_ALTGPR(6) SETB 1                                                    000102
&CCN_ALTGPR(7) SETB 1                                                    000102
&CCN_ALTGPR(8) SETB 0                                                    000102
&CCN_ALTGPR(9) SETB 0                                                    000102
&CCN_ALTGPR(10) SETB 0                                                   000102
&CCN_ALTGPR(11) SETB 1                                                   000102
&CCN_ALTGPR(12) SETB 0                                                   000102
&CCN_ALTGPR(13) SETB 0                                                   000102
&CCN_ALTGPR(14) SETB 1                                                   000102
&CCN_ALTGPR(15) SETB 1                                                   000102
&CCN_ALTGPR(16) SETB 1                                                   000102
         MYPROLOG                                                        000102
@@BGN@6  DS    0H                                                        000102
         AIF   (NOT &CCN_SASIG).@@NOSIG6                                 000102
         LLILH 9,X'C6F4'                                                 000102
         OILL  9,X'E2C1'                                                 000102
         ST    9,4(,13)                                                  000102
.@@NOSIG6 ANOP                                                           000102
         USING @@AUTO@6,13                                               000102
         LARL  3,@@LIT@6                                                 000102
         USING @@LIT@6,3                                                 000102
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_6                        000102
*  long segments;                                                        000103
*  long origin;                                                          000104
*  long use2To32G = *use2To32GArea;                                      000105
         LG    14,464(0,13)            #SR_PARM_6                        000105
         USING @@PARMD@6,14                                              000105
         LG    14,@16use2To32GArea@2                                     000105
         LGF   14,0(0,14)              (*)int                            000105
         STG   14,@19use2To32G@7                                         000105
*  int  iarv64_rc = 0;                                                   000106
         LGHI  14,0                                                      000106
         ST    14,@20iarv64_rc@8                                         000106
*                                                                        000107
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000108
*                                                                        000109
*  segments = *numMBSegments;                                            000110
         LG    14,464(0,13)            #SR_PARM_6                        000110
         LG    14,@15numMBSegments@1                                     000110
         LGF   14,0(0,14)              (*)int                            000110
         STG   14,@22segments@5                                          000110
*  wgetstor = ngetstor;                                                  000111
         MVC   @21wgetstor,@13ngetstor                                   000111
*                                                                        000112
*  if (0 == use2To32G) {                                                 000113
         LG    14,@19use2To32G@7                                         000113
         LTGR  14,14                                                     000113
         BRNE  @6L11                                                     000113
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000114
         LA    2,@23origin@6                                             000114
         DROP  10                                                        000114
         LA    4,@22segments@5                                           000114
         LA    5,@21wgetstor                                             000114
         LG    14,464(0,13)            #SR_PARM_6                        000114
         LG    6,@17ttkn@3                                               000114
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000114
               AMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(4),ORIGIN=(X 000114
               2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))                  000114
         BRU   @6L12                                                     000114
@6L11    DS    0H                                                        000114
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000115
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000116
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000117
*  } else {                                                              000118
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000119
         LA    2,@23origin@6                                             000119
         LA    4,@22segments@5                                           000119
         LA    5,@21wgetstor                                             000119
         LG    14,464(0,13)            #SR_PARM_6                        000119
         LG    6,@17ttkn@3                                               000119
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000119
               O32G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000119
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))   000119
@6L12    DS    0H                                                        000119
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000120
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000121
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000122
*  }                                                                     000123
*                                                                        000124
*  if (0 != iarv64_rc) {                                                 000125
         LGF   14,@20iarv64_rc@8                                         000125
         LTR   14,14                                                     000125
         BRE   @6L13                                                     000125
*   return (void *)0;                                                    000126
         LGHI  15,0                                                      000126
         BRU   @6L19                                                     000126
@6L13    DS    0H                                                        000126
*  }                                                                     000127
*  return (void *)origin;                                                000128
         LG    15,@23origin@6                                            000128
* }                                                                      000129
@6L19    DS    0H                                                        000129
         DROP                                                            000129
         MYEPILOG                                                        000129
J9IARV64 CSECT ,                                                         000129
         DS    0FD                                                       000129
@@LIT@6  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@6  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@6)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(38)                                                   000000
         DC    C'j9allocate_1M_pageable_pages_above_bar'                 000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@6 DSECT                                                           000000
         DS    59FD                                                      000000
         ORG   @@AUTO@6                                                  000000
#GPR_SA_6 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@6+176                                              000000
@22segments@5 DS FD                                                      000000
         ORG   @@AUTO@6+184                                              000000
@23origin@6 DS FD                                                        000000
         ORG   @@AUTO@6+192                                              000000
@19use2To32G@7 DS FD                                                     000000
         ORG   @@AUTO@6+200                                              000000
@20iarv64_rc@8 DS F                                                      000000
         ORG   @@AUTO@6+208                                              000000
@21wgetstor DS XL256                                                     000000
         ORG   @@AUTO@6+464                                              000000
#SR_PARM_6 DS  XL8                                                       000000
@@PARMD@6 DSECT                                                          000000
         DS    XL24                                                      000000
         ORG   @@PARMD@6+0                                               000000
@15numMBSegments@1 DS FD                                                 000000
         ORG   @@PARMD@6+8                                               000000
@16use2To32GArea@2 DS FD                                                 000000
         ORG   @@PARMD@6+16                                              000000
@17ttkn@3 DS   FD                                                        000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* void * j9allocate_2G_pages(int *num2GBUnits, int *use2To32GArea, cons  000145
         ENTRY @@CCN@25                                                  000145
@@CCN@25 AMODE 64                                                        000145
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000145
         DC    A(@@FPB@5-*+8)          Signed offset to FPB              000145
         DC    XL4'00000000'           Reserved                          000145
@@CCN@25 DS    0FD                                                       000145
&CCN_PRCN SETC '@@CCN@25'                                                000145
&CCN_PRCN_LONG SETC 'j9allocate_2G_pages'                                000145
&CCN_LITN SETC '@@LIT@5'                                                 000145
&CCN_BEGIN SETC '@@BGN@5'                                                000145
&CCN_ASCM SETC 'P'                                                       000145
&CCN_DSASZ SETA 472                                                      000145
&CCN_SASZ SETA 144                                                       000145
&CCN_ARGS SETA 3                                                         000145
&CCN_RLOW SETA 14                                                        000145
&CCN_RHIGH SETA 10                                                       000145
&CCN_NAB SETB  0                                                         000145
&CCN_MAIN SETB 0                                                         000145
&CCN_NAB_STORED SETB 0                                                   000145
&CCN_STATIC SETB 0                                                       000145
&CCN_ALTGPR(1) SETB 1                                                    000145
&CCN_ALTGPR(2) SETB 1                                                    000145
&CCN_ALTGPR(3) SETB 1                                                    000145
&CCN_ALTGPR(4) SETB 1                                                    000145
&CCN_ALTGPR(5) SETB 1                                                    000145
&CCN_ALTGPR(6) SETB 1                                                    000145
&CCN_ALTGPR(7) SETB 1                                                    000145
&CCN_ALTGPR(8) SETB 0                                                    000145
&CCN_ALTGPR(9) SETB 0                                                    000145
&CCN_ALTGPR(10) SETB 0                                                   000145
&CCN_ALTGPR(11) SETB 1                                                   000145
&CCN_ALTGPR(12) SETB 0                                                   000145
&CCN_ALTGPR(13) SETB 0                                                   000145
&CCN_ALTGPR(14) SETB 1                                                   000145
&CCN_ALTGPR(15) SETB 1                                                   000145
&CCN_ALTGPR(16) SETB 1                                                   000145
         MYPROLOG                                                        000145
@@BGN@5  DS    0H                                                        000145
         AIF   (NOT &CCN_SASIG).@@NOSIG5                                 000145
         LLILH 9,X'C6F4'                                                 000145
         OILL  9,X'E2C1'                                                 000145
         ST    9,4(,13)                                                  000145
.@@NOSIG5 ANOP                                                           000145
         USING @@AUTO@5,13                                               000145
         LARL  3,@@LIT@5                                                 000145
         USING @@LIT@5,3                                                 000145
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_5                        000145
*  long units;                                                           000146
*  long origin;                                                          000147
*  long use2To32G = *use2To32GArea;                                      000148
         LG    14,464(0,13)            #SR_PARM_5                        000148
         USING @@PARMD@5,14                                              000148
         LG    14,@27use2To32GArea@9                                     000148
         LGF   14,0(0,14)              (*)int                            000148
         STG   14,@30use2To32G@13                                        000148
*  int  iarv64_rc = 0;                                                   000149
         LGHI  14,0                                                      000149
         ST    14,@31iarv64_rc@14                                        000149
*                                                                        000150
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000151
*                                                                        000152
*  units = *num2GBUnits;                                                 000153
         LG    14,464(0,13)            #SR_PARM_5                        000153
         LG    14,@26num2GBUnits                                         000153
         LGF   14,0(0,14)              (*)int                            000153
         STG   14,@33units                                               000153
*  wgetstor = ogetstor;                                                  000154
         MVC   @32wgetstor,@24ogetstor                                   000154
*                                                                        000155
*  if (0 == use2To32G) {                                                 000156
         LG    14,@30use2To32G@13                                        000156
         LTGR  14,14                                                     000156
         BRNE  @5L7                                                      000156
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000157
         LA    2,@34origin@12                                            000157
         DROP  10                                                        000157
         LA    4,@33units                                                000157
         LA    5,@32wgetstor                                             000157
         LG    14,464(0,13)            #SR_PARM_5                        000157
         LG    6,@28ttkn@10                                              000157
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000157
               AMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(4),ORIGIN=(2),TX 000157
               TOKEN=(6),RETCODE=200(13),MF=(E,(5))                      000157
         BRU   @5L8                                                      000157
@5L7     DS    0H                                                        000157
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000158
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000159
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000160
*  } else {                                                              000161
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000162
         LA    2,@34origin@12                                            000162
         LA    4,@33units                                                000162
         LA    5,@32wgetstor                                             000162
         LG    14,464(0,13)            #SR_PARM_5                        000162
         LG    6,@28ttkn@10                                              000162
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000162
               O32G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000162
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))       000162
@5L8     DS    0H                                                        000162
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000163
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000164
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000165
*  }                                                                     000166
*                                                                        000167
*  if (0 != iarv64_rc) {                                                 000168
         LGF   14,@31iarv64_rc@14                                        000168
         LTR   14,14                                                     000168
         BRE   @5L9                                                      000168
*   return (void *)0;                                                    000169
         LGHI  15,0                                                      000169
         BRU   @5L20                                                     000169
@5L9     DS    0H                                                        000169
*  }                                                                     000170
*  return (void *)origin;                                                000171
         LG    15,@34origin@12                                           000171
* }                                                                      000172
@5L20    DS    0H                                                        000172
         DROP                                                            000172
         MYEPILOG                                                        000172
J9IARV64 CSECT ,                                                         000172
         DS    0FD                                                       000172
@@LIT@5  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@5  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@5)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(19)                                                   000000
         DC    C'j9allocate_2G_pages'                                    000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@5 DSECT                                                           000000
         DS    59FD                                                      000000
         ORG   @@AUTO@5                                                  000000
#GPR_SA_5 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@5+176                                              000000
@33units DS    FD                                                        000000
         ORG   @@AUTO@5+184                                              000000
@34origin@12 DS FD                                                       000000
         ORG   @@AUTO@5+192                                              000000
@30use2To32G@13 DS FD                                                    000000
         ORG   @@AUTO@5+200                                              000000
@31iarv64_rc@14 DS F                                                     000000
         ORG   @@AUTO@5+208                                              000000
@32wgetstor DS XL256                                                     000000
         ORG   @@AUTO@5+464                                              000000
#SR_PARM_5 DS  XL8                                                       000000
@@PARMD@5 DSECT                                                          000000
         DS    XL24                                                      000000
         ORG   @@PARMD@5+0                                               000000
@26num2GBUnits DS FD                                                     000000
         ORG   @@PARMD@5+8                                               000000
@27use2To32GArea@9 DS FD                                                 000000
         ORG   @@PARMD@5+16                                              000000
@28ttkn@10 DS  FD                                                        000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* void * j9allocate_4K_pages_in_2to32G_area(int *numMBSegments, const c  000187
         ENTRY @@CCN@36                                                  000187
@@CCN@36 AMODE 64                                                        000187
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000187
         DC    A(@@FPB@4-*+8)          Signed offset to FPB              000187
         DC    XL4'00000000'           Reserved                          000187
@@CCN@36 DS    0FD                                                       000187
&CCN_PRCN SETC '@@CCN@36'                                                000187
&CCN_PRCN_LONG SETC 'j9allocate_4K_pages_in_2to32G_area'                 000187
&CCN_LITN SETC '@@LIT@4'                                                 000187
&CCN_BEGIN SETC '@@BGN@4'                                                000187
&CCN_ASCM SETC 'P'                                                       000187
&CCN_DSASZ SETA 464                                                      000187
&CCN_SASZ SETA 144                                                       000187
&CCN_ARGS SETA 2                                                         000187
&CCN_RLOW SETA 14                                                        000187
&CCN_RHIGH SETA 10                                                       000187
&CCN_NAB SETB  0                                                         000187
&CCN_MAIN SETB 0                                                         000187
&CCN_NAB_STORED SETB 0                                                   000187
&CCN_STATIC SETB 0                                                       000187
&CCN_ALTGPR(1) SETB 1                                                    000187
&CCN_ALTGPR(2) SETB 1                                                    000187
&CCN_ALTGPR(3) SETB 1                                                    000187
&CCN_ALTGPR(4) SETB 1                                                    000187
&CCN_ALTGPR(5) SETB 1                                                    000187
&CCN_ALTGPR(6) SETB 1                                                    000187
&CCN_ALTGPR(7) SETB 1                                                    000187
&CCN_ALTGPR(8) SETB 1                                                    000187
&CCN_ALTGPR(9) SETB 0                                                    000187
&CCN_ALTGPR(10) SETB 0                                                   000187
&CCN_ALTGPR(11) SETB 1                                                   000187
&CCN_ALTGPR(12) SETB 0                                                   000187
&CCN_ALTGPR(13) SETB 0                                                   000187
&CCN_ALTGPR(14) SETB 1                                                   000187
&CCN_ALTGPR(15) SETB 1                                                   000187
&CCN_ALTGPR(16) SETB 1                                                   000187
         MYPROLOG                                                        000187
@@BGN@4  DS    0H                                                        000187
         AIF   (NOT &CCN_SASIG).@@NOSIG4                                 000187
         LLILH 9,X'C6F4'                                                 000187
         OILL  9,X'E2C1'                                                 000187
         ST    9,4(,13)                                                  000187
.@@NOSIG4 ANOP                                                           000187
         USING @@AUTO@4,13                                               000187
         LARL  3,@@LIT@4                                                 000187
         USING @@LIT@4,3                                                 000187
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,456(0,13)             #SR_PARM_4                        000187
*  long segments;                                                        000188
*  long origin;                                                          000189
*  int  iarv64_rc = 0;                                                   000190
         LGHI  2,0                                                       000190
         ST    2,@40iarv64_rc@20                                         000190
*                                                                        000191
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000192
*                                                                        000193
*  segments = *numMBSegments;                                            000194
         LG    14,456(0,13)            #SR_PARM_4                        000194
         USING @@PARMD@4,14                                              000194
         LG    14,@37numMBSegments@15                                    000194
         LGF   14,0(0,14)              (*)int                            000194
         STG   14,@42segments@18                                         000194
*  wgetstor = mgetstor;                                                  000195
         MVC   @41wgetstor,@35mgetstor                                   000195
*                                                                        000196
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\    000197
         LA    4,@43origin@19                                            000197
         DROP  10                                                        000197
         LA    5,@42segments@18                                          000197
         LA    6,@41wgetstor                                             000197
         LG    14,456(0,13)            #SR_PARM_4                        000197
         LG    7,@38ttkn@16                                              000197
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000197
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(5),ORIGIN=(4),TTOKENX 000197
               =(7),RETCODE=192(13),MF=(E,(6))                           000197
         LGR   15,2                                                      000197
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000198
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000199
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000200
*                                                                        000201
*  if (0 != iarv64_rc) {                                                 000202
         LGF   14,@40iarv64_rc@20                                        000202
         LTR   14,14                                                     000202
         BRE   @4L5                                                      000202
*   return (void *)0;                                                    000203
         BRU   @4L21                                                     000203
@4L5     DS    0H                                                        000203
*  }                                                                     000204
*  return (void *)origin;                                                000205
         LG    15,@43origin@19                                           000205
* }                                                                      000206
@4L21    DS    0H                                                        000206
         DROP                                                            000206
         MYEPILOG                                                        000206
J9IARV64 CSECT ,                                                         000206
         DS    0FD                                                       000206
@@LIT@4  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@4  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@4)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(34)                                                   000000
         DC    C'j9allocate_4K_pages_in_2to32G_area'                     000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@4 DSECT                                                           000000
         DS    58FD                                                      000000
         ORG   @@AUTO@4                                                  000000
#GPR_SA_4 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@4+176                                              000000
@42segments@18 DS FD                                                     000000
         ORG   @@AUTO@4+184                                              000000
@43origin@19 DS FD                                                       000000
         ORG   @@AUTO@4+192                                              000000
@40iarv64_rc@20 DS F                                                     000000
         ORG   @@AUTO@4+200                                              000000
@41wgetstor DS XL256                                                     000000
         ORG   @@AUTO@4+456                                              000000
#SR_PARM_4 DS  XL8                                                       000000
@@PARMD@4 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@4+0                                               000000
@37numMBSegments@15 DS FD                                                000000
         ORG   @@PARMD@4+8                                               000000
@38ttkn@16 DS  FD                                                        000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* void * j9allocate_4K_pages_above_bar(int *numMBSegments, const char *  000221
         ENTRY @@CCN@45                                                  000221
@@CCN@45 AMODE 64                                                        000221
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000221
         DC    A(@@FPB@3-*+8)          Signed offset to FPB              000221
         DC    XL4'00000000'           Reserved                          000221
@@CCN@45 DS    0FD                                                       000221
&CCN_PRCN SETC '@@CCN@45'                                                000221
&CCN_PRCN_LONG SETC 'j9allocate_4K_pages_above_bar'                      000221
&CCN_LITN SETC '@@LIT@3'                                                 000221
&CCN_BEGIN SETC '@@BGN@3'                                                000221
&CCN_ASCM SETC 'P'                                                       000221
&CCN_DSASZ SETA 464                                                      000221
&CCN_SASZ SETA 144                                                       000221
&CCN_ARGS SETA 2                                                         000221
&CCN_RLOW SETA 14                                                        000221
&CCN_RHIGH SETA 10                                                       000221
&CCN_NAB SETB  0                                                         000221
&CCN_MAIN SETB 0                                                         000221
&CCN_NAB_STORED SETB 0                                                   000221
&CCN_STATIC SETB 0                                                       000221
&CCN_ALTGPR(1) SETB 1                                                    000221
&CCN_ALTGPR(2) SETB 1                                                    000221
&CCN_ALTGPR(3) SETB 1                                                    000221
&CCN_ALTGPR(4) SETB 1                                                    000221
&CCN_ALTGPR(5) SETB 1                                                    000221
&CCN_ALTGPR(6) SETB 1                                                    000221
&CCN_ALTGPR(7) SETB 1                                                    000221
&CCN_ALTGPR(8) SETB 1                                                    000221
&CCN_ALTGPR(9) SETB 0                                                    000221
&CCN_ALTGPR(10) SETB 0                                                   000221
&CCN_ALTGPR(11) SETB 1                                                   000221
&CCN_ALTGPR(12) SETB 0                                                   000221
&CCN_ALTGPR(13) SETB 0                                                   000221
&CCN_ALTGPR(14) SETB 1                                                   000221
&CCN_ALTGPR(15) SETB 1                                                   000221
&CCN_ALTGPR(16) SETB 1                                                   000221
         MYPROLOG                                                        000221
@@BGN@3  DS    0H                                                        000221
         AIF   (NOT &CCN_SASIG).@@NOSIG3                                 000221
         LLILH 9,X'C6F4'                                                 000221
         OILL  9,X'E2C1'                                                 000221
         ST    9,4(,13)                                                  000221
.@@NOSIG3 ANOP                                                           000221
         USING @@AUTO@3,13                                               000221
         LARL  3,@@LIT@3                                                 000221
         USING @@LIT@3,3                                                 000221
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,456(0,13)             #SR_PARM_3                        000221
*  long segments;                                                        000222
*  long origin;                                                          000223
*  int  iarv64_rc = 0;                                                   000224
         LGHI  2,0                                                       000224
         ST    2,@49iarv64_rc@26                                         000224
*                                                                        000225
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000226
*                                                                        000227
*  segments = *numMBSegments;                                            000228
         LG    14,456(0,13)            #SR_PARM_3                        000228
         USING @@PARMD@3,14                                              000228
         LG    14,@46numMBSegments@21                                    000228
         LGF   14,0(0,14)              (*)int                            000228
         STG   14,@51segments@24                                         000228
*  wgetstor = rgetstor;                                                  000229
         MVC   @50wgetstor,@44rgetstor                                   000229
*                                                                        000230
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000231
         LA    4,@52origin@25                                            000231
         DROP  10                                                        000231
         LA    5,@51segments@24                                          000231
         LA    6,@50wgetstor                                             000231
         LG    14,456(0,13)            #SR_PARM_3                        000231
         LG    7,@47ttkn@22                                              000231
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000231
               AMESIZE=4K,SEGMENTS=(5),ORIGIN=(4),TTOKEN=(7),RETCODE=19X 000231
               2(13),MF=(E,(6))                                          000231
         LGR   15,2                                                      000231
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000232
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000233
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000234
*                                                                        000235
*  if (0 != iarv64_rc) {                                                 000236
         LGF   14,@49iarv64_rc@26                                        000236
         LTR   14,14                                                     000236
         BRE   @3L3                                                      000236
*   return (void *)0;                                                    000237
         BRU   @3L22                                                     000237
@3L3     DS    0H                                                        000237
*  }                                                                     000238
*  return (void *)origin;                                                000239
         LG    15,@52origin@25                                           000239
* }                                                                      000240
@3L22    DS    0H                                                        000240
         DROP                                                            000240
         MYEPILOG                                                        000240
J9IARV64 CSECT ,                                                         000240
         DS    0FD                                                       000240
@@LIT@3  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@3  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@3)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(29)                                                   000000
         DC    C'j9allocate_4K_pages_above_bar'                          000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@3 DSECT                                                           000000
         DS    58FD                                                      000000
         ORG   @@AUTO@3                                                  000000
#GPR_SA_3 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@3+176                                              000000
@51segments@24 DS FD                                                     000000
         ORG   @@AUTO@3+184                                              000000
@52origin@25 DS FD                                                       000000
         ORG   @@AUTO@3+192                                              000000
@49iarv64_rc@26 DS F                                                     000000
         ORG   @@AUTO@3+200                                              000000
@50wgetstor DS XL256                                                     000000
         ORG   @@AUTO@3+456                                              000000
#SR_PARM_3 DS  XL8                                                       000000
@@PARMD@3 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@3+0                                               000000
@46numMBSegments@21 DS FD                                                000000
         ORG   @@PARMD@3+8                                               000000
@47ttkn@22 DS  FD                                                        000000
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
* int j9free_memory_above_bar(void *address, const char * ttkn){         000254
         ENTRY @@CCN@54                                                  000254
@@CCN@54 AMODE 64                                                        000254
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000254
         DC    A(@@FPB@2-*+8)          Signed offset to FPB              000254
         DC    XL4'00000000'           Reserved                          000254
@@CCN@54 DS    0FD                                                       000254
&CCN_PRCN SETC '@@CCN@54'                                                000254
&CCN_PRCN_LONG SETC 'j9free_memory_above_bar'                            000254
&CCN_LITN SETC '@@LIT@2'                                                 000254
&CCN_BEGIN SETC '@@BGN@2'                                                000254
&CCN_ASCM SETC 'P'                                                       000254
&CCN_DSASZ SETA 456                                                      000254
&CCN_SASZ SETA 144                                                       000254
&CCN_ARGS SETA 2                                                         000254
&CCN_RLOW SETA 14                                                        000254
&CCN_RHIGH SETA 10                                                       000254
&CCN_NAB SETB  0                                                         000254
&CCN_MAIN SETB 0                                                         000254
&CCN_NAB_STORED SETB 0                                                   000254
&CCN_STATIC SETB 0                                                       000254
&CCN_ALTGPR(1) SETB 1                                                    000254
&CCN_ALTGPR(2) SETB 1                                                    000254
&CCN_ALTGPR(3) SETB 1                                                    000254
&CCN_ALTGPR(4) SETB 1                                                    000254
&CCN_ALTGPR(5) SETB 1                                                    000254
&CCN_ALTGPR(6) SETB 1                                                    000254
&CCN_ALTGPR(7) SETB 0                                                    000254
&CCN_ALTGPR(8) SETB 0                                                    000254
&CCN_ALTGPR(9) SETB 0                                                    000254
&CCN_ALTGPR(10) SETB 0                                                   000254
&CCN_ALTGPR(11) SETB 1                                                   000254
&CCN_ALTGPR(12) SETB 0                                                   000254
&CCN_ALTGPR(13) SETB 0                                                   000254
&CCN_ALTGPR(14) SETB 1                                                   000254
&CCN_ALTGPR(15) SETB 1                                                   000254
&CCN_ALTGPR(16) SETB 1                                                   000254
         MYPROLOG                                                        000254
@@BGN@2  DS    0H                                                        000254
         AIF   (NOT &CCN_SASIG).@@NOSIG2                                 000254
         LLILH 9,X'C6F4'                                                 000254
         OILL  9,X'E2C1'                                                 000254
         ST    9,4(,13)                                                  000254
.@@NOSIG2 ANOP                                                           000254
         USING @@AUTO@2,13                                               000254
         LARL  3,@@LIT@2                                                 000254
         USING @@LIT@2,3                                                 000254
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,448(0,13)             #SR_PARM_2                        000254
*  void * xmemobjstart;                                                  000255
*  int  iarv64_rc = 0;                                                   000256
         LGHI  14,0                                                      000256
         ST    14,@58iarv64_rc@29                                        000256
*                                                                        000257
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000258
*                                                                        000259
*  xmemobjstart = address;                                               000260
         LG    14,448(0,13)            #SR_PARM_2                        000260
         USING @@PARMD@2,14                                              000260
         LG    14,@55address                                             000260
         STG   14,@60xmemobjstart                                        000260
*  wgetstor = pgetstor;                                                  000261
         MVC   @59wgetstor,@53pgetstor                                   000261
*                                                                        000262
*  __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),  000263
         LA    2,@59wgetstor                                             000263
         DROP  10                                                        000263
         LA    4,@60xmemobjstart                                         000263
         LG    14,448(0,13)            #SR_PARM_2                        000263
         LG    5,@56ttkn@27                                              000263
         IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(4),TTOKEN=(5),RETCX 000263
               ODE=184(13),MF=(E,(2))                                    000263
*    ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));      000264
*  return iarv64_rc;                                                     000265
         LGF   15,@58iarv64_rc@29                                        000265
* }                                                                      000266
@2L23    DS    0H                                                        000266
         DROP                                                            000266
         MYEPILOG                                                        000266
J9IARV64 CSECT ,                                                         000266
         DS    0FD                                                       000266
@@LIT@2  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@2  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111111100011'   Saved GPR Mask                    000000
         DC    A(@@PFD@@-@@FPB@2)      Signed Offset to Prefix Data      000000
         DC    BL1'10000000'           Flag Set 1                        000000
         DC    BL1'10000001'           Flag Set 2                        000000
         DC    BL1'00000000'           Flag Set 3                        000000
         DC    BL1'00000001'           Flag Set 4                        000000
         DC    XL4'00000000'           Reserved                          000000
         DC    XL4'00000000'           Reserved                          000000
         DC    AL2(23)                                                   000000
         DC    C'j9free_memory_above_bar'                                000000
J9IARV64 LOCTR                                                           000000
         EJECT                                                           000000
@@AUTO@2 DSECT                                                           000000
         DS    57FD                                                      000000
         ORG   @@AUTO@2                                                  000000
#GPR_SA_2 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@2+176                                              000000
@60xmemobjstart DS AD                                                    000000
         ORG   @@AUTO@2+184                                              000000
@58iarv64_rc@29 DS F                                                     000000
         ORG   @@AUTO@2+192                                              000000
@59wgetstor DS XL256                                                     000000
         ORG   @@AUTO@2+448                                              000000
#SR_PARM_2 DS  XL8                                                       000000
@@PARMD@2 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@2+0                                               000000
@55address DS  FD                                                        000000
         ORG   @@PARMD@2+8                                               000000
@56ttkn@27 DS  FD                                                        000000
*                                                                        000305
         EJECT                                                           000000
J9IARV64 CSECT ,                                                         000000
$STATIC  DS    0D                                                        000000
         DC    (1792)X'00'                                               000000
         LCLC  &DSMAC                                                    000000
         LCLA  &DSSIZE                                                   000000
         LCLA  &MSIZE                                                    000000
         ORG   $STATIC                                                   000000
@@LAB@1  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)                             000000
@@LAB@1L EQU   *-@@LAB@1                                                 000000
&DSMAC   SETC  '@@LAB@1'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@1L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@1                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@1  ANOP                                                            000000
         ORG   $STATIC+256                                               000000
@@LAB@2  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)                             000000
@@LAB@2L EQU   *-@@LAB@2                                                 000000
&DSMAC   SETC  '@@LAB@2'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@2L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@2                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@2  ANOP                                                            000000
         ORG   $STATIC+512                                               000000
@@LAB@3  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)                             000000
@@LAB@3L EQU   *-@@LAB@3                                                 000000
&DSMAC   SETC  '@@LAB@3'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@3L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@3                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@3  ANOP                                                            000000
         ORG   $STATIC+768                                               000000
@@LAB@4  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)                             000000
@@LAB@4L EQU   *-@@LAB@4                                                 000000
&DSMAC   SETC  '@@LAB@4'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@4L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@4                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@4  ANOP                                                            000000
         ORG   $STATIC+1024                                              000000
@@LAB@5  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)                             000000
@@LAB@5L EQU   *-@@LAB@5                                                 000000
&DSMAC   SETC  '@@LAB@5'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@5L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@5                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@5  ANOP                                                            000000
         ORG   $STATIC+1280                                              000000
@@LAB@6  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)                             000000
@@LAB@6L EQU   *-@@LAB@6                                                 000000
&DSMAC   SETC  '@@LAB@6'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@6L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@6                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@6  ANOP                                                            000000
         ORG   $STATIC+1536                                              000000
@@LAB@7  EQU   *                                                         000000
         IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)                             000000
@@LAB@7L EQU   *-@@LAB@7                                                 000000
&DSMAC   SETC  '@@LAB@7'                                                 000000
&DSSIZE  SETA  256                                                       000000
&MSIZE   SETA  @@LAB@7L                                                  000000
         AIF   (&DSSIZE GE &MSIZE).@@OK@7                                000000
         MNOTE 4,'Expanded size(&MSIZE) from &DSMAC exceeds XL:DS size(X 000000
               &DSSIZE)'                                                 000000
.@@OK@7  ANOP                                                            000000
         EJECT                                                           000000
@@STATICD@@ DSECT                                                        000000
         DS    XL1792                                                    000000
         ORG   @@STATICD@@                                               000000
@1lgetstor DS  XL256                                                     000000
         ORG   @@STATICD@@+256                                           000000
@13ngetstor DS XL256                                                     000000
         ORG   @@STATICD@@+512                                           000000
@24ogetstor DS XL256                                                     000000
         ORG   @@STATICD@@+768                                           000000
@35mgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1024                                          000000
@44rgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1280                                          000000
@53pgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1536                                          000000
@61qgetstor DS XL256                                                     000000
         END   ,(5650ZOS   ,2100,16060)                                  000000
