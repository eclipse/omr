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
*                                                                        000001
* #include "omriarv64.h"                                                 000002
*                                                                        000003
* #pragma prolog(omrallocate_1M_fixed_pages,"MYPROLOG")                  000004
* #pragma epilog(omrallocate_1M_fixed_pages,"MYEPILOG")                  000005
*                                                                        000006
* __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(lgetstor));          000007
*                                                                        000008
* /*                                                                     000009
*  * Allocate 1MB fixed pages using IARV64 system macro.                 000010
*  * Memory allocated is freed using omrfree_memory_above_bar().         000011
*  *                                                                     000012
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000013
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000014
*  *                                                                     000015
*  * @return pointer to memory allocated, NULL on failure.               000016
*  */                                                                    000017
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000018
*  long segments;                                                        000019
*  long origin;                                                          000020
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000021
*  int  iarv64_rc = 0;                                                   000022
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000023
*                                                                        000024
*  segments = *numMBSegments;                                            000025
*  wgetstor = lgetstor;                                                  000026
*                                                                        000027
*  if (startAddress == (void*)0) {                                       000028
*   switch (useMemoryType) {                                             000029
*         case ZOS64_VMEM_ABOVE_BAR_GENERAL:                             000030
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PA  000031
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000032
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000033
*    break;                                                              000034
*   case ZOS64_VMEM_2_TO_32G:                                            000035
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000036
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000037
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000038
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000039
*    break;                                                              000040
*   case ZOS64_VMEM_2_TO_64G:                                            000041
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000042
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000043
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000044
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000045
*    break;                                                              000046
*   }                                                                    000047
*  } else {                                                              000048
*   switch (useMemoryType) {                                             000049
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000050
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PA  000051
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000052
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000053
*    break;                                                              000054
*   case ZOS64_VMEM_2_TO_32G:                                            000055
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000056
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000057
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000058
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000059
*    break;                                                              000060
*   case ZOS64_VMEM_2_TO_64G:                                            000061
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000062
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000063
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000064
*                     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(  000065
*    break;                                                              000066
*   }                                                                    000067
*  }                                                                     000068
*                                                                        000069
*  if (0 != iarv64_rc) {                                                 000070
*   return (void *)0;                                                    000071
*  }                                                                     000072
*  return (void *)origin;                                                000073
* }                                                                      000074
*                                                                        000075
* #pragma prolog(omrallocate_1M_pageable_pages_above_bar,"MYPROLOG")     000076
* #pragma epilog(omrallocate_1M_pageable_pages_above_bar,"MYEPILOG")     000077
*                                                                        000078
* __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(ngetstor));          000079
*                                                                        000080
* /*                                                                     000081
*  * Allocate 1MB pageable pages above 2GB bar using IARV64 system macr  000082
*  * Memory allocated is freed using omrfree_memory_above_bar().         000083
*  *                                                                     000084
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000085
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000086
*  *                                                                     000087
*  * @return pointer to memory allocated, NULL on failure.               000088
*  */                                                                    000089
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000090
*  long segments;                                                        000091
*  long origin;                                                          000092
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000093
*  int  iarv64_rc = 0;                                                   000094
*                                                                        000095
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000096
*                                                                        000097
*  segments = *numMBSegments;                                            000098
*  wgetstor = ngetstor;                                                  000099
*                                                                        000100
*  if (startAddress == (void*)0) {                                       000101
*   switch (useMemoryType) {                                             000102
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000103
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000104
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000105
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000106
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000107
*    break;                                                              000108
*   case ZOS64_VMEM_2_TO_32G:                                            000109
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000110
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000111
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000112
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000113
*    break;                                                              000114
*   case ZOS64_VMEM_2_TO_64G:                                            000115
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000116
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000117
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000118
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000119
*    break;                                                              000120
*   }                                                                    000121
*  } else {                                                              000122
*   switch (useMemoryType) {                                             000123
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000124
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000125
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000126
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000127
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000128
*    break;                                                              000129
*   case ZOS64_VMEM_2_TO_32G:                                            000130
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000131
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000132
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000133
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000134
*    break;                                                              000135
*   case ZOS64_VMEM_2_TO_64G:                                            000136
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000137
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000138
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000139
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000140
*    break;                                                              000141
*   }                                                                    000142
*  }                                                                     000143
*                                                                        000144
*  if (0 != iarv64_rc) {                                                 000145
*   return (void *)0;                                                    000146
*  }                                                                     000147
*  return (void *)origin;                                                000148
* }                                                                      000149
*                                                                        000150
* #pragma prolog(omrallocate_2G_pages,"MYPROLOG")                        000151
* #pragma epilog(omrallocate_2G_pages,"MYEPILOG")                        000152
*                                                                        000153
* __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(ogetstor));          000154
*                                                                        000155
* /*                                                                     000156
*  * Allocate 2GB fixed pages using IARV64 system macro.                 000157
*  * Memory allocated is freed using omrfree_memory_above_bar().         000158
*  *                                                                     000159
*  * @params[in] num2GBUnits Number of 2GB units to be allocated         000160
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000161
*  *                                                                     000162
*  * @return pointer to memory allocated, NULL on failure.               000163
*  */                                                                    000164
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000165
*  long units;                                                           000166
*  long origin;                                                          000167
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000168
*  int  iarv64_rc = 0;                                                   000169
*                                                                        000170
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000171
*                                                                        000172
*  units = *num2GBUnits;                                                 000173
*  wgetstor = ogetstor;                                                  000174
*                                                                        000175
*  if (startAddress == (void*)0) {                                       000176
*   switch (useMemoryType) {                                             000177
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000178
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000179
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000180
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000181
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000182
*    break;                                                              000183
*   case ZOS64_VMEM_2_TO_32G:                                            000184
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000185
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000186
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000187
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000188
*    break;                                                              000189
*   case ZOS64_VMEM_2_TO_64G:                                            000190
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000191
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000192
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000193
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000194
*    break;                                                              000195
*   }                                                                    000196
*  } else {                                                              000197
*   switch (useMemoryType) {                                             000198
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000199
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000200
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000201
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000202
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000203
*    break;                                                              000204
*   case ZOS64_VMEM_2_TO_32G:                                            000205
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000206
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000207
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000208
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000209
*    break;                                                              000210
*   case ZOS64_VMEM_2_TO_64G:                                            000211
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000212
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000213
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000214
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000215
*    break;                                                              000216
*   }                                                                    000217
*  }                                                                     000218
*                                                                        000219
*  if (0 != iarv64_rc) {                                                 000220
*   return (void *)0;                                                    000221
*  }                                                                     000222
*  return (void *)origin;                                                000223
* }                                                                      000224
*                                                                        000225
* #pragma prolog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYPRO  000226
* #pragma epilog(omrallocate_4K_pages_in_userExtendedPrivateArea,"MYEPI  000227
*                                                                        000228
* __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(mgetstor));          000229
*                                                                        000230
* /*                                                                     000231
*  * Allocate 4KB pages in 2G-32G range using IARV64 system macro.       000232
*  * Memory allocated is freed using omrfree_memory_above_bar().         000233
*  *                                                                     000234
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000235
*  * @params[in] userExtendedPrivateAreaMemoryType capability of OS: 0   000236
*  *                                                                     000237
*  * @return pointer to memory allocated, NULL on failure.               000238
*  */                                                                    000239
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000240
*  long segments;                                                        000241
*  long origin;                                                          000242
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000243
*  int  iarv64_rc = 0;                                                   000244
*                                                                        000245
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000246
*                                                                        000247
*  segments = *numMBSegments;                                            000248
*  wgetstor = mgetstor;                                                  000249
*                                                                        000250
*  if (startAddress == (void*)0) {                                       000251
*   switch (useMemoryType) {                                             000252
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000253
*    break;                                                              000254
*   case ZOS64_VMEM_2_TO_32G:                                            000255
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000256
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000257
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000258
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000259
*    break;                                                              000260
*   case ZOS64_VMEM_2_TO_64G:                                            000261
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000262
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000263
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000264
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000265
*    break;                                                              000266
*   }                                                                    000267
*  } else {                                                              000268
*   switch (useMemoryType) {                                             000269
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000270
*    break;                                                              000271
*   case ZOS64_VMEM_2_TO_32G:                                            000272
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000273
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000274
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000275
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000276
*    break;                                                              000277
*   case ZOS64_VMEM_2_TO_64G:                                            000278
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000279
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000280
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000281
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000282
*    break;                                                              000283
*   }                                                                    000284
*  }                                                                     000285
*                                                                        000286
*  if (0 != iarv64_rc) {                                                 000287
*   return (void *)0;                                                    000288
*  }                                                                     000289
*  return (void *)origin;                                                000290
* }                                                                      000291
*                                                                        000292
* #pragma prolog(omrallocate_4K_pages_above_bar,"MYPROLOG")              000293
* #pragma epilog(omrallocate_4K_pages_above_bar,"MYEPILOG")              000294
*                                                                        000295
* __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(rgetstor));          000296
*                                                                        000297
* /*                                                                     000298
*  * Allocate 4KB pages using IARV64 system macro.                       000299
*  * Memory allocated is freed using omrfree_memory_above_bar().         000300
*  *                                                                     000301
*  * @params[in] numMBSegments Number of 1MB segments to be allocated    000302
*  *                                                                     000303
*  * @return pointer to memory allocated, NULL on failure.               000304
*  */                                                                    000305
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000306
*  long segments;                                                        000307
*  long origin;                                                          000308
*  int  iarv64_rc = 0;                                                   000309
*                                                                        000310
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000311
*                                                                        000312
*  segments = *numMBSegments;                                            000313
*  wgetstor = rgetstor;                                                  000314
*                                                                        000315
*  if (startAddress == (void*)0) {                                       000316
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                  000317
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000318
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000319
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000320
*  } else {                                                              000321
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                  000322
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000323
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INO  000324
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000325
*  }                                                                     000326
*                                                                        000327
*  if (0 != iarv64_rc) {                                                 000328
*   return (void *)0;                                                    000329
*  }                                                                     000330
*  return (void *)origin;                                                000331
* }                                                                      000332
*                                                                        000333
* #pragma prolog(omrfree_memory_above_bar,"MYPROLOG")                    000334
* #pragma epilog(omrfree_memory_above_bar,"MYEPILOG")                    000335
*                                                                        000336
* __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(pgetstor));          000337
*                                                                        000338
* /*                                                                     000339
*  * Free memory allocated using IARV64 system macro.                    000340
*  *                                                                     000341
*  * @params[in] address pointer to memory region to be freed            000342
*  *                                                                     000343
*  * @return non-zero if memory is not freed successfully, 0 otherwise.  000344
*  */                                                                    000345
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000346
*  void * xmemobjstart;                                                  000347
*  int  iarv64_rc = 0;                                                   000348
*                                                                        000349
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000350
*                                                                        000351
*  xmemobjstart = address;                                               000352
*  wgetstor = pgetstor;                                                  000353
*                                                                        000354
*     __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%  000355
*             ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(tt  000356
*  return iarv64_rc;                                                     000357
* }                                                                      000358
*                                                                        000359
* #pragma prolog(omrdiscard_data,"MYPROLOG")                             000360
* #pragma epilog(omrdiscard_data,"MYEPILOG")                             000361
*                                                                        000362
* __asm(" IARV64 PLISTVER=MAX,MF=(L,QGETSTOR)":"DS"(qgetstor));          000363
*                                                                        000364
* /* Used to pass parameters to IARV64 DISCARDDATA in omrdiscard_data()  000365
* struct rangeList {                                                     000366
*  void *startAddr;                                                      000367
*  long pageCount;                                                       000368
* };                                                                     000369
*                                                                        000370
* /*                                                                     000371
*  * Discard memory allocated using IARV64 system macro.                 000372
*  *                                                                     000373
*  * @params[in] address pointer to memory region to be discarded        000374
*  * @params[in] numFrames number of frames to be discarded. Frame size  000375
*  *                                                                     000376
*  * @return non-zero if memory is not discarded successfully, 0 otherw  000377
*  */                                                                    000378
* int omrdiscard_data(void *address, int *numFrames) {                   000379
         J     @@CCN@69                                                  000379
@@PFD@@  DC    XL8'00C300C300D50000'   Prefix Data Marker                000379
         DC    CL8'20190606'           Compiled Date YYYYMMDD            000379
         DC    CL6'133637'             Compiled Time HHMMSS              000379
         DC    XL4'42010001'           Compiler Version                  000379
         DC    XL2'0000'               Reserved                          000379
         DC    BL1'00000000'           Flag Set 1                        000379
         DC    BL1'00000000'           Flag Set 2                        000379
         DC    BL1'00000000'           Flag Set 3                        000379
         DC    BL1'00000000'           Flag Set 4                        000379
         DC    XL4'00000000'           Reserved                          000379
         ENTRY @@CCN@69                                                  000379
@@CCN@69 AMODE 64                                                        000379
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000379
         DC    A(@@FPB@1-*+8)          Signed offset to FPB              000379
         DC    XL4'00000000'           Reserved                          000379
@@CCN@69 DS    0FD                                                       000379
&CCN_PRCN SETC '@@CCN@69'                                                000379
&CCN_PRCN_LONG SETC 'omrdiscard_data'                                    000379
&CCN_LITN SETC '@@LIT@1'                                                 000379
&CCN_BEGIN SETC '@@BGN@1'                                                000379
&CCN_ASCM SETC 'P'                                                       000379
&CCN_DSASZ SETA 472                                                      000379
&CCN_SASZ SETA 144                                                       000379
&CCN_ARGS SETA 2                                                         000379
&CCN_RLOW SETA 14                                                        000379
&CCN_RHIGH SETA 4                                                        000379
&CCN_NAB SETB  0                                                         000379
&CCN_MAIN SETB 0                                                         000379
&CCN_NAB_STORED SETB 0                                                   000379
&CCN_STATIC SETB 0                                                       000379
&CCN_ALTGPR(1) SETB 1                                                    000379
&CCN_ALTGPR(2) SETB 1                                                    000379
&CCN_ALTGPR(3) SETB 1                                                    000379
&CCN_ALTGPR(4) SETB 1                                                    000379
&CCN_ALTGPR(5) SETB 1                                                    000379
&CCN_ALTGPR(6) SETB 0                                                    000379
&CCN_ALTGPR(7) SETB 0                                                    000379
&CCN_ALTGPR(8) SETB 0                                                    000379
&CCN_ALTGPR(9) SETB 0                                                    000379
&CCN_ALTGPR(10) SETB 0                                                   000379
&CCN_ALTGPR(11) SETB 0                                                   000379
&CCN_ALTGPR(12) SETB 0                                                   000379
&CCN_ALTGPR(13) SETB 0                                                   000379
&CCN_ALTGPR(14) SETB 1                                                   000379
&CCN_ALTGPR(15) SETB 1                                                   000379
&CCN_ALTGPR(16) SETB 1                                                   000379
         MYPROLOG                                                        000379
@@BGN@1  DS    0H                                                        000379
         AIF   (NOT &CCN_SASIG).@@NOSIG1                                 000379
         LLILH 4,X'C6F4'                                                 000379
         OILL  4,X'E2C1'                                                 000379
         ST    4,4(,13)                                                  000379
.@@NOSIG1 ANOP                                                           000379
         USING @@AUTO@1,13                                               000379
         LARL  3,@@LIT@1                                                 000379
         USING @@LIT@1,3                                                 000379
         STG   1,464(0,13)             #SR_PARM_1                        000379
*  struct rangeList range;                                               000380
*  void *rangePtr;                                                       000381
*  int iarv64_rc = 0;                                                    000382
         LGHI  14,0                                                      000382
         ST    14,@73iarv64_rc@38                                        000382
*                                                                        000383
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000384
*                                                                        000385
*  range.startAddr = address;                                            000386
         LG    14,464(0,13)            #SR_PARM_1                        000386
         USING @@PARMD@1,14                                              000386
         LG    14,@70address@36                                          000386
         STG   14,176(0,13)            range_rangeList_startAddr         000386
*  range.pageCount = *numFrames;                                         000387
         LG    14,464(0,13)            #SR_PARM_1                        000387
         LG    14,@71numFrames                                           000387
         LGF   14,0(0,14)              (*)int                            000387
         STG   14,184(0,13)            range_rangeList_pageCount         000387
*  rangePtr = (void *)&range;                                            000388
         LA    14,@75range                                               000388
         STG   14,@78rangePtr                                            000388
*  wgetstor = qgetstor;                                                  000389
         LARL  14,$STATIC                                                000389
         DROP  14                                                        000389
         USING @@STATICD@@,14                                            000389
         MVC   @74wgetstor,@68qgetstor                                   000389
*                                                                        000390
*  __asm(" IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,"\                     000391
         LA    2,@78rangePtr                                             000391
         DROP  14                                                        000391
         LA    4,@74wgetstor                                             000391
         IARV64 REQUEST=DISCARDDATA,KEEPREAL=NO,RANGLIST=(2),RETCODE=20X 000391
               0(13),MF=(E,(4))                                          000391
*    "RANGLIST=(%1),RETCODE=%0,MF=(E,(%2))"\                             000392
*    ::"m"(iarv64_rc),"r"(&rangePtr),"r"(&wgetstor));                    000393
*                                                                        000394
*  return iarv64_rc;                                                     000395
         LGF   15,@73iarv64_rc@38                                        000395
* }                                                                      000396
@1L36    DS    0H                                                        000396
         DROP                                                            000396
         MYEPILOG                                                        000396
