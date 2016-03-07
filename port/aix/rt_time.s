###############################################################################
#
# (c) Copyright IBM Corp. 2002, 2010
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
#   32-bit:  r0, r2-r10, cr0

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
       lwz        r9, T.baseTicks(RTOC)      ## address of ticks since epoch from system
       lwz        r6, multval_millis+4(RTOC) ## ticks to millis factor from local
       lwz        r5, multval_millis(RTOC)
       lwz        r7, epochTicks_millis+4(RTOC)     ## ticks sice epoch from local structure
       lwz        r8, 0x4(r9)                ## ticks since epoch from system
mftb_loop_millis:                            ## get current ticks in r3, r4
     mftbu        r3
      mftb        r4
     mftbu        r0
      cmpw        r0, r3
       bne        mftb_loop_millis
      cmpw        cr0, r7, r8               ## has ticks since epoch changed?? lower 32 bits
      addc        r4, r4, r7                ## TB = tb + ticks since epoch
       lwz        r7, epochTicks_millis(RTOC)      ## ticks sice epoch from local structure
       lwz        r8, 0x0(r9)               ## ticks since epoch from system
      bne-        refreshTime_millis
      cmpw        cr0, r7, r8               ## has ticks since epoch changed?? upper 32 bits
      adde        r3, r3, r8                ## TB = tb + ticks since epoch
    mulhwu        r7, r4, r6                ## calculate upper 64-bits of
      bne-        refreshTime_millis
     mullw        r8, r3, r6                ## TB * multval_millis
     mullw        r9, r4, r5
    mulhwu        r4, r4, r5
      addc        r8, r7, r8
    mulhwu        r6, r3, r6
     mullw        r7, r3, r5
    mulhwu        r3, r3, r5
      adde        r4, r4, r6
     addze        r3, r3
      addc        r8, r8, r9
       lwz        r5, shiftval_millis(RTOC) ## right shift amount from local
      adde        r4, r4, r7
     addze        r3, r3
    subfic        r6, r5, 32
       srw        r4, r4, r5                ## shift product right
      addi        r7, r5, -32
       slw        r6, r3, r6
       srw        r7, r3, r7
       srw        r3, r3, r5
        or        r4, r4, r6
        or        r4, r4, r7
       blr

##--------------------------------------------------------------------------------
##  refreshTime_millis is called the first time through and whenever the tb frequency
##  changes (should be rare).
##  It updates the local structure with:
##     ticks since epoch (epochTicks)
##     ticks to milliseconds factor (multval_millis)
##     right shift amount for milliseconds product (shiftval_millis)
##  Input:
##     r9 = address of ticks since epoch (from system)
##--------------------------------------------------------------------------------
refreshTime_millis:
      subi        SP, SP, 40
      mflr        r0
       stw        r0, 0(SP)
     mfctr        r0
       stw        r0, 4(SP)
       stw        r31, 8(SP)
       stw        r30, 12(SP)
       stw        r29, 16(SP)
       stw        r28, 20(SP)
       stw        r27, 24(SP)
       stw        r26, 28(SP)
       stw        r25, 32(SP)
       stw        r24, 36(SP)

       lwz        r7, 0x0(r9)               ## ticks since epoch from system
       lwz        r8, 0x4(r9)               ## ticks since epoch from system
       lwz        r6, T.sysconfig(RTOC)     ## @sysconfig structure

       lis        r4, 0xf                   ## high 16 bits of 1000000
       stw        r7, epochTicks_millis(RTOC)      ## save ticks since epoch in local structure
      addi        r4, r4, 0x4240            ## r4 = 1000000
       stw        r8, epochTicks_millis+4(RTOC)

       lwz        r30, Xfrac(r6)            ## Xfrac from sysconfig
       lwz        r28, Xint(r6)             ## Xint from sysconfig
     mullw        r31, r30, r4              ## Xfrac * 1000000, lower 32 bits
    mulhwu        r30, r30, r4              ## Xfrac * 1000000, higher 32 bits

    cntlzw        r27, r28                  ## leading zeros in Xint
       slw        r28, r28, r27             ## Xint * (2**(r27))
        li        r29, 0                    ## lower 32 bits of Xint * (2**(r27))
      addi        r27, r27, 32              ## true shift applied to Xint
      addi        r6, r31, -1               ## 32 - cntlz((Xfrac-1) & ~Xfrac)
      andc        r6, r6, r31               ## = trailing zeros in
    cntlzw        r7, r6                    ##   (Xfrac * 1000000)
    subfic        r6, r7, 32                ## (assumes <= 32 trailing zeros
                                            ##  but code works if assumption
                                            ##  isn't correct)
       srw        r31, r31, r6              ## (Xfrac * 1000000) / (2**(r6))
       slw        r7, r30, r7
       srw        r30, r30, r6
        or        r31, r31, r7
       add        r27, r27, r6              ## sum of shifts applied

        mr        r3, r28                   ## r3, r4 = shifted Xint
        mr        r4, r29
        mr        r5, r30                   ## r5, r6 = shifted (Xfrac * 1000000)
        mr        r6, r31
        bl        __divu64                  ## r3, r4 = quotient * 2**(r27)

    cntlzw        r6, r3                    ## leading zeros in quotient
     cmpwi        cr0, r6, 32
       blt        ok1_millis
    cntlzw        r7, r4                    ## more leading zeros in quotient
       add        r6, r6, r7
