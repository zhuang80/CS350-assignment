#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<iostream>
#include"checkpoint.h"
using namespace std;

void ReadCheckpoint(Checkpoint *checkpoint){
	int checkpoint_fd, block_num;
	char live;
	checkpoint_fd=open("DRIVE/CHECKPOINT_REGION", O_RDONLY, 0777);
	for(int i=0;i<40;i++){
		read(checkpoint_fd, &block_num, sizeof(int));
		checkpoint->index[i]=block_num;
	}
	for(int i=0;i<64;i++){
		read(checkpoint_fd, &live, sizeof(char));
		checkpoint->live[i]=live;
	}
	close(checkpoint_fd);
}

void WriteCheckpoint(Checkpoint *checkpoint){
	int checkpoint_fd;
	checkpoint_fd=open("DRIVE/CHECKPOINT_REGION", O_RDWR);
	for(int i=0;i<40;i++){
		write(checkpoint_fd, &checkpoint->index[i], sizeof(int));
		
	}
	for(int i=0;i<64;i++){
		write(checkpoint_fd, &checkpoint->live[i], sizeof(char));
	}
	close(checkpoint_fd);

}

int FindCleanSegment(Checkpoint *checkpoint){
	for(int i=0;i<64;i++){
		if(checkpoint->live[i]==0){
			return i;
		}
	}
	return -1;
}

int CheckNumOfCleanSegment(Checkpoint *checkpoint){
	int count=0;
	for(int i=0;i<64;i++){
		if(checkpoint->live[i]==0) count++;
	}
	return count;
}