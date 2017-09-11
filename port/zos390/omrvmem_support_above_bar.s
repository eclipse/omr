***********************************************************************
* Copyright (c) 1991, 2016 IBM Corp. and others
* 
* This program and the accompanying materials are made available 
* under the terms of the Eclipse Public License 2.0 which accompanies 
* this distribution and is available at  
* https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
* Version 2.0 which accompanies this distribution and
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
* SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
***********************************************************************

         TITLE 'omrvmem_support_above_bar'

*** Please note: This file contains 2 Macros:
*
* NOTE: Each of these macro definitions start with "MACRO" and end
*       with "MEND"
*
* 1. MYPROLOG. This was needed for the METAL C compiler implementation
*       of omrallocate_large_pages and omrfree_large_pages (implemented
*       at bottom).
* 2. MYEPILOG. See explanation for MYPROLOG
*
*** This file also includes multiple HLASM calls to IARV64 HLASM 
* 		macro
*		- These calls were generated using the METAL-C compiler
*		- See omriarv64.c for details/instructions.
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
* Insert contents of omriarv64.s below
**************************************************
*
         ACONTROL AFPR                                                   000000
OMRIARV64 CSECT                                                          000000
OMRIARV64 AMODE 64                                                       000000
OMRIARV64 RMODE ANY                                                      000000
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
&CCN_CSECT SETC 'OMRIARV64'                                              000000
&CCN_ARCHLVL SETC '7'                                                    000000
         SYSSTATE ARCHLVL=2,AMODE64=YES                                  000000
         IEABRCX DEFINE                                                  000000
