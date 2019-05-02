#include<iostream>
#include<string.h>
#include<sstream>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"inode.h"
#include"imap.h"
#include"segment.h"
#include"checkpoint.h"

using namespace std;

void Import(char* filename, char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint);
void Remove(char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint);
void List(Imap *imap, Segment *segment);
void Shutdown(Checkpoint *checkpoint, Segment *segment);
void Init(Imap *imap, Segment *segment, Checkpoint *checkpoint);

int main(int argc, char ** argv){
	Imap imap;
	Segment segment;
	Checkpoint checkpoint;
	Init(&imap, &segment, &checkpoint);

	char filename[128], lfs_filename[128], commandline[128], command[128];
	int result;
	while(fgets(commandline, 128, stdin)){
		switch(commandline[0]){
			case 'i':
				result=sscanf(commandline, "%s %s %s", command, filename, lfs_filename);
				if(result!=3) {
					cerr<<"The command is not right. Usage: import <filename> <lfs_filename>\n";
					break;
				}
				Import(filename, lfs_filename, &imap, &segment, &checkpoint);
				break;
			case 'r':
				result=sscanf(commandline, "%s %s", command, lfs_filename);
				if(result!=2) {
					cerr<<"The command is not right. Usage: remove <lfs_filename>\n";
					break;
				}
				Remove(lfs_filename, &imap, &segment, &checkpoint);
				break;
			case 'c':
				break;
			case 'd':
				break;
			case 'o':
				break;
			case 'l':
				List(&imap, &segment);
				break;
			case 's':
				Shutdown(&checkpoint, &segment);
				break;
			default:
				cerr<<"Failure to run. Please enter the right command."<<endl;
				break;
		}		
	}
	return 0;
}

