//
// Simple FIle System
// Student Name :
// Student Number :
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	case -11:
		printf("%s: input file size exceeds the max file size\n", message); return;
	case -12:
		printf("%s: can't open %s input file\n", message,path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

u_int32_t bitcmp(u_int8_t *BY) {
	int i;

	u_int8_t FL = (*BY) ^ 0xFF;
	for (i = 0; i < 8; i++) {
		if (BIT_CHECK(FL, i)) {
			BIT_SET((*BY), i);
			return i;
		}
	}
	return 10000;
}

u_int32_t bitmapSetBlock() {
	u_int32_t blocknum;
	u_int32_t bitmapSize = SFS_BITBLOCKS(spb.sp_nblocks);
	int i,j;
	int flag = 0;
	u_int8_t bitmapBuffer[SFS_BLOCKSIZE];

	for (i = 0; i < bitmapSize; i++) {
		disk_read(bitmapBuffer, i + 2);
		for (j = 0; j < SFS_BLOCKSIZE; j++) {
			if (bitmapBuffer[j] != 0xFF) {
				blocknum = SFS_BLOCKSIZE * i*CHAR_BIT + j * CHAR_BIT + bitcmp(&bitmapBuffer[j]);
				disk_write(bitmapBuffer,i+2);
				return blocknum;
			}
		}
	}
	return 0;
}

void bitmapErase(u_int32_t num) {
	u_int32_t bitmapSize = SFS_BITBLOCKS(spb.sp_nblocks);
	int i, j;
	int flag = 0;
	u_int8_t bitmapBuffer[SFS_BLOCKSIZE];

	u_int32_t bitmapNum = num / (SFS_BLOCKSIZE*CHAR_BIT);
	u_int32_t in_index = (num % (SFS_BLOCKSIZE*CHAR_BIT)) / CHAR_BIT;
	int biit = (num % (SFS_BLOCKSIZE*CHAR_BIT)) % CHAR_BIT;

	disk_read(bitmapBuffer,bitmapNum+2);

	u_int8_t temp = 0xFF ^ (1 << biit);
	bitmapBuffer[in_index] &= temp;

	disk_write(bitmapBuffer, bitmapNum + 2);
}

void sfs_touch(const char* path)
{
	//skeleton implementation
	int i,j;
	struct sfs_inode si;
	int flag = 0;
	int dirN,block_in_index;
	disk_read( &si, sd_cwd.sfd_ino );
	u_int32_t arry[3] = { 0,0,0 };

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (flag) break;
		if (si.sfi_direct[i] == 0) {
			dirN = i;
			block_in_index = 0;
			si.sfi_direct[dirN] =arry[0] = bitmapSetBlock();
			flag = 1; break;
		}
		disk_read(sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			//빈공간 발견
			if (sd[j].sfd_ino == 0) {
				dirN = i; block_in_index = j;
				flag = 1; 
				break;
			}
			//같은 이름 발견.
			if (strcmp(path, sd[j].sfd_name) == 0) {
				error_message("touch", path, -6);
				return;
			}
		}
	}
	//direct를 전부 다돌았다.
	if (!flag) {
		error_message("touch", path, -3);
		return;
	}
	if (si.sfi_direct[dirN] == 0) {
		error_message("touch", path, -4); 
		return;
	}

	disk_read(sd, si.sfi_direct[dirN]);
	//새 블록을 할당해준 것이라면 깨끗하게 만들어준다.
	if (arry[0]) bzero(sd, sizeof(sd));

	//allocate new block
	int newbie_ino = bitmapSetBlock();
	if (newbie_ino == 0) {
		if (arry[0]) bitmapErase(arry[0]); //할당해줬던거라면 다시 반납
		error_message("touch", path, -4); 
		return;
	}

	sd[block_in_index].sfd_ino = newbie_ino;
	strncpy(sd[block_in_index].sfd_name, path, SFS_NAMELEN);

	disk_write( sd, si.sfi_direct[dirN] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write( &newbie, newbie_ino );
}

void sfs_cd(const char* path)
{
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	u_int32_t ino;
	int i, j;
	int flag = 0;
	if (path == NULL){
		sd_cwd.sfd_ino = 1;
		strncpy(sd_cwd.sfd_name, "/",SFS_NAMELEN);
		return;
	}
	else {
		disk_read(&c_inode, sd_cwd.sfd_ino);
		if (c_inode.sfi_type == SFS_TYPE_DIR) {
			for (i = 0; i < SFS_NDIRECT; i++) {
				if (c_inode.sfi_direct[i] == 0 || flag) break;
				disk_read(dir_entry, c_inode.sfi_direct[i]);
				for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
					if (dir_entry[j].sfd_ino == 0)continue;
					if (strcmp(path, dir_entry[j].sfd_name)==0) {
						ino = dir_entry[j].sfd_ino;
						flag = 1;
					}
				}
			}
			if (flag == 0) {
				error_message("cd", path, -1);
				return;
			}
		}
	}
	disk_read(&c_inode, ino);
	if (c_inode.sfi_type == SFS_TYPE_DIR) {
		sd_cwd.sfd_ino = ino;
		strncpy(sd_cwd.sfd_name, path,SFS_NAMELEN);
	}
	else {
		error_message("cd", path, -2);
		return;
	}
}

