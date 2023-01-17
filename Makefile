all:
	make -C as
	make -C ld
	
clean:
	make -C as clean
	make -C ld clean