ok1_millis:
       slw        r25, r3, r6               ## r25, r26 = shifted initial quotient
    subfic        r7, r6, 32
      addi        r8, r6, -32
       srw        r7, r4, r7
       slw        r8, r4, r8
       slw        r26, r4, r6
        or        r25, r25, r7
        or        r25, r25, r8
       add        r27, r27, r6               ## total shift in quotient
      addi        r27, r27, -64              ## reduced because mulhdu used
       stw        r27, shiftval_millis(RTOC) ## save shift in local structure
        mr        r27, r6                    ## r27 = shift applied to quotient

again_millis:                                ## loop until 64 bits of quotient
    mulhwu        r5, r4, r31                ## r5, r6 = (r3, r4) * (r30, r31)
     mullw        r7, r3, r31
     mullw        r8, r4, r30
     mullw        r6, r4, r31
       add        r5, r5, r7
       add        r5, r5, r8

     subfc        r29, r6, r29              ## r28, r29 = remainder from divdu
     subfe        r28, r5, r28
    cntlzw        r24, r28                  ## leading zeros in remainder
     cmpwi        cr0, r24, 32
       blt        ok2_millis
    cntlzw        r7, r29                   ## more leading zeros in remainder
       add        r24, r24, r7
ok2_millis:
       slw        r28, r28, r24             ## r28, r29 = shifted remainder
    subfic        r7, r24, 32
      addi        r8, r24, -32
       srw        r7, r29, r7
       slw        r8, r29, r8
       slw        r29, r29, r24
        or        r28, r28, r7
        or        r28, r28, r8

        mr        r3, r28                   ## r3, r4 = remainder
        mr        r4, r29
        mr        r5, r30                   ## r5, r6 = shifted (Xfrac * 1000000)
        mr        r6, r31
        bl        __divu64                  ## r3, r4 = additional bits of quotient
      subf.       r24, r27, r24             ## right shift to apply to r3, r4
       bge        done_millis
       neg        r27, r24                  ## actually a left shift
       slw        r5, r3, r27               ## align for merge
    subfic        r7, r27, 32
      addi        r8, r27, -32
       srw        r7, r4, r7
       slw        r8, r4, r8
       slw        r6, r4, r27
        or        r5, r5, r7
        or        r5, r5, r8
        or        r25, r5, r25              ## partial merged quotient
        or        r26, r6, r26
         b        again_millis              ## don't have enough bits yet

