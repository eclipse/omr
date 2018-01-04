/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_ILOPCODES_ENUM_INCL
#define OMR_ILOPCODES_ENUM_INCL

//  NOTE: IF you add opcodes or change the order then you must fix the following
//        files (at least): ./ILOpCodeProperties.hpp
//                          compiler/ras/Tree.cpp (2 tables)
//                          trj9/codegen/CodeGenGPU.cpp
//                          compiler/optimizer/SimplifierTable.hpp
//                          compiler/optimizer/ValuePropagationTable.hpp
//                          compiler/x/amd64/codegen/TreeEvaluatorTable.cpp
//                          compiler/x/i386/codegen/TreeEvaluatorTable.cpp
//                          compiler/p/codegen/TreeEvaluatorTable.cpp
//                          compiler/z/codegen/TreeEvaluatorTable.cpp
//                          compiler/arm/codegen/TreeEvaluatorTable.cpp
//                          compiler/il/OMRILOpCodesEnum.hpp
//                          compiler/il/ILOpCodes.hpp
// Also check tables in ../codegen/ILOps.hpp


   FirstOMROp,
   BadILOp = 0,  // illegal op hopefully help with uninitialised nodes
   aconst,   // load address constant (zero value means NULL)
   iconst,   // load integer constant (32-bit signed 2's complement)
   lconst,   // load long integer constant (64-bit signed 2's complement)
   fconst,   // load float constant (32-bit ieee fp)
   dconst,   // load double constant (64-bit ieee fp)
   bconst,   // load byte integer constant (8-bit signed 2's complement)
   sconst,   // load short integer constant (16-bit signed 2's complement)
   iload,    // load integer
   fload,    // load float
   dload,    // load double
   aload,    // load address
   bload,    // load byte
   sload,    // load short integer
   lload,    // load long integer
   iloadi,   // load indirect integer
   floadi,   // load indirect float
   dloadi,   // load indirect double
   aloadi,   // load indirect address
   bloadi,   // load indirect byte
   sloadi,   // load indirect short integer
   lloadi,   // load indirect long integer
   istore,   // store integer
   lstore,   // store long integer
   fstore,   // store float
   dstore,   // store double
   astore,   // store address
   wrtbar,   // direct write barrier store checks for new space in old space reference store
                // the first child is the value as in astore.  The second child is
                // the address of the object that must be checked for old space
                // the symbol reference holds addresses, flags and offsets as in astore
   bstore,   // store byte
   sstore,   // store short integer
   lstorei,  // store indirect long integer           (child1 a, child2 l)
   fstorei,  // store indirect float                  (child1 a, child2 f)
   dstorei,  // store indirect double                 (child1 a, child2 d)
   astorei,  // store indirect address                (child1 a dest, child2 a value)
   wrtbari,  // indirect write barrier store checks for new space in old space reference store
                // The first two children are as in astorei.  The third child is address
                // of the beginning of the destination object.  For putfield this will often
                // be the same as the first child (when the offset is on the symbol reference.
                // But for array references, children 1 and 3 will be quite different although
                // child 1's subtree will contain a reference to child 3's subtree
   bstorei,  // store indirect byte                   (child1 a, child2 b)
   sstorei,  // store indirect short integer          (child1 a, child2 s)
   istorei,  // store indirect integer                (child1 a, child2 i)
   Goto,     // goto label address
   ireturn,  // return an integer
   lreturn,  // return a long integer
   freturn,  // return a float
   dreturn,  // return a double
   areturn,  // return an address
   Return,   // void return
   asynccheck,// GC point
   athrow,   // throw an exception
   icall,    // direct call returning integer
   lcall,    // direct call returning long integer
   fcall,    // direct call returning float
   dcall,    // direct call returning double
   acall,    // direct call returning reference
   call,    // direct call returning void
   iadd,     // add 2 integers
   ladd,     // add 2 long integers
   fadd,     // add 2 floats
   dadd,     // add 2 doubles
   badd,     // add 2 bytes
   sadd,     // add 2 short integers
   isub,     // subtract 2 integers                (child1 - child2)
   lsub,     // subtract 2 long integers           (child1 - child2)
   fsub,     // subtract 2 floats                  (child1 - child2)
   dsub,     // subtract 2 doubles                 (child1 - child2)
   bsub,     // subtract 2 bytes                   (child1 - child2)
   ssub,     // subtract 2 short integers          (child1 - child2)
   asub,     // subtract 2 addresses (child1 - child2)
   imul,     // multiply 2 integers
   lmul,     // multiply 2 signed or unsigned long integers
   fmul,     // multiply 2 floats
   dmul,     // multiply 2 doubles
   bmul,     // multiply 2 bytes
   smul,     // multiply 2 short integers
   iumul,    // multiply 2 unsigned integers
   idiv,     // divide 2 integers                (child1 / child2)
   ldiv,     // divide 2 long integers           (child1 / child2)
   fdiv,     // divide 2 floats                  (child1 / child2)
   ddiv,     // divide 2 doubles                 (child1 / child2)
   bdiv,     // divide 2 bytes                   (child1 / child2)
   sdiv,     // divide 2 short integers          (child1 / child2)
   iudiv,    // divide 2 unsigned integers       (child1 / child2)
   ludiv,    // divide 2 unsigned long integers  (child1 / child2)
   irem,     // remainder of 2 integers                (child1 % child2)
   lrem,     // remainder of 2 long integers           (child1 % child2)
   frem,     // remainder of 2 floats                  (child1 % child2)
   drem,     // remainder of 2 doubles                 (child1 % child2)
   brem,     // remainder of 2 bytes                   (child1 % child2)
   srem,     // remainder of 2 short integers          (child1 % child2)
   iurem,    // remainder of 2 unsigned integers       (child1 % child2)
   ineg,     // negate an integer
   lneg,     // negate a long integer
   fneg,     // negate a float
   dneg,     // negate a double
   bneg,     // negate a bytes
   sneg,     // negate a short integer
   iabs,     // absolute value of integer
   labs,     // absolute value of long
   fabs,     // absolute value of float
   dabs,     // absolute value of double
   ishl,     // shift integer left                (child1 << child2)
   lshl,     // shift long integer left           (child1 << child2)
   bshl,     // shift byte left                   (child1 << child2)
   sshl,     // shift short integer left          (child1 << child2)
   ishr,     // shift integer right arithmetically               (child1 >> child2)
   lshr,     // shift long integer right arithmetically          (child1 >> child2)
   bshr,     // shift byte right arithmetically                  (child1 >> child2)
   sshr,     // shift short integer arithmetically               (child1 >> child2)
   iushr,    // shift integer right logically                   (child1 >> child2)
   lushr,    // shift long integer right logically              (child1 >> child2)
   bushr,    // shift byte right logically                      (child1 >> child2)
   sushr,    // shift short integer right logically             (child1 >> child2)
   irol,     // rotate integer left
   lrol,     // rotate long integer left
   iand,     // boolean and of 2 integers
   land,     // boolean and of 2 long integers
   band,     // boolean and of 2 bytes
   sand,     // boolean and of 2 short integers
   ior,      // boolean or of 2 integers
   lor,      // boolean or of 2 long integers
   bor,      // boolean or of 2 bytes
   sor,      // boolean or of 2 short integers
   ixor,     // boolean xor of 2 integers
   lxor,     // boolean xor of 2 long integers
   bxor,     // boolean xor of 2 bytes
   sxor,     // boolean xor of 2 short integers

   i2l,      // convert integer to long integer with sign extension
   i2f,      // convert integer to float
   i2d,      // convert integer to double
   i2b,      // convert integer to byte
   i2s,      // convert integer to short integer
   i2a,      // convert integer to address

   iu2l,     // convert unsigned integer to long integer with zero extension
   iu2f,     // convert unsigned integer to float
   iu2d,     // convert unsigned integer to double
   iu2a,     // convert unsigned integer to address

   l2i,      // convert long integer to integer
   l2f,      // convert long integer to float
   l2d,      // convert long integer to double
   l2b,      // convert long integer to byte
   l2s,      // convert long integer to short integer
   l2a,      // convert long integer to address

   lu2f,     // convert unsigned long integer to float
   lu2d,     // convert unsigned long integer to double
   lu2a,     // convert unsigned long integer to address

   f2i,      // convert float to integer
   f2l,      // convert float to long integer
   f2d,      // convert float to double
   f2b,      // convert float to byte
   f2s,      // convert float to short integer

   d2i,      // convert double to integer
   d2l,      // convert double to long integer
   d2f,      // convert double to float
   d2b,      // convert double to byte
   d2s,      // convert double to short integer

   b2i,      // convert byte to integer with sign extension
   b2l,      // convert byte to long integer with sign extension
   b2f,      // convert byte to float
   b2d,      // convert byte to double
   b2s,      // convert byte to short integer with sign extension
   b2a,      // convert byte to address

   bu2i,     // convert byte to integer with zero extension
   bu2l,     // convert byte to long integer with zero extension
   bu2f,     // convert unsigned byte to float
   bu2d,     // convert unsigned byte to double
   bu2s,     // convert byte to short integer with zero extension
   bu2a,     // convert unsigned byte to unsigned address

   s2i,      // convert short integer to integer with sign extension
   s2l,      // convert short integer to long integer with sign extension
   s2f,      // convert short integer to float
   s2d,      // convert short integer to double
   s2b,      // convert short integer to byte
   s2a,      // convert short integer to address

   su2i,     // zero extend short to int
   su2l,     // zero extend char to long
   su2f,     // convert char to float
   su2d,     // convert char to double
   su2a,     // convert char to address

   a2i,      // convert address to integer
   a2l,      // convert address to long integer
   a2b,      // convert address to byte
   a2s,      // convert address to short
   icmpeq,   // integer compare if equal
   icmpne,   // integer compare if not equal
   icmplt,   // integer compare if less than
   icmpge,   // integer compare if greater than or equal
   icmpgt,   // integer compare if greater than
   icmple,   // integer compare if less than or equal
   iucmpeq,   // unsigned integer compare if equal
   iucmpne,   // unsigned integer compare if not equal
   iucmplt,   // unsigned integer compare if less than
   iucmpge,   // unsigned integer compare if greater than or equal
   iucmpgt,   // unsigned integer compare if greater than
   iucmple,   // unsigned integer compare if less than or equal
   lcmpeq,   // long compare if equal
   lcmpne,   // long compare if not equal
   lcmplt,   // long compare if less than
   lcmpge,   // long compare if greater than or equal
   lcmpgt,   // long compare if greater than
   lcmple,   // long compare if less than or equal
   lucmpeq,   // unsigned long compare if equal
   lucmpne,   // unsigned long compare if not equal
   lucmplt,   // unsigned long compare if less than
   lucmpge,   // unsigned long compare if greater than or equal
   lucmpgt,   // unsigned long compare if greater than
   lucmple,   // unsigned long compare if less than or equal
   fcmpeq,   // float compare if equal
   fcmpne,   // float compare if not equal
   fcmplt,   // float compare if less than
   fcmpge,   // float compare if greater than or equal
   fcmpgt,   // float compare if greater than
   fcmple,   // float compare if less than or equal
   fcmpequ,  // float compare if equal or unordered
   fcmpneu,  // float compare if not equal or unordered
   fcmpltu,  // float compare if less than or unordered
   fcmpgeu,  // float compare if greater than or equal or unordered
   fcmpgtu,  // float compare if greater than or unordered
   fcmpleu,  // float compare if less than or equal or unordered
   dcmpeq,   // double compare if equal
   dcmpne,   // double compare if not equal
   dcmplt,   // double compare if less than
   dcmpge,   // double compare if greater than or equal
   dcmpgt,   // double compare if greater than
   dcmple,   // double compare if less than or equal
   dcmpequ,  // double compare if equal or unordered
   dcmpneu,  // double compare if not equal or unordered
   dcmpltu,  // double compare if less than or unordered
   dcmpgeu,  // double compare if greater than or equal or unordered
   dcmpgtu,  // double compare if greater than or unordered
   dcmpleu,  // double compare if less than or equal or unordered
   acmpeq,   // address compare if equal
   acmpne,   // address compare if not equal
   acmplt,   // address compare if less than
   acmpge,   // address compare if greater than or equal
   acmpgt,   // address compare if greater than
   acmple,   // address compare if less than or equal
   bcmpeq,   // byte compare if equal
   bcmpne,   // byte compare if not equal
   bcmplt,   // byte compare if less than
   bcmpge,   // byte compare if greater than or equal
   bcmpgt,   // byte compare if greater than
   bcmple,   // byte compare if less than or equal
   bucmpeq,  // unsigned byte compare if equal
   bucmpne,  // unsigned byte compare if not equal
   bucmplt,  // unsigned byte compare if less than
   bucmpge,  // unsigned byte compare if greater than or equal
   bucmpgt,  // unsigned byte compare if greater than
   bucmple,  // unsigned byte compare if less than or equal
   scmpeq,   // short integer compare if equal
   scmpne,   // short integer compare if not equal
   scmplt,   // short integer compare if less than
   scmpge,   // short integer compare if greater than or equal
   scmpgt,   // short integer compare if greater than
   scmple,   // short integer compare if less than or equal
   sucmpeq,   // char compare if equal
   sucmpne,   // char compare if not equal
   sucmplt,   // char compare if less than
   sucmpge,   // char compare if greater than or equal
   sucmpgt,   // char compare if greater than
   sucmple,   // char compare if less than or equal
   lcmp,     // long compare (1 if child1 > child2, 0 if child1 == child2, -1 if child1 < child2)
   fcmpl,    // float compare l (1 if child1 > child2, 0 if child1 == child2, -1 if child1 < child2 or unordered)
   fcmpg,    // float compare g (1 if child1 > child2 or unordered, 0 if child1 == child2, -1 if child1 < child2)
   dcmpl,    // double compare l (1 if child1 > child2, 0 if child1 == child2, -1 if child1 < child2 or unordered)
   dcmpg,    // double compare g (1 if child1 > child2 or unordered, 0 if child1 == child2, -1 if child1 < child2)
   ificmpeq, // integer compare and branch if equal
   ificmpne, // integer compare and branch if not equal
   ificmplt, // integer compare and branch if less than
   ificmpge, // integer compare and branch if greater than or equal
   ificmpgt, // integer compare and branch if greater than
   ificmple, // integer compare and branch if less than or equal
   ifiucmpeq, // unsigned integer compare and branch if equal
   ifiucmpne, // unsigned integer compare and branch if not equal
   ifiucmplt, // unsigned integer compare and branch if less than
   ifiucmpge, // unsigned integer compare and branch if greater than or equal
   ifiucmpgt, // unsigned integer compare and branch if greater than
   ifiucmple, // unsigned integer compare and branch if less than or equal
   iflcmpeq, // long compare and branch if equal
   iflcmpne, // long compare and branch if not equal
   iflcmplt, // long compare and branch if less than
   iflcmpge, // long compare and branch if greater than or equal
   iflcmpgt, // long compare and branch if greater than
   iflcmple, // long compare and branch if less than or equal
   iflucmpeq, // unsigned long compare and branch if equal
   iflucmpne, // unsigned long compare and branch if not equal
   iflucmplt, // unsigned long compare and branch if less than
   iflucmpge, // unsigned long compare and branch if greater than or equal
   iflucmpgt, // unsigned long compare and branch if greater than
   iflucmple, // unsigned long compare and branch if less than or equal
   iffcmpeq, // float compare and branch if equal
   iffcmpne, // float compare and branch if not equal
   iffcmplt, // float compare and branch if less than
   iffcmpge, // float compare and branch if greater than or equal
   iffcmpgt, // float compare and branch if greater than
   iffcmple, // float compare and branch if less than or equal
   iffcmpequ,// float compare and branch if equal or unordered
   iffcmpneu,// float compare and branch if not equal or unordered
   iffcmpltu,// float compare and branch if less than or unordered
   iffcmpgeu,// float compare and branch if greater than or equal or unordered
   iffcmpgtu,// float compare and branch if greater than or unordered
   iffcmpleu,// float compare and branch if less than or equal or unordered
   ifdcmpeq, // double compare and branch if equal
   ifdcmpne, // double compare and branch if not equal
   ifdcmplt, // double compare and branch if less than
   ifdcmpge, // double compare and branch if greater than or equal
   ifdcmpgt, // double compare and branch if greater than
   ifdcmple, // double compare and branch if less than or equal
   ifdcmpequ,// double compare and branch if equal or unordered
   ifdcmpneu,// double compare and branch if not equal or unordered
   ifdcmpltu,// double compare and branch if less than or unordered
   ifdcmpgeu,// double compare and branch if greater than or equal or unordered
   ifdcmpgtu,// double compare and branch if greater than or unordered
   ifdcmpleu,// double compare and branch if less than or equal or unordered
   ifacmpeq, // address compare and branch if equal
   ifacmpne, // address compare and branch if not equal
   ifacmplt, // address compare and branch if less than
   ifacmpge, // address compare and branch if greater than or equal
   ifacmpgt, // address compare and branch if greater than
   ifacmple, // address compare and branch if less than or equal
   ifbcmpeq, // byte compare and branch if equal
   ifbcmpne, // byte compare and branch if not equal
   ifbcmplt, // byte compare and branch if less than
   ifbcmpge, // byte compare and branch if greater than or equal
   ifbcmpgt, // byte compare and branch if greater than
   ifbcmple, // byte compare and branch if less than or equal
   ifbucmpeq, // unsigned byte compare and branch if equal
   ifbucmpne, // unsigned byte compare and branch if not equal
   ifbucmplt, // unsigned byte compare and branch if less than
   ifbucmpge, // unsigned byte compare and branch if greater than or equal
   ifbucmpgt, // unsigned byte compare and branch if greater than
   ifbucmple, // unsigned byte compare and branch if less than or equal
   ifscmpeq, // short integer compare and branch if equal
   ifscmpne, // short integer compare and branch if not equal
   ifscmplt, // short integer compare and branch if less than
   ifscmpge, // short integer compare and branch if greater than or equal
   ifscmpgt, // short integer compare and branch if greater than
   ifscmple, // short integer compare and branch if less than or equal
   ifsucmpeq, // char compare and branch if equal
   ifsucmpne, // char compare and branch if not equal
   ifsucmplt, // char compare and branch if less than
   ifsucmpge, // char compare and branch if greater than or equal
   ifsucmpgt, // char compare and branch if greater than
   ifsucmple, // char compare and branch if less than or equal
   loadaddr, // load address of non-heap storage item (Auto, Parm, Static or Method)
   ZEROCHK,  // Zero-check an int.  Symref indicates call to perform when first child is zero.  Other children are arguments to the call.
   callIf,   // Call symref if first child evaluates to true.  Other childrem are arguments to the call.
   iRegLoad,  // Load integer global register
   aRegLoad,  // Load address global register
   lRegLoad,  // Load long integer global register
   fRegLoad,  // Load float global register
   dRegLoad,  // Load double global register
   sRegLoad,  // Load short global register
   bRegLoad,  // Load byte global register
   iRegStore, // Store integer global register
   aRegStore, // Store address global register
   lRegStore, // Store long integer global register
   fRegStore, // Store float global register
   dRegStore, // Store double global register
   sRegStore, // Store short global register
   bRegStore, // Store byte global register
   GlRegDeps, // Global Register Dependency List
   iternary,   // Ternary Operator:  Based on the result of the first child, take the value of the
   lternary,   //   second (first child evaluates to true) or third(first child evaluates to false) child
   bternary,   //
   sternary,   //
   aternary,   //
   fternary,   //
   dternary,   //
   treetop,  // tree top to anchor subtrees with side-effects
   MethodEnterHook, // called after a frame is built, temps initialized, and monitor acquired (if necessary)
   MethodExitHook,  // called immediately before returning, frame not yet collapsed, monitor released (if necessary)
   PassThrough, // Dummy node that represents its single child.
   compressedRefs,   // no-op anchor providing optimizable subexpressions used for compression/decompression.  First child is address load/store, second child is heap base displacement
   BBStart,  // Start of Basic Block
   BBEnd,    // End of Basic Block

   virem,         // vector integer remainder
   vimin,         // vector integer minimum
   vimax,         // vector integer maximum
   vigetelem,     // get vector int element
   visetelem,     // set vector int element
   vimergel,      // vector int merge low
   vimergeh,      // vector int merge high

   vicmpeq,       // vector integer compare equal  (return vector mask)
   vicmpgt,       // vector integer compare greater than
   vicmpge,       // vector integer compare greater equal
   vicmplt,       // vector integer compare less than
   vicmple,       // vector integer compare less equal

   vicmpalleq,       // vector integer all equal (return boolean)
   vicmpallne,       // vector integer all not equal
   vicmpallgt,       // vector integer all greater than
   vicmpallge,       // vector integer all greater equal
   vicmpalllt,       // vector integer all less than
   vicmpallle,       // vector integer all less equal
   vicmpanyeq,       // vector integer any equal
   vicmpanyne,       // vector integer any not equal
   vicmpanygt,       // vector integer any greater than
   vicmpanyge,       // vector integer any greater equal
   vicmpanylt,       // vector integer any less than
   vicmpanyle,       // vector integer any less equal

   vnot,          // vector boolean not
   vselect,       // vector select
   vperm,         // vector permute

   vsplats,       // vector splats
   vdmergel,      // vector double merge low
   vdmergeh,      // vector double merge high
   vdsetelem,     // set vector double element
   vdgetelem,     // get vector double element
   vdsel,         // get vector select double

   vdrem,         // vector double remainder
   vdmadd,        // vector double fused multiply add
   vdnmsub,       // vector double fused negative multiply subtract
   vdmsub,        // vector double fused multiply subtract
   vdmax,         // vector double maximum
   vdmin,         // vector double minimum

   vdcmpeq,       // vector double compare equal  (return vector mask)
   vdcmpne,       // vector double compare not equal  (return vector mask)
   vdcmpgt,       // vector double compare greater than
   vdcmpge,       // vector double compare greater equal
   vdcmplt,       // vector double compare less than
   vdcmple,       // vector double compare less equal

   vdcmpalleq,    // vector double compare all equal  (return boolean)
   vdcmpallne,    // vector double compare all not equal  (return boolean)
   vdcmpallgt,    // vector double compare all greater than
   vdcmpallge,    // vector double compare all greater equal
   vdcmpalllt,    // vector double compare all less than
   vdcmpallle,    // vector double compare all less equal

   vdcmpanyeq,    // vector double compare any equal  (return boolean)
   vdcmpanyne,    // vector double compare any not equal  (return boolean)
   vdcmpanygt,    // vector double compare any greater than
   vdcmpanyge,    // vector double compare any greater equal
   vdcmpanylt,    // vector double compare any less than
   vdcmpanyle,    // vector double compare any less equal
   vdsqrt,        // vector double square root
   vdlog,         // vector double natural log

   /* Begin general vector opcodes */
   vinc,       // vector increment
   vdec,       // vector decrement
   vneg,       // vector negation
   vcom,       // vector complement
   vadd,       // vector add
   vsub,       // vector subtract
   vmul,       // vector multiply
   vdiv,       // vector divide
   vrem,       // vector remainder
   vand,       // vector logical AND
   vor,        // vector logical OR
   vxor,       // vector exclusive OR integer
   vshl,       // vector shift left
   vushr,      // vector shift right logical
   vshr,       // vector shift right arithmetic
   vcmpeq,     // vector compare equal
   vcmpne,     // vector compare not equal
   vcmplt,     // vector compare less than
   vucmplt,    // vector unsigned compare less than
   vcmpgt,     // vector compare greater than
   vucmpgt,    // vector unsigned compare greater than
   vcmple,     // vector compare less or equal
   vucmple,    // vector unsigned compare less or equal
   vcmpge,     // vector compare greater or equal
   vucmpge,    // vector unsigned compare greater or equal
   vload,      // load vector
   vloadi,     // load indirect vector
   vstore,     // store vector
   vstorei,    // store indirect vector
   vrand,      // AND all elements into single value of element size
   vreturn,    // return a vector
   vcall,      // direct call returning a vector
   vcalli,     // indirect call returning a vector
   vternary,   // vector ternary operator
   v2v,        // vector to vector conversion. preserves bit pattern (noop), only changes datatype
   vl2vd,      // vector to vector conversion. converts each long element to double
   vconst,     // vector constant
   getvelem,   // get vector element, returns a scalar
   vsetelem,   // vector set element

   vbRegLoad,     // Load vector global register
   vsRegLoad,     // Load vector global register
   viRegLoad,     // Load vector global register
   vlRegLoad,     // Load vector global register
   vfRegLoad,     // Load vector global register
   vdRegLoad,     // Load vector global register
   vbRegStore,    // Store vector global register
   vsRegStore,    // Store vector global register
   viRegStore,    // Store vector global register
   vlRegStore,    // Store vector global register
   vfRegStore,    // Store vector global register
   vdRegStore,    // Store vector global register

   iuconst,  // load unsigned integer constant (32-but unsigned)
   luconst,  // load unsigned long integer constant (64-bit unsigned)
   buconst,  // load unsigned byte integer constant (8-bit unsigned)
   iuload,   // load unsigned integer
   luload,   // load unsigned long integer
   buload,   // load unsigned byte
   iuloadi,  // load indirect unsigned integer
   luloadi,  // load indirect unsigned long integer
   buloadi,  // load indirect unsigned byte
   iustore,  // store unsigned integer
   lustore,  // store unsigned long integer
   bustore,  // store unsigned byte
   iustorei, // store indirect unsigned integer       (child1 a, child2 i)
   lustorei, // store indirect unsigned long integer  (child1 a, child2 l)
   bustorei, // store indirect unsigned byte          (child1 a, child2 b)
   iureturn, // return an unsigned integer
   lureturn, // return a long unsigned integer
   iucall,   // direct call returning unsigned integer
   lucall,   // direct call returning unsigned long integer
   iuadd,    // add 2 unsigned integers
   luadd,    // add 2 unsigned long integers
   buadd,    // add 2 unsigned bytes
   iusub,    // subtract 2 unsigned integers       (child1 - child2)
   lusub,    // subtract 2 unsigned long integers  (child1 - child2)
   busub,    // subtract 2 unsigned bytes          (child1 - child2)
   iuneg,    // negate an unsigned integer
   luneg,    // negate a unsigned long integer
   iushl,    // shift unsigned integer left       (child1 << child2)
   lushl,    // shift unsigned long integer left  (child1 << child2)
   f2iu,     // convert float to unsigned integer
   f2lu,     // convert float to unsigned long integer
   f2bu,     // convert float to unsigned byte
   f2c,      // convert float to char
   d2iu,     // convert double to unsigned integer
   d2lu,     // convert double to unsigned long integer
   d2bu,     // convert double to unsigned byte
   d2c,      // convert double to char
   iuRegLoad, // Load unsigned integer global register
   luRegLoad, // Load unsigned long integer global register
   iuRegStore,// Store unsigned integer global register
   luRegStore,// Store long integer global register
   iuternary,  // second or the third child.  Analogous to the "condition ? a : b" operations in C/Java.
   luternary,  //
   buternary,  //
   suternary,  //
   cconst,   // load unicode constant (16-bit unsigned)
   cload,    // load short unsigned integer
   cloadi,   // load indirect unsigned short integer
   cstore,   // store unsigned short integer
   cstorei,  // store indirect unsigned short integer (child1 a, child2 c)
   monent,   // acquire lock for synchronising method
   monexit,  // release lock for synchronising method
   monexitfence,     //denotes the end of a monitored region solely for live monitor meta data
   tstart,   // transaction begin
   tfinish,     // transaction end
   tabort,     // transaction abort
   instanceof,// instanceof - symref is the class object, cp index is in the
                // "int" field, child is the object reference
   checkcast,// checkcast
   checkcastAndNULLCHK,// checkcast and NULL check the underlying object reference
   New,      // new - child is class
   newarray, // new array of primitives
   anewarray,// new array of objects
   variableNew,// new - child is class, type not known at compile time
   variableNewArray,// new array - type not known at compile time, type must be a j9class, do not use type enums
   multianewarray,// multi-dimensional new array of objects
   arraylength,// number of elements in an array
   contigarraylength, // number of elements in a contiguous array
   discontigarraylength, // number of elements in a discontiguous array
   icalli,   // indirect call returning integer (child1 is addr of function)
   iucalli,  // indirect call returning unsigned integer (child1 is addr of function)
   lcalli,   // indirect call returning long integer (child1 is addr of function)
   lucalli,  // indirect call returning unsigned long integer (child1 is addr of function)
   fcalli,   // indirect call returning float (child1 is addr of function)
   dcalli,   // indirect call returning double (child1 is addr of function)
   acalli,   // indirect call returning reference
   calli,   // indirect call returning void (child1 is addr of function)
   fence,    // barrier to optimization
   luaddh,   // add 2 unsigned long integers (the high parts of prior luadd) as high part of 128bit addition.
   cadd,     // add 2 unsigned short integers
   aiadd,    // add integer to address with address result (child1 a, child2 i)
   aiuadd,   // add unsigned integer to address with address result (child1 a, child2 i)
   aladd,    // add long integer to address with address result (child1 a, child2 i) (64-bit only)
   aluadd,   // add unsigned long integer to address with address result (child1 a, child2 i) (64-bit only)
   lusubh,   // subtract 2 unsigned long integers (the high parts of prior lusub) as high part of 128bit subtraction.
   csub,     // subtract 2 unsigned short integers (child1 - child2)
   imulh,    // multiply 2 integers, and return the high word of the product
   iumulh,   // multiply 2 unsigned integers, and return the high word of the product
   lmulh,    // multiply 2 long integers, and return the high word of the product
   lumulh,   // multiply 2 unsigned long integers, and return the high word of the product
   ibits2f,  // type-coerce int to float
   fbits2i,  // type-coerce float to int
   lbits2d,  // type-coerce long to double
   dbits2l,  // type-coerce double to long
   lookup,   // lookupswitch (child1 is selector expression, child2 the default destination, subsequent children are case nodes
   trtLookup,   // special lookupswitch (child1 must be trt, child2 the default destination, subsequent children are case nodes)
                // The internal control flow is similar to lookup, but each CASE represents a special semantics associated with a flag on it
   Case,     // case nodes that are children of TR_switch.  Uses the branchdestination and the int const field
   table,    // tableswitch (child1 is the selector, child2 the default destination, subsequent children are the branch targets
             // (the last child may be a branch table address, use getCaseIndexUpperBound() when iterating over branch targets)
   exceptionRangeFence,    // (J9) SymbolReference is the aliasing effect, initialiser is where the code address gets put when binary is generated
                           // used for delimiting function, try blocks, catch clauses, finally clauses, etc.
   dbgFence, // used to delimit code (stmts) for debug info.  Has no symbol reference.
   NULLCHK,  // Null check a pointer.  child 1 is indirect reference. Symbolref indicates failure action/destination
   ResolveCHK, // Resolve check a static, field or method. child 1 is reference to be resolved. Symbolref indicates failure action/destination
   ResolveAndNULLCHK, // Resolve check a static, field or method and Null check the underlying pointer.  child 1 is reference to be resolved. Symbolref indicates failure action/destination
   DIVCHK,   // Divide by zero check. child 1 is the divide. Symbolref indicates failure action/destination
   OverflowCHK, // Overflow check. child 1 is the operation node(add, mul, sub). Child 2 and child 3 are the operands of the operation of the operation. Symbolref indicates failure action/destination
   UnsignedOverflowCHK, // UnsignedOverflow check. child 1 is the operation node(add, mul, sub). Child 2 and child 3 are the operands of the operation of the operation. Symbolref indicates failure action/destination
   BNDCHK,    // Array bounds check, checks that child 1 > child 2 >= 0 (child 1 is bound, 2 is index). Symbolref indicates failure action/destination
   ArrayCopyBNDCHK, // Array copy bounds check, checks that child 1 >= child 2. Symbolref indicates failure action/destination
   BNDCHKwithSpineCHK,  // Array bounds check and spine check
   SpineCHK,   // Check if the base array has a spine
   ArrayStoreCHK,// Array store check. child 1 is object, 2 is array. Symbolref indicates failure action/destination
   ArrayCHK, // Array compatibility check. child 1 is object1, 2 is object2. Symbolref indicates failure action/destination
   Ret,      // Used by ilGen only
   arraycopy,// Call to System.arraycopy that may be partially inlined
   arrayset,// Inline code for memory initialization of part of an array
   arraytranslate,// Inline code for translation of part of an array to another form via lookup
   arraytranslateAndTest,// Inline code for scanning of part of an array for a particular 8-bit character
   long2String, // Convert integer/long value to String
   bitOpMem, // bit operations (AND, OR, XOR) for memory to memory
   bitOpMemND, // 3 operand(source1,source2,target) version of bitOpMem
   arraycmp,// Inline code for memory comparison of part of an array
   arraycmpWithPad, // memory comparison when src1 length != src2 length and padding is needed
   allocationFence,    // Internal fence guarding escape of newObject & final fields - eliminatable
   loadFence,          // JEP171: prohibits loadLoad and loadStore reordering (on globals)
   storeFence,         // JEP171: prohibits loadStore and storeStore reordering (on globals)
   fullFence,          // JEP171: prohibits loadLoad, loadStore, storeLoad, and storeStore reordering (on globals)
   MergeNew, // Parent for New etc. nodes that can all be allocated together
   computeCC,  // compute Condition Codes
   butest,     // zEmulator: mask unsigned byte (UInt8) and set condition codes
   sutest,     // zEmulator: mask unsigned short (UInt16) and set condition codes

   bucmp,      // Currently only valid for zEmulator. Based on the ordering of the two children set the return value:
   bcmp,       //    0 : child1 == child2
   sucmp,      //    1 : child1 < child2
   scmp,       //    2 : child1 > child2
   iucmp,      //
   icmp,       //
   lucmp,      //

   ificmpo,    // integer compare and branch if overflow
   ificmpno,   // integer compare and branch if not overflow
   iflcmpo,    // long compare and branch if overflow
   iflcmpno,   // long compare and branch if not overflow
   ificmno,    // integer compare negative and branch if overflow
   ificmnno,   // integer compare negative and branch if not overflow
   iflcmno,    // long compare negative and branch if overflow
   iflcmnno,   // long compare negative and branch if not overflow

   iuaddc,     // Currently only valid for zEmulator.  Add two unsigned ints with carry
   luaddc,     // Add two longs with carry
   iusubb,     // Subtract two ints with borrow
   lusubb,     // Subtract two longs with borrow

   icmpset,    // icmpset(pointer,c,r): compare *pointer with c, if it matches, replace with r.  Returns 0 on match, 1 otherwise
   lcmpset,    // the operation is done atomically - return type is int for both [il]cmpset
   bztestnset, // bztestnset(pointer,c): atomically sets *pointer to c and returns the original value of *p (represents Test And Set on Z)

   // the atomic ops.. atomically update the symref.  first child is address, second child is the RHS
   // interestingly, these ops act like loads and stores at the same time
   ibatomicor, //
   isatomicor, //
   iiatomicor, //
   ilatomicor, //


   dexp,     // double exponent


   branch,     // generic branch --> DEPRECATED use TR::case instead
   igoto,      // indirect goto, branches to the address specified by a child

   bexp,     // signed byte exponent  (raise signed byte to power)
   buexp,    // unsigned byte exponent
   sexp,     // short exponent
   cexp,     // unsigned short exponent
   iexp,     // integer exponent
   iuexp,    // unsigned integer exponent
   lexp,     // long exponent
   luexp,    // unsigned long exponent
   fexp,     // float exponent
   fuexp,    // float base to unsigned integral exponent
   duexp,    // double base to unsigned integral exponent

   ixfrs,    // transfer sign integer
   lxfrs,    // transfer sign long
   fxfrs,    // transfer sign float
   dxfrs,    // transfer sign double

   fint,     // truncate float to int
   dint,     // truncate double to int
   fnint,    // round float to nearest int
   dnint,    // round double to nearest int

   fsqrt,    // square root of float
   dsqrt,    // square root of double

   getstack, // returns current value of SP
   dealloca, // resets value of SP

   ishfl,    // int shift logical
   lshfl,    // long shift logical
   iushfl,   // unsigned int shift logical
   lushfl,   // unsigned long shift logical
   bshfl,    // byte shift logical
   sshfl,    // short shift logical
   bushfl,   // unsigned byte shift logical
   sushfl,   // unsigned short shift logical

   idoz,     // difference or zero

   dcos,       // cos of double, returning double
   dsin,       // sin of double, returning double
   dtan,       // tan of double, returning double

   dcosh,       // cos of double, returning double
   dsinh,       // sin of double, returning double
   dtanh,       // tan of double, returning double

   dacos,      // arccos of double, returning double
   dasin,      // arcsin of double, returning double
   datan,      // arctan of double, returning double

   datan2,     // arctan2 of double, returning double

   dlog,       // log of double, returning double

   imulover,   // (int) overflow predicate of int multiplication

   dfloor,     // floor of double, returning double
   ffloor,     // floor of float, returning float
   dceil,      // ceil of double, returning double
   fceil,      // ceil of float, returning float
   ibranch,    // generic indirct branch --> first child is a constant indicating the mask
   mbranch,    // generic branch to multiple potential targets
   getpm,      // get program mask
   setpm,      // set program mask
   loadAutoOffset, // loads the offset (from the SP) of an auto

   imax,       // max of 2 or more integers
   iumax,      // max of 2 or more unsigned integers
   lmax,       // max of 2 or more longs
   lumax,      // max of 2 or more unsigned longs
   fmax,       // max of 2 or more floats
   dmax,       // max of 2 or more doubles

   imin,       // min of 2 or more integers
   iumin,      // min of 2 or more unsigned integers
   lmin,       // min of 2 or more longs
   lumin,      // min of 2 or more unsigned longs
   fmin,       // min of 2 or more floats
   dmin,       // min of 2 or more doubles

   trt,           // translate and test
   trtSimple,     // same as TRT but ignoring the returned source byte address and table entry value


   ihbit,
   ilbit,
   inolz,
   inotz,
   ipopcnt,

   lhbit,
   llbit,
   lnolz,
   lnotz,
   lpopcnt,

   ibyteswap,// swap bytes in an integer

   bbitpermute,
   sbitpermute,
   ibitpermute,
   lbitpermute,

   Prefetch, // Prefetch


   LastOMROp = Prefetch,

#endif