.* The HLASM GOFF option is needed to assemble this program              000000
@@CCN@64 ALIAS C'omrdiscard_data'                                        000000
@@CCN@56 ALIAS C'omrfree_memory_above_bar'                               000000
@@CCN@47 ALIAS C'omrallocate_4K_pages_above_bar'                         000000
@@CCN@36 ALIAS C'omrallocate_4K_pages_in_userExtendedPrivateArea'        000000
@@CCN@25 ALIAS C'omrallocate_2G_pages'                                   000000
@@CCN@14 ALIAS C'omrallocate_1M_pageable_pages_above_bar'                000000
@@CCN@2  ALIAS C'omrallocate_1M_fixed_pages'                             000000
*                                                                        000047
* #include "omriarv64.h"                                                 000048
*                                                                        000049
* #pragma prolog(omrallocate_1M_fixed_pages,"MYPROLOG")                  000050
* #pragma epilog(omrallocate_1M_fixed_pages,"MYEPILOG")                  000051
*                                                                        000052
* __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));          000053
*                                                                        000054
* /*                                                                     000055
*  * Allocate 1MB fixed pages using IARV64 system macro.                 000056
*  * Memory allocated is freed using omrfree_memory_above_bar().         000057
*  *                                                                     000058
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000059
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000060
*  *                                                                     000061
*  * @return pointer to memory allocated, NULL on failure.               000062
*  */                                                                    000063
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000064
*  long segments;                                                        000065
*  long origin;                                                          000066
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000067
*  int  iarv64_rc = 0;                                                   000068
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000069
*                                                                        000070
*  segments = *numMBSegments;                                            000071
*  wgetstor = lgetstor;                                                  000072
*                                                                        000073
*  switch (useMemoryType) {                                              000074
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000075
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000076
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000077
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000078
*   break;                                                               000079
*  case ZOS64_VMEM_2_TO_32G:                                             000080
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000081
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000082
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000083
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000084
*   break;                                                               000085
*  case ZOS64_VMEM_2_TO_64G:                                             000086
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000087
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000088
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000089
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000090
*   break;                                                               000091
*  }                                                                     000092
*                                                                        000093
*  if (0 != iarv64_rc) {                                                 000094
*   return (void *)0;                                                    000095
*  }                                                                     000096
*  return (void *)origin;                                                000097
* }                                                                      000098
*                                                                        000099
* #pragma prolog(omrallocate_1M_pageable_pages_above_bar,"MYPROLOG")     000100
* #pragma epilog(omrallocate_1M_pageable_pages_above_bar,"MYEPILOG")     000101
*                                                                        000102
* __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));          000103
*                                                                        000104
* /*                                                                     000105
*  * Allocate 1MB pageable pages above 2GB bar using IARV64 system macr  000106
*  * Memory allocated is freed using omrfree_memory_above_bar().         000107
*  *                                                                     000108
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000109
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000110
*  *                                                                     000111
*  * @return pointer to memory allocated, NULL on failure.               000112
*  */                                                                    000113
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000114
*  long segments;                                                        000115
*  long origin;                                                          000116
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000117
*  int  iarv64_rc = 0;                                                   000118
*                                                                        000119
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000120
*                                                                        000121
*  segments = *numMBSegments;                                            000122
*  wgetstor = ngetstor;                                                  000123
*                                                                        000124
*  switch (useMemoryType) {                                              000125
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000126
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000127
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000128
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000129
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000130
*   break;                                                               000131
*  case ZOS64_VMEM_2_TO_32G:                                             000132
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000133
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000134
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000135
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000136
*   break;                                                               000137
*  case ZOS64_VMEM_2_TO_64G:                                             000138
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000139
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000140
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000141
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000142
*   break;                                                               000143
*  }                                                                     000144
*                                                                        000145
*  if (0 != iarv64_rc) {                                                 000146
*   return (void *)0;                                                    000147
*  }                                                                     000148
*  return (void *)origin;                                                000149
* }                                                                      000150
*                                                                        000151
* #pragma prolog(omrallocate_2G_pages,"MYPROLOG")                        000152
* #pragma epilog(omrallocate_2G_pages,"MYEPILOG")                        000153
*                                                                        000154
* __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));          000155
*                                                                        000156
* /*                                                                     000157
*  * Allocate 2GB fixed pages using IARV64 system macro.                 000158
*  * Memory allocated is freed using omrfree_memory_above_bar().         000159
*  *                                                                     000160
*  * @params[in] num2GBUnits Number of 2GB units to be allocated         000161
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000162
*  *                                                                     000163
*  * @return pointer to memory allocated, NULL on failure.               000164
*  */                                                                    000165
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000166
*  long units;                                                           000167
*  long origin;                                                          000168
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000169
*  int  iarv64_rc = 0;                                                   000170
*                                                                        000171
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000172
*                                                                        000173
*  units = *num2GBUnits;                                                 000174
*  wgetstor = ogetstor;                                                  000175
*                                                                        000176
*  switch (useMemoryType) {                                              000177
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000178
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000179
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000180
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000181
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000182
*   break;                                                               000183
*  case ZOS64_VMEM_2_TO_32G:                                             000184
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000185
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000186
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000187
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000188
*   break;                                                               000189
*  case ZOS64_VMEM_2_TO_64G:                                             000190
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000191
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000192
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000193
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000194
*   break;                                                               000195
*  }                                                                     000196
*                                                                        000197
*  if (0 != iarv64_rc) {                                                 000198
*   return (void *)0;                                                    000199
*  }                                                                     000200
*  return (void *)origin;                                                000201
* }                                                                      000202
*                                                                        000203
* #pragma prolog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYPRO  000204
* #pragma epilog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYEPI  000205
*                                                                        000206
* __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));          000207
*                                                                        000208
* /*                                                                     000209
*  * Allocate 4KB pages in 2G-32G range using IARV64 system macro.       000210
*  * Memory allocated is freed using omrfree_memory_above_bar().         000211
*  *                                                                     000212
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000213
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000214
*  *                                                                     000215
*  * @return pointer to memory allocated, NULL on failure.               000216
*  */                                                                    000217
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000218
*  long segments;                                                        000219
*  long origin;                                                          000220
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000221
*  int  iarv64_rc = 0;                                                   000222
*                                                                        000223
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000224
*                                                                        000225
*  segments = *numMBSegments;                                            000226
*  wgetstor = mgetstor;                                                  000227
*                                                                        000228
*  switch (useMemoryType) {                                              000229
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000230
*   break;                                                               000231
*  case ZOS64_VMEM_2_TO_32G:                                             000232
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000233
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000234
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000235
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000236
*   break;                                                               000237
*  case ZOS64_VMEM_2_TO_64G:                                             000238
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000239
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000240
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000241
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000242
*   break;                                                               000243
*  }                                                                     000244
*                                                                        000245
*  if (0 != iarv64_rc) {                                                 000246
*   return (void *)0;                                                    000247
*  }                                                                     000248
*  return (void *)origin;                                                000249
* }                                                                      000250
*                                                                        000251
* #pragma prolog(omrallocate_4K_pages_above_bar,"MYPROLOG")              000252
* #pragma epilog(omrallocate_4K_pages_above_bar,"MYEPILOG")              000253
*                                                                        000254
* __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));          000255
*                                                                        000256
* /*                                                                     000257
*  * Allocate 4KB pages using IARV64 system macro.                       000258
*  * Memory allocated is freed using omrfree_memory_above_bar().         000259
*  *                                                                     000260
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000261
*  *                                                                     000262
*  * @return pointer to memory allocated, NULL on failure.               000263
*  */                                                                    000264
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000265
*  long segments;                                                        000266
*  long origin;                                                          000267
*  int  iarv64_rc = 0;                                                   000268
*                                                                        000269
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000270
*                                                                        000271
*  segments = *numMBSegments;                                            000272
*  wgetstor = rgetstor;                                                  000273
*                                                                        000274
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000275
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000276
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000277
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000278
*                                                                        000279
*  if (0 != iarv64_rc) {                                                 000280
*   return (void *)0;                                                    000281
*  }                                                                     000282
*  return (void *)origin;                                                000283
* }                                                                      000284
*                                                                        000285
* #pragma prolog(omrfree_memory_above_bar,"MYPROLOG")                    000286
* #pragma epilog(omrfree_memory_above_bar,"MYEPILOG")                    000287
*                                                                        000288
* __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));          000289
*                                                                        000290
* /*                                                                     000291
*  * Free memory allocated using IARV64 system macro.                    000292
*  *                                                                     000293
*  * @params[in] address pointer to memory region to be freed            000294
*  *                                                                     000295
*  * @return non-zero if memory is not freed successfully, 0 otherwise.  000296
*  */                                                                    000297
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000298
*  void * xmemobjstart;                                                  000299
*  int  iarv64_rc = 0;                                                   000300
*                                                                        000301
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000302
*                                                                        000303
*  xmemobjstart = address;                                               000304
*  wgetstor = pgetstor;                                                  000305
*                                                                        000306
*  __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),  000307
*    ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));      000308
*  return iarv64_rc;                                                     000309
* }                                                                      000310
*                                                                        000311
* #pragma prolog(omrdiscard_data,"MYPROLOG")                             000312
* #pragma epilog(omrdiscard_data,"MYEPILOG")                             000313
*                                                                        000314
* __asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));          000315
*                                                                        000316
* /* Used to pass parameters to IARV64 DISCARDDATA in omrdiscard_data()  000317
* struct rangeList {                                                     000318
*  void *startAddr;                                                      000319
*  long pageCount;                                                       000320
* };                                                                     000321
*                                                                        000322
* /*                                                                     000323
*  * Discard memory allocated using IARV64 system macro.                 000324
*  *                                                                     000325
*  * @params[in] address pointer to memory region to be discarded        000326
*  * @params[in] numFrames number of frames to be discarded. Frame size  000327
*  *                                                                     000328
*  * @return non-zero if memory is not discarded successfully, 0 otherw  000329
*  */                                                                    000330
* int omrdiscard_data(void *address, int *numFrames) {                   000331
         J     @@CCN@64                                                  000331
@@PFD@@  DC    XL8'00C300C300D50000'   Prefix Data Marker                000331
         DC    CL8'20160715'           Compiled Date YYYYMMDD            000331
         DC    CL6'110416'             Compiled Time HHMMSS              000331
         DC    XL4'42010000'           Compiler Version                  000331
         DC    XL2'0000'               Reserved                          000331
         DC    BL1'00000000'           Flag Set 1                        000331
         DC    BL1'00000000'           Flag Set 2                        000331
         DC    BL1'00000000'           Flag Set 3                        000331
         DC    BL1'00000000'           Flag Set 4                        000331
         DC    XL4'00000000'           Reserved                          000331
         ENTRY @@CCN@64                                                  000331
@@CCN@64 AMODE 64                                                        000331
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000331
         DC    A(@@FPB@1-*+8)          Signed offset to FPB              000331
         DC    XL4'00000000'           Reserved                          000331
@@CCN@64 DS    0FD                                                       000331
&CCN_PRCN SETC '@@CCN@64'                                                000331
&CCN_PRCN_LONG SETC 'omrdiscard_data'                                    000331
&CCN_LITN SETC '@@LIT@1'                                                 000331
&CCN_BEGIN SETC '@@BGN@1'                                                000331
&CCN_ASCM SETC 'P'                                                       000331
&CCN_DSASZ SETA 472                                                      000331
&CCN_SASZ SETA 144                                                       000331
&CCN_ARGS SETA 2                                                         000331
&CCN_RLOW SETA 14                                                        000331
&CCN_RHIGH SETA 10                                                       000331
&CCN_NAB SETB  0                                                         000331
&CCN_MAIN SETB 0                                                         000331
&CCN_NAB_STORED SETB 0                                                   000331
&CCN_STATIC SETB 0                                                       000331
&CCN_ALTGPR(1) SETB 1                                                    000331
&CCN_ALTGPR(2) SETB 1                                                    000331
&CCN_ALTGPR(3) SETB 1                                                    000331
&CCN_ALTGPR(4) SETB 1                                                    000331
&CCN_ALTGPR(5) SETB 1                                                    000331
&CCN_ALTGPR(6) SETB 0                                                    000331
&CCN_ALTGPR(7) SETB 0                                                    000331
&CCN_ALTGPR(8) SETB 0                                                    000331
&CCN_ALTGPR(9) SETB 0                                                    000331
&CCN_ALTGPR(10) SETB 0                                                   000331
&CCN_ALTGPR(11) SETB 1                                                   000331
&CCN_ALTGPR(12) SETB 0                                                   000331
&CCN_ALTGPR(13) SETB 0                                                   000331
&CCN_ALTGPR(14) SETB 1                                                   000331
&CCN_ALTGPR(15) SETB 1                                                   000331
&CCN_ALTGPR(16) SETB 1                                                   000331
         MYPROLOG                                                        000331
@@BGN@1  DS    0H                                                        000331
         AIF   (NOT &CCN_SASIG).@@NOSIG1                                 000331
         LLILH 9,X'C6F4'                                                 000331
         OILL  9,X'E2C1'                                                 000331
         ST    9,4(,13)                                                  000331
.@@NOSIG1 ANOP                                                           000331
         USING @@AUTO@1,13                                               000331
         LARL  3,@@LIT@1                                                 000331
         USING @@LIT@1,3                                                 000331
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_1                        000331
*  struct rangeList range;                                               000332
*  void *rangePtr;                                                       000333
*  int iarv64_rc = 0;                                                    000334
         LGHI  14,0                                                      000334
         ST    14,@68iarv64_rc@34                                        000334
*                                                                        000335
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000336
*                                                                        000337
*  range.startAddr = address;                                            000338
         LG    14,464(0,13)            #SR_PARM_1                        000338
         USING @@PARMD@1,14                                              000338
         LG    14,@65address@32                                          000338
         STG   14,176(0,13)            range_rangeList_startAddr         000338
*  range.pageCount = *numFrames;                                         000339
         LG    14,464(0,13)            #SR_PARM_1                        000339
         LG    14,@66numFrames                                           000339
         LGF   14,0(0,14)              (*)int                            000339
         STG   14,184(0,13)            range_rangeList_pageCount         000339
*  rangePtr = (void *)&range;                                            000340
         LA    14,@70range                                               000340
         DROP  10                                                        000340
         STG   14,@73rangePtr                                            000340
*  wgetstor = qgetstor;                                                  000341
         MVC   @69wgetstor,1536(10)                                      000341
*                                                                        000342
*  __asm(" IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,"\                     000343
         LA    2,@73rangePtr                                             000343
         LA    4,@69wgetstor                                             000343
         IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,RANGLIST=(2),RETCODE=20X 000343
               0(13),MF=(E,(4))                                          000343
*    "RANGLIST=(%1),RETCODE=%0,MF=(E,(%2))"\                             000344
*    ::"m"(iarv64_rc),"r"(&rangePtr),"r"(&wgetstor));                    000345
*                                                                        000346
*  return iarv64_rc;                                                     000347
         LGF   15,@68iarv64_rc@34                                        000347
* }                                                                      000348
@1L22    DS    0H                                                        000348
         DROP                                                            000348
         MYEPILOG                                                        000348
