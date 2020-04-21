/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
/*
 * This table is #included in a static table.
 * Only Function Pointers are allowed.
 */

    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::badILOpEvaluator ,	// TR::badILOp    // illegal op hopefully help with uninitialized nodes
    TR::TreeEvaluator::aconstEvaluator, // TR::aconst		// load address constant (zero value means NULL)
    TR::TreeEvaluator::iconstEvaluator, // TR::iconst		// load integer constant (32-bit signed 2's complement)
    TR::TreeEvaluator::lconstEvaluator, // TR::lconst		// load long integer constant (64-bit signed 2's complement)
    TR::TreeEvaluator::fconstEvaluator, // TR::fconst		// load float constant (32-bit ieee fp)
    TR::TreeEvaluator::dconstEvaluator, // TR::dconst		// load double constant (64-bit ieee fp)
    TR::TreeEvaluator::bconstEvaluator, // TR::bconst		// load byte integer constant (8-bit signed 2's complement)
    TR::TreeEvaluator::sconstEvaluator, // TR::sconst		// load short integer constant (16-bit signed 2's complement)
    TR::TreeEvaluator::iloadEvaluator, // TR::iload		// load integer
    TR::TreeEvaluator::floadEvaluator, // TR::fload		// load float
    TR::TreeEvaluator::dloadEvaluator, // TR::dload		// load double
    TR::TreeEvaluator::aloadEvaluator, // TR::aload		// load address
    TR::TreeEvaluator::bloadEvaluator, // TR::bload		// load byte
    TR::TreeEvaluator::sloadEvaluator, // TR::sload		// load short integer
    TR::TreeEvaluator::lloadEvaluator, // TR::lload		// load long integer
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::irdbarEvaluator ,	// TR::irdbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::frdbarEvaluator ,	// TR::frdbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::drdbarEvaluator ,	// TR::drdbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ardbarEvaluator ,	// TR::ardbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::brdbarEvaluator ,	// TR::brdbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::srdbarEvaluator ,	// TR::srdbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lrdbarEvaluator ,	// TR::lrdbar
    TR::TreeEvaluator::iloadEvaluator, // TR::iloadi		// load indirect integer
    TR::TreeEvaluator::floadEvaluator, // TR::floadi		// load indirect float
    TR::TreeEvaluator::dloadEvaluator, // TR::dloadi		// load indirect double
    TR::TreeEvaluator::aloadEvaluator, // TR::aloadi		// load indirect address
    TR::TreeEvaluator::bloadEvaluator, // TR::bloadi		// load indirect byte
    TR::TreeEvaluator::sloadEvaluator, // TR::sloadi		// load indirect short integer
    TR::TreeEvaluator::lloadEvaluator, // TR::lloadi		// load indirect long integer
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::irdbariEvaluator ,	// TR::irdbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::frdbariEvaluator ,	// TR::frdbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::drdbariEvaluator ,	// TR::drdbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ardbariEvaluator ,	// TR::ardbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::brdbariEvaluator ,	// TR::brdbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::srdbariEvaluator ,	// TR::srdbari
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lrdbariEvaluator ,	// TR::lrdbari
    TR::TreeEvaluator::istoreEvaluator, // TR::istore		// store integer
    TR::TreeEvaluator::lstoreEvaluator, // TR::lstore		// store long integer
    TR::TreeEvaluator::fstoreEvaluator, // TR::fstore		// store float
    TR::TreeEvaluator::dstoreEvaluator, // TR::dstore		// store double
    TR::TreeEvaluator::lstoreEvaluator, // TR::astore		// store address
    TR::TreeEvaluator::bstoreEvaluator, // TR::bstore		// store byte
    TR::TreeEvaluator::sstoreEvaluator, // TR::sstore		// store short integer
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iwrtbarEvaluator , //TR::iwrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lwrtbarEvaluator , //TR::lwrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fwrtbarEvaluator , //TR::fwrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dwrtbarEvaluator , //TR::dwrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::awrtbarEvaluator , //TR::awrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bwrtbarEvaluator , //TR::bwrtbar
    TR::TreeEvaluator::unImpOpEvaluator,         // TODO:RV: Enable when Implemented: TR::TreeEvaluator::swrtbarEvaluator , //TR::swrtbar
    TR::TreeEvaluator::lstoreEvaluator, // TR::lstorei		// store indirect long integer           (child1 a; child2 l)
    TR::TreeEvaluator::fstoreEvaluator, // TR::fstorei		// store indirect float                  (child1 a; child2 f)
    TR::TreeEvaluator::dstoreEvaluator, // TR::dstorei		// store indirect double                 (child1 a; child2 d)
    TR::TreeEvaluator::lstoreEvaluator, // TR::astorei		// store indirect address                (child1 a dest; child2 a value)
    TR::TreeEvaluator::bstoreEvaluator, // TR::bstorei		// store indirect byte                   (child1 a; child2 b)
    TR::TreeEvaluator::sstoreEvaluator, // TR::sstorei		// store indirect short integer          (child1 a; child2 s)
    TR::TreeEvaluator::istoreEvaluator, // TR::istorei		// store indirect integer                (child1 a; child2 i)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lwrtbariEvaluator , // TR::lwrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fwrtbariEvaluator , // TR::fwrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dwrtbariEvaluator , // TR::dwrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::awrtbariEvaluator , // TR::awrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bwrtbariEvaluator , // TR::bwrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::swrtbariEvaluator , // TR::swrtbari
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iwrtbariEvaluator , // TR::iwrtbari

    TR::TreeEvaluator::gotoEvaluator, // TR::goto		// goto label address
    TR::TreeEvaluator::ireturnEvaluator, // TR::ireturn		// return an integer
    TR::TreeEvaluator::lreturnEvaluator, // TR::lreturn		// return a long integer
    TR::TreeEvaluator::freturnEvaluator, // TR::freturn		// return a float
    TR::TreeEvaluator::dreturnEvaluator, // TR::dreturn		// return a double
    TR::TreeEvaluator::lreturnEvaluator, // TR::areturn		// return an address
    TR::TreeEvaluator::returnEvaluator, // TR::return		// void return
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::asynccheckEvaluator ,	// TR::asynccheck	// GC point
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::athrowEvaluator ,	// TR::athrow		// throw an exception
    TR::TreeEvaluator::directCallEvaluator, // TR::icall		// direct call returning integer
    TR::TreeEvaluator::directCallEvaluator, // TR::lcall		// direct call returning long integer
    TR::TreeEvaluator::directCallEvaluator, // TR::fcall		// direct call returning float
    TR::TreeEvaluator::directCallEvaluator, // TR::dcall		// direct call returning double
    TR::TreeEvaluator::directCallEvaluator, // TR::acall		// direct call returning reference
    TR::TreeEvaluator::directCallEvaluator, // TR::call		// direct call returning void
    TR::TreeEvaluator::iaddEvaluator, // TR::iadd		// add 2 integers
    TR::TreeEvaluator::laddEvaluator ,	// TR::ladd		// add 2 long integers
    TR::TreeEvaluator::faddEvaluator, // TR::fadd		// add 2 floats
    TR::TreeEvaluator::daddEvaluator, // TR::dadd		// add 2 doubles
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::baddEvaluator ,	// TR::badd		// add 2 bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::saddEvaluator ,	// TR::sadd		// add 2 short integers
    TR::TreeEvaluator::isubEvaluator, // TR::isub		// subtract 2 integers                (child1 - child2)
    TR::TreeEvaluator::lsubEvaluator ,	// TR::lsub		// subtract 2 long integers           (child1 - child2)
    TR::TreeEvaluator::fsubEvaluator, // TR::fsub		// subtract 2 floats                  (child1 - child2)
    TR::TreeEvaluator::dsubEvaluator, // TR::dsub		// subtract 2 doubles                 (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bsubEvaluator ,	// TR::bsub		// subtract 2 bytes                   (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ssubEvaluator ,	// TR::ssub		// subtract 2 short integers          (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::asubEvaluator ,	// TR::asub		// subtract 2 addresses (child1 - child2)
    TR::TreeEvaluator::imulEvaluator, // TR::imul		// multiply 2 integers
    TR::TreeEvaluator::imulEvaluator ,	// TR::lmul		// multiply 2 signed or unsigned long integers
    TR::TreeEvaluator::fmulEvaluator, // TR::fmul		// multiply 2 floats
    TR::TreeEvaluator::dmulEvaluator, // TR::dmul		// multiply 2 doubles
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bmulEvaluator ,	// TR::bmul		// multiply 2 bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::smulEvaluator ,	// TR::smul		// multiply 2 short integers
    TR::TreeEvaluator::idivEvaluator, // TR::idiv		// divide 2 integers                (child1 / child2)
    TR::TreeEvaluator::ldivEvaluator, // TR::ldiv		// divide 2 long integers           (child1 / child2)
    TR::TreeEvaluator::fdivEvaluator, // TR::fdiv		// divide 2 floats                  (child1 / child2)
    TR::TreeEvaluator::ddivEvaluator, // TR::ddiv		// divide 2 doubles                 (child1 / child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bdivEvaluator ,	// TR::bdiv		// divide 2 bytes                   (child1 / child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sdivEvaluator ,	// TR::sdiv		// divide 2 short integers          (child1 / child2)
    TR::TreeEvaluator::iudivEvaluator ,	// TR::iudiv		// divide 2 unsigned integers       (child1 / child2)
    TR::TreeEvaluator::ludivEvaluator ,	// TR::ludiv		// divide 2 unsigned long integers  (child1 / child2)
    TR::TreeEvaluator::iremEvaluator, // TR::irem		// remainder of 2 integers                (child1 % child2)
    TR::TreeEvaluator::lremEvaluator, // TR::lrem		// remainder of 2 long integers           (child1 % child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fremEvaluator ,	// TR::frem		// remainder of 2 floats                  (child1 % child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dremEvaluator ,	// TR::drem		// remainder of 2 doubles                 (child1 % child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bremEvaluator ,	// TR::brem		// remainder of 2 bytes                   (child1 % child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sremEvaluator ,	// TR::srem		// remainder of 2 short integers          (child1 % child2)
    TR::TreeEvaluator::iuremEvaluator ,	// TR::iurem		// remainder of 2 unsigned integers       (child1 % child2)
    TR::TreeEvaluator::inegEvaluator, // TR::ineg		// negate an integer
    TR::TreeEvaluator::inegEvaluator, // TR::lneg		// negate a long integer
    TR::TreeEvaluator::fnegEvaluator, // TR::fneg		// negate a float
    TR::TreeEvaluator::dnegEvaluator, // TR::dneg		// negate a double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bnegEvaluator ,	// TR::bneg		// negate a bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::snegEvaluator ,	// TR::sneg		// negate a short integer
    TR::TreeEvaluator::iabsEvaluator, // TR::iabs		// absolute value of integer
    TR::TreeEvaluator::labsEvaluator, // TR::labs		// absolute value of long
    TR::TreeEvaluator::fabsEvaluator, // TR::fabs		// absolute value of float
    TR::TreeEvaluator::dabsEvaluator, // TR::dabs		// absolute value of double
    TR::TreeEvaluator::ishlEvaluator, // TR::ishl		// shift integer left                (child1 << child2)
    TR::TreeEvaluator::lshlEvaluator, // TR::lshl		// shift long integer left           (child1 << child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bshlEvaluator ,	// TR::bshl		// shift byte left                   (child1 << child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sshlEvaluator ,	// TR::sshl		// shift short integer left          (child1 << child2)
    TR::TreeEvaluator::ishrEvaluator, // TR::ishr		// shift integer right arithmetically               (child1 >> child2)
    TR::TreeEvaluator::lshrEvaluator, // TR::lshr		// shift long integer right arithmetically          (child1 >> child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bshrEvaluator ,	// TR::bshr		// shift byte right arithmetically                  (child1 >> child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sshrEvaluator ,	// TR::sshr		// shift short integer arithmetically               (child1 >> child2)
    TR::TreeEvaluator::iushrEvaluator, // TR::iushr		// shift integer right logically                   (child1 >> child2)
    TR::TreeEvaluator::lushrEvaluator, // TR::lushr		// shift long integer right logically              (child1 >> child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bushrEvaluator ,	// TR::bushr		// shift byte right logically                      (child1 >> child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sushrEvaluator ,	// TR::sushr		// shift short integer right logically             (child1 >> child2)
    TR::TreeEvaluator::irolEvaluator, // TR::irol		// rotate integer left
    TR::TreeEvaluator::irolEvaluator, // TR::lrol		// rotate long integer left
    TR::TreeEvaluator::iandEvaluator ,	// TR::iand		// boolean and of 2 integers
    TR::TreeEvaluator::landEvaluator ,	// TR::land		// boolean and of 2 long integers
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bandEvaluator ,	// TR::band		// boolean and of 2 bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sandEvaluator ,	// TR::sand		// boolean and of 2 short integers
    TR::TreeEvaluator::iorEvaluator ,	// TR::ior		// boolean or of 2 integers
    TR::TreeEvaluator::lorEvaluator ,	// TR::lor		// boolean or of 2 long integers
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::borEvaluator ,	// TR::bor		// boolean or of 2 bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sorEvaluator ,	// TR::sor		// boolean or of 2 short integers
    TR::TreeEvaluator::ixorEvaluator ,	// TR::ixor		// boolean xor of 2 integers
    TR::TreeEvaluator::lxorEvaluator ,	// TR::lxor		// boolean xor of 2 long integers
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bxorEvaluator ,	// TR::bxor		// boolean xor of 2 bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sxorEvaluator ,	// TR::sxor		// boolean xor of 2 short integers
    TR::TreeEvaluator::i2lEvaluator, // TR::i2l		// convert integer to long integer with sign extension
    TR::TreeEvaluator::i2fEvaluator ,	// TR::i2f		// convert integer to float
    TR::TreeEvaluator::i2dEvaluator ,	// TR::i2d		// convert integer to double
    TR::TreeEvaluator::l2iEvaluator, // TR::i2b		// convert integer to byte
    TR::TreeEvaluator::l2iEvaluator, // TR::i2s		// convert integer to short integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::i2aEvaluator ,	// TR::i2a		// convert integer to address
    TR::TreeEvaluator::iu2lEvaluator, // TR::iu2l		// convert unsigned integer to long integer with zero extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iu2fEvaluator ,	// TR::iu2f		// convert unsigned integer to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iu2dEvaluator ,	// TR::iu2d		// convert unsigned integer to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iu2aEvaluator ,	// TR::iu2a		// convert unsigned integer to address
    TR::TreeEvaluator::l2iEvaluator, // TR::l2i		// convert long integer to integer
    TR::TreeEvaluator::l2fEvaluator ,	// TR::l2f		// convert long integer to float
    TR::TreeEvaluator::l2dEvaluator ,	// TR::l2d		// convert long integer to double
    TR::TreeEvaluator::l2iEvaluator, // TR::l2b		// convert long integer to byte
    TR::TreeEvaluator::l2iEvaluator, // TR::l2s		// convert long integer to short integer
    TR::TreeEvaluator::passThroughEvaluator, // TR::l2a		// convert long integer to address
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lu2fEvaluator ,	// TR::lu2f		// convert unsigned long integer to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lu2dEvaluator ,	// TR::lu2d		// convert unsigned long integer to double
    TR::TreeEvaluator::passThroughEvaluator, // TR::lu2a		// convert unsigned long integer to address
    TR::TreeEvaluator::f2iEvaluator ,	// TR::f2i		// convert float to integer
    TR::TreeEvaluator::f2lEvaluator ,	// TR::f2l		// convert float to long integer
    TR::TreeEvaluator::f2dEvaluator ,	// TR::f2d		// convert float to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2bEvaluator ,	// TR::f2b		// convert float to byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2sEvaluator ,	// TR::f2s		// convert float to short integer
    TR::TreeEvaluator::d2iEvaluator ,	// TR::d2i		// convert double to integer
    TR::TreeEvaluator::d2lEvaluator ,	// TR::d2l		// convert double to long integer
    TR::TreeEvaluator::d2fEvaluator ,	// TR::d2f		// convert double to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2bEvaluator ,	// TR::d2b		// convert double to byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2sEvaluator ,	// TR::d2s		// convert double to short integer
    TR::TreeEvaluator::b2iEvaluator, // TR::b2i		// convert byte to integer with sign extension
    TR::TreeEvaluator::b2lEvaluator, // TR::b2l		// convert byte to long integer with sign extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::b2fEvaluator ,	// TR::b2f		// convert byte to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::b2dEvaluator ,	// TR::b2d		// convert byte to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::b2sEvaluator ,	// TR::b2s		// convert byte to short integer with sign extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::b2aEvaluator ,	// TR::b2a		// convert byte to address
    TR::TreeEvaluator::bu2iEvaluator, // TR::bu2i		// convert byte to integer with zero extension
    TR::TreeEvaluator::bu2lEvaluator, // TR::bu2l		// convert byte to long integer with zero extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bu2fEvaluator ,	// TR::bu2f		// convert unsigned byte to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bu2dEvaluator ,	// TR::bu2d		// convert unsigned byte to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bu2sEvaluator ,	// TR::bu2s		// convert byte to short integer with zero extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bu2aEvaluator ,	// TR::bu2a		// convert unsigned byte to unsigned address
    TR::TreeEvaluator::s2iEvaluator, // TR::s2i		// convert short integer to integer with sign extension
    TR::TreeEvaluator::s2lEvaluator, // TR::s2l		// convert short integer to long integer with sign extension
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::s2fEvaluator ,	// TR::s2f		// convert short integer to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::s2dEvaluator ,	// TR::s2d		// convert short integer to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::s2bEvaluator ,	// TR::s2b		// convert short integer to byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::s2aEvaluator ,	// TR::s2a		// convert short integer to address
    TR::TreeEvaluator::su2iEvaluator, // TR::su2i		// zero extend short to int
    TR::TreeEvaluator::su2lEvaluator, // TR::su2l		// zero extend char to long
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::su2fEvaluator ,	// TR::su2f		// convert char to float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::su2dEvaluator ,	// TR::su2d		// convert char to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::su2aEvaluator ,	// TR::su2a		// convert char to address
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::a2iEvaluator ,	// TR::a2i		// convert address to integer
    TR::TreeEvaluator::passThroughEvaluator, // TR::a2l		// convert address to long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::a2bEvaluator ,	// TR::a2b		// convert address to byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::a2sEvaluator ,	// TR::a2s		// convert address to short
    TR::TreeEvaluator::icmpeqEvaluator, // TR::icmpeq		// integer compare if equal
    TR::TreeEvaluator::icmpneEvaluator, // TR::icmpne		// integer compare if not equal
    TR::TreeEvaluator::icmpltEvaluator, // TR::icmplt		// integer compare if less than
    TR::TreeEvaluator::icmpgeEvaluator, // TR::icmpge		// integer compare if greater than or equal
    TR::TreeEvaluator::icmpgtEvaluator, // TR::icmpgt		// integer compare if greater than
    TR::TreeEvaluator::icmpleEvaluator, // TR::icmple		// integer compare if less than or equal
    TR::TreeEvaluator::icmpeqEvaluator, // TR::iucmpeq		// unsigned integer compare if equal
    TR::TreeEvaluator::icmpneEvaluator, // TR::iucmpne		// unsigned integer compare if not equal
    TR::TreeEvaluator::iucmpltEvaluator, // TR::iucmplt		// unsigned integer compare if less than
    TR::TreeEvaluator::iucmpgeEvaluator, // TR::iucmpge		// unsigned integer compare if greater than or equal
    TR::TreeEvaluator::iucmpgtEvaluator, // TR::iucmpgt		// unsigned integer compare if greater than
    TR::TreeEvaluator::iucmpleEvaluator, // TR::iucmple		// unsigned integer compare if less than or equal
    TR::TreeEvaluator::lcmpeqEvaluator, // TR::lcmpeq		// long compare if equal
    TR::TreeEvaluator::lcmpneEvaluator, // TR::lcmpne		// long compare if not equal
    TR::TreeEvaluator::lcmpltEvaluator, // TR::lcmplt		// long compare if less than
    TR::TreeEvaluator::lcmpgeEvaluator, // TR::lcmpge		// long compare if greater than or equal
    TR::TreeEvaluator::lcmpgtEvaluator, // TR::lcmpgt		// long compare if greater than
    TR::TreeEvaluator::lcmpleEvaluator, // TR::lcmple		// long compare if less than or equal
    TR::TreeEvaluator::lcmpeqEvaluator, // TR::lucmpeq		// unsigned long compare if equal
    TR::TreeEvaluator::lcmpneEvaluator, // TR::lucmpne		// unsigned long compare if not equal
    TR::TreeEvaluator::lucmpltEvaluator, // TR::lucmplt		// unsigned long compare if less than
    TR::TreeEvaluator::lucmpgeEvaluator, // TR::lucmpge		// unsigned long compare if greater than or equal
    TR::TreeEvaluator::lucmpgtEvaluator, // TR::lucmpgt		// unsigned long compare if greater than
    TR::TreeEvaluator::lucmpleEvaluator, // TR::lucmple		// unsigned long compare if less than or equal
    TR::TreeEvaluator::fcmpeqEvaluator ,	// TR::fcmpeq		// float compare if equal
    TR::TreeEvaluator::fcmpneEvaluator ,	// TR::fcmpne		// float compare if not equal
    TR::TreeEvaluator::fcmpltEvaluator ,	// TR::fcmplt		// float compare if less than
    TR::TreeEvaluator::fcmpgeEvaluator ,	// TR::fcmpge		// float compare if greater than or equal
    TR::TreeEvaluator::fcmpgtEvaluator ,	// TR::fcmpgt		// float compare if greater than
    TR::TreeEvaluator::fcmpleEvaluator ,	// TR::fcmple		// float compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpequEvaluator ,	// TR::fcmpequ		// float compare if equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpneuEvaluator ,	// TR::fcmpneu		// float compare if not equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpltuEvaluator ,	// TR::fcmpltu		// float compare if less than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpgeuEvaluator ,	// TR::fcmpgeu		// float compare if greater than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpgtuEvaluator ,	// TR::fcmpgtu		// float compare if greater than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpleuEvaluator ,	// TR::fcmpleu		// float compare if less than or equal or unordered
    TR::TreeEvaluator::dcmpeqEvaluator ,	// TR::dcmpeq		// double compare if equal
    TR::TreeEvaluator::dcmpneEvaluator ,	// TR::dcmpne		// double compare if not equal
    TR::TreeEvaluator::dcmpltEvaluator ,	// TR::dcmplt		// double compare if less than
    TR::TreeEvaluator::dcmpgeEvaluator ,	// TR::dcmpge		// double compare if greater than or equal
    TR::TreeEvaluator::dcmpgtEvaluator ,	// TR::dcmpgt		// double compare if greater than
    TR::TreeEvaluator::dcmpleEvaluator ,	// TR::dcmple		// double compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpequEvaluator ,	// TR::dcmpequ		// double compare if equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpneuEvaluator ,	// TR::dcmpneu		// double compare if not equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpltuEvaluator ,	// TR::dcmpltu		// double compare if less than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpgeuEvaluator ,	// TR::dcmpgeu		// double compare if greater than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpgtuEvaluator ,	// TR::dcmpgtu		// double compare if greater than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpleuEvaluator ,	// TR::dcmpleu		// double compare if less than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpeqEvaluator ,	// TR::acmpeq		// address compare if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpneEvaluator ,	// TR::acmpne		// address compare if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpltEvaluator ,	// TR::acmplt		// address compare if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpgeEvaluator ,	// TR::acmpge		// address compare if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpgtEvaluator ,	// TR::acmpgt		// address compare if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acmpleEvaluator ,	// TR::acmple		// address compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpeqEvaluator ,	// TR::bcmpeq		// byte compare if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpneEvaluator ,	// TR::bcmpne		// byte compare if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpltEvaluator ,	// TR::bcmplt		// byte compare if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpgeEvaluator ,	// TR::bcmpge		// byte compare if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpgtEvaluator ,	// TR::bcmpgt		// byte compare if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpleEvaluator ,	// TR::bcmple		// byte compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpeqEvaluator ,	// TR::bucmpeq		// unsigned byte compare if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpneEvaluator ,	// TR::bucmpne		// unsigned byte compare if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpltEvaluator ,	// TR::bucmplt		// unsigned byte compare if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpgeEvaluator ,	// TR::bucmpge		// unsigned byte compare if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpgtEvaluator ,	// TR::bucmpgt		// unsigned byte compare if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpleEvaluator ,	// TR::bucmple		// unsigned byte compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpeqEvaluator ,	// TR::scmpeq		// short integer compare if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpneEvaluator ,	// TR::scmpne		// short integer compare if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpltEvaluator ,	// TR::scmplt		// short integer compare if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpgeEvaluator ,	// TR::scmpge		// short integer compare if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpgtEvaluator ,	// TR::scmpgt		// short integer compare if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpleEvaluator ,	// TR::scmple		// short integer compare if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpeqEvaluator ,	// TR::sucmpeq		// char compare if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpneEvaluator ,	// TR::sucmpne		// char compare if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpltEvaluator ,	// TR::sucmplt		// char compare if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpgeEvaluator ,	// TR::sucmpge		// char compare if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpgtEvaluator ,	// TR::sucmpgt		// char compare if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpleEvaluator ,	// TR::sucmple		// char compare if less than or equal
    TR::TreeEvaluator::lcmpEvaluator, // TR::lcmp		// long compare (1 if child1 > child2; 0 if child1 == child2; -1 if child1 < child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmplEvaluator ,	// TR::fcmpl		// float compare l (1 if child1 > child2; 0 if child1 == child2; -1 if child1 < child2 or unordered)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcmpgEvaluator ,	// TR::fcmpg		// float compare g (1 if child1 > child2 or unordered; 0 if child1 == child2; -1 if child1 < child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmplEvaluator ,	// TR::dcmpl		// double compare l (1 if child1 > child2; 0 if child1 == child2; -1 if child1 < child2 or unordered)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcmpgEvaluator ,	// TR::dcmpg		// double compare g (1 if child1 > child2 or unordered; 0 if child1 == child2; -1 if child1 < child2)
    TR::TreeEvaluator::ificmpeqEvaluator, // TR::ificmpeq		// integer compare and branch if equal
    TR::TreeEvaluator::ificmpneEvaluator, // TR::ificmpne		// integer compare and branch if not equal
    TR::TreeEvaluator::ificmpltEvaluator, // TR::ificmplt		// integer compare and branch if less than
    TR::TreeEvaluator::ificmpgeEvaluator, // TR::ificmpge		// integer compare and branch if greater than or equal
    TR::TreeEvaluator::ificmpgtEvaluator, // TR::ificmpgt		// integer compare and branch if greater than
    TR::TreeEvaluator::ificmpleEvaluator, // TR::ificmple		// integer compare and branch if less than or equal
    TR::TreeEvaluator::ificmpeqEvaluator, // TR::ifiucmpeq		// unsigned integer compare and branch if equal
    TR::TreeEvaluator::ificmpneEvaluator, // TR::ifiucmpne		// unsigned integer compare and branch if not equal
    TR::TreeEvaluator::ifiucmpltEvaluator, // TR::ifiucmplt		// unsigned integer compare and branch if less than
    TR::TreeEvaluator::ifiucmpgeEvaluator, // TR::ifiucmpge		// unsigned integer compare and branch if greater than or equal
    TR::TreeEvaluator::ifiucmpgtEvaluator, // TR::ifiucmpgt		// unsigned integer compare and branch if greater than
    TR::TreeEvaluator::ifiucmpleEvaluator, // TR::ifiucmple		// unsigned integer compare and branch if less than or equal
    TR::TreeEvaluator::iflcmpeqEvaluator, // TR::iflcmpeq		// long compare and branch if equal
    TR::TreeEvaluator::iflcmpneEvaluator, // TR::iflcmpne		// long compare and branch if not equal
    TR::TreeEvaluator::iflcmpltEvaluator, // TR::iflcmplt		// long compare and branch if less than
    TR::TreeEvaluator::iflcmpgeEvaluator, // TR::iflcmpge		// long compare and branch if greater than or equal
    TR::TreeEvaluator::iflcmpgtEvaluator, // TR::iflcmpgt		// long compare and branch if greater than
    TR::TreeEvaluator::iflcmpleEvaluator, // TR::iflcmple		// long compare and branch if less than or equal
    TR::TreeEvaluator::iflcmpeqEvaluator, // TR::iflucmpeq		// unsigned long compare and branch if equal
    TR::TreeEvaluator::iflcmpneEvaluator, // TR::iflucmpne		// unsigned long compare and branch if not equal
    TR::TreeEvaluator::iflucmpltEvaluator, // TR::iflucmplt		// unsigned long compare and branch if less than
    TR::TreeEvaluator::iflucmpgeEvaluator, // TR::iflucmpge		// unsigned long compare and branch if greater than or equal
    TR::TreeEvaluator::iflucmpgtEvaluator, // TR::iflucmpgt		// unsigned long compare and branch if greater than
    TR::TreeEvaluator::iflucmpleEvaluator, // TR::iflucmple		// unsigned long compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpeqEvaluator ,	// TR::iffcmpeq		// float compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpneEvaluator ,	// TR::iffcmpne		// float compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpltEvaluator ,	// TR::iffcmplt		// float compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpgeEvaluator ,	// TR::iffcmpge		// float compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpgtEvaluator ,	// TR::iffcmpgt		// float compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpleEvaluator ,	// TR::iffcmple		// float compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpequEvaluator ,	// TR::iffcmpequ		// float compare and branch if equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpneuEvaluator ,	// TR::iffcmpneu		// float compare and branch if not equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpltuEvaluator ,	// TR::iffcmpltu		// float compare and branch if less than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpgeuEvaluator ,	// TR::iffcmpgeu		// float compare and branch if greater than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpgtuEvaluator ,	// TR::iffcmpgtu		// float compare and branch if greater than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iffcmpleuEvaluator ,	// TR::iffcmpleu		// float compare and branch if less than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpeqEvaluator ,	// TR::ifdcmpeq		// double compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpneEvaluator ,	// TR::ifdcmpne		// double compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpltEvaluator ,	// TR::ifdcmplt		// double compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpgeEvaluator ,	// TR::ifdcmpge		// double compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpgtEvaluator ,	// TR::ifdcmpgt		// double compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpleEvaluator ,	// TR::ifdcmple		// double compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpequEvaluator ,	// TR::ifdcmpequ		// double compare and branch if equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpneuEvaluator ,	// TR::ifdcmpneu		// double compare and branch if not equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpltuEvaluator ,	// TR::ifdcmpltu		// double compare and branch if less than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpgeuEvaluator ,	// TR::ifdcmpgeu		// double compare and branch if greater than or equal or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpgtuEvaluator ,	// TR::ifdcmpgtu		// double compare and branch if greater than or unordered
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifdcmpleuEvaluator ,	// TR::ifdcmpleu		// double compare and branch if less than or equal or unordered
    TR::TreeEvaluator::iflcmpeqEvaluator, // TR::ifacmpeq		// address compare and branch if equal
    TR::TreeEvaluator::iflcmpneEvaluator, // TR::ifacmpne		// address compare and branch if not equal
    TR::TreeEvaluator::iflucmpltEvaluator, // TR::ifacmplt		// address compare and branch if less than
    TR::TreeEvaluator::iflucmpgeEvaluator, // TR::ifacmpge		// address compare and branch if greater than or equal
    TR::TreeEvaluator::iflucmpgtEvaluator, // TR::ifacmpgt		// address compare and branch if greater than
    TR::TreeEvaluator::iflucmpleEvaluator, // TR::ifacmple		// address compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpeqEvaluator ,	// TR::ifbcmpeq		// byte compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpneEvaluator ,	// TR::ifbcmpne		// byte compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpltEvaluator ,	// TR::ifbcmplt		// byte compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpgeEvaluator ,	// TR::ifbcmpge		// byte compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpgtEvaluator ,	// TR::ifbcmpgt		// byte compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbcmpleEvaluator ,	// TR::ifbcmple		// byte compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpeqEvaluator ,	// TR::ifbucmpeq		// unsigned byte compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpneEvaluator ,	// TR::ifbucmpne		// unsigned byte compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpltEvaluator ,	// TR::ifbucmplt		// unsigned byte compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpgeEvaluator ,	// TR::ifbucmpge		// unsigned byte compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpgtEvaluator ,	// TR::ifbucmpgt		// unsigned byte compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifbucmpleEvaluator ,	// TR::ifbucmple		// unsigned byte compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpeqEvaluator ,	// TR::ifscmpeq		// short integer compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpneEvaluator ,	// TR::ifscmpne		// short integer compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpltEvaluator ,	// TR::ifscmplt		// short integer compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpgeEvaluator ,	// TR::ifscmpge		// short integer compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpgtEvaluator ,	// TR::ifscmpgt		// short integer compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifscmpleEvaluator ,	// TR::ifscmple		// short integer compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpeqEvaluator ,	// TR::ifsucmpeq		// char compare and branch if equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpneEvaluator ,	// TR::ifsucmpne		// char compare and branch if not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpltEvaluator ,	// TR::ifsucmplt		// char compare and branch if less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpgeEvaluator ,	// TR::ifsucmpge		// char compare and branch if greater than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpgtEvaluator ,	// TR::ifsucmpgt		// char compare and branch if greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ifsucmpleEvaluator ,	// TR::ifsucmple		// char compare and branch if less than or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::loadaddrEvaluator ,	// TR::loadaddr		// load address of non-heap storage item (Auto; Parm; Static or Method)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ZEROCHKEvaluator ,	// TR::ZEROCHK		// Zero-check an int.  Symref indicates call to perform when first child is zero.  Other children are arguments to the call.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::callIfEvaluator ,	// TR::callIf		// Call symref if first child evaluates to true.  Other childrem are arguments to the call.
    TR::TreeEvaluator::iRegLoadEvaluator, // TR::iRegLoad		// Load integer global register
    TR::TreeEvaluator::aRegLoadEvaluator, // TR::aRegLoad		// Load address global register
    TR::TreeEvaluator::iRegLoadEvaluator, // TR::lRegLoad		// Load long integer global register
    TR::TreeEvaluator::fRegLoadEvaluator, // TR::fRegLoad		// Load float global register
    TR::TreeEvaluator::fRegLoadEvaluator, // TR::dRegLoad		// Load double global register
    TR::TreeEvaluator::iRegLoadEvaluator, // TR::sRegLoad		// Load short global register
    TR::TreeEvaluator::iRegLoadEvaluator, // TR::bRegLoad		// Load byte global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::iRegStore		// Store integer global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::aRegStore		// Store address global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::lRegStore		// Store long integer global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::fRegStore		// Store float global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::dRegStore		// Store double global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::sRegStore		// Store short global register
    TR::TreeEvaluator::iRegStoreEvaluator, // TR::bRegStore		// Store byte global register
    TR::TreeEvaluator::GlRegDepsEvaluator, // TR::GlRegDeps		// Global Register Dependency List
    TR::TreeEvaluator::iselectEvaluator ,	// TR::iselect		// Select Operator:  Based on the result of the first child; take the value of the
    TR::TreeEvaluator::iselectEvaluator ,	// TR::lselect		//   second (first child evaluates to true) or third(first child evaluates to false) child
    TR::TreeEvaluator::iselectEvaluator ,	// TR::bselect
    TR::TreeEvaluator::iselectEvaluator ,	// TR::sselect
    TR::TreeEvaluator::iselectEvaluator ,	// TR::aselect
    TR::TreeEvaluator::fselectEvaluator ,	// TR::fselect
    TR::TreeEvaluator::dselectEvaluator ,	// TR::dselect
    TR::TreeEvaluator::treetopEvaluator, // TR::treetop		// tree top to anchor subtrees with side-effects
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::MethodEnterHookEvaluator ,	// TR::MethodEnterHook	// called after a frame is built; temps initialized; and monitor acquired (if necessary)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::MethodExitHookEvaluator ,	// TR::MethodExitHook	// called immediately before returning; frame not yet collapsed; monitor released (if necessary)
    TR::TreeEvaluator::passThroughEvaluator, // TR::passThrough	// Dummy node that represents its single child.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::compressedRefsEvaluator ,	// TR::compressedRefs	// no-op anchor providing optimizable subexpressions used for compression/decompression.  First child is address load/store; second child is heap base displacement
    TR::TreeEvaluator::BBStartEvaluator, // TR::BBStart		// Start of Basic Block
    TR::TreeEvaluator::BBEndEvaluator, // TR::BBEnd		// End of Basic Block
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::viremEvaluator ,	// TR::virem		// vector integer remainder
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::viminEvaluator ,	// TR::vimin		// vector integer minimum
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vimaxEvaluator ,	// TR::vimax		// vector integer maximum
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vigetelemEvaluator ,	// TR::vigetelem		// get vector int element
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::visetelemEvaluator ,	// TR::visetelem		// set vector int element
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vimergelEvaluator ,	// TR::vimergel		// vector int merge low
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vimergehEvaluator ,	// TR::vimergeh		// vector int merge high
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpeqEvaluator ,	// TR::vicmpeq		// vector integer compare equal  (return vector mask)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpgtEvaluator ,	// TR::vicmpgt		// vector integer compare greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpgeEvaluator ,	// TR::vicmpge		// vector integer compare greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpltEvaluator ,	// TR::vicmplt		// vector integer compare less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpleEvaluator ,	// TR::vicmple		// vector integer compare less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpalleqEvaluator ,	// TR::vicmpalleq	// vector integer all equal (return boolean)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpallneEvaluator ,	// TR::vicmpallne	// vector integer all not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpallgtEvaluator ,	// TR::vicmpallgt	// vector integer all greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpallgeEvaluator ,	// TR::vicmpallge	// vector integer all greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpallltEvaluator ,	// TR::vicmpalllt	// vector integer all less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpallleEvaluator ,	// TR::vicmpallle	// vector integer all less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanyeqEvaluator ,	// TR::vicmpanyeq	// vector integer any equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanyneEvaluator ,	// TR::vicmpanyne	// vector integer any not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanygtEvaluator ,	// TR::vicmpanygt	// vector integer any greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanygeEvaluator ,	// TR::vicmpanyge	// vector integer any greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanyltEvaluator ,	// TR::vicmpanylt	// vector integer any less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vicmpanyleEvaluator ,	// TR::vicmpanyle	// vector integer any less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vnotEvaluator ,	// TR::vnot		// vector boolean not
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vselectEvaluator ,	// TR::vselect		// vector select
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vpermEvaluator ,	// TR::vperm		// vector permute
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vsplatsEvaluator ,	// TR::vsplats		// vector splats
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdmergelEvaluator ,	// TR::vdmergel		// vector double merge low
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdmergehEvaluator ,	// TR::vdmergeh		// vector double merge high
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdsetelemEvaluator ,	// TR::vdsetelem		// set vector double element
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdgetelemEvaluator ,	// TR::vdgetelem		// get vector double element
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdselEvaluator ,	// TR::vdsel		// get vector select double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdremEvaluator ,	// TR::vdrem		// vector double remainder
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdmaddEvaluator ,	// TR::vdmadd		// vector double fused multiply add
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdnmsubEvaluator ,	// TR::vdnmsub		// vector double fused negative multiply subtract
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdmsubEvaluator ,	// TR::vdmsub		// vector double fused multiply subtract
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdmaxEvaluator ,	// TR::vdmax		// vector double maximum
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdminEvaluator ,	// TR::vdmin		// vector double minimum
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpeqEvaluator ,	// TR::vdcmpeq		// vector double compare equal  (return vector mask)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpneEvaluator ,	// TR::vdcmpne		// vector double compare not equal  (return vector mask)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpgtEvaluator ,	// TR::vdcmpgt		// vector double compare greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpgeEvaluator ,	// TR::vdcmpge		// vector double compare greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpltEvaluator ,	// TR::vdcmplt		// vector double compare less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpleEvaluator ,	// TR::vdcmple		// vector double compare less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpalleqEvaluator ,	// TR::vdcmpalleq	// vector double compare all equal  (return boolean)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpallneEvaluator ,	// TR::vdcmpallne	// vector double compare all not equal  (return boolean)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpallgtEvaluator ,	// TR::vdcmpallgt	// vector double compare all greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpallgeEvaluator ,	// TR::vdcmpallge	// vector double compare all greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpallltEvaluator ,	// TR::vdcmpalllt	// vector double compare all less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpallleEvaluator ,	// TR::vdcmpallle	// vector double compare all less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanyeqEvaluator ,	// TR::vdcmpanyeq	// vector double compare any equal  (return boolean)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanyneEvaluator ,	// TR::vdcmpanyne	// vector double compare any not equal  (return boolean)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanygtEvaluator ,	// TR::vdcmpanygt	// vector double compare any greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanygeEvaluator ,	// TR::vdcmpanyge	// vector double compare any greater equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanyltEvaluator ,	// TR::vdcmpanylt	// vector double compare any less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdcmpanyleEvaluator ,	// TR::vdcmpanyle	// vector double compare any less equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdsqrtEvaluator ,	// TR::vdsqrt		// vector double square root
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdlogEvaluator ,	// TR::vdlog		// vector double natural log
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vincEvaluator ,	// TR::vinc		// vector increment
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdecEvaluator ,	// TR::vdec		// vector decrement
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vnegEvaluator ,	// TR::vneg		// vector negation
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcomEvaluator ,	// TR::vcom		// vector complement
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vaddEvaluator ,	// TR::vadd		// vector add
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vsubEvaluator ,	// TR::vsub		// vector subtract
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vmulEvaluator ,	// TR::vmul		// vector multiply
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdivEvaluator ,	// TR::vdiv		// vector divide
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vremEvaluator ,	// TR::vrem		// vector remainder
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vandEvaluator ,	// TR::vand		// vector logical AND
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vorEvaluator ,	// TR::vor		// vector logical OR
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vxorEvaluator ,	// TR::vxor		// vector exclusive OR integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vshlEvaluator ,	// TR::vshl		// vector shift left
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vushrEvaluator ,	// TR::vushr		// vector shift right logical
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vshrEvaluator ,	// TR::vshr		// vector shift right arithmetic
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpeqEvaluator ,	// TR::vcmpeq		// vector compare equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpneEvaluator ,	// TR::vcmpne		// vector compare not equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpltEvaluator ,	// TR::vcmplt		// vector compare less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vucmpltEvaluator ,	// TR::vucmplt		// vector unsigned compare less than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpgtEvaluator ,	// TR::vcmpgt		// vector compare greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vucmpgtEvaluator ,	// TR::vucmpgt		// vector unsigned compare greater than
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpleEvaluator ,	// TR::vcmple		// vector compare less or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vucmpleEvaluator ,	// TR::vucmple		// vector unsigned compare less or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcmpgeEvaluator ,	// TR::vcmpge		// vector compare greater or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vucmpgeEvaluator ,	// TR::vucmpge		// vector unsigned compare greater or equal
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vloadEvaluator ,	// TR::vload		// load vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vloadiEvaluator ,	// TR::vloadi		// load indirect vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vstoreEvaluator ,	// TR::vstore		// store vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vstoreiEvaluator ,	// TR::vstorei		// store indirect vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vrandEvaluator ,	// TR::vrand		// AND all elements into single value of element size
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vreturnEvaluator ,	// TR::vreturn		// return a vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcallEvaluator ,	// TR::vcall		// direct call returning a vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vcalliEvaluator ,	// TR::vcalli		// indirect call returning a vector
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vselectEvaluator ,	// TR::vselect		// vector select operator
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::v2vEvaluator ,	// TR::v2v		// vector to vector conversion. preserves bit pattern (noop); only changes datatype
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vl2vdEvaluator ,	// TR::vl2vd		// vector to vector conversion. converts each long element to double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vconstEvaluator ,	// TR::vconst		// vector constant
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::getvelemEvaluator ,	// TR::getvelem		// get vector element; returns a scalar
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vsetelemEvaluator ,	// TR::vsetelem		// vector set element
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vbRegLoadEvaluator ,	// TR::vbRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vsRegLoadEvaluator ,	// TR::vsRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::viRegLoadEvaluator ,	// TR::viRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vlRegLoadEvaluator ,	// TR::vlRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vfRegLoadEvaluator ,	// TR::vfRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdRegLoadEvaluator ,	// TR::vdRegLoad		// Load vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vbRegStoreEvaluator ,	// TR::vbRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vsRegStoreEvaluator ,	// TR::vsRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::viRegStoreEvaluator ,	// TR::viRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vlRegStoreEvaluator ,	// TR::vlRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vfRegStoreEvaluator ,	// TR::vfRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::vdRegStoreEvaluator ,	// TR::vdRegStore	// Store vector global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuconstEvaluator ,	// TR::iuconst		// load unsigned integer constant (32-but unsigned)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luconstEvaluator ,	// TR::luconst		// load unsigned long integer constant (64-bit unsigned)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::buconstEvaluator ,	// TR::buconst		// load unsigned byte integer constant (8-bit unsigned)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuloadEvaluator ,	// TR::iuload		// load unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luloadEvaluator ,	// TR::luload		// load unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::buloadEvaluator ,	// TR::buload		// load unsigned byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuloadiEvaluator ,	// TR::iuloadi		// load indirect unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luloadiEvaluator ,	// TR::luloadi		// load indirect unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::buloadiEvaluator ,	// TR::buloadi		// load indirect unsigned byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iustoreEvaluator ,	// TR::iustore		// store unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lustoreEvaluator ,	// TR::lustore		// store unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bustoreEvaluator ,	// TR::bustore		// store unsigned byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iustoreiEvaluator ,	// TR::iustorei		// store indirect unsigned integer       (child1 a; child2 i)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lustoreiEvaluator ,	// TR::lustorei		// store indirect unsigned long integer  (child1 a; child2 l)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bustoreiEvaluator ,	// TR::bustorei		// store indirect unsigned byte          (child1 a; child2 b)
    TR::TreeEvaluator::ireturnEvaluator, // TR::iureturn		// return an unsigned integer
    TR::TreeEvaluator::lreturnEvaluator, // TR::lureturn		// return a long unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iucallEvaluator ,	// TR::iucall		// direct call returning unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lucallEvaluator ,	// TR::lucall		// direct call returning unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuaddEvaluator ,	// TR::iuadd		// add 2 unsigned integers
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luaddEvaluator ,	// TR::luadd		// add 2 unsigned long integers
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::buaddEvaluator ,	// TR::buadd		// add 2 unsigned bytes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iusubEvaluator ,	// TR::iusub		// subtract 2 unsigned integers       (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lusubEvaluator ,	// TR::lusub		// subtract 2 unsigned long integers  (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::busubEvaluator ,	// TR::busub		// subtract 2 unsigned bytes          (child1 - child2)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iunegEvaluator ,	// TR::iuneg		// negate an unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lunegEvaluator ,	// TR::luneg		// negate a unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2iuEvaluator ,	// TR::f2iu		// convert float to unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2luEvaluator ,	// TR::f2lu		// convert float to unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2buEvaluator ,	// TR::f2bu		// convert float to unsigned byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::f2cEvaluator ,	// TR::f2c		// convert float to char
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2iuEvaluator ,	// TR::d2iu		// convert double to unsigned integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2luEvaluator ,	// TR::d2lu		// convert double to unsigned long integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2buEvaluator ,	// TR::d2bu		// convert double to unsigned byte
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::d2cEvaluator ,	// TR::d2c		// convert double to char
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuRegLoadEvaluator ,	// TR::iuRegLoad		// Load unsigned integer global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luRegLoadEvaluator ,	// TR::luRegLoad		// Load unsigned long integer global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuRegStoreEvaluator ,	// TR::iuRegStore	// Store unsigned integer global register
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luRegStoreEvaluator ,	// TR::luRegStore	// Store long integer global register
    TR::TreeEvaluator::cconstEvaluator, // TR::cconst		// load unicode constant (16-bit unsigned)
    TR::TreeEvaluator::cloadEvaluator, // TR::cload		// load short unsigned integer
    TR::TreeEvaluator::cloadEvaluator, // TR::cloadi		// load indirect unsigned short integer
    TR::TreeEvaluator::sstoreEvaluator, // TR::cstore		// store unsigned short integer
    TR::TreeEvaluator::sstoreEvaluator, // TR::cstorei		// store indirect unsigned short integer (child1 a; child2 c)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::monentEvaluator ,	// TR::monent		// acquire lock for synchronising method
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::monexitEvaluator ,	// TR::monexit		// release lock for synchronising method
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::monexitfenceEvaluator ,	// TR::monexitfence	//denotes the end of a monitored region solely for live monitor meta data
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::tstartEvaluator ,	// TR::tstart		// transaction begin
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::tfinishEvaluator ,	// TR::tfinish		// transaction end
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::tabortEvaluator ,	// TR::tabort		// transaction abort
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::instanceofEvaluator ,	// TR::instanceof	// instanceof - symref is the class object; cp index is in the "int" field; child is the object reference
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::checkcastEvaluator ,	// TR::checkcast		// checkcast
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::checkcastAndNULLCHKEvaluator ,	// TR::checkcastAndNULLCHK	// checkcast and NULL check the underlying object reference
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::NewEvaluator ,	// TR::New		// new - child is class
    TR::TreeEvaluator::unImpOpEvaluator ,        // TR::newvalue (should be lowered before evaluation)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::newarrayEvaluator ,	// TR::newarray		// new array of primitives
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::anewarrayEvaluator ,	// TR::anewarray		// new array of objects
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::variableNewEvaluator ,	// TR::variableNew	// new - child is class; type not known at compile time
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::variableNewArrayEvaluator ,	// TR::variableNewArray	// new array - type not known at compile time; type must be a j9class; do not use type enums
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::multianewarrayEvaluator ,	// TR::multianewarray	// multi-dimensional new array of objects
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraylengthEvaluator ,	// TR::arraylength	// number of elements in an array
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::contigarraylengthEvaluator ,	// TR::contigarraylength	// number of elements in a contiguous array
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::discontigarraylengthEvaluator ,	// TR::discontigarraylength	// number of elements in a discontiguous array
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::icalliEvaluator ,	// TR::icalli		// indirect call returning integer (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iucalliEvaluator ,	// TR::iucalli		// indirect call returning unsigned integer (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lcalliEvaluator ,	// TR::lcalli		// indirect call returning long integer (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lucalliEvaluator ,	// TR::lucalli		// indirect call returning unsigned long integer (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fcalliEvaluator ,	// TR::fcalli		// indirect call returning float (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcalliEvaluator ,	// TR::dcalli		// indirect call returning double (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::acalliEvaluator ,	// TR::acalli		// indirect call returning reference
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::calliEvaluator ,	// TR::calli		// indirect call returning void (child1 is addr of function)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fenceEvaluator ,	// TR::fence		// barrier to optimization
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luaddhEvaluator ,	// TR::luaddh		// add 2 unsigned long integers (the high parts of prior luadd) as high part of 128bit addition.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::caddEvaluator ,	// TR::cadd		// add 2 unsigned short integers
    TR::TreeEvaluator::laddEvaluator ,	// TR::aiadd		// add integer to address with address result (child1 a; child2 i)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::aiuaddEvaluator ,	// TR::aiuadd		// add unsigned integer to address with address result (child1 a; child2 i)
    TR::TreeEvaluator::laddEvaluator ,	// TR::aladd		// add long integer to address with address result (child1 a; child2 i) (64-bit only)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::aluaddEvaluator ,	// TR::aluadd		// add unsigned long integer to address with address result (child1 a; child2 i) (64-bit only)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lusubhEvaluator ,	// TR::lusubh		// subtract 2 unsigned long integers (the high parts of prior lusub) as high part of 128bit subtraction.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::csubEvaluator ,	// TR::csub		// subtract 2 unsigned short integers (child1 - child2)
    TR::TreeEvaluator::imulhEvaluator ,  // TR::imulh		// multiply 2 integers; and return the high word of the product
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iumulhEvaluator ,	// TR::iumulh		// multiply 2 unsigned integers; and return the high word of the product
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lmulhEvaluator ,	// TR::lmulh		// multiply 2 long integers; and return the high word of the product
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lumulhEvaluator ,	// TR::lumulh		// multiply 2 unsigned long integers; and return the high word of the product
    TR::TreeEvaluator::ibits2fEvaluator, // TR::ibits2f		// type-coerce int to float
    TR::TreeEvaluator::fbits2iEvaluator, // TR::fbits2i		// type-coerce float to int
    TR::TreeEvaluator::lbits2dEvaluator, // TR::lbits2d		// type-coerce long to double
    TR::TreeEvaluator::dbits2lEvaluator, // TR::dbits2l		// type-coerce double to long
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lookupEvaluator ,	// TR::lookup		// lookupswitch (child1 is selector expression; child2 the default destination; subsequent children are case nodes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::trtLookupEvaluator ,	// TR::trtLookup		// special lookupswitch (child1 must be trt; child2 the default destination; subsequent children are case nodes) The internal control flow is similar to lookup; but each CASE represents a special semantics associated with a flag on it
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::CaseEvaluator ,	// TR::Case		// case nodes that are children of TR_switch.  Uses the branchdestination and the int const field
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::tableEvaluator ,	// TR::table		// tableswitch (child1 is the selector; child2 the default destination; subsequent children are the branch targets (the last child may be a branch table address; use getCaseIndexUpperBound() when iterating over branch targets)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::exceptionRangeFenceEvaluator ,	// TR::exceptionRangeFence	// (J9) SymbolReference is the aliasing effect; initializer is where the code address gets put when binary is generated used for delimiting function; try blocks; catch clauses; finally clauses; etc.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dbgFenceEvaluator ,	// TR::dbgFence		// used to delimit code (stmts) for debug info.  Has no symbol reference.
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::NULLCHKEvaluator ,	// TR::NULLCHK		// Null check a pointer.  child 1 is indirect reference. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ResolveCHKEvaluator ,	// TR::ResolveCHK	// Resolve check a static; field or method. child 1 is reference to be resolved. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ResolveAndNULLCHKEvaluator ,	// TR::ResolveAndNULLCHK	// Resolve check a static; field or method and Null check the underlying pointer.  child 1 is reference to be resolved. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::DIVCHKEvaluator ,	// TR::DIVCHK		// Divide by zero check. child 1 is the divide. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::OverflowCHKEvaluator ,	// TR::OverflowCHK	// Overflow check. child 1 is the operation node(add; mul; sub). Child 2 and child 3 are the operands of the operation of the operation. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::UnsignedOverflowCHKEvaluator ,	// TR::UnsignedOverflowCHK	// UnsignedOverflow check. child 1 is the operation node(add; mul; sub). Child 2 and child 3 are the operands of the operation of the operation. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::BNDCHKEvaluator ,	// TR::BNDCHK		// Array bounds check; checks that child 1 > child 2 >= 0 (child 1 is bound; 2 is index). Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ArrayCopyBNDCHKEvaluator ,	// TR::ArrayCopyBNDCHK	// Array copy bounds check; checks that child 1 >= child 2. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::BNDCHKwithSpineCHKEvaluator ,	// TR::BNDCHKwithSpineCHK	// Array bounds check and spine check
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::SpineCHKEvaluator ,	// TR::SpineCHK		// Check if the base array has a spine
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ArrayStoreCHKEvaluator ,	// TR::ArrayStoreCHK	// Array store check. child 1 is object; 2 is array. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ArrayCHKEvaluator ,	// TR::ArrayCHK		// Array compatibility check. child 1 is object1; 2 is object2. Symbolref indicates failure action/destination
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::RetEvaluator ,	// TR::Ret		// Used by ilGen only
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraycopyEvaluator ,	// TR::arraycopy		// Call to System.arraycopy that may be partially inlined
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraysetEvaluator ,	// TR::arrayset		// Inline code for memory initialization of part of an array
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraytranslateEvaluator ,	// TR::arraytranslate	// Inline code for translation of part of an array to another form via lookup
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraytranslateAndTestEvaluator ,	// TR::arraytranslateAndTest	// Inline code for scanning of part of an array for a particular 8-bit character
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::long2StringEvaluator ,	// TR::long2String		// Convert integer/long value to String
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bitOpMemEvaluator ,	// TR::bitOpMem			// bit operations (AND; OR; XOR) for memory to memory
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bitOpMemNDEvaluator ,	// TR::bitOpMemND		// 3 operand(source1;source2;target) version of bitOpMem
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraycmpEvaluator ,	// TR::arraycmp			// Inline code for memory comparison of part of an array
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::arraycmpWithPadEvaluator ,	// TR::arraycmpWithPad		// memory comparison when src1 length != src2 length and padding is needed
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::allocationFenceEvaluator ,	// TR::allocationFence		// Internal fence guarding escape of newObject & final fields - eliminatable
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::loadFenceEvaluator ,	// TR::loadFence			// JEP171: prohibits loadLoad and loadStore reordering (on globals)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::storeFenceEvaluator ,	// TR::storeFence		// JEP171: prohibits loadStore and storeStore reordering (on globals)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fullFenceEvaluator ,	// TR::fullFence			// JEP171: prohibits loadLoad; loadStore; storeLoad; and storeStore reordering (on globals)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::MergeNewEvaluator ,	// TR::MergeNew			// Parent for New etc. nodes that can all be allocated together
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::computeCCEvaluator ,	// TR::computeCC			// compute Condition Codes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::butestEvaluator ,	// TR::butest			// zEmulator: mask unsigned byte (UInt8) and set condition codes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sutestEvaluator ,	// TR::sutest			// zEmulator: mask unsigned short (UInt16) and set condition codes
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bucmpEvaluator ,	// TR::bucmp			// Currently only valid for zEmulator. Based on the ordering of the two children set the return value:
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bcmpEvaluator ,	// TR::bcmp			//    0 : child1 == child2
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sucmpEvaluator ,	// TR::sucmp			//    1 : child1 < child2
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::scmpEvaluator ,	// TR::scmp			//    2 : child1 > child2
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iucmpEvaluator ,	// TR::iucmp      
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::icmpEvaluator ,	// TR::icmp       
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lucmpEvaluator ,	// TR::lucmp      
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ificmpoEvaluator ,	// TR::ificmpo			// integer compare and branch if overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ificmpnoEvaluator ,	// TR::ificmpno			// integer compare and branch if not overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iflcmpoEvaluator ,	// TR::iflcmpo			// long compare and branch if overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iflcmpnoEvaluator ,	// TR::iflcmpno			// long compare and branch if not overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ificmnoEvaluator ,	// TR::ificmno			// integer compare negative and branch if overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ificmnnoEvaluator ,	// TR::ificmnno			// integer compare negative and branch if not overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iflcmnoEvaluator ,	// TR::iflcmno			// long compare negative and branch if overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iflcmnnoEvaluator ,	// TR::iflcmnno			// long compare negative and branch if not overflow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuaddcEvaluator ,	// TR::iuaddc			// Currently only valid for zEmulator.  Add two unsigned ints with carry
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luaddcEvaluator ,	// TR::luaddc			// Add two longs with carry
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iusubbEvaluator ,	// TR::iusubb			// Subtract two ints with borrow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lusubbEvaluator ,	// TR::lusubb			// Subtract two longs with borrow
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::icmpsetEvaluator ,	// TR::icmpset			// icmpset(pointer;c;r): compare *pointer with c; if it matches; replace with r.  Returns 0 on match; 1 otherwise
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lcmpsetEvaluator ,	// TR::lcmpset			// the operation is done atomically - return type is int for both [il]cmpset
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bztestnsetEvaluator ,	// TR::bztestnset		// bztestnset(pointer;c): atomically sets *pointer to c and returns the original value of *p (represents Test And Set on Z) the atomic ops.. atomically update the symref.  first child is address; second child is the RHS interestingly; these ops act like loads and stores at the same time
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ibatomicorEvaluator ,	// TR::ibatomicor 
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::isatomicorEvaluator ,	// TR::isatomicor 
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iiatomicorEvaluator ,	// TR::iiatomicor 
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ilatomicorEvaluator ,	// TR::ilatomicor 
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dexpEvaluator ,	// TR::dexp		// double exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::branchEvaluator ,	// TR::branch		// generic branch --> DEPRECATED use TR::case instead
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::igotoEvaluator ,	// TR::igoto		// indirect goto; branches to the address specified by a child
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bexpEvaluator ,	// TR::bexp		// signed byte exponent  (raise signed byte to power)
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::buexpEvaluator ,	// TR::buexp		// unsigned byte exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sexpEvaluator ,	// TR::sexp		// short exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::cexpEvaluator ,	// TR::cexp		// unsigned short exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iexpEvaluator ,	// TR::iexp		// integer exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::iuexpEvaluator ,	// TR::iuexp		// unsigned integer exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lexpEvaluator ,	// TR::lexp		// long exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::luexpEvaluator ,	// TR::luexp		// unsigned long exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fexpEvaluator ,	// TR::fexp		// float exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fuexpEvaluator ,	// TR::fuexp		// float base to unsigned integral exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::duexpEvaluator ,	// TR::duexp		// double base to unsigned integral exponent
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ixfrsEvaluator ,	// TR::ixfrs		// transfer sign integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lxfrsEvaluator ,	// TR::lxfrs		// transfer sign long
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fxfrsEvaluator ,	// TR::fxfrs		// transfer sign float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dxfrsEvaluator ,	// TR::dxfrs		// transfer sign double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fintEvaluator ,	// TR::fint		// truncate float to int
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dintEvaluator ,	// TR::dint		// truncate double to int
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fnintEvaluator ,	// TR::fnint		// round float to nearest int
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dnintEvaluator ,	// TR::dnint		// round double to nearest int
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fsqrtEvaluator ,	// TR::fsqrt		// square root of float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dsqrtEvaluator ,	// TR::dsqrt		// square root of double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::getstackEvaluator ,	// TR::getstack		// returns current value of SP
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::deallocaEvaluator ,	// TR::dealloca		// resets value of SP
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::idozEvaluator ,	// TR::idoz		// difference or zero
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcosEvaluator ,	// TR::dcos		// cos of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dsinEvaluator ,	// TR::dsin		// sin of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dtanEvaluator ,	// TR::dtan		// tan of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dcoshEvaluator ,	// TR::dcosh		// cos of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dsinhEvaluator ,	// TR::dsinh		// sin of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dtanhEvaluator ,	// TR::dtanh		// tan of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dacosEvaluator ,	// TR::dacos		// arccos of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dasinEvaluator ,	// TR::dasin		// arcsin of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::datanEvaluator ,	// TR::datan		// arctan of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::datan2Evaluator ,	// TR::datan2		// arctan2 of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dlogEvaluator ,	// TR::dlog		// log of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dfloorEvaluator ,	// TR::dfloor		// floor of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ffloorEvaluator ,	// TR::ffloor		// floor of float; returning float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::dceilEvaluator ,	// TR::dceil		// ceil of double; returning double
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::fceilEvaluator ,	// TR::fceil		// ceil of float; returning float
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ibranchEvaluator ,	// TR::ibranch		// generic indirct branch --> first child is a constant indicating the mask
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::mbranchEvaluator ,	// TR::mbranch		// generic branch to multiple potential targets
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::getpmEvaluator ,	// TR::getpm		// get program mask
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::setpmEvaluator ,	// TR::setpm		// set program mask
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::loadAutoOffsetEvaluator ,	// TR::loadAutoOffset	// loads the offset (from the SP) of an auto
    TR::TreeEvaluator::imaxEvaluator ,	// TR::imax		// max of 2 or more integers
    TR::TreeEvaluator::iumaxEvaluator ,	// TR::iumax		// max of 2 or more unsigned integers
    TR::TreeEvaluator::imaxEvaluator ,	// TR::lmax		// max of 2 or more longs
    TR::TreeEvaluator::iumaxEvaluator ,	// TR::lumax		// max of 2 or more unsigned longs
    TR::TreeEvaluator::fmaxEvaluator ,	// TR::fmax		// max of 2 or more floats
    TR::TreeEvaluator::dmaxEvaluator ,	// TR::dmax		// max of 2 or more doubles
    TR::TreeEvaluator::iminEvaluator ,	// TR::imin		// min of 2 or more integers
    TR::TreeEvaluator::iuminEvaluator ,	// TR::iumin		// min of 2 or more unsigned integers
    TR::TreeEvaluator::iminEvaluator ,	// TR::lmin		// min of 2 or more longs
    TR::TreeEvaluator::iuminEvaluator ,	// TR::lumin		// min of 2 or more unsigned longs
    TR::TreeEvaluator::fminEvaluator ,	// TR::fmin		// min of 2 or more floats
    TR::TreeEvaluator::dminEvaluator ,	// TR::dmin		// min of 2 or more doubles
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::trtEvaluator ,	// TR::trt		// translate and test
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::trtSimpleEvaluator ,	// TR::trtSimple		// same as TRT but ignoring the returned source byte address and table entry value
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ihbitEvaluator ,	// TR::ihbit
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ilbitEvaluator ,	// TR::ilbit
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::inolzEvaluator ,	// TR::inolz
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::inotzEvaluator ,	// TR::inotz
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ipopcntEvaluator ,	// TR::ipopcnt
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lhbitEvaluator ,	// TR::lhbit
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::llbitEvaluator ,	// TR::llbit
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lnolzEvaluator ,	// TR::lnolz
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lnotzEvaluator ,	// TR::lnotz
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lpopcntEvaluator ,	// TR::lpopcnt
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ibyteswapEvaluator ,	// TR::ibyteswap		// swap bytes in an integer
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::bbitpermuteEvaluator ,	// TR::bbitpermute
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::sbitpermuteEvaluator ,	// TR::sbitpermute
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::ibitpermuteEvaluator ,	// TR::ibitpermute
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::lbitpermuteEvaluator ,	// TR::lbitpermute
    TR::TreeEvaluator::unImpOpEvaluator ,        // TODO:RV: Enable when Implemented: TR::TreeEvaluator::PrefetchEvaluator, 		// TR::Prefetch
