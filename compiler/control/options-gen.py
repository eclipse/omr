#! /usr/bin/env python

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

import argparse
import datetime
import json
import os

class OptionTable:

    # These values are used by both the python and C++ hashing function to
    # arrive at the same result. While it may currently seem unnecessary,
    # this mechanism is in place so that in the future we can apply an algorithm
    # similar to gperf's associated values array constructor to reduce or completely
    # eliminate hash table collisions.
    _char_values = {
        "0": ord('0'),
        "1": ord('1'),
        "2": ord('2'),
        "3": ord('3'),
        "4": ord('4'),
        "5": ord('5'),
        "6": ord('6'),
        "7": ord('7'),
        "8": ord('8'),
        "9": ord('9'),
        "a": ord('A'),
        "b": ord('B'),
        "c": ord('C'),
        "d": ord('D'),
        "e": ord('E'),
        "f": ord('F'),
        "g": ord('G'),
        "h": ord('H'),
        "i": ord('I'),
        "j": ord('J'),
        "k": ord('K'),
        "l": ord('L'),
        "m": ord('M'),
        "n": ord('N'),
        "o": ord('O'),
        "p": ord('P'),
        "q": ord('Q'),
        "r": ord('R'),
        "s": ord('S'),
        "t": ord('T'),
        "u": ord('U'),
        "v": ord('V'),
        "w": ord('W'),
        "x": ord('X'),
        "y": ord('Y'),
        "z": ord('Z')
    }

    # 3-way "alist" to contain our intermediate data to build the final hash table
    _hash_values = []
    _hashed_strings = []
    _table_entries = []

    def __init__(self, json_entries):
        self._num_table_entries = len(json_entries)
        self._max_bucket_size = 1
        self._populate_table(json_entries)

    def hash_string(self, key):
        """
        hash option name string to a bucket index in the final table using an
        algorithm derived from dbj2
        """
        h = 5381
        for i in key:
            h = ((h * 7) + self._char_values[i]) % (self._num_table_entries * 2)
        return h

    def get_char_values(self):
        return self._char_values

    def get_char_value(self, char):
        return self._char_values[char]

    def get_minimum_hash_value(self):
        return min(self._hash_values)

    def get_maximum_hash_value(self):
        return max(self._hash_values)

    def get_total_entries(self):
        return self._num_table_entries

    def get_hash_range(self):
        return self.get_maximum_hash_value() - self.get_minimum_hash_value()

    def get_entry_at_index(self, index):
        return self._table_entries[index]

    def get_entry_by_hash_value(self, hash_value):
        return self._table_entries[self._hash_values.index(hash_value)]

    def get_hash_value_at_index(self, index):
        return self._hash_values[index]

    def get_hash_indices(self, hash_value):
        return [i for i, x in enumerate(self._hash_values) if x == hash_value]

    def get_hash_values(self):
        return self._hash_values

    def get_hashed_string_at_index(self, index):
        return self._hashed_strings[index]

    def get_max_bucket_size(self):
        return self._max_bucket_size

    def set_max_bucket_size(self, bucket_size):
        self._max_bucket_size = bucket_size


    def _populate_table(self, json_entries):
        for entry in json_entries:
            option_name = entry['name'].lower()
            if option_name in self._hashed_strings:
                print("WARNING: " + entry['name'] + " is not an unique option!" \
                    " This may result in unexpected behavior!") #todo: exit with failure
            else:
                self._hash_values.append(self.hash_string(option_name))
                self._hashed_strings.append(option_name)
                self._table_entries.append(entry)


class OptionsGenerator:

    def __init__(self, option_table):
        self.option_table = option_table

    def write_char_values(self, writer):
        """
        Writes out the char values as array elements to be used by the c++
        hashing function, indexed by the ascii values

        """
        self._write_file_header(writer)
        index = 0
        char_values = self.option_table.get_char_values()

        while index < ord("0"):
            writer.write("0, ")
            index += 1

        writer.write("\n")

        while index <= ord("9"):
            writer.write(str(char_values[chr(index)]))
            writer.write(", ")
            index += 1

        writer.write("\n")

        while index < ord("A"):
            writer.write("0, ")
            index += 1

        writer.write("\n")

        while index <= ord("Z"):
            writer.write(str(char_values[chr(index).lower()]))
            writer.write(", ")
            index += 1

        writer.write("\n")

        while index < ord("a"):
            writer.write("0, ")
            index += 1

        writer.write("\n")

        while index <= ord("z"):
            writer.write(str(char_values[chr(index)]))
            writer.write(", ")
            index += 1

        writer.write("0\n")

    def write_hash_table_properties(self, writer):
        self._write_file_header(writer)

        writer.write("#define OPTION_TABLE_MIN_HASH " + str(self.option_table.get_minimum_hash_value()) + "\n")
        writer.write("#define OPTION_TABLE_MAX_HASH " + str(self.option_table.get_maximum_hash_value()) + "\n")
        writer.write("#define OPTION_TABLE_TOTAL_ENTRIES " + str(self.option_table.get_total_entries()) + "\n")
        writer.write("#define OPTION_TABLE_SIZE " + str((self.option_table.get_total_entries() * 2)) + "\n")
        writer.write("#define OPTION_TABLE_HASH_RANGE " + str(self.option_table.get_hash_range()) + "\n")
        writer.write("#define OPTION_TABLE_MAX_BUCKET_SIZE " + str(self.option_table.get_max_bucket_size()) + "\n")

    def write_table_entries_and_data_members(self, table_writer, member_writer, initializer_writer):
        self.option_members_written = [] # often multiple options can affect the same member, and
                                         # this is used to prevent duplication

        self._write_file_header(member_writer)
        self._write_file_header(table_writer)

        max_hash = self.option_table.get_maximum_hash_value()
        hash_values = self.option_table.get_hash_values()
        hash_value = 0
        while hash_value <= max_hash:
            if hash_value in hash_values:
                hash_indices = self.option_table.get_hash_indices(hash_value)
                if len(hash_indices) == 1:
                    self._write_non_colliding_entry(table_writer, self.option_table.get_entry_by_hash_value(hash_value))
                    self._write_option_data_member(member_writer, initializer_writer, self.option_table.get_entry_by_hash_value(hash_value))
                else:
                    self._write_colliding_entries_and_members(table_writer, member_writer, initializer_writer, hash_indices)
            else:
                self._write_non_colliding_entry(table_writer, None)
            hash_value += 1

        table_writer.write("{ }")

    # temporary mechanism to enable boolean option querying using the existing API
    def write_option_translating_switch(self, writer):
        self._write_file_header(writer)

        for option in self.option_members_written:
            writer.write("case " + option + ": return &TR::CompilerOptions::"+ option + ";\n" )

    def write_option_enum_to_char_translating_switch(self, writer):
        self._write_file_header(writer)

        for option in self.option_members_written:
            writer.write("case " + option + ": return \"" + option + "\";\n")


    def _write_file_header(self, writer):
        header_text = \
