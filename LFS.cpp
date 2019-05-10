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

/*main command functions*/
void Import(char* filename, char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint);
void Remove(char* lfs_filename, Imap *imap, Segment *segment, Checkpoint *checkpoint);
void List(Imap *imap, Segment *segment);
void Shutdown(Checkpoint *checkpoint, Segment *segment);
void Init(Imap *imap, Segment *segment, Checkpoint *checkpoint);
void Cat(char* lfs_filename, Imap *imap, Segment *segment);
void Display(char* lfs_filename, int howmany, int start, Imap * imap, Segment *segment);
void Overwrite(char* lfs_filename, int howmany, int start, char c, Imap *imap, Segment *segment, Checkpoint *checkpoint);

/*other helpful functions*/
int FindInodeNumByFileName(char* lfs_filename);
void FindInodeByInodeNum(int inode_num, Inode *inode, Imap *imap, Segment *segment);

int main(int argc, char ** argv){
	Imap imap;
	Segment segment;
	Checkpoint checkpoint;
	Init(&imap, &segment, &checkpoint);

	char filename[128], lfs_filename[128], commandline[128], command[128], c;
	int result, howmany, start;
	while(fgets(commandline, 128, stdin)){
		
		 
		switch(commandline[0]){
			case 'i':
				result=sscanf(commandline, "%s %s %s", command, filename, lfs_filename);
				if(result!=3) {
					cerr<<"The command is not right. Usage: import <filename> <lfs_filename>\n";
					break;
				}

				if(CheckNumOfCleanSegment(&checkpoint)<=10){
					 Cleaning(&imap, &segment, &checkpoint);
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
				result=sscanf(commandline, "%s %s", command, lfs_filename);
				if(result!=2){ 
					cerr<<"The command is not right. Usage: cat <lfs_filename>\n";
					break;				
				}
				Cat(lfs_filename, &imap, &segment);
				break;
			case 'd':
				result=sscanf(commandline, "%s %s %d %d", command, lfs_filename, &howmany, &start);
				if(result!=4){
					cerr<<"The command is not right. Usage: display <lfs_filename> <howmany> <start>\n";
					break;
				}
				if(start<=0) {
					cerr<<"The start argument must be greater than 0\n";
					break;
				}
				Display(lfs_filename, howmany, start, &imap, &segment);
				break;
			case 'o':
				result=sscanf(commandline, "%s %s %d %d %c", command, lfs_filename, &howmany, &start, &c);
				if(result!=5){
					cerr<<"The command is not right. Usage: overwrite <lfs_filename> <howmany> <start> <c>\n";
					break;
				}
				Overwrite(lfs_filename, howmany, start, c, &imap, &segment, &checkpoint);
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
	int fileNameMap_fd, file_num, inode_num=-1, segment_num, segment_fd, offset, imap_num;
	char name[128], buffer[128];
	char *temp=(char*)calloc(sizeof(char)*128, 0);
	char segment_name[32];
	strcpy(name, lfs_filename);

	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDONLY, 0777);
	read(fileNameMap_fd, &file_num, sizeof(int));
	for(int i=0;i<file_num;i++){
		read(fileNameMap_fd, buffer, 128);
		if(strcmp(name, buffer)==0){
			inode_num=i;
			break;
		}
	}
	close(fileNameMap_fd);
	if(inode_num==-1){ 
		cerr<<"No found. Failure to remove such file."<<endl;
		return;
	}

	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_WRONLY, 0777);
	lseek(fileNameMap_fd, 4+inode_num*128, SEEK_SET);
	write(fileNameMap_fd, temp, 128);
	imap->index[inode_num]=-1;
	imap_num=inode_num%256;
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

			//cout<<"block_num "<<block_num<<" "<<"seg_num "<<segment_num<<" "<<"offset "<<offset<<endl;
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

int FindInodeNumByFileName(char* lfs_filename){
	int fileNameMap_fd, file_num, inode_num=-1;
	char buffer[128];
	fileNameMap_fd=open("DRIVE/FILENAMEMAP", O_RDONLY, 0777);
	read(fileNameMap_fd, &file_num, sizeof(int));
	for(int i=0;i<file_num;i++){
		read(fileNameMap_fd, buffer, 128);
		if(strcmp(lfs_filename, buffer)==0){
			inode_num=i;
			break;
		}
	}
	close(fileNameMap_fd);
	return inode_num;
}

void FindInodeByInodeNum(int inode_num, Inode *inode, Imap *imap, Segment *segment){
	int block_num, segment_num, offset, segment_fd;
	char segment_name[32];
	block_num=imap->index[inode_num];
	segment_num = block_num/1024;
	offset = block_num%1024;

	if(segment_num==segment->id){
		int index=(offset-8)*1024;
		memcpy(inode, &segment->buffer[index], sizeof(Inode)); 
	}
	else{
		sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
		segment_fd=open(segment_name, O_RDONLY, 0777);
		lseek(segment_fd, offset*1024, SEEK_SET);
		read(segment_fd, inode, sizeof(Inode));
		close(segment_fd);
	}
}

void Cat(char* lfs_filename, Imap *imap, Segment * segment){
	Inode inode;
	int inode_num;
	
	inode_num=FindInodeNumByFileName(lfs_filename);
	if(inode_num==-1){ 
		cerr<<"No found. Failure to cat such file."<<endl;
		return;
	}
	
	FindInodeByInodeNum(inode_num, &inode, imap, segment);
	/*display the contents of the file*/
	int segment_num, segment_fd, offset;
	char blockBuffer[1024], segment_name[32];
	for(int i=0;i<128;i++){
		for(int j=0;j<1024;j++) blockBuffer[j]=0;
		if(inode.dblocks[i]!=-1){
			segment_num=inode.dblocks[i]/1024;
			offset=inode.dblocks[i]%1024;
			if(segment_num==segment->id){
				int index=(offset-8)*1024;	
				memcpy(blockBuffer, &segment->buffer[index], 1024);			
			}
			else{
				sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
				segment_fd=open(segment_name, O_RDONLY, 0777);
				lseek(segment_fd, offset*1024, SEEK_SET);
				read(segment_fd, blockBuffer, 1024);
				close(segment_fd);		
			}
			printf("%.1024s", blockBuffer);	
		}else{
			break;		
		}
	}

}

void Display(char* lfs_filename, int howmany, int start, Imap * imap, Segment *segment){
	Inode inode;
	int inode_num;
	inode_num=FindInodeNumByFileName(lfs_filename);
	if(inode_num==-1){ 
		cerr<<"No found. Failure to display such file."<<endl;
		return;
	}
	FindInodeByInodeNum(inode_num, &inode, imap, segment);
	if(start>inode.size){
		cerr<<"The start byte exceeds the size of file. Please enter an appropriate value.\n";
		return;
	}

	int block_offset, byte_offset, rest_bytes, max_bytes, segment_num, segment_fd, offset;
	char blockBuffer[1024], segment_name[32];
	block_offset=(start-1)/1024;
	byte_offset=(start-1)%1024;
	rest_bytes=howmany;
	while(rest_bytes>0 && block_offset<=127){
		for(int i=0;i<1024;i++) blockBuffer[i]=0;
		max_bytes=1024-byte_offset;

		if(rest_bytes>max_bytes) {
			if(inode.dblocks[block_offset]!=-1){
				segment_num=inode.dblocks[block_offset]/1024;
				offset=inode.dblocks[block_offset]%1024;
				if(segment_num==segment->id){
					int index=(offset-8)*1024 + byte_offset;	
					memcpy(blockBuffer, &segment->buffer[index], max_bytes);
				}
				else{
					sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
					segment_fd=open(segment_name, O_RDONLY, 0777);
					lseek(segment_fd, offset*1024 + byte_offset, SEEK_SET);
					read(segment_fd, blockBuffer, max_bytes);
					close(segment_fd);		
				}
			}
			rest_bytes=rest_bytes-max_bytes;
		}
		else{
			if(inode.dblocks[block_offset]!=-1){
				segment_num=inode.dblocks[block_offset]/1024;
				offset=inode.dblocks[block_offset]%1024;
				if(segment_num==segment->id){
					int index=(offset-8)*1024 + byte_offset;	
					memcpy(blockBuffer, &segment->buffer[index], rest_bytes);
				}
				else{
					sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
					segment_fd=open(segment_name, O_RDONLY, 0777);
					lseek(segment_fd, offset*1024 + byte_offset, SEEK_SET);
					read(segment_fd, blockBuffer, rest_bytes);
					close(segment_fd);		
				}
			}
			rest_bytes=0;
		}
		printf("%.1024s", blockBuffer);
		//only the first one needs to start from byte_offset 
		byte_offset=0;
		block_offset++;
	}
}

void Overwrite(char* lfs_filename, int howmany, int start, char c, Imap *imap, Segment *segment, Checkpoint *checkpoint){
	Inode inode;
	int inode_num;
	inode_num=FindInodeNumByFileName(lfs_filename);
	if(inode_num==-1){ 
		cerr<<"No found. Failure to display such file."<<endl;
		return;
	}
	FindInodeByInodeNum(inode_num, &inode, imap, segment);

	if(start>inode.size){
		cerr<<"The start byte exceeds the size of file. Please enter an appropriate value.\n";
		return;
	}
	if((start+howmany-1)>131072){
		cerr<<"The sum of start and howmany exceeds the maximum size 128 blocks. Please enter appropriate values\n";
		return;
	}
	if((start+howmany-1)>inode.size){
		inode.size=start+howmany-1;
	}

	int block_offset, byte_offset, rest_bytes, max_bytes, segment_num, segment_fd, offset;
	char blockBuffer[1024], segment_name[32];
	block_offset=(start-1)/1024;
	byte_offset=(start-1)%1024;
	rest_bytes=howmany;

	while(rest_bytes>0 && block_offset<=127){
		for(int i=0;i<1024;i++) blockBuffer[i]=0;
		max_bytes=1024-byte_offset;
		//read the block which we will overwrite next
		if(inode.dblocks[block_offset]!=-1){
			segment_num=inode.dblocks[block_offset]/1024;
			offset=inode.dblocks[block_offset]%1024;
			if(segment_num==segment->id){
				int index=(offset-8)*1024;	
				memcpy(blockBuffer, &segment->buffer[index], 1024);
			}
			else{
				sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
				segment_fd=open(segment_name, O_RDONLY, 0777);
				lseek(segment_fd, offset*1024, SEEK_SET);
				read(segment_fd, blockBuffer, 1024);
				close(segment_fd);		
			}
		}

		if(rest_bytes>max_bytes) {
			if(inode.dblocks[block_offset]!=-1){
				for(int i=byte_offset;i<1024;i++) blockBuffer[i]=c;
			}
			else{
				for(int i=0;i<1024;i++) blockBuffer[i]=c;
			}
			rest_bytes=rest_bytes-max_bytes;
		}else{
			if(inode.dblocks[block_offset]!=-1){
				for(int i=byte_offset;i<rest_bytes+byte_offset;i++) blockBuffer[i]=c;
			}
			else{
				for(int i=0;i<rest_bytes;i++) blockBuffer[i]=c;
			}
			rest_bytes=0;
		}
		/*write the updated data block to in-memory segment, and update inode information*/
		memcpy(&segment->buffer[(segment->used_num-8)*1024], blockBuffer, 1024);
		segment->SSB[segment->used_num-8][0]=inode_num;
		segment->SSB[segment->used_num-8][1]=block_offset;
		inode.dblocks[block_offset]=segment->id*1024+segment->used_num;
		segment->used_num++;
		if(segment->used_num==1024) WriteToDisk(segment, checkpoint);

		byte_offset=0;
		block_offset++;
	}

	memcpy(&segment->buffer[(segment->used_num-8)*1024], &inode, sizeof(Inode));
	
	segment->SSB[segment->used_num-8][0]=-1;
	segment->SSB[segment->used_num-8][1]=inode_num;
	imap->index[inode_num]=segment->id*1024+segment->used_num;
	segment->used_num++;
	if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
	/*update checkpoint and write the corresponding piece of imap to in-memory segment
	each piece of imap contains 256 inode numbers 
	*/
	int imap_num=inode_num/256;
	memcpy(&segment->buffer[(segment->used_num-8)*1024], &imap->index[imap_num*1024], 1024);
	segment->SSB[segment->used_num-8][0]=-2;
	segment->SSB[segment->used_num-8][1]=imap_num;
	checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
	segment->used_num++;
	if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
}
