###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

#[==========[ .rst

OmrOption
---------

OmrOption is a module which provides a declarative syntax for specifying
and validating on/off build options. Options are stored as ``BOOL`` cmake
cache variables, and revalidated on every cmake run. Validation for an
options group is manually invoked, after all options and other
prerequisites have been initialized.

The general syntax is:
::
  omr_add_option_group(<group>)

  omr_add_option(<option> <group>
    COMMENT <comment>
    DEFAULT <expression>
    REQUIRES <option>...
    REQUIRES_EXPR <expr>
    CONFLICTS <option>...
  )

  omr_validate_option_group(<group>)

The Basic API
^^^^^^^^^^^^^

Every option is associated with a group. A group has to be manually
created and validated.

::
  omr_add_option_group(<group>...)

Create an option group. A global property called ``<group>`` contains a
list of all options in the group.

::
  omr_validate_option_group(<group>...)

Validate the options registered to ``<group>``. A validation error causes
a ``FATAL_ERROR``. A group can be validated multiple times. It is best to
delay validation until after all options have been set.

::
  omr_add_option(<option> <group>
    COMMENT <comment>
    DEFAULT <expression>
    REQUIRES <option>...
    REQUIRES_EXPR <expr>
    CONFLICTS <option>...
  )

Create a new option. The new option is a ``BOOL`` cache variable.
The option will be registered in ``<group>``. When ``<group>`` is
validated, this option's requirements will be tested.

``DEFAULT`` is an expression, which is evaluated as an if-expression and
coerced to an on/off value. The default value is only evaluated on the
first run, and cached in subsequent runs. Since options are regular cmake
cache variables, the value can be manually overridden by users at any
point. A default value must always be provided.

``REQUIRES`` allows users to pass a list of required flags or variables,
which must be ``ON`` if the option is ``ON``.

Similarly, ``CONFLICTS`` is a list of options which must be ``OFF``, if the
option is ON.

``REQUIRES_EXPR`` is an expression which must be ``TRUE``, if the option is
ON. This can be used for expressing more complex option requirements.
Each option can have only one associated ``REQUIRES_EXPR``.

Registering Additional Requirements
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In addition to the :command:`omr_add_option`, requirements can be
registered to an option later.

::
  omr_option_requires(<option> [<requirement>...])

Add requirements to ``<option>``. If the option is ``ON``, each variable
named in the requirements must also be ``ON``.

::
  omr_option_conflicts(<option> [<conflict>...])

Add conflicts to ``<option>``. If the option is ``ON``, each variable named
in the conflicts must ``OFF``.

::
  omr_option_requires_expr(<option> <expr>)

Add a "requirement expression" to an option. The expression will be
evaluated as an if-expr. The expression must be true if ``<option>`` is
``ON``.

Low level Options API
^^^^^^^^^^^^^^^^^^^^^

::
  omr_declare_option(<option>)

Declare an option, without creating a cache variable or registering the
option into a group. This function will defines the three properties:

- ``option_REQUIRES``
- ``option_CONFLICTS``
- ``option_REQUIRES_EXPR``

At this point, it is possible to register requirements on the option,
put the option into a group, or validate the option (despite having no
requirements).

::
  omr_group_options(<group> <option>...)

Register a set of options into an options group. Options must have already
been declared. It's possible (but not recommended) to register an option
into multiple groups.

::
  omr_validate_option(<option>)

Validate a single option. If the option is false or not declared, do
nothing.

An Example
^^^^^^^^^^