OMRIARV64 CSECT ,                                                        000348
         DS    0FD                                                       000348
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
         DC    AL2(15)                                                   000000
         DC    C'omrdiscard_data'                                        000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@1 DSECT                                                           000000
         DS    59FD                                                      000000
         ORG   @@AUTO@1                                                  000000
#GPR_SA_1 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@1+176                                              000000
@70range DS    XL16                                                      000000
         ORG   @@AUTO@1+192                                              000000
@73rangePtr DS AD                                                        000000
         ORG   @@AUTO@1+200                                              000000
@68iarv64_rc@34 DS F                                                     000000
         ORG   @@AUTO@1+208                                              000000
@69wgetstor DS XL256                                                     000000
         ORG   @@AUTO@1+464                                              000000
#SR_PARM_1 DS  XL8                                                       000000
@@PARMD@1 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@1+0                                               000000
@65address@32 DS FD                                                      000000
         ORG   @@PARMD@1+8                                               000000
@66numFrames DS FD                                                       000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000064
         ENTRY @@CCN@2                                                   000064
@@CCN@2  AMODE 64                                                        000064
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000064
         DC    A(@@FPB@7-*+8)          Signed offset to FPB              000064
         DC    XL4'00000000'           Reserved                          000064
@@CCN@2  DS    0FD                                                       000064
&CCN_PRCN SETC '@@CCN@2'                                                 000064
&CCN_PRCN_LONG SETC 'omrallocate_1M_fixed_pages'                         000064
&CCN_LITN SETC '@@LIT@7'                                                 000064
&CCN_BEGIN SETC '@@BGN@7'                                                000064
&CCN_ASCM SETC 'P'                                                       000064
&CCN_DSASZ SETA 480                                                      000064
&CCN_SASZ SETA 144                                                       000064
&CCN_ARGS SETA 3                                                         000064
&CCN_RLOW SETA 14                                                        000064
&CCN_RHIGH SETA 10                                                       000064
&CCN_NAB SETB  0                                                         000064
&CCN_MAIN SETB 0                                                         000064
&CCN_NAB_STORED SETB 0                                                   000064
&CCN_STATIC SETB 0                                                       000064
&CCN_ALTGPR(1) SETB 1                                                    000064
&CCN_ALTGPR(2) SETB 1                                                    000064
&CCN_ALTGPR(3) SETB 1                                                    000064
&CCN_ALTGPR(4) SETB 1                                                    000064
&CCN_ALTGPR(5) SETB 1                                                    000064
&CCN_ALTGPR(6) SETB 1                                                    000064
&CCN_ALTGPR(7) SETB 1                                                    000064
&CCN_ALTGPR(8) SETB 0                                                    000064
&CCN_ALTGPR(9) SETB 0                                                    000064
&CCN_ALTGPR(10) SETB 0                                                   000064
&CCN_ALTGPR(11) SETB 1                                                   000064
&CCN_ALTGPR(12) SETB 0                                                   000064
&CCN_ALTGPR(13) SETB 0                                                   000064
&CCN_ALTGPR(14) SETB 1                                                   000064
&CCN_ALTGPR(15) SETB 1                                                   000064
&CCN_ALTGPR(16) SETB 1                                                   000064
         MYPROLOG                                                        000064
@@BGN@7  DS    0H                                                        000064
         AIF   (NOT &CCN_SASIG).@@NOSIG7                                 000064
         LLILH 9,X'C6F4'                                                 000064
         OILL  9,X'E2C1'                                                 000064
         ST    9,4(,13)                                                  000064
.@@NOSIG7 ANOP                                                           000064
         USING @@AUTO@7,13                                               000064
         LARL  3,@@LIT@7                                                 000064
         USING @@LIT@7,3                                                 000064
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_7                        000064
*  long segments;                                                        000065
*  long origin;                                                          000066
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000067
         LG    14,464(0,13)            #SR_PARM_7                        000067
         USING @@PARMD@7,14                                              000067
         LG    14,@4userExtendedPrivateAreaMemoryType                    000067
         LGF   14,0(0,14)              (*)int                            000067
         STG   14,@7useMemoryType                                        000067
*  int  iarv64_rc = 0;                                                   000068
         LGHI  14,0                                                      000068
         ST    14,@9iarv64_rc                                            000068
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000069
*                                                                        000070
*  segments = *numMBSegments;                                            000071
         LG    14,464(0,13)            #SR_PARM_7                        000071
         LG    14,@3numMBSegments                                        000071
         LGF   14,0(0,14)              (*)int                            000071
         STG   14,@11segments                                            000071
*  wgetstor = lgetstor;                                                  000072
         MVC   @10wgetstor,@1lgetstor                                    000072
