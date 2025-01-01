#ifndef J_STACK_H
#define J_STACK_H

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: used to impement stack

        Retrurns: N/A
*/
struct stackItem{   // Structure declaration
        void* data;
        struct stackItem *prev;//points to next
};

//documentation in .c file for space reasons
bool isEmptyS(struct stackItem* end);
bool addS(struct stackItem** end, void* data);
void* popS(struct stackItem** end);
void* readS(struct stackItem* end);

#endif
