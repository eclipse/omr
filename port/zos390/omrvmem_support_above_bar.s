***********************************************************************
* Copyright (c) 1991, 2019 IBM Corp. and others
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
* SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
* GPL-2.0 WITH Classpath-exception-2.0 OR
* LicenseRef-GPL-2.0 WITH Assembly-exception
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
@@CCN@69 ALIAS C'omrdiscard_data'                                        000000
@@CCN@61 ALIAS C'omrfree_memory_above_bar'                               000000
@@CCN@51 ALIAS C'omrallocate_4K_pages_above_bar'                         000000
@@CCN@39 ALIAS C'omrallocate_4K_pages_in_userExtendedPrivateArea'        000000
@@CCN@27 ALIAS C'omrallocate_2G_pages'                                   000000
@@CCN@15 ALIAS C'omrallocate_1M_pageable_pages_above_bar'                000000
@@CCN@2  ALIAS C'omrallocate_1M_fixed_pages'                             000000
* #include "omriarv64.h"                                                 000001
*                                                                        000002
* #pragma prolog(omrallocate_1M_fixed_pages,"MYPROLOG")                  000003
* #pragma epilog(omrallocate_1M_fixed_pages,"MYEPILOG")                  000004
*                                                                        000005
* __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));          000006
*                                                                        000007
* /*                                                                     000008
*  * Allocate 1MB fixed pages using IARV64 system macro.                 000009
*  * Memory allocated is freed using omrfree_memory_above_bar().         000010
*  *                                                                     000011
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000012
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000013
*  *                                                                     000014
*  * @return pointer to memory allocated, NULL on failure.               000015
*  */                                                                    000016
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000017
*  long segments;                                                        000018
*  long origin;                                                          000019
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000020
*  int  iarv64_rc = 0;                                                   000021
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000022
*                                                                        000023
*  segments = *numMBSegments;                                            000024
*  wgetstor = lgetstor;                                                  000025
*                                                                        000026
*  switch (useMemoryType) {                                              000027
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000028
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000029
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000030
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000031
*   break;                                                               000032
*  case ZOS64_VMEM_2_TO_32G:                                             000033
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000034
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000035
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000036
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000037
*   break;                                                               000038
*  case ZOS64_VMEM_2_TO_64G:                                             000039
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000040
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000041
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000042
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000043
*   break;                                                               000044
*  }                                                                     000045
*                                                                        000046
*  if (0 != iarv64_rc) {                                                 000047
*   return (void *)0;                                                    000048
*  }                                                                     000049
*  return (void *)origin;                                                000050
* }                                                                      000051
*                                                                        000052
* #pragma prolog(omrallocate_1M_pageable_pages_above_bar,"MYPROLOG")     000053
* #pragma epilog(omrallocate_1M_pageable_pages_above_bar,"MYEPILOG")     000054
*                                                                        000055
* __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));          000056
*                                                                        000057
* /*                                                                     000058
*  * Allocate 1MB pageable pages above 2GB bar using IARV64 system macr  000059
*  * Memory allocated is freed using omrfree_memory_above_bar().         000060
*  *                                                                     000061
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000062
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000063
*  *                                                                     000064
*  * @return pointer to memory allocated, NULL on failure.               000065
*  */                                                                    000066
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000067
*  long segments;                                                        000068
*  long origin;                                                          000069
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000070
*  int  iarv64_rc = 0;                                                   000071
*                                                                        000072
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000073
*                                                                        000074
*  segments = *numMBSegments;                                            000075
*  wgetstor = ngetstor;                                                  000076
*                                                                        000077
*  switch (useMemoryType) {                                              000078
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000079
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000080
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000081
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000082
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000083
*   break;                                                               000084
*  case ZOS64_VMEM_2_TO_32G:                                             000085
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000086
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000087
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000088
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000089
*   break;                                                               000090
*  case ZOS64_VMEM_2_TO_64G:                                             000091
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000092
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000093
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000094
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000095
*   break;                                                               000096
*  }                                                                     000097
*                                                                        000098
*  if (0 != iarv64_rc) {                                                 000099
*   return (void *)0;                                                    000100
*  }                                                                     000101
*  return (void *)origin;                                                000102
* }                                                                      000103
*                                                                        000104
* #pragma prolog(omrallocate_2G_pages,"MYPROLOG")                        000105
* #pragma epilog(omrallocate_2G_pages,"MYEPILOG")                        000106
*                                                                        000107
* __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));          000108
*                                                                        000109
* /*                                                                     000110
*  * Allocate 2GB fixed pages using IARV64 system macro.                 000111
*  * Memory allocated is freed using omrfree_memory_above_bar().         000112
*  *                                                                     000113
*  * @params[in] num2GBUnits Number of 2GB units to be allocated         000114
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000115
*  *                                                                     000116
*  * @return pointer to memory allocated, NULL on failure.               000117
*  */                                                                    000118
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000119
*  long units;                                                           000120
*  long origin;                                                          000121
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000122
*  int  iarv64_rc = 0;                                                   000123
*                                                                        000124
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000125
*                                                                        000126
*  units = *num2GBUnits;                                                 000127
*  wgetstor = ogetstor;                                                  000128
*                                                                        000129
*  switch (useMemoryType) {                                              000130
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000131
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000132
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000133
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000134
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000135
*   break;                                                               000136
*  case ZOS64_VMEM_2_TO_32G:                                             000137
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000138
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000139
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000140
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000141
*   break;                                                               000142
*  case ZOS64_VMEM_2_TO_64G:                                             000143
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000144
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000145
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000146
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000147
*   break;                                                               000148
*  }                                                                     000149
*                                                                        000150
*  if (0 != iarv64_rc) {                                                 000151
*   return (void *)0;                                                    000152
*  }                                                                     000153
*  return (void *)origin;                                                000154
* }                                                                      000155
*                                                                        000156
* #pragma prolog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYPRO  000157
* #pragma epilog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYEPI  000158
*                                                                        000159
* __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));          000160
*                                                                        000161
* /*                                                                     000162
*  * Allocate 4KB pages in 2G-32G range using IARV64 system macro.       000163
*  * Memory allocated is freed using omrfree_memory_above_bar().         000164
*  *                                                                     000165
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000166
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000167
*  *                                                                     000168
*  * @return pointer to memory allocated, NULL on failure.               000169
*  */                                                                    000170
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000171
*  long segments;                                                        000172
*  long origin;                                                          000173
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000174
*  int  iarv64_rc = 0;                                                   000175
*                                                                        000176
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000177
*                                                                        000178
*  segments = *numMBSegments;                                            000179
*  wgetstor = mgetstor;                                                  000180
*                                                                        000181
*  switch (useMemoryType) {                                              000182
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000183
*   break;                                                               000184
*  case ZOS64_VMEM_2_TO_32G:                                             000185
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000186
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000187
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000188
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000189
*   break;                                                               000190
*  case ZOS64_VMEM_2_TO_64G:                                             000191
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000192
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000193
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000194
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000195
*   break;                                                               000196
*  }                                                                     000197
*                                                                        000198
*  if (0 != iarv64_rc) {                                                 000199
*   return (void *)0;                                                    000200
*  }                                                                     000201
*  return (void *)origin;                                                000202
* }                                                                      000203
*                                                                        000204
* #pragma prolog(omrallocate_4K_pages_above_bar,"MYPROLOG")              000205
* #pragma epilog(omrallocate_4K_pages_above_bar,"MYEPILOG")              000206
*                                                                        000207
* __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));          000208
*                                                                        000209
* /*                                                                     000210
*  * Allocate 4KB pages using IARV64 system macro.                       000211
*  * Memory allocated is freed using omrfree_memory_above_bar().         000212
*  *                                                                     000213
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000214
*  *                                                                     000215
*  * @return pointer to memory allocated, NULL on failure.               000216
*  */                                                                    000217
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000218
*  long segments;                                                        000219
*  long origin;                                                          000220
*  int  iarv64_rc = 0;                                                   000221
*                                                                        000222
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000223
*                                                                        000224
*  segments = *numMBSegments;                                            000225
*  wgetstor = rgetstor;                                                  000226
*                                                                        000227
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000228
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000229
*             "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(  000230
*             ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetsto  000231
*                                                                        000232
*  if (0 != iarv64_rc) {                                                 000233
*   return (void *)0;                                                    000234
*  }                                                                     000235
*  return (void *)origin;                                                000236
* }                                                                      000237
*                                                                        000238
* #pragma prolog(omrfree_memory_above_bar,"MYPROLOG")                    000239
* #pragma epilog(omrfree_memory_above_bar,"MYEPILOG")                    000240
*                                                                        000241
* __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));          000242
*                                                                        000243
* /*                                                                     000244
*  * Free memory allocated using IARV64 system macro.                    000245
*  *                                                                     000246
*  * @params[in] address pointer to memory region to be freed            000247
*  *                                                                     000248
*  * @return non-zero if memory is not freed successfully, 0 otherwise.  000249
*  */                                                                    000250
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000251
*  void * xmemobjstart;                                                  000252
*  int  iarv64_rc = 0;                                                   000253
*                                                                        000254
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000255
*                                                                        000256
*  xmemobjstart = address;                                               000257
*  wgetstor = pgetstor;                                                  000258
*                                                                        000259
*     __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%  000260
*             ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(tt  000261
*  return iarv64_rc;                                                     000262
* }                                                                      000263
*                                                                        000264
* #pragma prolog(omrdiscard_data,"MYPROLOG")                             000265
* #pragma epilog(omrdiscard_data,"MYEPILOG")                             000266
*                                                                        000267
* __asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));          000268
*                                                                        000269
* /* Used to pass parameters to IARV64 DISCARDDATA in omrdiscard_data()  000270
* struct rangeList {                                                     000271
*  void *startAddr;                                                      000272
*  long pageCount;                                                       000273
* };                                                                     000274
*                                                                        000275
* /*                                                                     000276
*  * Discard memory allocated using IARV64 system macro.                 000277
*  *                                                                     000278
*  * @params[in] address pointer to memory region to be discarded        000279
*  * @params[in] numFrames number of frames to be discarded. Frame size  000280
*  *                                                                     000281
*  * @return non-zero if memory is not discarded successfully, 0 otherw  000282
*  */                                                                    000283
* int omrdiscard_data(void *address, int *numFrames) {                   000284
         J     @@CCN@69                                                  000284
@@PFD@@  DC    XL8'00C300C300D50000'   Prefix Data Marker                000284
         DC    CL8'20190528'           Compiled Date YYYYMMDD            000284
         DC    CL6'141611'             Compiled Time HHMMSS              000284
         DC    XL4'42010001'           Compiler Version                  000284
         DC    XL2'0000'               Reserved                          000284
         DC    BL1'00000000'           Flag Set 1                        000284
         DC    BL1'00000000'           Flag Set 2                        000284
         DC    BL1'00000000'           Flag Set 3                        000284
         DC    BL1'00000000'           Flag Set 4                        000284
         DC    XL4'00000000'           Reserved                          000284
         ENTRY @@CCN@69                                                  000284
@@CCN@69 AMODE 64                                                        000284
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000284
         DC    A(@@FPB@1-*+8)          Signed offset to FPB              000284
         DC    XL4'00000000'           Reserved                          000284
@@CCN@69 DS    0FD                                                       000284
&CCN_PRCN SETC '@@CCN@69'                                                000284
&CCN_PRCN_LONG SETC 'omrdiscard_data'                                    000284
&CCN_LITN SETC '@@LIT@1'                                                 000284
&CCN_BEGIN SETC '@@BGN@1'                                                000284
&CCN_ASCM SETC 'P'                                                       000284
&CCN_DSASZ SETA 472                                                      000284
&CCN_SASZ SETA 144                                                       000284
&CCN_ARGS SETA 2                                                         000284
&CCN_RLOW SETA 14                                                        000284
&CCN_RHIGH SETA 4                                                        000284
&CCN_NAB SETB  0                                                         000284
&CCN_MAIN SETB 0                                                         000284
&CCN_NAB_STORED SETB 0                                                   000284
&CCN_STATIC SETB 0                                                       000284
&CCN_ALTGPR(1) SETB 1                                                    000284
&CCN_ALTGPR(2) SETB 1                                                    000284
&CCN_ALTGPR(3) SETB 1                                                    000284
&CCN_ALTGPR(4) SETB 1                                                    000284
&CCN_ALTGPR(5) SETB 1                                                    000284
&CCN_ALTGPR(6) SETB 0                                                    000284
&CCN_ALTGPR(7) SETB 0                                                    000284
&CCN_ALTGPR(8) SETB 0                                                    000284
&CCN_ALTGPR(9) SETB 0                                                    000284
&CCN_ALTGPR(10) SETB 0                                                   000284
&CCN_ALTGPR(11) SETB 0                                                   000284
&CCN_ALTGPR(12) SETB 0                                                   000284
&CCN_ALTGPR(13) SETB 0                                                   000284
&CCN_ALTGPR(14) SETB 1                                                   000284
&CCN_ALTGPR(15) SETB 1                                                   000284
&CCN_ALTGPR(16) SETB 1                                                   000284
         MYPROLOG                                                        000284
@@BGN@1  DS    0H                                                        000284
         AIF   (NOT &CCN_SASIG).@@NOSIG1                                 000284
         LLILH 4,X'C6F4'                                                 000284
         OILL  4,X'E2C1'                                                 000284
         ST    4,4(,13)                                                  000284
.@@NOSIG1 ANOP                                                           000284
         USING @@AUTO@1,13                                               000284
         LARL  3,@@LIT@1                                                 000284
         USING @@LIT@1,3                                                 000284
         STG   1,464(0,13)             #SR_PARM_1                        000284
*  struct rangeList range;                                               000285
*  void *rangePtr;                                                       000286
*  int iarv64_rc = 0;                                                    000287
         LGHI  14,0                                                      000287
         ST    14,@73iarv64_rc@38                                        000287
*                                                                        000288
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000289
*                                                                        000290
*  range.startAddr = address;                                            000291
         LG    14,464(0,13)            #SR_PARM_1                        000291
         USING @@PARMD@1,14                                              000291
         LG    14,@70address@36                                          000291
         STG   14,176(0,13)            range_rangeList_startAddr         000291
*  range.pageCount = *numFrames;                                         000292
         LG    14,464(0,13)            #SR_PARM_1                        000292
         LG    14,@71numFrames                                           000292
         LGF   14,0(0,14)              (*)int                            000292
         STG   14,184(0,13)            range_rangeList_pageCount         000292
*  rangePtr = (void *)&range;                                            000293
         LA    14,@75range                                               000293
         STG   14,@78rangePtr                                            000293
*  wgetstor = qgetstor;                                                  000294
         LARL  14,$STATIC                                                000294
         DROP  14                                                        000294
         USING @@STATICD@@,14                                            000294
         MVC   @74wgetstor,@68qgetstor                                   000294
*                                                                        000295
*  __asm(" IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,"\                     000296
         LA    2,@78rangePtr                                             000296
         DROP  14                                                        000296
         LA    4,@74wgetstor                                             000296
         IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,RANGLIST=(2),RETCODE=20X 000296
               0(13),MF=(E,(4))                                          000296
*    "RANGLIST=(%1),RETCODE=%0,MF=(E,(%2))"\                             000297
*    ::"m"(iarv64_rc),"r"(&rangePtr),"r"(&wgetstor));                    000298
*                                                                        000299
*  return iarv64_rc;                                                     000300
         LGF   15,@73iarv64_rc@38                                        000300
* }                                                                      000301
@1L22    DS    0H                                                        000301
         DROP                                                            000301
         MYEPILOG                                                        000301