OMRIARV64 CSECT ,                                                        000396
         DS    0FD                                                       000396
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
* void * omrallocate_1M_fixed_pages(int *numMBSegments, int *userExtend  000018
         ENTRY @@CCN@2                                                   000018
@@CCN@2  AMODE 64                                                        000018
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000018
         DC    A(@@FPB@7-*+8)          Signed offset to FPB              000018
         DC    XL4'00000000'           Reserved                          000018
@@CCN@2  DS    0FD                                                       000018
&CCN_PRCN SETC '@@CCN@2'                                                 000018
&CCN_PRCN_LONG SETC 'omrallocate_1M_fixed_pages'                         000018
&CCN_LITN SETC '@@LIT@7'                                                 000018
&CCN_BEGIN SETC '@@BGN@7'                                                000018
&CCN_ASCM SETC 'P'                                                       000018
&CCN_DSASZ SETA 480                                                      000018
&CCN_SASZ SETA 144                                                       000018
&CCN_ARGS SETA 4                                                         000018
&CCN_RLOW SETA 14                                                        000018
&CCN_RHIGH SETA 7                                                        000018
&CCN_NAB SETB  0                                                         000018
&CCN_MAIN SETB 0                                                         000018
&CCN_NAB_STORED SETB 0                                                   000018
&CCN_STATIC SETB 0                                                       000018
&CCN_ALTGPR(1) SETB 1                                                    000018
&CCN_ALTGPR(2) SETB 1                                                    000018
&CCN_ALTGPR(3) SETB 1                                                    000018
&CCN_ALTGPR(4) SETB 1                                                    000018
&CCN_ALTGPR(5) SETB 1                                                    000018
&CCN_ALTGPR(6) SETB 1                                                    000018
&CCN_ALTGPR(7) SETB 1                                                    000018
&CCN_ALTGPR(8) SETB 1                                                    000018
&CCN_ALTGPR(9) SETB 0                                                    000018
&CCN_ALTGPR(10) SETB 0                                                   000018
&CCN_ALTGPR(11) SETB 0                                                   000018
&CCN_ALTGPR(12) SETB 0                                                   000018
&CCN_ALTGPR(13) SETB 0                                                   000018
&CCN_ALTGPR(14) SETB 1                                                   000018
&CCN_ALTGPR(15) SETB 1                                                   000018
&CCN_ALTGPR(16) SETB 1                                                   000018
         MYPROLOG                                                        000018
@@BGN@7  DS    0H                                                        000018
         AIF   (NOT &CCN_SASIG).@@NOSIG7                                 000018
         LLILH 7,X'C6F4'                                                 000018
         OILL  7,X'E2C1'                                                 000018
         ST    7,4(,13)                                                  000018
.@@NOSIG7 ANOP                                                           000018
         USING @@AUTO@7,13                                               000018
         LARL  3,@@LIT@7                                                 000018
         USING @@LIT@7,3                                                 000018
         STG   1,464(0,13)             #SR_PARM_7                        000018
*  long segments;                                                        000019
*  long origin;                                                          000020
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000021
         LG    14,464(0,13)            #SR_PARM_7                        000021
         USING @@PARMD@7,14                                              000021
         LG    14,@4userExtendedPrivateAreaMemoryType                    000021
         LGF   14,0(0,14)              (*)int                            000021
         STG   14,@8useMemoryType                                        000021
*  int  iarv64_rc = 0;                                                   000022
         LGHI  14,0                                                      000022
         ST    14,@10iarv64_rc                                           000022
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,LGETSTOR)":"DS"(wgetstor));         000023
*                                                                        000024
*  segments = *numMBSegments;                                            000025
         LG    14,464(0,13)            #SR_PARM_7                        000025
         LG    14,@3numMBSegments                                        000025
         LGF   14,0(0,14)              (*)int                            000025
         STG   14,@12segments                                            000025
