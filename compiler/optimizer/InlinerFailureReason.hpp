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

///////////////////////////////////////////////////////////////////////////
// This file contains a listing of all Reasons Inliner may fail or inline something
//
//
///////////////////////////////////////////////////////////////////////////


#ifndef INLFAILREASON_INCL
#define INLFAILREASON_INCL

#define FailureReasonStr( str ) #str
enum TR_InlinerFailureReason
   {
   InlineableTarget,       //Not A failure Reason
   OverrideInlineTarget,   //Not A failure Reason
   TryToInlineTarget,      //Not A failure Reason
   Trimmed_List_of_Callees,
   Recognized_Callee,
   Exceeds_ByteCode_Threshold,
   Recursive_Callee,
   Excessive_FanIn,
   NonInlineable_WCode,
   Virtual_Inlining_Disabled,
   NonVirtual_Inlining_Disabled,
   Sync_Method_Inlining_Disabled,
   Not_Compilable_Callee,
   JNI_Callee,
   EH_Aware_Callee,
   StrictFP_Callee,
   DontInline_Callee,
   Not_InlineOnly_Callee,
   Selective_Debugable_Callee,
   Exceeds_Size_Threshold,
   Unresolved_Callee,
   Cold_Call,
   Cold_Block,
   Exceeded_Caller_Budget,
   No_Single_Interface_Callee,
   EDO_Callee,
   Not_Sane,
   Not_Profiled_Interface_Callee,
   Callee_Too_Many_Bytecodes,
   ECS_Failed,
   Unresolved_In_ECS,
   Exceeded_Caller_Node_Budget,
   Exceeded_Caller_SiteSize,
   ProfileManager_ColdCall,
   Decompilation_Point,
   No_Inlineable_Targets,
   Cant_Match_Parms_to_Args,
   Needs_Method_Tracing,
   Will_Create_Unallowed_Temps,
   MT_Marked,

   Unknown_Reason
   };
static const char *TR_InlinerFailureReasonStr [] =
   {
   FailureReasonStr( InlineableTarget ),
   FailureReasonStr( OverrideInlineTarget ),
   FailureReasonStr( TryToInlineTarget ),
   FailureReasonStr( Trimmed_List_of_Callees ),
   FailureReasonStr( Recognized_Callee ),
   FailureReasonStr( Exceeds_ByteCode_Threshold ),
   FailureReasonStr( Recursive_Callee ),
   FailureReasonStr( Excessive_FanIn ),
   FailureReasonStr( NonInlineable_WCode ),
   FailureReasonStr( Virtual_Inlining_Disabled ),
   FailureReasonStr( NonVirtual_Inlining_Disabled ),
   FailureReasonStr( Sync_Method_Inlining_Disabled ),
   FailureReasonStr( Not_Compilable_Callee ),
   FailureReasonStr( JNI_Callee ),
   FailureReasonStr( EH_Aware_Callee ),
   FailureReasonStr( StrictFP_Callee ),
   FailureReasonStr( DontInline_Callee ),
   FailureReasonStr( Not_InlineOnly_Callee ),
   FailureReasonStr( Selective_Debugable_Callee ),
   FailureReasonStr( Exceeds_Size_Threshold ),
   FailureReasonStr( Unresolved_Callee ),
   FailureReasonStr( Cold_Call ),
   FailureReasonStr( Cold_Block ),
   FailureReasonStr( Exceeded_Caller_Budget ),
   FailureReasonStr( No_Single_Interface_Callee ),
   FailureReasonStr( EDO_Callee ),
   FailureReasonStr( Not_Sane ),
   FailureReasonStr( Not_Profiled_Interface_Callee ),
   FailureReasonStr( Callee_Too_Many_Bytecodes ),
   FailureReasonStr( ECS_Failed ),
   FailureReasonStr( Unresolved_In_ECS ),
   FailureReasonStr( Exceeded_Caller_Node_Budget ),
   FailureReasonStr( Exceeded_Caller_SiteSize ),
   FailureReasonStr( ProfileManager_ColdCall ),
   FailureReasonStr( Decompilation_Point ),
   FailureReasonStr( No_Inlineable_Targets ),
   FailureReasonStr( Cant_Match_Parms_to_Args ),
   FailureReasonStr( Needs_Method_Tracing ),
   FailureReasonStr( Will_Create_Unallowed_Temps ),
   FailureReasonStr( MT_Marked ),

   FailureReasonStr( Unknown_Reason ),
   };

#endif