OMRIARV64 CSECT ,                                                        000301
         DS    0FD                                                       000301
@@LIT@1  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@1  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111100000000011'   Saved GPR Mask                    000000
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
@75range DS    XL16                                                      000000
         ORG   @@AUTO@1+192                                              000000
@78rangePtr DS AD                                                        000000
         ORG   @@AUTO@1+200                                              000000
@73iarv64_rc@38 DS F                                                     000000
         ORG   @@AUTO@1+208                                              000000
@74wgetstor DS XL256                                                     000000
         ORG   @@AUTO@1+464                                              000000
#SR_PARM_1 DS  XL8                                                       000000
@@PARMD@1 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@1+0                                               000000
@70address@36 DS FD                                                      000000
         ORG   @@PARMD@1+8                                               000000
@71numFrames DS FD                                                       000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000017
         ENTRY @@CCN@2                                                   000017
@@CCN@2  AMODE 64                                                        000017
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000017
         DC    A(@@FPB@7-*+8)          Signed offset to FPB              000017
         DC    XL4'00000000'           Reserved                          000017
@@CCN@2  DS    0FD                                                       000017
&CCN_PRCN SETC '@@CCN@2'                                                 000017
&CCN_PRCN_LONG SETC 'omrallocate_1M_fixed_pages'                         000017
&CCN_LITN SETC '@@LIT@7'                                                 000017
&CCN_BEGIN SETC '@@BGN@7'                                                000017
&CCN_ASCM SETC 'P'                                                       000017
&CCN_DSASZ SETA 480                                                      000017
&CCN_SASZ SETA 144                                                       000017
&CCN_ARGS SETA 4                                                         000017
&CCN_RLOW SETA 14                                                        000017
&CCN_RHIGH SETA 7                                                        000017
&CCN_NAB SETB  0                                                         000017
&CCN_MAIN SETB 0                                                         000017
&CCN_NAB_STORED SETB 0                                                   000017
&CCN_STATIC SETB 0                                                       000017
&CCN_ALTGPR(1) SETB 1                                                    000017
&CCN_ALTGPR(2) SETB 1                                                    000017
&CCN_ALTGPR(3) SETB 1                                                    000017
&CCN_ALTGPR(4) SETB 1                                                    000017
&CCN_ALTGPR(5) SETB 1                                                    000017
&CCN_ALTGPR(6) SETB 1                                                    000017
&CCN_ALTGPR(7) SETB 1                                                    000017
&CCN_ALTGPR(8) SETB 1                                                    000017
&CCN_ALTGPR(9) SETB 0                                                    000017
&CCN_ALTGPR(10) SETB 0                                                   000017
&CCN_ALTGPR(11) SETB 0                                                   000017
&CCN_ALTGPR(12) SETB 0                                                   000017
&CCN_ALTGPR(13) SETB 0                                                   000017
&CCN_ALTGPR(14) SETB 1                                                   000017
&CCN_ALTGPR(15) SETB 1                                                   000017
&CCN_ALTGPR(16) SETB 1                                                   000017
         MYPROLOG                                                        000017
