all:
	make -C as
	make -C ld
	make -C reloc
	
clean:
	make -C as clean
	make -C ld clean
	make -C reloc clean