void sfs_ls(const char* path)
{
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	u_int32_t ino;
	struct sfs_inode inode;
	int i,j;
	int flag = 0;
	if (path == NULL) ino = sd_cwd.sfd_ino;
	else {
		disk_read(&c_inode, sd_cwd.sfd_ino);
		if (c_inode.sfi_type == SFS_TYPE_DIR) {
			for (i = 0; i < SFS_NDIRECT; i++) {
				if (c_inode.sfi_direct[i] == 0 || flag) break;
				disk_read(dir_entry, c_inode.sfi_direct[i]);
				for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
					if (dir_entry[j].sfd_ino == 0 )continue;
					if (strcmp(path, dir_entry[j].sfd_name)==0) {
						ino = dir_entry[j].sfd_ino;
						flag = 1;
					}
				}
			}
		}
		if(flag == 0){
			error_message("ls",path,-1);
			return;
		}
	}
	disk_read(&c_inode,ino);

	if (c_inode.sfi_type == SFS_TYPE_DIR) {
		for (i = 0; i < SFS_NDIRECT; i++) {
			if (c_inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, c_inode.sfi_direct[i]);
			int j;
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (dir_entry[j].sfd_ino == 0)continue;
				disk_read(&inode, dir_entry[j].sfd_ino);
				if (inode.sfi_type == SFS_TYPE_FILE) {
					printf("%s\t", dir_entry[j].sfd_name);
				}
				else {
					printf("%s/\t", dir_entry[j].sfd_name);
				}
			}
		}
	}
	else{
		printf("%s",path);
	}
	printf("\n");
}

void sfs_mkdir(const char* org_path) 
{
	int i, j;
	struct sfs_inode si;
	int flag = 0;
	int dirN, block_in_index;
	disk_read(&si, sd_cwd.sfd_ino);
	u_int32_t arry[3] = { 0,0,0 };

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (flag) break;
		//block이 모자르면 할당!
		if (si.sfi_direct[i] == 0) {
			dirN = i;
			block_in_index = 0;
			si.sfi_direct[dirN] = arry[0] = bitmapSetBlock();
			flag = 1; break;
		}
		disk_read(sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			//빈공간 발견
			if (sd[j].sfd_ino == 0) {
				dirN = i; block_in_index = j;
				flag = 1;
				break;
			}
			//같은 이름 발견.
			if (strcmp(org_path, sd[j].sfd_name) == 0) {
				error_message("mkdir", org_path, -6);
				return;
			}
		}
	}
	//direct를 전부 다돌았다.
	if (!flag) {
		error_message("mkdir", org_path, -3);
		return;
	}
	if (si.sfi_direct[dirN] == 0) {
		error_message("mkdir", org_path, -4);
		return;
	}

	disk_read(sd, si.sfi_direct[dirN]);
	if (arry[0]) bzero(sd, sizeof(sd));
	//dir 할당
	int newbie_ino = arry[1] = bitmapSetBlock();
	if (newbie_ino == 0) {
		if (arry[0]) bitmapErase(arry[0]); //할당해줬던거라면 다시 반납
		error_message("mkdir", org_path, -4);
		return;
	}

	sd[block_in_index].sfd_ino = newbie_ino;
	strncpy(sd[block_in_index].sfd_name, org_path, SFS_NAMELEN);

	si.sfi_size += sizeof(struct sfs_dir);

	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 2 * (sizeof(struct sfs_dir));
	newbie.sfi_type = SFS_TYPE_DIR;

	newbie.sfi_direct[0] = bitmapSetBlock();
	if(newbie.sfi_direct[0] == 0){
		if (arry[0]) bitmapErase(arry[0]);
		if (arry[1]) bitmapErase(arry[1]);
		error_message("mkdir", org_path, -4);
		return;
	}
	disk_write(sd, si.sfi_direct[dirN]);
	disk_write(&si, sd_cwd.sfd_ino);
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	bzero(dir_entry, SFS_BLOCKSIZE);

	dir_entry[0].sfd_ino = newbie_ino;
	strncpy(dir_entry[0].sfd_name, ".", SFS_NAMELEN);

	dir_entry[1].sfd_ino = sd_cwd.sfd_ino;
	strncpy(dir_entry[1].sfd_name, "..", SFS_NAMELEN);

	disk_write(dir_entry, newbie.sfi_direct[0]);
	disk_write(&newbie, newbie_ino);
}


