###############################################################################
#
# (c) Copyright IBM Corp. 2004, 2011
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

 .file "rt_time.s"
 .global .__getMillis
 .type .__getMillis@function
 .global .__getNanos
 .type .__getNanos@function

 .section        ".opd","aw"
 .globl __getMillis
 .size __getMillis,24
 .globl __getNanos
 .size __getNanos,24

__getMillis:
 .quad .__getMillis
 .quad .TOC.@tocbase
 .long 0x0
 .long 0x0
__getNanos:
 .quad .__getNanos
 .quad .TOC.@tocbase
 .long 0x0
 .long 0x0

# allocate needed data directly in the toc
 .section        ".toc","aw"
 .globl systemcfgP_millis
 .globl systemcfgP_nanos
# pointer to systemcfg information exported by the Linux 64 kernel
systemcfgP_millis:  .llong 0 # set by port library initialization
systemcfgP_nanos:   .llong 0 # set by port library initialization
# local copies of data found in the systemcfg structure
partial_calc_millis:  	.llong 0	# stamp_xsec - tb_orig_stamp * tb_to_xs used for milliseconds
tb_to_xs_millis:  		.llong 0	# time base to 'xsec' multiplier used for milliseconds
tb_update_count_millis: .llong -1 	# systemcfg atomicity counter used for milliseconds
partial_calc_nanos:  	.llong 0	# stamp_xsec - tb_orig_stamp * tb_to_xs used for nanoseconds
tb_to_xs_nanos:   		.llong 0	# time base to 'xsec' multiplier used for nanoseconds
tb_update_count_nanos:  .llong -1 	# systemcfg atomicity counter used for nanoseconds

# offsets to fields in the systemcfg structure (from <asm/systemcfg.h>)
 .set tb_orig_stamp_offset,0x30
 .set tb_to_xs_offset,0x40
 .set stamp_xsec_offset,0x48
 .set tb_update_count_offset,0x50

 .section ".text"
 .align 4
.__getMillis:
 mftb 5    # tb_reg
 ld 6,tb_to_xs_millis(2)  # inverse of TB to 2^20
 ld 3,systemcfgP_millis(2)
 ld 7,partial_calc_millis(2) # get precomputed values
.L_continue_millis:
 mulhdu 4,5,6    # tb_reg * tb_to_xs
 ld 8,tb_update_count_millis(2) # saved update counter
 ld 9,tb_update_count_offset(3) # current update counter
        add 4,4,7    # tb_reg * tb_to_xs + partial_calc
 add 6,4,4    # sequence to convert xsec to millis (see design)
 sldi 5,4,7    # sequence to convert xsec to millis (see design)
 cmpld 0,8,9    # has update counter changed?
 add 6,6,4    # sequence to convert xsec to millis (see design)
 sub 5,5,6    # sequence to convert xsec to millis (see design)
 bne- 0,.L_refresh_millis  # refresh if update counter changed
 srdi 3,5,17    # millis
 blr

 # The code which updates systemcfg variables (ppc_adjtimex in kernel)
 # increments tb_update_count, updates the other variables, and then
 # increments tb_update_count again.  This code reads tb_update_count,
 # reads the other variables and then reads tb_update_count again.  It
 # loops doing this until the two reads of tb_update_count yield the
 # same value and that value is even.  This ensures a consistent view
 # of the systemcfg variables.
 # We use artificial dependencies in the code below to ensure that
 # the loads below execute in order.

