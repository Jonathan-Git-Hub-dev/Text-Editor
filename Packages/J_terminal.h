#ifndef J_TERMINAL_H
#define J_TERMINAL_H

#define INVERT "\033[7m"
#define INVERT_UNDO "\033[0m"
#define DULL "\033[2m"

#define clearScreen() printf("\x1B[2J")

#define jError(str){\
	exitAltScreen();\
	printf("Error: %s, in %s\n", str, __func__);\
}

//print: Colours, Bold Colours, Background Colours
//Normal, Red, Green, Yellow, Blue, Magenta, Cyan, White
#define COLOUR (char[8][20]) {"\x1B[0m", "\x1B[1;31m", "\x1B[1;32m", "\x1B[1;33m", "\x1B[1;34m", "\x1B[1;35m", "\x1B[1;36m", "\x1B[1;37m"}
#define COLOUR_B (char[8][20]) {"\x1B[0m", "\x1B[0;31m", "\x1B[0;32m", "\x1B[0;33m", "\x1B[0;34m", "\x1B[0;35m", "\x1B[0;36m", "\x1B[0;37m"}
#define COLOUR_BG (char[8][20]) {"\x1B[40m", "\x1B[41m", "\x1B[42m", "\x1B[43m", "\x1B[44m", "\x1B[45m", "\x1B[46m", "\x1B[47m"}

#define LEN 1000

#define STATUS_ERROR -1
#define STATUS_NUTERAL 0

extern bool altScreen;
//documentation in .c file for space reasons
void moveCursor(int x, int y);
bool EchoOn();
bool EchoOff();
bool tc_canon_on();
bool tc_canon_off();
void enterAltScreen();
void exitAltScreen();


#endif
