# Simple File System  _SFS_
# ***linux env***



## **instruction:**

> * mount <disk.img>
> * mv (only renaming)
> * cd
> * ls
> * mkdir
> * touch (command to make a file)
> * rmdir (remove directory)
> * rm
> * cpin [local] [sfs]  ( copy file from local system, to SFS )
> * cpout [local] [sfs] ( copy file from SFS, to local system )
> * fsck
> * exit

## **sfs_func_ext.o :**

> * this obj have Command  ( dump, fsck , bitmap ) <br>
> * this is compiled with out -fPIC,<br>
>   so you want to use on OS compiling Default -fPIC ex) Ubuntu,<br>
>   must use "-no-pie" option<br>
> ***Refer to Makefile***


