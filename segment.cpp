#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>
#include<unistd.h>
#include<iostream>
#include"segment.h"

void InitSegment(Segment *segment, int id){
	//int inode_num=-1, offset=-1;
	segment->id=id;
	segment->used_num=8;
	for(int i=0;i<1024;i++){
		for(int j=0;j<2;j++){
			segment->SSB[i][j]=-1;
		}
	}
	for(int i=0;i<1024*(1024-8);i++){
		segment->buffer[i]=0;
	}
}

void WriteToDisk(Segment *segment, Checkpoint *checkpoint){
	char segment_name[32];
	int fd;
	sprintf(segment_name, "DRIVE/SEGMENT%d", segment->id);
	fd=open(segment_name, O_RDWR);
	for(int i=0;i<1024*2;i++){
		write(fd, &segment->SSB[i*sizeof(int)], sizeof(int)); 
	}
	for(int i=0;i<1024*(1024-8);i++){
		write(fd, &segment->buffer[i*sizeof(char)], sizeof(char));
	}
	close(fd);
	checkpoint->live[segment->id]=1;
	//find a new clean segment 
	int free_segment_num=FindCleanSegment(checkpoint);
	if(free_segment_num==-1) std::cerr<<"There is no segment available."<<std::endl;
	else InitSegment(segment, free_segment_num);
}