@@BGN@7  DS    0H                                                        000017
         AIF   (NOT &CCN_SASIG).@@NOSIG7                                 000017
         LLILH 7,X'C6F4'                                                 000017
         OILL  7,X'E2C1'                                                 000017
         ST    7,4(,13)                                                  000017
.@@NOSIG7 ANOP                                                           000017
         USING @@AUTO@7,13                                               000017
         LARL  3,@@LIT@7                                                 000017
         USING @@LIT@7,3                                                 000017
         STG   1,464(0,13)             #SR_PARM_7                        000017
*  long segments;                                                        000018
*  long origin;                                                          000019
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000020
         LG    14,464(0,13)            #SR_PARM_7                        000020
         USING @@PARMD@7,14                                              000020
         LG    14,@4userExtendedPrivateAreaMemoryType                    000020
         LGF   14,0(0,14)              (*)int                            000020
         STG   14,@8useMemoryType                                        000020
*  int  iarv64_rc = 0;                                                   000021
         LGHI  14,0                                                      000021
         ST    14,@10iarv64_rc                                           000021
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000022
*                                                                        000023
*  segments = *numMBSegments;                                            000024
         LG    14,464(0,13)            #SR_PARM_7                        000024
         LG    14,@3numMBSegments                                        000024
         LGF   14,0(0,14)              (*)int                            000024
         STG   14,@12segments                                            000024
*  wgetstor = lgetstor;                                                  000025
         LARL  14,$STATIC                                                000025
         DROP  14                                                        000025
         USING @@STATICD@@,14                                            000025
         MVC   @11wgetstor,@1lgetstor                                    000025
*                                                                        000026
*  switch (useMemoryType) {                                              000027
         LG    14,@8useMemoryType                                        000027
         STG   14,472(0,13)            #SW_WORK7                         000027
         CLG   14,=X'0000000000000002'                                   000027
         BRH   @7L41                                                     000027
         LG    14,472(0,13)            #SW_WORK7                         000027
         SLLG  14,14,2                                                   000027
         LGFR  15,14                                                     000027
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,0(15,14)                                               000027
         B     0(3,14)                                                   000027
@7L41    DS    0H                                                        000027
         BRU   @7L46                                                     000027
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000028
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAG  000029
@7L42    DS    0H                                                        000029
         LA    2,@13origin                                               000029
         DROP  14                                                        000029
         LA    4,@12segments                                             000029
         LA    5,@11wgetstor                                             000029
         LG    14,464(0,13)            #SR_PARM_7                        000029
         USING @@PARMD@7,14                                              000029
         LG    6,@5ttkn                                                  000029
         LA    7,@6startAddress                                          000029
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000029
               AMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=X 000029
               200(13),MF=(E,(5)),INORIGIN=(7)                           000029
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000030
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000031
*   break;                                                               000032
         BRU   @7L12                                                     000032
*  case ZOS64_VMEM_2_TO_32G:                                             000033
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000034
@7L43    DS    0H                                                        000034
         LA    2,@13origin                                               000034
         LA    4,@12segments                                             000034
         LA    5,@11wgetstor                                             000034
         LG    14,464(0,13)            #SR_PARM_7                        000034
         LG    6,@5ttkn                                                  000034
         LA    7,@6startAddress                                          000034
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000034
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000034
               EN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)            000034
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000035
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000036
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000037
*   break;                                                               000038
         BRU   @7L12                                                     000038
