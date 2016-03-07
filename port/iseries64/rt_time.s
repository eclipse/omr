###############################################################################
#
# (c) Copyright IBM Corp. 2002, 2015
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
###############################################################################

# AIX runtime function: __getMillis
# Code called in response to invokestatic java/lang/System/currentTimeMillis
# Input: None
# Output: 64 bit count of elapsed milliseconds since epoch (Jan. 1 1970)

# Killed registers:
#   64-bit:  r0, r2-r9, cr0

 .file "rt_time.s"

# GPR definitions
 .set r0,0; .set SP,1; .set r3,3; .set r4,4
 .set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
 .set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
 .set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
 .set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
 .set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
 .set r30,30; .set r31,31

# Condition Register definitions
 .set cr0,0; .set cr1,1; .set cr2,2; .set cr3,3; .set cr4,4
 .set cr5,5; .set cr6,6; .set cr7,7

# VM Register Use
 .set RTOC,2

        .extern _system_TB_config{RW}
        .extern _system_configuration{RW}

# _system_configuration structure field offsets
 .set Xint,116; .set Xfrac,120;

        .csect rt_time_TEXT{PR}
 .align 2
 .globl .__getMillis
.__getMillis:
 .function .__getMillis,startproc.__getMillis,16,0,(endproc.__getMillis-startproc.__getMillis)
 startproc.__getMillis:


retry_millis:
        ld        r4, epochMilli(RTOC)      ## ticks since epoch from local structure
        ld        r5, T.baseTicks(RTOC)     ## address of ticks since epoch from system
      mftb        r6                        ## get current ticks
        ld        r7, multval_millis(RTOC)  ## ticks to millis factor from local
        ld        r8, shiftval_millis(RTOC) ## right shift amount from local

       add        r0, r6, r4                ## TB = tb + ticks since epoch
        ld        r5, 0x0(r5)               ## ticks since epoch from system
    mulhdu        r9, r7, r0                ## TB * millis factor
     cmpld        cr0, r4, r5               ## has ticks since epoch changed??
       srd        r3, r9, r8                ## whole part of result
        .long 0x4de20020                    ## beqlr+ for PowerPC AS
                                            ## return if time since epoch is unchanged

##--------------------------------------------------------------------------------
##  refreshTime is called the first time through and whenever the tb frequency
##  changes (should be rare).
##  It updates the local structure with:
##     ticks since epoch (epochMilli)
##     ticks to milliseconds factor (multval)
##     right shift amount for milliseconds product (shiftval)
##  Input:
##     r5 = ticks since epoch (from system)
##--------------------------------------------------------------------------------
refreshTime_millis:
        ld        r6, T.sysconfig(RTOC)     ## @sysconfig structure

       lis        r4, 0xf                   ## high 16 bits of 1000000
       std        r5, epochMilli(RTOC)      ## save ticks since epoch in local structure
      addi        r4, r4, 0x4240            ## r4 = 1000000

       lwz        r7, Xfrac(r6)             ## Xfrac from sysconfig
       lwz        r8, Xint(r6)              ## Xint from sysconfig

     mulld        r7, r7, r4                ## Xfrac * 1000000

    cntlzd        r5, r8                    ## leading zeros in Xint (>= 32)
       sld        r8, r8, r5                ## Xint * (2**(r5))
      addi        r6, r7, -1                ## 64 - cntlz((Xfrac-1) & ~Xfrac)
      andc        r6, r6, r7                ## = trailing zeros in
    cntlzd        r6, r6                    ##   (Xfrac * 1000000)
    subfic        r6, r6, 64
       srd        r7, r7, r6                ## (Xfrac * 1000000) / (2**(r6))
       add        r5, r5, r6                ## sum of shifts applied
     divdu        r4, r8, r7                ## r4 = quotient * 2**(r5)
    cntlzd        r6, r4                    ## leading zeros in quotient
       sld        r9, r4, r6                ## shifted initial quotient
       add        r5, r5, r6                ## total shift in quotient
      addi        r5, r5, -64               ## reduced because mulhdu used
       std        r5, shiftval_millis(RTOC) ## save shift in local structure

again_millis:                               ## loop until 64 bits of quotient
     mulld        r3, r4, r7
      subf        r8, r3, r8                ## r8 = remainder from divdu
    cntlzd        r5, r8                    ## leading zeros in remainder
       sld        r8, r8, r5
     divdu        r4, r8, r7                ## additional bits of quotient
      subf.       r5, r6, r5                ## right shift to apply to r4
       bge        done_millis
       neg        r6, r5                    ## actually a left shift
       sld        r3, r4, r6                ## align for merge
        or        r9, r3, r9                ## partial merged quotient
         b        again_millis              ## don't have enough bits yet

done_millis:
       srd        r3, r4, r5                ## align for merge
        or        r9, r3, r9                ## final merged quotient
       std        r9, multval_millis(RTOC)  ## save multiplier in local structure

         b        retry_millis
 endproc.__getMillis:

# AIX runtime function: __getNanos
# Code called in response to invokestatic java/lang/System/nanoTime
# Input: None
# Output: 64 bit count of elapsed nanoseconds since epoch (Jan. 1 1970)
# The following must be done in this order:
#	1. Read the local cache of Xfrac and Xint
#	2. Read TB 
#	3. Read the system Xfrac and Xint

# Killed registers:
#   64-bit:  r0, r2-r12, cr0

 .align 2
 .globl .__getNanos
.__getNanos:
 .function .__getNanos,startproc.__getNanos,16,0,(endproc.__getNanos-startproc.__getNanos)
 startproc.__getNanos:


