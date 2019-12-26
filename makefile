mysfs:
	gcc -o mysfs sfs_disk.c sfs_func_hw.c sfs_main.c sfs_func_ext.o

rmdisk:
	rm DISK*

disk:
	cp -a ~ksilab/oshw4/DISK* .

redisk:
	make rmdisk
	make disk

recom:
	make redisk
	rm mysfs
	make mysfs

diff:
	make redisk
	mysfs < test_touch > out1
	cp DISK1.img out1.img
	make redisk
	sfs < test_touch > out2
	diff out1 out2
	diff DISK1.img out1.img

rmout:
	rm out1 out2 out1.img

