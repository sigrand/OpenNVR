#!/bin/sh

./pargen --header-name test --module-name my_module test.par
valgrind ./.libs/lt-pargen --header-name test --module-name my_module test.par

