#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

#include "J_terminal.h"
#include "J_expand_string.h"

/*
        Written by: Jonathan Pincus

        Parameters:
		sturct expandString *e : destination object

        Does: constructs exandStirng

        Returns: bool
		true : success
		false : fail
*/
bool makeES(struct expandString *e)
{
	e->current =0;
        e->max =10;
        e->str = malloc((*e).max * sizeof(char));

        if(e->str == NULL)
        {
                jError("malloc");
                return false;
        }
        e->str[0]='\0';
        return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct expandString *e : destination object

        Does: equivilant of strcpy(desintaion, "\0")

        Returns: NULL
*/
void clearES(struct expandString *e)
{
	(*e).current = 0;
	(*e).str[0] = '\0';
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct expandString *e : appends to
		char *str : is appended to previous

        Does: appends string to expandString

        Returns: int
		0 : success
		-1 : fail
*/
int appendES(struct expandString *e, char *str)
{
	while(((*e).current + (int)strlen(str)) >= (*e).max)//expand till can fit new string
	{
		char *temp = (*e).str;

		if((*e).max <= ULONG_MAX/2)
		{
			(*e).max = 2 * (*e).max;
		}
		else if((*e).max == ULONG_MAX)//alredy max
		{
			jError("Max Buffer Exceeded");
			return STATUS_ERROR;
		}
		else//still room to grow but not double
		{
			(*e).max = ULONG_MAX;
		}

        	(*e).str = malloc( (*e).max * sizeof(char) );//new room
        	if((*e).str == NULL)
		{
			jError("malloc");
			return STATUS_ERROR;
		}

		//tranfer over
		(void)strcpy((*e).str, temp);
		free(temp);
	}


	(void)strcat((*e).str, str);
	(*e).current+=strlen(str);//update size of string

	return 0;
}


/*int main(void)
{
	//Unit Testing

	//testing makeES()
	struct expandString e;
	struct expandString* Pe = &e;
	printf("makeES allocated memory succeded (%s)\n", makeES(Pe) ? "true" : "false");

	//testing appendES()
	//no malloc needed yet
	appendES(Pe, "Success");
	printf("expandString: %s\n", e.str);

	//testing clearES()
	clearES(Pe);
	printf("expandString: %s\n", e.str);

	//testing appendES()
	//this will malloc more space
	printf("appendES success: %s\n", appendES(Pe, "a line much longer that 10 so that teh string will need to be enlargened") == 0 ? "true" : "false" );
	printf("expandString: %s\n", e.str);

	return 0;
}*/
