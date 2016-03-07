#! /bin/sh
###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
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
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

## Program Usage
program_name="`basename "$0"`"
program_usage="\
Usage: $program_name <toolchain> <library> <export-file> <output-file>"

## Argument Parsing
if test "$#" -ne "4"; then
    echo "$program_usage"
    exit 1
fi

toolchain="$1"; shift
case "$toolchain" in
    gcc|msvc|xlc)
        ;;
    *)
        echo "toolchain not valid"
        echo "$program_usage"
        exit 1
        ;;
esac

library_name="$1"; shift
if test "x" = "x$library_name"; then
    echo "library cannot be empty"
    echo "$program_usage"
    exit 1
fi

export_file="$1"; shift
if test ! -e "$export_file"; then
    echo "export-file does not exit"
    echo "$program_usage"
    exit 1
fi

output_file="$1"; shift
if test "x" = "x$output_file"; then
    echo "output-file cannot be empty"
    echo "$program_usage"
    exit 1
fi

## Output Functions
open_output () {
    exec 4>$output_file
}

close_output () {
    exec 4>&-
}

output () {
    cat >&4
}

## Strip Comments

strip_comments () {
    sed "/^#/d"
}

## gcc
gcc_header="\
{
	global :"

gcc_footer="\

	local : *;
};
"

write_gcc_export_file () {
    echo "$gcc_header"
    sed 's/.*/		&;/'
    echo "$gcc_footer"
}

## xlc

write_xlc_export_file () {
    cat
}

## msvc

write_msvc_export_file () {
    echo "LIBRARY $(echo $library_name | sed -e 's/\(.*\)/\U\1/')"
    echo "EXPORTS"
    sed 's/.*/	&/'
    echo ""
}

## Main
open_output

case "$toolchain" in
    gcc)
        cat $export_file | strip_comments | write_gcc_export_file | output
        ;;
    msvc)
        cat $export_file | strip_comments | write_msvc_export_file | output
        ;;
    xlc)
        cat $export_file | strip_comments | write_xlc_export_file | output
        ;;
    *)
        # unreachable
        ;;
esac

close_output