*                                                                        000073
*  switch (useMemoryType) {                                              000074
         LG    14,@7useMemoryType                                        000074
         STG   14,472(0,13)            #SW_WORK7                         000074
         CLG   14,=X'0000000000000002'                                   000074
         BRH   @7L41                                                     000074
         LG    14,472(0,13)            #SW_WORK7                         000074
         SLLG  14,14,2                                                   000074
         LGFR  15,14                                                     000074
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,0(15,14)                                               000074
         B     0(3,14)                                                   000074
@7L41    DS    0H                                                        000074
         BRU   @7L46                                                     000074
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000075
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000076
@7L42    DS    0H                                                        000076
         LA    2,@12origin                                               000076
         DROP  10                                                        000076
         LA    4,@11segments                                             000076
         LA    5,@10wgetstor                                             000076
         LG    14,464(0,13)            #SR_PARM_7                        000076
         LG    6,@5ttkn                                                  000076
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000076
               AMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=X 000076
               200(13),MF=(E,(5))                                        000076
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000077
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000078
*   break;                                                               000079
         BRU   @7L12                                                     000079
*  case ZOS64_VMEM_2_TO_32G:                                             000080
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000081
@7L43    DS    0H                                                        000081
         LA    2,@12origin                                               000081
         LA    4,@11segments                                             000081
         LA    5,@10wgetstor                                             000081
         LG    14,464(0,13)            #SR_PARM_7                        000081
         LG    6,@5ttkn                                                  000081
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000081
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000081
               EN=(6),RETCODE=200(13),MF=(E,(5))                         000081
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000082
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000083
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000084
*   break;                                                               000085
         BRU   @7L12                                                     000085
*  case ZOS64_VMEM_2_TO_64G:                                             000086
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000087
@7L44    DS    0H                                                        000087
         LA    2,@12origin                                               000087
         LA    4,@11segments                                             000087
         LA    5,@10wgetstor                                             000087
         LG    14,464(0,13)            #SR_PARM_7                        000087
         LG    6,@5ttkn                                                  000087
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000087
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000087
               EN=(6),RETCODE=200(13),MF=(E,(5))                         000087
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000088
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000089
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000090
*   break;                                                               000091
@7L12    DS    0H                                                        000074
@7L46    DS    0H                                                        000000
*  }                                                                     000092
*                                                                        000093
*  if (0 != iarv64_rc) {                                                 000094
         LGF   14,@9iarv64_rc                                            000094
         LTR   14,14                                                     000094
         BRE   @7L11                                                     000094
*   return (void *)0;                                                    000095
         LGHI  15,0                                                      000095
         BRU   @7L13                                                     000095
@7L11    DS    0H                                                        000095
*  }                                                                     000096
*  return (void *)origin;                                                000097
         LG    15,@12origin                                              000097
* }                                                                      000098
@7L13    DS    0H                                                        000098
         DROP                                                            000098
         MYEPILOG                                                        000098
OMRIARV64 CSECT ,                                                        000098
         DS    0FD                                                       000098
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
         DC    AL2(26)                                                   000000
         DC    C'omrallocate_1M_fixed_pages'                             000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@7 DSECT                                                           000000
         DS    60FD                                                      000000
         ORG   @@AUTO@7                                                  000000
#GPR_SA_7 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@7+176                                              000000
@11segments DS FD                                                        000000
         ORG   @@AUTO@7+184                                              000000
@12origin DS   FD                                                        000000
         ORG   @@AUTO@7+192                                              000000
@7useMemoryType DS FD                                                    000000
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
@4userExtendedPrivateAreaMemoryType DS FD                                000000
         ORG   @@PARMD@7+16                                              000000
@5ttkn   DS    FD                                                        000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000114
         ENTRY @@CCN@14                                                  000114
@@CCN@14 AMODE 64                                                        000114
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000114
         DC    A(@@FPB@6-*+8)          Signed offset to FPB              000114
         DC    XL4'00000000'           Reserved                          000114
@@CCN@14 DS    0FD                                                       000114
&CCN_PRCN SETC '@@CCN@14'                                                000114
&CCN_PRCN_LONG SETC 'omrallocate_1M_pageable_pages_above_bar'            000114
&CCN_LITN SETC '@@LIT@6'                                                 000114
&CCN_BEGIN SETC '@@BGN@6'                                                000114
&CCN_ASCM SETC 'P'                                                       000114
&CCN_DSASZ SETA 480                                                      000114
&CCN_SASZ SETA 144                                                       000114
&CCN_ARGS SETA 3                                                         000114
&CCN_RLOW SETA 14                                                        000114
&CCN_RHIGH SETA 10                                                       000114
&CCN_NAB SETB  0                                                         000114
&CCN_MAIN SETB 0                                                         000114
&CCN_NAB_STORED SETB 0                                                   000114
&CCN_STATIC SETB 0                                                       000114
&CCN_ALTGPR(1) SETB 1                                                    000114
&CCN_ALTGPR(2) SETB 1                                                    000114
&CCN_ALTGPR(3) SETB 1                                                    000114
&CCN_ALTGPR(4) SETB 1                                                    000114
&CCN_ALTGPR(5) SETB 1                                                    000114
&CCN_ALTGPR(6) SETB 1                                                    000114
&CCN_ALTGPR(7) SETB 1                                                    000114
&CCN_ALTGPR(8) SETB 0                                                    000114
&CCN_ALTGPR(9) SETB 0                                                    000114
&CCN_ALTGPR(10) SETB 0                                                   000114
&CCN_ALTGPR(11) SETB 1                                                   000114
&CCN_ALTGPR(12) SETB 0                                                   000114
&CCN_ALTGPR(13) SETB 0                                                   000114
&CCN_ALTGPR(14) SETB 1                                                   000114
&CCN_ALTGPR(15) SETB 1                                                   000114
&CCN_ALTGPR(16) SETB 1                                                   000114
         MYPROLOG                                                        000114
@@BGN@6  DS    0H                                                        000114
         AIF   (NOT &CCN_SASIG).@@NOSIG6                                 000114
         LLILH 9,X'C6F4'                                                 000114
         OILL  9,X'E2C1'                                                 000114
         ST    9,4(,13)                                                  000114
.@@NOSIG6 ANOP                                                           000114
         USING @@AUTO@6,13                                               000114
         LARL  3,@@LIT@6                                                 000114
         USING @@LIT@6,3                                                 000114
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_6                        000114
*  long segments;                                                        000115
*  long origin;                                                          000116
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000117
         LG    14,464(0,13)            #SR_PARM_6                        000117
         USING @@PARMD@6,14                                              000117
         LG    14,@16userExtendedPrivateAreaMemoryType@2                 000117
         LGF   14,0(0,14)              (*)int                            000117
         STG   14,@19useMemoryType@7                                     000117
*  int  iarv64_rc = 0;                                                   000118
         LGHI  14,0                                                      000118
         ST    14,@20iarv64_rc@8                                         000118
*                                                                        000119
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000120
*                                                                        000121
*  segments = *numMBSegments;                                            000122
         LG    14,464(0,13)            #SR_PARM_6                        000122
         LG    14,@15numMBSegments@1                                     000122
         LGF   14,0(0,14)              (*)int                            000122
         STG   14,@22segments@5                                          000122
*  wgetstor = ngetstor;                                                  000123
         MVC   @21wgetstor,@13ngetstor                                   000123
*                                                                        000124
*  switch (useMemoryType) {                                              000125
         LG    14,@19useMemoryType@7                                     000125
         STG   14,472(0,13)            #SW_WORK6                         000125
         CLG   14,=X'0000000000000002'                                   000125
         BRH   @6L35                                                     000125
         LG    14,472(0,13)            #SW_WORK6                         000125
         SLLG  14,14,2                                                   000125
         LGFR  15,14                                                     000125
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,12(15,14)                                              000125
         B     0(3,14)                                                   000125
@6L35    DS    0H                                                        000125
         BRU   @6L40                                                     000125
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000126
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000127
@6L36    DS    0H                                                        000127
         LA    2,@23origin@6                                             000127
         DROP  10                                                        000127
         LA    4,@22segments@5                                           000127
         LA    5,@21wgetstor                                             000127
         LG    14,464(0,13)            #SR_PARM_6                        000127
         LG    6,@17ttkn@3                                               000127
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000127
               AMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(4),ORIGIN=(X 000127
               2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))                  000127
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000128
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000129
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000130
*   break;                                                               000131
         BRU   @6L14                                                     000131
