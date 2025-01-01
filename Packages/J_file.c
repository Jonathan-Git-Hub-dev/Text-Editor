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
#include <fcntl.h>
#include <sys/stat.h>

#include "J_terminal.h"

/*
        Written by: Jonathan Pincus

        Parameters:
		FILE* fd : file to operate on
		int stop : final byte of new file

        Does: truncates and close a file

        Returns: int
		1 : success
		-1 : fail
*/
int TruncReOpen(FILE* fd, int stop)
{

        if(ftruncate(fileno(fd), stop) == -1)
        {
		jError("ftruncate");
                return STATUS_ERROR;
        }

        if(fclose(fd) != 0)
	{
		jError("fclose");
		return STATUS_ERROR;
	}

        return 1;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		const char* filePath : name of file

        Does: checks if file in '\n' terminated

        Returns: int
		0 : success
		-1: fail
*/
int prepFile(const char* filePath)
{
        FILE* fd = fopen(filePath, "r+");
	if(fd == NULL)
	{
		jError("fopen");
		return STATUS_ERROR;
	}

	if( lseek(fileno(fd), -1, SEEK_END) == -1)
        {
                jError("lseek");
		return STATUS_ERROR;
        }

        char c = fgetc(fd);

        if(c != '\n')
        {
                if(fprintf(fd, "\n") < 0)
                {
			jError("fprintf");
                        return STATUS_ERROR;
                }
       	}

	if(fclose(fd) != 0)
        {
                jError("fclose");
                return -1;
        }

        return 0;
}


/*
        Written by: Jonathan Pincus

        Parameters:
		char *str : final line of file to be used in calling function
		const char *filePathe : name of file

        Does: retrives final line of file if file not empty

        Returns: int
		0 : empty file
		1 : success
		-1 : fail
		-26 : text file busy
*/
int lastLine(char *str, const char* filePath)
{
        FILE *fd = fopen(filePath, "r+");
        if(fd == NULL)
        {
		//if file does not exist create it
		if(errno == 2)
		{
			fd = fopen(filePath, "w+");
        		if(fd == NULL)
        		{
				jError("fopen");
				return STATUS_ERROR;
			}
		}
		else if(errno == 26)
		{
			jError("Text File Busy");
			return -26;
		}
		else
		{//else error is fatal
			jError("fopen");
                	return STATUS_ERROR;
		}
        }
        if( lseek(fileno(fd), 0, SEEK_END) == -1)//move ofset to end
        {
		jError("lseek");
                return STATUS_ERROR;
        }

        int size = ftell(fd);//bytes in file(ftell may return error)
        if(size == -1)
	{
		jError("ftell");
		return STATUS_ERROR;
	}
	if(size == 0)//if empty file
        {
                return 0;
        }

        if(prepFile(filePath) == STATUS_ERROR)//make sure file is ended with '\n', this is the nessesary file format
        {
                jError("prepFile");
		return STATUS_ERROR;
        }

        //after prepFile() beacuse it potentially changes file size + fd position, redo
        if( lseek(fileno(fd), 0, SEEK_END) == -1)
        {
		jError("lseek");
                return STATUS_ERROR;
        }

        size = ftell(fd);//new byte size
	if(size == -1)
        {
                jError("ftell");
                return STATUS_ERROR;
        }

        if(size == 1)//last char always '\n'
        {
                (void)strcpy(str, "\n");
                return TruncReOpen(fd, 0);
        }

        char c;
        int i = 1;
        do
        {//move backward till start of file or end of second last line
                i++;
                if(fseek(fd, -i, SEEK_END) == -1)
                {
                        jError("fseek");
                        return STATUS_ERROR;
                }
                c = fgetc(fd);//may return error
                if(c=='\n')//hit start of previous line
                {

                        if(fgets(str, LEN, fd) == NULL)
			{
				jError("fgets");
				return STATUS_ERROR;
			}

			if(strcmp(str, "") == 0)
			{
				printf("this should never print\n\n");
				sleep(2);
				return STATUS_ERROR;
			}
			else if(str[strlen(str)-1] != '\n')//line too big
			{
				jError("fgets(file line too big)");
				return STATUS_ERROR;
			}

                        return TruncReOpen(fd, size-(i-1));
                }
                if(i==size)//current line is only line in file
                {
                        if(fseek(fd, 0, SEEK_SET) == -1)
                        {
                                jError("fseek");
                                return STATUS_ERROR;
                        }

                        if(fgets(str, LEN, fd) == NULL)
                        {
                                jError("fgets");
                                return STATUS_ERROR;
                        }

			if(str[strlen(str)] != '\0')//line too big
                        {
                                jError("fgets(file line too big)");
                                return STATUS_ERROR;
                        }
                        return TruncReOpen(fd, 0);
                }
        }while(i!=size);

	jError("undefined behaviour");//should never get here
        return STATUS_ERROR;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                const char* str : string to be printed
                const char* filePath : destination file

        Does: appends to file

        Returns:
                0 : success
                -1 : fail
*/
int append(const char* str, const char* filePath)
{
        FILE* fptr = fopen(filePath,"a");
        if(fptr == NULL)
        {
                jError("fopen");
                return -1;
        }

        if(fprintf(fptr, "%s", str) < 0)
        {
                jError("fprintf");
                return -1;
        }
        if(fclose(fptr) != 0)
        {
                jError("fclose");
                return -1;
        }
        return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
		const char* filePath: source file
		const char* filePath2: destination file

        Does: flips file line order (last line becomes first and so on)

        Returns: int
		1 : success
		-1 : fail
*/
int flipFile(const char* filePath, const char* filePath2)
{
        int test;
        char str[LEN]="";

        while(1)
        {
                test = lastLine(str,filePath);//get last line
                if(test == -1)
                {
                        jError("lastLine");
                        return STATUS_ERROR;
                }
		else if(test == -26)
		{
			jError("lastLine");
			return test;
		}

                if(test == 0)//file empty
                {
                        return 0;
                }

		if(append(str, filePath2) == STATUS_ERROR)
		{
			jError("append");
			return STATUS_ERROR;
		}
        }
        return 0;
}

/*
        Written by: Jonathan Pincus

        Parameters:
                const char* filePath1 : source path
                const char* filePath2 : destination file path

        Does: recreates soruce file in destination file

        Returns: int
                1 : success
                -1 : fail
*/
int duplicateFile(const char* filePath1, const char* filePath2)
{

	FILE *from = fopen(filePath1, "r");
    	if (from == NULL)
	{
		jError("fopen");
        	return STATUS_ERROR;
	}

    	FILE *to = fopen(filePath2, "w");
    	if (to == NULL)
	{
		jError("fopen");
		return STATUS_ERROR;
	}

	char line[LEN];

	while(fgets(line, LEN, from))
	{
		if(fprintf(to, "%s", line) < 0)
		{
			jError("fprintf");
			return STATUS_ERROR;
		}

	}

	if(fclose(from) != 0)
	{
		jError("fclose");
		return STATUS_ERROR;
	}
	if(fclose(to) != 0)
	{
		jError("fclose");
		return STATUS_ERROR;
	}

	return 1;
}


/*
        Written by: Jonathan Pincus

        Parameters:
                const char* filePath : full filepath for file

        Does: checks if file can be created in directory specified in path

        Returns: int
                1 : yes
                0 : no
		-1 : fail
*/
int validDir(const char* filePath)
{
	//if access file is ok then file exists and can be accesed
	int accessStatus = access(filePath, F_OK);
	if(accessStatus == 0)
	{
                return 1;
        }
	else if(accessStatus == -1 && errno != 2)
	{//2 is not error because 2 means file does not exist and that is fine
		jError("access");
		return STATUS_ERROR;
	}//file does not exist

	//check if file is createable
	FILE* fptr = fopen(filePath, "w");
	if(fptr == NULL && errno == 2)//directory error
	{
		return 0;
	}
	if(fptr == NULL)//other error
	{
		jError("fopen");
		return STATUS_ERROR;
	}
	else
	{
		if(fclose(fptr) != 0)
        	{
                	jError("fclose");
                	return STATUS_ERROR;
        	}

		if(remove(filePath) != 0)
		{
			jError("remove");
			return STATUS_ERROR;
		}
		return 1;
	}
	return 0;//never gets here
}

/*
        Written by: Jonathan Pincus

        Parameters:
                const char* filePath1 : path to a file
		const char* filePath2 : path to a file

        Does: checks if two file paths lead to same file

        Returns: int
                1 : yes
                0 : no
                -1 : fail
*/
int sameFile(const char* filePath1, const char* filePath2)
{
	struct stat s1, s2;
	if(stat(filePath1, &s1) == -1)
	{
		jError("stat")
		return STATUS_ERROR;
	}

    	if(stat(filePath2, &s2) == -1)
        {
                jError("stat")
                return STATUS_ERROR;
        }

	if(s1.st_ino == s2.st_ino)
	{
		return 1;
	}
	return 0;
}

/*void main()
{
        //Unit Testing

        //Testing duplicateFile() // flip file only fails if a file manipulation call is blocked
        char *fp1 = "TempFile1";
        char *fp2 = "TempFile2";

        //set up file to duplicate
        FILE *fptr = fopen(fp1, "w");
        fprintf(fptr, "Line1\nSecond Line\nThird Line");
        printf("Original file Contents:\n\n");
        printf("Line1\nSecond Line\nThird Line");
        fclose(fptr);

        duplicateFile(fp1, fp2);

        //print new file to check if it worked
        printf("\n\nNew file Contents:\n\n");
        FILE *from = fopen(fp2, "r");
        char line[LEN];
        while(fgets(line, LEN, from))
        {
                printf("%s", line);
        }
        printf("\n\n\n");

        //reset files
        remove(fp1);
        remove(fp2);


        //Testing ValidDir()
        //a valid directory is one that exists
        int vd = validDir("../hello");
        printf("\nFile path(../hello) ");
        if(vd == 0)
        {
                printf("is not valid\n");
        }
        else if(vd == 1)
        {
                printf("is valid\n");
        }

        vd = validDir("NewDir/hello");//dir does not exist
        printf("\nFile path(NewDir/hello) ");
        if(vd == 0)
        {
                printf("is not valid\n");
        }
        else if(vd == 1)
        {
                printf("is valid\n");
        }


        //Testing flipFile()
        //flipfile is created using the following functions, which this will also test:
        // prepFile(), LastLine(), TruncReOpen() and append()

        //flip file will return error if line size is larger than LEN

        fptr = fopen(fp1, "w");
        fprintf(fptr, "Line1\nSecond Line\nThird Line\nForth-Line\n(55555)Line(55555)");
        printf("\n\nOriginal file Contents:\n\n");
        printf("Line1\nSecond Line\nThird Line\nForth-Line\n(55555)Line(55555)");
        fclose(fptr);

        flipFile(fp1, fp2);

        //print new file to check if it worked
        printf("\n\nNew file Contents:\n\n");

        from = fopen(fp2, "r");

        while(fgets(line, LEN, from))
        {
                //printf("heloo\n");
                printf("%s", line);
        }
        fflush(stdout);

        //reset files
        remove(fp1);
        remove(fp2);


	//testing sameFile()
	char *fp3 = "J_file.h";
	char *fp4 = "../Packages/J_file.h";
	char *fp5 = "J_stack.c";
	printf("filepaths: (%s), (%s) are the same: %s\n", fp3, fp4, (sameFile(fp3, fp4) == 1 ? "True" : "False"));
	printf("filepaths: (%s), (%s) are the same: %s\n", fp3, fp5, (sameFile(fp3, fp5) == 1 ? "True" : "False"));
}*/
