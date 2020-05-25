###############################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

	.file	"cas32helper.s"
	.option nopic
	.text

# Prototype: uint32_t RiscvCAS32Helper(volatile uint32_t *addr, uint32_t compareValue, uint32_t swapValue);
# Defined in: #Args: 3
	.align	2
	.globl	RiscvCAS32Helper
	.type	RiscvCAS32Helper, @function
RiscvCAS32Helper:
	addi	sp,sp,-32
	sd	s0,24(sp)
	addi	s0,sp,32
	sd	a0,-24(s0)
	mv	a5,a1
	mv	a4,a2
	sw	a5,-28(s0)
	mv	a5,a4
	sw	a5,-32(s0)
	ld	a5,-24(s0)
	lw	a4,-28(s0)
	lw	a3,-32(s0)
	fence iorw,ow
retry:
	lr.w.aq a2,0(a5)
	bne a2,a4,fail
	sc.w.aqrl a1,a3,0(a5)
	bnez a1,retry
fail:
	sext.w	a5,a2
	mv	a0,a5
	ld	s0,24(sp)
	addi	sp,sp,32
	jr	ra
	.size	RiscvCAS32Helper, .-RiscvCAS32Helper
