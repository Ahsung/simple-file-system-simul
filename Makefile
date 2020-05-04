mysfs:
	gcc -o mysfs sfs_disk.c sfs_func_hw.c sfs_main.c sfs_func_ext.o
	
mysfs-no-pie:
	gcc -o mysfs sfs_disk.c sfs_func_hw.c sfs_main.c sfs_func_ext.o -no-pie