retry_nanos:
        ld        r4, T.sysconfig(RTOC)      ## address of @sysconfig structure

        ld        r5, Xfrac_nanos(RTOC)       ## Xfrac from local structure
        ld        r6, Xint_nanos(RTOC)        ## Xint from local structure     

      mftb        r0                         ## get current ticks (TB)

       lwz        r7, Xfrac(r4)              ## Xfrac from system (sysconfig)
       lwz        r8, Xint(r4)               ## Xint from system (sysconfig)

        ld        r11, fracmult_nanos(RTOC)  ## fraction part of ticks to nanos factor from local
       lwz        r12, intmult_nanos(RTOC)   ## integer part of ticks to nanos factor from local
        ld        r9, shiftval_nanos(RTOC)   ## right shift amount from local

     cmpldi        cr0, r5, 0                ## has Xint initialized??
     
     mulld        r10, r0, r12               ## TB * intmult_nanos
   
    mulhdu        r0, r0, r11                ## TB * fracmult_nanos
       srd        r3, r0, r9                 ## shift right
       add        r3, r3, r10                ## final result

      beq-        refreshTime_nanos			 ## refresh if Xint not initialized

     cmpldi        cr0, r6, 0                ## has Xfraq initialized??
    bnelr+                                   ## return if Xfraq and Xint have initialized, else, fall through


##--------------------------------------------------------------------------------
##  refreshTime is called the first time through and whenever the tb frequency
##  changes (should be rare).
##  It updates the local structure with:
##     Xfrac
##	   Xint
##     integer ticks to nanoseconds factor (intmult_nanos)
##     fractional ticks to nanoseconds factor (fracmult_nanos)
##     right shift amount for nanoseconds product (shiftval_nanos)
##  Input:
##     r7 = Xfrac (from system)
##     r8 = Xint  (from system)
##--------------------------------------------------------------------------------
refreshTime_nanos:
       std        r7, Xfrac_nanos(RTOC)     ## save Xfrac in local structure
       std        r8, Xint_nanos(RTOC)      ## save Xint in local structure

     divwu        r5, r8, r7                ## integer part of Xint / Xfrac
       stw        r5, intmult_nanos(RTOC)

     mullw        r5, r7, r5                ## Calculate the remainder of the division
      subf        r8, r5, r8
                                            ## Calculate fraction part of Xint/Xfract
                                            ## Which comes from the remainder above
    cntlzd        r5, r8                    ## leading zeros in quotient (>= 32)
       sld        r8, r8, r5                ## quotient * (2**(r5))
      addi        r6, r7, -1                ## 64 - cntlz((Xfrac-1) & ~Xfrac)
      andc        r6, r6, r7                ## = trailing zeros in
    cntlzd        r6, r6                    ##   Xfrac
    subfic        r6, r6, 64
       srd        r7, r7, r6                ## Xfrac / (2**(r6))
       add        r5, r5, r6                ## sum of shifts applied
     divdu        r4, r8, r7                ## r4 = quotient * 2**(r5)
    cntlzd        r6, r4                    ## leading zeros in quotient
       sld        r9, r4, r6                ## shifted initial quotient
       add        r5, r5, r6                ## total shift in quotient
      addi        r5, r5, -64               ## reduced because mulhdu used
       std        r5, shiftval_nanos(RTOC)  ## save shift in local structure

again_nanos:                                ## loop until 64 bits of quotient
     mulld        r3, r4, r7
      subf        r8, r3, r8                ## r8 = remainder from divdu
    cntlzd        r5, r8                    ## leading zeros in remainder
       sld        r8, r8, r5
     divdu        r4, r8, r7                ## additional bits of quotient
      subf.       r5, r6, r5                ## right shift to apply to r4
       bge        done_nanos
       neg        r6, r5                    ## actually a left shift
       sld        r3, r4, r6                ## align for merge
        or        r9, r3, r9                ## partial merged quotient
         b        again_nanos               ## don't have enough bits yet

done_nanos:
       srd        r3, r4, r5                ## align for merge
        or        r9, r3, r9                ## final merged quotient
       std        r9, fracmult_nanos(RTOC)  ## save multiplier in local structure

         b        retry_nanos
 endproc.__getNanos:

 .align 2
 .globl .__clearTickTock
.__clearTickTock:
 .function .__clearTickTock,startproc.__clearTickTock,16,0,(endproc.__clearTickTock-startproc.__clearTickTock)
 startproc.__clearTickTock:
        li         r3, 0
       std         r3, epochMilli(RTOC)
       std         r3, Xfrac_nanos(RTOC)
       std         r3, Xint_nanos(RTOC)
       blr

 endproc.__clearTickTock:

        .toc
T.baseTicks:      .tc data[tc],_system_TB_config[RW]
T.sysconfig:      .tc data[tc],_system_configuration[RW]
        .csect   TICtock{TC}
##--------------------------------------------------------------------------------
##  The local data is frequently read but rarely written and resides in the TOC
##--------------------------------------------------------------------------------
epochMilli:         .llong  0               ## milliseconds since epoch at boot
multval_millis:     .llong  0               ## timebase to milliseconds factor
shiftval_millis:    .llong  0               ## right shift to apply to product
Xfrac_nanos:		.llong 	0				## Xfrac
Xint_nanos:			.llong 	0				## Xint
fracmult_nanos:     .llong  0               ## fraction part of timebase to nanoseconds factor
intmult_nanos:      .long   0               ## integer part of timebase to nanoseconds factor
shiftval_nanos:     .llong  0               ## right shift to apply to product (nanoseconds)