.L_refresh_millis:
	# At exit:
	# 	5 contains tb
	# 	6 contains tb_to_xs
	# 	7 contains partial_calc
	# 	8 contains tb_update_count

	ld 10,tb_update_count_offset(3) 	# get the update count
	andc 0,10,10    					# result 0, but dependent on r10
	add 3,3,0    						# base address dependent on r10

	ld 4,tb_orig_stamp_offset(3) 		# timebase at boot
	ld 6,tb_to_xs_offset(3)  			# inverse of TB to 2^20

	andi. 8,10,0x1   					# test if update count is odd

	std 6,tb_to_xs_millis(2)  			# save in local copy

	bne- 0,.L_refresh_millis  			# kernel updating systemcfg

	ld 9,stamp_xsec_offset(3)  		# get the stamp_xsec

	add 0,4,6    						# dependence on r4 and r6
	add 0,0,9    						# dependence on r4, r6 and r9

	mulhdu 7,4,6    					# tb_orig_stamp * tb_to_xs
	sub 7,9,7    						# stamp_xsec - tb_orig_stamp * tb_to_xs

	std 7,partial_calc_millis(2) 		# save in local copy
	andc 0,0,0    						# result 0, but dependent on r4, r6 and r9
	add 3,3,0    						# base address dependent on r4, r6 and r9

	ld 8,tb_update_count_offset(3) # get the update count

	mftb 5    # tb_reg

 # r10 and r8 will be equal if systemcfg structure unchanged
	cmpld 0,10,8
	bne- 0,.L_refresh_millis

	lwsync     # wait for other std to complete
	std 8,tb_update_count_millis(2) # save in local copy

	b .L_continue_millis

#############################################################################################

 .align 4
.__getNanos:
 	mftb 	5    # tb_reg
	ld 		6,tb_to_xs_nanos(2)  	# inverse of TB to 2^20
	ld 		3,systemcfgP_nanos(2)

.L_continue_nanos:
	mulhdu 	4,5,6    				# tb_reg * tb_to_xs
	ld 		8,tb_update_count_nanos(2) # saved update counter
	ld 		9,tb_update_count_offset(3) # current update counter

	sldi    5,4,9    				# See JIT design 1101 for a detailed
	sldi    6,4,7    				# Description of this sequence
	# This is actually a multiply by
   	# 1000000000
	cmpld 	0,8,9    				# has update counter changed?

	sldi    7,4,5
	sldi    8,4,2
	add     6,5,6
	sldi    9,4,6

	bne- 0,.L_refresh_nanos  		# refresh if update counter changed

	sldi    10,4,3
	add     8,8,4
	sldi    0,4,1
	add     6,6,7
	add     9,9,10
	subf    6,8,6
	subf    9,0,9
	addi    6,6,0x7ff   			# round up
	srdi    6,6,11
	subf    9,9,5
	subf    6,6,5
	add     3,9,6
	blr

 # The code which updates systemcfg variables (ppc_adjtimex in kernel)
 # increments tb_update_count, updates the other variables, and then
 # increments tb_update_count again.  This code reads tb_update_count,
 # reads the other variables and then reads tb_update_count again.  It
 # loops doing this until the two reads of tb_update_count yield the
 # same value and that value is even.  This ensures a consistent view
 # of the systemcfg variables.
 # We use artificial dependencies in the code below to ensure that
 # the loads below execute in order.

.L_refresh_nanos:
	# At exit:
		# 5 contains tb
		# 6 contains tb_to_xs
		# 8 contains tb_update_count
	ld 		10,tb_update_count_offset(3)# get the update count
	andc 	0,10,10    					# result 0, but dependent on r10
	add 	3,3,0    					# base address dependent on r10

	ld 		6,tb_to_xs_offset(3)  		# inverse of TB to 2^20

	andi. 	8,10,0x1   					# test if update count is odd

	std 	6,tb_to_xs_nanos(2)  		# save in local copy

	bne- 	0,.L_refresh_nanos  		# kernel updating systemcfg

	add 	0,6,9    					# dependence on r6 and r9

	andc 	0,0,0    					# result 0, but dependent on r6 and r9
	add 	3,3,0    					# base address dependent on r6 and r9

	ld 		8,tb_update_count_offset(3) # get the update count

	mftb 	5    						# tb_reg

	# r10 and r8 will be equal if systemcfg structure unchanged
 	cmpld 	0,10,8
 	bne- 	0,.L_refresh_nanos

	lwsync     							# wait for other std to complete
	std 8,tb_update_count_nanos(2) 		# save in local copy

	b .L_continue_nanos

#############################################################################################

