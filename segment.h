#ifndef SEGMENT_H
#define SEGMENT_H

#include"checkpoint.h"

struct Segment{
	int id; 
	int used_num;
	//the first 8 blocks are used for segment summary block
	
	int SSB[1024][2];
	//segment is 1MB, 
	char buffer[1024*(1024-8)];
};

void InitSegment(Segment *segment, int id);
void WriteToDisk(Segment *segment, Checkpoint *checkpoint);

#endif