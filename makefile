all:	chess.c
	gcc -Wall -O3 -o ddlchess chess.c
	chmod a+rx ddlchess

s:	chess.c
	gcc -O3 -S chess.c