*  wgetstor = lgetstor;                                                  000026
         LARL  14,$STATIC                                                000026
         DROP  14                                                        000026
         USING @@STATICD@@,14                                            000026
         MVC   @11wgetstor,@1lgetstor                                    000026
*                                                                        000027
*  if (startAddress == (void*)0) {                                       000028
         LG    14,464(0,13)            #SR_PARM_7                        000028
         DROP  14                                                        000028
         USING @@PARMD@7,14                                              000028
         LG    14,@6startAddress                                         000028
         CLG   14,=X'0000000000000000'                                   000028
         BRNE  @7L19                                                     000028
*   switch (useMemoryType) {                                             000029
         LG    14,@8useMemoryType                                        000029
         STG   14,472(0,13)            #SW_WORK7                         000029
         CLG   14,=X'0000000000000002'                                   000029
         BRH   @7L73                                                     000029
         LG    14,472(0,13)            #SW_WORK7                         000029
         SLLG  14,14,2                                                   000029
         LGFR  15,14                                                     000029
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,0(15,14)                                               000029
         B     0(3,14)                                                   000029
@7L73    DS    0H                                                        000029
         BRU   @7L78                                                     000029
*         case ZOS64_VMEM_ABOVE_BAR_GENERAL:                             000030
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PA  000031
@7L74    DS    0H                                                        000031
         LA    2,@13origin                                               000031
         LA    4,@12segments                                             000031
         LA    5,@11wgetstor                                             000031
         LG    14,464(0,13)            #SR_PARM_7                        000031
         LG    6,@5ttkn                                                  000031
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000031
               AMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=X 000031
               200(13),MF=(E,(5))                                        000031
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000032
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000033
*    break;                                                              000034
         BRU   @7L22                                                     000034
*   case ZOS64_VMEM_2_TO_32G:                                            000035
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000036
@7L75    DS    0H                                                        000036
         LA    2,@13origin                                               000036
         LA    4,@12segments                                             000036
         LA    5,@11wgetstor                                             000036
         LG    14,464(0,13)            #SR_PARM_7                        000036
         LG    6,@5ttkn                                                  000036
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000036
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000036
               EN=(6),RETCODE=200(13),MF=(E,(5))                         000036
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000037
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000038
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000039
*    break;                                                              000040
         BRU   @7L22                                                     000040
*   case ZOS64_VMEM_2_TO_64G:                                            000041
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000042
@7L76    DS    0H                                                        000042
         LA    2,@13origin                                               000042
         LA    4,@12segments                                             000042
         LA    5,@11wgetstor                                             000042
         LG    14,464(0,13)            #SR_PARM_7                        000042
         LG    6,@5ttkn                                                  000042
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000042
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000042
               EN=(6),RETCODE=200(13),MF=(E,(5))                         000042
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000043
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000044
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000045
*    break;                                                              000046
@7L22    DS    0H                                                        000029
@7L78    DS    0H                                                        000000
         BRU   @7L20                                                     000046
@7L19    DS    0H                                                        000046
*   }                                                                    000047
*  } else {                                                              000048
*   switch (useMemoryType) {                                             000049
         LG    14,@8useMemoryType                                        000049
         STG   14,472(0,13)            #SW_WORK7                         000049
         CLG   14,=X'0000000000000002'                                   000049
         BRH   @7L79                                                     000049
         LG    14,472(0,13)            #SW_WORK7                         000049
         SLLG  14,14,2                                                   000049
         LGFR  15,14                                                     000049
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,12(15,14)                                              000049
         B     0(3,14)                                                   000049
@7L79    DS    0H                                                        000049
         BRU   @7L84                                                     000049
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000050
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PA  000051
@7L80    DS    0H                                                        000051
         LA    2,@13origin                                               000051
         LA    4,@12segments                                             000051
         LA    5,@11wgetstor                                             000051
         LG    14,464(0,13)            #SR_PARM_7                        000051
         LG    6,@5ttkn                                                  000051
         LA    7,@6startAddress                                          000051
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000051
               AMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=X 000051
               200(13),MF=(E,(5)),INORIGIN=(7)                           000051
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000052
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000053
*    break;                                                              000054
         BRU   @7L23                                                     000054
*   case ZOS64_VMEM_2_TO_32G:                                            000055
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000056
@7L81    DS    0H                                                        000056
         LA    2,@13origin                                               000056
         LA    4,@12segments                                             000056
         LA    5,@11wgetstor                                             000056
         LG    14,464(0,13)            #SR_PARM_7                        000056
         LG    6,@5ttkn                                                  000056
         LA    7,@6startAddress                                          000056
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000056
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000056
               EN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)            000056
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000057
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000058
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000059
*    break;                                                              000060
         BRU   @7L23                                                     000060
*   case ZOS64_VMEM_2_TO_64G:                                            000061
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000062
@7L82    DS    0H                                                        000062
         LA    2,@13origin                                               000062
         LA    4,@12segments                                             000062
         LA    5,@11wgetstor                                             000062
         LG    14,464(0,13)            #SR_PARM_7                        000062
         LG    6,@5ttkn                                                  000062
         LA    7,@6startAddress                                          000062
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000062
               L=UNAUTH,PAGEFRAMESIZE=1MEG,SEGMENTS=(4),ORIGIN=(2),TTOKX 000062
               EN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)            000062
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=1MEG,"\                             000063
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000064
*                     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(  000065
*    break;                                                              000066
@7L23    DS    0H                                                        000049
@7L84    DS    0H                                                        000000
@7L20    DS    0H                                                        000066
*   }                                                                    000067
*  }                                                                     000068
*                                                                        000069
*  if (0 != iarv64_rc) {                                                 000070
         LGF   14,@10iarv64_rc                                           000070
         LTR   14,14                                                     000070
         BRE   @7L21                                                     000070
*   return (void *)0;                                                    000071
         LGHI  15,0                                                      000071
         BRU   @7L24                                                     000071
@7L21    DS    0H                                                        000071
*  }                                                                     000072
*  return (void *)origin;                                                000073
         LG    15,@13origin                                              000073