*  case ZOS64_VMEM_2_TO_32G:                                             000132
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000133
@6L37    DS    0H                                                        000133
         LA    2,@23origin@6                                             000133
         LA    4,@22segments@5                                           000133
         LA    5,@21wgetstor                                             000133
         LG    14,464(0,13)            #SR_PARM_6                        000133
         LG    6,@17ttkn@3                                               000133
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000133
               O32G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000133
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))   000133
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000134
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000135
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000136
*   break;                                                               000137
         BRU   @6L14                                                     000137
*  case ZOS64_VMEM_2_TO_64G:                                             000138
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000139
@6L38    DS    0H                                                        000139
         LA    2,@23origin@6                                             000139
         LA    4,@22segments@5                                           000139
         LA    5,@21wgetstor                                             000139
         LG    14,464(0,13)            #SR_PARM_6                        000139
         LG    6,@17ttkn@3                                               000139
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000139
               O64G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000139
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))   000139
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000140
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000141
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000142
*   break;                                                               000143
@6L14    DS    0H                                                        000125
@6L40    DS    0H                                                        000000
*  }                                                                     000144
*                                                                        000145
*  if (0 != iarv64_rc) {                                                 000146
         LGF   14,@20iarv64_rc@8                                         000146
         LTR   14,14                                                     000146
         BRE   @6L9                                                      000146
*   return (void *)0;                                                    000147
         LGHI  15,0                                                      000147
         BRU   @6L15                                                     000147
@6L9     DS    0H                                                        000147
*  }                                                                     000148
*  return (void *)origin;                                                000149
         LG    15,@23origin@6                                            000149
* }                                                                      000150
@6L15    DS    0H                                                        000150
         DROP                                                            000150
         MYEPILOG                                                        000150
OMRIARV64 CSECT ,                                                        000150
         DS    0FD                                                       000150
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
         DC    AL2(39)                                                   000000
         DC    C'omrallocate_1M_pageable_pages_above_bar'                000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@6 DSECT                                                           000000
         DS    60FD                                                      000000
         ORG   @@AUTO@6                                                  000000
#GPR_SA_6 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@6+176                                              000000
@22segments@5 DS FD                                                      000000
         ORG   @@AUTO@6+184                                              000000
@23origin@6 DS FD                                                        000000
         ORG   @@AUTO@6+192                                              000000
@19useMemoryType@7 DS FD                                                 000000
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
@16userExtendedPrivateAreaMemoryType@2 DS FD                             000000
         ORG   @@PARMD@6+16                                              000000
@17ttkn@3 DS   FD                                                        000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000166
         ENTRY @@CCN@25                                                  000166
@@CCN@25 AMODE 64                                                        000166
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000166
         DC    A(@@FPB@5-*+8)          Signed offset to FPB              000166
         DC    XL4'00000000'           Reserved                          000166
@@CCN@25 DS    0FD                                                       000166
&CCN_PRCN SETC '@@CCN@25'                                                000166
&CCN_PRCN_LONG SETC 'omrallocate_2G_pages'                               000166
&CCN_LITN SETC '@@LIT@5'                                                 000166
&CCN_BEGIN SETC '@@BGN@5'                                                000166
&CCN_ASCM SETC 'P'                                                       000166
&CCN_DSASZ SETA 480                                                      000166
&CCN_SASZ SETA 144                                                       000166
&CCN_ARGS SETA 3                                                         000166
&CCN_RLOW SETA 14                                                        000166
&CCN_RHIGH SETA 10                                                       000166
&CCN_NAB SETB  0                                                         000166
&CCN_MAIN SETB 0                                                         000166
&CCN_NAB_STORED SETB 0                                                   000166
&CCN_STATIC SETB 0                                                       000166
&CCN_ALTGPR(1) SETB 1                                                    000166
&CCN_ALTGPR(2) SETB 1                                                    000166
&CCN_ALTGPR(3) SETB 1                                                    000166
&CCN_ALTGPR(4) SETB 1                                                    000166
&CCN_ALTGPR(5) SETB 1                                                    000166
&CCN_ALTGPR(6) SETB 1                                                    000166
&CCN_ALTGPR(7) SETB 1                                                    000166
&CCN_ALTGPR(8) SETB 0                                                    000166
&CCN_ALTGPR(9) SETB 0                                                    000166
&CCN_ALTGPR(10) SETB 0                                                   000166
&CCN_ALTGPR(11) SETB 1                                                   000166
&CCN_ALTGPR(12) SETB 0                                                   000166
&CCN_ALTGPR(13) SETB 0                                                   000166
&CCN_ALTGPR(14) SETB 1                                                   000166
&CCN_ALTGPR(15) SETB 1                                                   000166
&CCN_ALTGPR(16) SETB 1                                                   000166
         MYPROLOG                                                        000166
@@BGN@5  DS    0H                                                        000166
         AIF   (NOT &CCN_SASIG).@@NOSIG5                                 000166
         LLILH 9,X'C6F4'                                                 000166
         OILL  9,X'E2C1'                                                 000166
         ST    9,4(,13)                                                  000166
.@@NOSIG5 ANOP                                                           000166
         USING @@AUTO@5,13                                               000166
         LARL  3,@@LIT@5                                                 000166
         USING @@LIT@5,3                                                 000166
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_5                        000166
*  long units;                                                           000167
*  long origin;                                                          000168
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000169
         LG    14,464(0,13)            #SR_PARM_5                        000169
         USING @@PARMD@5,14                                              000169
         LG    14,@27userExtendedPrivateAreaMemoryType@9                 000169
         LGF   14,0(0,14)              (*)int                            000169
         STG   14,@30useMemoryType@13                                    000169
*  int  iarv64_rc = 0;                                                   000170
         LGHI  14,0                                                      000170
         ST    14,@31iarv64_rc@14                                        000170
*                                                                        000171
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000172
*                                                                        000173
*  units = *num2GBUnits;                                                 000174
         LG    14,464(0,13)            #SR_PARM_5                        000174
         LG    14,@26num2GBUnits                                         000174
         LGF   14,0(0,14)              (*)int                            000174
         STG   14,@33units                                               000174
*  wgetstor = ogetstor;                                                  000175
         MVC   @32wgetstor,@24ogetstor                                   000175
*                                                                        000176
*  switch (useMemoryType) {                                              000177
         LG    14,@30useMemoryType@13                                    000177
         STG   14,472(0,13)            #SW_WORK5                         000177
         CLG   14,=X'0000000000000002'                                   000177
         BRH   @5L29                                                     000177
         LG    14,472(0,13)            #SW_WORK5                         000177
         SLLG  14,14,2                                                   000177
         LGFR  15,14                                                     000177
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,24(15,14)                                              000177
         B     0(3,14)                                                   000177
@5L29    DS    0H                                                        000177
         BRU   @5L34                                                     000177
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000178
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000179
@5L30    DS    0H                                                        000179
         LA    2,@34origin@12                                            000179
         DROP  10                                                        000179
         LA    4,@33units                                                000179
         LA    5,@32wgetstor                                             000179
         LG    14,464(0,13)            #SR_PARM_5                        000179
         LG    6,@28ttkn@10                                              000179
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000179
               AMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(4),ORIGIN=(2),TX 000179
               TOKEN=(6),RETCODE=200(13),MF=(E,(5))                      000179
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000180
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000181
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000182
*   break;                                                               000183
         BRU   @5L16                                                     000183
*  case ZOS64_VMEM_2_TO_32G:                                             000184
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000185
@5L31    DS    0H                                                        000185
         LA    2,@34origin@12                                            000185
         LA    4,@33units                                                000185
         LA    5,@32wgetstor                                             000185
         LG    14,464(0,13)            #SR_PARM_5                        000185
         LG    6,@28ttkn@10                                              000185
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000185
               O32G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000185
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))       000185
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000186
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000187
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000188
*   break;                                                               000189
         BRU   @5L16                                                     000189