*  case ZOS64_VMEM_2_TO_64G:                                             000039
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000040
@7L44    DS    0H                                                        000040
         LA    2,@13origin                                               000040
         LA    4,@12segments                                             000040
         LA    5,@11wgetstor                                             000040
         LG    14,464(0,13)            #SR_PARM_7                        000040
         LG    6,@5ttkn                                                  000040
         LA    7,@6startAddress                                          000040
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000040
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000040
               EN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)            000040
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                              000041
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000042
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000043
*   break;                                                               000044
@7L12    DS    0H                                                        000027
@7L46    DS    0H                                                        000000
*  }                                                                     000045
*                                                                        000046
*  if (0 != iarv64_rc) {                                                 000047
         LGF   14,@10iarv64_rc                                           000047
         LTR   14,14                                                     000047
         BRE   @7L11                                                     000047
*   return (void *)0;                                                    000048
         LGHI  15,0                                                      000048
         BRU   @7L13                                                     000048
@7L11    DS    0H                                                        000048
*  }                                                                     000049
*  return (void *)origin;                                                000050
         LG    15,@13origin                                              000050
* }                                                                      000051
@7L13    DS    0H                                                        000051
         DROP                                                            000051
         MYEPILOG                                                        000051
OMRIARV64 CSECT ,                                                        000051
         DS    0FD                                                       000051
@@LIT@7  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@7  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111100000011'   Saved GPR Mask                    000000
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
@12segments DS FD                                                        000000
         ORG   @@AUTO@7+184                                              000000
@13origin DS   FD                                                        000000
         ORG   @@AUTO@7+192                                              000000
@8useMemoryType DS FD                                                    000000
         ORG   @@AUTO@7+200                                              000000
@10iarv64_rc DS F                                                        000000
         ORG   @@AUTO@7+208                                              000000
@11wgetstor DS XL256                                                     000000
         ORG   @@AUTO@7+464                                              000000
#SR_PARM_7 DS  XL8                                                       000000
@@PARMD@7 DSECT                                                          000000
         DS    XL32                                                      000000
         ORG   @@PARMD@7+0                                               000000
@3numMBSegments DS FD                                                    000000
         ORG   @@PARMD@7+8                                               000000
@4userExtendedPrivateAreaMemoryType DS FD                                000000
         ORG   @@PARMD@7+16                                              000000
@5ttkn   DS    FD                                                        000000
         ORG   @@PARMD@7+24                                              000000
@6startAddress DS FD                                                     000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000067
         ENTRY @@CCN@15                                                  000067
@@CCN@15 AMODE 64                                                        000067
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000067
         DC    A(@@FPB@6-*+8)          Signed offset to FPB              000067
         DC    XL4'00000000'           Reserved                          000067
@@CCN@15 DS    0FD                                                       000067
&CCN_PRCN SETC '@@CCN@15'                                                000067
&CCN_PRCN_LONG SETC 'omrallocate_1M_pageable_pages_above_bar'            000067
&CCN_LITN SETC '@@LIT@6'                                                 000067
&CCN_BEGIN SETC '@@BGN@6'                                                000067
&CCN_ASCM SETC 'P'                                                       000067
&CCN_DSASZ SETA 480                                                      000067
&CCN_SASZ SETA 144                                                       000067
&CCN_ARGS SETA 4                                                         000067
&CCN_RLOW SETA 14                                                        000067
&CCN_RHIGH SETA 7                                                        000067
&CCN_NAB SETB  0                                                         000067
&CCN_MAIN SETB 0                                                         000067
&CCN_NAB_STORED SETB 0                                                   000067
&CCN_STATIC SETB 0                                                       000067
&CCN_ALTGPR(1) SETB 1                                                    000067
&CCN_ALTGPR(2) SETB 1                                                    000067
&CCN_ALTGPR(3) SETB 1                                                    000067
&CCN_ALTGPR(4) SETB 1                                                    000067
&CCN_ALTGPR(5) SETB 1                                                    000067
&CCN_ALTGPR(6) SETB 1                                                    000067
&CCN_ALTGPR(7) SETB 1                                                    000067
&CCN_ALTGPR(8) SETB 1                                                    000067
&CCN_ALTGPR(9) SETB 0                                                    000067
&CCN_ALTGPR(10) SETB 0                                                   000067
&CCN_ALTGPR(11) SETB 0                                                   000067
&CCN_ALTGPR(12) SETB 0                                                   000067
&CCN_ALTGPR(13) SETB 0                                                   000067
&CCN_ALTGPR(14) SETB 1                                                   000067
&CCN_ALTGPR(15) SETB 1                                                   000067
&CCN_ALTGPR(16) SETB 1                                                   000067
         MYPROLOG                                                        000067
@@BGN@6  DS    0H                                                        000067
         AIF   (NOT &CCN_SASIG).@@NOSIG6                                 000067
         LLILH 7,X'C6F4'                                                 000067
         OILL  7,X'E2C1'                                                 000067
         ST    7,4(,13)                                                  000067
.@@NOSIG6 ANOP                                                           000067
         USING @@AUTO@6,13                                               000067
         LARL  3,@@LIT@6                                                 000067
         USING @@LIT@6,3                                                 000067
         STG   1,464(0,13)             #SR_PARM_6                        000067
*  long segments;                                                        000068
*  long origin;                                                          000069
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000070
         LG    14,464(0,13)            #SR_PARM_6                        000070
         USING @@PARMD@6,14                                              000070
         LG    14,@17userExtendedPrivateAreaMemoryType@2                 000070
         LGF   14,0(0,14)              (*)int                            000070
         STG   14,@21useMemoryType@8                                     000070
*  int  iarv64_rc = 0;                                                   000071
         LGHI  14,0                                                      000071
         ST    14,@22iarv64_rc@9                                         000071
*                                                                        000072
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000073
*                                                                        000074
*  segments = *numMBSegments;                                            000075
         LG    14,464(0,13)            #SR_PARM_6                        000075
         LG    14,@16numMBSegments@1                                     000075
         LGF   14,0(0,14)              (*)int                            000075
         STG   14,@24segments@6                                          000075
*  wgetstor = ngetstor;                                                  000076
         LARL  14,$STATIC                                                000076
         DROP  14                                                        000076
         USING @@STATICD@@,14                                            000076
         MVC   @23wgetstor,@14ngetstor                                   000076
*                                                                        000077
*  switch (useMemoryType) {                                              000078
         LG    14,@21useMemoryType@8                                     000078
         STG   14,472(0,13)            #SW_WORK6                         000078
         CLG   14,=X'0000000000000002'                                   000078
         BRH   @6L35                                                     000078
         LG    14,472(0,13)            #SW_WORK6                         000078
         SLLG  14,14,2                                                   000078
         LGFR  15,14                                                     000078
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,12(15,14)                                              000078
         B     0(3,14)                                                   000078
@6L35    DS    0H                                                        000078
         BRU   @6L40                                                     000078
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000079
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000080
@6L36    DS    0H                                                        000080
         LA    2,@25origin@7                                             000080
         DROP  14                                                        000080
         LA    4,@24segments@6                                           000080
         LA    5,@23wgetstor                                             000080
         LG    14,464(0,13)            #SR_PARM_6                        000080
         USING @@PARMD@6,14                                              000080
         LG    6,@18ttkn@3                                               000080
         LA    7,@19startAddress@4                                       000080
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000080
               AMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(4),ORIGIN=(X 000080
               2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)     000080
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000081
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000082
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000083
*   break;                                                               000084
         BRU   @6L14                                                     000084
*  case ZOS64_VMEM_2_TO_32G:                                             000085
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000086
@6L37    DS    0H                                                        000086
         LA    2,@25origin@7                                             000086
         LA    4,@24segments@6                                           000086
         LA    5,@23wgetstor                                             000086
         LG    14,464(0,13)            #SR_PARM_6                        000086
         LG    6,@18ttkn@3                                               000086
         LA    7,@19startAddress@4                                       000086
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000086
               O32G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000086
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),X 000086
               INORIGIN=(7)                                              000086
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000087
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000088
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000089
*   break;                                                               000090
         BRU   @6L14                                                     000090