* }                                                                      000074
@7L24    DS    0H                                                        000074
         DROP                                                            000074
         MYEPILOG                                                        000074
OMRIARV64 CSECT ,                                                        000074
         DS    0FD                                                       000074
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
* void * omrallocate_1M_pageable_pages_above_bar(int *numMBSegments, in  000090
         ENTRY @@CCN@15                                                  000090
@@CCN@15 AMODE 64                                                        000090
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000090
         DC    A(@@FPB@6-*+8)          Signed offset to FPB              000090
         DC    XL4'00000000'           Reserved                          000090
@@CCN@15 DS    0FD                                                       000090
&CCN_PRCN SETC '@@CCN@15'                                                000090
&CCN_PRCN_LONG SETC 'omrallocate_1M_pageable_pages_above_bar'            000090
&CCN_LITN SETC '@@LIT@6'                                                 000090
&CCN_BEGIN SETC '@@BGN@6'                                                000090
&CCN_ASCM SETC 'P'                                                       000090
&CCN_DSASZ SETA 480                                                      000090
&CCN_SASZ SETA 144                                                       000090
&CCN_ARGS SETA 4                                                         000090
&CCN_RLOW SETA 14                                                        000090
&CCN_RHIGH SETA 7                                                        000090
&CCN_NAB SETB  0                                                         000090
&CCN_MAIN SETB 0                                                         000090
&CCN_NAB_STORED SETB 0                                                   000090
&CCN_STATIC SETB 0                                                       000090
&CCN_ALTGPR(1) SETB 1                                                    000090
&CCN_ALTGPR(2) SETB 1                                                    000090
&CCN_ALTGPR(3) SETB 1                                                    000090
&CCN_ALTGPR(4) SETB 1                                                    000090
&CCN_ALTGPR(5) SETB 1                                                    000090
&CCN_ALTGPR(6) SETB 1                                                    000090
&CCN_ALTGPR(7) SETB 1                                                    000090
&CCN_ALTGPR(8) SETB 1                                                    000090
&CCN_ALTGPR(9) SETB 0                                                    000090
&CCN_ALTGPR(10) SETB 0                                                   000090
&CCN_ALTGPR(11) SETB 0                                                   000090
&CCN_ALTGPR(12) SETB 0                                                   000090
&CCN_ALTGPR(13) SETB 0                                                   000090
&CCN_ALTGPR(14) SETB 1                                                   000090
&CCN_ALTGPR(15) SETB 1                                                   000090
&CCN_ALTGPR(16) SETB 1                                                   000090
         MYPROLOG                                                        000090
@@BGN@6  DS    0H                                                        000090
         AIF   (NOT &CCN_SASIG).@@NOSIG6                                 000090
         LLILH 7,X'C6F4'                                                 000090
         OILL  7,X'E2C1'                                                 000090
         ST    7,4(,13)                                                  000090
.@@NOSIG6 ANOP                                                           000090
         USING @@AUTO@6,13                                               000090
         LARL  3,@@LIT@6                                                 000090
         USING @@LIT@6,3                                                 000090
         STG   1,464(0,13)             #SR_PARM_6                        000090
*  long segments;                                                        000091
*  long origin;                                                          000092
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000093
         LG    14,464(0,13)            #SR_PARM_6                        000093
         USING @@PARMD@6,14                                              000093
         LG    14,@17userExtendedPrivateAreaMemoryType@2                 000093
         LGF   14,0(0,14)              (*)int                            000093
         STG   14,@21useMemoryType@8                                     000093
*  int  iarv64_rc = 0;                                                   000094
         LGHI  14,0                                                      000094
         ST    14,@22iarv64_rc@9                                         000094
*                                                                        000095
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,NGETSTOR)":"DS"(wgetstor));         000096
*                                                                        000097
*  segments = *numMBSegments;                                            000098
         LG    14,464(0,13)            #SR_PARM_6                        000098
         LG    14,@16numMBSegments@1                                     000098
         LGF   14,0(0,14)              (*)int                            000098
         STG   14,@24segments@6                                          000098
*  wgetstor = ngetstor;                                                  000099
         LARL  14,$STATIC                                                000099
         DROP  14                                                        000099
         USING @@STATICD@@,14                                            000099
         MVC   @23wgetstor,@14ngetstor                                   000099
*                                                                        000100
*  if (startAddress == (void*)0) {                                       000101
         LG    14,464(0,13)            #SR_PARM_6                        000101
         DROP  14                                                        000101
         USING @@PARMD@6,14                                              000101
         LG    14,@19startAddress@4                                      000101
         CLG   14,=X'0000000000000000'                                   000101
         BRNE  @6L15                                                     000101
*   switch (useMemoryType) {                                             000102
         LG    14,@21useMemoryType@8                                     000102
         STG   14,472(0,13)            #SW_WORK6                         000102
         CLG   14,=X'0000000000000002'                                   000102
         BRH   @6L61                                                     000102
         LG    14,472(0,13)            #SW_WORK6                         000102
         SLLG  14,14,2                                                   000102
         LGFR  15,14                                                     000102
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,24(15,14)                                              000102
         B     0(3,14)                                                   000102
@6L61    DS    0H                                                        000102
         BRU   @6L66                                                     000102
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000103
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000104
@6L62    DS    0H                                                        000104
         LA    2,@25origin@7                                             000104
         LA    4,@24segments@6                                           000104
         LA    5,@23wgetstor                                             000104
         LG    14,464(0,13)            #SR_PARM_6                        000104
         LG    6,@18ttkn@3                                               000104
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000104
               AMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(4),ORIGIN=(X 000104
               2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))                  000104
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000105
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000106
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000107
*    break;                                                              000108
         BRU   @6L25                                                     000108
*   case ZOS64_VMEM_2_TO_32G:                                            000109
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000110
@6L63    DS    0H                                                        000110
         LA    2,@25origin@7                                             000110
         LA    4,@24segments@6                                           000110
         LA    5,@23wgetstor                                             000110
         LG    14,464(0,13)            #SR_PARM_6                        000110
         LG    6,@18ttkn@3                                               000110
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000110
               O32G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000110
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))   000110
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000111
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000112
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000113
*    break;                                                              000114
         BRU   @6L25                                                     000114
*   case ZOS64_VMEM_2_TO_64G:                                            000115
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000116
@6L64    DS    0H                                                        000116
         LA    2,@25origin@7                                             000116
         LA    4,@24segments@6                                           000116
         LA    5,@23wgetstor                                             000116
         LG    14,464(0,13)            #SR_PARM_6                        000116
         LG    6,@18ttkn@3                                               000116
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000116
               O64G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000116
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))   000116
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000117
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000118
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000119
*    break;                                                              000120
@6L25    DS    0H                                                        000102
@6L66    DS    0H                                                        000000
         BRU   @6L16                                                     000120
@6L15    DS    0H                                                        000120
*   }                                                                    000121
*  } else {                                                              000122
*   switch (useMemoryType) {                                             000123
         LG    14,@21useMemoryType@8                                     000123
         STG   14,472(0,13)            #SW_WORK6                         000123
         CLG   14,=X'0000000000000002'                                   000123
         BRH   @6L67                                                     000123
         LG    14,472(0,13)            #SW_WORK6                         000123
         SLLG  14,14,2                                                   000123
         LGFR  15,14                                                     000123
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,36(15,14)                                              000123
         B     0(3,14)                                                   000123
@6L67    DS    0H                                                        000123
         BRU   @6L72                                                     000123
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000124
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000125
@6L68    DS    0H                                                        000125
         LA    2,@25origin@7                                             000125
         LA    4,@24segments@6                                           000125
         LA    5,@23wgetstor                                             000125
         LG    14,464(0,13)            #SR_PARM_6                        000125
         LG    6,@18ttkn@3                                               000125
         LA    7,@19startAddress@4                                       000125
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000125
               AMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(4),ORIGIN=(X 000125
               2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)     000125
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000126
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000127
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000128
*    break;                                                              000129
         BRU   @6L26                                                     000129
*   case ZOS64_VMEM_2_TO_32G:                                            000130
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000131
@6L69    DS    0H                                                        000131
         LA    2,@25origin@7                                             000131
         LA    4,@24segments@6                                           000131
         LA    5,@23wgetstor                                             000131
         LG    14,464(0,13)            #SR_PARM_6                        000131
         LG    6,@18ttkn@3                                               000131
         LA    7,@19startAddress@4                                       000131
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000131
               O32G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000131
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),X 000131
               INORIGIN=(7)                                              000131
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000132
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000133
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000134
*    break;                                                              000135
         BRU   @6L26                                                     000135
*   case ZOS64_VMEM_2_TO_64G:                                            000136
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000137
@6L70    DS    0H                                                        000137
         LA    2,@25origin@7                                             000137
         LA    4,@24segments@6                                           000137
         LA    5,@23wgetstor                                             000137
         LG    14,464(0,13)            #SR_PARM_6                        000137
         LG    6,@18ttkn@3                                               000137
         LA    7,@19startAddress@4                                       000137
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000137
               O64G=YES,PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENX 000137
               TS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),X 000137
               INORIGIN=(7)                                              000137
