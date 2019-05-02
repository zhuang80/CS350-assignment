#include<string.h>
#include"inode.h"

void CreateInode(Inode *inode, char *filename){
	strcpy(inode->name, filename);
	//-1 represent the block is free
	for(int i=0;i<128;i++){
		inode->dblocks[i]=-1;
	}
	for(int i=0;i<380;i++){
		inode->dummy[i]=0;
	}
}