*  case ZOS64_VMEM_2_TO_64G:                                             000091
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000092
@6L38    DS    0H                                                        000092
         LA    2,@25origin@7                                             000092
         LA    4,@24segments@6                                           000092
         LA    5,@23wgetstor                                             000092
         LG    14,464(0,13)            #SR_PARM_6                        000092
         LG    6,@18ttkn@3                                               000092
         LA    7,@19startAddress@4                                       000092
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000092
               O64G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000092
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),X 000092
               INORIGIN=(7)                                              000092
*     "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\         000093
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000094
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000095
*   break;                                                               000096
@6L14    DS    0H                                                        000078
@6L40    DS    0H                                                        000000
*  }                                                                     000097
*                                                                        000098
*  if (0 != iarv64_rc) {                                                 000099
         LGF   14,@22iarv64_rc@9                                         000099
         LTR   14,14                                                     000099
         BRE   @6L9                                                      000099
*   return (void *)0;                                                    000100
         LGHI  15,0                                                      000100
         BRU   @6L15                                                     000100
@6L9     DS    0H                                                        000100
*  }                                                                     000101
*  return (void *)origin;                                                000102
         LG    15,@25origin@7                                            000102
* }                                                                      000103
@6L15    DS    0H                                                        000103
         DROP                                                            000103
         MYEPILOG                                                        000103
OMRIARV64 CSECT ,                                                        000103
         DS    0FD                                                       000103
@@LIT@6  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@6  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111100000011'   Saved GPR Mask                    000000
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
@24segments@6 DS FD                                                      000000
         ORG   @@AUTO@6+184                                              000000
@25origin@7 DS FD                                                        000000
         ORG   @@AUTO@6+192                                              000000
@21useMemoryType@8 DS FD                                                 000000
         ORG   @@AUTO@6+200                                              000000
@22iarv64_rc@9 DS F                                                      000000
         ORG   @@AUTO@6+208                                              000000
@23wgetstor DS XL256                                                     000000
         ORG   @@AUTO@6+464                                              000000
#SR_PARM_6 DS  XL8                                                       000000
@@PARMD@6 DSECT                                                          000000
         DS    XL32                                                      000000
         ORG   @@PARMD@6+0                                               000000
@16numMBSegments@1 DS FD                                                 000000
         ORG   @@PARMD@6+8                                               000000
@17userExtendedPrivateAreaMemoryType@2 DS FD                             000000
         ORG   @@PARMD@6+16                                              000000
@18ttkn@3 DS   FD                                                        000000
         ORG   @@PARMD@6+24                                              000000
@19startAddress@4 DS FD                                                  000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000119
         ENTRY @@CCN@27                                                  000119
@@CCN@27 AMODE 64                                                        000119
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000119
         DC    A(@@FPB@5-*+8)          Signed offset to FPB              000119
         DC    XL4'00000000'           Reserved                          000119
@@CCN@27 DS    0FD                                                       000119
&CCN_PRCN SETC '@@CCN@27'                                                000119
&CCN_PRCN_LONG SETC 'omrallocate_2G_pages'                               000119
&CCN_LITN SETC '@@LIT@5'                                                 000119
&CCN_BEGIN SETC '@@BGN@5'                                                000119
&CCN_ASCM SETC 'P'                                                       000119
&CCN_DSASZ SETA 480                                                      000119
&CCN_SASZ SETA 144                                                       000119
&CCN_ARGS SETA 4                                                         000119
&CCN_RLOW SETA 14                                                        000119
&CCN_RHIGH SETA 7                                                        000119
&CCN_NAB SETB  0                                                         000119
&CCN_MAIN SETB 0                                                         000119
&CCN_NAB_STORED SETB 0                                                   000119
&CCN_STATIC SETB 0                                                       000119
&CCN_ALTGPR(1) SETB 1                                                    000119
&CCN_ALTGPR(2) SETB 1                                                    000119
&CCN_ALTGPR(3) SETB 1                                                    000119
&CCN_ALTGPR(4) SETB 1                                                    000119
&CCN_ALTGPR(5) SETB 1                                                    000119
&CCN_ALTGPR(6) SETB 1                                                    000119
&CCN_ALTGPR(7) SETB 1                                                    000119
&CCN_ALTGPR(8) SETB 1                                                    000119
&CCN_ALTGPR(9) SETB 0                                                    000119
&CCN_ALTGPR(10) SETB 0                                                   000119
&CCN_ALTGPR(11) SETB 0                                                   000119
&CCN_ALTGPR(12) SETB 0                                                   000119
&CCN_ALTGPR(13) SETB 0                                                   000119
&CCN_ALTGPR(14) SETB 1                                                   000119
&CCN_ALTGPR(15) SETB 1                                                   000119
&CCN_ALTGPR(16) SETB 1                                                   000119
         MYPROLOG                                                        000119
@@BGN@5  DS    0H                                                        000119
         AIF   (NOT &CCN_SASIG).@@NOSIG5                                 000119
         LLILH 7,X'C6F4'                                                 000119
         OILL  7,X'E2C1'                                                 000119
         ST    7,4(,13)                                                  000119
.@@NOSIG5 ANOP                                                           000119
         USING @@AUTO@5,13                                               000119
         LARL  3,@@LIT@5                                                 000119
         USING @@LIT@5,3                                                 000119
         STG   1,464(0,13)             #SR_PARM_5                        000119
*  long units;                                                           000120
*  long origin;                                                          000121
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000122
         LG    14,464(0,13)            #SR_PARM_5                        000122
         USING @@PARMD@5,14                                              000122
         LG    14,@29userExtendedPrivateAreaMemoryType@10                000122
         LGF   14,0(0,14)              (*)int                            000122
         STG   14,@33useMemoryType@15                                    000122
*  int  iarv64_rc = 0;                                                   000123
         LGHI  14,0                                                      000123
         ST    14,@34iarv64_rc@16                                        000123
*                                                                        000124
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000125
*                                                                        000126
*  units = *num2GBUnits;                                                 000127
         LG    14,464(0,13)            #SR_PARM_5                        000127
         LG    14,@28num2GBUnits                                         000127
         LGF   14,0(0,14)              (*)int                            000127
         STG   14,@36units                                               000127
*  wgetstor = ogetstor;                                                  000128
         LARL  14,$STATIC                                                000128
         DROP  14                                                        000128
         USING @@STATICD@@,14                                            000128
         MVC   @35wgetstor,@26ogetstor                                   000128