*      "PAGEFRAMESIZE=PAGEABLE1MEG,TYPE=PAGEABLE,SEGMENTS=(%2),"\        000138
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000139
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000140
*    break;                                                              000141
@6L26    DS    0H                                                        000123
@6L72    DS    0H                                                        000000
@6L16    DS    0H                                                        000141
*   }                                                                    000142
*  }                                                                     000143
*                                                                        000144
*  if (0 != iarv64_rc) {                                                 000145
         LGF   14,@22iarv64_rc@9                                         000145
         LTR   14,14                                                     000145
         BRE   @6L17                                                     000145
*   return (void *)0;                                                    000146
         LGHI  15,0                                                      000146
         BRU   @6L27                                                     000146
@6L17    DS    0H                                                        000146
*  }                                                                     000147
*  return (void *)origin;                                                000148
         LG    15,@25origin@7                                            000148
* }                                                                      000149
@6L27    DS    0H                                                        000149
         DROP                                                            000149
         MYEPILOG                                                        000149
OMRIARV64 CSECT ,                                                        000149
         DS    0FD                                                       000149
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
* void * omrallocate_2G_pages(int *num2GBUnits, int *userExtendedPrivat  000165
         ENTRY @@CCN@27                                                  000165
@@CCN@27 AMODE 64                                                        000165
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000165
         DC    A(@@FPB@5-*+8)          Signed offset to FPB              000165
         DC    XL4'00000000'           Reserved                          000165
@@CCN@27 DS    0FD                                                       000165
&CCN_PRCN SETC '@@CCN@27'                                                000165
&CCN_PRCN_LONG SETC 'omrallocate_2G_pages'                               000165
&CCN_LITN SETC '@@LIT@5'                                                 000165
&CCN_BEGIN SETC '@@BGN@5'                                                000165
&CCN_ASCM SETC 'P'                                                       000165
&CCN_DSASZ SETA 480                                                      000165
&CCN_SASZ SETA 144                                                       000165
&CCN_ARGS SETA 4                                                         000165
&CCN_RLOW SETA 14                                                        000165
&CCN_RHIGH SETA 7                                                        000165
&CCN_NAB SETB  0                                                         000165
&CCN_MAIN SETB 0                                                         000165
&CCN_NAB_STORED SETB 0                                                   000165
&CCN_STATIC SETB 0                                                       000165
&CCN_ALTGPR(1) SETB 1                                                    000165
&CCN_ALTGPR(2) SETB 1                                                    000165
&CCN_ALTGPR(3) SETB 1                                                    000165
&CCN_ALTGPR(4) SETB 1                                                    000165
&CCN_ALTGPR(5) SETB 1                                                    000165
&CCN_ALTGPR(6) SETB 1                                                    000165
&CCN_ALTGPR(7) SETB 1                                                    000165
&CCN_ALTGPR(8) SETB 1                                                    000165
&CCN_ALTGPR(9) SETB 0                                                    000165
&CCN_ALTGPR(10) SETB 0                                                   000165
&CCN_ALTGPR(11) SETB 0                                                   000165
&CCN_ALTGPR(12) SETB 0                                                   000165
&CCN_ALTGPR(13) SETB 0                                                   000165
&CCN_ALTGPR(14) SETB 1                                                   000165
&CCN_ALTGPR(15) SETB 1                                                   000165
&CCN_ALTGPR(16) SETB 1                                                   000165
         MYPROLOG                                                        000165
@@BGN@5  DS    0H                                                        000165
         AIF   (NOT &CCN_SASIG).@@NOSIG5                                 000165
         LLILH 7,X'C6F4'                                                 000165
         OILL  7,X'E2C1'                                                 000165
         ST    7,4(,13)                                                  000165
.@@NOSIG5 ANOP                                                           000165
         USING @@AUTO@5,13                                               000165
         LARL  3,@@LIT@5                                                 000165
         USING @@LIT@5,3                                                 000165
         STG   1,464(0,13)             #SR_PARM_5                        000165
*  long units;                                                           000166
*  long origin;                                                          000167
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000168
         LG    14,464(0,13)            #SR_PARM_5                        000168
         USING @@PARMD@5,14                                              000168
         LG    14,@29userExtendedPrivateAreaMemoryType@10                000168
         LGF   14,0(0,14)              (*)int                            000168
         STG   14,@33useMemoryType@15                                    000168
*  int  iarv64_rc = 0;                                                   000169
         LGHI  14,0                                                      000169
         ST    14,@34iarv64_rc@16                                        000169
*                                                                        000170
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,OGETSTOR)":"DS"(wgetstor));         000171
*                                                                        000172
*  units = *num2GBUnits;                                                 000173
         LG    14,464(0,13)            #SR_PARM_5                        000173
         LG    14,@28num2GBUnits                                         000173
         LGF   14,0(0,14)              (*)int                            000173
         STG   14,@36units                                               000173
*  wgetstor = ogetstor;                                                  000174
         LARL  14,$STATIC                                                000174
         DROP  14                                                        000174
         USING @@STATICD@@,14                                            000174
         MVC   @35wgetstor,@26ogetstor                                   000174
*                                                                        000175
*  if (startAddress == (void*)0) {                                       000176
         LG    14,464(0,13)            #SR_PARM_5                        000176
         DROP  14                                                        000176
         USING @@PARMD@5,14                                              000176
         LG    14,@31startAddress@12                                     000176
         CLG   14,=X'0000000000000000'                                   000176
         BRNE  @5L11                                                     000176
*   switch (useMemoryType) {                                             000177
         LG    14,@33useMemoryType@15                                    000177
         STG   14,472(0,13)            #SW_WORK5                         000177
         CLG   14,=X'0000000000000002'                                   000177
         BRH   @5L49                                                     000177
         LG    14,472(0,13)            #SW_WORK5                         000177
         SLLG  14,14,2                                                   000177
         LGFR  15,14                                                     000177
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,48(15,14)                                              000177
         B     0(3,14)                                                   000177
@5L49    DS    0H                                                        000177
         BRU   @5L54                                                     000177
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000178
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000179
@5L50    DS    0H                                                        000179
         LA    2,@37origin@14                                            000179
         LA    4,@36units                                                000179
         LA    5,@35wgetstor                                             000179
         LG    14,464(0,13)            #SR_PARM_5                        000179
         LG    6,@30ttkn@11                                              000179
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000179
               AMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(4),ORIGIN=(2),TX 000179
               TOKEN=(6),RETCODE=200(13),MF=(E,(5))                      000179
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000180
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000181
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000182
*    break;                                                              000183
         BRU   @5L28                                                     000183
*   case ZOS64_VMEM_2_TO_32G:                                            000184
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000185
@5L51    DS    0H                                                        000185
         LA    2,@37origin@14                                            000185
         LA    4,@36units                                                000185
         LA    5,@35wgetstor                                             000185
         LG    14,464(0,13)            #SR_PARM_5                        000185
         LG    6,@30ttkn@11                                              000185
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000185
               O32G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000185
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))       000185
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000186
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000187
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000188
*    break;                                                              000189
         BRU   @5L28                                                     000189
*   case ZOS64_VMEM_2_TO_64G:                                            000190
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000191
@5L52    DS    0H                                                        000191
         LA    2,@37origin@14                                            000191
         LA    4,@36units                                                000191
         LA    5,@35wgetstor                                             000191
         LG    14,464(0,13)            #SR_PARM_5                        000191
         LG    6,@30ttkn@11                                              000191
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000191
               O64G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000191
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5))       000191
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000192
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\                 000193
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000194
*    break;                                                              000195
@5L28    DS    0H                                                        000177
@5L54    DS    0H                                                        000000
         BRU   @5L12                                                     000195
