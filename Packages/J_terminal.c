#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include "J_terminal.h"

bool altScreen = false;



/*
        Written by: Jonathan Pincus

        Parameters:
		int x : x axis of screen matrix
		int y : y axis of screen matrix

        Does: moves cursor

        Returns: N/A
*/
void moveCursor(int x, int y)
{
	(void)printf("\033[%d;%dH",y,x);
        (void)fflush(stdout);
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: makes keyboard inputs print to screen

        Returns: bool
		true : success
		false : fail
*/
bool EchoOn()
{
        struct termios t;
        if(tcgetattr(1, &t) == -1)
	{
		jError("tcgetattr");
		return false;
	}

        t.c_lflag |= ECHO;
        if(tcsetattr(1, TCSANOW, &t) == -1)
	{
		jError("tcsetattr");
		return false;
	}

	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: hides keyboard input so they dont print on screen

        Returns: bool
		true : success
		false : fail
*/
bool EchoOff()
{
        struct termios t;
        if(tcgetattr(1, &t) == -1)
	{
		jError("tcgetattr");
		return false;
	}
        t.c_lflag &= ~ECHO;
        if(tcsetattr(1, TCSANOW, &t) == -1)
	{
		jError("tcsetattr");
		return false;
	}

	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: makes stdin only return on '\n' (getchar)

        Returns: bool
		true : success
		false : fail
*/
bool tc_canon_on()
{
        struct termios t;
        if(tcgetattr(1, &t) == -1)
        {
		jError("tcgetattr");
		return false;
	}
	t.c_lflag |= ICANON;
        if(tcsetattr(1, TCSANOW, &t) == -1)
	{
		jError("tcsetattr");
		return false;
	}

	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: allow stdin to return without '\n' (getchar)

        Returns: bool
		true : success
		false : fail
*/
bool tc_canon_off()
{
        struct termios t;
        if(tcgetattr(1, &t) == -1)
        {
		jError("tcgetattr");
		return false;
	}
	t.c_lflag &= ~ICANON;
        if(tcsetattr(1, TCSANOW, &t) == -1)
	{
		jError("tcsetattr");
		return false;
	}

	return true;
}


/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: calls for use of alternate screen buffer if not doing so already

        Returns: N/A
*/
void enterAltScreen()
{
	if(altScreen == false)
	{
		printf("\033[?1049h\033[H");
		altScreen = true;
	}
}


/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: reverts back to original terminal buffer

        Returns: N/A
*/
void exitAltScreen()
{
	if(altScreen == true)
	{
		printf("\033[?1049l");
		altScreen = false;
	}
}

/*int main(void)
{
	//Unit Testing

	//test enterAltScreen()
	enterAltScreen();
	printf("you will not see me but only shortly\n");

	//testing moveCursor()
	moveCursor(10, 10);
	printf("gap created by moving cursor\n");

	//tests of echo and cannon modes can be seen by interacting with main program

	sleep(3);
	//testing J_error which contains exitAltScreen()
	jError("successful test");
	return 0;
}*/
