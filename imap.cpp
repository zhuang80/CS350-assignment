#include<iostream>
#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include"imap.h"

void ReadImap(Imap *imap){
	int checkpoint_fd, segment_fd, segment_num, offset;
	int block_num;
	char segment_name[32];

	checkpoint_fd=open("DRIVE/CHECKPOINT_REGION", O_RDONLY, 0777);
	for(int i=0;i<40;i++){
		read(checkpoint_fd, &block_num, sizeof(int));
		//each segment(1MB) contain 1024 blocks(1KB)
		segment_num=block_num/1024;
		//offset inside the segment 
		offset=block_num%1024;
		sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
		segment_fd=open(segment_name, O_RDONLY, 0777);
		lseek(segment_fd, offset*1024, SEEK_SET);
		//each block contain 1024/4=256 4-bit block number(inode location)
		for(int j=0;j<256;j++){
			read(segment_fd, &imap->index[i*256+j], sizeof(int));
		} 
		close(segment_fd);
	}
	close(checkpoint_fd);
}