*                                                                        000129
*  switch (useMemoryType) {                                              000130
         LG    14,@33useMemoryType@15                                    000130
         STG   14,472(0,13)            #SW_WORK5                         000130
         CLG   14,=X'0000000000000002'                                   000130
         BRH   @5L29                                                     000130
         LG    14,472(0,13)            #SW_WORK5                         000130
         SLLG  14,14,2                                                   000130
         LGFR  15,14                                                     000130
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,24(15,14)                                              000130
         B     0(3,14)                                                   000130
@5L29    DS    0H                                                        000130
         BRU   @5L34                                                     000130
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000131
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\   000132
@5L30    DS    0H                                                        000132
         LA    2,@37origin@14                                            000132
         DROP  14                                                        000132
         LA    4,@36units                                                000132
         LA    5,@35wgetstor                                             000132
         LG    14,464(0,13)            #SR_PARM_5                        000132
         USING @@PARMD@5,14                                              000132
         LG    6,@30ttkn@11                                              000132
         LA    7,@31startAddress@12                                      000132
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000132
               AMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(4),ORIGIN=(2),TX 000132
               TOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)         000132
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000133
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000134
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000135
*   break;                                                               000136
         BRU   @5L16                                                     000136
*  case ZOS64_VMEM_2_TO_32G:                                             000137
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000138
@5L31    DS    0H                                                        000138
         LA    2,@37origin@14                                            000138
         LA    4,@36units                                                000138
         LA    5,@35wgetstor                                             000138
         LG    14,464(0,13)            #SR_PARM_5                        000138
         LG    6,@30ttkn@11                                              000138
         LA    7,@31startAddress@12                                      000138
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000138
               O32G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000138
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORX 000138
               IGIN=(7)                                                  000138
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000139
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000140
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000141
*   break;                                                               000142
         BRU   @5L16                                                     000142
*  case ZOS64_VMEM_2_TO_64G:                                             000143
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE  000144
@5L32    DS    0H                                                        000144
         LA    2,@37origin@14                                            000144
         LA    4,@36units                                                000144
         LA    5,@35wgetstor                                             000144
         LG    14,464(0,13)            #SR_PARM_5                        000144
         LG    6,@30ttkn@11                                              000144
         LA    7,@31startAddress@12                                      000144
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000144
               O64G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000144
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORX 000144
               IGIN=(7)                                                  000144
*     "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\             000145
*                 "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORI  000146
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetst  000147
*   break;                                                               000148
@5L16    DS    0H                                                        000130
@5L34    DS    0H                                                        000000
*  }                                                                     000149
*                                                                        000150
*  if (0 != iarv64_rc) {                                                 000151
         LGF   14,@34iarv64_rc@16                                        000151
         LTR   14,14                                                     000151
         BRE   @5L7                                                      000151
*   return (void *)0;                                                    000152
         LGHI  15,0                                                      000152
         BRU   @5L17                                                     000152
@5L7     DS    0H                                                        000152
*  }                                                                     000153
*  return (void *)origin;                                                000154
         LG    15,@37origin@14                                           000154
* }                                                                      000155
@5L17    DS    0H                                                        000155
         DROP                                                            000155
         MYEPILOG                                                        000155
OMRIARV64 CSECT ,                                                        000155
         DS    0FD                                                       000155
@@LIT@5  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@5  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111100000011'   Saved GPR Mask                    000000
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
@36units DS    FD                                                        000000
         ORG   @@AUTO@5+184                                              000000
@37origin@14 DS FD                                                       000000
         ORG   @@AUTO@5+192                                              000000
@33useMemoryType@15 DS FD                                                000000
         ORG   @@AUTO@5+200                                              000000
@34iarv64_rc@16 DS F                                                     000000
         ORG   @@AUTO@5+208                                              000000
@35wgetstor DS XL256                                                     000000
         ORG   @@AUTO@5+464                                              000000
#SR_PARM_5 DS  XL8                                                       000000
@@PARMD@5 DSECT                                                          000000
         DS    XL32                                                      000000
         ORG   @@PARMD@5+0                                               000000
@28num2GBUnits DS FD                                                     000000
         ORG   @@PARMD@5+8                                               000000
@29userExtendedPrivateAreaMemoryType@10 DS FD                            000000
         ORG   @@PARMD@5+16                                              000000
@30ttkn@11 DS  FD                                                        000000
         ORG   @@PARMD@5+24                                              000000
@31startAddress@12 DS FD                                                 000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000171
         ENTRY @@CCN@39                                                  000171
@@CCN@39 AMODE 64                                                        000171
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000171
         DC    A(@@FPB@4-*+8)          Signed offset to FPB              000171
         DC    XL4'00000000'           Reserved                          000171
@@CCN@39 DS    0FD                                                       000171
&CCN_PRCN SETC '@@CCN@39'                                                000171
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_in_userExtendedPrivateArea'    000171
&CCN_LITN SETC '@@LIT@4'                                                 000171
&CCN_BEGIN SETC '@@BGN@4'                                                000171
&CCN_ASCM SETC 'P'                                                       000171
&CCN_DSASZ SETA 480                                                      000171
&CCN_SASZ SETA 144                                                       000171
&CCN_ARGS SETA 4                                                         000171
&CCN_RLOW SETA 14                                                        000171
&CCN_RHIGH SETA 7                                                        000171
&CCN_NAB SETB  0                                                         000171
&CCN_MAIN SETB 0                                                         000171
&CCN_NAB_STORED SETB 0                                                   000171
&CCN_STATIC SETB 0                                                       000171
&CCN_ALTGPR(1) SETB 1                                                    000171
&CCN_ALTGPR(2) SETB 1                                                    000171
&CCN_ALTGPR(3) SETB 1                                                    000171
&CCN_ALTGPR(4) SETB 1                                                    000171
&CCN_ALTGPR(5) SETB 1                                                    000171
&CCN_ALTGPR(6) SETB 1                                                    000171
&CCN_ALTGPR(7) SETB 1                                                    000171
&CCN_ALTGPR(8) SETB 1                                                    000171
&CCN_ALTGPR(9) SETB 0                                                    000171
&CCN_ALTGPR(10) SETB 0                                                   000171
&CCN_ALTGPR(11) SETB 0                                                   000171
&CCN_ALTGPR(12) SETB 0                                                   000171
&CCN_ALTGPR(13) SETB 0                                                   000171
&CCN_ALTGPR(14) SETB 1                                                   000171
&CCN_ALTGPR(15) SETB 1                                                   000171
&CCN_ALTGPR(16) SETB 1                                                   000171
         MYPROLOG                                                        000171
@@BGN@4  DS    0H                                                        000171
         AIF   (NOT &CCN_SASIG).@@NOSIG4                                 000171
         LLILH 7,X'C6F4'                                                 000171
         OILL  7,X'E2C1'                                                 000171
         ST    7,4(,13)                                                  000171
.@@NOSIG4 ANOP                                                           000171
         USING @@AUTO@4,13                                               000171
         LARL  3,@@LIT@4                                                 000171
         USING @@LIT@4,3                                                 000171
         STG   1,464(0,13)             #SR_PARM_4                        000171
*  long segments;                                                        000172
*  long origin;                                                          000173
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000174
         LG    14,464(0,13)            #SR_PARM_4                        000174
         USING @@PARMD@4,14                                              000174
         LG    14,@41userExtendedPrivateAreaMemoryType@18                000174
         LGF   14,0(0,14)              (*)int                            000174
         STG   14,@45useMemoryType@24                                    000174
*  int  iarv64_rc = 0;                                                   000175
         LGHI  14,0                                                      000175
         ST    14,@46iarv64_rc@25                                        000175
*                                                                        000176
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000177
*                                                                        000178
*  segments = *numMBSegments;                                            000179
         LG    14,464(0,13)            #SR_PARM_4                        000179
         LG    14,@40numMBSegments@17                                    000179
         LGF   14,0(0,14)              (*)int                            000179
         STG   14,@48segments@22                                         000179
