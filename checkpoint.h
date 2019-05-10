#ifndef CHECKPOINT_H
#define CHECKPOINT_H

struct Checkpoint{
	int index[40];
	char live[64];
};

void ReadCheckpoint(Checkpoint *checkpoint);
void WriteCheckpoint(Checkpoint *checkpoint);
int FindCleanSegment(Checkpoint *checkpoint);
int CheckNumOfCleanSegment(Checkpoint *checkpoint);

#endif