::

  omr_add_option_group(JS_OPTIONS)

  omr_add_option(JS_LOW_TAGGED_INTEGERS JS_OPTIONS
    COMMENT "Encode properties as either pointers or low-tagged integer immediates."
    DEFAULT ON
    CONFLICTS JS_NUNBOXED_VALUES
  )

  omr_add_option(JS_NUNBOXED_VALUES JS_OPTIONS
    COMMENT "Encode properties as double immediates, exploiting the format of NaNs to embed additional types."
    DEFAULT NOT JS_LOW_TAGGED_INTEGERS
    REQUIRES OMR_ENV64 IEEE_754
    CONFLICTS JS_LOW_TAGGED_INTEGERS
    REQUIRES_EXPR OMR_HOST_CPU IN_LIST "X86" "x86_64"
  )

  omr_validate_option_group(JS_OPTIONS)

  if(JS_LOW_TAGGED_INTEGERS)
    message(STATUS "tagging integer values")
  elseif(JS_NUNBOXED_VALUES)
    message(STATUS "Using NunBoxing to represent values")
  else()
    message(STATUS "not encoding immediate values")
  endif()

Comparison to :module:`DependentOptions`
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There is a similar module built in to cmake called DependentOption.
This module was not used because:

1. A dependent option is only shown to the user if it's dependencies are
   satisfied. OmrOption makes the choice to always present ALL flags in
   the cache.
2. A dependent option will revert to it's default value if it's
   requirements aren't satisfied, overriding any user provided value.
   OmrOption never tries to override the user, and relies on option
   validation instead.
3. OmrOption provides an easier to read, more obvious syntax.
   The default value in OmrOption is an expression, rather than a simple
   on/off constant. This gives us a succinct way to implement implicitly
   enabled/disabled flags, while still being user overridable.
   Dependent options have a dependency expression, but the default value
   is a constant, which cannot be overridden by the user.

]==========]

# include once
if(OMR_OPTION_)
	return()
endif()
set(OMR_OPTION_ 1)

include(OmrAssert)

function(omr_add_option_group)
	foreach (group IN LISTS ARGN)
		define_property(GLOBAL PROPERTY ${group}
			BRIEF_DOCS "A list of user-decideable options."
			FULL_DOCS "A list of options which can be validated after the project has been configured."
		)
		set_property(GLOBAL PROPERTY ${group} "")
	endforeach(group)
endfunction(omr_add_option_group)

# _omr_assert_is_option_group(<group>)
# Assert that <group> has been defined as an options group. Will FATAL_ERROR if
# group is not defined.
function(_omr_assert_is_option_group group)
	get_property(is_option_group GLOBAL PROPERTY ${group} SET)
	omr_assert(
		TEST is_option_group
		MESSAGE "${group} is not an options group"
	)
endfunction(_omr_assert_is_option_group)

function(omr_group_options group)
	_omr_assert_is_option_group(${group})
	set_property(GLOBAL APPEND PROPERTY ${group} ${ARGN})
endfunction(omr_group_options)

# _omr_get_options_group(<output> <group>)
# Store the list of options in <group> into the variable <output>.
function(_omr_get_option_group output group)
	_omr_assert_is_option_group(${group})
	get_property(${output} GLOBAL PROPERTY ${group})
	set(${output} ${${output}} PARENT_SCOPE)
endfunction()

function(omr_declare_option option)
	define_property(GLOBAL PROPERTY ${option}_REQUIRES
		BRIEF_DOCS "A list of required options for ${option}"
		FULL_DOCS "A list of variables which must be true if ${option} is true-ish"
	)
	define_property(GLOBAL PROPERTY ${option}_CONFLICTS
		BRIEF_DOCS "A list of conflicting options for ${option}"
		FULL_DOCS "A list of variables which must be false if ${option} is true-ish"
	)
	define_property(GLOBAL PROPERTY ${option}_REQUIRES_EXPR
		BRIEF_DOCS "An expression which must be true if ${option} is enabled."
		FULL_DOCS " "
	)
endfunction(omr_declare_option)

function(omr_option_requires option)
	set_property(GLOBAL APPEND PROPERTY ${option}_REQUIRES ${ARGN})
endfunction(omr_option_requires)

