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
#include <signal.h>
#include <poll.h>
#include <sys/stat.h>
#include <limits.h>

#include "Packages/J_file.h"
#include "Packages/J_terminal.h"
#include "Packages/J_stack.h"
#include "Packages/J_comment.h"
#include "Packages/J_expand_string.h"
#include "Packages/J_keywords.h"

#define EXCESS 4

#define WIN_MIN_ROW 14//min and max size for the terminal window
#define WIN_MIN_COL 100
#define WIN_MAX_ROW 140
#define WIN_MAX_COL 1000


#define UP 1792833//arrow inputs
#define DOWN 1792834
#define RIGHT 1792835
#define LEFT 1792836
#define CTRL_C 257//special keys
#define CTRL_D 4
#define CTRL_F 6
#define CTRL_W 23
#define ENTER_KEY 10
#define CTRL_G 7
#define CTRL_X 24
#define BACKSPACE_KEY 127
#define DELETE_KEY 458961790

#define BLANK "\033[7m-=NULL=-\033[0m\n"

#define CHEVRON_LEFT_ON  "\033[0m\x1B[1;31m<\033[0m"
#define CHEVRON_RIGHT_ON "\033[0m\x1B[1;31m>\033[0m"

#define CHEVRON_LEFT_OFF  "\033[0m\033[2m<\033[0m"
#define CHEVRON_RIGHT_OFF "\033[0m\033[2m>\033[0m"

int printMiddle(char ***lines, struct winsize *w, int *x, int *y, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, char *message, int *pipe, int first);
void printAllDull(char** lines, struct winsize w, int x, int y, struct expandString *e);
int loadfull(char** lines, struct winsize w);
int remakeFile(char** lines, struct winsize w, char *destination);
bool searchSubString(char *line, int ind, char *str);
int currentLineStart(char *str, int *x, struct winsize w);
int nearestEightDiff(int i);
int multi(int pipe[2], uint64_t *data);
int Arrows(uint64_t input, int *x, int *y, char **lines, struct winsize w, size_t *fileLine, struct stackItem **mLComment);
void nearestEight(struct winsize *w);
int saveFiles(char** ptr, struct winsize w);
bool changeWindowSize(char*** lines, struct winsize* w, int* y, size_t *fileLine, struct stackItem **mLComment);
int getMiddleLine(char *line, struct winsize *w, int *x, struct expandString *e, bool *mLC, bool current);
int printMiddleLine(char ***line, struct winsize *w, int *x, int *y, struct expandString *e, struct stackItem **mLComment, size_t *fileLine, int pipe[2]);
void reprint(struct expandString *e);


struct expandString pasteBuffer;
char message[LEN] = "";
char saveFileName[LEN] = "";

//signal flag
bool cancelFlag = false;
bool windowFlag = false;

//file paths
const char *fPU = "Temp_File_Up";
const char *fPD = "Temp_File_Down";
const char *fPS = "Temp_File_Save";
char *fPO;// points to argv[1]

/*
        Written by: Jonathan Pincus

        Parameters:
                int y : row of screen that will be cleared

        Does: clears row of screen

        Returns: N/A
*/
void removeline(int y)
{
        (void)moveCursor(1, y);
        (void)printf("\33[2K\r");
}

//void P_SigInt(int sig)//sets flag on signal
void P_SigInt()
{
	//printf("hello from cancel\n");
	//sleep(1);
        cancelFlag = true;
}