*  case ZOS64_VMEM_2_TO_64G:                                             000190
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000191
@5L32    DS    0H                                                        000191
         LA    2,@34origin@12                                            000191
         LA    4,@33units                                                000191
         LA    5,@32wgetstor                                             000191
         LG    14,464(0,13)            #SR_PARM_5                        000191
         LG    6,@28ttkn@10                                              000191
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000191
               O64G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000191
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))       000191
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000192
*     "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                  000193
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttkn  000194
*   break;                                                               000195
@5L16    DS    0H                                                        000177
@5L34    DS    0H                                                        000000
*  }                                                                     000196
*                                                                        000197
*  if (0 != iarv64_rc) {                                                 000198
         LGF   14,@31iarv64_rc@14                                        000198
         LTR   14,14                                                     000198
         BRE   @5L7                                                      000198
*   return (void *)0;                                                    000199
         LGHI  15,0                                                      000199
         BRU   @5L17                                                     000199
@5L7     DS    0H                                                        000199
*  }                                                                     000200
*  return (void *)origin;                                                000201
         LG    15,@34origin@12                                           000201
* }                                                                      000202
@5L17    DS    0H                                                        000202
         DROP                                                            000202
         MYEPILOG                                                        000202
OMRIARV64 CSECT ,                                                        000202
         DS    0FD                                                       000202
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
         DC    AL2(20)                                                   000000
         DC    C'omrallocate_2G_pages'                                   000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@5 DSECT                                                           000000
         DS    60FD                                                      000000
         ORG   @@AUTO@5                                                  000000
#GPR_SA_5 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@5+176                                              000000
@33units DS    FD                                                        000000
         ORG   @@AUTO@5+184                                              000000
@34origin@12 DS FD                                                       000000
         ORG   @@AUTO@5+192                                              000000
@30useMemoryType@13 DS FD                                                000000
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
@27userExtendedPrivateAreaMemoryType@9 DS FD                             000000
         ORG   @@PARMD@5+16                                              000000
@28ttkn@10 DS  FD                                                        000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000218
         ENTRY @@CCN@36                                                  000218
@@CCN@36 AMODE 64                                                        000218
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000218
         DC    A(@@FPB@4-*+8)          Signed offset to FPB              000218
         DC    XL4'00000000'           Reserved                          000218
@@CCN@36 DS    0FD                                                       000218
&CCN_PRCN SETC '@@CCN@36'                                                000218
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_in_userExtendedPrivateArea'    000218
&CCN_LITN SETC '@@LIT@4'                                                 000218
&CCN_BEGIN SETC '@@BGN@4'                                                000218
&CCN_ASCM SETC 'P'                                                       000218
&CCN_DSASZ SETA 480                                                      000218
&CCN_SASZ SETA 144                                                       000218
&CCN_ARGS SETA 3                                                         000218
&CCN_RLOW SETA 14                                                        000218
&CCN_RHIGH SETA 10                                                       000218
&CCN_NAB SETB  0                                                         000218
&CCN_MAIN SETB 0                                                         000218
&CCN_NAB_STORED SETB 0                                                   000218
&CCN_STATIC SETB 0                                                       000218
&CCN_ALTGPR(1) SETB 1                                                    000218
&CCN_ALTGPR(2) SETB 1                                                    000218
&CCN_ALTGPR(3) SETB 1                                                    000218
&CCN_ALTGPR(4) SETB 1                                                    000218
&CCN_ALTGPR(5) SETB 1                                                    000218
&CCN_ALTGPR(6) SETB 1                                                    000218
&CCN_ALTGPR(7) SETB 1                                                    000218
&CCN_ALTGPR(8) SETB 0                                                    000218
&CCN_ALTGPR(9) SETB 0                                                    000218
&CCN_ALTGPR(10) SETB 0                                                   000218
&CCN_ALTGPR(11) SETB 1                                                   000218
&CCN_ALTGPR(12) SETB 0                                                   000218
&CCN_ALTGPR(13) SETB 0                                                   000218
&CCN_ALTGPR(14) SETB 1                                                   000218
&CCN_ALTGPR(15) SETB 1                                                   000218
&CCN_ALTGPR(16) SETB 1                                                   000218
         MYPROLOG                                                        000218
@@BGN@4  DS    0H                                                        000218
         AIF   (NOT &CCN_SASIG).@@NOSIG4                                 000218
         LLILH 9,X'C6F4'                                                 000218
         OILL  9,X'E2C1'                                                 000218
         ST    9,4(,13)                                                  000218
.@@NOSIG4 ANOP                                                           000218
         USING @@AUTO@4,13                                               000218
         LARL  3,@@LIT@4                                                 000218
         USING @@LIT@4,3                                                 000218
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,464(0,13)             #SR_PARM_4                        000218
*  long segments;                                                        000219
*  long origin;                                                          000220
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000221
         LG    14,464(0,13)            #SR_PARM_4                        000221
         USING @@PARMD@4,14                                              000221
         LG    14,@38userExtendedPrivateAreaMemoryType@16                000221
         LGF   14,0(0,14)              (*)int                            000221
         STG   14,@41useMemoryType@21                                    000221
*  int  iarv64_rc = 0;                                                   000222
         LGHI  14,0                                                      000222
         ST    14,@42iarv64_rc@22                                        000222
*                                                                        000223
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000224
*                                                                        000225
*  segments = *numMBSegments;                                            000226
         LG    14,464(0,13)            #SR_PARM_4                        000226
         LG    14,@37numMBSegments@15                                    000226
         LGF   14,0(0,14)              (*)int                            000226
         STG   14,@44segments@19                                         000226
*  wgetstor = mgetstor;                                                  000227
         MVC   @43wgetstor,@35mgetstor                                   000227
