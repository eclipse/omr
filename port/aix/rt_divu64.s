###############################################################################
#
# (c) Copyright IBM Corp. 2002
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


