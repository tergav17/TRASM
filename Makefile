all:
	make -C as
	make -C ld
	make -C reloc
	make -C nm
	make -C size
	
clean:
	make -C as clean
	make -C ld clean
	make -C reloc clean
	make -C nm clean
	make -C size clean