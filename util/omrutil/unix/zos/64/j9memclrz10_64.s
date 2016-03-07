***********************************************************************
*
* (c) Copyright IBM Corp. 1991, 2010
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


J9ZERZ10 TITLE 'Java Zero memory'
         SPACE
* Leaf routine so set DSASIZE=0
* Must preserve registers 4,6, and 7
J9ZERZ10 CELQPRLG PARMWRDS=0,DSASIZE=0,BASEREG=NONE
r0       EQU      0
r1       EQU      1
r2       EQU      2
r3       EQU      3
r5       EQU      5
r8       EQU      8
r9       EQU      9
r10      EQU      10
r11      EQU      11
r12      EQU      12
r13      EQU      13
r14      EQU      14
r15      EQU      15
         LTR      r2,r2                               
         JE       @2L3                                
         AHI      r2,-1
         LR       r0,r2                               
         SRA      r0,8                                
         LGR      r3,r1                               
         JE       @2L20                               
* must be greater than 256 bytes
* Check if Greater than 1024 bytes to clear
         CHI      r0,3
         JH       GT1024B
         JL       LT768B
         DC    X'E32032010036' Store PREFETCH NEXT LINE
         DC    X'E32033010036' Store PREFETCH NEXT LINE
         J        LE1024B
* check if greater than 512 bytes
LT768B   CHI      r0,2
         JL       LE1024B
         DC    X'E32032010036' Store PREFETCH NEXT LINE
         J        LE1024B
* Greater than 512 bytes to clear so subtract two from loop count
GT1024B  AHI      r0,-3      
@2L19    DS       0H                                  
* z6 Limit of three concurrent cache line fetches
         DC    X'E32032010036' Store PREFETCH NEXT LINE
         DC    X'E32033010036' Store PREFETCH NEXT LINE
         XC       0(256,r3),0(r3)     
         LA       r3,256(,r3)
         BRCT     r0,@2L19                            
* add 2 back into loop count
         AHI      r0,3
LE1024B  XC       0(256,r3),0(r3)     
         LA       r3,256(,r3)
         BRCT     r0,LE1024B                           
@2L20    DS       0H                                  
         LARL     r1,@2XC                           
         EX       r2,0(0,r1)
@2L3     DS       0H                                  
         CELQEPLG      return
@2XC     XC       0(0,r3),0(r3)     
         END
