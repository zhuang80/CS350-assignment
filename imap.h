#ifndef IMAP_H
#define IMAP_H


struct Imap{
	//contain no more than 10K files, so store at most 10*1024 inodes 
	int index[10*1024];	
};

void ReadImap(Imap *map);
void UpdateImap(Imap *map, int inode_num, int block_num);
int GetInodeLocation(Imap *map, int inode_num);

#endif