@5L11    DS    0H                                                        000195
*   }                                                                    000196
*  } else {                                                              000197
*   switch (useMemoryType) {                                             000198
         LG    14,@33useMemoryType@15                                    000198
         STG   14,472(0,13)            #SW_WORK5                         000198
         CLG   14,=X'0000000000000002'                                   000198
         BRH   @5L55                                                     000198
         LG    14,472(0,13)            #SW_WORK5                         000198
         SLLG  14,14,2                                                   000198
         LGFR  15,14                                                     000198
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,60(15,14)                                              000198
         B     0(3,14)                                                   000198
@5L55    DS    0H                                                        000198
         BRU   @5L60                                                     000198
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000199
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,"\  000200
@5L56    DS    0H                                                        000200
         LA    2,@37origin@14                                            000200
         LA    4,@36units                                                000200
         LA    5,@35wgetstor                                             000200
         LG    14,464(0,13)            #SR_PARM_5                        000200
         LG    6,@30ttkn@11                                              000200
         LA    7,@31startAddress@12                                      000200
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000200
               AMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(4),ORIGIN=(2),TX 000200
               TOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)         000200
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000201
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000202
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000203
*    break;                                                              000204
         BRU   @5L29                                                     000204
*   case ZOS64_VMEM_2_TO_32G:                                            000205
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000206
@5L57    DS    0H                                                        000206
         LA    2,@37origin@14                                            000206
         LA    4,@36units                                                000206
         LA    5,@35wgetstor                                             000206
         LG    14,464(0,13)            #SR_PARM_5                        000206
         LG    6,@30ttkn@11                                              000206
         LA    7,@31startAddress@12                                      000206
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000206
               O32G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000206
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORX 000206
               IGIN=(7)                                                  000206
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000207
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000208
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000209
*    break;                                                              000210
         BRU   @5L29                                                     000210
*   case ZOS64_VMEM_2_TO_64G:                                            000211
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,US  000212
@5L58    DS    0H                                                        000212
         LA    2,@37origin@14                                            000212
         LA    4,@36units                                                000212
         LA    5,@35wgetstor                                             000212
         LG    14,464(0,13)            #SR_PARM_5                        000212
         LG    6,@30ttkn@11                                              000212
         LA    7,@31startAddress@12                                      000212
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,USE2GTX 000212
               O64G=YES,PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(X 000212
               4),ORIGIN=(2),TTOKEN=(6),RETCODE=200(13),MF=(E,(5)),INORX 000212
               IGIN=(7)                                                  000212
*      "PAGEFRAMESIZE=2G,TYPE=FIXED,UNITSIZE=2G,UNITS=(%2),"\            000213
*      "ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INORIGIN=(%5)"\   000214
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&units),"r"(&wgetstor),"r"(ttk  000215
*    break;                                                              000216
@5L29    DS    0H                                                        000198
@5L60    DS    0H                                                        000000
@5L12    DS    0H                                                        000216
*   }                                                                    000217
*  }                                                                     000218
*                                                                        000219
*  if (0 != iarv64_rc) {                                                 000220
         LGF   14,@34iarv64_rc@16                                        000220
         LTR   14,14                                                     000220
         BRE   @5L13                                                     000220
*   return (void *)0;                                                    000221
         LGHI  15,0                                                      000221
         BRU   @5L30                                                     000221
@5L13    DS    0H                                                        000221
*  }                                                                     000222
*  return (void *)origin;                                                000223
         LG    15,@37origin@14                                           000223
* }                                                                      000224
@5L30    DS    0H                                                        000224
         DROP                                                            000224
         MYEPILOG                                                        000224
OMRIARV64 CSECT ,                                                        000224
         DS    0FD                                                       000224
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
* void * omrallocate_4K_pages_in_userExtendedPrivateArea(int *numMBSegm  000240
         ENTRY @@CCN@39                                                  000240
@@CCN@39 AMODE 64                                                        000240
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000240
         DC    A(@@FPB@4-*+8)          Signed offset to FPB              000240
         DC    XL4'00000000'           Reserved                          000240
@@CCN@39 DS    0FD                                                       000240
&CCN_PRCN SETC '@@CCN@39'                                                000240
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_in_userExtendedPrivateArea'    000240
&CCN_LITN SETC '@@LIT@4'                                                 000240
&CCN_BEGIN SETC '@@BGN@4'                                                000240
&CCN_ASCM SETC 'P'                                                       000240
&CCN_DSASZ SETA 480                                                      000240
&CCN_SASZ SETA 144                                                       000240
&CCN_ARGS SETA 4                                                         000240
&CCN_RLOW SETA 14                                                        000240
&CCN_RHIGH SETA 7                                                        000240
&CCN_NAB SETB  0                                                         000240
&CCN_MAIN SETB 0                                                         000240
&CCN_NAB_STORED SETB 0                                                   000240
&CCN_STATIC SETB 0                                                       000240
&CCN_ALTGPR(1) SETB 1                                                    000240
&CCN_ALTGPR(2) SETB 1                                                    000240
&CCN_ALTGPR(3) SETB 1                                                    000240
&CCN_ALTGPR(4) SETB 1                                                    000240
&CCN_ALTGPR(5) SETB 1                                                    000240
&CCN_ALTGPR(6) SETB 1                                                    000240
&CCN_ALTGPR(7) SETB 1                                                    000240
&CCN_ALTGPR(8) SETB 1                                                    000240
&CCN_ALTGPR(9) SETB 0                                                    000240
&CCN_ALTGPR(10) SETB 0                                                   000240
&CCN_ALTGPR(11) SETB 0                                                   000240
&CCN_ALTGPR(12) SETB 0                                                   000240
&CCN_ALTGPR(13) SETB 0                                                   000240
&CCN_ALTGPR(14) SETB 1                                                   000240
&CCN_ALTGPR(15) SETB 1                                                   000240
&CCN_ALTGPR(16) SETB 1                                                   000240
         MYPROLOG                                                        000240
@@BGN@4  DS    0H                                                        000240
         AIF   (NOT &CCN_SASIG).@@NOSIG4                                 000240
         LLILH 7,X'C6F4'                                                 000240
         OILL  7,X'E2C1'                                                 000240
         ST    7,4(,13)                                                  000240
.@@NOSIG4 ANOP                                                           000240
         USING @@AUTO@4,13                                               000240
         LARL  3,@@LIT@4                                                 000240
         USING @@LIT@4,3                                                 000240
         STG   1,464(0,13)             #SR_PARM_4                        000240
*  long segments;                                                        000241
*  long origin;                                                          000242
*  long useMemoryType = *userExtendedPrivateAreaMemoryType;              000243
         LG    14,464(0,13)            #SR_PARM_4                        000243
         USING @@PARMD@4,14                                              000243
         LG    14,@41userExtendedPrivateAreaMemoryType@18                000243
         LGF   14,0(0,14)              (*)int                            000243
         STG   14,@45useMemoryType@24                                    000243
*  int  iarv64_rc = 0;                                                   000244
         LGHI  14,0                                                      000244
         ST    14,@46iarv64_rc@25                                        000244
*                                                                        000245
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,MGETSTOR)":"DS"(wgetstor));         000246
*                                                                        000247
*  segments = *numMBSegments;                                            000248
         LG    14,464(0,13)            #SR_PARM_4                        000248
         LG    14,@40numMBSegments@17                                    000248
         LGF   14,0(0,14)              (*)int                            000248
         STG   14,@48segments@22                                         000248
*  wgetstor = mgetstor;                                                  000249
         LARL  14,$STATIC                                                000249
         DROP  14                                                        000249
         USING @@STATICD@@,14                                            000249
         MVC   @47wgetstor,@38mgetstor                                   000249