*                                                                        000228
*  switch (useMemoryType) {                                              000229
         LG    14,@41useMemoryType@21                                    000229
         STG   14,472(0,13)            #SW_WORK4                         000229
         CLG   14,=X'0000000000000002'                                   000229
         BRH   @4L23                                                     000229
         LG    14,472(0,13)            #SW_WORK4                         000229
         SLLG  14,14,2                                                   000229
         LGFR  15,14                                                     000229
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,36(15,14)                                              000229
         B     0(3,14)                                                   000229
@4L23    DS    0H                                                        000229
         BRU   @4L28                                                     000229
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000230
*   break;                                                               000231
@4L24    DS    0H                                                        000231
         BRU   @4L18                                                     000231
*  case ZOS64_VMEM_2_TO_32G:                                             000232
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000233
@4L25    DS    0H                                                        000233
         LA    2,@45origin@20                                            000233
         DROP  10                                                        000233
         LA    4,@44segments@19                                          000233
         LA    5,@43wgetstor                                             000233
         LG    14,464(0,13)            #SR_PARM_4                        000233
         LG    6,@39ttkn@17                                              000233
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000233
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000233
               =(6),RETCODE=200(13),MF=(E,(5))                           000233
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000234
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000235
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000236
*   break;                                                               000237
         BRU   @4L18                                                     000237
*  case ZOS64_VMEM_2_TO_64G:                                             000238
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000239
@4L26    DS    0H                                                        000239
         LA    2,@45origin@20                                            000239
         LA    4,@44segments@19                                          000239
         LA    5,@43wgetstor                                             000239
         LG    14,464(0,13)            #SR_PARM_4                        000239
         LG    6,@39ttkn@17                                              000239
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000239
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000239
               =(6),RETCODE=200(13),MF=(E,(5))                           000239
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000240
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000241
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000242
*   break;                                                               000243
@4L18    DS    0H                                                        000229
@4L28    DS    0H                                                        000000
*  }                                                                     000244
*                                                                        000245
*  if (0 != iarv64_rc) {                                                 000246
         LGF   14,@42iarv64_rc@22                                        000246
         LTR   14,14                                                     000246
         BRE   @4L5                                                      000246
*   return (void *)0;                                                    000247
         LGHI  15,0                                                      000247
         BRU   @4L19                                                     000247
@4L5     DS    0H                                                        000247
*  }                                                                     000248
*  return (void *)origin;                                                000249
         LG    15,@45origin@20                                           000249
* }                                                                      000250
@4L19    DS    0H                                                        000250
         DROP                                                            000250
         MYEPILOG                                                        000250
OMRIARV64 CSECT ,                                                        000250
         DS    0FD                                                       000250
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
         DC    AL2(47)                                                   000000
         DC    C'omrallocate_4K_pages_in_userExtendedPrivateArea'        000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@4 DSECT                                                           000000
         DS    60FD                                                      000000
         ORG   @@AUTO@4                                                  000000
#GPR_SA_4 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@4+176                                              000000
@44segments@19 DS FD                                                     000000
         ORG   @@AUTO@4+184                                              000000
@45origin@20 DS FD                                                       000000
         ORG   @@AUTO@4+192                                              000000
@41useMemoryType@21 DS FD                                                000000
         ORG   @@AUTO@4+200                                              000000
@42iarv64_rc@22 DS F                                                     000000
         ORG   @@AUTO@4+208                                              000000
@43wgetstor DS XL256                                                     000000
         ORG   @@AUTO@4+464                                              000000
#SR_PARM_4 DS  XL8                                                       000000
@@PARMD@4 DSECT                                                          000000
         DS    XL24                                                      000000
         ORG   @@PARMD@4+0                                               000000
@37numMBSegments@15 DS FD                                                000000
         ORG   @@PARMD@4+8                                               000000
@38userExtendedPrivateAreaMemoryType@16 DS FD                            000000
         ORG   @@PARMD@4+16                                              000000
@39ttkn@17 DS  FD                                                        000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000265
         ENTRY @@CCN@47                                                  000265
@@CCN@47 AMODE 64                                                        000265
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000265
         DC    A(@@FPB@3-*+8)          Signed offset to FPB              000265
         DC    XL4'00000000'           Reserved                          000265
@@CCN@47 DS    0FD                                                       000265
&CCN_PRCN SETC '@@CCN@47'                                                000265
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_above_bar'                     000265
&CCN_LITN SETC '@@LIT@3'                                                 000265
&CCN_BEGIN SETC '@@BGN@3'                                                000265
&CCN_ASCM SETC 'P'                                                       000265
&CCN_DSASZ SETA 464                                                      000265
&CCN_SASZ SETA 144                                                       000265
&CCN_ARGS SETA 2                                                         000265
&CCN_RLOW SETA 14                                                        000265
&CCN_RHIGH SETA 10                                                       000265
&CCN_NAB SETB  0                                                         000265
&CCN_MAIN SETB 0                                                         000265
&CCN_NAB_STORED SETB 0                                                   000265
&CCN_STATIC SETB 0                                                       000265
&CCN_ALTGPR(1) SETB 1                                                    000265
&CCN_ALTGPR(2) SETB 1                                                    000265
&CCN_ALTGPR(3) SETB 1                                                    000265
&CCN_ALTGPR(4) SETB 1                                                    000265
&CCN_ALTGPR(5) SETB 1                                                    000265
&CCN_ALTGPR(6) SETB 1                                                    000265
&CCN_ALTGPR(7) SETB 1                                                    000265
&CCN_ALTGPR(8) SETB 1                                                    000265
&CCN_ALTGPR(9) SETB 0                                                    000265
&CCN_ALTGPR(10) SETB 0                                                   000265
&CCN_ALTGPR(11) SETB 1                                                   000265
&CCN_ALTGPR(12) SETB 0                                                   000265
&CCN_ALTGPR(13) SETB 0                                                   000265
&CCN_ALTGPR(14) SETB 1                                                   000265
&CCN_ALTGPR(15) SETB 1                                                   000265
&CCN_ALTGPR(16) SETB 1                                                   000265
         MYPROLOG                                                        000265
@@BGN@3  DS    0H                                                        000265
         AIF   (NOT &CCN_SASIG).@@NOSIG3                                 000265
         LLILH 9,X'C6F4'                                                 000265
         OILL  9,X'E2C1'                                                 000265
         ST    9,4(,13)                                                  000265
.@@NOSIG3 ANOP                                                           000265
         USING @@AUTO@3,13                                               000265
         LARL  3,@@LIT@3                                                 000265
         USING @@LIT@3,3                                                 000265
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,456(0,13)             #SR_PARM_3                        000265
*  long segments;                                                        000266
*  long origin;                                                          000267
*  int  iarv64_rc = 0;                                                   000268
         LGHI  2,0                                                       000268
         ST    2,@51iarv64_rc@28                                         000268
*                                                                        000269
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000270
*                                                                        000271
*  segments = *numMBSegments;                                            000272
         LG    14,456(0,13)            #SR_PARM_3                        000272
         USING @@PARMD@3,14                                              000272
         LG    14,@48numMBSegments@23                                    000272
         LGF   14,0(0,14)              (*)int                            000272
         STG   14,@53segments@26                                         000272
*  wgetstor = rgetstor;                                                  000273
         MVC   @52wgetstor,@46rgetstor                                   000273
*                                                                        000274
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000275
         LA    4,@54origin@27                                            000275
         DROP  10                                                        000275
         LA    5,@53segments@26                                          000275
         LA    6,@52wgetstor                                             000275
         LG    14,456(0,13)            #SR_PARM_3                        000275
         LG    7,@49ttkn@24                                              000275
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000275
               AMESIZE=4K,SEGMENTS=(5),ORIGIN=(4),TTOKEN=(7),RETCODE=19X 000275
               2(13),MF=(E,(6))                                          000275
         LGR   15,2                                                      000275
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000276
*    "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\     000277
*    ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(tt  000278
*                                                                        000279
*  if (0 != iarv64_rc) {                                                 000280
         LGF   14,@51iarv64_rc@28                                        000280
         LTR   14,14                                                     000280
         BRE   @3L3                                                      000280
*   return (void *)0;                                                    000281
         BRU   @3L20                                                     000281
@3L3     DS    0H                                                        000281
*  }                                                                     000282
*  return (void *)origin;                                                000283
         LG    15,@54origin@27                                           000283
* }                                                                      000284
@3L20    DS    0H                                                        000284
         DROP                                                            000284
         MYEPILOG                                                        000284