void sfs_rmdir(const char* org_path) 
{
	if(strncmp(org_path,".",SFS_NAMELEN)==0){
		error_message("rmdir",org_path,-8);
		return;
	}
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_inode L_inode;
	struct sfs_dir L_dir_entry[SFS_DENTRYPERBLOCK];
	u_int32_t ino;
	int i, j;
	int flag = 0;
	u_int32_t eraseDirEntryBlock;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	assert(c_inode.sfi_type == SFS_TYPE_DIR);

	//지울 dir 찾는다.
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (c_inode.sfi_direct[i] == 0 || flag) break;
		disk_read(dir_entry, c_inode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (dir_entry[j].sfd_ino == 0)continue;
			if (strcmp(org_path, dir_entry[j].sfd_name) == 0) {
				ino = dir_entry[j].sfd_ino; //지울 dir의 inode

				//dir을 지우게 확정됬을때 리스트에서 지우기 위해 임시 저장
				//현재 dir의 list를 L_dir_entry로 diskwirte
				disk_read(L_dir_entry, c_inode.sfi_direct[i]);
				L_dir_entry[j].sfd_ino = SFS_NOINO;
				eraseDirEntryBlock = c_inode.sfi_direct[i]; //어느 블록에 업데이트할지.
				flag = 1;
			}
		}
	}
	//없는 경우
	if (!flag) {error_message("rmdir", org_path, -1); return;}

	disk_read(&L_inode, ino);
	// dir이 아닌경우
	if (L_inode.sfi_type != SFS_TYPE_DIR) {error_message("rmdir", org_path, -5); return;}

	//empty check
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (L_inode.sfi_direct[i] == 0 ) break;
		disk_read(dir_entry, L_inode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (i == 0 && (j == 0 || j == 1)) continue;
			if (dir_entry[j].sfd_ino != 0) {
				error_message("rmdir", org_path, -7);
				return;
			}
		}
	}

	//release
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (L_inode.sfi_direct[i] == 0) break;
		bitmapErase(L_inode.sfi_direct[i]);
	}

	c_inode.sfi_size -= sizeof(struct sfs_dir);
	// dir inode  bitmap에서 free
	bitmapErase(ino);
	// current dir 엔트리에서 삭제된 inode 업데이트 write
	disk_write(&c_inode, sd_cwd.sfd_ino);
	disk_write(L_dir_entry, eraseDirEntryBlock);
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_dir L_dir_entry[SFS_DENTRYPERBLOCK];
	int i, j;
	int flag = 0;
	u_int32_t updateDirEntryBlock;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	assert(c_inode.sfi_type == SFS_TYPE_DIR);

	disk_read(dir_entry,11);


	for (i = 0; i < SFS_NDIRECT; i++) {
		if (c_inode.sfi_direct[i] == 0) break;
		disk_read(dir_entry, c_inode.sfi_direct[i]);

		//한블록씩 검사
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (dir_entry[j].sfd_ino == 0)continue;
			if (strncmp(src_name, dir_entry[j].sfd_name,SFS_NAMELEN) == 0) {

				// 수정할 entry를 L_dir_entry에 저장
				// update 확정은 dstname의 중복검사가 끝난후.
				disk_read(L_dir_entry, c_inode.sfi_direct[i]);
				strcpy(L_dir_entry[j].sfd_name, dst_name);
				//업데이트할 블록번호 저장
				updateDirEntryBlock = c_inode.sfi_direct[i];

				flag = 1; //src를 찾았다는 flag
			}
			if (strcmp(dst_name, dir_entry[j].sfd_name) == 0) {
				error_message("mv", dst_name, -6);
				return;
			}
		}
	}
	//src를 찾지 못했으면 error
	if (!flag) {
		error_message("mv", src_name, -1);
		return;
	}
	//조건이 맞으면
	//수정했던 엔트리를 disk write
	disk_write(L_dir_entry, updateDirEntryBlock);
}

