/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

/*
 * This file will be included within a static table.  Only comments and
 * definitions are permitted.
 */


   "BAD",   // Bad Opcode
   "A",  // Add
   "ADB",   // Add (LB)
   "ADBR",  // Add (LB)
   "ADTR",  // Add (DFP64)
   "AEB",   // Add (SB)
   "AEBR",  // Add (SB)
   "AFI",   // Add Immediate (32)
   "AG",    // (LongDisp) Add (64)
   "AGF",   // (LongDisp) Add (64 < 32)
   "AGFI",  // Add Immediate (64<32)
   "AGFR",  // Add (64 < 32)
   "AGHI",  // Add Halfword Immediate
   "AGHIK",    // Add Immediate (64 < 16)
   "AGR",   // Add (64)
   "AGRK",  // Add (32)
   "AH",    // Add Halfword
   "AHHHR",    // Add High (32)
   "AHHLR",    // Add High (32)
   "AHI",   // Add Halfword Immediate
   "AHIK",  // Add Immediate (32 < 16)
   "AHY",   // (LongDisp) Add Halfword
   "AIH",   // Add Immediate High (32)
   "AL",    // Add Logical
   "ALC",   // (LongDisp) Add Logical with Carry (32)
   "ALCG",  // (LongDisp) Add Logical with Carry (64)
   "ALCGR",    // Add Logical with Carry (64)
   "ALCR",  // Add Logical with Carry (32)
   "ALFI",  // Add Logical Immediate (32)
   "ALG",   // (LongDisp) Add Logical (64)
   "ALGF",  // (LongDisp) Add Logical (64 < 32)
   "ALGFI",    // Add Logical Immediate (64<32)
   "ALGFR",    // Add Logical (64 < 32)
   "ALGHSIK",  // Add Logicial With Signed Immediate (64 < 16)
   "ALGR",  // Add Logical (64)
   "ALGRK",    // Add Logical (64)
   "ALHHHR",   // Add Logical High (32)
   "ALHHLR",   // Add Logical High (32)
   "ALHSIK",   // Add Logicial With Signed Immediate (32 < 16)
   "ALR",   // Add Logical (32)
   "ALRK",  // Add Logical (32)
   "ALSIH",    // Add Logical with Signed Immediate High (32)
   "ALSIHN",   // Add Logical with Signed Immediate High (32)
   "ALY",   // (LongDisp) Add Logical
   "AR",    // Add (32)
   "ARK",   // Add (32)
   "AXTR",  // Add (DFP128)
   "AY",    // (LongDisp) Add
   "BAL",   // Branch and Link
   "BALR",  // Branch and Link
   "BAS",   // Branch and Save
   "BASR",  // Branch and Save
   "BC",    // Branch on Cond.
   "BCR",   // Branch on Cond.
   "BCT",   // Branch on Count (32)
   "BCTG",  // (LongDisp) Branch on Count (64)
   "BCTGR",    // Branch on Count (64)
   "BCTR",  // Branch on Count (32)
   "BPP",   // Branch Prediction Preload
   "BPRP",  // Branch Prediction Relative Preload
   "BRAS",  // Branch and Save
   "BRASL",    // Branch Rel.and Save Long
   "BRC",   // Branch Rel. on Cond.
   "BRCL",  // Branch Rel. on Cond. Long
   "BRCT",  // Branch Rel. on Count (32)
   "BRCTG",    // Branch Relative on Count (64)
   "BRCTH",    // Branch Rel. on Count High (32)
   "BRXH",  // Branch Rel. on Idx High
   "BRXHG",    // Branch Relative on Index High
   "BRXLE",    // Branch Rel. on Idx Low or Equal (32)
   "BRXLG",    // Branch Relative on Index Equal or Low
   "BXH",   // Branch on Idx High
   "BXHG",  // (LongDisp) Branch on Index High
   "BXLE",  // Branch on Idx Low or Equal (32)
   "BXLEG",    // (LongDisp) Branch on Index Low or Equ. (64)
   "C",  // Compare (32)
   "CDB",   // Compare (LB)
   "CDBR",  // Compare (LB)
   "CDFBR",    // Convert from Fixed (LB < 32)
   "CDGBR",    // Convert from Fixed (LB < 64)
   "CDGTR",    // Convert from Fixed (DFP64)
   "CDLFBR",   // Convert from Logical (LB < 32)
   "CDLGBR",   // Convert from Logical (LB < 64)
   "CDTR",  // Compare (DFP64)
   "CDSTR",    // Convert from Signed Packed
   "CDUTR",    // Convert From Unsigned BCD (DFP64)
   "CEB",   // Compare (SB)
   "CEBR",  // Compare (SB)
   "CEFBR",    // Convert from Fixed (SB < 32)
   "CEGBR",    // Convert from Fixed (SB < 64)
   "CEDTR",    // Compare biased exponent (DFP64)
   "CELFBR",   // Convert from Logical (SB < 32)
   "CELGBR",   // Convert from Logical (SB < 64)
   "CEXTR",    // Compare biased exponent (DFP128)
   "CFDBR",    // Convert to Fixed (LB < 32)
   "CFEBR",    // Convert to Fixed (SB < 32)
   "CFI",   // Compare Immediate (32)
   "CG",    // (LongDisp) Compare (64)
   "CGDBR",    // Convert to Fixed (64 < LB)
   "CGDTR",    // Convert to Fixed (DFP64)
   "CGEBR",    // Convert to Fixed (64 < SB)
   "CGF",   // (LongDisp) Compare (64 < 32)
   "CGFI",  // Compare Immediate (64<32)
   "CGFR",  // Compare (64 < 32)
   "CGFRB",    // Compare and Branch (64-32)
   "CGFRJ",    // Compare and Branch Relative (64-32)
   "CGFRT",    // Compare and Trap (64-32)
   "CGHI",  // Compare Halfword Immediate (64)
   "CGIB",  // Compare Immediate And Branch (64)
   "CGIJ",  // Compare Immediate And Branch Relative (64)
   "CGIT",  // Compare Immidiate And Trap (64)
   "CGR",   // Compare (64)
   "CGRB",  // Compare And Branch (64)
   "CGRJ",  // Compare And Branch Relative (64)
   "CGRT",  // Compare And Trap (64)
   "CGXTR",    // Convert to Fixed (DFP128)
   "CH",    // Compare Halfword
   "CHF",   // (LongDisp) Compare High (32)
   "CHHR",  // Compare High (32)
   "CHI",   // Compare Halfword Immediate (32)
   "CHLR",  // Compare High (32)
   "CHY",   // (LongDisp) Compare Halfword
   "CIB",   // Compare Immediate And Branch (32)
   "CIH",   // Compare Immediate High (32)
   "CIJ",   // Compare Immediate And Branch Relative(32)
   "CIT",   // Compare Immidiate And Trap (32)
   "CL",    // Compare Logical (32)
   "CLC",   // Compare Logical (character)
   "CLCL",  // Compare Logical Long
   "CLCLE",    // Compare Logical Long Extended
   "CLCLU",    // Compare Logical Long Unicode
   "CLFDBR",   // Convert to Logical (LB < 32)
   "CLFEBR",   // Convert to Logical (SB < 32)
   "CLFI",  // Compare Logical Immediate (32)
   "CLFIT",    // Compare Logical Immidiate And Trap (32)
   "CLG",   // (LongDisp) Compare Logical (64)
   "CLGDBR",   // Convert to Logical (LB < 64)
   "CLGEBR",   // Convert to Logical (SB < 64)
   "CLGF",  // (LongDisp) Compare Logical (64 < 32)
   "CLGFI",    // Compare Logical Immediate (64<32)
   "CLGFR",    // Compare Logical (64 < 32)
   "CLGFRB",   // Compare Logical And Branch (64-32)
   "CLGFRJ",   // Compare Logical And Branch Relative (64-32)
   "CLGFRT",   // Compare Logical And Trap (64-32)
   "CLGIB",    // Compare Logical Immediate And Branch (64)
   "CLGIJ",    // Compare Logical Immediate And Branch Relative (64)
   "CLGIT",    // Compare Logical Immidiate And Trap (64)
   "CLGR",  // Compare Logical (64)
   "CLGRB",    // Compare Logical And Branch (64)
   "CLGRJ",    // Compare Logical And Branch Relative (64)
   "CLGRT",    // Compare Logical And Trap (64)
   "CLGT",  // Compare Logical and Trap (64)
   "CLHF",  // (LongDisp) Compare Logical High (32)
   "CLHHR",    // Compare Logical High (32)
   "CLHLR",    // Compare Logical High (32)
   "CLIY",  // Compare Logical Immediate
   "CLI",   // Compare Logical Immediate
   "CLIB",  // Compare Logical Immediate And Branch (32)
   "CLIH",  // Compare Logical Immediate High (32)
   "CLIJ",  // Compare Logical Immidiate And Branch Relative (32)
   "CLM",   // Compare Logical Characters under Mask
   "CLMH",  // Compare Logical Characters under Mask High
   "CLMY",  // Compare Logical Characters under Mask Y Form
   "CLR",   // Compare Logical (32)
   "CLRB",  // Compare Logical And Branch (32)
   "CLRJ",  // Compare Logical And Branch Relative (32)
   "CLRT",  // Compare Logical And Trap (32)
   "CLT",   // Compare Logical and Trap (32)
   "CLY",   // (LongDisp) Compare Logical (32)
   "CPYA",  // Copy Access Register
   "CR",    // Compare (32)
   "CRB",   // Compare And Branch (32)
   "CRJ",   // Compare And Branch Relative (32)
   "CRT",   // Compare And Trap (32)
   "CS",    // Compare and Swap (32)
   "CSY",   // (LongDisp) Compare and Swap (32)
   "CSG",   // (LongDisp) Compare and Swap (64)
   "CSDTR",    // Convert to signed packed
   "CSXTR",    // Convert to signed packed(DFP128)
   "CUDTR",    // Convert to Unsigned BCD (DFP64)
   "CUXTR",    // Convert to Unsigned BCD (DFP64)
   "CVD",   // Convert to Decimal (32)
   "CVDY",  // (LongDisp) Convert to Decimal (32)
   "CVDG",  // Convert to Decimal (64)
   "CXGTR",    // Convert from Fixed (DFP128)
   "CXSTR",    // Convert from Signed Packed to DFP128
   "CXTR",  // Compare (DFP128)
   "CXUTR",    // Convert From Unsigned BCD (DFP128)
   "CY",    // (LongDisp) Compare (32)
   "D",  // Divide (32 < 64)
   "DIAG",  // Diagnose Macro
   "DDB",   // Divide (LB)
   "DDBR",  // Divide (LB)
   "DDTR",  // Divide (DFP64)
   "DEB",   // Divide (SB)
   "DEBR",  // Divide (SB)
   "DIDTR",    // Divide to Integer (DFP64)
   "DIDBR",    // Divide to Integer (LB)
   "DIEBR",    // Divide to Integer (SB)
   "DL",    // (LongDisp) Divide
   "DLGR",  // Divide Logical
   "DLR",   // Divide
   "DR",    // Divide
   "DSG",   // (LongDisp) Divide Single (64)
   "DSGF",  // (LongDisp) Divide Single (64 < 32)
   "DSGFR",    // Divide Single (64 < 32)
   "DSGR",  // Divide Single (64)
   "DXTR",  // Divide (DFP128)
   "EAR",   // Extract Access Register
   "ECAG",  // Extract Cache Attribute
   "EEDTR",    // Extract Biased Exponent (DFP64)
   "EEXTR",    // Extract Biased Exponent (DFP128)
   "EFPC",  // Extract FPC
   "EPSW",  // Extract PSW
   "ESDTR",    // Extract Significance (DFP64)
   "ESXTR",    // Extract Significance (DFP128)
   "ETND",  // Extract Transaction Nesting Depth
   "EX",    // Execute
   "EXRL",  // Execute Relative Long
   "FIDBR",    // Load FP Integer (LB)
   "FIDTR",    // Load FP Integer (DFP64)
   "FIEBR",    // Load FP Integer (SB)
   "FIXTR",    // Load FP Integer (DFP128)
   "FLOGR",    // Find Leftmost One
   "IC",    // Insert Character
   "ICM",   // Insert Character under Mask
   "ICMH",  // (LongDisp) Insert Characters under Mask (high)
   "ICMY",  // (LongDisp) Insert Character under Mask
   "ICY",   // (LongDisp) Insert Character
   "IEDTR",    // Insert Biased Exponent (DFP64)
   "IEXTR",    // Insert Biased Exponent (DFP128)
   "IIHH",  // Insert Immediate
   "IIHL",  // Insert Immediate
   "IIHF",  // Insert Immediate (high)
   "IILF",  // Insert Immediate (low)
   "IILH",  // Insert Immediate
   "IILL",  // Insert Immediate
   "IPM",   // Insert Program mask
   "KDTR",  // Compare (DFP64)
   "KIMD",  // Compute Intermediate Message Digest
   "KLMD",  // Compute Last Message Digest
   "KM",    // Cipher Message
   "KMAC",  // Compute Message Authentication Code
   "KMC",   // Cipher Message with Chaining
   "KMCTR",    // Cipher Message with Counter
   "KMF",   // Cipher Message with CFB (Cipher Feedback)
   "KMO",   // Cipher Message with OFB (Output Feedback)
   "KXTR",  // Compare (DFP128)
   "L",  // Load (32)
   "LA",    // Load Address
   "LAA",   // Load And Add (32)
   "LAAG",  // Load And Add (64)
   "LAAL",  // Load And Add Logical (32)
   "LAALG",    // Load And Add Logical (64)
   "LAM",   // Load Access Multiple
   "LAMY",  // (LongDisp) Load Access Multiple
   "LAN",   // (LongDisp) Load And AND (32)
   "LANG",  // (LongDisp) Load And AND (64)
   "LAO",   // (LongDisp) Load And OR (32)
   "LAOG",  // (LongDisp) Load And OR (64)
   "LARL",  // Load Address Relative Long
   "LAT",   // Load and Trap
   "LAX",   // (LongDisp) Load And Exclusive OR (32)
   "LAXG",  // (LongDisp) Load And Exclusive OR (64)
   "LAY",   // (LongDisp) Load Address
   "LB",    // (LongDisp) Load Byte (31) - note it is called LB in the PoP
   "LBH",   // (LongDisp) Load Byte High (32 < 8)
   "LBR",   // Load Byte (32)
   "LCDBR",    // Load Complement (LB)
   "LCDFR",    // Load Complement (DFP64)
   "LCEBR",    // Load Complement (SB)
   "LCGFR",    // Load Complement (64 < 32)
   "LCGR",  // Load Complement (64)
   "LCR",   // Load Complement (32)
   "LD",    // Load (L)
   "LDEB",  // Load Lengthened (LB < SB)
   "LDEBR",    // Load Leeeengthened (LB < SB)
   "LDETR",    // Load Lengthened (64DFP < 32DFP)
   "LDXTR",    // Load Rounded (64DFP < 128DFP)
   "LDGR",  // Load FPR from GR (SB, DB)
   "LDR",   // Load (L)
   "LDY",   // (LongDisp) Load (L)
   "LE",    // Load (S)
   "LEDBR",    // Load Rounded (SB < LB)
   "LEDTR",    // Load Rounded (32DFP < 64DFP)
   "LER",   // Load (S)
   "LEY",   // (LongDisp) Load (S)
   "LFH",   // (LongDisp) Load High (32)
   "LFHAT",    // Load High and Trap
   "LG",    // (LongDisp) Load (64)
   "LGAT",  // Load and Trap (64)
   "LGB",   // (LongDisp) Load Byte (64)
   "LGBR",  // Load Byte (64)
   "LGDR",  // Load GR from FPR (SB, DB)
   "LGF",   // (LongDisp) Load (64 < 32)
   "LGFI",  // Load Immediate (64<32)
   "LGFR",  // (LongDisp) Load (64 < 32)
   "LGH",   // (LongDisp) Load Halfword
   "LGHI",  // Load Halfword Immediate
   "LGHR",  // Load Halfword (64)
   "LGR",   // Load (64)
   "LGRL",  // Load Relative Long (64)
   "LGFRL",    // Load Relative Long (64 < 32)
   "LLGFRL",    //Load Logical Relative Long (64 < 32)
   "LH",    // Load Halfword
   "LHH",   // (LongDisp) Load Halfword High (32 < 16)
   "LHI",   // Load Halfword Immediate
   "LHR",   // Load Halfword (32)
   "LHY",   // (LongDisp)Load Halfword
   "LLC",   // (LongDisp) Load Logical Character (32)
   "LLCH",  // (Long Disp)Load Logical Character High (32 < 8)
   "LLCR",  // Load Logical Character (32)
   "LLGC",  // (LongDisp) Load Logical Character
   "LLGCR",    // Load Logical Character (64)
   "LLGF",  // (LongDisp) Load Logical Halfword
   "LLGFAT",   // Load Logical and Trap
   "LLGFR",    // Load Logical Halfword(64)
   "LLGH",  // (LongDisp) Load Logical Halfword
   "LLGHR",    // Load Logical Halfword(64)
   "LLGT",  // (LongDisp) Load Logical Thirty One Bits
   "LLGTAT",   // Load Logical Thirty One Bits and Trap
   "LLGTR",    // Load Logical Thirty One Bits
   "LLH",   // (LongDisp) Load Logical Halfword
   "LLHH",  // (LongDisp) Load Logical Halfword High (32 < 8)
   "LLHR",  // Load Logical Halfword(32)
   "LLIHH",    // Load Logical Halfword Immediate
   "LLIHL",    // Load Logical Halfword Immediate
   "LLIHF",    // Load Logical Immediate (high)
   "LLILF",    // Load Logical Immediate (low)
   "LLILH",    // Load Logical Halfword Immediate
   "LLILL",    // Load Logical Halfword Immediate
   "LLZRGF",   // Load Logical and Zero Rightmost Byte
   "LM",    // Load Multiple
   "LMG",   // (LongDisp) Load Multiple (64)
   "LMY",   // (LongDisp) Load Multiple
   "LNDBR",    // Load Negative (LB)
   "LNDFR",    // Load Negative (DFP64)
   "LNEBR",    // Load Negative (SB)
   "LNGR",  // Load Negative (64)
   "LNR",   // Load Negative (32)
   "LOC",   // (LongDisp) Load On Condition (32)
   "LOCFH",    // (LongDisp) Load High On Condition
   "LOCFHR",   // Load High On Condition
   "LOCG",  // (LongDisp) Load On Condition (64)
   "LOCGHI",   // Load Halfword Immediate On Condition (64)
   "LOCGR",    // Load On Condition (64)
   "LOCHHI",   // Load Halfword High Immediate On Condition
   "LOCHI",    // Load Halfword Immediate On Condition (32)
   "LOCR",  // Load On Condition (32)
   "LPD",   // Load Pair Disjoint (32)
   "LPDBR",    // Load Positive (LB)
   "LPDFR",    // Load Positive (DFP64)
   "LPDG",  // Load Pair Disjoint (64)
   "LPEBR",    // Load Positive (SB)
   "LPGFR",    // Load Positive (64 < 32)
   "LPGR",  // Load Positive (64)
   "LPQ",   // (LongDisp) Load Pair from Quadword
   "LPR",   // Load Positive (32)
   "LR",    // Load (32)
   "LRIC",  // Load Runtime Instrumentation Controls
   "LRL",   // Load Relative Long (32)
   "LRV",   // (LongDisp) Load Reversed (32)
   "LRVG",  // (LongDisp) Load Reversed (64)
   "LRVGR",    // Load Reversed (64)
   "LRVH",  // (LongDisp) Load Reversed Halfword
   "LRVR",  // Load Reversed (32)
   "LT",    // (LongDisp) Load and Test (32)
   "LTDBR",    // Load and Test (LB)
   "LTDTR",    // Load and Test (DFP64)
   "LTEBR",    // Load and Test (SB)
   "LTG",   // (LongDisp) Load and Test (64)
   "LTGF",  // (LongDisp) Load and Test (64 < 32)
   "LTGFR",    // Load and Test (64 < 32)
   "LTGR",  // Load and Test (64)
   "LTR",   // Load and Test (32)
   "LTXTR",    // Load and Test (DFP128)
   "LXDTR",    // Load Lengthened(64DFP < 128DFP)
   "LY",    // (LongDisp) Load (32)
   "LZDR",  // Load Zero (L)
   "LZER",  // Load Zero (S)
   "LZRF",  // Load and Zero Rightmost Byte
   "LZRG",  // Load and Zero Rightmost Byte
   "M",  // Multiply (64 < 32)
   "MADB",  // Multiply and Add (LB)
   "MADBR",    // Multiply and Add (LB)
   "MAEB",  // Multiply and Add (SB)
   "MAEBR",    // Multiply and Add (SB)
   "MDB",   // Multiply (LB)
   "MDBR",  // Multiply (LB)
   "MDTR",  // Multiply (DFP64)
   "MEEB",  // Multiply (SB)
   "MEEBR",    // Multiply (SB)
   "MGHI",  // Multiply Halfword Immediate
   "MH",    // Multiply Halfword (32)
   "MHI",   // Multiply Halfword Immediate (32)
   "MLG",   // (LongDisp) Multiply Logical ( 128<64 )
   "MLGR",  // Multiply Logical ( 128<64 )
   "MLR",   // Multiply Logical ( 64<32 )
   "MR",    // Multiple (64 < 32)
   "MRIC",  // Modify Runtime Instrumentation Controls
   "MS",    // Multiply Single
   "MSY",   // (LongDisp) Multiply Single
   "MSDB",  // Multiply and Subtract (LB)
   "MSDBR",    // Multiply and Subtract (LB)
   "MSEB",  // Multiply and Subtract (SB)
   "MSEBR",    // Multiply and Subtract (SB)
   "MSG",   // (LongDisp) Multiply Single (64)
   "MSGF",  // (LongDisp) Multiply Single (64 < 32)
   "MSGFR",    // Multiply Single (64 < 32)
   "MSGR",  // Multiply Single (64)
   "MSR",   // Multiply Single Register
   "MVC",   // Move (character)
   "MVCL",  // Move Long
   "MVGHI",    //Move and store immediate (64)
   "MVHHI",    //Move and store immediate (16)
   "MVHI",  //Move and store immediate (32)
   "MVI",   // Move (Immediate)
   "MVIY",  // (LongDisp) Move (Immediate)
   "MXTR",  // Multiply (DFP128)
   "N",  // And (32)
   "NC",    // And (character)
   "NG",    // (LongDisp) And (64)
   "NGR",   // And (64)
   "NGRK",  // And (64)
   "NI",    // And (Immediate)
   "NIAI",  // Next Instruction Access Intent
   "NIHF",  // And Immediate (high)
   "NIHH",  // And Immediate (high high)
   "NIHL",  // And Immediate (high low)
   "NILF",  // And Immediate (low)
   "NILH",  // And Immediate (low high)
   "NILL",  // And Immediate (low low)
   "NIY",   // (LongDisp) And (Immediate)
   "NR",    // And (32)
   "NRK",   // And (32)
   "NTSTG",    // Nontransactional Store
   "NY",    // (LongDisp) And (32)
   "O",  // Or (32)
   "OC",    // Or (character)
   "OG",    // (LongDisp) Or (64)
   "OGR",   // Or (64)
   "OGRK",  // Or (64)
   "OI",    // Or (Immediate)
   "OIHF",  // Or Immediate (high)
   "OIHH",  // Or Immediate (high high)
   "OIHL",  // Or Immediate (high low)
   "OILF",  // Or Immediate (low)
   "OILH",  // Or Immediate (low high)
   "OILL",  // Or Immediate (low low)
   "OIY",   // (LongDisp) Or (Immediate)
   "OR",    // Or (32)
   "ORK",   // Or (32)
   "OY",    // (LongDisp) Or (32)
   "PFD",   // Prefetch Data
   "PFPO",  //perform floating point operations.
   "POPCNT",   // Population Count
   "PPA",   // Perform Processor Assist
   "QADTR",    // Quantize (DFP64)
   "QAXTR",    // Quantize (DFP128)
   "RISBG",    // Rotate Then Insert Selected Bits
   "RISBGN",   // Rotate Then Insert Selected Bits
   "RISBHG",   // Rotate Then Insert Selected Bits High
   "RISBLG",   // Rotate Then Insert Selected Bits Low
   "RLL",   // Rotate Left Single Logical (32)
   "RLLG",  // Rotate Left Single Logical (64)
   "RNSBG",    // Rotate Then AND Selected Bits
   "ROSBG",    // Rotate Then OR Selected Bits
   "RRDTR",    // Reround (DFP64)
   "RRXTR",    // Reround (DFP128)
   "RXSBG",    // Rotate Then XOR Selected Bits
   "S",  // Subtract (32)
   "SAR",   // Set Access Register
   "SDB",   // Subtract (LB)
   "SDBR",  // Subtract (LB)
   "SDTR",  // Subtract (DFP64)
   "SEB",   // Subtract (SB)
   "SEBR",  // Subtract (SB)
   "SFASR",    // Set FPC And Signal
   "SFPC",  // Set FPC
   "SG",    // (LongDisp) Subtract (64)
   "SGF",   // (LongDisp) Subtract (64 < 32)
   "SGFR",  // Subtract (64 < 32)
   "SGR",   // Subtract (64)
   "SGRK",  // Subtract (64)
   "SH",    // Subtract Halfword
   "SHHHR",    // Subtract High (32)
   "SHHLR",    // Subtract High (32)
   "SHY",   // (LongDisp) Subtract Halfword
   "SL",    // Subtract Logical (32)
   "SLA",   // Shift Left Single (32)
   "SLAG",  // (LongDisp) Shift Left Single (64)
   "SLAK",  // (LongDisp) Shift Left Single (32)
   "SLB",   // (LongDisp) Subtract Logical with Borrow (32)
   "SLBG",  // (LongDisp) Subtract Logical With Borrow (64)
   "SLBGR",    // Subtract Logical With Borrow (64)
   "SLBR",  // Subtract Logical With Borrow (32)
   "SLDA",  // Shift Left Double
   "SLDL",  // Shift Left Double Logical
   "SLDT",  // Shift Left Double (DFP64)
   "SLXT",  // Shift Left Double (DFP128)
   "SLFI",  // Subtract Logical Immediate (32)
   "SLG",   // (LongDisp) Subtract Logical (64)
   "SLGF",  // (LongDisp) Subtract Logical (64 < 32)
   "SLGFI",    // Subtract Logical Immediate (64<32)
   "SLGFR",    // Subtract Logical (64 < 32)
   "SLGR",  // Subtract Logical (64)
   "SLGRK",    // Subtract (64)
   "SLHHHR",   // Subtract High (32)
   "SLHHLR",   // Subtract High (32)
   "SLL",   // Shift Left Single Logical
   "SLLG",  // (LongDisp) Shift Left Logical (64)
   "SLLK",  // (LongDisp) Shift Left Logical (32)
   "SLR",   // Subtract Logical (32)
   "SLRK",  // Subtract (32)
   "SLY",   // (LongDisp) Subtract Logical (32)
   "SQDB",  // Square Root Long BFP
   "SQDBR",    // Square Root Long BFP
   "SQEB",  // Square Root Long BFP
   "SQEBR",    // Square Root Long BFP
   "SR",    // Subtract (32)
   "SRA",   // Shift Right Single (32)
   "SRAG",  // (LongDisp) Shift Right Single (64)
   "SRAK",  // (LongDisp) Shift Right Single (32)
   "SRDA",  // Shift Right Double
   "SRDL",  // Shift Right Double Logical
   "SRDT",  // Shift Right Double (DFP64)
   "SRXT",  // Shift Right LongDouble (DFP128)
   "SRL",   // Shift Right Single Logical
   "SRLG",  // (LongDisp) Shift Right Logical (64)
   "SRLK",  // (LongDisp) Shift Right Logical (32)
   "SRK",   // Subtract (32)
   "SRST",  // Search String
   "SRSTU",    // Search String Unicode
   "SRNMT",    // Set RoundingMode (DFP64)
   "ST",    // Store (32)
   "STAM",  // Set Access Multiple
   "STAMY",    // (LongDisp) Set Access Multiple
   "STC",   // Store Character
   "STCH",  // (LongDisp) Store Character High (8)
   "STCK",  // Store Clock
   "STCKE",    // Store Clock Extended
   "STCKF",    // Store Clock Fast
   "STCM",  // Store Character under Mask (low)
   "STCMH",    // (LongDisp) Store Characters under Mask (high)
   "STCMY",    // (LongDisp) Store Characters under Mask (low)
   "STCY",  // (LongDisp) Store Character
   "STD",   // Store (S)
   "STDY",  // (LongDisp) Store (S)
   "STE",   // Store (L)
   "STEY",  // (LongDisp) Store (L)
   "STFH",  // (LongDisp) Store High (32)
   "STG",   // (LongDisp) Store (64)
   "STGRL",    // Store Relative Long (64)
   "STH",   // Store Halfword
   "STHH",  // (LongDisp) Store Halfword High (16)
   "STHY",  // (LongDisp) Store Halfword
   "STM",   // Store Multiple (32)
   "STMG",  // (LongDisp) Store Multiple (64)
   "STMY",  // (LongDisp) Store Multiple
   "STOC",  // (LongDisp) Store On Condition (32)
   "STOCFH",   // (LongDisp) Store High On Condition
   "STOCG",    // (LongDisp) Store On Condition (64)
   "STPQ",  // (LongDisp) Store Pair to Quadword
   "STRIC",    // Store Runtime Instrumentation Controls
   "STRL",  // Store Relative Long (32)
   "STRV",  // (LongDisp) Store (32)
   "STRVG",    // (LongDisp) Store Reversed (64)
   "STRVH",    // (LongDisp) Store Reversed Halfword
   "STY",   // (LongDisp) Store (32)
   "SXTR",  // Subtract (DFP128)
   "SY",    // (LongDisp) Subtract (32)
   "TABORT",   // Transaction Abort
   "TBEGIN",   // Transaction Begin
   "TBEGINC",  // Constrained Transaction Begin
   "TCDB",  // Test Data Class (LB)
   "TCDT",  // Test Data Class (DFP64)
   "TDCDT",    // Test Data Class (DFP64)
   "TCEB",  // Test Data Class (SB)
   "TDCET",    // Test Data Class (DFP32)
   "TDCXT",    // Test Data Class (DFP64)
   "TDGDT",    // Test Data Group (DFP64)
   "TDGET",    // Test Data Group (DFP32)
   "TDGXT",    // Test Data Group (DFP128)
   "TEND",  // Transaction End
   "TM",    // Test under Mask
   "TMLL",  // Test under Mask
   "TMLH",  // Test under Mask
   "TMHL",  // Test under Mask
   "TMHH",  // Test under Mask
   "TMY",   // Test under Mask
   "TR",    // Translate
   "TRE",   // Translate Extended
   "TRIC",  // Test Runtime Instrumentation Controls
   "TROO",  // Translate One to One
   "TROT",  // Translate One to Two
   "TRT",   // Translate and Test
   "TRTO",  // Translate Two to One
   "TRTT",  // Translate Two to Two
   "TRTE",  // Translate and Test Extended
   "TRTRE",    // Translate and Test Reversed Extended
   "TS",    // Test and Set
   "X",  // Exclusive Or (32)
   "XC",    // Exclusive Or (character)
   "XG",    // (LongDisp) Exclusive Or (64)
   "XGR",   // Exclusive Or (64)
   "XGRK",  // Exclusive Or (64)
   "XI",    // Exclusive Or (immediate)
   "XIHF",  // Exclusive Or Immediate (high)
   "XILF",  // Exclusive Or Immediate (low)
   "XIY",   // (LongDisp) Exclusive Or (immediate)
   "XR",    // Exclusive Or (32)
   "XRK",   // Exclusive Or (32)
   "XY",    // (LongDisp) Exclusive Or (32)
   "ASI",   // Add Direct to Memory
   "AGSI",  // Add Direct to Memory (64)
   "ALSI",  // Add Logical Direct to Memory
   "ALGSI",    // Add Logical Direct to Memory (64)
   "CRL",   // Compare Relative Long
   "CGRL",  // Compare Relative Long (64)
   "CGFRL",    // Compare Relative Long (32 < 64)
   "CHHSI",    // Compare Direct to Memory Halfword Immediate (16)
   "CHSI",  // Compare Direct to Memory Halfword Immediate (32)
   "CGHSI",    // Compare Direct to Memory Halfword Immediate (64)
   "CHHRL",    // Compare Halfword Relative Long (16)
   "CHRL",  // Compare Halfword Relative Long (32)
   "CGHRL",    // Compare Halfword Relative Long (64)
   "CLHHSI",   // Compare Logical Immediate (16)
   "CLFHSI",   // Compare Logical Immediate (32)
   "CLGHSI",   // Compare Logical Immediate (64)
   "CLRL",  // Compare Logical Relative Long
   "CLGRL",    // Compare Logical Relative Long (64)
   "CLGFRL",   // Compare Logical Relative Long (32 < 64)
   "CLHHRL",   // Compare Logical Relative Long Halfword (16)
   "CLHRL",    // Compare Logical Relative Long Halfword (32)
   "CLGHRL",   // Compare Logical Relative Long Halfword (64)
   "MSFI",  // Multiply Single Immediate
   "MSGFI",    // Multiply Single Immediate
   "AP",    // Add Decimal
   "BASSM",    // Branch and Save and Set Mode
   "BAKR",  // Branch and Stack
   "BSM",   // Branch and Set Mode
   "CDS",   // Compare Double and Swap
   "CDSY",  // Compare Double and Swap
   "CDSG",  // Compare Double and Swap
   "CFC",   // Compare and Form CodeWord
   "CLST",  // Compare Logical String
   "CKSM",  // Checksum
   "CP",    // Compare Decimal
   "CPSDR",    // Copy Sign
   "CSCH",  // Clear Subchannel
   "CUUTF",    // Convert Unicode to UTF-8
   "CUTFU",    // Convert UTF-8 to Unicode
   "CU14",  // Convert UTF-8 to UTF-32
   "CU24",  // Convert UTF-16 to UTF-32
   "CU41",  // Convert UTF-32 to UTF-8
   "CU42",  // Convert UTF-32 to UTF-16
   "CUSE",  // Compare Until Substring Equal
   "CVB",   // Convert to Binary
   "CVBY",  // (LongDisp) Convert to Binary
   "CVBG",  // Convert to Binary
   "DP",    // Divide Decimal
   "ED",    // Edit
   "EDMK",  // Edit and Mark
   "ESAR",  // Extract Secondary ASN
   "EPAR",  // Extract Primary ASN
   "EREG",  // Extract Stacked Registers
   "EREGG",    // Extract Stacked Registers
   "ESTA",  // Extract Stacked State
   "ESEA",  // Extract and Set Extended Authority
   "HSCH",  // Halt Subchannel
   "IAC",   // Insert Address Space Control
   "IPK",   // Insert PSW Key
   "IVSK",  // Insert Virtual Storage Key
   "ISKE",  // Insert Storage Key Extended
   "LAE",   // Load Address Extended
   "LAEY",  // Load Address Extended Y Form
   "LCBB",  // Load Count To Block Boundary
   "LCTL",  // Load Control
   "LCTLG",    // Load Control
   "LFPC",  // Load FPC
   "LMH",   // Load Multiple High
   "LNGFR",    // Load Negative
   "LPSW",  // Load PSW
   "LPSWE",    // Load PSW Extended
   "LRA",   // Load Real Address
   "LRAG",  // Load Real Address
   "LRAY",  // Load Real Address Y Form
   "LURA",  // Load Using Real Address
   "LURAG",    // Load Using Real Address
   "MC",    // Monitor Call
   "MP",    // Multiple Decimal
   "MSCH",  // Modify Subchannel
   "MSTA",  // Modify Stacked State
   "MVCDK",    // Move With Destination Key
   "MVCSK",    // Move With Source Key
   "MVCK",  // Move With Key
   "MVCLU",    // Move Long Unicode
   "MVCLE",    // Move Long Extended
   "MVCP",  // Move to Primary
   "MVCS",  // Move to Secondary
   "MVN",   // Move Numerics
   "MVO",   // Move With Offset
   "MVZ",   // Move Zones
   "MVST",  // Move String
   "PACK",  // Pack
   "PALB",  // Purge ALB
   "PC",    // Program Call
   "PLO",   // Perform Locked Operation
   "PKA",   // Pack ASCII
   "PKU",   // Pack Unicode
   "PR",    // Program Return
   "PT",    // Program Transfer
   "PFDRL",    // Prefetch Data Relative Long
   "RCHP",  // Reset Channel Path
   "RSCH",  // Resume Subchannel
   "SAC",   // Set Address Control
   "SAL",   // Set Address Limit
   "SCHM",  // Set Channel Monitor
   "SCK",   // Set Clock
   "SCKC",  // Set Clock Comparator
   "SIGP",  // Signal Processor
   "SP",    // Subtract Decimal
   "SPKA",  // Set PSW Key From Address
   "SPM",   // Set Program Mask
   "SPT",   // Set CPU Timer
   "SPX",   // Set Prefix
   "SRP",   // Shift and Round Decimal
   "SRNM",  // Set BFP Rounding Mode
   "SSAR",  // Set Secondary ASN
   "SSCH",  // Start Subchannel
   "SSKE",  // Set Storage Key Extended
   "SSM",   // Set System Mask
   "STAP",  // Store CPU Address
   "STCKC",    // Store Clock Comparator
   "STCPS",    // Store Channel Path Status
   "STCRW",    // Store Channel Report Word
   "STCTL",    // Store Control
   "STCTG",    // Store Control
   "STFPC",    // Store FPC
   "STIDP",    // Store CPU ID
   "STMH",  // Store Multiple High
   "STNSM",    // Store Then and System Mask
   "STOSM",    // Store Then or System Mask
   "STPT",  // Store CPU Timer
   "STPX",  // Store Prefix
   "STRAG",    // Store Real Address
   "STSCH",    // Store Subchannel
   "STURA",    // Store Using Real Address
   "STURG",    // Store Using Real Address
   "SVC",   // Supervisor Call
   "TAR",   // Test Access
   "TML",   // Test under Mask
   "TMH",   // Test under Mask
   "TP",    // Test Decimal
   "TPI",   // Test Pending Interruption
   "TRTR",  // Translate and Test Reverse
   "TPROT",    // Test Protection
   "TSCH",  // Test Subchannel
   "UNPK",  // Unpack
   "UNPKA",    // Unpack ASCII
   "UNPKU",    // Unpack Unicode
   "UPT",   // Update Tree
   "ZAP",   // Zero and Add
   "AXBR",  // Add (EB)
   "CFXBR",    // Convert to Fixed (EB < 32), note here
   "CGXBR",    // Convert to Fixed (EB < 64), note here
   "CLFXBR",   // Convert to Logical (EB < 32), note here
   "CLGXBR",   // Convert to Logical (EB < 64), note here
   "CXBR",  // Compare (EB)
   "CXFBR",    // Convert from Fixed (EB < 32)
   "CXGBR",    // Convert from Fixed (EB < 64)
   "CXLFBR",   // Convert from Logical (EB < 32)
   "CXLGBR",   // Convert from Logical (EB < 64)
   "DXBR",  // Divide (EB)
   "FIXBR",    // Load FP Integer (EB)
   "LTXBR",    // Load and Test (DFP64)
   "LCXBR",    // Load Complement (EB)
   "LDXBR",    // Load Rounded (LB < EB)
   "LEXBR",    // Load Rounded (SB < EB)
   "LNXBR",    // Load Negative (EB)
   "LPXBR",    // Load Positive (EB)
   "LXDB",  // Load Lengthened (EB < LB)
   "LXDBR",    // Load Leeeengthened (EB < DB)
   "LXEB",  // Load Lengthened (EB < SB)
   "LXEBR",    // Load Leeeengthened (EB < SB)
   "LXR",   // Load Leeeengthened (EB < SB)
   "LZXR",  // Load Zero (EB)
   "MXBR",  // Multiply (EB)
   "SQXBR",    // Square Root (EB)
   "SXBR",  // Subtract (EB)
   "TCXB",  // Test Data Class (EB)
   "AXR",   // Add Normalized, Extended
   "ADR",   // Add Normalized, Long
   "AD",    // Add Normalized, Long
   "AER",   // Add Normalized, Short
   "AE",    // Add Normalized, Short
   "AWR",   // Add Unnormalized, Long
   "AW",    // Add Unnormalized, Long
   "AUR",   // Add Unnormalized, Short
   "AU",    // Add Unnormalized, Short
   "CXR",   // Compare, Extended
   "CDR",   // Compare, Long
   "CD",    // Compare, Long
   "CER",   // Compare, Short
   "CE",    // Compare, Short
   "CEFR",  // Convert from int32 to short HFP
   "CDFR",  // Convert from int32 to long HFP
   "CXFR",  // Convert from int32 to extended HFP
   "CEGR",  // Convert from int64 to short HFP
   "CDGR",  // Convert from int64 to long HFP
   "CXGR",  // Convert from int64 to extended HFP
   "CFER",  // Convert short HFP to int32
   "CFDR",  // Convert long HFP to int32
   "CFXR",  // Convert long HFP to int32
   "CGER",  // Convert short HFP to int64
   "CGDR",  // Convert long HFP to int64
   "CGXR",  // Convert long HFP to int64
   "DDR",   // Divide, Long HFP
   "DD",    // Divide, Long HFP
   "DER",   // Divide, Short HFP
   "DE",    // Divide, short HFP
   "DXR",   // Divide, extended HFP
   "FIER",  // Load short HFP Integer (round toward 0)
   "FIDR",  // Load long HFP Integer (round toward 0)
   "FIXR",  // Load extended HFP Integer (round toward 0)
   "HDR",   // Halve, Long HFP
   "HER",   // Halve, short HFP
   "LTDR",  // Load and Test, Long HFP
   "LTER",  // Load and Test, Short HFP
   "LTXR",  // Load and Test, extended HFP
   "LCDR",  // Load Complement, Long HFP
   "LCER",  // Load Complement, short HFP
   "LCXR",  // Load Complement, extended HFP
   "LNDR",  // Load Negative, Long HFP
   "LNER",  // Load Negative, short HFP
   "LNXR",  // Load Negative, extended HFP
   "LPDR",  // Load Positive, Long HFP
   "LPER",  // Load Positive, short HFP
   "LPXR",  // Load Positive, extended HFP
   "LEDR",  // Load Rounded, long to short HFP
   "LDXR",  // Load Rounded, extended to long HFP
   "LEXR",  // Load Rounded, extended to short HFP
   "LDER",  // Load Rounded, long to short HFP
   "LXDR",  // Load Lengthened, long to extended HFP
   "LXER",  // Load Rounded, short to extended HFP
   "LDE",   // Load Lengthened short to long HFP
   "LXD",   // Load Lengthened short to long HFP
   "LXE",   // Load Lengthened short to long HFP
   "MEER",  // Multiply, short HFP source and result
   "MEE",   // Multiply short HFP
   "MXR",   // Multiply, extended HFP source and result
   "MDR",   // Multiply, long HFP source and result
   "MD",    // Multiply, long HFP source and result
   "MXDR",  // Multiply, long HFP source, extended HFP result
   "MXD",   // Multiply, long HFP source, extended HFP result
   "MDER",  // Multiply, short HFP source, long HFP result
   "MDE",   // Multiply, short HFP source, long HFP result
   "MAER",  // Multiply and Add, short HFP sources and result
   "MADR",  // Multiply and Add, long HFP sources and result
   "MAE",   // Multiply and Add, short HFP sources and result
   "MAD",   // Multiply and Add, long HFP sources and result
   "MSER",  // Multiply and Subtract, short HFP sources and result
   "MSDR",  // Multiply and Subtract, long HFP sources and result
   "MSE",   // Multiply and Subtract, short HFP sources and result
   "MSD",   // Multiply and Subtract, long HFP sources and result
   "MAYR",  // Multiply and Add Unnormalized, long HFP sources and extended HFP result
   "MAYHR",    // Multiply and Add Unnormalized, long HFP sources and high-order part of extended HFP result
   "MAYLR",    // Multiply and Add Unnormalized, long HFP sources and low-order part of extended HFP result
   "MAY",   // Multiply and Add Unnormalized, long HFP sources and extended HFP result
   "MAYH",  // Multiply and Add Unnormalized, long HFP sources and high-order part of extended HFP result
   "MAYL",  // Multiply and Add Unnormalized, long HFP sources and low-order part of extended HFP result
   "MYR",   // Multiply Unnormalized, long HFP sources and extended HFP result
   "MYHR",  // Multiply Unnormalized, long HFP sources and high-order part of extended HFP result
   "MYLR",  // Multiply Unnormalized, long HFP sources and low-order part of extended HFP result
   "MY",    // Multiply Unnormalized, long HFP sources and extended HFP result
   "MYH",   // Multiply Unnormalized, long HFP sources and high-order part of extended HFP result
   "MYL",   // Multiply Unnormalized, long HFP sources and low-order part of extended HFP result
   "SQER",  // Square Root short HFP
   "SQDR",  // Square Root Long HFP
   "SQXR",  // Square Root extended HFP
   "SQE",   // Square Root short HFP
   "SQD",   // Square Root Long HFP
   "SXR",   // Subtract Normalized, extended HFP
   "SDR",   // Subtract Normalized,long HFP
   "SER",   // Subtract Normalized,short HFP
   "SD",    // Subtract Normalized,long HFP
   "SE",    // Subtract Normalized,short HFP
   "SWR",   // Subtract Unnormalized,long HFP
   "SW",    // Subtract Unnormalized,long HFP
   "SUR",   // Subtract Unnormalized,short HFP
   "SU",    // Subtract Unnormalized,short HFP
   "TBDR",  // Convert HFP to BFP,long HFP,long BFP
   "TBEDR",    // Convert HFP to BFP,long HFP,short BFP
   "THDER",    // Convert BFP to HFP,short BFP,long HFP
   "THDR",  // Convert BFP to HFP,long BFP,long HFP
   "SAM24",    // Set 24 bit addressing mode
   "SAM31",    // Set 31 bit addressing mode
   "SAM64",    // Set 64 bit addressing mode
   "TAM",   // Test addressing mode
   "CDZT",  // Convert Zoned to DFP Long
   "CXZT",  // Convert Zoned to DFP Extended
   "CZDT",  // Convert DFP Long to Zoned
   "CZXT",  // Convert DFP Extended to Zoned
   "CDPT",  // Convert Packed to DFP Long
   "CXPT",  // Convert Packed to DFP Extended
   "CPDT",  // Convert DFP Long to Packed
   "CPXT",  // Convert DFP Extended to Packed
   "LHHR",  // Load (High <- High)
   "LHLR",  // Load (High <- Low)
   "LLHFR",    // Load (Low <- High)
   "LLHHHR",   // Load Logical Halfword (High <- High)
   "LLHHLR",   // Load Logical Halfword (High <- low)
   "LLHLHR",   // Load Logical Halfword (Low <- High)
   "LLCHHR",   // Load Logical Character (High <- High)
   "LLCHLR",   // Load Logical Character (High <- low)
   "LLCLHR",   // Load Logical Character (Low <- High)
   "SLLHH",    // Shift Left Logical (High <- High)
   "SLLLH",    // Shift Left Logical (Low <- High)
   "SRLHH",    // Shift Right Logical (High <- High)
   "SRLLH",    // Shift Right Logical (Low <- High)
   "NHHR",  // AND High (High <- High)
   "NHLR",  // AND High (High <- Low)
   "NLHR",  // AND High (Low <- High)
   "XHHR",  // Exclusive OR High (High <- High)
   "XHLR",  // Exclusive OR High (High <- Low)
   "XLHR",  // Exclusive OR High (Low <- High)
   "OHHR",  // OR High (High <- High)
   "OHLR",  // OR High (High <- Low)
   "OLHR",  // OR High (Low <- High)
   "VGEF",  // vector gather element (32)
   "VGEG",  // vector gather element (64)
   "VGBM",  // vector generate byte mask
   "VGM",   // vector generate mask
   "VL",    // vector load
   "VLR",   // vector load
   "VLREP",  // vector load and replicate
   "VLEB",  // vector load element (8)
   "VLEH",  // vector load element (16)
   "VLEF",  // vector load element (32)
   "VLEG",  // vector load element (64)
   "VLEIB",    // vector load element immediate (8)
   "VLEIH",    // vector load element immediate (16)
   "VLEIF",    // vector load element immediate (32)
   "VLEIG",    // vector load element immediate (64)
   "VLGV",  // vector load gr from vr element
   "VLLEZ",    // vector load logical element and zero
   "VLM",   // vector load multiple
   "VLBB",  // vector load to block boundary
   "VLVG",  // vector load vr element from gr
   "VLVGP",    // vector load vr from grs disjoint
   "VLL",   // vector load with length
   "VMRH",  // vector merge high
   "VMRL",  // vector merge low
   "VPK",   // vector pack
   "VPKS",  // vector pack saturate CC Set
   "VPKLS",    // vector pack logical saturate CC Set
   "VPERM",    // vector permute
   "VPDI",  // vector permute doubleword immediate
   "VREP",  // vector replicate
   "VREPI",    // vector replicate immediate
   "VSCEF",    // vector scatter element (32)
   "VSCEG",    // vector scatter element (64)
   "VSEL",  // vector select
   "VSEG",  // vector sign extend to doubleword
   "VST",   // vector store
   "VSTEB",    // vector store element (8)
   "VSTEH",    // vector store element (16)
   "VSTEF",    // vector store element (32)
   "VSTEG",    // vector store element (64)
   "VSTM",  // vector store multiple
   "VSTL",  // vector store with length
   "VUPH",  // vector unpack high
   "VUPLH",    // vector unpack logical high
   "VUPL",  // vector unpack low
   "VUPLL",    // vector unpack logical low
   "VA",    // vector add
   "VACC",  // vector add compute carry
   "VAC",   // vector add with carry
   "VACCC",    // vector add with carry compute carry
   "VN",    // vector and
   "VNC",   // vector and with complement
   "VAVG",  // vector average
   "VAVGL",    // vector average logical
   "VCKSM",    // vector checksum
   "VEC",   // vector element comp are CC Set
   "VECL",  // vector element comp are logical CC Set
   "VCEQ",  // vector comp are equal CC Set
   "VCH",   // vector comp are high CC Set
   "VCHL",  // vector comp are high logical CC Set
   "VCLZ",  // vector count leading zeros
   "VCTZ",  // vector count trailing zeros
   "VX",    // vector exclusive or
   "VGFM",  // vector galois field multiply sum
   "VGFMA",    // vector galois field multiply sum and accumulate
   "VLC",   // vector load complement
   "VLP",   // vector load positive
   "VMX",   // vector maximum
   "VMXL",  // vector maximum logical
   "VMN",   // vector minimum
   "VMNL",  // vector minimum logical
   "VMAL",  // vector multiply and add low
   "VMAH",  // vector multiply and add high
   "VMALH",    // vector multiply and add logical high
   "VMAE",  // vector multiply and add even
   "VMALE",    // vector multiply and add logical even
   "VMAO",  // vector multiply and add odd
   "VMALO",    // vector multiply and add logical odd
   "VMH",   // vector multiply high
   "VMLH",  // vector multiply logical high
   "VML",   // vector multiply low
   "VME",   // vector multiply even
   "VMLE",  // vector multiply logical even
   "VMO",   // vector multiply odd
   "VMLO",  // vector multiply logical odd
   "VNO",   // vector nor
   "VO",    // vector or
   "VPOPCT",   // vector population count
   "VERLLV",   // vector element rotate left logical
   "VERLL",    // vector element rotate left logical
   "VERIM",    // vector element rotate and insert under mask
   "VESLV",    // vector element shift left
   "VESL",  // vector element shift left
   "VESRAV",   // vector element shift right arithmetic
   "VESRA",    // vector element shift right arithmetic
   "VESRLV",   // vector element shift right logical
   "VESRL",    // vector element shift right logical
   "VSL",   // vector shift left
   "VSLB",  // vector shift left by byte
   "VSLDB",    // vector shift left double by byte
   "VSRA",  // vector shift right arithmetic
   "VSRAB",    // vector shift right arithmetic by byte
   "VSRL",  // vector shift right logical
   "VSRLB",    // vector shift right logical by byte
   "VS",    // vector subtract
   "VSCBI",  // vector subtract compute borrow indication I
   "VSBI",   // vector subtract with borrow indication I
   "VSBCBI",    // vector subtract with borrow compute borrow indication I
   "VSUMG",    // vector sum across doubleword
   "VSUMQ",    // vector sum across quadword
   "VSUM",  // vector sum across word
   "VTM",   // vector test under mask CC Set
   "VFAE",  // vector find any element equal CC Set* (*: If CS bit != 0)
   "VFEE",  // vector find element equal CC Set*
   "VFENE",    // vector find element not equal CC Set*
   "VISTR",    // vector isolate string CC Set*
   "VSTRC",    // vector string range compare CC Set*
   "VFA",   // vector floating-point add
   "WFC",   // vector floating-point comp are scalar CC Set
   "WFK",   // vector floating-point comp are and signal scalar CC Set
   "VFCE",  // vector floating-point comp are equal CC Set*
   "VFCH",  // vector floating-point comp are high CC Set*
   "VFCHE",    // vector floating-point comp are high or equal CC Set*
   "VCDG",  // vector floating-point convert from fixed 64-bit
   "VCDLG",    // vector floating-point convert from logical 64-bit
   "VCGD",  // vector floating-point convert to fixed 64-bit
   "VCLGD",    // vector floating-point convert to logical 64-bit
   "VFD",   // vector floating-point divide
   "VFI",   // vector load floating-point integer
   "VLDE",  // vector floating-point load lengthened
   "VLED",  // vector floating-point load rounded
   "VFM",   // vector floating-point multiply
   "VFMA",  // vector floating-point multiply and add
   "VFMS",  // vector floating-point multiply and subtract
   "VFPSO",    // vector floating-point perform sign operation
   "VFSQ",  // vector floating-point square root
   "VFS",   // vector floating-point subtract
   "VFTCI",    // vector floating-point test data class immediate CC Set
   "RIEMIT",   // Runtime Instrumentation Emit
   "RIOFF",    // Runtime Instrumentation Off
   "RION",  // Runtime Instrumentation On
   "RINEXT",   // Runtime Instrumentation Next
   "ASSOCREGS",   // Register Association
   "DEPEND",   // Someplace to hang dependencies
   "DS",    // DS
   "FENCE",    // Fence
   "SCHEDFENCE",  // Scheduling Fence
   "PROC",  // Entry to the method
   "RET",   // Return
   "DIRECTIVE",   // WCode DIR related
   "WRTBAR",   // Write Barrier
   "XPCALLDESC",  // zOS-31 LE Call Descriptor.
   "DC",    // DC
   "DC2",   // DC2
   "VGNOP",    // ValueGuardNOP
   "NOP",   // No-Op (for Labels)
   "ASM",   // ASM WCode Support
   "LOCK",  // lock registers
   "UNLOCK",   // unlock registers
   "LABEL",    // Destination of a jump
   "TAILCALL",   // Tail Call
   "DCB",   // Debug Counter Bump

