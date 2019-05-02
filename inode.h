#ifndef INODE_H
#define INODE_H


struct Inode{
	int size;
	char name[128];
	//at most 128 blocks data
	int dblocks[128];
	//handle with the rest space
	char dummy[380];
};

void CreateInode(Inode *inode, char *filename);

#endif