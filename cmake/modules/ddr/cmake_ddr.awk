###############################################################################
# Copyright (c) 2018, 2020 IBM Corp. and others
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

function get_basename() {
	n = split(FILENAME, array, "/")
	split(array[n], filename_array, ".")

	# capitalize the first letter
	return toupper(substr(filename_array[1], 1, 1)) substr(filename_array[1], 2)
}

function set_default_namespaces() {
	basename = get_basename()
	const_space = basename "Constants"
	flag_space = basename "Flags"
}

function set_map_to_type_namespaces() {
	match_str = "@ddr_namespace: map_to_type="
	idx = match($0, /@ddr_namespace: ?map_to_type=/)
	if (!idx) {
		print "XXXXXXXXXXX ERROR XXXXXXXXXXXXXXXX"
	}
	type_id = substr($0, idx + RLENGTH)
	if (match(type_id, /[A-Za-z][A-Za-z0-9_]*/) != 1) {
		print "XXXXXXXXXXX ERROR XXXXXXXXXXXXXXXX"
	}
	type_id = substr(type_id, 1, RLENGTH)
	const_space = type_id
	sub(/Constants$/, "Flags", type_id)
	flag_space = type_id
}

function begin_namespace(new_namespace) {
	if (new_namespace != namespace) {
		namespace = new_namespace
		print "TYPE_" namespace
	}
}

function write_define(macroname, outputname) {
	print "#ifdef " macroname
	print outputname " 1"
	print "#else"
	print outputname " 0"
	print "#endif"
}

function write_flag_macro(macro_name) {
	begin_namespace(const_space)
	write_define(macro_name, "MACRO_" macro_name)
}

function write_flag_defined(macro_name) {
	begin_namespace(flag_space)
	write_define(macro_name, "MACRO_" macro_name "_DEFINED")
}

function define_flag_macro(macro_name) {
	# Don't process any include guard macros.
	if (match(macro_name, /_[Hh]([Pp]{2})?_?$/) == 0) {
		write_flag_macro(macro_name)
		write_flag_defined(macro_name)
	}
}

function undef_flag_macro(macro_name) {
	write_flag_macro(macro_name)
	write_flag_defined(macro_name)
}

function define_value_macro(macro_name) {
	if (add_values) {
		begin_namespace(const_space)
		print "#ifdef " macro_name
		print "MACRO_"macro_name, macro_name
		print "#endif"
	}

	if (add_flags) {
		write_flag_defined(macro_name)
	}
}

function begin_file(filename) {
	if (current_file) {
		print "DDRFILE_END " filename
	}
	if (filename) {
		print "DDRFILE_BEGIN " filename
	}
	current_file = filename

	add_values = 1
	add_flags = 0
}

BEGIN {
	macro_name = ""
	idx = 0
	add_values = 0
	add_flags = 0
	pending_macro = ""
	continuation = 0
}

END {
	print "DDRFILE_END " FILENAME
}

NR == 1 {
	begin_file(FILENAME)
}

/@ddr_options: *valuesonly/ {
	add_values = 1
	add_flags = 0
}

/@ddr_options: *buildflagsonly/ {
	add_values = 0
	add_flags = 1
}

/@ddr_options: *valuesandbuildflags/ {
	add_values = 1
	add_flags = 1
}

/@ddr_namespace: *default/ {
	set_default_namespaces()
}

/@ddr_namespace: *map_to_type=/ {
	set_map_to_type_namespaces()
}

# basic line cleanup
{
	# replace tabs with spaces, and cut multiple spaces
	gsub(/[ \t]+/, " ")
	# clean up any space between '#' and define
	sub(/^ ?# ?define/, "#define")
	# normalize #undefs, i.e. remove excess spaces, and possible leading '/*'
	sub(/^ ?(\/\* ?)?# ?undef/, "#undef")
}

/^#define / {
	macro_name = $2
	# if this is a function style macro, ignore it
	if (macro_name ~ /\(/) {
		next
	}

	# strip C style comments
	sub(/\/\*([^*]|\*+[^\/])*(\*+\/|\*?$)/, "")
	# strip C++ style comments
	sub(/\/\/.*/, "")

	# if we have any fields beyond the '#define' and macro name, then assume it's a value
	if (NF > 2) {
		define_value_macro(macro_name)
	} else {
		define_flag_macro(macro_name)
	}
	next
}

/^#undef / {
	if (add_flags) {
		undef_flag_macro($2)
	}
}