done_millis:
       srw        r6, r4, r24               ## align for merge
    subfic        r7, r24, 32
      addi        r8, r24, -32
       slw        r7, r3, r7
       srw        r8, r3, r8
       srw        r5, r3, r24
        or        r6, r6, r7
        or        r6, r6, r8
        or        r25, r5, r25              ## final merged quotient
        or        r26, r6, r26
       stw        r25, multval_millis(RTOC) ## save multiplier in local structure
       stw        r26, multval_millis+4(RTOC)

return_millis:
       lwz        r24, 36(SP)
       lwz        r25, 32(SP)
       lwz        r26, 28(SP)
       lwz        r27, 24(SP)
       lwz        r28, 20(SP)
       lwz        r29, 16(SP)
       lwz        r30, 12(SP)
       lwz        r31, 8(SP)
       lwz        r0, 4(SP)
     mtctr        r0
       lwz        r0, 0(SP)
      mtlr        r0
      addi        SP, SP, 40
         b        retry_millis

###########################################################################
#                                                                         #
#   IBM Confidential                                                      #
#                                                                         #
#   OCO Source Materials                                                  #
#                                                                         #
#   VisualAge Micro Edition                                               #
#                                                                         #
#   (C) Copyright IBM Corp. 2002                                          #
#                                                                         #
#   The source code for this program is not published or otherwise        #
#   divested of its trade secrets, irrespective of what has been          #
#   deposited with the U.S. Copyright Office.                             #
#                                                                         #
#  sccsid %I%                                                             #
#                                                                         #
###########################################################################

# From src/bos/usr/ccs/lib/libc/POWER/divu64.s, libccnv, bos520
# long long __divu64( long long, long long )
#
#   (r3:r4) = (r3:r4) / (r5:r6)    (64b) = (64b / 64b)
#     quo       dvd       dvs
#
# Final REMAINDER is computed and returned in r5:r6.
#
# Implementation:
#
# This algorithm, described in detail in the design specification, rotates the
# dividend (dvd) left into a temporary (tmp) one bit per iteration, each
# iteration comparing tmp with the divisor (dvs).  The result of the compare
# (0 if ## tmp<dvs, else 1) is rotated into the quotient (quo), which is built
# physically in the dvd as it is rotated out.  When the dvd is exhausted
# (logically ## after 64 bits), quo occupies dvd, the remainder (rmd) is in tmp.
#
# First, the dvd is preshifted into tmp based on the number of leading zeroes
# in both the dvd and dvs.  This makes the first iteration of the main loop
# always produce a quo '0' bit, but otherwise reduces the number of iterations
# in a highly data-dependent way.  The code is a little convoluted since the
# MSWs and LSWs have to be treated separately in word-sized (32-bit) chunks.
# Thus the count leading zeroes may return 32, i.e. a whole word of zeroes,
# and such situations are handled.  The main loop iteration count is lowered
# to reflect any optimization from preshifting.
#
# When the Count Register reaches zero, the last quo bit is rotated in.
#
# Code comment notation:
#   MSW : Most Significant (High) Word, i.e. bits 0..31
#   LSW : Least Significant (Low) Word, i.e. bits 32..63
#   LZ  : Leading Zeroes
#   SD  : Significant Digits
#
# r3:r4 : dvd  (dividend)     :     quo (quotient)
# r5:r6 : dvs  (divisor)      :     rem (remainder)
#
# r7:r8 : tmp
# r0    : Intermediate subtract LSW
# r9    : Intermediate subtract MSW
# r10   : -1 (always)
# CTR   : cnt

__divu64:

     ## Compute total Dividend Leading 0s
 cntlzw  r0,  r3   ## dvd.msw
 cmpwi cr0,  r0,  32   ## dvd.msw == 0?
 cntlzw  r9,  r4   ## dvd.lsw
 bne cr0, ud1   ## dvd.msw == 0
 add  r0,  r0,  r9   ## all dvd.LZ