*                                                                        000250
*  if (startAddress == (void*)0) {                                       000251
         LG    14,464(0,13)            #SR_PARM_4                        000251
         DROP  14                                                        000251
         USING @@PARMD@4,14                                              000251
         LG    14,@43startAddress@20                                     000251
         CLG   14,=X'0000000000000000'                                   000251
         BRNE  @4L7                                                      000251
*   switch (useMemoryType) {                                             000252
         LG    14,@45useMemoryType@24                                    000252
         STG   14,472(0,13)            #SW_WORK4                         000252
         CLG   14,=X'0000000000000002'                                   000252
         BRH   @4L37                                                     000252
         LG    14,472(0,13)            #SW_WORK4                         000252
         SLLG  14,14,2                                                   000252
         LGFR  15,14                                                     000252
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,72(15,14)                                              000252
         B     0(3,14)                                                   000252
@4L37    DS    0H                                                        000252
         BRU   @4L42                                                     000252
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000253
*    break;                                                              000254
@4L38    DS    0H                                                        000254
         BRU   @4L31                                                     000254
*   case ZOS64_VMEM_2_TO_32G:                                            000255
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000256
@4L39    DS    0H                                                        000256
         LA    2,@49origin@23                                            000256
         LA    4,@48segments@22                                          000256
         LA    5,@47wgetstor                                             000256
         LG    14,464(0,13)            #SR_PARM_4                        000256
         LG    6,@42ttkn@19                                              000256
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000256
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000256
               =(6),RETCODE=200(13),MF=(E,(5))                           000256
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000257
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000258
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000259
*    break;                                                              000260
         BRU   @4L31                                                     000260
*   case ZOS64_VMEM_2_TO_64G:                                            000261
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000262
@4L40    DS    0H                                                        000262
         LA    2,@49origin@23                                            000262
         LA    4,@48segments@22                                          000262
         LA    5,@47wgetstor                                             000262
         LG    14,464(0,13)            #SR_PARM_4                        000262
         LG    6,@42ttkn@19                                              000262
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000262
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000262
               =(6),RETCODE=200(13),MF=(E,(5))                           000262
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000263
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\   000264
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000265
*    break;                                                              000266
@4L31    DS    0H                                                        000252
@4L42    DS    0H                                                        000000
         BRU   @4L8                                                      000266
@4L7     DS    0H                                                        000266
*   }                                                                    000267
*  } else {                                                              000268
*   switch (useMemoryType) {                                             000269
         LG    14,@45useMemoryType@24                                    000269
         STG   14,472(0,13)            #SW_WORK4                         000269
         CLG   14,=X'0000000000000002'                                   000269
         BRH   @4L43                                                     000269
         LG    14,472(0,13)            #SW_WORK4                         000269
         SLLG  14,14,2                                                   000269
         LGFR  15,14                                                     000269
         LARL  14,@@CONST@AREA@@                                         000000
         LGF   14,84(15,14)                                              000269
         B     0(3,14)                                                   000269
@4L43    DS    0H                                                        000269
         BRU   @4L48                                                     000269
*   case ZOS64_VMEM_ABOVE_BAR_GENERAL:                                   000270
*    break;                                                              000271
@4L44    DS    0H                                                        000271
         BRU   @4L32                                                     000271
*   case ZOS64_VMEM_2_TO_32G:                                            000272
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,"\  000273
@4L45    DS    0H                                                        000273
         LA    2,@49origin@23                                            000273
         LA    4,@48segments@22                                          000273
         LA    5,@47wgetstor                                             000273
         LG    14,464(0,13)            #SR_PARM_4                        000273
         LG    6,@42ttkn@19                                              000273
         LA    7,@43startAddress@20                                      000273
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO32G=YES,CONTROX 000273
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000273
               =(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)              000273
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000274
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000275
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000276
*    break;                                                              000277
         BRU   @4L32                                                     000277
*   case ZOS64_VMEM_2_TO_64G:                                            000278
*    __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,"\  000279
@4L46    DS    0H                                                        000279
         LA    2,@49origin@23                                            000279
         LA    4,@48segments@22                                          000279
         LA    5,@47wgetstor                                             000279
         LG    14,464(0,13)            #SR_PARM_4                        000279
         LG    6,@42ttkn@19                                              000279
         LA    7,@43startAddress@20                                      000279
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,USE2GTO64G=YES,CONTROX 000279
               L=UNAUTH,PAGEFRAMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKENX 000279
               =(6),RETCODE=200(13),MF=(E,(5)),INORIGIN=(7)              000279
*      "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                               000280
*      "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),IN  000281
*      ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(  000282
*    break;                                                              000283
@4L32    DS    0H                                                        000269
@4L48    DS    0H                                                        000000
@4L8     DS    0H                                                        000283
*   }                                                                    000284
*  }                                                                     000285
*                                                                        000286
*  if (0 != iarv64_rc) {                                                 000287
         LGF   14,@46iarv64_rc@25                                        000287
         LTR   14,14                                                     000287
         BRE   @4L9                                                      000287
*   return (void *)0;                                                    000288
         LGHI  15,0                                                      000288
         BRU   @4L33                                                     000288
@4L9     DS    0H                                                        000288
*  }                                                                     000289
*  return (void *)origin;                                                000290
         LG    15,@49origin@23                                           000290
* }                                                                      000291
@4L33    DS    0H                                                        000291
         DROP                                                            000291
         MYEPILOG                                                        000291
OMRIARV64 CSECT ,                                                        000291
         DS    0FD                                                       000291
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
* void * omrallocate_4K_pages_above_bar(int *numMBSegments, const char   000306
         ENTRY @@CCN@51                                                  000306
@@CCN@51 AMODE 64                                                        000306
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000306
         DC    A(@@FPB@3-*+8)          Signed offset to FPB              000306
         DC    XL4'00000000'           Reserved                          000306
@@CCN@51 DS    0FD                                                       000306
&CCN_PRCN SETC '@@CCN@51'                                                000306
&CCN_PRCN_LONG SETC 'omrallocate_4K_pages_above_bar'                     000306
&CCN_LITN SETC '@@LIT@3'                                                 000306
&CCN_BEGIN SETC '@@BGN@3'                                                000306
&CCN_ASCM SETC 'P'                                                       000306
&CCN_DSASZ SETA 464                                                      000306
&CCN_SASZ SETA 144                                                       000306
&CCN_ARGS SETA 3                                                         000306
&CCN_RLOW SETA 14                                                        000306
&CCN_RHIGH SETA 7                                                        000306
&CCN_NAB SETB  0                                                         000306
&CCN_MAIN SETB 0                                                         000306
&CCN_NAB_STORED SETB 0                                                   000306
&CCN_STATIC SETB 0                                                       000306
&CCN_ALTGPR(1) SETB 1                                                    000306
&CCN_ALTGPR(2) SETB 1                                                    000306
&CCN_ALTGPR(3) SETB 1                                                    000306
&CCN_ALTGPR(4) SETB 1                                                    000306
&CCN_ALTGPR(5) SETB 1                                                    000306
&CCN_ALTGPR(6) SETB 1                                                    000306
&CCN_ALTGPR(7) SETB 1                                                    000306
&CCN_ALTGPR(8) SETB 1                                                    000306
&CCN_ALTGPR(9) SETB 0                                                    000306
&CCN_ALTGPR(10) SETB 0                                                   000306
&CCN_ALTGPR(11) SETB 0                                                   000306
&CCN_ALTGPR(12) SETB 0                                                   000306
&CCN_ALTGPR(13) SETB 0                                                   000306
&CCN_ALTGPR(14) SETB 1                                                   000306
&CCN_ALTGPR(15) SETB 1                                                   000306
&CCN_ALTGPR(16) SETB 1                                                   000306
         MYPROLOG                                                        000306
@@BGN@3  DS    0H                                                        000306
         AIF   (NOT &CCN_SASIG).@@NOSIG3                                 000306
         LLILH 7,X'C6F4'                                                 000306
         OILL  7,X'E2C1'                                                 000306
         ST    7,4(,13)                                                  000306
.@@NOSIG3 ANOP                                                           000306
         USING @@AUTO@3,13                                               000306
         LARL  3,@@LIT@3                                                 000306
         USING @@LIT@3,3                                                 000306
         STG   1,456(0,13)             #SR_PARM_3                        000306
*  long segments;                                                        000307
*  long origin;                                                          000308
*  int  iarv64_rc = 0;                                                   000309
         LGHI  14,0                                                      000309
         ST    14,@56iarv64_rc@32                                        000309
*                                                                        000310
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,RGETSTOR)":"DS"(wgetstor));         000311
*                                                                        000312
*  segments = *numMBSegments;                                            000313
         LG    14,456(0,13)            #SR_PARM_3                        000313
         USING @@PARMD@3,14                                              000313
         LG    14,@52numMBSegments@26                                    000313
         LGF   14,0(0,14)              (*)int                            000313
         STG   14,@58segments@30                                         000313
*  wgetstor = rgetstor;                                                  000314
         LARL  14,$STATIC                                                000314
         DROP  14                                                        000314
         USING @@STATICD@@,14                                            000314
         MVC   @57wgetstor,@50rgetstor                                   000314
