all:
	make -C as
	make -C ld
	make -C reloc
	make -C nm
	
clean:
	make -C as clean
	make -C ld clean
	make -C reloc clean
	make -C nm clean