#include <stdio.h>
#include <stdbool.h>
#include "J_stack.h"
#include "J_comment.h"

/*
        Written by: Jonathan Pincus

        Parameters:
		struct comment **ptr : destination object
		int a : initial data

        Does: sets a to param and b to imposible value of -1

        Returns: NULL
*/
void constructorS(struct comment **ptr, size_t a)
{
        (*ptr)->a=a;
        (*ptr)->b=0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem *end : stack holding comment object

        Does: check if b != -1 meaning comment object is complete

        Returns:
		ture : not complete
		false : complete
*/
bool halfS(struct stackItem *end)
{
        void *vp = end->data;
        struct comment *temp = (struct comment*)vp;
        if(temp->b == 0)//b is always atleast a+1 so 0 is impossible for b
        {
                return true;
        }
        return false;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct stackItem **end : stack for comment object
		int b : new value of b

        Does: acts as setter for b element of comment

        Returns: NULL
*/
void addRestS(struct stackItem **end, size_t b)
{
        struct comment *temp = (*end)->data;
        temp->b=b;
        (*end)->data= temp;
}


/*int main(void)
{
	//Unit Testing

	//testing constructorS()
	struct comment c;
	struct comment* Pc = &c;
	constructorS(&Pc, 72);
	printf("After construction A: %zu\n", c.a);

	struct stackItem s;
	s.data = Pc;
	struct stackItem* Ps = &s;

	//testing halfS()
	printf("When comment is only half full halfs returns (%s)\n", halfS(Ps) ? "true" : "false");

	//testing addRestS()
	addRestS(&Ps, 1005);
	printf("After addRest A: %zu\n", c.b);

	//testing halfS() now that comment is full
	printf("When comment is full halfs returns (%s)\n", halfS(Ps) ? "true" : "false");

	return 0;
}*/

