compile:
	gcc -c main.c z_compressor.c init.c commit.c add_remove.c auxiliary_functions.c log.c
	gcc main.o z_compressor.o init.o commit.o add_remove.o auxiliary_functions.o log.c -o fuk -lcrypto -lz