ud1:     ## Compute total Divisor Leading 0s
 cntlzw  r9,  r5   ## dvs.msw
 cmpwi cr0,  r9,  32   ## dvd.msw == 0?
 cntlzw r10,  r6   ## dvs.lsw
 bne cr0, ud2   ## dvs.msw == 0
 add  r9,  r9, r10   ## all dvs.LZ
ud2:     ## Is Divisor > Dividend?
 cmpw cr0,  r0,  r9   ## dvd.LZ to dvs.LZ
 subfic r10,  r0,  64   ## dvd.LZ -> dvd.SD
 bgt cr0, ud9   ## dvs > dvd (quo=0)
     ## Compute the number of iterations
 addi  r9,  r9,   1   ## ++dvs.LZ (--dvs.SD)
 subfic  r9,  r9,  64   ## dvs.LZ -> dvs.SD
 add  r0,  r0,  r9   ## dvd shift (L) dvd.LZ+dvs.SD
 subf  r9,  r9, r10   ## tmp shift (R) dvd.SD-dvs.SD
 mtctr  r9    ## Iterations
     ## Preshift Divisor into Temp
      ## r7:r8 = r3:r4 << r9
 cmpwi cr0,  r9,  32   ## 32 or more bits?
 addi  r7,  r9,  -32   ##
 blt cr0, ud3   ## Jumps if not
 srw  r8,  r3,  r7   ## MSW shifted into LSW
 addi  r7, r0, 0   ## final tmp.msw
 b ud4    ##
ud3:      ##
 srw  r8,  r4,  r9   ## Shift LSW
 subfic  r7,  r9,  32   ## Bits to rotate (left)
 slw  r7,  r3,  r7   ## Isolate LSBits of MSW
 or  r8,  r8,  r7   ## Insert MSBits of LSW
 srw  r7,  r3,  r9   ## final tmp.msw
ud4:     ## Preshift Divisor, Preclear Quotient
      ## r3:r4 = r3:r4 << r0
 cmpwi cr0,  r0,  32   ## 32 or more bits?
 addic  r9,  r0,  -32   ##
 blt cr0, ud5   ## Jumps if not
 slw  r3,  r4,  r9   ## LSW shifted into MSW
 addi  r4, r0, 0   ## final dvd.lsw
 b ud6    ##
ud5:      ##
 slw  r3,  r3,  r0   ## Shift MSW
 subfic  r9,  r0,  32   ##
 srw  r9,  r4,  r9   ##
 or  r3,  r3,  r9   ## final dvd.msw
 slw  r4,  r4,  r0   ## final dvd.lsw
ud6:     ## Prepare constants for the main loop
 addi r10, r0, -1   ##
 addic  r7,  r7,   0   ## tmp.lsw (Clear CY)
ud7:     ## Main Loop
 adde  r4,  r4,  r4   ## dvd.lsw (lsb = CY)
 adde  r3,  r3,  r3   ## dvd.msw (lsb from lsw.msb)
 adde  r8,  r8,  r8   ## tmp.lsw (lsb from dvd carry)
 adde  r7,  r7,  r7   ## tmp.msw
      ##
 subfc  r0,  r6,  r8   ##
 subfe.  r9,  r5,  r7   ##
 blt ud8    ## Jmp #CY
 ori  r8,  r0,   0   ## Move lsw
 ori  r7,  r9,   0   ## Move msw
 addic  r0, r10,   1   ## Set CY
ud8:      ##
 bc 16,0,ud7    ## Decrement CTR, branch #0
     ## Rotate in the final Quotient bit
 adde  r4,  r4,  r4   ## quo.lsw (lsb = CY)
 adde  r3,  r3,  r3   ## quo.msw (lsb from lsw)
 ori  r6,  r8,   0   ## rem.lsw
 ori  r5,  r7,   0   ## rem.msw
 bclr 20,0    ## Return
