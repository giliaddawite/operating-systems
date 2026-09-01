# list/Makefile
#
# Makefile for list implementation and test file.
#
# Giliad Dawite
list: list.c main.c list.h
	gcc -Wall -Wextra list.c main.c -o list