*  wgetstor = mgetstor;                                                  000180
         LARL  14,$STATIC                                                000180
         DROP  14                                                        000180
         USING @@STATICD@@,14                                            000180
         MVC   @47wgetstor,@38mgetstor                                   000180
*                                                                        000181
*  switch (useMemoryType) {                                              000182
         LG    14,@45useMemoryType@24                                    000182
         STG   14,472(0,13)            #SW_WORK4                         000182
         CLG   14,=X'0000000000000002'                                   000182
         BRH   @4L23                                                     000182
         LG    14,472(0,13)            #SW_WORK4                         000182
         SLLG  14,14,2                                                   000182
         LGFR  15,14                                                     000182
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,36(15,14)                                              000182
         B     0(3,14)                                                   000182
@4L23    DS    0H                                                        000182
         BRU   @4L28                                                     000182
*  case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                    000183
*   break;                                                               000184
@4L24    DS    0H                                                        000184
         BRU   @4L18                                                     000184
*  case ZOS64_VMEM_2_TO_32G:                                             000185
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\   000186
@4L25    DS    0H                                                        000186
         LA    2,@49origin@23                                            000186
         DROP  14                                                        000186
         LA    4,@48segments@22                                          000186
         LA    5,@47wgetstor                                             000186
         LG    14,464(0,13)            #SR_PARM_4                        000186
         USING @@PARMD@4,14                                              000186
         LG    6,@42ttkn@19                                              000186
         LA    7,@43startAddress@20                                      000186
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000186
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000186
               =(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)              000186
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000187
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000188
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000189
*   break;                                                               000190
         BRU   @4L18                                                     000190
*  case ZOS64_VMEM_2_TO_64G:                                             000191
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\   000192
@4L26    DS    0H                                                        000192
         LA    2,@49origin@23                                            000192
         LA    4,@48segments@22                                          000192
         LA    5,@47wgetstor                                             000192
         LG    14,464(0,13)            #SR_PARM_4                        000192
         LG    6,@42ttkn@19                                              000192
         LA    7,@43startAddress@20                                      000192
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000192
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000192
               =(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)              000192
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000193
*                 "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=  000194
*                 ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wge  000195
*   break;                                                               000196
@4L18    DS    0H                                                        000182
@4L28    DS    0H                                                        000000
*  }                                                                     000197
*                                                                        000198
*  if (0 != iarv64_rc) {                                                 000199
         LGF   14,@46iarv64_rc@25                                        000199
         LTR   14,14                                                     000199
         BRE   @4L5                                                      000199
*   return (void *)0;                                                    000200
         LGHI  15,0                                                      000200
         BRU   @4L19                                                     000200
@4L5     DS    0H                                                        000200
*  }                                                                     000201
*  return (void *)origin;                                                000202
         LG    15,@49origin@23                                           000202
* }                                                                      000203
@4L19    DS    0H                                                        000203
         DROP                                                            000203
         MYEPILOG                                                        000203
OMRIARV64 CSECT ,                                                        000203
         DS    0FD                                                       000203
@@LIT@4  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@4  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111100000011'   Saved GPR Mask                    000000
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
@48segments@22 DS FD                                                     000000
         ORG   @@AUTO@4+184                                              000000
@49origin@23 DS FD                                                       000000
         ORG   @@AUTO@4+192                                              000000
@45useMemoryType@24 DS FD                                                000000
         ORG   @@AUTO@4+200                                              000000
@46iarv64_rc@25 DS F                                                     000000
         ORG   @@AUTO@4+208                                              000000
@47wgetstor DS XL256                                                     000000
         ORG   @@AUTO@4+464                                              000000
#SR_PARM_4 DS  XL8                                                       000000
@@PARMD@4 DSECT                                                          000000
         DS    XL32                                                      000000
         ORG   @@PARMD@4+0                                               000000
@40numMBSegments@17 DS FD                                                000000
         ORG   @@PARMD@4+8                                               000000
@41userExtendedPrivateAreaMemoryType@18 DS FD                            000000
         ORG   @@PARMD@4+16                                              000000
@42ttkn@19 DS  FD                                                        000000
         ORG   @@PARMD@4+24                                              000000
@43startAddress@20 DS FD                                                 000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000218
         ENTRY @@CCN@51                                                  000218
@@CCN@51 AMODE 64                                                        000218
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000218
         DC    A(@@FPB@3-*+8)          Signed offset to FPB              000218
         DC    XL4'00000000'           Reserved                          000218
@@CCN@51 DS    0FD                                                       000218
&CCN_PRCN SETC '@@CCN@51'                                                000218
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_above_bar'                     000218
&CCN_LITN SETC '@@LIT@3'                                                 000218
&CCN_BEGIN SETC '@@BGN@3'                                                000218
&CCN_ASCM SETC 'P'                                                       000218
&CCN_DSASZ SETA 464                                                      000218
&CCN_SASZ SETA 144                                                       000218
&CCN_ARGS SETA 3                                                         000218
&CCN_RLOW SETA 14                                                        000218
&CCN_RHIGH SETA 8                                                        000218
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
&CCN_ALTGPR(8) SETB 1                                                    000218
&CCN_ALTGPR(9) SETB 1                                                    000218
&CCN_ALTGPR(10) SETB 0                                                   000218
&CCN_ALTGPR(11) SETB 0                                                   000218
&CCN_ALTGPR(12) SETB 0                                                   000218
&CCN_ALTGPR(13) SETB 0                                                   000218
&CCN_ALTGPR(14) SETB 1                                                   000218
&CCN_ALTGPR(15) SETB 1                                                   000218
&CCN_ALTGPR(16) SETB 1                                                   000218
         MYPROLOG                                                        000218
@@BGN@3  DS    0H                                                        000218
         AIF   (NOT &CCN_SASIG).@@NOSIG3                                 000218
         LLILH 8,X'C6F4'                                                 000218
         OILL  8,X'E2C1'                                                 000218
         ST    8,4(,13)                                                  000218
.@@NOSIG3 ANOP                                                           000218
         USING @@AUTO@3,13                                               000218
         LARL  3,@@LIT@3                                                 000218
         USING @@LIT@3,3                                                 000218
         STG   1,456(0,13)             #SR_PARM_3                        000218
*  long segments;                                                        000219
*  long origin;                                                          000220
*  int  iarv64_rc = 0;                                                   000221
         LGHI  2,0                                                       000221
         ST    2,@56iarv64_rc@32                                         000221
*                                                                        000222
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000223
*                                                                        000224
*  segments = *numMBSegments;                                            000225
         LG    14,456(0,13)            #SR_PARM_3                        000225
         USING @@PARMD@3,14                                              000225
         LG    14,@52numMBSegments@26                                    000225
         LGF   14,0(0,14)              (*)int                            000225
         STG   14,@58segments@30                                         000225
*  wgetstor = rgetstor;                                                  000226
         LARL  14,$STATIC                                                000226
         DROP  14                                                        000226
         USING @@STATICD@@,14                                            000226
         MVC   @57wgetstor,@50rgetstor                                   000226
*                                                                        000227
*  __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                   000228
         LA    4,@59origin@31                                            000228
         DROP  14                                                        000228
         LA    5,@58segments@30                                          000228
         LA    6,@57wgetstor                                             000228
         LG    14,456(0,13)            #SR_PARM_3                        000228
         USING @@PARMD@3,14                                              000228
         LG    7,@53ttkn@27                                              000228
         LA    8,@54startAddress@28                                      000228
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000228
               AMESIZE=4K,SEGMENTS=(5),ORIGIN=(4),TTOKEN=(7),RETCODE=19X 000228
               2(13),MF=(E,(6)),INORIGIN=(8)                             000228
         LGR   15,2                                                      000228