/* create a directory DRIVE, 64 SEGMENT files,
	a FILENAMEMAP file
	
*/
void Init(Imap *imap, Segment *segment, Checkpoint *checkpoint){
	int fd, file_num=0;
	char name[32];
	int result= mkdir("DRIVE", 0777);
	//the DRIVE is created successfully, then create necessary files  
	if(result!=-1){
		for(int i=0;i<64;i++){
			sprintf(name, "DRIVE/SEGMENT%d", i);
			fd = open(name, O_CREAT | O_WRONLY, 0777);
			close(fd);
		}
		//at first, there is no file, so file_num is 0
		fd = open("DRIVE/FILENAMEMAP", O_CREAT | O_WRONLY, 0777);
		write(fd, &file_num, sizeof(int));
		close(fd);
		//initialize all imap location to -1 and free list to 0. (0-clean, 1-live)
		int temp=-1;
		fd=open("DRIVE/CHECKPOINT_REGION", O_CREAT | O_WRONLY, 0777);
		for(int i=0;i<40;i++){
			write(fd, &temp, sizeof(int));
		}
		char live=0;
		for(int i=0;i<64;i++){
			write(fd, &live, sizeof(char));
		}
		close(fd);
	}
	//The directory exist 
	ReadCheckpoint(checkpoint);
	ReadImap(imap);
	int free_segment_num=FindCleanSegment(checkpoint);
	if(free_segment_num==-1) cerr<<"There is no segment available."<<endl;
	else InitSegment(segment, free_segment_num);
}
void Import(char* filename, char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint){
	//first map the filename to a inode and update the file name map
	int fileNameMap_fd, file_num=0, free_inode_num, file_fd;
	char buffer[128], f_name[128];
	char *name=(char*)calloc(sizeof(char)*128, 0);
	strcpy(name, lfs_filename);
	strcpy(f_name, filename);

	//in the file name map, it stores the number of files at the beginning
	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDWR, 0777);
	read(fileNameMap_fd, &file_num, sizeof(int));
	
	
	//check whether there has the file of the same name
	for(int i=0;i<file_num;i++){
		int result=read(fileNameMap_fd, buffer, 128);
		if(strcmp(buffer, name)==0){
			cerr<<"Failure to import the file. There already has the same name file."<<endl;
			return;
		}
	}

	lseek(fileNameMap_fd, 4, SEEK_SET);
	free_inode_num=file_num;
	for(int i=0;i<file_num;i++){
		int result=read(fileNameMap_fd, buffer, 128);
		if(result==0 || buffer[0]==0){
			free_inode_num=i;
			break;
		}
	}
	close(fileNameMap_fd);

	Inode inode;
	CreateInode(&inode, name);

	//write to in-memory segment
	int result, offset=0;
	file_fd=open(f_name, O_RDONLY);
	if(file_fd==-1){
		cerr<<"There is no such Linux file. Please try again.\n";
		return;
	}
	while(segment->used_num<1024){
		int index=(segment->used_num-8)*1024;
		result=read(file_fd, &segment->buffer[index], 1024);
		//update SSB and inode
		if(result>0){
			segment->SSB[segment->used_num-8][0]=free_inode_num;
			segment->SSB[segment->used_num-8][1]=offset;
			inode.dblocks[offset]=segment->id*1024+segment->used_num;
			offset++;
			segment->used_num++;
		}
		if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
		if(result<1024) break;
	}
	close(file_fd);
	//update filesize of inode and imap, then write inode to in-memory segment 
	if(result==0)	inode.size=offset*1024+result;
	else if(result>0) inode.size=(offset-1)*1024+result;

	memcpy(&segment->buffer[(segment->used_num-8)*1024], &inode, sizeof(Inode));
	Inode new_inode;
	memcpy(&new_inode, &segment->buffer[(segment->used_num-8)*1024], sizeof(Inode));
	
	segment->SSB[segment->used_num-8][0]=-1;
	segment->SSB[segment->used_num-8][1]=free_inode_num;
	imap->index[free_inode_num]=segment->id*1024+segment->used_num;
	segment->used_num++;
	if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
	/*update checkpoint and write the corresponding piece of imap to in-memory segment
	each piece of imap contains 256 inode numbers 
	*/
	int imap_num=free_inode_num/256;
	memcpy(&segment->buffer[(segment->used_num-8)*1024], &imap->index[imap_num*1024], 1024);
	segment->SSB[segment->used_num-8][0]=-2;
	segment->SSB[segment->used_num-8][1]=imap_num;
	checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
	segment->used_num++;
	if(segment->used_num==1024) WriteToDisk(segment, checkpoint);

	//add the mapping of file name and inode number to the file name map
	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDWR, 0777);
	//if there is no freed inode number available, increment the file_num
	if(free_inode_num==file_num){
		file_num++;
		write(fileNameMap_fd, &file_num, sizeof(int));
	}
	lseek(fileNameMap_fd, 4+free_inode_num*128, SEEK_SET);
	write(fileNameMap_fd, name, 128);
	close(fileNameMap_fd);
}
void Remove(char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint){
	int fileNameMap_fd, file_num, block_num=-1, segment_num, segment_fd, offset, imap_num;
	char name[128], buffer[128];
	char *temp=(char*)calloc(sizeof(char)*128, 0);
	char segment_name[32];
	strcpy(name, lfs_filename);

	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDONLY, 0777);
	read(fileNameMap_fd, &file_num, sizeof(int));
	for(int i=0;i<file_num;i++){
		read(fileNameMap_fd, buffer, 128);
		if(strcmp(name, buffer)==0){
			block_num=i;
			break;
		}
	}
	close(fileNameMap_fd);
	if(block_num==-1){ 
		cerr<<"No found. Failure to remove such file."<<endl;
		return;
	}

	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_WRONLY, 0777);
	lseek(fileNameMap_fd, 4+block_num*128, SEEK_SET);
	write(fileNameMap_fd, temp, 128);
	imap->index[block_num]=-1;
	imap_num=block_num%256;
	memcpy(&segment->buffer[(segment->used_num-8)*1024], &imap->index[imap_num*1024], 1024);
	segment->SSB[segment->used_num-8][0]=-2;
	segment->SSB[segment->used_num-8][1]=imap_num;
	checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
	segment->used_num++;
	if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
}

void List(Imap *imap, Segment *segment){
	Inode inode;
	int fileNameMap_fd, segment_fd, file_num, block_num, segment_num, offset, size;
	char name[128];
	char segment_name[32];

	printf("Name            |Size(Bytes)    \n");
	printf("---------------------------------------\n");
	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDONLY, 0777);
	read(fileNameMap_fd, &file_num, sizeof(int));
	for(int i=0;i<file_num;i++){
		int result=read(fileNameMap_fd, name, 128);
		if(name[0]!=0){
			block_num=imap->index[i];
			segment_num=block_num/1024;
			offset=block_num%1024;
			//This file is not written back to drive
			if(segment_num==segment->id){
				int index=(offset-8)*1024;
				memcpy(&inode, &segment->buffer[index], sizeof(Inode)); 
			}
			else{
				sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
				segment_fd=open(segment_name, O_RDONLY, 0777);
				lseek(segment_fd, offset*1024, SEEK_SET);
				read(segment_fd, &inode, sizeof(Inode));
				close(segment_fd);
			}
			printf("%-16s|%-16d\n", name, inode.size);
			
		}
	}
	close(fileNameMap_fd);
}

void Shutdown(Checkpoint *checkpoint, Segment *segment){
	
	/*if used_num >8, that means there are some data in the in-memory segment.
	So we need write it back to disk. Otherwise, we don't need*/
	if(segment->used_num>8) WriteToDisk(segment, checkpoint);
	WriteCheckpoint(checkpoint);
	exit(0);
}