void sfs_rm(const char* path) 
{
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_inode L_inode;
	struct sfs_dir L_dir_entry[SFS_DENTRYPERBLOCK];
	u_int32_t ino;
	int i, j;
	int flag = 0;
	u_int32_t eraseDirEntryBlock;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	assert(c_inode.sfi_type == SFS_TYPE_DIR);

	//지울 file의 inode 찾는다.
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (c_inode.sfi_direct[i] == 0 || flag) break;
		disk_read(dir_entry, c_inode.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (dir_entry[j].sfd_ino == 0)continue;
			if (strcmp(path, dir_entry[j].sfd_name) == 0) {
				ino = dir_entry[j].sfd_ino; //inode 저장

				//현 dir의 리스트에서 지운후 diskwrite하기 위한 객체들
				//마지막에 file이 지우는게 확정될때 write한다.
				disk_read(L_dir_entry, c_inode.sfi_direct[i]);
				L_dir_entry[j].sfd_ino = SFS_NOINO;
				eraseDirEntryBlock = c_inode.sfi_direct[i];
				flag = 1;
			}
		}
	}
	//없는 경우
	if (!flag) { error_message("rm", path, -1); return; }

	disk_read(&L_inode, ino);
	// dir이 아닌경우
	if (L_inode.sfi_type != SFS_TYPE_FILE) { error_message("rm", path, -9); return; }


	//direct block 모두 release
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (L_inode.sfi_direct[i] == 0) break;
		bitmapErase(L_inode.sfi_direct[i]);
	}
	//indirect가 있는 경우
	if (L_inode.sfi_indirect) {
		u_int32_t indirect[SFS_DBPERIDB];
		disk_read(indirect, L_inode.sfi_indirect);
		//각 블록들 free
		for (i = 0; i < SFS_DBPERIDB; i++) {
			if(indirect[i]) bitmapErase(indirect[i]);
		}
		//indirect 블록 free
		bitmapErase(L_inode.sfi_indirect);
	}

	c_inode.sfi_size -= sizeof(struct sfs_dir);
	// dir inode  bitmap에서 free
	bitmapErase(ino);
	// current dir 엔트리에서 삭제된 inode 업데이트 write
	disk_write(&c_inode, sd_cwd.sfd_ino);
	disk_write(L_dir_entry, eraseDirEntryBlock);
}

