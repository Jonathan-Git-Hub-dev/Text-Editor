#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "J_terminal.h"
#include "J_stack.h"

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem* end : stack object

        Does: checks if stack is empty or not

        Returns: bool
		true : stack is empty
		false : stack is not empty
*/
bool isEmptyS(struct stackItem* end)
{
        if(end == NULL)
        {
                return true;
        }
        return false;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem** end : stack holder
		void* data : new stack node's payload

        Does: add new stack node

        Returns: bool
		false : fail
		true : success
*/
bool addS(struct stackItem** end, void* data)
{
        struct stackItem* newS = malloc(sizeof(struct stackItem));
        if(newS==NULL)
        {
        	jError("malloc");
		return false;
	}
        newS->prev=*end;
        newS->data=data;

        *end = newS;
	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem** end : source stack object

        Does: removes and returns top of stack

        Returns: void*
		NULL : empty stack
		!NULL : data
*/
void* popS(struct stackItem** end)
{
        if(isEmptyS(*end))
        {
                return NULL;
        }

        void* data = (*end)->data;
        struct stackItem *temp = (*end);
        (*end)=(*end)->prev;
        free(temp);
        return data;

}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem* end : source stack objects

        Does: reads off top of stack

        Returns: void*

*/
void* readS(struct stackItem* end)
{
	if(isEmptyS(end))
        {
                return NULL;
        }

        return end->data;
}

/*int main(void)
{
	//Unit Testing

	struct stackItem *s = NULL;
	int d1 = 40002;
	int d2 = 555;

	//testing addS()
	printf("addS was able to malloc %s\n", addS(&s, &d1)? "true" : "false");
	printf("addS was able to malloc %s\n", addS(&s, &d2)? "true" : "false");

	//testing isEmptyS() on non empty stack
	printf("stack is empty %s\n", isEmptyS(s) ? "true" : "false");

	//testing readS()
	printf("Data held in stack is (%d)\n", *((int*)(readS(s))));//returns null so check

	//testing popS()
	int *ptr = NULL;
	ptr = popS(&s);//if pop return null this will segfault so check should be made if pop returns null
	printf("Data held in stack is (%d)\n", *(ptr));
	ptr = popS(&s);
	printf("Data held in stack is (%d)\n", *(ptr));
	//if called again will return NULL

	//testing isEmptyS() on empty stack
        printf("stack is empty %s\n", isEmptyS(s) ? "true" : "false");
	return 0;
}*/