OMRIARV64 CSECT ,                                                        000284
         DS    0FD                                                       000284
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
         DC    AL2(30)                                                   000000
         DC    C'omrallocate_4K_pages_above_bar'                         000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@3 DSECT                                                           000000
         DS    58FD                                                      000000
         ORG   @@AUTO@3                                                  000000
#GPR_SA_3 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@3+176                                              000000
@53segments@26 DS FD                                                     000000
         ORG   @@AUTO@3+184                                              000000
@54origin@27 DS FD                                                       000000
         ORG   @@AUTO@3+192                                              000000
@51iarv64_rc@28 DS F                                                     000000
         ORG   @@AUTO@3+200                                              000000
@52wgetstor DS XL256                                                     000000
         ORG   @@AUTO@3+456                                              000000
#SR_PARM_3 DS  XL8                                                       000000
@@PARMD@3 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@3+0                                               000000
@48numMBSegments@23 DS FD                                                000000
         ORG   @@PARMD@3+8                                               000000
@49ttkn@24 DS  FD                                                        000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000298
         ENTRY @@CCN@56                                                  000298
@@CCN@56 AMODE 64                                                        000298
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000298
         DC    A(@@FPB@2-*+8)          Signed offset to FPB              000298
         DC    XL4'00000000'           Reserved                          000298
@@CCN@56 DS    0FD                                                       000298
&CCN_PRCN SETC '@@CCN@56'                                                000298
&CCN_PRCN_LONG SETC 'omrfree_memory_above_bar'                           000298
&CCN_LITN SETC '@@LIT@2'                                                 000298
&CCN_BEGIN SETC '@@BGN@2'                                                000298
&CCN_ASCM SETC 'P'                                                       000298
&CCN_DSASZ SETA 456                                                      000298
&CCN_SASZ SETA 144                                                       000298
&CCN_ARGS SETA 2                                                         000298
&CCN_RLOW SETA 14                                                        000298
&CCN_RHIGH SETA 10                                                       000298
&CCN_NAB SETB  0                                                         000298
&CCN_MAIN SETB 0                                                         000298
&CCN_NAB_STORED SETB 0                                                   000298
&CCN_STATIC SETB 0                                                       000298
&CCN_ALTGPR(1) SETB 1                                                    000298
&CCN_ALTGPR(2) SETB 1                                                    000298
&CCN_ALTGPR(3) SETB 1                                                    000298
&CCN_ALTGPR(4) SETB 1                                                    000298
&CCN_ALTGPR(5) SETB 1                                                    000298
&CCN_ALTGPR(6) SETB 1                                                    000298
&CCN_ALTGPR(7) SETB 0                                                    000298
&CCN_ALTGPR(8) SETB 0                                                    000298
&CCN_ALTGPR(9) SETB 0                                                    000298
&CCN_ALTGPR(10) SETB 0                                                   000298
&CCN_ALTGPR(11) SETB 1                                                   000298
&CCN_ALTGPR(12) SETB 0                                                   000298
&CCN_ALTGPR(13) SETB 0                                                   000298
&CCN_ALTGPR(14) SETB 1                                                   000298
&CCN_ALTGPR(15) SETB 1                                                   000298
&CCN_ALTGPR(16) SETB 1                                                   000298
         MYPROLOG                                                        000298
@@BGN@2  DS    0H                                                        000298
         AIF   (NOT &CCN_SASIG).@@NOSIG2                                 000298
         LLILH 9,X'C6F4'                                                 000298
         OILL  9,X'E2C1'                                                 000298
         ST    9,4(,13)                                                  000298
.@@NOSIG2 ANOP                                                           000298
         USING @@AUTO@2,13                                               000298
         LARL  3,@@LIT@2                                                 000298
         USING @@LIT@2,3                                                 000298
         LARL  10,$STATIC                                                000000
         USING @@STATICD@@,10                                            000000
         STG   1,448(0,13)             #SR_PARM_2                        000298
*  void * xmemobjstart;                                                  000299
*  int  iarv64_rc = 0;                                                   000300
         LGHI  14,0                                                      000300
         ST    14,@60iarv64_rc@31                                        000300
*                                                                        000301
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000302
*                                                                        000303
*  xmemobjstart = address;                                               000304
         LG    14,448(0,13)            #SR_PARM_2                        000304
         USING @@PARMD@2,14                                              000304
         LG    14,@57address                                             000304
         STG   14,@62xmemobjstart                                        000304
*  wgetstor = pgetstor;                                                  000305
         MVC   @61wgetstor,@55pgetstor                                   000305
*                                                                        000306
*  __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%3),  000307
         LA    2,@61wgetstor                                             000307
         DROP  10                                                        000307
         LA    4,@62xmemobjstart                                         000307
         LG    14,448(0,13)            #SR_PARM_2                        000307
         LG    5,@58ttkn@29                                              000307
         IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(4),TTOKEN=(5),RETCX 000307
               ODE=184(13),MF=(E,(2))                                    000307
*    ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(ttkn));      000308
*  return iarv64_rc;                                                     000309
         LGF   15,@60iarv64_rc@31                                        000309
* }                                                                      000310
@2L21    DS    0H                                                        000310
         DROP                                                            000310
         MYEPILOG                                                        000310
OMRIARV64 CSECT ,                                                        000310
         DS    0FD                                                       000310
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
         DC    AL2(24)                                                   000000
         DC    C'omrfree_memory_above_bar'                               000000
OMRIARV64 LOCTR                                                          000000
         EJECT                                                           000000
@@AUTO@2 DSECT                                                           000000
         DS    57FD                                                      000000
         ORG   @@AUTO@2                                                  000000
#GPR_SA_2 DS   18FD                                                      000000
         DS    FD                                                        000000
         ORG   @@AUTO@2+176                                              000000
@62xmemobjstart DS AD                                                    000000
         ORG   @@AUTO@2+184                                              000000
@60iarv64_rc@31 DS F                                                     000000
         ORG   @@AUTO@2+192                                              000000
@61wgetstor DS XL256                                                     000000
         ORG   @@AUTO@2+448                                              000000
#SR_PARM_2 DS  XL8                                                       000000
@@PARMD@2 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@2+0                                               000000
@57address DS  FD                                                        000000
         ORG   @@PARMD@2+8                                               000000
@58ttkn@29 DS  FD                                                        000000
*                                                                        000349
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
@@CONST@AREA@@ DS 0D                                                     000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         ORG   @@CONST@AREA@@+0                                          000000
         DC    A(@7L42-@@LIT@7)                                          000000
         DC    A(@7L43-@@LIT@7)                                          000000
         DC    A(@7L44-@@LIT@7)                                          000000
         ORG   @@CONST@AREA@@+12                                         000000
         DC    A(@6L36-@@LIT@6)                                          000000
         DC    A(@6L37-@@LIT@6)                                          000000
         DC    A(@6L38-@@LIT@6)                                          000000
         ORG   @@CONST@AREA@@+24                                         000000
         DC    A(@5L30-@@LIT@5)                                          000000
         DC    A(@5L31-@@LIT@5)                                          000000
         DC    A(@5L32-@@LIT@5)                                          000000
         ORG   @@CONST@AREA@@+36                                         000000
         DC    A(@4L24-@@LIT@4)                                          000000
         DC    A(@4L25-@@LIT@4)                                          000000
         DC    A(@4L26-@@LIT@4)                                          000000
         ORG   ,                                                         000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
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
@46rgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1280                                          000000
@55pgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1536                                          000000
@63qgetstor DS XL256                                                     000000
         END   ,(5650ZOS   ,2100,16197)                                  000000