ud9:     ## Quotient is 0 (dvs > dvd)
 ori  r6,  r4,   0   ## rmd.lsw = dvd.lsw
 ori  r5,  r3,   0   ## rmd.msw = dvd.msw
 addi  r4, r0, 0   ## dvd.lsw = 0
 ori  r3,  r4,   0   ## dvd.msw = 0

        blr

 endproc.__getMillis:

# AIX runtime function: __getNanos
# Code called in response to invokestatic java/lang/System/nanoTime
# Input: None
# Output: 64 bit count of elapsed nanoseconds since fixed but arbitrary point in time. Not affected by changes to system clock.
# The following must be done in this order:
#	1. Read the local cache of Xfrac and Xint
#	2. Read TB
#	3. Read the system Xfrac and Xint

# Killed registers:
#   32-bit:  r0, r2-r12, cr0

 .align 2
 .globl .__getNanos
.__getNanos:
 .function .__getNanos,startproc.__getNanos,16,0,(endproc.__getNanos-startproc.__getNanos)
 startproc.__getNanos:

retry_nanos:
       lwz        r4, T.sysconfig(RTOC)        ## address of @sysconfig structure

       lwz        r9, Xfrac_nanos(RTOC)        ## Xfrac from local structure
       lwz        r3, Xint_nanos(RTOC)         ## Xint from local structure

       lwz        r10, intmult_nanos(RTOC)      ## integer part of ticks to nanos factor from local
       lwz        r5,  fracmult_nanos(RTOC)
       lwz        r6,  fracmult_nanos+4(RTOC)   ## fraction part of ticks to nanos factor from local

mftb_loop_nanos:                                ## get current ticks in r11, r12
     mftbu        r11
      mftb        r12
     mftbu        r0
      cmpw        r0, r11
       bne        mftb_loop_nanos

       lwz        r7, Xfrac(r4)              	## Xfrac from system (sysconfig)
       lwz        r8, Xint(r4)               	## Xint from system (sysconfig)
 
      cmpw        cr0, r8, r3                   ## has Xint changed??
      cmpw        cr7, r7, r9                   ## has Xfraq changed??
    mulhwu        r7, r12, r6
     mullw        r8, r11, r6                   ## TB (r11, r12) * fracmult_nanos (r5, r6)
     mullw        r9, r12, r5
    mulhwu        r3, r12, r5
      addc        r8, r7, r8

      bne-        refreshTime_nanos             ## branch if Xfrac changed

    mulhwu        r7, r11, r6                   ## TB (r11, r12) * fracmult_nanos (r5, r6) continued
     mullw        r4, r11, r5
    mulhwu        r6, r11, r5

      adde        r5, r3, r7
     addze        r7, r6
      addc        r6, r8, r9

      bne-        cr7,refreshTime_nanos         ## branch if Xint changed

       lwz        r9, shiftval_nanos(RTOC)      ## right shift amount from local
      adde        r4, r5, r4
     addze        r3, r7
    subfic        r6, r9, 32
       srw        r4, r4, r9                    ## shift product right
      addi        r7, r9, -32
       slw        r6, r3, r6
       srw        r8, r3, r7
       srw        r3, r3, r9

                                                ## Calculate the 64-bit product
     mullw        r5, r11, r10                  ## TB (r11, r12) * intmult_nanos (r10)
    mulhwu        r9, r12, r10
     mullw        r7, r12, r10

        or        r4, r4, r6                    ## moved after intmult calculation for perf
        or        r4, r4, r8                    ## r3, r4 = nanoseconds2

       add        r6, r5, r9                    ## r6, r7 = nanoseconds1
 
      addc        r4, r4, r7                    ## nanoseconds is (r3, r4) + (r6, r7)
      adde        r3, r3, r6

       blr

