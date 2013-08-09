#!/bin/bash

do_test ()
{
    SOURCE=$1

    echo "testing $SOURCE"
    ../scruffy $SOURCE > /dev/null 2>&1

    if test "x$?" != "x0"; then
	echo "$SOURCE failed"
	exit 0
    fi
}

do_test "cpp_1.cpp"
do_test "1.cpp"
do_test "2.cpp"
do_test "3.cpp"
do_test "4.cpp"
do_test "5.cpp"
do_test "6.cpp"
do_test "7.cpp"
do_test "8.cpp"
do_test "9.cpp"
do_test "10.cpp"

