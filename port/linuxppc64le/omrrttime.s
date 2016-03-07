###############################################################################
#
# (c) Copyright IBM Corp. 2004, 2013
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
 .global __getMillis
 .type __getMillis@function
 .global __getNanos
 .type __getNanos@function

# allocate needed data directly in the toc

 .globl systemcfgP_millis
 .type systemcfgP_millis,@object
 .globl systemcfgP_nanos
 .type systemcfgP_nanos,@object
# pointer to systemcfg information exported by the Linux 64 kernel
.section        ".data","aw"
systemcfgP_millis:  .llong 0 # set by port library initialization
systemcfgP_nanos:   .llong 0 # set by port library initialization
# local copies of data found in the systemcfg structure
	.align 3
	.type   partial_calc_millis, @object
	.size   partial_calc_millis, 8
partial_calc_millis:
		.llong 0	# stamp_xsec - tb_orig_stamp * tb_to_xs used for milliseconds
	.align 3
	.type   tb_to_xs_millis, @object
	.size   tb_to_xs_millis, 8
tb_to_xs_millis:
		.llong 0	# time base to 'xsec' multiplier used for milliseconds
	.align 3
	.type   tb_update_count_millis, @object
	.size   tb_update_count_millis, 8
tb_update_count_millis:
		.llong -1 	# systemcfg atomicity counter used for milliseconds
	.align 3
	.type   partial_calc_nanos, @object
	.size   partial_calc_nanos, 8
partial_calc_nanos:
		.llong 0	# stamp_xsec - tb_orig_stamp * tb_to_xs used for nanoseconds
	.align 3
	.type   tb_to_xs_nanos, @object
	.size   tb_to_xs_nanos, 8
tb_to_xs_nanos:
		.llong 0	# time base to 'xsec' multiplier used for nanoseconds
	.align 3
	.type   tb_update_count_nanos, @object
	.size   tb_update_count_nanos, 8
tb_update_count_nanos:
		.llong -1 	# systemcfg atomicity counter used for nanoseconds

# offsets to fields in the systemcfg structure (from <asm/systemcfg.h>)
 .set tb_orig_stamp_offset,0x30
 .set tb_to_xs_offset,0x40
 .set stamp_xsec_offset,0x48
 .set tb_update_count_offset,0x50

 .section ".text"
 .align 4
__getMillis:
addis 2,12,.TOC.-__getMillis@ha
addi 2,2,.TOC.-__getMillis@l
.localentry    __getMillis,.-__getMillis

 mftb 5    # tb_reg
 addis 6,2,tb_to_xs_millis@toc@ha  # inverse of TB to 2^20
 addi 6,6,tb_to_xs_millis@toc@l
 ld 6,0(6)
 addis 3,2,systemcfgP_millis@toc@ha
 addi 3,3,systemcfgP_millis@toc@l
 ld 3,0(3)
 addis 7,2,partial_calc_millis@toc@ha # get precomputed values
 addi 7,7,partial_calc_millis@toc@l
 ld 7,0(7)
L_continue_millis:
 mulhdu 4,5,6    # tb_reg * tb_to_xs
 addis 8,2,tb_update_count_millis@toc@ha # saved update counter
 addi 8,8,tb_update_count_millis@toc@l
 ld 8,0(8)
 ld 9,tb_update_count_offset(3) # current update counter
         add 4,4,7    # tb_reg * tb_to_xs + partial_calc
 add 6,4,4    # sequence to convert xsec to millis (see design)
 sldi 5,4,7    # sequence to convert xsec to millis (see design)
 cmpld 0,8,9    # has update counter changed?
 add 6,6,4    # sequence to convert xsec to millis (see design)
 sub 5,5,6    # sequence to convert xsec to millis (see design)
 bne- 0,L_refresh_millis  # refresh if update counter changed
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