##  refreshTime_nanos is called the first time through and whenever the tb frequency
##  changes (should be rare).
##  It updates the local structure with:
##     Xfrac
##	   Xint
##     integer ticks to nanoseconds factor (intmult_nanos)
##     fractional ticks to nanoseconds factor (fracmult_nanos)
##     right shift amount for nanoseconds product (shiftval_nanos)
##  Input:
##     r4 = address of @sysconfig structure
##--------------------------------------------------------------------------------
refreshTime_nanos:
      subi        SP, SP, 40
      mflr        r0
       stw        r0, 0(SP)
     mfctr        r0
       stw        r0, 4(SP)
       stw        r31, 8(SP)
       stw        r30, 12(SP)
       stw        r29, 16(SP)
       stw        r28, 20(SP)
       stw        r27, 24(SP)
       stw        r26, 28(SP)
       stw        r25, 32(SP)
       stw        r24, 36(SP)

        li        r30, 0                        ## Higher 32 bits of Xfrac
       lwz        r31, Xfrac(r4)              	## Xfrac from system (sysconfig)
        li        r28, 0                        ## Higher 32 bits of Xint
       lwz        r29, Xint(r4)               	## Xint from system (sysconfig)
        
       stw        r31, Xfrac_nanos(RTOC)        ## save Xfrac in local structure
       stw        r29, Xint_nanos(RTOC)         ## save Xint in local structure        

        mr        r3, r28                       ## r3, r4 = Xint
        mr        r4, r29
        mr        r5, r30                       ## r5, r6 = Xfrac
        mr        r6, r31

        bl        __divu64                      ## r3, r4 = quotient
                                                ## r5, r6 = remainder
       stw        r4, intmult_nanos(RTOC)       ## save integer multiplier in local structure

        mr        r28, r6                       ## r5 should be zero

                                                ## Calculate fraction part of Xint/Xfract
                                                ## Which comes from the remainder above
    cntlzw        r27, r28                      ## leading zeros in quotient
       slw        r28, r28, r27                 ## quotient * (2**(r27))
        li        r29, 0                        ## lower 32 bits of quotient * (2**(r27))
      addi        r27, r27, 32                  ## true shift applied to quotient
      addi        r6, r31, -1                   ## 32 - cntlz((Xfrac-1) & ~Xfrac)
      andc        r6, r6, r31                   ## = trailing zeros in
    cntlzw        r7, r6                        ##   Xfrac
    subfic        r6, r7, 32                    ## (assumes <= 32 trailing zeros
                                                ##  but code works if assumption
                                                ##  isn't correct)
       srw        r31, r31, r6                  ## (Xfrac) / (2**(r6))
       add        r27, r27, r6                  ## sum of shifts applied

        mr        r3, r28                       ## r3, r4 = shifted quotient
        mr        r4, r29
        mr        r5, r30                       ## r5, r6 = shifted Xfrac
        mr        r6, r31
        bl        __divu64                      ## r3, r4 = quotient  * 2**(r27)
                                                ## r5, r6 = remainder * 2**(r27)

    cntlzw        r6, r3                        ## leading zeros in quotient
     cmpwi        cr0, r6, 32
       blt        ok1_nanos
    cntlzw        r7, r4                        ## more leading zeros in quotient
       add        r6, r6, r7
ok1_nanos:
       slw        r25, r3, r6                   ## r25, r26 = shifted initial quotient
    subfic        r7, r6, 32
      addi        r8, r6, -32
       srw        r7, r4, r7
       slw        r8, r4, r8
       slw        r26, r4, r6
        or        r25, r25, r7
        or        r25, r25, r8
       add        r27, r27, r6                  ## total shift in quotient
      addi        r27, r27, -64                 ## reduced because mulhdu used
       stw        r27, shiftval_nanos(RTOC)     ## save shift in local structure
        mr        r27, r6                       ## r27 = shift applied to quotient

again_nanos:                                    ## loop until 64 bits of quotient
    mulhwu        r5, r4, r31                   ## r5, r6 = (r3, r4) * (r30, r31)
     mullw        r7, r3, r31
     mullw        r8, r4, r30
     mullw        r6, r4, r31
       add        r5, r5, r7
       add        r5, r5, r8

     subfc        r29, r6, r29                  ## r28, r29 = remainder from divdu
     subfe        r28, r5, r28
    cntlzw        r24, r28                      ## leading zeros in remainder
     cmpwi        cr0, r24, 32
       blt        ok2_nanos
    cntlzw        r7, r29                       ## more leading zeros in remainder
       add        r24, r24, r7