*    "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                 000229
*             "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(  000230
*             ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetsto  000231
*                                                                        000232
*  if (0 != iarv64_rc) {                                                 000233
         LGF   14,@56iarv64_rc@32                                        000233
         LTR   14,14                                                     000233
         BRE   @3L3                                                      000233
*   return (void *)0;                                                    000234
         BRU   @3L20                                                     000234
@3L3     DS    0H                                                        000234
*  }                                                                     000235
*  return (void *)origin;                                                000236
         LG    15,@59origin@31                                           000236
* }                                                                      000237
@3L20    DS    0H                                                        000237
         DROP                                                            000237
         MYEPILOG                                                        000237
OMRIARV64 CSECT ,                                                        000237
         DS    0FD                                                       000237
@@LIT@3  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@3  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111110000011'   Saved GPR Mask                    000000
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
@58segments@30 DS FD                                                     000000
         ORG   @@AUTO@3+184                                              000000
@59origin@31 DS FD                                                       000000
         ORG   @@AUTO@3+192                                              000000
@56iarv64_rc@32 DS F                                                     000000
         ORG   @@AUTO@3+200                                              000000
@57wgetstor DS XL256                                                     000000
         ORG   @@AUTO@3+456                                              000000
#SR_PARM_3 DS  XL8                                                       000000
@@PARMD@3 DSECT                                                          000000
         DS    XL24                                                      000000
         ORG   @@PARMD@3+0                                               000000
@52numMBSegments@26 DS FD                                                000000
         ORG   @@PARMD@3+8                                               000000
@53ttkn@27 DS  FD                                                        000000
         ORG   @@PARMD@3+16                                              000000
@54startAddress@28 DS FD                                                 000000
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000251
         ENTRY @@CCN@61                                                  000251
@@CCN@61 AMODE 64                                                        000251
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000251
         DC    A(@@FPB@2-*+8)          Signed offset to FPB              000251
         DC    XL4'00000000'           Reserved                          000251
@@CCN@61 DS    0FD                                                       000251
&CCN_PRCN SETC '@@CCN@61'                                                000251
&CCN_PRCN_LONG SETC 'omrfree_memory_above_bar'                           000251
&CCN_LITN SETC '@@LIT@2'                                                 000251
&CCN_BEGIN SETC '@@BGN@2'                                                000251
&CCN_ASCM SETC 'P'                                                       000251
&CCN_DSASZ SETA 456                                                      000251
&CCN_SASZ SETA 144                                                       000251
&CCN_ARGS SETA 2                                                         000251
&CCN_RLOW SETA 14                                                        000251
&CCN_RHIGH SETA 5                                                        000251
&CCN_NAB SETB  0                                                         000251
&CCN_MAIN SETB 0                                                         000251
&CCN_NAB_STORED SETB 0                                                   000251
&CCN_STATIC SETB 0                                                       000251
&CCN_ALTGPR(1) SETB 1                                                    000251
&CCN_ALTGPR(2) SETB 1                                                    000251
&CCN_ALTGPR(3) SETB 1                                                    000251
&CCN_ALTGPR(4) SETB 1                                                    000251
&CCN_ALTGPR(5) SETB 1                                                    000251
&CCN_ALTGPR(6) SETB 1                                                    000251
&CCN_ALTGPR(7) SETB 0                                                    000251
&CCN_ALTGPR(8) SETB 0                                                    000251
&CCN_ALTGPR(9) SETB 0                                                    000251
&CCN_ALTGPR(10) SETB 0                                                   000251
&CCN_ALTGPR(11) SETB 0                                                   000251
&CCN_ALTGPR(12) SETB 0                                                   000251
&CCN_ALTGPR(13) SETB 0                                                   000251
&CCN_ALTGPR(14) SETB 1                                                   000251
&CCN_ALTGPR(15) SETB 1                                                   000251
&CCN_ALTGPR(16) SETB 1                                                   000251
         MYPROLOG                                                        000251
@@BGN@2  DS    0H                                                        000251
         AIF   (NOT &CCN_SASIG).@@NOSIG2                                 000251
         LLILH 5,X'C6F4'                                                 000251
         OILL  5,X'E2C1'                                                 000251
         ST    5,4(,13)                                                  000251
.@@NOSIG2 ANOP                                                           000251
         USING @@AUTO@2,13                                               000251
         LARL  3,@@LIT@2                                                 000251
         USING @@LIT@2,3                                                 000251
         STG   1,448(0,13)             #SR_PARM_2                        000251
*  void * xmemobjstart;                                                  000252
*  int  iarv64_rc = 0;                                                   000253
         LGHI  14,0                                                      000253
         ST    14,@65iarv64_rc@35                                        000253
*                                                                        000254
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000255
*                                                                        000256
*  xmemobjstart = address;                                               000257
         LG    14,448(0,13)            #SR_PARM_2                        000257
         USING @@PARMD@2,14                                              000257
         LG    14,@62address                                             000257
         STG   14,@67xmemobjstart                                        000257
*  wgetstor = pgetstor;                                                  000258
         LARL  14,$STATIC                                                000258
         DROP  14                                                        000258
         USING @@STATICD@@,14                                            000258
         MVC   @66wgetstor,@60pgetstor                                   000258
*                                                                        000259
*     __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%  000260
         LA    2,@66wgetstor                                             000260
         DROP  14                                                        000260
         LA    4,@67xmemobjstart                                         000260
         LG    14,448(0,13)            #SR_PARM_2                        000260
         USING @@PARMD@2,14                                              000260
         LG    5,@63ttkn@33                                              000260
         IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(4),TTOKEN=(5),RETCX 000260
               ODE=184(13),MF=(E,(2))                                    000260
*             ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(tt  000261
*  return iarv64_rc;                                                     000262
         LGF   15,@65iarv64_rc@35                                        000262
* }                                                                      000263
@2L21    DS    0H                                                        000263
         DROP                                                            000263
         MYEPILOG                                                        000263
OMRIARV64 CSECT ,                                                        000263
         DS    0FD                                                       000263
@@LIT@2  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@2  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111110000000011'   Saved GPR Mask                    000000
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
@67xmemobjstart DS AD                                                    000000
         ORG   @@AUTO@2+184                                              000000
@65iarv64_rc@35 DS F                                                     000000
         ORG   @@AUTO@2+192                                              000000
@66wgetstor DS XL256                                                     000000
         ORG   @@AUTO@2+448                                              000000
#SR_PARM_2 DS  XL8                                                       000000
@@PARMD@2 DSECT                                                          000000
         DS    XL16                                                      000000
         ORG   @@PARMD@2+0                                               000000
@62address DS  FD                                                        000000
         ORG   @@PARMD@2+8                                               000000
@63ttkn@33 DS  FD                                                        000000
*                                                                        000302
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
@14ngetstor DS XL256                                                     000000
         ORG   @@STATICD@@+512                                           000000
@26ogetstor DS XL256                                                     000000
         ORG   @@STATICD@@+768                                           000000
@38mgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1024                                          000000
@50rgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1280                                          000000
@60pgetstor DS XL256                                                     000000
         ORG   @@STATICD@@+1536                                          000000
@68qgetstor DS XL256                                                     000000
         END   ,(5650ZOS   ,2101,19148)                                  000000
