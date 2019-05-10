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
	//int inode_num=-3, offset=-3;
	segment->id=id;
	segment->used_num=8;
	for(int i=0;i<1024;i++){
		for(int j=0;j<2;j++){
			segment->SSB[i][j]=-3;
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
	for(int i=0;i<1024;i++){
		for(int j=0;j<2;j++){
			write(fd, &segment->SSB[i][j], sizeof(int)); 
		}
	}
	for(int i=0;i<1024*(1024-8);i++){
		write(fd, &segment->buffer[i*sizeof(char)], sizeof(char));
	}
	close(fd);
	checkpoint->live[segment->id]=1;
	std::cout<<"write to segment "<<segment->id<<std::endl;
	//find a new clean segment 
	int free_segment_num=FindCleanSegment(checkpoint);
	std::cout<<"select free segment "<<free_segment_num<<std::endl;
	if(free_segment_num==-1) std::cerr<<"There is no segment available."<<std::endl;
	else InitSegment(segment, free_segment_num);
}

void Cleaning(Imap *imap, Segment *segment, Checkpoint *checkpoint){
	Inode inode;
	char segment_name[32];
	int clena_list[10];
	int fd, block_num, imap_num, inode_num, offset, segment_offset, segment_num, segment_fd;
	Segment segment_clean;
	
	int id=0;
	while(CheckNumOfCleanSegment(checkpoint)<=63 && id<64){
		for(int i=0;i<10;i++){
			if(checkpoint->live[id]==1){
				segment_clean.id=id;	
				sprintf(segment_name, "DRIVE/SEGMENT%d", id);
				fd=open(segment_name, O_RDWR);
				read(fd, &segment_clean.SSB[0][0], 1024*8);
				read(fd, &segment_clean.buffer[0], 1024*(1024-8));
				close(fd);
				
				for(int j=0;j<1024-8;j++){
					if(segment_clean.SSB[j][0]==-2){
						imap_num=segment_clean.SSB[j][1];

						if(checkpoint->index[imap_num]==id*1024+j+8){
							memcpy(&segment->buffer[(segment->used_num-8)*1024], &segment_clean.buffer[j*1024], 1024);
							segment->SSB[segment->used_num-8][0]=-2;
							segment->SSB[segment->used_num-8][1]=imap_num;
							checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
							segment->used_num++;
							if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
						}
											
					}else if(segment_clean.SSB[j][0]==-1){
						inode_num=segment_clean.SSB[j][1];
						if(inode_num!=-1){
							block_num=imap->index[inode_num];
					
							if(block_num==id*1024+j+8){
								memcpy(&segment->buffer[(segment->used_num-8)*1024], &segment_clean.buffer[j*1024], 1024);
								segment->SSB[segment->used_num-8][0]=-1;
								segment->SSB[segment->used_num-8][1]=inode_num;
								imap->index[inode_num]=segment->id*1024+segment->used_num;
								segment->used_num++;
								if(segment->used_num==1024) WriteToDisk(segment, checkpoint);

								imap_num=inode_num/256;
								memcpy(&segment->buffer[(segment->used_num-8)*1024], &imap->index[imap_num*1024], 1024);
								segment->SSB[segment->used_num-8][0]=-2;
								segment->SSB[segment->used_num-8][1]=imap_num;
								checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
								segment->used_num++;
								if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
							}	
						}				
					}else if(segment_clean.SSB[j][0]>=0){
						inode_num=segment_clean.SSB[j][0];
						if(inode_num!=-1) {
							block_num=imap->index[inode_num];
					
							segment_num=block_num/1024;
							segment_offset=block_num%1024;

							if(segment_num==segment->id){
								int index=(segment_offset-8)*1024;
								memcpy(&inode, &segment->buffer[index], sizeof(Inode)); 
							}else if(segment_num==segment_clean.id){
								int index=(segment_offset-8)*1024;
								memcpy(&inode, &segment_clean.buffer[index], sizeof(Inode));						
							}
							else{
								sprintf(segment_name, "DRIVE/SEGMENT%d", segment_num);
								segment_fd=open(segment_name, O_RDONLY, 0777);
								lseek(segment_fd, segment_offset*1024, SEEK_SET);
								read(segment_fd, &inode, sizeof(Inode));
								close(segment_fd);
							}
							//traverse the following block to see whether there is the data block which belongs to the same file. 
							for(int k=j;k<1024-8;k++){
								if(segment_clean.SSB[k][0]==inode_num){
									offset=segment_clean.SSB[k][1];	
									if(inode.dblocks[offset]==id*1024+k+8){
										//std::cout<<"move data"<<std::endl;
										memcpy(&segment->buffer[(segment->used_num-8)*1024], &segment_clean.buffer[k*1024], 1024);
										segment->SSB[segment->used_num-8][0]=inode_num;
										segment->SSB[segment->used_num-8][1]=offset;
										inode.dblocks[offset]=segment->id*1024+segment->used_num;
										segment->used_num++;
										if(segment->used_num==1024) WriteToDisk(segment, checkpoint);							
									}	
									segment_clean.SSB[k][0]==-3;				
								}					
							}
							memcpy(&segment->buffer[(segment->used_num-8)*1024], &inode, sizeof(Inode));
							segment->SSB[segment->used_num-8][0]=-1;
							segment->SSB[segment->used_num-8][1]=inode_num;
							imap->index[inode_num]=segment->id*1024+segment->used_num;
							segment->used_num++;
							if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
	
							imap_num=inode_num/256;
							memcpy(&segment->buffer[(segment->used_num-8)*1024], &imap->index[imap_num*1024], 1024);
							segment->SSB[segment->used_num-8][0]=-2;
							segment->SSB[segment->used_num-8][1]=imap_num;
							checkpoint->index[imap_num]=segment->id*1024+segment->used_num;
							segment->used_num++;
							if(segment->used_num==1024) WriteToDisk(segment, checkpoint);
						}
					}			
				}	
				checkpoint->live[id]=0;	
			}
			id++;	
		}	
	}

}