ok2_nanos:
       slw        r28, r28, r24                 ## r28, r29 = shifted remainder
    subfic        r7, r24, 32
      addi        r8, r24, -32
       srw        r7, r29, r7
       slw        r8, r29, r8
       slw        r29, r29, r24
        or        r28, r28, r7
        or        r28, r28, r8

        mr        r3, r28                       ## r3, r4 = remainder
        mr        r4, r29
        mr        r5, r30                       ## r5, r6 = shifted Xfrac
        mr        r6, r31
        bl        __divu64                      ## r3, r4 = additional bits of quotient
      subf.       r24, r27, r24                 ## right shift to apply to r3, r4
       bge        done_nanos
       neg        r27, r24                      ## actually a left shift
       slw        r5, r3, r27                   ## align for merge
    subfic        r7, r27, 32
      addi        r8, r27, -32
       srw        r7, r4, r7
       slw        r8, r4, r8
       slw        r6, r4, r27
        or        r5, r5, r7
        or        r5, r5, r8
        or        r25, r5, r25                 ## partial merged quotient
        or        r26, r6, r26
         b        again_nanos                  ## don't have enough bits yet

done_nanos:
       srw        r6, r4, r24                  ## align for merge
    subfic        r7, r24, 32
      addi        r8, r24, -32
       slw        r7, r3, r7
       srw        r8, r3, r8
       srw        r5, r3, r24
        or        r6, r6, r7
        or        r6, r6, r8
        or        r25, r5, r25                 ## final merged quotient
        or        r26, r6, r26
       stw        r25, fracmult_nanos(RTOC)    ## save multiplier in local structure
       stw        r26, fracmult_nanos+4(RTOC)

return_nanos:
       lwz        r24, 36(SP)
       lwz        r25, 32(SP)
       lwz        r26, 28(SP)
       lwz        r27, 24(SP)
       lwz        r28, 20(SP)
       lwz        r29, 16(SP)
       lwz        r30, 12(SP)
       lwz        r31, 8(SP)
       lwz        r0, 4(SP)
     mtctr        r0
       lwz        r0, 0(SP)
      mtlr        r0
      addi        SP, SP, 40
         b        retry_nanos

 endproc.__getNanos:
 
 .align 2
 .globl .__clearTickTock
.__clearTickTock:
 .function .__clearTickTock,startproc.__clearTickTock,16,0,(endproc.__clearTickTock-startproc.__clearTickTock)
 startproc.__clearTickTock:
        li        r3, 0
       stw        r3, epochTicks_millis(RTOC)
       stw        r3, epochTicks_millis+4(RTOC)
       stw        r3, Xfrac_nanos(RTOC)
       stw        r3, Xint_nanos(RTOC)
       blr

 endproc.__clearTickTock:

        .toc
T.baseTicks:      .tc data[tc],_system_TB_config[RW]
T.sysconfig:      .tc data[tc],_system_configuration[RW]
        .csect   TICtock{TC}
##--------------------------------------------------------------------------------
##  The local data is frequently read but rarely written and resides in the TOC
##--------------------------------------------------------------------------------
epochTicks_millis: .llong  0                   ## ticks since epoch at boot for updating millis structures
multval_millis:    .llong  0                   ## timebase to milliseconds factor
shiftval_millis:   .long   0                   ## right shift to apply to product (millisencods)
Xfrac_nanos:       .long   0                   ## Xfrac
Xint_nanos:        .long   0                   ## Xint
fracmult_nanos:    .llong  0                   ## fraction part of timebase to nanoseconds factor
intmult_nanos:     .long   0                   ## integer part of timebase to nanoseconds factor
shiftval_nanos:    .long   0                   ## right shift to apply to product (nanoseconds)