L_refresh_millis:
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

	std 14,-8(1)
	addis 14,2,tb_to_xs_millis@toc@ha  # save in local copy
 	addi 14,14,tb_to_xs_millis@toc@l
 	std 6,0(14)
	ld 14,-8(1)

	bne- 0,L_refresh_millis  			# kernel updating systemcfg

	ld 9,stamp_xsec_offset(3)  		# get the stamp_xsec

	add 0,4,6    						# dependence on r4 and r6
	add 0,0,9    						# dependence on r4, r6 and r9

	mulhdu 7,4,6    					# tb_orig_stamp * tb_to_xs
	sub 7,9,7    						# stamp_xsec - tb_orig_stamp * tb_to_xs

	std 14,-8(1)						# save r14 onto stack - Allowed to use 288 bytes below the sp
	addis 14,2,partial_calc_millis@toc@ha  # save in local copy
 	addi 14,14,partial_calc_millis@toc@l
 	std 7,0(14)
	ld 14,-8(1)							# restore r14 from stack

	andc 0,0,0    						# result 0, but dependent on r4, r6 and r9
	add 3,3,0    						# base address dependent on r4, r6 and r9

	ld 8,tb_update_count_offset(3) # get the update count

	mftb 5    # tb_reg

 # r10 and r8 will be equal if systemcfg structure unchanged
	cmpld 0,10,8
	bne- 0,L_refresh_millis

	lwsync     # wait for other std to complete
	std 14,-8(1)						# save r14 onto stack - Allowed to use 288 bytes below the sp
	addis 14,2,tb_update_count_millis@toc@ha  # save in local copy
 	addi 14,14,tb_update_count_millis@toc@l
 	std 8,0(14)
	ld 14,-8(1)							# restore r14 from stack

	b L_continue_millis

#############################################################################################

 .align 4
__getNanos:
addis 2,12,.TOC.-__getNanos@ha
addi 2,2,.TOC.-__getNanos@l
.localentry    __getNanos,.-__getNanos
 	mftb 	5    # tb_reg
 	addis 6,2,tb_to_xs_nanos@toc@ha 
 	addi 6,6,tb_to_xs_nanos@toc@l
 	ld 6,0(6)						# inverse of TB to 2^20
	addis 3,2,systemcfgP_nanos@toc@ha 
 	addi 3,3,systemcfgP_nanos@toc@l
 	ld 3,0(3)						

L_continue_nanos:
	mulhdu 	4,5,6    				# tb_reg * tb_to_xs
	addis 8,2,tb_update_count_nanos@toc@ha 
 	addi 8,8,tb_update_count_nanos@toc@l
 	ld 8,0(8)							# saved update counter
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

	bne- 0,L_refresh_nanos  		# refresh if update counter changed

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

L_refresh_nanos:
	# At exit:
		# 5 contains tb
		# 6 contains tb_to_xs
		# 8 contains tb_update_count
	ld 		10,tb_update_count_offset(3)# get the update count
	andc 	0,10,10    					# result 0, but dependent on r10
	add 	3,3,0    					# base address dependent on r10

	ld 		6,tb_to_xs_offset(3)  		# inverse of TB to 2^20

	andi. 	8,10,0x1   					# test if update count is odd

	std 14,-8(1)						# save r14 onto stack - Allowed to use 288 bytes below the sp
	addis 14,2,tb_to_xs_nanos@toc@ha  	# save in local copy
 	addi 14,14,tb_to_xs_nanos@toc@l
 	std 6,0(14)
	ld 14,-8(1)							# restore r14 from stack

	bne- 	0,L_refresh_nanos  		# kernel updating systemcfg

	add 	0,6,9    					# dependence on r6 and r9

	andc 	0,0,0    					# result 0, but dependent on r6 and r9
	add 	3,3,0    					# base address dependent on r6 and r9

	ld 		8,tb_update_count_offset(3) # get the update count

	mftb 	5    						# tb_reg

	# r10 and r8 will be equal if systemcfg structure unchanged
 	cmpld 	0,10,8
 	bne- 	0,L_refresh_nanos

	lwsync     							# wait for other std to complete

	std 14,-8(1)						# save r14 onto stack - Allowed to use 288 bytes below the sp
	addis 14,2,tb_update_count_nanos@toc@ha  # save in local copy
 	addi 14,14,tb_update_count_nanos@toc@l
 	std 8,0(14)
	ld 14,-8(1)							# restore r14 from stack
	

	b L_continue_nanos

#############################################################################################