"""\
/*******************************************************************************
* Copyright (c) 2019, {0} IBM Corp. and others
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

/******************************************************************************
                            GENERATED FILE
*******************************************************************************/

"""
        writer.write(header_text.format(datetime.datetime.now().year))

    def _write_option_data_member(self, member_writer, initializer_writer, entry):
        option_member = entry["option-member"]
        if not ( option_member in self.option_members_written ): # currently writes boolean members only
            member_writer.write(entry["type"] + " " + entry["option-member"] + ";\n")
            initializer_writer.write(entry["option-member"] + "(" + entry["default"] + "),\n")
            self.option_members_written.append(option_member)

    def _write_option_table_entry(self, writer, entry):
        if entry == None:
            writer.write(" ")
        else:
            writer.write("{\n")
            writer.write("    \"" + entry["name"] + "\",\n")
            writer.write("    \"" + entry["category"] + "\",\n")
            writer.write("    \"" + entry["desc"] +  "\",\n")
            writer.write("    TR::OptionProcessors::" + entry["processing-fn"] + ",\n")
            writer.write("    OPTION_MEMBER_TO_SET(" + entry["option-member"] + "),\n")
            writer.write("    0,\n")
            writer.write("    false\n")
            writer.write("}")

    def _write_non_colliding_entry(self, writer, entry):
        writer.write("{")

        self._write_option_table_entry(writer,entry)

        writer.write("},\n")

    def _write_colliding_entries_and_members(self, table_writer, member_writer, initializer_writer, colliding_indices):
        if self.option_table.get_max_bucket_size() < len(colliding_indices):
            self.option_table.set_max_bucket_size(len(colliding_indices))

        table_writer.write("{")

        for index in colliding_indices:
            entry = self.option_table.get_entry_at_index(index)
            self._write_option_table_entry(table_writer, entry)
            self._write_option_data_member(member_writer, initializer_writer, entry)
            if index != colliding_indices[-1]:
                table_writer.write(",")

        table_writer.write("},\n")

if __name__ == "__main__":

    cl_parser = argparse.ArgumentParser()
    cl_parser.add_argument("-omroptions",type=str,
                        default="OMROptions.json",
                        help="JSON file containing OMR compiler options")
    cl_parser.add_argument("-omroptionsdir",type=str,
                        default=os.getcwd(),
                        help="Path to JSON file containing OMR compiler options")
    cl_parser.add_argument("-projoptions",type=str,
                        default="",
                        help="JSON file containing project-specific options")
    cl_parser.add_argument("-optionsdir",type=str,
                        default="",
                        help="Path to JSON file containing project-specific options")
    cl_parser.add_argument("-outputdir", type=str,
                            default="",
                            help="Path to output generated files to")
    cl_parser.add_argument("-platform",type=str,
                        default="",
                        help="Set this option to exclude/include platform-specific options") #todo

    cl_args = cl_parser.parse_args()

    if len(cl_args.outputdir) != 0:
        output_dir = cl_args.outputdir
    else:
        output_dir = cl_args.omroptionsdir

    with open(os.path.join(cl_args.omroptionsdir,cl_args.omroptions), "r") as omr_ot:
        json_entries = json.load(omr_ot)

    if len(cl_args.projoptions) != 0:
        with open(os.path.join(cl_args.optionsdir,cl_args.projoptions), "r") as proj_ot:
            json_entries = json_entries + json.load(proj_ot)

    option_table = OptionTable(json_entries)
    options_generator = OptionsGenerator(option_table)
    with open(os.path.join(output_dir,"OptionCharMap.inc"), "w") as writer:
        options_generator.write_char_values(writer)
    with open(os.path.join(output_dir,"OptionTableEntries.inc"), "w") as table_writer, \
        open(os.path.join(output_dir,"Options.inc"), "w") as member_writer, \
            open(os.path.join(output_dir, "OptionInitializerList.inc"), "w") as initializer_writer:
        options_generator.write_table_entries_and_data_members(table_writer, member_writer, initializer_writer)

    with open(os.path.join(output_dir,"OptionTableProperties.inc"), "w") as writer:
        options_generator.write_hash_table_properties(writer)

    with open(os.path.join(output_dir,"OptionTranslatingSwitch.inc"), "w") as writer:
        options_generator.write_option_translating_switch(writer)

    with open(os.path.join(output_dir,"OptionEnumToStringSwitch.inc"), "w") as writer:
        options_generator.write_option_enum_to_char_translating_switch(writer)