void sfs_cpin(const char* local_path, const char* path) 
{
	FILE * in;
	//file open 실패
	if ((in = fopen(path, "rb")) == NULL) {
		error_message("cpin", path, -12);
		return;
	}
	fseek(in, 0L, SEEK_END);
	u_int32_t fsize = ftell(in);
	//file 크기 초과
	if (fsize >= (SFS_NDIRECT*SFS_BLOCKSIZE + SFS_DBPERIDB * SFS_BLOCKSIZE)) {
		error_message("cpin", path, -11);
		return;
	}

	//skeleton implementation
	int i, j;
	struct sfs_inode si;
	int flag = 0;
	int dirN, block_in_index;
	disk_read(&si, sd_cwd.sfd_ino);
	u_int32_t arry[3] = { 0,0,0 };

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	//inode할당하기 위한 자리를 찾는다.
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (flag) break;
		if (si.sfi_direct[i] == 0) {
			dirN = i;
			block_in_index = 0;
			si.sfi_direct[dirN] = arry[0] = bitmapSetBlock();
			flag = 1;
			break;
		}
		disk_read(sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			//빈공간 발견
			if (sd[j].sfd_ino == 0) {
				dirN = i; block_in_index = j;
				flag = 1;
				break;
			}
			//같은 이름 발견.
			if (strcmp(local_path, sd[j].sfd_name) == 0) {
				error_message("cpin", local_path, -6);
				return;
			}
		}
	}
	//direct를 전부 다돌았다.
	if (!flag) {
		error_message("cpin", local_path, -3);
		return;
	}
	//만약 추가 dir entry block생성에 실패한다면
	if (si.sfi_direct[dirN] == 0) {
		error_message("cpin", local_path, -4);
		return;
	}

	disk_read(sd, si.sfi_direct[dirN]);
	//새 블록을 할당해준 것이라면 깨끗하게 만들어준다.
	if (arry[0]) bzero(sd, sizeof(sd));

	//allocate new block
	int newbie_ino = bitmapSetBlock();
	//새로운 i-node생성에 실패하면
	if (newbie_ino == 0) {
		if (arry[0]) bitmapErase(arry[0]); //할당해줬던거라면 다시 반납
		error_message("cpin", local_path, -4);
		return;
	}

	

	sd[block_in_index].sfd_ino = newbie_ino;
	strncpy(sd[block_in_index].sfd_name, local_path, SFS_NAMELEN);

	//디렉토리 entry에 파일이름 추가
	disk_write(sd, si.sfi_direct[dirN]);
	//디렉토리 추가된 엔트리만큼 사이즈 증가
	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	
	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_type = SFS_TYPE_FILE;
	
	rewind(in);
	newbie.sfi_size = 0;
	u_int32_t blockNum = fsize / SFS_BLOCKSIZE;
	u_int32_t remainB = fsize % SFS_BLOCKSIZE;
	u_int8_t blockbit[SFS_BLOCKSIZE];
	u_int32_t c_block=0;
	int bflag = 0;

	for (i = 0; i < SFS_NDIRECT; i++) {
		//최대 블록 개수 전까지만 저장..
		if (c_block < blockNum) {
			if (newbie.sfi_direct[i] == 0) newbie.sfi_direct[i] = bitmapSetBlock();
			//더이상 할당할 블록이 없는경우 여기까지 저장하고 끝,
			if (newbie.sfi_direct[i] == 0) {
				error_message("cpin", local_path, -4);
				disk_write(&newbie, newbie_ino);
				return;
			}
			fread(blockbit, SFS_BLOCKSIZE, 1, in);
			c_block++;
			disk_write(blockbit, newbie.sfi_direct[i]);
			newbie.sfi_size += SFS_BLOCKSIZE;
		}
		else {
			bflag = 1;
			break;
		}
	}
	//direct를 다 채우지못하고 블럭을 꽉채웠을 경우.
	//나머지채운다.
	if (bflag && remainB) {
		if (newbie.sfi_direct[c_block] == 0) newbie.sfi_direct[c_block] = bitmapSetBlock();
		//더이상 할당할 블록이 없는경우 여기까지 저장하고 끝,
		if (newbie.sfi_direct[i] == 0) {
			error_message("cpin", local_path, -4);
			disk_write(&newbie, newbie_ino);
			return;
		}
		bzero(blockbit, SFS_BLOCKSIZE);
		fread(blockbit, remainB, 1, in);
		disk_write(blockbit, newbie.sfi_direct[c_block]);
		newbie.sfi_size += remainB;
	}
	else if(bflag && !remainB){}
	//direct를 넘어서 indirect를 저장해야되는 경우
	else {
		newbie.sfi_indirect = bitmapSetBlock();
		//더 이상 블록할당 불가능하면 여기까지 저장하고 끝
		if (newbie.sfi_indirect == 0) {
			disk_write(&newbie, newbie_ino); 
			error_message("cpin",local_path,-4);
			return;
		}
		//하나씩 배정 시작한다.
		u_int32_t indirectB[SFS_DBPERIDB];
		int kflag = 0;
		bzero(indirectB,sizeof(indirectB));
		for (i = 0; i < SFS_DBPERIDB; i++) {
			if (c_block >= blockNum) { kflag = 1;  break; }
			indirectB[i] = bitmapSetBlock();
			//블록 더이상 할당 불가.
			if (!indirectB[i]) {error_message("cpin",local_path,-4); break;}
			fread(blockbit, SFS_BLOCKSIZE, 1, in);
			disk_write(blockbit, indirectB[i]);
			newbie.sfi_size += SFS_BLOCKSIZE;
			c_block++;
		}
		
		//블록넘만큼 돌고 나머지가 존재하면 disk_write.
		if (kflag && remainB) {
			//할당할 블록이 없으면 error만.있으면 나머지 배정
			if (indirectB[i] = bitmapSetBlock()) {
				//bzero(blockbit, SFS_BLOCKSIZE);
				fread(blockbit, remainB, 1, in);
				disk_write(blockbit,indirectB[i]);
				newbie.sfi_size += remainB;
			}else{
				error_message("cpin",local_path,-4);
			}
		}
		disk_write(indirectB, newbie.sfi_indirect);
	}
	disk_write(&newbie, newbie_ino);
}