//void P_SigWinCh(int sig)//sets flag on signal
void P_SigWinCh()
{
        windowFlag = true;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: sets signal handlers for main process

        Returns: bool
		true : success
		false : fail
*/
bool mainProcessSIG()
{
	struct sigaction sa1;
        sa1.sa_handler = &P_SigInt;
        if(sigaction(SIGINT, &sa1, NULL) == STATUS_ERROR)
        {
                jError("sigaction");
                return false;
        }

        struct sigaction sa2;
        sa2.sa_handler = SIG_IGN;
	if(sigaction(SIGQUIT, &sa2, NULL) == STATUS_ERROR)
        {
                jError("sigaction");
                return false;
        }

        struct sigaction sa3;
        sa3.sa_handler = &P_SigWinCh;
        if(sigaction(SIGWINCH, &sa3, NULL) == STATUS_ERROR)
        {
                jError("sigaction");
                return false;
        }

	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: sets signal handlers for input(child) process

        Returns: bool
		true : success
		false : fail
*/
bool inputProcessSIG_IGN()
{
	struct sigaction sa1;
        sa1.sa_handler = SIG_IGN;
        if(sigaction(SIGINT, &sa1, NULL) == STATUS_ERROR)
	{
		jError("sigaction");
		return false;
	}

        struct sigaction sa2;
        sa2.sa_handler = SIG_IGN;
        if(sigaction(SIGQUIT, &sa2, NULL) == STATUS_ERROR)
        {
                jError("sigaction");
                return false;
        }

	struct sigaction sa3;
	sa3.sa_handler = SIG_IGN;
	if(sigaction(SIGWINCH, &sa3, NULL) == STATUS_ERROR)
        {
                jError("sigaction");
                return false;
        }

	return true;
}


/*
        Written by: Jonathan Pincus

        Parameters:
		int first : clear in range starting here
		int end : clear in range ending here

        Does: clears portion of content portion of screen (leave header and footer)

        Returns: N/A
*/
void clearScreenMiddle(int first, int end)
{
        for(int i = first; i < end; i++)
        {
                (void)moveCursor(1, i+3);
                (void)printf("\33[2K\r");
        }
}


/*
        Written by: Jonathan Pincus

        Parameters:
                size_t fileLine : number of lines above loaded protion of file
		struct winsize w : screen size

        Does: print number of lines above current portion of file (in header)

        Returns: N/A
*/
void printFL(size_t fileLine, struct winsize w)
{
	(void)moveCursor(w.ws_col - 19, 1);
	(void)printf("%s%020zu%s", INVERT, fileLine, INVERT_UNDO);
	(void)fflush( stdout );
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct winsize w : size of screen

        Does: prints header and footer of screen interface

        Returns: N/A
*/
void printHF(struct winsize w)
{
        char *str1 = "Name: ";
        char *str2 = " Lines^^:                     ";//(20 sapces for unsigned long)

        size_t total = w.ws_col;
	total -= strlen(str1);
        total -= strlen(str2);//total holds room left for name

        char str3[LEN];
        if(strlen(fPO) > total)//file to long to have full name in header
        {
                (void)sprintf(str3, "%.*s..", (int)(total-2), fPO);
        }
        else
        {
                (void)sprintf(str3, "%s%*s", fPO, (int)(total-strlen(fPO)), "");
        }

        char header[2*LEN];

	(void)moveCursor(1,1);
        (void)sprintf(header, "%s%s%s%s%s%s", COLOUR[0], INVERT, str1, str3, str2, INVERT_UNDO);
	(void)clearScreen();
        (void)printf("%s",header);

        (void)moveCursor(1,w.ws_row);
        char footer[2*LEN];
        char *str4 = "MANUAL : CTRL-H";//15
        (void)sprintf(footer, "%s%*s%s%s", INVERT, (w.ws_col-(int)strlen(str4)), "", str4, INVERT_UNDO);
        (void)printf("%s", footer);
        (void)fflush(stdout);
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *str : string/line to insert into
                char c : what will be inserted
                size_t pos : insert at

        Does: puts char into string moves rest of string to accomidate

        Returns: int
                0 : string allready full, no room
                1 : success
*/
int insertIntoStringNoNL(char *str, char c, size_t pos)
{
        if(strlen(str) == 0)//empty line
        {
		str[0] = c;
		str[1] = '\0';
                return 1;
        }
        if(strlen(str) == (LEN - 1))
        {
                (void)strcpy(message, "input line too long");
                return 0;
        }

        while(pos < strlen(str)+1)//shift chars over
	{
                char temp = str[pos];
                str[pos] = c;
                c = temp;
                pos++;
        }
        return 1;
}

/*
	Written by: Jonathan Pincus

	Parameters:
		char *str: user prompted with this
		char *strOut: users input
		int index: position of cursor
		int room: input space/ page size

	Does: prints special input bar

	Returns: N/A

*/
void printSpecialInput(char *str, char *strOut, int index, int room)
{
	int page = index / room;
	char *strStart = &strOut[(page * room)];
        int excess = room - (strlen(strStart));
        if(excess < 0)
	{
		excess = 0;
	}
	int maxPage = ((int)strlen(strOut) - 1)/ room;

        (void)removeline(2);
        (void)moveCursor(1,2);
        (void)printf("\t%s%s%s", COLOUR[1], INVERT, str);//prints prompt
	(void)printf("%s", page > 0 ? CHEVRON_LEFT_ON : CHEVRON_LEFT_OFF);
        (void)printf("%s%s%.*s%s%*s", COLOUR[1], INVERT, room, strStart, INVERT_UNDO, excess, "");//print page of input and spaces till end of line
        (void)printf("%s", page < maxPage ? CHEVRON_RIGHT_ON : CHEVRON_RIGHT_OFF);
        (void)moveCursor(index - (page * room) + 10 + strlen(str) , 2);
        (void)fflush(stdout);
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char *str : prints to screen
		struct winsize w : screen size
		char *strOut : stores input string for calling process
		char ***lines : loaded portion of file
		int *x : cursor position
		int *y : cursor position
		size_t *fileLine : unloaded lines above loaded portion of file
		struct stackItem **mlComment : tracks multi line comment (/oo/) in unloaded above portion of file
		struct expandString *e : hold prints so a single io call can be used
		int *pipe : used to communicate between input and display programs

        Does: prints to screen in banner style and gets string input if called to

        Returns:
		0 : input interupted / no input aseked for
		1 : input success
		-1 : error (can only error if input is asked for)
*/
int PrintError(char *str, struct winsize *w, char *strOut, char ***lines, int *x, int *y, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, int *pipe)
{
	int returner = 0;
	(void)removeline(2);//clear line to print on

	if(str != NULL)//print message
        {
                (void)moveCursor(1,2);
                (void)printf("\t%s%s%s", COLOUR[1], INVERT, str);//print what will fit on screen %*s
                (void)fflush(stdout);
        }
	if(strOut != NULL)//user input required
	{
		int room = w->ws_col-10-strlen(str);//room for input minus arrows and prompt space
		int index = 0;

		(void)printSpecialInput(str, strOut, index, room);

		(void)strcpy(strOut, "");
		do
		{
			int returner = -1;
			uint64_t data;
                	if(multi(pipe, &data) == STATUS_ERROR)//user input
                	{
                        	jError("multi");
                        	return STATUS_ERROR;
                	}

			if(data == LEFT)
			{
				if(index > 0)
				{
					index--;
				}
			}
			else if(data == RIGHT)
			{
				if(index < (int)strlen(strOut))
				{
					index++;
				}
			}
			else if(data >= ' ' && data <= '~')
			{
				if(insertIntoStringNoNL(strOut, (char)data, index) == 0)
				{
					(void)strcpy(message, "Input too long");
					returner = 0;
				}
				index++;
			}
			else if(data == CTRL_C)
			{
				returner = 0;
			}
			else if(data == 771)//code for change in window size
			{
				if(changeWindowSize(lines, w, y, fileLine, mLComment) == false)
                        	{
                                	jError("changeWindowSize");
                                	return STATUS_ERROR;
                        	}
				if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, 0) == STATUS_ERROR)
        			{
					jError("printMidde");
                			return STATUS_ERROR;
        			}
				room = w->ws_col-10-strlen(str);
			}
			else if(data == ENTER_KEY)
			{
				returner = 1;
			}
			else if(data == BACKSPACE_KEY)
			{
				if(index > 0)
				{
					for(size_t i = index - 1; i < strlen(strOut); i++)
					{
						strOut[i] = strOut[i+1];
					}
					index--;
				}
			}

			if(returner != -1)
			{
				(void)printf("%s",INVERT_UNDO);

				return returner;
			}


			(void)printSpecialInput(str, strOut, index, room);

		}while(1);//do until input finalised or cancel
	}

	(void)printf("%s",INVERT_UNDO);

	return returner;
}


/*
        Written by: Jonathan Pincus

        Parameters:
                struct timespec t1 : initial time
                struct timespec t2 : new time

        Does: calculates elapsed time

        Returns: long long : elapsed time in nano seconds
*/
long long TimeDiff(struct timespec t1, struct timespec t2)
{
        if(t1.tv_sec == t2.tv_sec)//same second
        {
                return (long long)(t2.tv_nsec-t1.tv_nsec);
        }
        else//spans multiple seconds
        {
                long long temp = t2.tv_sec - t1.tv_sec;
                temp = temp * 1000000000;
                temp = temp + (long long)t2.tv_nsec;
        	return temp - (long long)t1.tv_nsec;
        }
        return 0;//imposible to return
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *str : input string

        Does: checks for the closing of multi line comment(MLC) eg "o/"

        Returns: bool
		ture : contains unterminated MLC
		false : comments are terminated MLC or no MLC at all
*/
bool unload_MLC_End(char *str)
{
	bool slcC = false;//single line comment check
        bool strC = false;//string check
	bool mlcsC = false;//multi line comment start check
	bool mlceC = false;//multi line comment end check
        for(size_t i = 0; i<strlen(str); i++)
        {
                if(str[i] == '/' && str[i+1] == '/' && !strC)
                {
                        slcC = true;
			i++;
                }
                else if(str[i] == '"' && !strC && !slcC && !mlcsC)
                {
                        strC = true;
                }
                else if(str[i] == '"' && strC && !slcC && !mlcsC)
                {
                        strC = false;
                }
		else if(str[i] == '/' && str[i+1] == '*' && !strC)
		{
			mlcsC = true;
			mlceC = false;
			i++;
		}
		else if(str[i] == '*' && str[i+1] == '/' && !strC)
                {
			mlceC = true;
			mlcsC = false;
			i++;
                }
        }
        return mlceC;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char** lines : current segment of file
		size_t *fileLine : number of lines above current segment
		struct stackItem **mlComment : tracks multi line comments /oo/
		struct winsize w : screen size

        Does: shift portion of page up a single line if not at top of file

        Returns: int
		0 : top of file
		1 : scrolled up
		-1 : error
*/
int scrollUp(char** lines, size_t *fileLine, struct stackItem **mLComment, struct winsize w)
{
	char str[LEN] = "";

	int lastLineStatus = lastLine(str, fPU);//get new line

	if(lastLineStatus == STATUS_ERROR)
        {
                jError("lastLine");
                return STATUS_ERROR;
        }
	if(lastLineStatus == 0)//file empty
	{
		return 0;
	}

	if(append(lines[w.ws_row-EXCESS-1],fPD) == STATUS_ERROR)//unload into file
        {
                jError("append");
                return STATUS_ERROR;
        }

	(*fileLine)--;
	for(int i = w.ws_row-EXCESS-2; i>=0; i--)//shift make room
        {
                (void)strcpy(lines[i+1], lines[i]);
        }
	(void)strcpy(lines[0], str);

	if(!isEmptyS(*mLComment))//manage multi line comment tracker
	{
		void* vp3 = readS(*mLComment);
        	struct comment *ptr3 = (struct comment*)vp3;

		if(ptr3->b > *fileLine)
		{
			(void)addRestS(mLComment, 0);
		}
		if(ptr3->a > *fileLine)
		{
			struct frame *ptr2;
			ptr2=popS(mLComment);
                	(void)free(ptr2);
		}
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char *str : string to analyze

        Does: checks if unclosed multiline comment is in string (/o)

        Returns: bool
		true : unclosed mulyiline comment exists
		false : no unclosed multiline comment
*/
bool unload_MLC_Start(char *str)
{
        int index = -1;

        for(int i = strlen(str)-1; i>=1; i--)
        {
                if(str[i-1] == '*' &&  str[i] == '/')
                {
			//chance that this is found /0/
			//this counts as a start not an end
			if(i == 1)
			{
                        	return false;
			}
			if(str[i-2] != '/')
			{
				return false;
			}

                }
                if(str[i-1] == '/' &&  str[i] == '*')
                {
                        index = i-1;
                        break;
                }
        }
        if(index == -1)//no "/o" found
        {
                return false;
        }

	//if "/o" is found make sure not part of string
        bool strC = false;
        for(int i = 0; i<index; i++)
        {
                if(str[i] == '/' && str[i+1] == '/' && !strC)
                {
                        return true;
                }
                if(str[i] == '"' && !strC)
                {
                        strC = true;
                }
                else if(str[i] == '"' && strC)
                {
                        strC = false;
                }
        }
	if(strC)
	{
		return false;
	}
	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *oldStr : string that is being unloaded
		size_t *fileLine : number of lines above loaded portion of file
		struct stackItem **mLComment : track multi line comments "\oo\"

        Does: when unloading a multi line comment tracking stack is updated for highlighting

        Returns: bool
                true : success
                false : error
*/
bool MLC_Tracker(char *oldStr, size_t *fileLine, struct stackItem **mLComment)
{
	if(!isEmptyS(*mLComment))//not empty
        {
                if(halfS(*mLComment))//do we have open comment, this is oporunity to close it
                {
                        if(unload_MLC_End(oldStr))
                        {
                                (void)addRestS(mLComment, *fileLine);
                        }
                }
                else
                {//(n>1)th multi line comment
                        if(unload_MLC_Start(oldStr))
                        {
                                struct comment *ptr = malloc(sizeof(struct comment));
                                if(ptr == NULL)
                                {
                                        jError("malloc");
                                        return false;
                                }
                                (void)constructorS(&ptr, *fileLine);
                                if(addS(mLComment, ptr) == false)
                                {
                                        jError("addS");
                                        return false;
                                }
                        }
                }
        }
        else
        {//first multi line comment
                if(unload_MLC_Start(oldStr))
                {
                        struct comment *ptr = malloc(sizeof(struct comment));
                        if(ptr == NULL)
                        {
                                jError("malloc");
                                return false;
                        }
                        (void)constructorS(&ptr, *fileLine);
                        if(addS(mLComment, ptr) == false)
                        {
                                jError("addS");
                                return false;
                        }
                }
        }
	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char** lines : current segment of file
                size_t *fileLine : number of lines above current segment
                struct stackItem **mlComment : tracks multi line comments (/oo/)
                struct winsize w : screen size

        Does: shift portion of file down a single line if not at bottum of file

        Returns: int
                0 : bottum of file
                1 : scrolled down
                -1 : error
*/
int scrollDown(char** lines, size_t *fileLine, struct stackItem **mLComment, struct winsize w)
{

        char oldStr[LEN];
        char str[LEN];

        (void)strcpy(oldStr, lines[0]);

        int lastLineStatus = lastLine (str, fPD);//get new line

        if(lastLineStatus == 0)//no more lines
        {
		//if file is empty can still scroll down a single line under the last '\n'
		if(lines[w.ws_row-EXCESS-1][0] == '\0')
		{
			return 0;
		}
		(void)strcpy(str, "");
        }
        if(lastLineStatus == STATUS_ERROR)
        {
                jError("lastLine");
                return STATUS_ERROR;
        }

	if((*fileLine) != ULONG_MAX)//file too long
	{
		(*fileLine)++;
	}
	else
	{
		jError("file too long");
		return STATUS_ERROR;
	}

	if(append(oldStr,fPU) == STATUS_ERROR)//unload line
        {
                jError("append");
                return STATUS_ERROR;
        }
        for(int i = 1; i<w.ws_row-EXCESS; i++)//shuffle
        {
                (void)strcpy(lines[i-1], lines[i]);
        }
        (void)strcpy(lines[w.ws_row-EXCESS-1], str);

	if(!MLC_Tracker(oldStr, fileLine, mLComment))
	{
		jError("MLC_Tracker");
		return STATUS_ERROR;
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: deletes temporary storage files so program can closes

        Returns: N/A
*/
void deleteTempFiles()
{
	//not checking or returning errors because this is called at ent of program so there is no real recourse
        (void)remove(fPU);
        (void)remove(fPD);
        (void)remove(fPS);
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char ***lines : current segment of file
                struct winsize w : screen size
                int *x : cursor position
                int *y : cursor position
                size_t *fileLine : unloaded lines above loaded portion of file
		struct stackItem **mlComment : tracks multi line comment (/oo/) in unloaded above portion of file
                struct expandString *e : hold prints so a single io call can be used
		int *pipe : used to communicate between input and display programs

        Does: checks if user really to exit program and get new file name(global var)

        Returns: int
                0 : cancelled
                1 : exit
                -1 : error
		-2 : new file invalid
*/
int exitProgram(char ***lines, struct winsize *w, int *x, int *y, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, int *pipe)
{
	char str[LEN];
	str[0] = '\0';

	do
	{
		int printErrorStatus = PrintError("Do you want to save? (y/n):", w, str, lines, x, y, fileLine, mLComment, e, pipe);

		if(printErrorStatus == 0)
		{
			return 0;//cancelled
		}
		if(printErrorStatus == STATUS_ERROR)
		{
			jError("printError");
			return STATUS_ERROR;
		}

		if(strcmp("N", str) == 0 || strcmp("n", str) == 0)
                {
                        (void)strcpy(saveFileName, "");
                        return 1;
                }

		if(strcmp(str, "Y") == 0 || strcmp(str, "y") == 0)
		{
			break;
		}
		(void)strcpy(str , "");
	}while(1);

	do//get name to save uder
	{
		(void)strcpy(str, "");
		int printErrorStatus = printErrorStatus = PrintError("Press enter for orginal name or enter a new one:", w, str,  lines, x, y, fileLine, mLComment, e, pipe);
		if(printErrorStatus == STATUS_ERROR)
		{
			jError("printError");
			return STATUS_ERROR;
		}
		if(printErrorStatus == 0)
		{
			return 0;
		}

		if(str[0] == '\0')
		{
			(void)strcpy(saveFileName, "\n");//old name
			return 1;
		}
		(void)strcpy(saveFileName, str);


		//validate directory
		int validDirStatus = validDir(str);
		if(validDirStatus == STATUS_ERROR)
		{
			jError("validDirectory");
			return STATUS_ERROR;
		}
		else if (validDirStatus == 0)
		{
			(void)strcpy(message, "Invalid-FilePath");
                	return -2;
		}

		int accessStatus = access(str, F_OK);
		if(accessStatus == -1 && errno != 2)
		{//2 mean file does not exist which is fine
			jError("access");
			return STATUS_ERROR;
		}
		else if(accessStatus == 0)//file exists so confirm before rewrite
		{
			int sameFileStatus = sameFile(fPO, str);
			if(sameFileStatus == STATUS_ERROR)
			{
				jError("sameFile");
				return STATUS_ERROR;
			}
			if(sameFileStatus == 1)
			{//save to orinal file
                        	(void)strcpy(saveFileName, "\n");
                        	return 1;
			}
			do
			{
				(void)strcpy(str , "");
				printErrorStatus = PrintError("File name in use, do you want to save(y/n):", w, str, lines, x, y, fileLine, mLComment, e, pipe);
				if(printErrorStatus == 0)
                		{
                        		return 0;//cancelled
                		}
                		if(printErrorStatus == STATUS_ERROR)
                		{
                        		jError("printError");
                        		return STATUS_ERROR;
                		}
				if(strcmp("N", str) == 0 || strcmp("n", str) == 0)
                		{
                        		break;
                		}
				if(strcmp("Y", str) == 0 || strcmp("y", str) == 0)
                                {
                                        return 1;
                                }

			}while(1);

		}
		else
		{//file does not exist so fine
			//file name still may be wrong ie(too long)
			return 1;
		}
	}while(1);
	return STATUS_ERROR;//never gets here
}


/*
        Written by: Jonathan Pincus

        Parameters:
                char** lines : loaded portion of file
		struct winsize w : screen size
		char *destination : destination file path

        Does: creates final file by combining the file above and bellow and portion of file currently loaded

        Returns: int
		0 : success
		-1 : error
*/
int remakeFile(char** lines, struct winsize w, char *destination)
{
	FILE *fpO;

	fpO = fopen(destination, "a");
        if (fpO == NULL)
        {
        	jError("fopen");
        	return STATUS_ERROR;
        }

	if(duplicateFile(fPU, destination) == STATUS_ERROR)//fPU contains start of file
        {
                jError("dublicateFile");
                return STATUS_ERROR;
        }

	for(int i=0; i<(w.ws_row-EXCESS); i++)//middle of file (loaded part)
        {
                if(fprintf(fpO,"%s",lines[i]) < 0)
                {
                        jError("fprintf");
			return STATUS_ERROR;
                }
        }

	if(fclose(fpO) != 0)
        {
                jError("fclose");
                return STATUS_ERROR;
        }

	if(flipFile(fPD, destination) == STATUS_ERROR)//flip unloaded lower part of file into final file
	{
		jError("flipFile");
		return STATUS_ERROR;
	}

	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int pipe[2] : used to send user input to other process

        Does: inputing process

        Returns: int
		1 : success(never reaches this)
		-1 : error
*/
int inputs(int pipe[2])
{
        while(1)
        {
		//tell other program what the user did and when
                char c;
		c = getchar();

		struct timespec t;
       	       	if(clock_gettime(CLOCK_MONOTONIC_RAW, &t) == STATUS_ERROR)
		{
			jError("clock_gettime");
			exit(STATUS_ERROR);//main knows cause pipe no longer readable
		}
		if(write(pipe[1], &c, sizeof(char)) == STATUS_ERROR)
    		{
			jError("write");
			exit(STATUS_ERROR);
		}
		if(write(pipe[1], &t, sizeof(struct timespec)) == STATUS_ERROR)
		{
			jError("write");
			exit(STATUS_ERROR);
		}
        }
	return 1;//never reaches here
}


/*
        Written by: Jonathan Pincus

        Parameters:
                int *x : cursor position
		int *y : cursor position
		char **lines : current portion of file
		size_t *fileLine : number of lines above current portion of file
		struct stackItem **mLComment : tracks (/oo/) multi line comment
		struct winsize w : screen size

        Does: handles when user enters

        Returns: int
		0 : success
		1 : success (scrolled so reprint)
		-1 : error
*/
int Enter(int *x, int *y, char **lines, size_t *fileLine, struct stackItem **mLComment, struct winsize w)
{
	if(strcmp(lines[*y], "") == 0)//blank line/ last line of file
	{
		lines[(*y)][0] = '\n';
                lines[(*y)][1] = '\0';
		if( (*y) == (w.ws_row-EXCESS-1))
        	{
			if(scrollDown(lines, fileLine, mLComment, w) == STATUS_ERROR)
                	{
                        	jError("scrollDown");
                        	return STATUS_ERROR;
                	}
			return 1;
		}
		(*y)++;
		return 0;
	}

	int returner = 0;//tracks if function had to scroll

	if( (*y) == (w.ws_row-EXCESS-1))//enter at bottum line make more space for new (already checked if last line)
	{
                int scrollDownStatus = scrollDown(lines, fileLine, mLComment, w);
        	if(scrollDownStatus == STATUS_ERROR)
		{
			jError("scrollDown");
			return STATUS_ERROR;
		}
		returner = 1;
		(*y)--;
        }

	//move bellow y down
	char temp[LEN];
	(void)strcpy(temp, lines[w.ws_row-EXCESS-1]);
	for(int i=w.ws_row-EXCESS-2; i>(*y); i--)
	{
		(void)strcpy(lines[i+1], lines[i]);
	}
	if(append(temp,fPD) == STATUS_ERROR)
	{
		jError("append");
		return STATUS_ERROR;
	}

	//after x move to the line under
	(void)strcpy(temp, "");
	char *ptr = &lines[*y][*x];
	(void)strcpy(temp, ptr);
	(void)strcpy(lines[(*y) + 1], temp);

	//put newline after x
	lines[(*y)][(*x)]='\n';
	lines[(*y)][(*x)+1]='\0';

	///move x,y
	(*y)++;
	(*x)=0;

	return returner;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int *x : cursor position
                int *y : cursor position
                char **lines : current portion of file
                struct winsize w : screen size

        Does: handles when user backspaces at x 0 so lines converge

        Returns: int
                0 : success
                1 : success
                -1 : error
		-2 : new line created is too long
*/
int backspaceChangeLine(int *x, int *y, char **lines, struct winsize w)
{
	if(lines[(*y)][0] == '\0')//on absolute last line of file
	{
		//move up to previous line
		(*y)--;
		(*x)= (strlen(lines[(*y)]) -1);
		return 1;
	}

	if((strlen(lines[(*y)]) + strlen(lines[((*y) - 1)]) -1) >= LEN)//2 "\n" so minus 1
	{
		(void)strcpy(message, "Backspace results in too large a line");
		return -2;
	}

	int newX = strlen(lines[(*y)-1]) -1;

	//append lines
	lines[(*y)-1][strlen(lines[(*y)-1]) - 1] = '\0';
	(void)strcat(lines[(*y)-1], lines[(*y)]);

	//move all up
	for(int i = ((*y)); i<w.ws_row-EXCESS-1; i++)
	{
		(void)strcpy(lines[i], lines[i+1]);
	}

	//new positions
	(*x) = newX;
	(*y)--;

	//get new line from file to fill space
	char str[LEN];
	int lastLineStatus = lastLine(str, fPD);
        if(lastLineStatus == 0)//no more line
        {
                lines[w.ws_row-EXCESS-1][0] = '\0';
		return 0;
        }
        if(lastLineStatus == STATUS_ERROR)
        {
                jError("lastLine");
                return STATUS_ERROR;
        }
	(void)strcpy(lines[w.ws_row-EXCESS-1], str);

	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int *x : cursor position
                int *y : cursor position
                char **lines : current portion of file
                size_t *fileLine : number of lines above current portion of file
                struct stackItem **mLComment : tracks (/oo/) multi line comment
                struct winsize w : screen size

        Does: handles when user backspaces

        Returns: int
                0 : success
                1 : success
                -1 : error
		-2 : new line created is too long(backspaceChangeLine)
*/
int backspace(int *x, int *y, char **lines, size_t *fileLine, struct stackItem **mLComment, struct winsize w)
{
	if((*x) == 0 && (*y) == 0)//if at top of screen backspace will move beyond screen so scroll
	{
		int scrollUpStatus = scrollUp(lines, fileLine, mLComment, w);
		if(scrollUpStatus == STATUS_ERROR)
		{
			return STATUS_ERROR;
		}
		else if(scrollUpStatus == 0)//fist line of file
		{
			return 0;
		}

		(*y)++;
	}

	if( (*x) == 0 )//backspace cuases lines to merge
	{
		return backspaceChangeLine(x, y, lines, w);
	}
	else//just remove char
	{
		(*x)--;
		for(size_t i = (*x); i < strlen(lines[ (*y) ]); i++)
		{
			lines[ (*y) ][i] = lines[ (*y) ][i+1];
		}
	}
	return 1;
}


/*
        Written by: Jonathan Pincus

        Parameters:
                int *x : cursor position
                int *y : cursor position
                char **lines : current portion of file
                struct winsize w : screen size

        Does: handles when user presses delete key

        Returns: int
                0 : success (no scrolling)
                1 : success (with scrolling)
                -1 : error
                -2 : new line created is too long
*/
int deleteFunc(int *x, int *y, char **lines, struct winsize w)
{
	//middle of line
	if((*x) < (int)strlen(lines[(*y)])-1)
	{
		for(size_t i = (*x)+1; i<=strlen(lines[*y]); i++)
		{
			lines[*y][i-1]=lines[*y][i];
		}
		return 0;
	}

	if((*y)+1 == w.ws_row-EXCESS)//end of page so need new line
	{
		char str[LEN];
        	int lastLineStatus = lastLine(str, fPD);
        	if(lastLineStatus == 0)//no more lines
        	{
               		return 1;
       		}
       		if(lastLineStatus == STATUS_ERROR)
       		{
               		jError("lastLine");
               		return STATUS_ERROR;
       		}

		if(strlen(lines[*y]) + strlen(str) >= LEN)//make sure there is space in LEN
		{
			strcpy(message, "delete results in line too large for buffer");
			//add line back to file
			if(append(str,fPD) == STATUS_ERROR)
                        {
                        	jError("append");
                                return STATUS_ERROR;
                        }
			return -2;
		}
		lines[*y][strlen(lines[*y])-1]='\0';
		(void)strcat(lines[*y], str);

		return 1;
	}

	//merge current line with next line
	if(strlen(lines[*y]) + strlen(lines[(*y) +1]) >= LEN)//make sure there is space in LEN
        {
        	(void)strcpy(message, "delete results in line too large for buffer");
                return -2;
        }
	if(lines[(*y)+1][0] == '\0')
	{
		return 0;
	}
	lines[*y][strlen(lines[*y])-1]='\0';
        (void)strcat(lines[*y], lines[(*y)+1]);

	//shuffle lines to fill void
	for(int i = (*y)+2; i<w.ws_row-EXCESS; i++)
	{
		(void)strcpy(lines[i-1], lines[i]);
	}

	char str[LEN];
       	int lastLineStatus = lastLine(str, fPD);//get last line for space at bottom
        if(lastLineStatus == 0)//no more line
        {
              	lines[w.ws_row-EXCESS-1][0] = '\0';
               	return 1;
        }
        if(lastLineStatus == STATUS_ERROR)
        {
                jError("lastLine");
                return STATUS_ERROR;
        }
        (void)strcpy(lines[w.ws_row-EXCESS-1], str);
	return 1;
}


/*
        Written by: Jonathan Pincus

        Parameters:
		int startX : original x value
		size_t startY : original y value
		int x : end value of x
		int y : end value of y
		int nowx : value of x we are testing
		size_t nowY : value of y we are testing
		size_t fileLine : depth of current top line in file

        Does: checks if nowx and nowy are in the range set by x,y and startX,startY (for highlighting during copy)

        Returns: bool
		true : is in range
		false : is not in range
*/
bool highlightRangeCheck(int startX, size_t startY, int x, int y, int nowx, size_t nowy, size_t fileLine)
{
	nowx++;
	size_t yST = y + fileLine;//y was relative to page now relative to file
	nowy += fileLine;

	if(startY == yST && startX == x)//no range so imposible to be within range
	{
		return false;
	}
	if(nowy>yST || nowy<startY)
	{
		return false;
	}

	if(startY == yST)//single line range
	{
		if(nowx > startX && nowx <= x)
		{
			return true;
		}
	}
	else if(nowy>startY && nowy<yST)//in middle of lines
	{
		return true;
	}
	else if(nowy == startY)//top line
	{
		if(nowx > startX)
		{
			return true;
		}
	}
	else//bottum line
	{
		if(nowx <= x)
		{
			return true;
		}
	}
	return false;
}


/*
	Written By: Jonathan Pincus

	Parameter:
		char *line : line of file to be formated
		int x : cursor position
		int y : ''
		int i : index of line in loaded part of file
		int startX : cursor position when copy started
		size_t startY : ''
		struct winsize *w : window size
		size_t *fileline : number of lines above loaded portion of file that are unloaded
		struct expandString *e : holds all print for singal IO call

	Does: process a single line of loaded file into printing format

	Returns: int
		0 : success
		-1 : error
*/
int getCopyInterfaceLine(char *line, int x, int y, int i, int startX, size_t startY, struct winsize *w, size_t *fileLine, struct expandString *e)
{
	int xend = x;//save end point of highlight
        size_t yend = y;
        int end = w->ws_col;
        int tempX = x;
        int start = 0;

	if(y==i)//line where curesor is is only line that might not print start cuase cursor would be off screen
        {
        	start += currentLineStart(line, &tempX, *w);
                end = start + w->ws_col;
       	}
        int jProper =0;//track index of print, would be equivilent to index if no tabs

	if(line[0] == '\0')
	{
		if(appendES(e, "-=NULL=-\n") == STATUS_ERROR)
                {
                	jError("appendES");
                       	return STATUS_ERROR;
                }
		return 0;
	}

	for(int j = 0; j<(int)strlen(line); j++, jProper++)
        {
                if(j>=start && j < end)//on current page (some of line is trucated if too long or current position later in line)
                {
                	if(line[j] == '\t')//tab takes to nearest x%8 = 0
                        {
                        	int plus = nearestEightDiff(jProper) - 1;// the excess a tab prints over a signle char
                                jProper += plus;
                                end -= plus;
                        }


                        if(highlightRangeCheck(startX, startY, xend, yend, j, i, *fileLine))//print as highlighted
			{
                        	if(appendES(e, INVERT) == STATUS_ERROR)
                                {
                                	jError("appendES");
                                        return STATUS_ERROR;
                                }
                                if(appendES(e, (char[2]){line[j], '\0'}) == STATUS_ERROR)
                                {
                                	jError("appendES");
                                        return STATUS_ERROR;
                                }
                                if(appendES(e, INVERT_UNDO) == STATUS_ERROR)
                                {
                                	jError("appendES");
                                        return STATUS_ERROR;
                               	}
   			}
                        else//print as normal
                       	{
                        	if(appendES(e, ((char[2]){line[j],'\0'})) == STATUS_ERROR)
                                {
                                	jError("appendES");
                                       	return STATUS_ERROR;
                                }
                        }
                }
  	}
        if((int)strlen(line) > end)//line too big
        {
        	if(appendES(e, "\n") == STATUS_ERROR)
                {
                	jError("appendES");
                        return STATUS_ERROR;
                }
        }
	return 0;
}


/*
        Written by: Jonathan Pincus

        Parameters:
		char ***lines : portion of file
		struct winsize *w : size of screen
		int startX : start of buffer
		size_t startY : start of buffer
		int *x : end of buffer
		int *y : end of buffer
		size_t fileLine : position of page in file (number of lines above char[0])
                struct expandString *e : hold prints so a single io call can be used
		int *pipe : used to communicate between input and display programs
		int start : print loaded portion of file starting at this index
		int end : print loaded portion of file ending at this index


        Does: prints screen while user is copying data highlighting data that will be added to the buffer

        Returns: int
		0 : success
		-1 : fail
*/
int printCopyInterface(char ***lines, struct winsize *w, int startX, size_t startY, int *x, int *y, size_t *fileLine, struct expandString *e, int start, int end)
{
	if(ULONG_MAX - (w->ws_row-EXCESS) < (*fileLine))
	{
		jError("file too long");
		return STATUS_ERROR;
	}

	(void)clearES(e);

        for(int i = start; i < end; i++)
	{
        	if(getCopyInterfaceLine((*lines)[i], *x, *y, i, startX, startY, w, fileLine, e) == STATUS_ERROR)
		{
			jError("getCopyInterfaceLine");
			return STATUS_ERROR;
		}
	}


	(void)clearScreenMiddle(start, end);
	(void)printFL(*fileLine, *w);
	(void)moveCursor(1,3 + start);
	(void)printf("%s", e->str);
	(void)fflush( stdout );
	int tempx1 = *x;
        (void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
        (void)moveCursor(tempx1+1, *y+3);
	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int startX : start of highlight cursor position
                size_t startY : start of highlight cursor position
                int endX : end of highlight cursor position
		size_t endY : end of highlight cursor position
		char **lines : current portion of file
		struct winsize w : screen size
                size_t *fileLine : number of lines above current portion of file
                struct stackItem **mLComment : tracks (/oo/) multi line comments in above portion of file

        Does: saves highlighted/copied section of file for pasteing

        Returns: int
                0 : success
                1 : success
                -1 : error
*/
int saveCopyBuffer(int startX, size_t startY, int endX, size_t endY, char **lines, struct winsize w, size_t *fileLine, struct stackItem **mLComment)
{
	if(startY == endY + *fileLine && endX == startX)//no actual area highlighted
        {
        	return 0;
        }

        (void)clearES(&pasteBuffer);

        if(startY == endY + *fileLine)//single line
        {
        	for(int i = startX; i<endX; i++ )
                {
                	if(appendES(&pasteBuffer, (char[2]){lines[endY][i],'\0'}) == STATUS_ERROR)
                        {
                        	jError("appendES");
                                return STATUS_ERROR;
                        }
                }
                return 1;
	}

        //multi line
        if(startY >= *fileLine)//start alread loaded
        {//comment fully loaded cuase end is always on screen
        	int current = startY - *fileLine;
        	for(int i=startX; i < (int)strlen(lines[current]); i++)//first line
        	{
       			if(appendES(&pasteBuffer, (char[2]){lines[current][i], '\0'}) == STATUS_ERROR)
        	       	{
        	       		jError("appendES");
        	                return STATUS_ERROR;
        	        }
        	}
        	current++;

        	while((size_t)current < endY)//middle line/lines
       		{
        		if(appendES(&pasteBuffer, lines[current]) == STATUS_ERROR)
                	{
               			jError("appendES");
                        	return STATUS_ERROR;
                	}
                	current++;
		}

        	for(int i=0; i<endX; i++)//last line
        	{
        		if(appendES(&pasteBuffer, (char[2]){lines[current][i], '\0'}) == STATUS_ERROR)
                	{
                		jError("appendES");
                        	return STATUS_ERROR;
                	}
        	}
       		return 1;
	}
        else
       	{//start of copy is not loaded in file
        	size_t temp = endY + *fileLine;
                int count = 0;
                while(*fileLine > startY )//move up to load start of comment
                {
                	if(scrollUp(lines, fileLine, mLComment, w) == STATUS_ERROR)
                        {
                        	jError("scrollUp");
                                return STATUS_ERROR;
                        }
                        count++;
                }

                for(int i=startX; i < (int)strlen(lines[0]); i++)//first line of comment
               	{
                	if(appendES(&pasteBuffer, (char[2]){lines[0][i], '\0'}) == STATUS_ERROR)
                        {
                        	jError("appendES");
                                return STATUS_ERROR;
                        }
                }

                while(count > 0)//scroll back to orinal position
                {
                	count--;
                        if(scrollDown(lines, fileLine, mLComment, w) == STATUS_ERROR)
                        {
                        	jError("scrollDown");
                                return STATUS_ERROR;
                        }
                        if(*fileLine == temp)//end of highlight was on first line of screen
                        {
                        	for(int i=0; i< endX; i++)
                                {
                                	if(appendES(&pasteBuffer, (char[2]){lines[0][i], '\0'}) == STATUS_ERROR)
                                        {
                                        	jError("appendES");
                                                return STATUS_ERROR;
                                        }
				}
                                return 1;
                        }
                        if(appendES(&pasteBuffer, lines[0]) == STATUS_ERROR)
                        {
                        	jError("appendES");
                                return STATUS_ERROR;
                        }
		}

               	for(size_t i=1; i< endY; i++)//copy from initially loaded and final portion
                {
                	if(appendES(&pasteBuffer, lines[i]) == STATUS_ERROR)
                        {
                        	jError("appendES");
                                return STATUS_ERROR;
                        }
                }

                for(int i=0; i< endX; i++)//last line
                {
                	if(appendES(&pasteBuffer, (char[2]){lines[endY][i], '\0'}) == STATUS_ERROR)
                       	{
                                jError("appendES");
                        	return STATUS_ERROR;
                        }
                }
                return 1;
	}
}

/*
        Written by: Jonathan Pincus

        Parameters:
		int *x : cursor position on screen
		int *y : cursor position on screen
		char ***lines : loaded portion of file
		struct winsize *w : screen size
		size_t *fileLine : depth of currnet segment of file
		struct stackItem **mLComment : tracks Multi Line Comments
		struct expandString *e : hold things to be printed so can use a single io call
		int *pipe : used to get input from inputing process

        Does: handles user inputs while user is copying protion of file

        Returns: int
*/
int copyRoutine(int *x, int *y, char ***lines, struct winsize *w, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, int *pipe)
{
	int startX = *x;
	size_t startY = *y + *fileLine;//highlight start position

	(void)strcpy(message, "COPY INTERFACE Press CTRL-D");
        (void)PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe);//cast as void because only print

	if(printCopyInterface(lines, w, startX, startY, x, y, fileLine, e, 0, (w->ws_row-EXCESS)) == STATUS_ERROR)
	{
		jError("printCopyInterface");
		return STATUS_ERROR;
	}
	while(1)
	{
		bool scroll = false;
		if(ULONG_MAX - (*y) < (*fileLine))
		{
			jError("file too long");
			return STATUS_ERROR;
		}


		uint64_t data;
		if(multi(pipe, &data) == STATUS_ERROR)//user input
		{
			jError("multi");
			return STATUS_ERROR;
		}

		if(data >= UP && data <= LEFT)//arrows
		{
			int ArrowsStatus = Arrows(data, x, y, *lines, *w, fileLine, mLComment);
			if(ArrowsStatus == STATUS_ERROR)
			{
				jError("Arrows");
				return STATUS_ERROR;
			}
			else if(ArrowsStatus == 0)
			{
				scroll = true;
			}

			if(*y+*fileLine < startY ||( startY==*y+*fileLine && *x<startX))//out of range
			{
				(void)strcpy(message, "Copy canceled(out of range)");
				return 0;
			}
		}
		else if(data == CTRL_D)//stops copy interface and saves for pasting
		{
			if(saveCopyBuffer(startX, startY, *x, *y, *lines, *w, fileLine, mLComment) == STATUS_ERROR)
			{
				jError("saveCopyBuffer");
				return STATUS_ERROR;
			}
			(void)strcpy(message, "");
			return 1;
		}
		else if(data == CTRL_C)
		{
			(void)strcpy(message, "");
			return 0;
		}
		else if(data == 771)//window size change
		{
			if(changeWindowSize(lines, w, y, fileLine, mLComment) == false)
                        {
                        	jError("changeWindowSize");
                                return STATUS_ERROR;
                        }
			scroll = true;//this fources full reprint
		}

		//print minimal
		if(scroll)
		{
			if(printCopyInterface(lines, w, startX, startY, x, y, fileLine, e, 0, (w->ws_row-EXCESS)) == STATUS_ERROR)
			{
				jError("printCopyInterface");
				return STATUS_ERROR;
			}
		}
		else//else only one above and bellow could be effected (ternary does this)
		{
			if(printCopyInterface(lines, w, startX, startY, x, y, fileLine, e,
				((*y) > 0 ? (*y)-1 : (*y)),
				((*y) < (w->ws_row-EXCESS-1) ? (*y)+2 : (*y)+1)) == STATUS_ERROR)
                	{
                        	jError("printCopyInterface");
                        	return STATUS_ERROR;
                	}
		}
	}
	return STATUS_ERROR;//should never reach here
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char **lines : loaded portion of file
                int *y : position of cursor

        Does: if file smaller than screen that y maybe in inacessable NULL portion of file, this makes sure y is not

        Returns: N/A
*/
void legitimizeY(char **lines, int *y)
{
	if(*y == 0)
	{
		return;
	}

	while(strcmp(lines[*y], "") == 0 && strcmp(lines[(*y) -1], "") == 0)
	{
		(*y)--;

		if(*y == 0)
        	{
                	return;
        	}
	}
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int *x : cursor position
		int *Y : cursor position
		char ***lines : loaded portion of file
		struct winsize *w : screen size
		size_t *fileLine : lines above loaded portion of file
		struct stackItem **mlComment : tracks (/oo/) multi line comments in above unloaded protion of file
		struct expandString *e : hold print for single io call
		int *pipe : used to communicate between input and display programs

        Does: moves to line of file

        Returns: int
		0 : user input was cancel or illigitimate
		1 : success
		-1 : error
*/
int moveTo(int *x, int *y, char ***lines, struct winsize *w,
size_t *fileLine, struct stackItem **mLComment, struct expandString *e, int *pipe)
{
	char str[LEN];
	str[0] = '\0';//user input
	int printErrorStatus = PrintError("Line:", w, str, lines, x, y, fileLine, mLComment, e, pipe);

	if(printErrorStatus == STATUS_ERROR)
	{
		jError("printError");
		return STATUS_ERROR;
	}
	if(printErrorStatus == 0 || strcmp(str, "") == 0)//canceled
	{
                return 0;
        }
	for(size_t i = 0; i < strlen(str); i++)//makeing sure it is only numbers
	{
		if(str[i] < 48 || str[i] > 57)
		{
			(void)strcpy(message, "Invalid Input");
			return 0;
		}
	}


	size_t num;
	(void)sscanf(str, "%zu", &num);
	if(num == 0)//lines are 1 indexed
	{
		num = 1;
	}

	*x=0;//all moves reset x

	if(ULONG_MAX - w->ws_row < *fileLine)
	{
		jError("file too long");
                return STATUS_ERROR;
	}

	if(num > *fileLine && num <= *fileLine + w->ws_row - EXCESS)//current page
        {
                *y = (num -1) - *fileLine;
                (void)legitimizeY(*lines, y);
                return 1;
        }

	if((num - 1) < *fileLine)//above
	{
		while(1)
		{
			if(scrollUp(*lines, fileLine, mLComment, *w) == STATUS_ERROR)
			{
				jError("scrollUp");
				return STATUS_ERROR;
			}
			if((num - 1) == *fileLine)
			{
				*y=0;
				return 1;
			}
		}
	}

	//bellow
	while(1)
	{
		int scrollDownStatus = scrollDown(*lines, fileLine, mLComment, *w);
		if(scrollDownStatus == STATUS_ERROR)
		{
			jError("scollDown");
			return STATUS_ERROR;
		}
		if(scrollDownStatus == 0)
		{
			//bottum of file
			*y = w->ws_row - EXCESS - 1;
			(void)legitimizeY(*lines, y);
			return 1;
		}

		if(num == (*fileLine + (w->ws_row-EXCESS)))
		{
			*y = w->ws_row - EXCESS - 1;
			return 1;
		}
	}

	return STATUS_ERROR;//never gets here
}

/*
	Written by: Jonathan Pincus

	Parameters:
		int *x : cursor position
		int *y : cursor position
		char ***lines : loaded portion of file
		struct winsize *w : screen size
		size_t *fileLine : lines above loaded portion of file
                struct stackItem **mlComment : tracks (/oo/) multi line comments in above unloaded protion of file
		struct expandString *e : hold prints for single io call
		int *pipe : used to communicate between input and display programs

	Does: users enters target and target is moved to if on or bellow screen

	Returns: int
		0 : user input was cancel or illigitimate
		1 : success
		-1 : error
*/
int find(int *x, int *y, char ***lines, struct winsize *w, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, int *pipe)
{
	char str[LEN];
	str[0] = '\0';
	int printErrorStatus = PrintError("Search for:", w, str, lines, x, y, fileLine, mLComment, e, pipe);

	if(printErrorStatus == STATUS_ERROR)
	{
		jError("printError");
		return STATUS_ERROR;
	}
	if(printErrorStatus == 0 || strcmp(str, "") == 0)
	{//cancel/ illigitimate input
		return 0;
	}

	//got input
        size_t fileLineSave = (*fileLine);
        int oldX = (*x);//current position in file
        int oldY = (*y);

        //for lines under y
        (*x)++;
        for(int i = *y; i < (w->ws_row-EXCESS); i++)
        {
                for(size_t j = *x; j < strlen((*lines)[i]); j++)
               	{
                        if(searchSubString((*lines)[i], j, str))
                        {
                        	*x=j;
                                *y=i;
                                return 1;
                        }
                }
                *x = 0;//for second line on start at 0
	}

	//not on current page so check rest of file bellow
        do
       	{
              	int scrollDownStatus  = scrollDown((*lines), fileLine, mLComment, *w);

		if(scrollDownStatus == STATUS_ERROR)
		{
			jError("scrollDown");
			return STATUS_ERROR;
		}

		if(scrollDownStatus == 0)//end of file
		{
			break;
		}

                for(size_t j = 0; j < strlen((*lines)[(w->ws_row-EXCESS)-1]); j++)
                {
                	if(searchSubString((*lines)[(w->ws_row-EXCESS)-1], j, str))
                        {
                        	*x=j;
                                *y=(w->ws_row-EXCESS)-1;
                              	return 1;
                       	}
        	}
	}while(1);

        //else no found so move back to original position
        while(*fileLine != fileLineSave)
        {
              	if(scrollUp((*lines), fileLine, mLComment, *w) == STATUS_ERROR)
		{
			jError("scrollUp");
			return STATUS_ERROR;
		}
        }

        *x = oldX;
        *y =oldY;
	(void)strcpy(message, "Could not find Target in remainingfile");
	return 1;
}

/*
	Written: Jonathan Pincus

	Parameters:
		int *x : cursor position
		int *y : cursor position
		char **lines : loaded portion of file
		struct winsize w : screen size
		size_t *fileLine : number of lines above loaded portion of file
		struct stakcItem **mLComment : tracks multi line comments (/oo/) above loaded protion of file

	Does: pastes paste buffer at cursor, moves cursor to end of paste data

	Returns: int
		0 : no paste data
		1 : success
		-1 : error
		-2 : paste results in line that is too large so aborted (some cahnges are made)
		-3 : paste results in line that is too large so aborted (no cahnges are made)

*/
int pasteFunc(int *x, int *y, char **lines, struct winsize w, size_t *fileLine, struct stackItem **mLComment)
{
	if(pasteBuffer.str[0] == '\0')//nothing to paste
        {
		(void)strcpy(message, "No Buffer for pasting");
        	return 0;
	}

	char endStr[LEN] = "";//past x will be moved down or to the right

	char curStr[LEN] = "";
	bool firstNewLineDone = false;
	for(size_t i=0; i<strlen(pasteBuffer.str); i++)//pasteBuffer has len built in
	{
		(void)strcat(curStr, (char[2]){pasteBuffer.str[i], '\0'});//holds current portion of paste buffer

		if(pasteBuffer.str[i] == '\n')
		{
			if(!firstNewLineDone)//first is special becase only line where cursor may be in middle of line
			{
				firstNewLineDone=true;

				if((*x) + strlen(curStr) >= LEN)
                                {
                                        (void)strcpy(message, "Line too full to paste");
                                        return -3;
                                }
				char endStr[LEN] = "";//past x will be moves down or to the right
                        	char *ptr = &lines[*y][*x];
                        	(void)strcpy(endStr, ptr);
	                	lines[*y][*x] = '\0';

                                if(append(lines[w.ws_row-EXCESS-1],fPD) == STATUS_ERROR)//make room
                                {
                                        jError("append");
                                        return STATUS_ERROR;
                                }
				for(int i = w.ws_row-EXCESS-2; i>*y; i--)//move down
                                {
                                        (void)strcpy(lines[i+1], lines[i]);
                                }

                                (void)strcat(lines[*y], curStr);
				//may be at bottom of file so scroll
				if(*y == w.ws_row-EXCESS-1)
				{
					int scrollDownStatus = scrollDown(lines, fileLine, mLComment,w);
                                	if(scrollDownStatus == STATUS_ERROR)
                                	{
                                	        jError("scrollDown");
                                        	return STATUS_ERROR;
                                	}
                                	if(scrollDownStatus != 0)
                                	{
                                	        (*y)--;
                                	}
				}

				(void)strcpy(lines[(*y)+1], endStr);
			}
			else
			{
				if(*y == w.ws_row-EXCESS-1)//always move for more space
                        	{
                                	int scrollDownStatus = scrollDown(lines, fileLine, mLComment,w);
                                	if(scrollDownStatus == STATUS_ERROR)
                                	{
                                        	jError("scrollDown");
                                        	return STATUS_ERROR;
                                	}
                                	if(scrollDownStatus != 0)
                                	{
                                        	(*y)--;
                                	}
				}


                          	if(append(lines[w.ws_row-EXCESS-1],fPD) == STATUS_ERROR)//move into file to make room
                                {
                                        jError("append");
                                        return STATUS_ERROR;
                                }
			      	for(int j = w.ws_row-EXCESS-2; j>=*y; j--)
                                {
                                	(void)strcpy(lines[j+1], lines[j]);
                                }
				(void)strcpy(lines[*y], curStr);
			}

			if(*y == w.ws_row-EXCESS-1)//at bottom
			{
				int scrollDownStatus = scrollDown(lines, fileLine, mLComment,w);
                                if(scrollDownStatus == STATUS_ERROR)
                                {
                                        jError("scrollDown");
                                        return STATUS_ERROR;
                                }
			}
			else
			{
				(*y)++;//move down and reset buffer for next loop
			}
			(*x)=0;
			(void)strcpy(curStr, "");
		}
	}

	if(pasteBuffer.str[strlen(pasteBuffer.str) - 1] == '\n')//buffer is '\n' terminated so no remaining chars
	{
		return 1;
	}

	if(!firstNewLineDone)
	{//no '\n' at all
		if(strcmp(lines[*y], "") == 0)//last line of file
		{
			//paste buffer max unterminated (\n) sequence is 998 chars so garanteed room
			//add buffer and terminate it
			(void)strcpy(lines[*y], curStr);
			int index = strlen(lines[*y]);
                        lines[*y][index] = '\n';
                        lines[*y][index+1] = '\0';
			return 1;
		}



		if(strlen(curStr) + strlen(lines[*y]) > LEN)
		{
			(void)strcpy(message, "Line too full to paste");
                	return -3;
		}

		//portion of line after cursor
		char endStr[LEN];
        	char *ptr = &lines[*y][*x];
        	(void)strcpy(endStr, ptr);

		lines[*y][*x] = '\0';
		(void)strcat(lines[*y], curStr);
        	(void)strcat(lines[*y], endStr);
        	(*x)+= strlen(curStr);

		return 1;
	}

	//had '/n' but aslo has bit at end without it
	if(strlen(lines[*y]) + strlen(curStr) >= LEN)
	{
		(void)strcpy(message, "Line too full to paste");
		return -2;
	}

	char *ptr = &lines[*y][*x];
	(void)strcpy(endStr, ptr);
	lines[*y][*x] = '\0';
	(void)strcat(lines[*y], curStr);
	(void)strcat(lines[*y], endStr);
	(*x)+= strlen(curStr);

	//if pasted at last line of file it will need a \n added
        if(lines[*y][ strlen(lines[*y]) -1 ] != '\n')
        {
        	if(strlen(lines[*y]) == LEN - 1)
                {
                	lines[*y][LEN-2] = '\n';
                        return -2;
                }

                int index = strlen(lines[*y]);
                lines[*y][index] = '\n';
        	lines[*y][index+1] = '\0';
        }
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		uint64_t input : user input, one of the arrows
                int *x : cursor position
                int *y : cursor position
                char **lines : loaded portion of file
                struct winsize w : screen size
                size_t *fileLine : lines above loaded portion of file
                struct stackItem **mlComment : tracks (/oo/) multi line comments in above unloaded protion of file

        Does: moves cursor on user input of arrow, scrolls screen if need

        Returns: int
		0 : success + scroll
		1 : success no scroll
		-1 : error
*/
int Arrows(uint64_t input, int *x, int *y, char **lines, struct winsize w, size_t *fileLine, struct stackItem **mLComment)
{
	bool scroll = false;
	bool end = false;

	switch (input)
        {
        	case UP:
                        (*y)--;
                break;
                case DOWN:
                        (*y)++;
                break;
                case RIGHT:
                        (*x)++;

	                if( (*x) > (int)(strlen(lines[(*y)]) - 1 ) )
                        {
                                (*y)++;
                                (*x)=0;
                        }
                break;
                case LEFT:
                        (*x)--;
			end = false;

                        if((*x)<0)
                        {
                                (*y)--;
				end = true;
                        }
                break;
	}

	if((*y)<0)//moved too far up
        {
		int scrollUpStatus = scrollUp(lines, fileLine, mLComment, w);
		if(scrollUpStatus == STATUS_ERROR)
                {
                        jError("scrollUp");
                        return STATUS_ERROR;
                }
		if(scrollUpStatus == 1)
		{
			scroll = true;
		}
		(*y) = 0;
        }
       	if(*y>=(w.ws_row-EXCESS))//moved too far down
        {
		int scrollDownStatus = scrollDown(lines, fileLine, mLComment, w);
		if(scrollDownStatus == STATUS_ERROR)
                {
                        jError("scrollDown");
                        return STATUS_ERROR;
                }
		if(scrollDownStatus == 1)
		{
			scroll = true;
		}
		(*y)=(w.ws_row-EXCESS)-1;
        }

        if(lines[(*y)][0] == '\0')//double \0 check (cant move through -=NULL=- lines line they are full)
	{
		(*x)=0;
		if(*y > 0)
		{
                	if(lines[ (*y)-1 ][0] == '\0')
                	{
                	        (*y)--;
                	}
		}

		return (scroll ? 0 : 1);
        }

	//when y chages x may be larger then line
      	if((*x)>(int)strlen(lines[(*y)]) - 1)
        {
        	(*x)=strlen(lines[(*y)]) - 1;
	}
	if(end)
	{
		(*x)=strlen(lines[(*y)]) - 1;
	}

	return (scroll ? 0 : 1);
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *str : string/ line to insert into
		char c : what will be inserted
		int pos : insert at

        Does: puts char into string moves rest of sting to accomidate

        Returns: int
                0 : string allready full, no room
                1 : success
*/
int insertIntoString(char *str, char c, int pos)
{
	if(strlen(str) == 0)//empty line
	{
		char temp[3];
		temp[0]=c;
		temp[1]='\n';
		temp[2]='\0';

		(void)strcpy(str,temp);
		return 1;
	}
        if(strlen(str) == (LEN - 1))
        {
                (void)strcpy(message, "line full");
		return 0;
        }

        while(pos < (int)strlen(str)+1)
        {//shift chars after index over
                char temp = str[pos];
                str[pos] = c;
                c = temp;
                pos++;
        }
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char ***ptr : holds array of char pointers to hold file lines
		int oldSize : <<
		int newSize : <<

        Does: changes number of lines screen can hold

        Returns: bool
                true : success
                false : fail
*/
bool remallocFunc(char ***ptr, int oldSize, int newSize)
{
        char **newPtr = malloc(newSize * sizeof(char*));
        if(newPtr == NULL)
        {
                jError("malloc");
		return false;
        }

        //move old values over
        for(int i=0; i<(oldSize<=newSize? oldSize: newSize); i++)
        {
                newPtr[i] = (*ptr)[i];
        }
        (void)free(*ptr);
        (*ptr) = newPtr;

        return true;
}


/*
        Written by: Jonathan Pincus

        Parameters:
		struct winsize w : screen size

        Does: checks if screen is smaller of larger then predifined limit

        Returns: bool
                true : screen right size
                false : screen too small or large
*/
bool rightSize(struct winsize w)
{
	if( w.ws_row < WIN_MIN_ROW || w.ws_row > WIN_MAX_ROW )
	{
		return false;
	}
	if(w.ws_col < WIN_MIN_COL || w.ws_col > WIN_MAX_COL )
	{
		return false;
	}
	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char*** lines : loaded portion of file
		struct winsize* w : screen size
		int* y : cursor position
		size_t *fileLine : track number of unloaded lines above loaded portion of file

        Does: handles when the window size is changed

        Returns: bool
                true : success
                false : fail
*/
bool changeWindowSize(char*** lines, struct winsize* w, int* y, size_t *fileLine, struct stackItem **mLComment)
{
	int oldRow = w->ws_row;
        if(ioctl(STDOUT_FILENO, TIOCGWINSZ, w) == STATUS_ERROR)//get new size
	{
		jError("ioctl");
		return false;
	}
        (void)nearestEight(w);

        if(!rightSize(*w))//window size wrong
        {
		jError("Window is wrong size");
		w->ws_row = oldRow;
		return false;
	}

	(void)printHF(*w);
        int change = oldRow - (*w).ws_row;
     	if(change == 0)
        {
        	return true;
        }

        if(change > 0)//screen got smaller
        {
		//remove from both top and bottum so cursor stays on screen
		int linesUnderCursor = (oldRow - EXCESS)-((*y)+1);
		int neededFromAbove = change;

		//remove lines bellow cursor
		int j = 0;
		for(int i = oldRow-EXCESS-1; j<linesUnderCursor; i--)
                {
			neededFromAbove--;
                       	if(append((*lines)[i],fPD) == STATUS_ERROR)
                        {
                        	jError("append");
                                return false;
                	}
			j++;
			if(j == change)
			{
				break;
			}
                }

		//remove rest of lines from top
		for(int i =0; i < neededFromAbove; i++)
		{
			//may move multi line comment into file
			if(append((*lines)[i],fPU) == STATUS_ERROR)
                        {
                                jError("append");
                                return false;
                        }

			if(!MLC_Tracker((*lines)[i], fileLine, mLComment))
        		{
                		jError("MLC_Tracker");
                		return STATUS_ERROR;
        		}
		}


		//shift up before remalloc
		int index = 0;
		for(int i = neededFromAbove; i < w->ws_row-EXCESS+neededFromAbove; i++)
		{
			(void)strcpy((*lines)[index], (*lines)[i]);
			index++;
		}

		//free
		for(int i = oldRow-change-EXCESS; i< oldRow-EXCESS; i++)
		{
			(void)free((*lines)[i]);
		}

		if(remallocFunc(lines, oldRow, w->ws_row) == false)
                {
               		jError("remallocFunc");
                        return false;
                }

		//translate y
		(*y)-=neededFromAbove;

		//checking if number of lines in file longer then max unsigned long
                if(ULONG_MAX-neededFromAbove >= *fileLine)
                {
                        (*fileLine) += neededFromAbove;
                }
                else
                {
                        jError("FileLine too long");
                        return false;
                }
		return true;
	}

        //screen got bigger
	if(remallocFunc(lines, oldRow, w->ws_row) == false)
        {
        	jError("remallocFunc");
                return false;
        }
       	for(int i=oldRow-EXCESS; i<((*w).ws_row-EXCESS); i++)
        {
        	(*lines)[i] = malloc(LEN * sizeof(char));
        	if((*lines)[i] == NULL)
        	{
                	jError("malloc");
			return false;
                }

               	//fill new space with data
                char str[LEN];

                int lastLineStatus = lastLine(str, fPD);
                if(lastLineStatus == 0)//no more line
                {
                	(void)strcpy((*lines)[i], "");
                }
                else if(lastLineStatus == STATUS_ERROR)
               	{
                	jError("lastLine");
			return false;
                }
                else
                {
                	(void)strcpy((*lines)[i], str);
               	}
	}

	return true;
}


/*
        Written by: Jonathan Pincus

        Parameters:
               struct winsize w : size of screen

        Does: prints manual

        Returns: N/A
*/
void manual(struct winsize w)
{
        char *man[] = {
        "Search for word:    CTRL-W",
        "Go to Line:         CTRL-G",
        "Copy:               CTRL-D",
	"Paste:              CTRL-F",
	"Exit:               CTRL-X",
	"                          ",
        "Cancel Sortcut:     CTRL-C"};

        int halfy = w.ws_row/2;
        int starty = halfy - (sizeof(man) / sizeof(man[0]))/2;

        int halfx = w.ws_col/2;
        int startx = halfx - ( strlen( man[0] )/2 );

       	(void)printf("%s%s", COLOUR[1], INVERT);
        for(int i =0; i<(int)(sizeof(man) / sizeof(man[0])); i++)
	{
                (void)moveCursor(startx, starty);
                (void)printf("%s",man[i]);
        	starty++;
        }
	(void)printf("%s%s", COLOUR[0], INVERT_UNDO);
	(void)fflush(stdout);
}

/*
	Written by: Jonthan Pincus

	Parameter:
		int y : position of cursor
		struct stackItem **mLComment : tracks (/oo/) Multi Line Comments above loaded portion of file
		char **lines : loaded portion of file

	Does: check if position of cursor is in Multi Line Comment (/oo/)

	Returns: bool
		true : in Multi Line Comment
		false : not part of Multi Line Comment
*/
bool MLC_Checker(int y, struct stackItem **mLComment, char **lines)
{
	//go up until opening or closing is found
	for(int i = y-1; i>=0; i--)
	{
		if( unload_MLC_Start(lines[i]) )
		{
			return true;
		}
		if( unload_MLC_End(lines[i]) )
		{
			return false;
		}
	}

	//if no opening or closing still possible to be in one becuase of unloaded portion of file
	if(!isEmptyS(*mLComment))//checks if there is unloaded multi line comment
        {
                if(halfS(*mLComment))
                {
                        return true;
                }
        }
	return false;
}

/*
	Written By: Jonathan Pinucs

	Parameter:
		int *x : cursor position
		int *y : cursor position
		uint64_t input : users char input
		char ***lines : loaded portion of file
		struct stackItem **mlComment : track Multi line Comments (/o) above file
		size_t *fileLine : number of lines above loaded portion of file
		struct expandString *e : holds all prints for single IO call
		struct winsize *w : size of screen
		bool reprint : system may need a fourced reprint

	Does: prints new screen after user enters char only prints bit affected by change

	Returns: int
		-1 : error
		0 : printing done in this func
		1 : full reprint needed
*/
int handleCharInput(int *x, int *y, uint64_t input, char ***lines,
struct stackItem **mLComment, size_t *fileLine, struct expandString *e, struct winsize *w, bool reprint)
{
	//tracking MLCs
	bool CommentStart1 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd1 = unload_MLC_End((*lines)[*y]);

	int  insertIntoStringStatus = insertIntoString((*lines)[(*y)], (char)input, (*x));
        if(insertIntoStringStatus == 1)//good
        {
        	(*x)++;
        }

	if(reprint)
        {
                return 1;
        }

	int *pipe = NULL;//real pipe not needed
	bool CommentStart2 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd2 = unload_MLC_End((*lines)[*y]);

	/*printf("original has open %s\n", (CommentStart1 ? "true" : "false"));
	printf("original has closed %s\n", (CommentEnd1 ? "true" : "false"));
	printf("original has open %s\n", (CommentStart2 ? "true" : "false"));
	printf("original has closed %s\n", (CommentEnd2 ? "true" : "false"));
	sleep(5);*/

        if(insertIntoStringStatus == 0)//no room print errro message
	{
		if(PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe) == STATUS_ERROR)
        	{
                	jError("printError");
                	return STATUS_ERROR;
        	}
		(void)strcpy(message,"");
	}
        else if(CommentStart1 == CommentStart2 && CommentEnd1 == CommentEnd2)//no changes to impact under lines
        {//only print line
                if(printMiddleLine(lines, w, x, y, e, mLComment, fileLine, pipe) == STATUS_ERROR)
		{
			jError("printMiddleLine");
			return STATUS_ERROR;
		}
	}
        else
      	{//print bellow
               	if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, *y) == STATUS_ERROR)
		{
			jError("printMiddle");
			return STATUS_ERROR;
		}
        }

	int tempx1 = *x;
        (void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
        (void)moveCursor(tempx1+1, *y+3);
	return 0;
}

/*
        Written By: Jonathan Pinucs

        Parameter:
                int *x : cursor position
                int *y : cursor position
                char ***lines : loaded portion of file
                struct stackItem **mlComment : track Multi line Comments (/o) above file
                size_t *fileLine : number of lines above loaded portion of file
                struct expandString *e : holds all prints for single IO call
                struct winsize *w : size of screen
                bool reprint : system may need a fourced reprint

        Does: prints new screen after user backspaces only prints bit affected by change

        Returns: int
                -1 : error
                0 : printing done in this func
                1 : full reprint needed
*/
int handleBackspaceInput(int *x, int *y, char ***lines,
struct stackItem **mLComment, size_t *fileLine, struct expandString *e, struct winsize *w, bool reprint)
{
	int oldY = (*y);
	//tracking MLCs
	bool CommentStart1 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd1 = unload_MLC_End((*lines)[*y]);
	int backspaceStatus = backspace(x, y, *lines, fileLine, mLComment, *w);
	if(backspaceStatus == STATUS_ERROR)
	{
		jError("backspace");
		return STATUS_ERROR;
	}
	if(reprint)
	{
		return 1;
	}

	int *pipe = NULL;//real pipe not needed
	bool CommentStart2 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd2 = unload_MLC_End((*lines)[*y]);

	if(backspaceStatus == -2)//backspace compines two lines and results in line larger that buffer so cancel
	{
        	if(PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe) == STATUS_ERROR)
               	{
                        jError("printError");
                	return STATUS_ERROR;
                }
                (void)strcpy(message,"");
	}
	else if(oldY != (*y) || CommentStart1 != CommentStart2 || CommentEnd1 != CommentEnd2)
	{//backspace scroll or changed /oo/ which effects under
		if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, *y) == STATUS_ERROR)
               	{
                	jError("printMiddle");
                        return STATUS_ERROR;
         	}
	}
	else//only single line effected
	{
		if(printMiddleLine(lines, w, x, y, e, mLComment, fileLine, pipe) == STATUS_ERROR)
                {
                	jError("printMiddleLine");
                        return STATUS_ERROR;
                }
	}
	int tempx1 = *x;
        (void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
        (void)moveCursor(tempx1+1, *y+3);
        return 0;
}

/*
        Written By: Jonathan Pinucs

        Parameter:
                int *x : cursor position
                int *y : cursor position
                char ***lines : loaded portion of file
                struct stackItem **mlComment : track Multi line Comments (/o) above file
                size_t *fileLine : number of lines above loaded portion of file
                struct expandString *e : holds all prints for single IO call
                struct winsize *w : size of screen
                bool reprint : system may need a fourced reprint

        Does: prints new screen after user uses delete key only prints bit affected by change

        Returns: int
                -1 : error
                0 : printing done in this func
                1 : full reprint needed
*/
int handleDeleteInput(int *x, int *y, char ***lines,
struct stackItem **mLComment, size_t *fileLine, struct expandString *e, struct winsize *w, bool reprint)
{
	//tracking MLCs
	bool CommentStart1 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd1 = unload_MLC_End((*lines)[*y]);
        int deleteFuncStatus = deleteFunc(x, y, *lines, *w);
	if(deleteFuncStatus == STATUS_ERROR)
        {
        	jError("delete");
                return STATUS_ERROR;
        }
	if(reprint)
        {
                return 1;
        }

	int *pipe = NULL;//real pipe not needed
	bool CommentStart2 = unload_MLC_Start((*lines)[*y]);
        bool CommentEnd2 = unload_MLC_End((*lines)[*y]);

	if(deleteFuncStatus == -2)//delete would have merged two lines resulting in line longer than buffer
	{//no changes made, but error needs print
		if(PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe) == STATUS_ERROR)
                {
                	jError("printError");
                        return STATUS_ERROR;
                }
                (void)strcpy(message,"");
	}
	else if(deleteFuncStatus == 1 || CommentStart1 != CommentStart2 || CommentEnd1 != CommentEnd2)//backspace scrolled so reprint under or /oo/ change detected
        {
                if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, *y) == STATUS_ERROR)
                {
                        jError("printMiddle");
                        return STATUS_ERROR;
                }
        }
        else//only single line effected
        {
                if(printMiddleLine(lines, w, x, y, e, mLComment, fileLine, pipe) == STATUS_ERROR)
                {
                        jError("printMiddleLine");
                        return STATUS_ERROR;
                }
        }
        int tempx1 = *x;
        (void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
        (void)moveCursor(tempx1+1, *y+3);
        return 0;
}

/*
        Written By: Jonathan Pinucs

        Parameter:
                int *x : cursor position
                int *y : cursor position
                uint64_t input : users char input
                char ***lines : loaded portion of file
                struct stackItem **mlComment : track Multi line Comments (/o) above file
                size_t *fileLine : number of lines above loaded portion of file
                struct expandString *e : holds all prints for single IO call
                struct winsize *w : size of screen
                bool reprint : system may need a fourced reprint

        Does: depending on what that arrow key does function prints appropriate screen changes

        Returns: int
                -1 : error
                0 : printing done in this func
                1 : full reprint needed
*/
int handleArrowInput(int *x, int *y, uint64_t input, char ***lines,
struct stackItem **mLComment, size_t *fileLine, struct expandString *e, struct winsize *w, bool reprint)
{
	int preMoveY = *y;
	int tempX = *x;
	int preMoveStart = currentLineStart((*lines)[*y], &tempX, *w);

	//move cursor
	int arrowStatus = Arrows(input, x, y, *lines, *w, fileLine, mLComment);
        if(arrowStatus == STATUS_ERROR)
        {
        	jError("Arrows");
                return STATUS_ERROR;
        }

        if(reprint || arrowStatus == 0)//full reprint
        {
		return 1;
        }

	int *pipe = NULL;//pipe not actually used so use fake

	if(preMoveY != *y && preMoveStart != 0)//move of line that was not on it's first page so reset
	{
		int newX = 0;
		if(printMiddleLine(lines, w, &newX, &preMoveY, e, mLComment, fileLine, pipe) == STATUS_ERROR)
                {
                        jError("printMiddleLine");
                        return STATUS_ERROR;
                }
	}

	tempX = *x;
        int newStart = currentLineStart((*lines)[*y], &tempX, *w);
	if(newStart != 0 || (preMoveY == *y && newStart != preMoveStart))
	{
		if(printMiddleLine(lines, w, x, y, e, mLComment, fileLine, pipe) == STATUS_ERROR)
                {
                        jError("printMiddleLine");
                        return STATUS_ERROR;
                }
	}

        (void)moveCursor(tempX+1, *y+3);
        return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		int *x : cursor position
		int *y : cursor position
		uint64_t input : user's keyboard input
		char ***lines : loaded protion of screen
		struct stackItem **mLComment : tracks (/oo/) multiline comments above loaded portion of file
		size_t *fileLine : traks number of lines above loaded protion of file
		struct expandString *e : holds screen print for single io call
		int *pipe : pipe to inputing process
		struct winsize *w : size of screen
		bool *reprint : fources reprint of screen because of manual

        Does: handles user input by calling routines to handle different inputs + prints result on screen

        Returns: int
                0 : normal exectuion
		1 : user wants to quit
                -1 : error
*/
int process2(int *x, int *y, uint64_t input, char ***lines,
struct stackItem **mLComment, size_t *fileLine, struct expandString *e, int *pipe, struct winsize *w, bool *reprint)
{
	if( (input >= ' ' && input <= '~') || input == 9 )
	{
		int handleCharInputStatus = handleCharInput(x, y, input, lines, mLComment, fileLine, e, w, *reprint);
		if(handleCharInputStatus == STATUS_ERROR)
		{
			jError("handleCharInput");
			return STATUS_ERROR;
		}
		else if(handleCharInputStatus == 0)
		{
			return 0;
		}
	}
	else if(input >= UP && input <= LEFT)
	{
		int handleArrowInputStatus = handleArrowInput(x, y, input, lines, mLComment, fileLine, e, w, *reprint);
		if(handleArrowInputStatus == STATUS_ERROR)
		{
			jError("handleArrowInputStatus");
                        return STATUS_ERROR;
		}
		if(handleArrowInputStatus == 0)
		{
			return 0;
		}
	}
	else
	{
		switch (input)
        	{
                	case CTRL_G://move to line
                        	if(moveTo(x, y, lines, w, fileLine, mLComment, e, pipe) == STATUS_ERROR)
				{
					jError("moveTo");
					return STATUS_ERROR;
				}
                	break;
			case ENTER_KEY:
			{
				int oldY = *y;

				int enterStatus = Enter(x, y, *lines, fileLine, mLComment, *w);
				if(enterStatus == STATUS_ERROR)
                                {
                                        jError("Enter");
                                        return STATUS_ERROR;
                                }
				if(!(*reprint) && enterStatus == 0)
				{
					//only print under y
					if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, oldY) == STATUS_ERROR)
                			{
                        			jError("printMiddle");
                        			return STATUS_ERROR;
                			}

        				int tempx1 = *x;
        				(void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
        				(void)moveCursor(tempx1+1, *y+3);
        				return 0;
				}
			}
                	break;
			case CTRL_D://copy
				if(copyRoutine(x, y, lines, w, fileLine, mLComment, e, pipe) == STATUS_ERROR)
                                {
                                        jError("copyRoutine");
                                        return STATUS_ERROR;
                                }
                        break;
			case CTRL_F://Paste
			{
				int pasteFuncStatus = pasteFunc(x, y, *lines, *w, fileLine, mLComment);
				if(pasteFuncStatus == STATUS_ERROR)
                                {
                                        jError("pasteFunc");
                                        return STATUS_ERROR;
                                }

				if((pasteFuncStatus == 0 || pasteFuncStatus == -3) && !(*reprint))//no changes made
				{
					(void)PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe);
					(void)strcpy(message,"");

					//move cursor
					int tempx1 = *x;
                        		(void)currentLineStart((*lines)[*y], &tempx1, *w);//translates x to actual position acounting for tabs
                        		(void)moveCursor(tempx1+1, *y+3);
					return 0;
				}
			}
			break;
			case CTRL_W://find word
				if(find(x, y, lines, w, fileLine, mLComment, e, pipe)  == STATUS_ERROR)
                                {
                                        jError("find");
                                        return STATUS_ERROR;
                                }
			break;
			case CTRL_X://exits program
			{
				int exitProgramStatus = exitProgram(lines, w, x, y, fileLine, mLComment, e, pipe);
				if(exitProgramStatus == 1)
				{
					return 1;
				}
				if(exitProgramStatus == STATUS_ERROR)
				{
					jError("exitProgramStatus");
					return STATUS_ERROR;
				}
			}
			break;
			case BACKSPACE_KEY:
                        	if(handleBackspaceInput(x, y, lines, mLComment, fileLine, e, w, *reprint)  == STATUS_ERROR)
                                {
                                        jError("handleBackspaceInput");
                                        return STATUS_ERROR;
                                }
				if(!(*reprint))
				{
					return 0;
				}
			break;
			case 8://manual
				manual(*w);
				*reprint = true;
				return 0;
			break;
			case 771://custom code for winsize change
				if(changeWindowSize(lines, w, y, fileLine, mLComment) == false)
                                {
                                        jError("changeWindowSize");
					return STATUS_ERROR;
                                }
			break;
			case DELETE_KEY:
				if(handleDeleteInput(x, y, lines, mLComment, fileLine, e, w, *reprint)  == STATUS_ERROR)
                                {
                                        jError("handleDeleteInput");
                                        return STATUS_ERROR;
                                }
                                if(!(*reprint))
                                {
                                        return 0;
                                }
			break;
			/*default :
				printf("NUMBER: %zu\n", input);
				sleep(5);
			break;*/
		}
	}
	//printf("full reprint\n");
	//sleep(1);
	if(printMiddle(lines, w, x, y, fileLine, mLComment, e, message, pipe, 0) == STATUS_ERROR)
	{
		jError("printMidde");
		return STATUS_ERROR;
	}
	*reprint = false;
	return 0;//fine
}

/*
        Written by: Jonathan Pincus

        Parameters:
		struct winsize *w : screen size

        Does: changes recorded value of screen size to nearest 8 (previous 8 eg: 15 -> 8, 17 -> 16) so tabs always end at end of screen

        Returns: N/A
*/
void nearestEight(struct winsize *w)
{
        int new = w->ws_col;
        while(new%8 != 0)
        {
                new--;
        }
        w->ws_col = new;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char *line : destination string
		int ind : check if string exists here
		char *str : target

        Does: checks for string at index in other string

        Returns: bool
                true : sting at index
                false : sting not at index
*/
bool searchSubString(char *line, int ind, char *str)
{
	//if line left to small to hold string
	if((strlen(line) - ind)  <  strlen(str) )
	{
		return false;
	}

	for(int i=0; i< (int)strlen(str); i++)
	{
		if(line[ind+i] != str[i])
		{
			return false;
		}
	}
	return true;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int pipe[2] : used by inputing process to save inputs
		uint64_t *data : stores user's final input

        Does: handle multi stage inputs line Arrows

        Returns: bool
                0 : success
                -1 : error
*/
int multi(int pipe[2], uint64_t *data)
{
	char c = 0;
        struct timespec t;

	int readStatus = read(pipe[0], &c, sizeof(char));//skiped on interupt??

	if(readStatus == 0)//pipe has been closed because of error in input func
	{
		//(void)printf("pipe has been closed in input process because of error\n");
		jError("read");
		return STATUS_ERROR;
	}
	if(readStatus == STATUS_ERROR)
	{
		//errors anticipated if flags are set
		if(cancelFlag)
        	{
                	*data = 1;
                	*data <<= 8;
                	*data += 1;
                	cancelFlag=false;
                	return 0;
        	}
		else if(windowFlag)
        	{
                	*data = 3;
                	*data <<= 8;
                	*data += 3;
                	windowFlag = false;
                	return 0;
        	}
		else
		{
			jError("read");
                        return STATUS_ERROR;
		}
	}

        readStatus = read(pipe[0], &t, sizeof(struct timespec));
	if(readStatus == 0)//pipe has been closed because of error in input func
        {
                printf("pipe has been closed in input process because of error\n");
                return STATUS_ERROR;
        }
        if(readStatus == STATUS_ERROR)
        {
                jError("read");
                return STATUS_ERROR;
        }

	(*data) = c;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);

	//adding poll should stop some errors
	while(TimeDiff(t, now) < 1000000 || poll(&(struct pollfd){ .fd = pipe[0], .events = POLLIN }, 1, 0)==1)
	{//1000000 seems to be enough for system but too little for user inputs to be muddled
        	if(clock_gettime(CLOCK_MONOTONIC_RAW, &now) == STATUS_ERROR)
		{
			jError("clock_gettime");
			return STATUS_ERROR;
		}
                if(poll(&(struct pollfd){ .fd = pipe[0], .events = POLLIN }, 1, 0)==1)
                {
			readStatus = read(pipe[0], &c, sizeof(char));
			if(readStatus == 0)//pipe has been closed because of error in input func
        		{
                		//(void)printf("pipe has been closed in input process because of error\n");
                		jError("read");
				return STATUS_ERROR;
        		}
        		if(readStatus == STATUS_ERROR)
        		{
                		jError("read");
                		return STATUS_ERROR;
        		}
                        readStatus = read(pipe[0], &t, sizeof(struct timespec));
                        if(readStatus == 0)//pipe has been closed because of error in input func
        		{
                		printf("pipe has been closed in input process because of error\n");
                		return STATUS_ERROR;
        		}
        		if(readStatus == STATUS_ERROR)
        		{
                		jError("read");
                		return STATUS_ERROR;
        		}
			if(clock_gettime(CLOCK_MONOTONIC_RAW, &now) == STATUS_ERROR)
			{
				jError("clock_gettime");
				return STATUS_ERROR;
			}
			*data <<= 8;
                        *data +=c;
                }
        }
	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		int pipe[2] : used to retrive input from inputing process
		struct winsize *w : size of screen
		char ***ptr : loaded portion of file

        Does: retrieves and loads file, then loops through input and output till error or quit

        Returns: int
                1 : user wants to quit
                -1 : Error (file needs reconstuction and changes need to be save seperately)
		-2 : Error (file needs to be reconstructed)
		-3 : Error (file not been touched, no remake)
		-26 : Error (trying to edit working code)
*/
int process(int pipe[2], struct winsize *w, char ***ptr)
{
	int x=0;
        int y=0;
	size_t fileLine = 0;//track #of lines above top of screen
	bool reprint=false;
	struct stackItem *mLComment = NULL;

	(void)nearestEight(w);
        if(!rightSize(*w))//new window may be too small
        {
                jError("Screen Size Error");
		return -3;
        }

        if (access(fPD, F_OK) == 0) {
		printf("This program uses a temporary file called: %s, this file already exists\n", fPD);
		return -3;
	}

	if (access(fPU, F_OK) == 0) {
                printf("This program uses a temporary file called: %s, this file already exists\n", fPU);
                return -3;
        }

        if (access(fPS, F_OK) == 0) {
                printf("This program uses a temporary file called: %s, this file already exists\n", fPS);
                return -3;
        }

	if(access(fPO, F_OK) == 0)
	{
		if(duplicateFile(fPO, fPS) == STATUS_ERROR)//save original context
		{
			jError("dublicateFile");
			return -3;
		}
	}
	else//file passed in needs to be created
	{
		FILE *fptr = fopen(fPO, "w");
        	fclose(fptr);
		fptr = fopen(fPS, "w");
                fclose(fptr);
		fptr = fopen(fPD, "w");
        	if(fptr == NULL)
        	{
                	jError("fopen");
                	return STATUS_ERROR;
        	}
        	if(fclose(fptr) != 0)
        	{
        	        jError("fclose");
                	return STATUS_ERROR;
        	}
	}

	FILE *fptr = fopen(fPU, "w");//create file for later use to store lines above current protion
	if(fptr == NULL)
	{
		jError("fopen");
		return -3;
	}

	struct expandString e;//stores prints for single IO call
        if(makeES(&e) == false)
        {
                jError("makeES");
                return -3;
        }
        if(makeES(&pasteBuffer) == false)
        {
                jError("makeES");
                return -3;
        }

	(*ptr) = malloc(((*w).ws_row - EXCESS) * sizeof(char*));//array of strings to hold screen data
        if( (*ptr) == NULL )
        {
                printf("malloc fial\n");
                return -3;
        }

        for( int i = 0; i < ((*w).ws_row - EXCESS); i++ )
        {
                (*ptr)[i] = malloc(LEN * sizeof(char));
                if( (*ptr)[i] == NULL )
                {
                        printf("malloc fail\n");
                        return -3;
                }
        }

	//beyond this point actual file manipulatiion takes place so error will trigger recration of original file


	int flipFileStatus = flipFile(fPO, fPD);
	if(flipFileStatus == STATUS_ERROR)
	{
		jError("flipFile");
		return -2;
	}
	else if(flipFileStatus == -26)
	{
		jError("flipFile");
                return -26;
	}

	int loadfullStatus = loadfull(*ptr, *w);
	if(loadfullStatus < 0)
        {
               	jError("loadfull");
       		return loadfullStatus;
	}

	(void)printHF(*w);
	if(printMiddle(ptr, w, &x, &y, &fileLine, &mLComment, &e, message, pipe, 0) == STATUS_ERROR)
	{
		jError("printMiddle");
		return -2;
	}

	while(true)//input print loop
	{
		uint64_t data;
		if(multi(pipe, &data) == STATUS_ERROR)
		{
			jError("multi");
			return STATUS_ERROR;
		}
		int process2Status = process2(&x, &y, data, ptr, &mLComment, &fileLine, &e, pipe, w, &reprint);
		if(process2Status == 1)//quiting because of user
		{
			return process2Status;
		}
		if(process2Status != 0)//quiting because of error
		{
			jError("Process2");
			return process2Status;
		}
	}
	return STATUS_ERROR;//never gets here
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char** lines : portion of file for display
		struct winsize w : size of screen


        Does: loads file into lines

        Returns: int
                0 : success
                -1 : error
*/
int loadfull(char** lines, struct winsize w)
{
        for(int i=0; i<(w.ws_row-EXCESS); i++ )
        {
                char str[LEN] = "";
                int lastLineStatus = lastLine(str, fPD);
		if(lastLineStatus == 1)
                {
                        (void)strcpy(lines[i], str);
                }
                else if(lastLineStatus == 0)
                {
                        (void)strcpy(lines[i], "");
                }
		else if(lastLineStatus < 0)
		{
			jError("lastLine");
			return lastLineStatus;
		}
        }
	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int i : num

        Does: return gap to next eight (3 = 5), (7 = 1)

        Returns: int
                n : gap
*/
int nearestEightDiff(int i)
{
	if(i%8 == 0)
	{
		return 8;
	}
	int gap=0;
	while(i%8 != 0)
	{
		gap++;
		i++;
	}
	return gap;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *str : current line
                int *x : index of cursor
		struct winsize w : size of screen

        Does: find start index so cursor of will be on screen (cursor at 300, but screen size only 200, so start at 100+)

        Returns: int
                n : index of start
*/
int currentLineStart(char *str, int *x, struct winsize w)
{
	int end=0;
        int jProper =0;
	int xminus = 0;
        int xplus = 0;
	int start;

        do
	{
		xplus=0;

		if(end != 0)
		{
                	xminus=end;
		}

                start=end;
		end+=w.ws_col;

                for(int j = start; j<end; j++)
                {
                	if(j >= (int)strlen(str))//end may be greater then length
                       	{
                       		break;
                       	}

			if(str[j] == '\t')//tab takes to nearest x%8 = 0
                        {
				int plus = nearestEightDiff(jProper) - 1;//total printed by tab
                                jProper += plus;

                                end -= plus;//print less of line cause tab

                                if( (*x)-1 >= j )//does not hapen after our cursor
                                {
                                	xplus+=plus;
                                }
                	}
                        jProper++;
           	}

        }while(end <= (*x));//while our cursor not in buffer
	(*x)=(*x)+xplus-xminus;//new x outside function
	return start;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                const char *str : current line
                int index : index to check at


        Does: checks for Bold words

        Returns: int
                -1 : no caps at index
                n : starting at index the n chars are caps
*/
int Caps(char *str, int index)
{
	//check if previous is spaces or stuff
	if(index == 0)
	{}
	else if(str[index-1] == '\t'|| str[index-1] == ' ' ||
		str[index-1] == '[' || str[index-1] == ']' ||
		str[index-1] == '(' || str[index-1] == ')' ||
		str[index-1] == '{' || str[index-1] == '}' ||
		str[index-1] == '-' || str[index-1] == ',' ||
		str[index-1] == '+' || str[index-1] == '/' ||
		str[index-1] == '*' )
	{}
	else
	{
		return -1;
	}

	int count=0;
	int digitCount=0;
	while(index < (int)strlen(str))
	{
		if((str[index] >= 65 && str[index] <= 90) || str[index] == 95)
		{
			count++;
		}
		else if(str[index] >= 48 && str[index] <= 57 )
		{//number can be part of constants-names but not constant-names in themself
			count++;
			digitCount++;
		}
		else if(str[index] == '\t'|| str[index] == ' ' ||
                	str[index] == '[' || str[index] == ']' ||
                	str[index] == '(' || str[index] == ')' ||
                	str[index] == '{' || str[index] == '}' ||
			str[index] == ',' || str[index] == '\n'||
			str[index] == '-' || str[index] == '+' ||
			str[index] == '/' || str[index] == '*' ||
			str[index] == ';' )
		{
			if(count != 0 && count != digitCount)
			{
				return count;
			}
			return -1;
		}
		else
		{
			return -1;
		}

		index++;


	}
	if(count > 0 && count != digitCount)
	{
		return count;
	}
	return -1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                const char *str : current line
		int index : index to check at
		char *strSub : look for this


        Does: checks for string at index

        Returns: bool
                true : exists
                false : does not exist
*/
bool subString(const char *str, int index, const char *strSub)
{
	if(strlen(str)-index < strlen(strSub))//no room
        {
                return false;
        }

	if(index == 0)
	{}
	else if(str[index-1] == '\t'|| str[index-1] == ' ' ||
                str[index-1] == '[' || str[index-1] == ']' ||
                str[index-1] == '(' || str[index-1] == ')' ||
                str[index-1] == '{' || str[index-1] == '}' ||
                str[index-1] == '-' || str[index-1] == ',' ||
                str[index-1] == '+' || str[index-1] == '/' ||
                str[index-1] == '*' )
	{}
	else
	{
		return false;
	}

	for(int i=0; i<(int)strlen(strSub); i++)
	{
		if(str[index] != strSub[i])
		{
			return false;
		}
		index++;
	}
	if(str[index] == '\t'|| str[index] == ' ' ||
	   str[index] == '[' || str[index] == ']' ||
           str[index] == '(' || str[index] == ')' ||
           str[index] == '{' || str[index] == '}' ||
           str[index] == ',' || str[index] == '\n'||
           str[index] == '-' || str[index] == '+' ||
           str[index] == '/' || str[index] == '*' ||
	   str[index] == ';' )
	{//followed by these alloed allowed char
		return true;
	}
	return false;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		char c: current char to be printed
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int index : index of element in str
                int jProper : absolute x position on screen
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: print(stores print) and highlights edge of screen if line is too long

        Returns: int
                -1 : fail
                1 : success
*/
int printInverted(char c, int start, int end, int index, int jProper, char *str, struct expandString *e)
{
	char strTemp[LEN];
	if(c == '\t')
	{
		int plus = nearestEightDiff(jProper) - 1;
		index += plus;
	}

	if(index>=end && end < ((int)strlen(str)-1) )//line goes of screen to right
	{//highligh char to indicate more line to be seen
		if(c != '\t')
		{
			(void)strcpy(strTemp, "");
			(void)sprintf(strTemp, "%s%c%s", COLOUR_BG[6], c, COLOUR_BG[0]);
			if(appendES(e, strTemp) == STATUS_ERROR)
			{
				jError("appendES");
				return STATUS_ERROR;
			}
		}
		else
		{
			int plus = nearestEightDiff(jProper) - 1;
			(void)strcpy(strTemp, "");
                        (void)sprintf(strTemp, "%*s%s %s", plus, "", COLOUR_BG[6], COLOUR_BG[0]);
			if(appendES(e, strTemp) == STATUS_ERROR)
                        {
                                jError("appendES");
                                return STATUS_ERROR;
                        }
		}
	}
	else if(start != 0 && start == index-1)//line goes of screen on left
	{//highlight char to indicate more line can be seen
		if(c == '\t')//first tap is always 8 spaces
		{
			(void)strcpy(strTemp, "");
                        (void)sprintf(strTemp, "%s %s       ", COLOUR_BG[6], COLOUR_BG[0]);
			if(appendES(e, strTemp) == STATUS_ERROR)
                        {
                                jError("appendES");
                                return STATUS_ERROR;
                        }
		}
		else
		{
			(void)strcpy(strTemp, "");
                        (void)sprintf(strTemp, "%s%c%s", COLOUR_BG[6], c, COLOUR_BG[0]);
                        if(appendES(e, strTemp) == STATUS_ERROR)
                        {
                                jError("appendES");
                                return STATUS_ERROR;
                        }
		}
	}
	else
	{//normal print
                if(appendES(e, (char[2]) {c, '\0'}) == STATUS_ERROR)
              	{
                	jError("appendES");
                	return STATUS_ERROR;
		}
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *str : current line
                int index : check for fisrt ' at this index

        Does: checks for single quotes ('G') at index given

        Returns: bool
                true : single quotes found at index
                false : not found at this index
*/
bool SingleQuotes(char *str, int index)
{
        //only does standard eg 'B', not '\n'
        if(strlen(str)-index < 3)
        {
                return false;
        }

        if(str[index] == '\'' && str[index+2] == '\'' && str[index+1] != '\'')
        {
		return true;
        }
        else
        {
                return false;
        }
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int jProper : track absolute postion on line
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int j : index of element in str
                int *skip : records if element has be printed
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights single quotes on sreen eg: 'B'

        Returns: int
                -1 : fail
                1 : success
*/
int printSingleQuotes(int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
	if(appendES(e, COLOUR[5]) == STATUS_ERROR)
	{
        	jError("appendES");
       		return STATUS_ERROR;
	}

	for(int ind = 0; ind<3; ind++)
	{
		if(j+ind >= start && j+ind<end)
                {
                        if(printInverted(str[j + ind ], start, end, j+ind+1, jProper, str, e) == STATUS_ERROR)
                        {
                               	jError("printInverted");
                                return STATUS_ERROR;
                	}
                }
		(*skip)++;
	}


	if(appendES(e, COLOUR[0]) == STATUS_ERROR)
        {
                jError("appendES");
        	return STATUS_ERROR;
	}

	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int jProper : track absolute postion on line
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int j : index of element in str
                int *skip : records if element has be printed
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights Bold words (all caps)

        Returns: int
                -1 : fail
                1 : success
*/
int printBold(int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
	int check = Caps(str, j);
        if(check != -1)
        {
               	if(appendES(e, COLOUR[1]) == STATUS_ERROR)
		{
			jError("appendES");
			return STATUS_ERROR;
		}
               	//check = num of chars to print
               	int ind = 0;
               	while(ind < check)
               	{
               		if(j+ind >= start && j+ind<end)
                        {
                        	if(printInverted(str[j+ind], start, end, j+ind+1, jProper, str,e) == STATUS_ERROR)
				{
					jError("printInverted");
					return STATUS_ERROR;
				}
                        }
			(*skip)++;
                        ind++;
                }
        	if(appendES(e, COLOUR[0]) == STATUS_ERROR)
                {
                        jError("appendES");
                        return STATUS_ERROR;
                }
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int jProper : track absolute postion on line
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int j : index of element in str
                int *skip : records if element has be printed
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights c keywords

        Returns: int
                -1 : fail
                1 : success
*/
int printKeywords(int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
	for(int l =0; l<COLOUR_WORDS_NUM; l++)
        {//for all keywords
        	if(subString(str, j, COLOUR_WORDS[l]))//if keyword found
               	{
               		if(appendES(e, COLOUR[COLOUR_WORDS_C[l]]) == STATUS_ERROR)
                        {
				jError("appendES");
				return STATUS_ERROR;
			}

			int ind = 0;
                        while(ind < (int)strlen(COLOUR_WORDS[l]))
                       	{
                        	if(j+ind >= start && j+ind<end)
                               	{
                                	if(printInverted(COLOUR_WORDS[l][ind], start, end, j+ind+1, jProper, str,e) == STATUS_ERROR)
                        		{
                                		jError("printInverted");
                                		return STATUS_ERROR;
                        		}

                               	}
                                ind++;
                                (*skip)++;
                        }

                        if(appendES(e, COLOUR[0]) == STATUS_ERROR)
                        {
                                jError("appendES");
                                return STATUS_ERROR;
                        }

                        break;
        	}
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		bool *mLComment2 : tracks if currently in (/oo/) multi line comment
                int jProper : track absolute postion on line
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int j : index of element in str
                int *skip : records if element has be printed
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights start of (\oo\) multi line comment

        Returns: int
                -1 : fail
                1 : success
*/
int printMultiStart(bool *mLComment2, int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
	(*mLComment2) = true;
        if(appendES(e, COLOUR[4]) == STATUS_ERROR)
        {
        	jError("appendES");
                return STATUS_ERROR;
        }

	//always print colour, but only print chars if on screen
        if(j >= start && j<end)
        {
        	if(printInverted(str[j], start, end, j+1, jProper, str, e) == STATUS_ERROR)
                {
                        jError("printInverted");
                        return STATUS_ERROR;
                }
        }

      	if(j+1 >= start && j+1<end)
	{
       		if(printInverted(str[j+1], start, end, j+2, jProper, str, e) == STATUS_ERROR)
                {
                        jError("printInverted");
                        return STATUS_ERROR;
                }
	}

       	(*skip) += 2;
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		bool *mLComment2 : tracks if currently in (/oo/) multi line comment
                int jProper : track absolute postion on line
                int start : index of first element of current line that will be printed
                int end : index of last element of line that will be printed
                int j : index of element in str
                int *skip : records if element has be printed
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights end of (/oo/) multi line comment on screen

        Returns: int
                -1 : fail
                1 : success
*/
int printMultiEnd(bool *mLComment2, int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
	(*mLComment2) = false;
        if(j >= start && j<end)
	{
        	if(printInverted(str[j], start, end, j+1, jProper, str, e) == STATUS_ERROR)
        	{
                	jError("printInverted");
                	return STATUS_ERROR;
        	}
        }

        if(j+1 >= start && j+1<end)
	{
        	if(printInverted(str[j+1], start, end, j+2, jProper, str, e) == STATUS_ERROR)
        	{
                	jError("printInverted");
                	return STATUS_ERROR;
        	}
        }
        (*skip)+=2;

	if(appendES(e, COLOUR[0]) == STATUS_ERROR)
	{
		jError("appendES");
		return STATUS_ERROR;
	}
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		bool *stringC : traks if current part of line file is string
                int j : index of element in str
                char *str : current line
                struct expandString *e : holds screen so single io call can be used

        Does: highlights start of string on screen

        Returns: int
                -1 : fail
                1 : success
*/
int printStringStart(bool *stringC, int j, char *str, struct expandString *e)
{
	if(j==0)//not possible to be escape
        {
        	(*stringC) = true;
                if(appendES(e, COLOUR[3]) == STATUS_ERROR)
        	{
			jError("appendES");
			return STATUS_ERROR;
		}
	}
        else if(str[j-1] != '\\')//not escape
       	{
        	(*stringC) = true;
                if(appendES(e, COLOUR[3]) == STATUS_ERROR)
                {
                        jError("appendES");
                        return STATUS_ERROR;
                }
        }
	return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                int jProper : track absolute postion on line
		int start : index of first element of current line that will be printed
		int end : index of last element of line that will be printed
		int j : index of element in str
		int *skip : records if element has be printed
		char *str : current line
		struct expandString *e : holds screen so single io call can be used

        Does: highlights end of string on screen

        Returns: int
		-1 : fail
		1 : success
*/
int printStringEnd(int jProper, int start, int end, int j, int *skip, char *str, struct expandString *e)
{
        if(j >= start && j<end)
	{
        	if(printInverted(str[j], start, end, j+1, jProper, str, e) == STATUS_ERROR)
        	{
			jError("printInverted");
			return STATUS_ERROR;
		}
	}
        (*skip)++;
        if(appendES(e, COLOUR[0]) == STATUS_ERROR)
        {
        	jError("appendES");
               	return STATUS_ERROR;
        }
	return 1;
}

/*
	Written by: Jonathan Pincus

	Parameters:
		char ***lines : loaded portion of file
		struct winsize *w : screen size
		int *x : cursor position
		int *y : cursor position
		struct expandString *e : holds screen so single io call can be used
		struct stackItem **mLComment : tracks multi line comments above loaded portion of file (/o)
		size_t *fileLine : lines unloaded above loaded portion of file
		int pipe[2] : used for inter-process communication


	Does: prints single line of file

	Returns: int
		-1 : error
		0 : success
*/
int printMiddleLine(char ***lines, struct winsize *w, int *x, int *y, struct expandString *e, struct stackItem **mLComment, size_t *fileLine, int pipe[2])
{
	(void)clearES(e);
        bool mLC = MLC_Checker(*y, mLComment, *lines);//check if line is part of MLC

      	if(getMiddleLine((*lines)[*y], w, x, e, &mLC, true) == STATUS_ERROR)
	{
		jError("getMiddleLine");
                return STATUS_ERROR;
	}
        (void)moveCursor(1,3+(*y));
        (void)printf("\33[2K\r");
        (void)printf("%s", e->str); (void)clearES(e);

	//when screen is updated warning messages are removed
	if(PrintError(NULL, w, NULL, lines, x, y, fileLine, mLComment, e, pipe)  == STATUS_ERROR)
        {
                jError("printError");
                return STATUS_ERROR;
        }

	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char *line : current line of file
                struct winsize *w : screen size
                int *x : cursor position
                struct expandString *e : holds screen so single io call can be used
                struct stackItem **mLC : tracks multi line comments above loaded portion of file (/o)
                bool current : if line contains cursor

        Does: formats line for printing

        Returns: int
                -1 : error
                0 : success
*/
int getMiddleLine(char *line, struct winsize *w, int *x, struct expandString *e, bool *mLC, bool current)
{
	if( line[0] == '\0' )//emptyline
        {
                if(appendES(e, BLANK) == STATUS_ERROR)
                {
                	jError("appendES");
                        return STATUS_ERROR;
                }
		return 0;
	}

	int jProper=0;//index of cursor position
        int end = w->ws_col;
     	int start = 0;
	bool sLCommentC = false;
        bool stringC = false;
        int skip = 0;
	int tempX = (*x);

        if(current)//on current cursor line
        {//line where cursor is is only line where start may not be zero
        	start += currentLineStart(line, &tempX, *w);
                end = start + w->ws_col;
        }

	if(appendES(e, ((*mLC) ? COLOUR[4] : COLOUR[0])) == STATUS_ERROR)//in multi line comment
        {
		jError("appendES");
                return STATUS_ERROR;
        }

	for(int j = 0; j<(int)strlen(line); j++)
        {
        	if(line[j] == '"' && !sLCommentC  && !stringC && !(*mLC) && skip<1)//string start
                {
                	if(printStringStart(&stringC, j, line, e) == STATUS_ERROR)
                        {
                        	jError("printStringStart");
                                return STATUS_ERROR;
                        }
                }
                else if(line[j] == '"' && line[j-1] != '\\' && stringC  && !(*mLC) && skip<1)//string end
                {
                	stringC = false;
                        if(printStringEnd(jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                        {
                         	jError("printStringEnd");
                                return STATUS_ERROR;
                        }
                }

                if(line[j] ==  '/' && line[j+1] == '/' && !stringC && skip<1)//single line comment
                {
                 	sLCommentC = true;
                        if(appendES(e, COLOUR[4])  == STATUS_ERROR)
                        {
                        	jError("appendES");
                                return STATUS_ERROR;
                        }
                }

            	if(line[j] ==  '/' && line[j+1] == '*' && !stringC && skip<1)//multi line comment start
                {
                 	if(printMultiStart(mLC, jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                      	{
                        	jError("printMultiStart");
                                return STATUS_ERROR;
                       	}
                }
                else if(line[j] ==  '*' && line[j+1] == '/' && !stringC && skip<1)//multi Line comment end
                {
                	if(printMultiEnd(mLC, jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                        {
                        	jError("printMultiEnd");
                                return STATUS_ERROR;
                       	}
        	}

                if(!stringC && !(*mLC) && !sLCommentC && skip<1)//keywords
                {
                	if(printKeywords(jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                     	{
                        	jError("printKeywords");
                                return STATUS_ERROR;
                        }
                }

                if(!stringC && !(*mLC) && !sLCommentC && skip<1 && SingleQuotes(line, j))//single quotes
                {
                	if(printSingleQuotes(jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                        {
                        	jError("printSingleQuotes");
                                return STATUS_ERROR;
                        }
		}

                if(!stringC && !(*mLC) && !sLCommentC && skip<1)//Caps
                {
                	if(printBold(jProper, start, end, j, &skip, line, e) == STATUS_ERROR)
                        {
                        	jError("printBold");
                                return STATUS_ERROR;
                        }
                }

                if(j >= start && j<end && skip<1)
               	{
                	if(printInverted(line[j], start, end, j+1, jProper, line, e) == STATUS_ERROR)
                        {
                        	jError("printInverted");
                                return STATUS_ERROR;
                        }
                }


                if(line[j] == '\t' && j >= start && j<end)//tab takes to nearest x%8 = 0
                {
                	int plus = nearestEightDiff(jProper) - 1;
                        jProper += plus;
                        end -= plus;//print less of line cause tab
                }
                if(j >= start && j<end)
                {
              		jProper++;
              	}

                if(skip>0)
               	{
                	skip--;
                }
	}
	if((int)strlen(line) > end)//line too long for page so natural '\n' will not print
        {
        	if(appendES(e, "\n") == STATUS_ERROR)
                {
                	jError("appendES");
                        return STATUS_ERROR;
                }
	}

	if(appendES(e, "\x1B[0m") == STATUS_ERROR)//reset colour
        {
                jError("appendES");
                return STATUS_ERROR;
        }
	return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                char ***lines : loaded portion of file
		struct winsize *w : size of screen
		int *x : position of cursor
		iny *y : position of cursor
		size_t *fileLine : number of unloaded lines ubove current segment of file
		struct stackItem **mlComment : track (/oo/) multi line comments above portion of file
		struct expandString *e : hold screen so can use a single print
		char *message : error message
		int *pipe : used to communicate between input and display programs
		int first : only prints portion of loaded file starting at this index

        Does: prints middle of screen (actual data, not header/footer)

        Returns: int
		-1 : fail
		1 : success
*/
int printMiddle(char ***lines, struct winsize *w, int *x, int *y, size_t *fileLine, struct stackItem **mLComment, struct expandString *e, char *message, int *pipe, int first)
{
	(void)clearES(e);//clear print buffer

	bool mLComment2 = MLC_Checker(first, mLComment, *lines);

        for(int i=first; i<(w->ws_row-EXCESS); i++)
        {
		bool current = false;
		if((*y) == i )
		{
			current = true;
		}

		if(getMiddleLine((*lines)[i], w, x, e, &mLComment2, current) == STATUS_ERROR)
		{
			jError("getMiddleLine");
			return STATUS_ERROR;
		}
        }

	int tempX = (*x);
	(void)currentLineStart((*lines)[*y], &tempX, *w);//translates x to actual position acounting for tabs

	//prints screen
	(void)printFL(*fileLine, *w);
	(void)clearScreenMiddle(first, w->ws_row - EXCESS);
	(void)moveCursor(1,3+(first));
	(void)printf("%s", e->str);
	(void)PrintError(message, w, NULL, lines, x, y, fileLine, mLComment, e, pipe);//void becuase not using to get input
	(void)moveCursor(tempX+1, (*y)+3);
	(void)fflush(stdout);

	(void)strcpy(message,"");//resets error message
	return 1;
}

/*
	Written by: Jonathan Pincus

        Parameters:
                struct expandString *e : old screen data

        Does: prints screen when screen contents has no posiblity of changing

        Returns: void
*/
void reprint(struct expandString *e)
{
	(void)moveCursor(1,3);
        (void)printf("%s", e->str);
	fflush(stdout);
}


/*
        Written by: Jonathan Pincus

        Parameters:
		char** lines : loaded portion of file
		struct winsize w : screen size

        Does: saves file before exiting program

        Returns: int
		-1 : error
		1 : success
*/
int saveFiles(char** lines, struct winsize w)
{
	if(strcmp(saveFileName, "") == 0)//do not save
	{
		if(duplicateFile(fPS, fPO) == STATUS_ERROR)//restore original file
                {
                	jError("duplicateFile");
                        return STATUS_ERROR;
                }
	}
	else if((strcmp(saveFileName, "\n") == 0))
	{//save as old name
		if(remakeFile(lines, w, fPO) == STATUS_ERROR)//remake to original
                {
                	jError("remakeFile");
                        return STATUS_ERROR;
                }
	}
	else//save in new name, with original name having origianl file
	{
		if(duplicateFile(fPS, fPO) == STATUS_ERROR)//save original
                {
                	jError("duplicateFile");
                        return STATUS_ERROR;
                }
                if(remakeFile(lines, w, saveFileName) == STATUS_ERROR)//save new
                {
                	jError("remakeFile");
                        return STATUS_ERROR;
                }
	}

	(void)deleteTempFiles();
        return 1;
}




/*
        Written by: Jonathan Pincus

        Parameters:
                int argc : number of command line arguements
		char *argv : array of command line arguements

        Does: creates child process for input, process input in main process

        Returns: int
                0 : program terminated on user's discretion
		-1 : error
*/
int main( int argc, char *argv[] )
{
	(void)enterAltScreen();
	if( argc != 2 )
	{
		jError("No file Passed in");
		return STATUS_ERROR;
	}
	int validDirStatus = validDir(argv[1]);
        if(validDirStatus == STATUS_ERROR)
        {
        	jError("validDirectory");
        	return STATUS_ERROR;
	}
        else if(validDirStatus == 0)
        {
		jError("Invalid file path");
        	return STATUS_ERROR;
	}

	if(tc_canon_off() == false)
	{
		jError("tc_canon_off");
		return STATUS_ERROR;
	}
        if(EchoOff() == false)
	{
		jError("EchoOff");
		return STATUS_ERROR;
	}

	int p[2];
	if(pipe(p) <= STATUS_ERROR)
	{
		jError("pipe");
		return STATUS_ERROR;
	}

	int pid;
     	if((pid = fork()) <= STATUS_ERROR)
	{
      		jError("fork");
		return STATUS_ERROR;
	}
    	else if ( pid == 0)
	{//child gets input
		if(inputProcessSIG_IGN() == false)
		{
			jError("inputProcessSig_IGN");
		}
		if(close(p[0]) == STATUS_ERROR)
                {
                        jError("close");
                        return STATUS_ERROR;
                }
		(void)inputs(p);//does not return only exits
	}
    	else
	{//parent displays
		if(mainProcessSIG() == false)
		{
			jError("mainProcessSig");
			kill(pid, SIGKILL);
			return STATUS_ERROR;
		}
		if(close(p[1]) == STATUS_ERROR)
		{
			jError("close");
                        kill(pid, SIGKILL);
			return STATUS_ERROR;
		}

		fPO = argv[1];
		struct winsize w;
		if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == STATUS_ERROR)//get screen size
        	{
                	jError("ioctl");
			kill(pid, SIGKILL);
                	return STATUS_ERROR;
        	}
		char **ptr;

		int processStatus = process(p, &w, &ptr);
		kill(pid, SIGKILL);

		if(processStatus == -3 || processStatus == -26)
		{
			jError("process");
			(void)deleteTempFiles();
		}
		else if(processStatus == 1)
		{
			saveFiles(ptr, w);
			(void)tc_canon_on();//status irrelavant program closing
        		(void)EchoOn();
			(void)exitAltScreen();
			return 0;
		}
		else if(processStatus == STATUS_ERROR)
		{
			(void)strcpy(saveFileName, "ErrorSaveFile");
			saveFiles(ptr, w);
		}
		else//-2
		{
			(void)strcpy(saveFileName, "");
			saveFiles(ptr, w);
		}
	}
	(void)exitAltScreen();
        (void)tc_canon_on();//status irrelavant program closing
        (void)EchoOn();

    return -1;
}
