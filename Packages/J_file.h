#ifndef J_FILE_H
#define J_FILE_H

//documentation in .c file for space reasons
//void adder();
int lastLine(char *str, const char* filePath);//trying to simplify
int flipFile(const char* filePath, const char* filePath2);
int prepFile(const char* filePath);
int append(const char* str,const char* filePath);
int duplicateFile(const char* filePath, const char* filePath2);
int validDir(const char* filePath);
int sameFile(const char* filePath1, const char* filepath2);
#endif