void sfs_cpout(const char* local_path, const char* path) 
{
	struct sfs_inode c_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	int i, j;
	u_int32_t ino; //복사할 file(dir)의 inode;
	int flag = 0;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	assert(c_inode.sfi_type == SFS_TYPE_DIR);

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (c_inode.sfi_direct[i] == 0) break;
		disk_read(dir_entry, c_inode.sfi_direct[i]);

		//한블록씩 검사
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (dir_entry[j].sfd_ino == 0)continue;
			if (strcmp(local_path, dir_entry[j].sfd_name) == 0) {
				ino = dir_entry[j].sfd_ino;
				flag = 1; //src를 찾았다는 flag
			}
		}
	}
	//아무것도 찾지 못하고 다 돌았다면,
	if (!flag) {
		error_message("cpout", local_path, -1);
		return;
	}

	FILE * out;
	int check = access(path, 0);
	//file이 존재하면 종료
	if (check == 0) {
		error_message("cpout", path, -6);
		return;
	}
	out = fopen(path, "wb");

	//밖으로 복사할 파일의 inode를 가져온다.
	disk_read(&c_inode, ino);
	if (c_inode.sfi_type != SFS_TYPE_FILE) {
		error_message("cpout", local_path, -10);
		return;
	}

	u_int8_t bitblock[SFS_BLOCKSIZE];
	int blockNum = c_inode.sfi_size / SFS_BLOCKSIZE;
	//마지막에 나머지 크기만큼만 write 해야한다!
	int remainB = c_inode.sfi_size % SFS_BLOCKSIZE;
	int bflag = 0;

	for (i = 0; i < SFS_NDIRECT; i++) {
		//블럭 개수만큼만 direct에서 복사
		if (i == blockNum) { bflag = 1;  break; }
		if (c_inode.sfi_direct[i] == 0) break;
		disk_read(bitblock, c_inode.sfi_direct[i]);
		fwrite(bitblock, SFS_BLOCKSIZE, 1, out);
	}
	//마지막 블록에서 빠져나왔다면, remainB확인
	//남은 것만큼만 write후 종료
	if (bflag && remainB) {
		disk_read(bitblock, c_inode.sfi_direct[i]);
		fwrite(bitblock, remainB, 1, out);
		return;
	}
	//blocknum이 15라면 모두 꽉찬 direct,

	//indirect가 있다면 계속 더 write
	if (c_inode.sfi_indirect) {
		u_int32_t indirectB[SFS_DBPERIDB];
		disk_read(indirectB, c_inode.sfi_indirect);
		int kflag = 0;
		for (i = 0; i < SFS_DBPERIDB; i++) {
			if (i == blockNum-15) { kflag = 1;  break; }
			if (indirectB[i] == 0) break;
			disk_read(bitblock, indirectB[i]);
			fwrite(bitblock, SFS_BLOCKSIZE, 1, out);
		}
		if (kflag && remainB) {
			disk_read(bitblock, indirectB[i]);
			fwrite(bitblock, remainB, 1, out);
			return;
		}
	}
}


void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