*                                                                        000315
*  if (startAddress == (void*)0) {                                       000316
         LG    14,456(0,13)            #SR_PARM_3                        000316
         DROP  14                                                        000316
         USING @@PARMD@3,14                                              000316
         LG    14,@54startAddress@28                                     000316
         CLG   14,=X'0000000000000000'                                   000316
         BRNE  @3L3                                                      000316
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                  000317
         LA    2,@59origin@31                                            000317
         LA    4,@58segments@30                                          000317
         LA    5,@57wgetstor                                             000317
         LG    14,456(0,13)            #SR_PARM_3                        000317
         LG    6,@53ttkn@27                                              000317
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000317
               AMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=19X 000317
               2(13),MF=(E,(5))                                          000317
         BRU   @3L4                                                      000317
@3L3     DS    0H                                                        000317
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000318
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3))"\    000319
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000320
*  } else {                                                              000321
*   __asm(" IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,"\                  000322
         LA    2,@59origin@31                                            000322
         LA    4,@58segments@30                                          000322
         LA    5,@57wgetstor                                             000322
         LG    14,456(0,13)            #SR_PARM_3                        000322
         LG    6,@53ttkn@27                                              000322
         LA    7,@54startAddress@28                                      000322
         IARV64 REQUEST=GETSTOR,COND=YES,SADMP=NO,CONTROL=UNAUTH,PAGEFRX 000322
               AMESIZE=4K,SEGMENTS=(4),ORIGIN=(2),TTOKEN=(6),RETCODE=19X 000322
               2(13),MF=(E,(5)),INORIGIN=(7)                             000322
@3L4     DS    0H                                                        000322
*     "CONTROL=UNAUTH,PAGEFRAMESIZE=4K,"\                                000323
*     "SEGMENTS=(%2),ORIGIN=(%1),TTOKEN=(%4),RETCODE=%0,MF=(E,(%3)),INO  000324
*     ::"m"(iarv64_rc),"r"(&origin),"r"(&segments),"r"(&wgetstor),"r"(t  000325
*  }                                                                     000326
*                                                                        000327
*  if (0 != iarv64_rc) {                                                 000328
         LGF   14,@56iarv64_rc@32                                        000328
         LTR   14,14                                                     000328
         BRE   @3L5                                                      000328
*   return (void *)0;                                                    000329
         LGHI  15,0                                                      000329
         BRU   @3L34                                                     000329
@3L5     DS    0H                                                        000329
*  }                                                                     000330
*  return (void *)origin;                                                000331
         LG    15,@59origin@31                                           000331
* }                                                                      000332
@3L34    DS    0H                                                        000332
         DROP                                                            000332
         MYEPILOG                                                        000332
OMRIARV64 CSECT ,                                                        000332
         DS    0FD                                                       000332
@@LIT@3  LTORG                                                           000000
@@FPB@   LOCTR                                                           000000
@@FPB@3  DS    0FD                     Function Property Block           000000
         DC    XL2'CCD5'               Eyecatcher                        000000
         DC    BL2'1111111100000011'   Saved GPR Mask                    000000
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
* int omrfree_memory_above_bar(void *address, const char * ttkn){        000346
         ENTRY @@CCN@61                                                  000346
@@CCN@61 AMODE 64                                                        000346
         DC    XL8'00C300C300D50100'   Function Entry Point Marker       000346
         DC    A(@@FPB@2-*+8)          Signed offset to FPB              000346
         DC    XL4'00000000'           Reserved                          000346
@@CCN@61 DS    0FD                                                       000346
&CCN_PRCN SETC '@@CCN@61'                                                000346
&CCN_PRCN_LONG SETC 'omrfree_memory_above_bar'                           000346
&CCN_LITN SETC '@@LIT@2'                                                 000346
&CCN_BEGIN SETC '@@BGN@2'                                                000346
&CCN_ASCM SETC 'P'                                                       000346
&CCN_DSASZ SETA 456                                                      000346
&CCN_SASZ SETA 144                                                       000346
&CCN_ARGS SETA 2                                                         000346
&CCN_RLOW SETA 14                                                        000346
&CCN_RHIGH SETA 5                                                        000346
&CCN_NAB SETB  0                                                         000346
&CCN_MAIN SETB 0                                                         000346
&CCN_NAB_STORED SETB 0                                                   000346
&CCN_STATIC SETB 0                                                       000346
&CCN_ALTGPR(1) SETB 1                                                    000346
&CCN_ALTGPR(2) SETB 1                                                    000346
&CCN_ALTGPR(3) SETB 1                                                    000346
&CCN_ALTGPR(4) SETB 1                                                    000346
&CCN_ALTGPR(5) SETB 1                                                    000346
&CCN_ALTGPR(6) SETB 1                                                    000346
&CCN_ALTGPR(7) SETB 0                                                    000346
&CCN_ALTGPR(8) SETB 0                                                    000346
&CCN_ALTGPR(9) SETB 0                                                    000346
&CCN_ALTGPR(10) SETB 0                                                   000346
&CCN_ALTGPR(11) SETB 0                                                   000346
&CCN_ALTGPR(12) SETB 0                                                   000346
&CCN_ALTGPR(13) SETB 0                                                   000346
&CCN_ALTGPR(14) SETB 1                                                   000346
&CCN_ALTGPR(15) SETB 1                                                   000346
&CCN_ALTGPR(16) SETB 1                                                   000346
         MYPROLOG                                                        000346
@@BGN@2  DS    0H                                                        000346
         AIF   (NOT &CCN_SASIG).@@NOSIG2                                 000346
         LLILH 5,X'C6F4'                                                 000346
         OILL  5,X'E2C1'                                                 000346
         ST    5,4(,13)                                                  000346
.@@NOSIG2 ANOP                                                           000346
         USING @@AUTO@2,13                                               000346
         LARL  3,@@LIT@2                                                 000346
         USING @@LIT@2,3                                                 000346
         STG   1,448(0,13)             #SR_PARM_2                        000346
*  void * xmemobjstart;                                                  000347
*  int  iarv64_rc = 0;                                                   000348
         LGHI  14,0                                                      000348
         ST    14,@65iarv64_rc@35                                        000348
*                                                                        000349
*  __asm(" IARV64 PLISTVER=MAX,MF=(L,PGETSTOR)":"DS"(wgetstor));         000350
*                                                                        000351
*  xmemobjstart = address;                                               000352
         LG    14,448(0,13)            #SR_PARM_2                        000352
         USING @@PARMD@2,14                                              000352
         LG    14,@62address                                             000352
         STG   14,@67xmemobjstart                                        000352
*  wgetstor = pgetstor;                                                  000353
         LARL  14,$STATIC                                                000353
         DROP  14                                                        000353
         USING @@STATICD@@,14                                            000353
         MVC   @66wgetstor,@60pgetstor                                   000353
*                                                                        000354
*     __asm(" IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(%2),TTOKEN=(%  000355
         LA    2,@66wgetstor                                             000355
         DROP  14                                                        000355
         LA    4,@67xmemobjstart                                         000355
         LG    14,448(0,13)            #SR_PARM_2                        000355
         USING @@PARMD@2,14                                              000355
         LG    5,@63ttkn@33                                              000355
         IARV64 REQUEST=DETACH,COND=YES,MEMOBJSTART=(4),TTOKEN=(5),RETCX 000355
               ODE=184(13),MF=(E,(2))                                    000355
*             ::"m"(iarv64_rc),"r"(&wgetstor),"r"(&xmemobjstart),"r"(tt  000356
*  return iarv64_rc;                                                     000357
         LGF   15,@65iarv64_rc@35                                        000357
* }                                                                      000358
@2L35    DS    0H                                                        000358
         DROP                                                            000358
         MYEPILOG                                                        000358
OMRIARV64 CSECT ,                                                        000358
         DS    0FD                                                       000358
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
*                                                                        000397
         EJECT                                                           000000
OMRIARV64 CSECT ,                                                        000000
@@CONST@AREA@@ DS 0D                                                     000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         DC    XL16'00000000000000000000000000000000'                    000000
         ORG   @@CONST@AREA@@+0                                          000000
         DC    A(@7L74-@@LIT@7)                                          000000
         DC    A(@7L75-@@LIT@7)                                          000000
         DC    A(@7L76-@@LIT@7)                                          000000
         ORG   @@CONST@AREA@@+12                                         000000
         DC    A(@7L80-@@LIT@7)                                          000000
         DC    A(@7L81-@@LIT@7)                                          000000
         DC    A(@7L82-@@LIT@7)                                          000000
         ORG   @@CONST@AREA@@+24                                         000000
         DC    A(@6L62-@@LIT@6)                                          000000
         DC    A(@6L63-@@LIT@6)                                          000000
         DC    A(@6L64-@@LIT@6)                                          000000
         ORG   @@CONST@AREA@@+36                                         000000
         DC    A(@6L68-@@LIT@6)                                          000000
         DC    A(@6L69-@@LIT@6)                                          000000
         DC    A(@6L70-@@LIT@6)                                          000000
         ORG   @@CONST@AREA@@+48                                         000000
         DC    A(@5L50-@@LIT@5)                                          000000
         DC    A(@5L51-@@LIT@5)                                          000000
         DC    A(@5L52-@@LIT@5)                                          000000
         ORG   @@CONST@AREA@@+60                                         000000
         DC    A(@5L56-@@LIT@5)                                          000000
         DC    A(@5L57-@@LIT@5)                                          000000
         DC    A(@5L58-@@LIT@5)                                          000000
         ORG   @@CONST@AREA@@+72                                         000000
         DC    A(@4L38-@@LIT@4)                                          000000
         DC    A(@4L39-@@LIT@4)                                          000000
         DC    A(@4L40-@@LIT@4)                                          000000
         ORG   @@CONST@AREA@@+84                                         000000
         DC    A(@4L44-@@LIT@4)                                          000000
         DC    A(@4L45-@@LIT@4)                                          000000
         DC    A(@4L46-@@LIT@4)                                          000000
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
         END   ,(5650ZOS   ,2101,19157)                                  000000
