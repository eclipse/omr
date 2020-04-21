/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
 * This file will be included within an enum.  Only comments and enumerator
 * definitions are permitted.
 */

   /* Pseudo Instructions */

   ASSOCREGS,           // Register Association
   BAD,                 // Bad Opcode
   BREAK,               // Breakpoint (debugger)
   DC,                  // DC
   DC2,                 // DC2
   DCB,                 // Debug Counter Bump
   DEPEND,              // Someplace to hang dependencies
   FENCE,               // Fence
   LABEL,               // Destination of a jump
   LHHR,                // Load (High <- High)
   LHLR,                // Load (High <- Low)
   LLCHHR,              // Load Logical Character (High <- High)
   LLCHLR,              // Load Logical Character (High <- low)
   LLCLHR,              // Load Logical Character (Low <- High)
   LLHFR,               // Load (Low <- High)
   LLHHHR,              // Load Logical Halfword (High <- High)
   LLHHLR,              // Load Logical Halfword (High <- low)
   LLHLHR,              // Load Logical Halfword (Low <- High)
   NHHR,                // AND High (High <- High)
   NHLR,                // AND High (High <- Low)
   NLHR,                // AND High (Low <- High)
   OHHR,                // OR High (High <- High)
   OHLR,                // OR High (High <- Low)
   OLHR,                // OR High (Low <- High)
   PROC,                // Entry to the method
   RET,                 // Return
   SLLHH,               // Shift Left Logical (High <- High)
   SLLLH,               // Shift Left Logical (Low <- High)
   SRLHH,               // Shift Right Logical (High <- High)
   SRLLH,               // Shift Right Logical (Low <- High)
   VGNOP,               // ValueGuardNOP
   XHHR,                // Exclusive OR High (High <- High)
   XHLR,                // Exclusive OR High (High <- Low)
   XLHR,                // Exclusive OR High (Low <- High)

   /* z900 Instructions */

   A,                   // Add
   AD,                  // Add Normalized, Long
   ADB,                 // Add (LB)
   ADBR,                // Add (LB)
   ADR,                 // Add Normalized, Long
   AE,                  // Add Normalized, Short
   AEB,                 // Add (SB)
   AEBR,                // Add (SB)
   AER,                 // Add Normalized, Short
   AGFR,                // Add (64 < 32)
   AGHI,                // Add Halfword Immediate
   AGR,                 // Add (64)
   AH,                  // Add Halfword
   AHI,                 // Add Halfword Immediate
   AL,                  // Add Logical
   ALCGR,               // Add Logical with Carry (64)
   ALCR,                // Add Logical with Carry (32)
   ALGFR,               // Add Logical (64 < 32)
   ALGR,                // Add Logical (64)
   ALR,                 // Add Logical (32)
   AP,                  // Add Decimal
   AR,                  // Add (32)
   AU,                  // Add Unnormalized, Short
   AUR,                 // Add Unnormalized, Short
   AW,                  // Add Unnormalized, Long
   AWR,                 // Add Unnormalized, Long
   AXBR,                // Add (EB)
   AXR,                 // Add Normalized, Extended
   BAKR,                // Branch and Stack
   BAL,                 // Branch and Link
   BALR,                // Branch and Link
   BAS,                 // Branch and Save
   BASR,                // Branch and Save
   BASSM,               // Branch and Save and Set Mode
   BC,                  // Branch on Cond.
   BCR,                 // Branch on Cond.
   BCT,                 // Branch on Count (32)
   BCTGR,               // Branch on Count (64)
   BCTR,                // Branch on Count (32)
   BRAS,                // Branch and Save
   BRASL,               // Branch Rel.and Save Long
   BRC,                 // Branch Rel. on Cond.
   BRCL,                // Branch Rel. on Cond. Long
   BRCT,                // Branch Rel. on Count (32)
   BRCTG,               // Branch Relative on Count (64)
   BRXH,                // Branch Rel. on Idx High
   BRXHG,               // Branch Relative on Index High
   BRXLE,               // Branch Rel. on Idx Low or Equal (32)
   BRXLG,               // Branch Relative on Index Equal or Low
   BSM,                 // Branch and Set Mode
   BXH,                 // Branch on Idx High
   BXLE,                // Branch on Idx Low or Equal (32)
   C,                   // Compare (32)
   CD,                  // Compare, Long
   CDB,                 // Compare (LB)
   CDBR,                // Compare (LB)
   CDFBR,               // Convert from Fixed (LB < 32)
   CDFR,                // Convert from int32 to long HFP
   CDGBR,               // Convert from Fixed (LB < 64)
   CDGR,                // Convert from int64 to long HFP
   CDR,                 // Compare, Long
   CDS,                 // Compare Double and Swap
   CE,                  // Compare, Short
   CEB,                 // Compare (SB)
   CEBR,                // Compare (SB)
   CEFBR,               // Convert from Fixed (SB < 32)
   CEFR,                // Convert from int32 to short HFP
   CEGBR,               // Convert from Fixed (SB < 64)
   CEGR,                // Convert from int64 to short HFP
   CER,                 // Compare, Short
   CFC,                 // Compare and Form CodeWord
   CFDBR,               // Convert to Fixed (LB < 32)
   CFDR,                // Convert long HFP to int32
   CFEBR,               // Convert to Fixed (SB < 32)
   CFER,                // Convert short HFP to int32
   CFXBR,               // Convert to Fixed (EB < 32), note here
   CFXR,                // Convert long HFP to int32
   CGDBR,               // Convert to Fixed (64 < LB)
   CGDR,                // Convert long HFP to int64
   CGEBR,               // Convert to Fixed (64 < SB)
   CGER,                // Convert short HFP to int64
   CGFR,                // Compare (64 < 32)
   CGHI,                // Compare Halfword Immediate (64)
   CGR,                 // Compare (64)
   CGXBR,               // Convert to Fixed (EB < 64), note here
   CGXR,                // Convert long HFP to int64
   CH,                  // Compare Halfword
   CHI,                 // Compare Halfword Immediate (32)
   CKSM,                // Checksum
   CL,                  // Compare Logical (32)
   CLC,                 // Compare Logical (character)
   CLCL,                // Compare Logical Long
   CLCLE,               // Compare Logical Long Extended
   CLGFR,               // Compare Logical (64 < 32)
   CLGR,                // Compare Logical (64)
   CLI,                 // Compare Logical Immediate
   CLM,                 // Compare Logical Characters under Mask
   CLR,                 // Compare Logical (32)
   CLST,                // Compare Logical String
   CP,                  // Compare Decimal
   CPYA,                // Copy Access Register
   CR,                  // Compare (32)
   CS,                  // Compare and Swap (32)
   CSCH,                // Clear Subchannel
   CUSE,                // Compare Until Substring Equal
   CVB,                 // Convert to Binary
   CVD,                 // Convert to Decimal (32)
   CXBR,                // Compare (EB)
   CXFBR,               // Convert from Fixed (EB < 32)
   CXFR,                // Convert from int32 to extended HFP
   CXGBR,               // Convert from Fixed (EB < 64)
   CXGR,                // Convert from int64 to extended HFP
   CXR,                 // Compare, Extended
   D,                   // Divide (32 < 64)
   DD,                  // Divide, Long HFP
   DDB,                 // Divide (LB)
   DDBR,                // Divide (LB)
   DDR,                 // Divide, Long HFP
   DE,                  // Divide, short HFP
   DEB,                 // Divide (SB)
   DEBR,                // Divide (SB)
   DER,                 // Divide, Short HFP
   DIDBR,               // Divide to Integer (LB)
   DIEBR,               // Divide to Integer (SB)
   DLGR,                // Divide Logical
   DLR,                 // Divide
   DP,                  // Divide Decimal
   DR,                  // Divide
   DSGFR,               // Divide Single (64 < 32)
   DSGR,                // Divide Single (64)
   DXBR,                // Divide (EB)
   DXR,                 // Divide, extended HFP
   EAR,                 // Extract Access Register
   ED,                  // Edit
   EDMK,                // Edit and Mark
   EFPC,                // Extract FPC
   EPAR,                // Extract Primary ASN
   EPSW,                // Extract PSW
   EREG,                // Extract Stacked Registers
   EREGG,               // Extract Stacked Registers
   ESAR,                // Extract Secondary ASN
   ESEA,                // Extract and Set Extended Authority
   ESTA,                // Extract Stacked State
   EX,                  // Execute
   FIDBR,               // Load FP Integer (LB)
   FIDR,                // Load long HFP Integer (round toward 0)
   FIEBR,               // Load FP Integer (SB)
   FIER,                // Load short HFP Integer (round toward 0)
   FIXBR,               // Load FP Integer (EB)
   FIXR,                // Load extended HFP Integer (round toward 0)
   HDR,                 // Halve, Long HFP
   HER,                 // Halve, short HFP
   HSCH,                // Halt Subchannel
   IAC,                 // Insert Address Space Control
   IC,                  // Insert Character
   ICM,                 // Insert Character under Mask
   IIHH,                // Insert Immediate
   IIHL,                // Insert Immediate
   IILH,                // Insert Immediate
   IILL,                // Insert Immediate
   IPK,                 // Insert PSW Key
   IPM,                 // Insert Program mask
   ISKE,                // Insert Storage Key Extended
   IVSK,                // Insert Virtual Storage Key
   L,                   // Load (32)
   LA,                  // Load Address
   LAE,                 // Load Address Extended
   LAM,                 // Load Access Multiple
   LARL,                // Load Address Relative Long
   LCDBR,               // Load Complement (LB)
   LCDR,                // Load Complement, Long HFP
   LCEBR,               // Load Complement (SB)
   LCER,                // Load Complement, short HFP
   LCGFR,               // Load Complement (64 < 32)
   LCGR,                // Load Complement (64)
   LCR,                 // Load Complement (32)
   LCTL,                // Load Control
   LCXBR,               // Load Complement (EB)
   LCXR,                // Load Complement, extended HFP
   LD,                  // Load (L)
   LDE,                 // Load Lengthened short to long HFP
   LDEB,                // Load Lengthened (LB < SB)
   LDEBR,               // Load Leeeengthened (LB < SB)
   LDER,                // Load Rounded, long to short HFP
   LDR,                 // Load (L)
   LDXBR,               // Load Rounded (LB < EB)
   LDXR,                // Load Rounded, extended to long HFP
   LE,                  // Load (S)
   LEDBR,               // Load Rounded (SB < LB)
   LEDR,                // Load Rounded, long to short HFP
   LER,                 // Load (S)
   LEXBR,               // Load Rounded (SB < EB)
   LEXR,                // Load Rounded, extended to short HFP
   LFPC,                // Load FPC
   LGFR,                // (LongDisp) Load (64 < 32)
   LGHI,                // Load Halfword Immediate
   LGR,                 // Load (64)
   LH,                  // Load Halfword
   LHI,                 // Load Halfword Immediate
   LLGFR,               // Load Logical Halfword(64)
   LLGTR,               // Load Logical Thirty One Bits
   LLIHH,               // Load Logical Halfword Immediate
   LLIHL,               // Load Logical Halfword Immediate
   LLILH,               // Load Logical Halfword Immediate
   LLILL,               // Load Logical Halfword Immediate
   LM,                  // Load Multiple
   LNDBR,               // Load Negative (LB)
   LNDR,                // Load Negative, Long HFP
   LNEBR,               // Load Negative (SB)
   LNER,                // Load Negative, short HFP
   LNGFR,               // Load Negative
   LNGR,                // Load Negative (64)
   LNR,                 // Load Negative (32)
   LNXBR,               // Load Negative (EB)
   LNXR,                // Load Negative, extended HFP
   LPDBR,               // Load Positive (LB)
   LPDR,                // Load Positive, Long HFP
   LPEBR,               // Load Positive (SB)
   LPER,                // Load Positive, short HFP
   LPGFR,               // Load Positive (64 < 32)
   LPGR,                // Load Positive (64)
   LPR,                 // Load Positive (32)
   LPSW,                // Load PSW
   LPSWE,               // Load PSW Extended
   LPXBR,               // Load Positive (EB)
   LPXR,                // Load Positive, extended HFP
   LR,                  // Load (32)
   LRA,                 // Load Real Address
   LRVGR,               // Load Reversed (64)
   LRVR,                // Load Reversed (32)
   LTDBR,               // Load and Test (LB)
   LTDR,                // Load and Test, Long HFP
   LTEBR,               // Load and Test (SB)
   LTER,                // Load and Test, Short HFP
   LTGFR,               // Load and Test (64 < 32)
   LTGR,                // Load and Test (64)
   LTR,                 // Load and Test (32)
   LTXBR,               // Load and Test (DFP64)
   LTXR,                // Load and Test, extended HFP
   LURA,                // Load Using Real Address
   LURAG,               // Load Using Real Address
   LXD,                 // Load Lengthened short to long HFP
   LXDB,                // Load Lengthened (EB < LB)
   LXDBR,               // Load Leeeengthened (EB < DB)
   LXDR,                // Load Lengthened, long to extended HFP
   LXE,                 // Load Lengthened short to long HFP
   LXEB,                // Load Lengthened (EB < SB)
   LXEBR,               // Load Leeeengthened (EB < SB)
   LXER,                // Load Rounded, short to extended HFP
   LXR,                 // Load Leeeengthened (EB < SB)
   LZDR,                // Load Zero (L)
   LZER,                // Load Zero (S)
   LZXR,                // Load Zero (EB)
   M,                   // Multiply (64 < 32)
   MADB,                // Multiply and Add (LB)
   MADBR,               // Multiply and Add (LB)
   MAEB,                // Multiply and Add (SB)
   MAEBR,               // Multiply and Add (SB)
   MD,                  // Multiply, long HFP source and result
   MDB,                 // Multiply (LB)
   MDBR,                // Multiply (LB)
   MDE,                 // Multiply, short HFP source, long HFP result
   MDER,                // Multiply, short HFP source, long HFP result
   MDR,                 // Multiply, long HFP source and result
   MEE,                 // Multiply short HFP
   MEEB,                // Multiply (SB)
   MEEBR,               // Multiply (SB)
   MEER,                // Multiply, short HFP source and result
   MGHI,                // Multiply Halfword Immediate
   MH,                  // Multiply Halfword (32)
   MHI,                 // Multiply Halfword Immediate (32)
   MLGR,                // Multiply Logical ( 128<64 )
   MLR,                 // Multiply Logical ( 64<32 )
   MP,                  // Multiple Decimal
   MR,                  // Multiple (64 < 32)
   MS,                  // Multiply Single
   MSCH,                // Modify Subchannel
   MSDB,                // Multiply and Subtract (LB)
   MSDBR,               // Multiply and Subtract (LB)
   MSEB,                // Multiply and Subtract (SB)
   MSEBR,               // Multiply and Subtract (SB)
   MSGFR,               // Multiply Single (64 < 32)
   MSGR,                // Multiply Single (64)
   MSR,                 // Multiply Single Register
   MSTA,                // Modify Stacked State
   MVC,                 // Move (character)
   MVCDK,               // Move With Destination Key
   MVCK,                // Move With Key
   MVCL,                // Move Long
   MVCLE,               // Move Long Extended
   MVCP,                // Move to Primary
   MVCS,                // Move to Secondary
   MVCSK,               // Move With Source Key
   MVI,                 // Move (Immediate)
   MVN,                 // Move Numerics
   MVO,                 // Move With Offset
   MVST,                // Move String
   MVZ,                 // Move Zones
   MXBR,                // Multiply (EB)
   MXD,                 // Multiply, long HFP source, extended HFP result
   MXDR,                // Multiply, long HFP source, extended HFP result
   MXR,                 // Multiply, extended HFP source and result
   N,                   // And (32)
   NC,                  // And (character)
   NGR,                 // And (64)
   NI,                  // And (Immediate)
   NIHH,                // And Immediate (high high)
   NIHL,                // And Immediate (high low)
   NILH,                // And Immediate (low high)
   NILL,                // And Immediate (low low)
   NOP,                 // No-Op (for Labels)
   NR,                  // And (32)
   O,                   // Or (32)
   OC,                  // Or (character)
   OGR,                 // Or (64)
   OI,                  // Or (Immediate)
   OIHH,                // Or Immediate (high high)
   OIHL,                // Or Immediate (high low)
   OILH,                // Or Immediate (low high)
   OILL,                // Or Immediate (low low)
   OR,                  // Or (32)
   PACK,                // Pack
   PALB,                // Purge ALB
   PC,                  // Program Call
   PKA,                 // Pack ASCII
   PKU,                 // Pack Unicode
   PLO,                 // Perform Locked Operation
   PR,                  // Program Return
   PT,                  // Program Transfer
   RCHP,                // Reset Channel Path
   RSCH,                // Resume Subchannel
   S,                   // Subtract (32)
   SAC,                 // Set Address Control
   SAL,                 // Set Address Limit
   SAM24,               // Set 24 bit addressing mode
   SAM31,               // Set 31 bit addressing mode
   SAM64,               // Set 64 bit addressing mode
   SAR,                 // Set Access Register
   SCHM,                // Set Channel Monitor
   SCK,                 // Set Clock
   SCKC,                // Set Clock Comparator
   SD,                  // Subtract Normalized,long HFP
   SDB,                 // Subtract (LB)
   SDBR,                // Subtract (LB)
   SDR,                 // Subtract Normalized,long HFP
   SE,                  // Subtract Normalized,short HFP
   SEB,                 // Subtract (SB)
   SEBR,                // Subtract (SB)
   SER,                 // Subtract Normalized,short HFP
   SFPC,                // Set FPC
   SGFR,                // Subtract (64 < 32)
   SGR,                 // Subtract (64)
   SH,                  // Subtract Halfword
   SIGP,                // Signal Processor
   SL,                  // Subtract Logical (32)
   SLA,                 // Shift Left Single (32)
   SLBGR,               // Subtract Logical With Borrow (64)
   SLBR,                // Subtract Logical With Borrow (32)
   SLDA,                // Shift Left Double
   SLDL,                // Shift Left Double Logical
   SLGFR,               // Subtract Logical (64 < 32)
   SLGR,                // Subtract Logical (64)
   SLL,                 // Shift Left Single Logical
   SLR,                 // Subtract Logical (32)
   SP,                  // Subtract Decimal
   SPKA,                // Set PSW Key From Address
   SPM,                 // Set Program Mask
   SPT,                 // Set CPU Timer
   SPX,                 // Set Prefix
   SQD,                 // Square Root Long HFP
   SQDB,                // Square Root Long BFP
   SQDBR,               // Square Root Long BFP
   SQDR,                // Square Root Long HFP
   SQE,                 // Square Root short HFP
   SQEB,                // Square Root Long BFP
   SQEBR,               // Square Root Long BFP
   SQER,                // Square Root short HFP
   SQXBR,               // Square Root (EB)
   SQXR,                // Square Root extended HFP
   SR,                  // Subtract (32)
   SRA,                 // Shift Right Single (32)
   SRDA,                // Shift Right Double
   SRDL,                // Shift Right Double Logical
   SRL,                 // Shift Right Single Logical
   SRNM,                // Set BFP Rounding Mode
   SRP,                 // Shift and Round Decimal
   SRST,                // Search String
   SSAR,                // Set Secondary ASN
   SSCH,                // Start Subchannel
   SSM,                 // Set System Mask
   ST,                  // Store (32)
   STAM,                // Set Access Multiple
   STAP,                // Store CPU Address
   STC,                 // Store Character
   STCK,                // Store Clock
   STCKC,               // Store Clock Comparator
   STCKE,               // Store Clock Extended
   STCM,                // Store Character under Mask (low)
   STCPS,               // Store Channel Path Status
   STCRW,               // Store Channel Report Word
   STCTL,               // Store Control
   STD,                 // Store (S)
   STE,                 // Store (L)
   STFPC,               // Store FPC
   STH,                 // Store Halfword
   STIDP,               // Store CPU ID
   STM,                 // Store Multiple (32)
   STNSM,               // Store Then and System Mask
   STOSM,               // Store Then or System Mask
   STPT,                // Store CPU Timer
   STPX,                // Store Prefix
   STRAG,               // Store Real Address
   STSCH,               // Store Subchannel
   STURA,               // Store Using Real Address
   STURG,               // Store Using Real Address
   SU,                  // Subtract Unnormalized,short HFP
   SUR,                 // Subtract Unnormalized,short HFP
   SVC,                 // Supervisor Call
   SW,                  // Subtract Unnormalized,long HFP
   SWR,                 // Subtract Unnormalized,long HFP
   SXBR,                // Subtract (EB)
   SXR,                 // Subtract Normalized, extended HFP
   TAM,                 // Test addressing mode
   TAR,                 // Test Access
   TBDR,                // Convert HFP to BFP,long HFP,long BFP
   TBEDR,               // Convert HFP to BFP,long HFP,short BFP
   TCDB,                // Test Data Class (LB)
   TCEB,                // Test Data Class (SB)
   TCXB,                // Test Data Class (EB)
   THDER,               // Convert BFP to HFP,short BFP,long HFP
   THDR,                // Convert BFP to HFP,long BFP,long HFP
   TM,                  // Test under Mask
   TMH,                 // Test under Mask
   TMHH,                // Test under Mask
   TMHL,                // Test under Mask
   TML,                 // Test under Mask
   TMLH,                // Test under Mask
   TMLL,                // Test under Mask
   TP,                  // Test Decimal
   TPI,                 // Test Pending Interruption
   TPROT,               // Test Protection
   TR,                  // Translate
   TRE,                 // Translate Extended
   TRT,                 // Translate and Test
   TS,                  // Test and Set
   TSCH,                // Test Subchannel
   UNPK,                // Unpack
   UNPKA,               // Unpack ASCII
   UNPKU,               // Unpack Unicode
   UPT,                 // Update Tree
   X,                   // Exclusive Or (32)
   XC,                  // Exclusive Or (character)
   XGR,                 // Exclusive Or (64)
   XI,                  // Exclusive Or (immediate)
   XR,                  // Exclusive Or (32)
   ZAP,                 // Zero and Add

   /* z990 Instructions */

   AG,                  // (LongDisp) Add (64)
   AGF,                 // (LongDisp) Add (64 < 32)
   AHY,                 // (LongDisp) Add Halfword
   ALC,                 // (LongDisp) Add Logical with Carry (32)
   ALCG,                // (LongDisp) Add Logical with Carry (64)
   ALG,                 // (LongDisp) Add Logical (64)
   ALGF,                // (LongDisp) Add Logical (64 < 32)
   ALY,                 // (LongDisp) Add Logical
   AY,                  // (LongDisp) Add
   BCTG,                // (LongDisp) Branch on Count (64)
   BXHG,                // (LongDisp) Branch on Index High
   BXLEG,               // (LongDisp) Branch on Index Low or Equ. (64)
   CDSG,                // Compare Double and Swap
   CDSY,                // Compare Double and Swap
   CG,                  // (LongDisp) Compare (64)
   CGF,                 // (LongDisp) Compare (64 < 32)
   CHY,                 // (LongDisp) Compare Halfword
   CLCLU,               // Compare Logical Long Unicode
   CLG,                 // (LongDisp) Compare Logical (64)
   CLGF,                // (LongDisp) Compare Logical (64 < 32)
   CLIY,                // Compare Logical Immediate
   CLMH,                // Compare Logical Characters under Mask High
   CLMY,                // Compare Logical Characters under Mask Y Form
   CLY,                 // (LongDisp) Compare Logical (32)
   CSG,                 // (LongDisp) Compare and Swap (64)
   CSY,                 // (LongDisp) Compare and Swap (32)
   CVBG,                // Convert to Binary
   CVBY,                // (LongDisp) Convert to Binary
   CVDG,                // Convert to Decimal (64)
   CVDY,                // (LongDisp) Convert to Decimal (32)
   CY,                  // (LongDisp) Compare (32)
   DL,                  // (LongDisp) Divide
   DSG,                 // (LongDisp) Divide Single (64)
   DSGF,                // (LongDisp) Divide Single (64 < 32)
   ICMH,                // (LongDisp) Insert Characters under Mask (high)
   ICMY,                // (LongDisp) Insert Character under Mask
   ICY,                 // (LongDisp) Insert Character
   KIMD,                // Compute Intermediate Message Digest
   KLMD,                // Compute Last Message Digest
   KM,                  // Cipher Message
   KMAC,                // Compute Message Authentication Code
   KMC,                 // Cipher Message with Chaining
   LAMY,                // (LongDisp) Load Access Multiple
   LAY,                 // (LongDisp) Load Address
   LB,                  // (LongDisp) Load Byte (31) - note it is called LB in the PoP
   LCTLG,               // Load Control
   LDY,                 // (LongDisp) Load (L)
   LEY,                 // (LongDisp) Load (S)
   LG,                  // (LongDisp) Load (64)
   LGB,                 // (LongDisp) Load Byte (64)
   LGF,                 // (LongDisp) Load (64 < 32)
   LGH,                 // (LongDisp) Load Halfword
   LHY,                 // (LongDisp)Load Halfword
   LLGC,                // (LongDisp) Load Logical Character
   LLGF,                // (LongDisp) Load Logical Halfword
   LLGH,                // (LongDisp) Load Logical Halfword
   LLGT,                // (LongDisp) Load Logical Thirty One Bits
   LMG,                 // (LongDisp) Load Multiple (64)
   LMH,                 // Load Multiple High
   LMY,                 // (LongDisp) Load Multiple
   LPQ,                 // (LongDisp) Load Pair from Quadword
   LRAG,                // Load Real Address
   LRAY,                // Load Real Address Y Form
   LRV,                 // (LongDisp) Load Reversed (32)
   LRVG,                // (LongDisp) Load Reversed (64)
   LRVH,                // (LongDisp) Load Reversed Halfword
   LY,                  // (LongDisp) Load (32)
   MAD,                 // Multiply and Add, long HFP sources and result
   MADR,                // Multiply and Add, long HFP sources and result
   MAE,                 // Multiply and Add, short HFP sources and result
   MAER,                // Multiply and Add, short HFP sources and result
   MLG,                 // (LongDisp) Multiply Logical ( 128<64 )
   MSD,                 // Multiply and Subtract, long HFP sources and result
   MSDR,                // Multiply and Subtract, long HFP sources and result
   MSE,                 // Multiply and Subtract, short HFP sources and result
   MSER,                // Multiply and Subtract, short HFP sources and result
   MSG,                 // (LongDisp) Multiply Single (64)
   MSGF,                // (LongDisp) Multiply Single (64 < 32)
   MSY,                 // (LongDisp) Multiply Single
   MVCLU,               // Move Long Unicode
   MVIY,                // (LongDisp) Move (Immediate)
   NG,                  // (LongDisp) And (64)
   NIY,                 // (LongDisp) And (Immediate)
   NY,                  // (LongDisp) And (32)
   OG,                  // (LongDisp) Or (64)
   OIY,                 // (LongDisp) Or (Immediate)
   OY,                  // (LongDisp) Or (32)
   RLL,                 // Rotate Left Single Logical (32)
   RLLG,                // Rotate Left Single Logical (64)
   SG,                  // (LongDisp) Subtract (64)
   SGF,                 // (LongDisp) Subtract (64 < 32)
   SHY,                 // (LongDisp) Subtract Halfword
   SLAG,                // (LongDisp) Shift Left Single (64)
   SLB,                 // (LongDisp) Subtract Logical with Borrow (32)
   SLBG,                // (LongDisp) Subtract Logical With Borrow (64)
   SLG,                 // (LongDisp) Subtract Logical (64)
   SLGF,                // (LongDisp) Subtract Logical (64 < 32)
   SLLG,                // (LongDisp) Shift Left Logical (64)
   SLY,                 // (LongDisp) Subtract Logical (32)
   SRAG,                // (LongDisp) Shift Right Single (64)
   SRLG,                // (LongDisp) Shift Right Logical (64)
   STAMY,               // (LongDisp) Set Access Multiple
   STCMH,               // (LongDisp) Store Characters under Mask (high)
   STCMY,               // (LongDisp) Store Characters under Mask (low)
   STCTG,               // Store Control
   STCY,                // (LongDisp) Store Character
   STDY,                // (LongDisp) Store (S)
   STEY,                // (LongDisp) Store (L)
   STG,                 // (LongDisp) Store (64)
   STHY,                // (LongDisp) Store Halfword
   STMG,                // (LongDisp) Store Multiple (64)
   STMH,                // Store Multiple High
   STMY,                // (LongDisp) Store Multiple
   STPQ,                // (LongDisp) Store Pair to Quadword
   STRV,                // (LongDisp) Store (32)
   STRVG,               // (LongDisp) Store Reversed (64)
   STRVH,               // (LongDisp) Store Reversed Halfword
   STY,                 // (LongDisp) Store (32)
   SY,                  // (LongDisp) Subtract (32)
   TMY,                 // Test under Mask
   XG,                  // (LongDisp) Exclusive Or (64)
   XIY,                 // (LongDisp) Exclusive Or (immediate)
   XY,                  // (LongDisp) Exclusive Or (32)

   /* z9 Instructions */

   ADTR,                // Add (DFP64)
   AFI,                 // Add Immediate (32)
   AGFI,                // Add Immediate (64<32)
   ALFI,                // Add Logical Immediate (32)
   ALGFI,               // Add Logical Immediate (64<32)
   AXTR,                // Add (DFP128)
   CDGTR,               // Convert from Fixed (DFP64)
   CDSTR,               // Convert from Signed Packed
   CDTR,                // Compare (DFP64)
   CDUTR,               // Convert From Unsigned BCD (DFP64)
   CEDTR,               // Compare biased exponent (DFP64)
   CEXTR,               // Compare biased exponent (DFP128)
   CFI,                 // Compare Immediate (32)
   CGDTR,               // Convert to Fixed (DFP64)
   CGFI,                // Compare Immediate (64<32)
   CGXTR,               // Convert to Fixed (DFP128)
   CLFI,                // Compare Logical Immediate (32)
   CLGFI,               // Compare Logical Immediate (64<32)
   CPSDR,               // Copy Sign
   CSDTR,               // Convert to signed packed
   CSXTR,               // Convert to signed packed(DFP128)
   CU14,                // Convert UTF-8 to UTF-32
   CU24,                // Convert UTF-16 to UTF-32
   CU41,                // Convert UTF-32 to UTF-8
   CU42,                // Convert UTF-32 to UTF-16
   CUDTR,               // Convert to Unsigned BCD (DFP64)
   CUTFU,               // Convert UTF-8 to Unicode
   CUUTF,               // Convert Unicode to UTF-8
   CUXTR,               // Convert to Unsigned BCD (DFP64)
   CXGTR,               // Convert from Fixed (DFP128)
   CXSTR,               // Convert from Signed Packed to DFP128
   CXTR,                // Compare (DFP128)
   CXUTR,               // Convert From Unsigned BCD (DFP128)
   DDTR,                // Divide (DFP64)
   DXTR,                // Divide (DFP128)
   EEDTR,               // Extract Biased Exponent (DFP64)
   EEXTR,               // Extract Biased Exponent (DFP128)
   ESDTR,               // Extract Significance (DFP64)
   ESXTR,               // Extract Significance (DFP128)
   FIDTR,               // Load FP Integer (DFP64)
   FIXTR,               // Load FP Integer (DFP128)
   FLOGR,               // Find Leftmost One
   IEDTR,               // Insert Biased Exponent (DFP64)
   IEXTR,               // Insert Biased Exponent (DFP128)
   IIHF,                // Insert Immediate (high)
   IILF,                // Insert Immediate (low)
   KDTR,                // Compare (DFP64)
   KXTR,                // Compare (DFP128)
   LBR,                 // Load Byte (32)
   LCDFR,               // Load Complement (DFP64)
   LDETR,               // Load Lengthened (64DFP < 32DFP)
   LDGR,                // Load FPR from GR (SB, DB)
   LDXTR,               // Load Rounded (64DFP < 128DFP)
   LEDTR,               // Load Rounded (32DFP < 64DFP)
   LGBR,                // Load Byte (64)
   LGDR,                // Load GR from FPR (SB, DB)
   LGFI,                // Load Immediate (64<32)
   LGHR,                // Load Halfword (64)
   LHR,                 // Load Halfword (32)
   LLC,                 // (LongDisp) Load Logical Character (32)
   LLCR,                // Load Logical Character (32)
   LLGCR,               // Load Logical Character (64)
   LLGHR,               // Load Logical Halfword(64)
   LLH,                 // (LongDisp) Load Logical Halfword
   LLHR,                // Load Logical Halfword(32)
   LLIHF,               // Load Logical Immediate (high)
   LLILF,               // Load Logical Immediate (low)
   LNDFR,               // Load Negative (DFP64)
   LPDFR,               // Load Positive (DFP64)
   LT,                  // (LongDisp) Load and Test (32)
   LTDTR,               // Load and Test (DFP64)
   LTG,                 // (LongDisp) Load and Test (64)
   LTXTR,               // Load and Test (DFP128)
   LXDTR,               // Load Lengthened(64DFP < 128DFP)
   MAY,                 // Multiply and Add Unnormalized, long HFP sources and extended HFP result
   MAYH,                // Multiply and Add Unnormalized, long HFP sources and high-order part of extended HFP result
   MAYHR,               // Multiply and Add Unnormalized, long HFP sources and high-order part of extended HFP result
   MAYL,                // Multiply and Add Unnormalized, long HFP sources and low-order part of extended HFP result
   MAYLR,               // Multiply and Add Unnormalized, long HFP sources and low-order part of extended HFP result
   MAYR,                // Multiply and Add Unnormalized, long HFP sources and extended HFP result
   MDTR,                // Multiply (DFP64)
   MXTR,                // Multiply (DFP128)
   MY,                  // Multiply Unnormalized, long HFP sources and extended HFP result
   MYH,                 // Multiply Unnormalized, long HFP sources and high-order part of extended HFP result
   MYHR,                // Multiply Unnormalized, long HFP sources and high-order part of extended HFP result
   MYL,                 // Multiply Unnormalized, long HFP sources and low-order part of extended HFP result
   MYLR,                // Multiply Unnormalized, long HFP sources and low-order part of extended HFP result
   MYR,                 // Multiply Unnormalized, long HFP sources and extended HFP result
   NIHF,                // And Immediate (high)
   NILF,                // And Immediate (low)
   OIHF,                // Or Immediate (high)
   OILF,                // Or Immediate (low)
   PFPO,                // perform floating point operations.
   QADTR,               // Quantize (DFP64)
   QAXTR,               // Quantize (DFP128)
   RRDTR,               // Reround (DFP64)
   RRXTR,               // Reround (DFP128)
   SDTR,                // Subtract (DFP64)
   SFASR,               // Set FPC And Signal
   SLDT,                // Shift Left Double (DFP64)
   SLFI,                // Subtract Logical Immediate (32)
   SLGFI,               // Subtract Logical Immediate (64<32)
   SLXT,                // Shift Left Double (DFP128)
   SRDT,                // Shift Right Double (DFP64)
   SRNMT,               // Set RoundingMode (DFP64)
   SRSTU,               // Search String Unicode
   SRXT,                // Shift Right LongDouble (DFP128)
   SSKE,                // Set Storage Key Extended
   STCKF,               // Store Clock Fast
   SXTR,                // Subtract (DFP128)
   TDCDT,               // Test Data Class (DFP64)
   TDCET,               // Test Data Class (DFP32)
   TDCXT,               // Test Data Class (DFP64)
   TDGDT,               // Test Data Group (DFP64)
   TDGET,               // Test Data Group (DFP32)
   TDGXT,               // Test Data Group (DFP128)
   TROO,                // Translate One to One
   TROT,                // Translate One to Two
   TRTO,                // Translate Two to One
   TRTR,                // Translate and Test Reverse
   TRTT,                // Translate Two to Two
   XIHF,                // Exclusive Or Immediate (high)
   XILF,                // Exclusive Or Immediate (low)

   /* z10 Instructions */

   AGSI,                // Add Direct to Memory (64)
   ALGSI,               // Add Logical Direct to Memory (64)
   ALSI,                // Add Logical Direct to Memory
   ASI,                 // Add Direct to Memory
   CGFRL,               // Compare Relative Long (32 < 64)
   CGHRL,               // Compare Halfword Relative Long (64)
   CGHSI,               // Compare Direct to Memory Halfword Immediate (64)
   CGIB,                // Compare Immediate And Branch (64)
   CGIJ,                // Compare Immediate And Branch Relative (64)
   CGIT,                // Compare Immidiate And Trap (64)
   CGRB,                // Compare And Branch (64)
   CGRJ,                // Compare And Branch Relative (64)
   CGRL,                // Compare Relative Long (64)
   CGRT,                // Compare And Trap (64)
   CHHSI,               // Compare Direct to Memory Halfword Immediate (16)
   CHRL,                // Compare Halfword Relative Long (32)
   CHSI,                // Compare Direct to Memory Halfword Immediate (32)
   CIB,                 // Compare Immediate And Branch (32)
   CIJ,                 // Compare Immediate And Branch Relative(32)
   CIT,                 // Compare Immidiate And Trap (32)
   CLFHSI,              // Compare Logical Immediate (32)
   CLFIT,               // Compare Logical Immidiate And Trap (32)
   CLGFRL,              // Compare Logical Relative Long (32 < 64)
   CLGHRL,              // Compare Logical Relative Long Halfword (64)
   CLGHSI,              // Compare Logical Immediate (64)
   CLGIB,               // Compare Logical Immediate And Branch (64)
   CLGIJ,               // Compare Logical Immediate And Branch Relative (64)
   CLGIT,               // Compare Logical Immidiate And Trap (64)
   CLGRB,               // Compare Logical And Branch (64)
   CLGRJ,               // Compare Logical And Branch Relative (64)
   CLGRL,               // Compare Logical Relative Long (64)
   CLGRT,               // Compare Logical And Trap (64)
   CLHHSI,              // Compare Logical Immediate (16)
   CLHRL,               // Compare Logical Relative Long Halfword (32)
   CLIB,                // Compare Logical Immediate And Branch (32)
   CLIJ,                // Compare Logical Immidiate And Branch Relative (32)
   CLRB,                // Compare Logical And Branch (32)
   CLRJ,                // Compare Logical And Branch Relative (32)
   CLRL,                // Compare Logical Relative Long
   CLRT,                // Compare Logical And Trap (32)
   CRB,                 // Compare And Branch (32)
   CRJ,                 // Compare And Branch Relative (32)
   CRL,                 // Compare Relative Long
   CRT,                 // Compare And Trap (32)
   ECAG,                // Extract Cache Attribute
   EXRL,                // Execute Relative Long
   LAEY,                // Load Address Extended Y Form
   LGFRL,               // Load Relative Long (64 < 32)
   LGRL,                // Load Relative Long (64)
   LLGFRL,              // Load Logical Relative Long (64 < 32)
   LRL,                 // Load Relative Long (32)
   LTGF,                // (LongDisp) Load and Test (64 < 32)
   MC,                  // Monitor Call
   MFY,                 // Multiply (64 < 32)
   MHY,                 // Multiply Halfword (32)
   MSFI,                // Multiply Single Immediate
   MSGFI,               // Multiply Single Immediate
   MVGHI,               // Move and store immediate (64)
   MVHHI,               // Move and store immediate (16)
   MVHI,                // Move and store immediate (32)
   PFD,                 // Prefetch Data
   PFDRL,               // Prefetch Data Relative Long
   RISBG,               // Rotate Then Insert Selected Bits
   RNSBG,               // Rotate Then AND Selected Bits
   ROSBG,               // Rotate Then OR Selected Bits
   RXSBG,               // Rotate Then XOR Selected Bits
   STGRL,               // Store Relative Long (64)
   STRL,                // Store Relative Long (32)
   TRTE,                // Translate and Test Extended
   TRTRE,               // Translate and Test Reversed Extended

   /* z196 Instructions */

   AGHIK,               // Add Immediate (64 < 16)
   AGRK,                // Add (32)
   AHHHR,               // Add High (32)
   AHHLR,               // Add High (32)
   AHIK,                // Add Immediate (32 < 16)
   AIH,                 // Add Immediate High (32)
   ALGHSIK,             // Add Logicial With Signed Immediate (64 < 16)
   ALGRK,               // Add Logical (64)
   ALHHHR,              // Add Logical High (32)
   ALHHLR,              // Add Logical High (32)
   ALHSIK,              // Add Logicial With Signed Immediate (32 < 16)
   ALRK,                // Add Logical (32)
   ALSIH,               // Add Logical with Signed Immediate High (32)
   ALSIHN,              // Add Logical with Signed Immediate High (32)
   ARK,                 // Add (32)
   BRCTH,               // Branch Rel. on Count High (32)
   CDLFBR,              // Convert from Logical (LB < 32)
   CDLGBR,              // Convert from Logical (LB < 64)
   CELFBR,              // Convert from Logical (SB < 32)
   CELGBR,              // Convert from Logical (SB < 64)
   CHF,                 // (LongDisp) Compare High (32)
   CHHR,                // Compare High (32)
   CHLR,                // Compare High (32)
   CIH,                 // Compare Immediate High (32)
   CLFDBR,              // Convert to Logical (LB < 32)
   CLFEBR,              // Convert to Logical (SB < 32)
   CLFXBR,              // Convert to Logical (EB < 32), note here
   CLGDBR,              // Convert to Logical (LB < 64)
   CLGEBR,              // Convert to Logical (SB < 64)
   CLGXBR,              // Convert to Logical (EB < 64), note here
   CLHF,                // (LongDisp) Compare Logical High (32)
   CLHHR,               // Compare Logical High (32)
   CLHLR,               // Compare Logical High (32)
   CLIH,                // Compare Logical Immediate High (32)
   CXLFBR,              // Convert from Logical (EB < 32)
   CXLGBR,              // Convert from Logical (EB < 64)
   KMCTR,               // Cipher Message with Counter
   KMF,                 // Cipher Message with CFB (Cipher Feedback)
   KMO,                 // Cipher Message with OFB (Output Feedback)
   LAA,                 // Load And Add (32)
   LAAG,                // Load And Add (64)
   LAAL,                // Load And Add Logical (32)
   LAALG,               // Load And Add Logical (64)
   LAN,                 // (LongDisp) Load And AND (32)
   LANG,                // (LongDisp) Load And AND (64)
   LAO,                 // (LongDisp) Load And OR (32)
   LAOG,                // (LongDisp) Load And OR (64)
   LAX,                 // (LongDisp) Load And Exclusive OR (32)
   LAXG,                // (LongDisp) Load And Exclusive OR (64)
   LBH,                 // (LongDisp) Load Byte High (32 < 8)
   LFH,                 // (LongDisp) Load High (32)
   LHH,                 // (LongDisp) Load Halfword High (32 < 16)
   LLCH,                // (Long Disp)Load Logical Character High (32 < 8)
   LLHH,                // (LongDisp) Load Logical Halfword High (32 < 8)
   LOC,                 // (LongDisp) Load On Condition (32)
   LOCG,                // (LongDisp) Load On Condition (64)
   LOCGR,               // Load On Condition (64)
   LOCR,                // Load On Condition (32)
   LPD,                 // Load Pair Disjoint (32)
   LPDG,                // Load Pair Disjoint (64)
   NGRK,                // And (64)
   NRK,                 // And (32)
   OGRK,                // Or (64)
   ORK,                 // Or (32)
   POPCNT,              // Population Count
   RISBHG,              // Rotate Then Insert Selected Bits High
   RISBLG,              // Rotate Then Insert Selected Bits Low
   SGRK,                // Subtract (64)
   SHHHR,               // Subtract High (32)
   SHHLR,               // Subtract High (32)
   SLAK,                // (LongDisp) Shift Left Single (32)
   SLGRK,               // Subtract (64)
   SLHHHR,              // Subtract High (32)
   SLHHLR,              // Subtract High (32)
   SLLK,                // (LongDisp) Shift Left Logical (32)
   SLRK,                // Subtract (32)
   SRAK,                // (LongDisp) Shift Right Single (32)
   SRK,                 // Subtract (32)
   SRLK,                // (LongDisp) Shift Right Logical (32)
   STCH,                // (LongDisp) Store Character High (8)
   STFH,                // (LongDisp) Store High (32)
   STHH,                // (LongDisp) Store Halfword High (16)
   STOC,                // (LongDisp) Store On Condition (32)
   STOCG,               // (LongDisp) Store On Condition (64)
   XGRK,                // Exclusive Or (64)
   XRK,                 // Exclusive Or (32)

   /* zEC12 Instructions */

   BPP,                 // Branch Prediction Preload
   BPRP,                // Branch Prediction Relative Preload
   CDZT,                // Convert Zoned to DFP Long
   CLGT,                // Compare Logical and Trap (64)
   CLT,                 // Compare Logical and Trap (32)
   CXZT,                // Convert Zoned to DFP Extended
   CZDT,                // Convert DFP Long to Zoned
   CZXT,                // Convert DFP Extended to Zoned
   ETND,                // Extract Transaction Nesting Depth
   LAT,                 // Load and Trap
   LFHAT,               // Load High and Trap
   LGAT,                // Load and Trap (64)
   LLGFAT,              // Load Logical and Trap
   LLGTAT,              // Load Logical Thirty One Bits and Trap
   NIAI,                // Next Instruction Access Intent
   NTSTG,               // Nontransactional Store
   PPA,                 // Perform Processor Assist
   RIEMIT,              // Runtime Instrumentation Emit
   RINEXT,              // Runtime Instrumentation Next
   RIOFF,               // Runtime Instrumentation Off
   RION,                // Runtime Instrumentation On
   RISBGN,              // Rotate Then Insert Selected Bits
   TABORT,              // Transaction Abort
   TBEGIN,              // Transaction Begin
   TBEGINC,             // Constrained Transaction Begin
   TEND,                // Transaction End

   /* z13 Instructions */

   CDPT,                // Convert Packed to DFP Long
   CPDT,                // Convert DFP Long to Packed
   CPXT,                // Convert DFP Extended to Packed
   CXPT,                // Convert Packed to DFP Extended
   LCBB,                // Load Count To Block Boundary
   LLZRGF,              // Load Logical and Zero Rightmost Byte
   LOCFH,               // (LongDisp) Load High On Condition
   LOCFHR,              // Load High On Condition
   LOCGHI,              // Load Halfword Immediate On Condition (64)
   LOCHHI,              // Load Halfword High Immediate On Condition
   LOCHI,               // Load Halfword Immediate On Condition (32)
   LZRF,                // Load and Zero Rightmost Byte
   LZRG,                // Load and Zero Rightmost Byte
   PRNO,                // perform random number operation
   STOCFH,              // (LongDisp) Store High On Condition
   VA,                  // vector add
   VAC,                 // vector add with carry
   VACC,                // vector add compute carry
   VACCC,               // vector add with carry compute carry
   VAVG,                // vector average
   VAVGL,               // vector average logical
   VCDG,                // vector floating-point convert from fixed 64-bit
   VCDLG,               // vector floating-point convert from logical 64-bit
   VCEQ,                // vector comp are equal CC Set
   VCGD,                // vector floating-point convert to fixed 64-bit
   VCH,                 // vector comp are high CC Set
   VCHL,                // vector comp are high logical CC Set
   VCKSM,               // vector checksum
   VCLGD,               // vector floating-point convert to logical 64-bit
   VCLZ,                // vector count leading zeros
   VCTZ,                // vector count trailing zeros
   VEC,                 // vector element comp are CC Set
   VECL,                // vector element comp are logical CC Set
   VERIM,               // vector element rotate and insert under mask
   VERLL,               // vector element rotate left logical
   VERLLV,              // vector element rotate left logical
   VESL,                // vector element shift left
   VESLV,               // vector element shift left
   VESRA,               // vector element shift right arithmetic
   VESRAV,              // vector element shift right arithmetic
   VESRL,               // vector element shift right logical
   VESRLV,              // vector element shift right logical
   VFA,                 // vector floating-point add
   VFAE,                // vector find any element equal CC Set* (*: If CS bit != 0)
   VFCE,                // vector floating-point comp are equal CC Set*
   VFCH,                // vector floating-point comp are high CC Set*
   VFCHE,               // vector floating-point comp are high or equal CC Set*
   VFD,                 // vector floating-point divide
   VFEE,                // vector find element equal CC Set*
   VFENE,               // vector find element not equal CC Set*
   VFI,                 // vector load floating-point integer
   VFM,                 // vector floating-point multiply
   VFMA,                // vector floating-point multiply and add
   VFMS,                // vector floating-point multiply and subtract
   VFPSO,               // vector floating-point perform sign operation
   VFS,                 // vector floating-point subtract
   VFSQ,                // vector floating-point square root
   VFTCI,               // vector floating-point test data class immediate CC Set
   VGBM,                // vector generate byte mask
   VGEF,                // vector gather element (32)
   VGEG,                // vector gather element (64)
   VGFM,                // vector galois field multiply sum
   VGFMA,               // vector galois field multiply sum and accumulate
   VGM,                 // vector generate mask
   VISTR,               // vector isolate string CC Set*
   VL,                  // vector load
   VLBB,                // vector load to block boundary
   VLC,                 // vector load complement
   VLDE,                // vector floating-point load lengthened
   VLEB,                // vector load element (8)
   VLED,                // vector floating-point load rounded
   VLEF,                // vector load element (32)
   VLEG,                // vector load element (64)
   VLEH,                // vector load element (16)
   VLEIB,               // vector load element immediate (8)
   VLEIF,               // vector load element immediate (32)
   VLEIG,               // vector load element immediate (64)
   VLEIH,               // vector load element immediate (16)
   VLGV,                // vector load gr from vr element
   VLL,                 // vector load with length
   VLLEZ,               // vector load logical element and zero
   VLM,                 // vector load multiple
   VLP,                 // vector load positive
   VLR,                 // vector load
   VLREP,               // vector load and replicate
   VLVG,                // vector load vr element from gr
   VLVGP,               // vector load vr from grs disjoint
   VMAE,                // vector multiply and add even
   VMAH,                // vector multiply and add high
   VMAL,                // vector multiply and add low
   VMALE,               // vector multiply and add logical even
   VMALH,               // vector multiply and add logical high
   VMALO,               // vector multiply and add logical odd
   VMAO,                // vector multiply and add odd
   VME,                 // vector multiply even
   VMH,                 // vector multiply high
   VML,                 // vector multiply low
   VMLE,                // vector multiply logical even
   VMLH,                // vector multiply logical high
   VMLO,                // vector multiply logical odd
   VMN,                 // vector minimum
   VMNL,                // vector minimum logical
   VMO,                 // vector multiply odd
   VMRH,                // vector merge high
   VMRL,                // vector merge low
   VMX,                 // vector maximum
   VMXL,                // vector maximum logical
   VN,                  // vector and
   VNC,                 // vector and with complement
   VNO,                 // vector nor
   VO,                  // vector or
   VPDI,                // vector permute doubleword immediate
   VPERM,               // vector permute
   VPK,                 // vector pack
   VPKLS,               // vector pack logical saturate CC Set
   VPKS,                // vector pack saturate CC Set
   VPOPCT,              // vector population count
   VREP,                // vector replicate
   VREPI,               // vector replicate immediate
   VS,                  // vector subtract
   VSBCBI,              // vector subtract with borrow compute borrow indication I
   VSBI,                // vector subtract with borrow indication I
   VSCBI,               // vector subtract compute borrow indication I
   VSCEF,               // vector scatter element (32)
   VSCEG,               // vector scatter element (64)
   VSEG,                // vector sign extend to doubleword
   VSEL,                // vector select
   VSL,                 // vector shift left
   VSLB,                // vector shift left by byte
   VSLDB,               // vector shift left double by byte
   VSRA,                // vector shift right arithmetic
   VSRAB,               // vector shift right arithmetic by byte
   VSRL,                // vector shift right logical
   VSRLB,               // vector shift right logical by byte
   VST,                 // vector store
   VSTEB,               // vector store element (8)
   VSTEF,               // vector store element (32)
   VSTEG,               // vector store element (64)
   VSTEH,               // vector store element (16)
   VSTL,                // vector store with length
   VSTM,                // vector store multiple
   VSTRC,               // vector string range compare CC Set*
   VSUM,                // vector sum across word
   VSUMG,               // vector sum across doubleword
   VSUMQ,               // vector sum across quadword
   VTM,                 // vector test under mask CC Set
   VUPH,                // vector unpack high
   VUPL,                // vector unpack low
   VUPLH,               // vector unpack logical high
   VUPLL,               // vector unpack logical low
   VX,                  // vector exclusive or
   WFC,                 // vector floating-point comp are scalar CC Set
   WFK,                 // vector floating-point comp are and signal scalar CC Set

   /* z14 Instructions */

   AGH,                 // add halfword (64 <- 16)
   BIC,                 // branch indirect on condition
   KMA,                 // Cipher Message with Authentication
   LGG,                 // load guarded
   LGSC,                // load guarded storage controls
   LLGFSG,              // load logical and shift guarded (64 <- 32)
   MG,                  // multiply (128 <- 64)
   MGH,                 // multiply halfword (64 <- 16)
   MGRK,                // multiply (128 <- 64)
   MSC,                 // multiply single (32)
   MSGC,                // multiply single (64)
   MSGRKC,              // multiply single (64)
   MSRKC,               // multiply single (32)
   SGH,                 // subtract halfword (64 <- 16)
   STGSC,               // store guarded storage controls
   VAP,                 // vector Add Decimal
   VBPERM,              // vector bit permute
   VCP,                 // vector Compare decimal
   VCVB,                // vector convert to binary
   VCVBG,               // vector convert to binary
   VCVD,                // vector convert to decimal
   VCVDG,               // vector convert to decmial
   VDP,                 // vector divide decimal
   VFLL,                // vector FP load lengthened
   VFLR,                // vector FP load rounded
   VFMAX,               // vector FP maximum
   VFMIN,               // vector FP minimum
   VFNMA,               // vector FP negative multiply and add
   VFNMS,               // vector FP negative multiply and subtract
   VLIP,                // vector load immediate decimal
   VLRL,                // vector Load Rightmost with Length(Length is immediate value)
   VLRLR,               // vector Load Rightmost with Length(Length is in register)
   VMP,                 // vector multiply decimal
   VMSL,                // vector multiply sum logical
   VMSP,                // vector multiply and shift decimal
   VNN,                 // vector NAND
   VNX,                 // vector not exclusive OR
   VOC,                 // vector OR with complement
   VPKZ,                // vector pack zoned
   VPSOP,               // vector perform sign operation decimal
   VRP,                 // vector remainder decimal
   VSDP,                // vector shift and divide decimal
   VSP,                 // vector subtract decimal
   VSRP,                // vector shift and round decimal
   VSTRL,               // vector Store Rightmost with Length(Length is immediate value)
   VSTRLR,              // vector Store Rightmost with Length(Length is in register)
   VTP,                 // vector test decimal
   VUPKZ,               // vector unpack zoned

   /* z15 Instructions */
   MVCRL,               // move right to left
   NCGRK,               // and with complement (64)
   NCRK,                // and with complement (32)
   NNGRK,               // nand (64)
   NNRK,                // nand (32)
   NOGRK,               // nor (64)
   NORK,                // nor (32)
   NXGRK,               // not exclusive or (64)
   NXRK,                // not exclusive or (32)
   OCGRK,               // or with complement (64)
   OCRK,                // or with complement (32)
   SELGR,               // select (64)
   SELHHHR,             // select high (32)
   SELR,                // select (32)
   VLBR,                // vector load byte reversed elements
   VLBRREP,             // vector load byte reversed element and replicate
   VLEBRF,              // vector load byte reversed element (32)
   VLEBRG,              // vector load byte reversed element (64)
   VLEBRH,              // vector load byte reversed element (16)   
   VLER,                // vector load elements reversed
   VLLEBRZ,             // vector load byte reversed element and zero
   VSTBR,               // vector store byte reversed elements
   VSTEBRF,             // vector store byte reversed element (32)
   VSTEBRG,             // vector store byte reversed element (64)
   VSTEBRH,             // vector store byte reversed element (16)
   VSTER,               // vector store elements reversed
   VSLD,                // vector shift left double by bit
   VSRD,                // vector shift right double by bit
   VSTRS,               // vector string search

   S390LastOp = VSTRS,
