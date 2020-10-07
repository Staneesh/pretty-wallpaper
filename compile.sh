#!/bin/bash

error_message="Provide debug/release arguments to this script!"

if [ $# -lt 1 ] || [ $# -gt 1 ] 
then
	echo $error_message 
	exit 1
fi

mode=$1
flags=""

if [ $mode == "debug" ]
then
	flags="-O0 -Wall -Wextra -Wshadow -fsanitize=undefined -lm"	
elif [ $mode == "release" ]
then
	flags="-O3 -lm"
else
	echo $error_message
	exit 1
fi

time gcc main.c -o main $flags