function(omr_option_conflicts option)
	set_property(GLOBAL APPEND PROPERTY ${option}_CONFLICTS ${ARGN})
endfunction(omr_option_conflicts)

function(omr_option_requires_expr option)
	get_property(expr_set GLOBAL PROPERTY ${option}_REQUIRES_EXPR SET)
	omr_assert(
		TEST NOT expr_set
		MESSAGE "More than one REQUIRES_EXPR defined for option ${option}"
	)
	set_property(GLOBAL PROPERTY ${option}_REQUIRES_EXPR ${ARGN})
endfunction(omr_option_requires_expr)

function(omr_add_option option group)
	cmake_parse_arguments(ARG "" "COMMENT" "DEFAULT;REQUIRES;CONFLICTS;REQUIRES_EXPR" ${ARGN})

	if (DEFINED ARG_COMMENT)
		set(comment ${ARG_COMMENT})
	else()
		set(comment "OMR: undocumented public option")
	endif()

	omr_assert(
		TEST DEFINED ARG_DEFAULT
		MESSAGE "Missing DEFAULT value for ${option}"
	)

	# Evaluate the default value as an if expression
	if(${ARG_DEFAULT})
		set(default ON)
	else()
		set(default OFF)
	endif()

	# Option setup and initialization

	omr_declare_option(${option} ${group})
	omr_group_options(${group} ${option})
	set(${option} ${default} CACHE BOOL "${comment}")

	# Register validation properties

	if(DEFINED ARG_REQUIRES)
		omr_option_requires(${option} ${ARG_REQUIRES})
	endif()

	if(DEFINED ARG_CONFLICTS)
		omr_option_conflicts(${option} ${ARG_CONFLICTS})
	endif()

	if(DEFINED ARG_REQUIRES_EXPR)
		omr_option_requires_expr(${option} ${ARG_REQUIRES_EXPR})
	endif()
endfunction(omr_add_option)

# _omr_check_option_requirements(<option>)
# Assert that every required option is enabled.
function(_omr_check_option_requirements option)
	get_property(requirements GLOBAL PROPERTY ${option}_REQUIRES)
	foreach(requirement IN LISTS requirements)
		omr_assert(
			TEST ${requirement}
			MESSAGE "option ${option} requires ${requirement}"
		)
	endforeach(requirement)
endfunction(_omr_check_option_requirements)

# _omr_check_option_conflicts(<option>)
# Assert that every conflicting option is disabled.
function(_omr_check_option_conflicts option)
	get_property(conflicts GLOBAL PROPERTY ${option}_CONFLICTS)
	foreach(conflict IN LISTS conflicts)
		omr_assert(
			TEST NOT ${conflict}
			MESSAGE "option ${option} conflicts with ${conflict}"
		)
	endforeach(conflict)
endfunction(_omr_check_option_conflicts)

# _omr_check_option_requirement_expr(<option>)
# if the option has a REQUIRE_EXPR defined, assert that the expression is true.
function(_omr_check_option_requirement_expr option)
	get_property(expr_set GLOBAL PROPERTY ${option}_REQUIRES_EXPR SET)
	if(expr_set)
		get_property(expr GLOBAL PROPERTY ${option}_REQUIRES_EXPR)
		omr_assert(
			TEST ${expr}
			MESSAGE "option ${option} REQUIRES_EXPR failed"
			
		)
	endif(expr_set)
endfunction(_omr_check_option_requirement_expr)

function(omr_validate_option option)
	if(${option})
		_omr_check_option_requirements(${option})
		_omr_check_option_conflicts(${option})
		_omr_check_option_requirement_expr(${option})
	endif(${option})
endfunction(omr_validate_option)

function(omr_validate_option_group)
	foreach(group IN LISTS ARGN)
		_omr_get_option_group(options ${group})
		foreach(option IN LISTS options)
			omr_validate_option(${option})
		endforeach(option)
	endforeach(group)
endfunction(omr_validate_option_group)
