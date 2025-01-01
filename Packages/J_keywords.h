#ifndef J_KEYWORDS_H
#define J_KEYWORDS_H

#include "J_terminal.h"

#define COLOUR_WORDS_NUM 40
#define COLOUR_WORDS_BUF 9

const char COLOUR_WORDS[COLOUR_WORDS_NUM][COLOUR_WORDS_BUF] = {
"auto",//the 32 C key words
"break",
"case",
"char",
"const",
"continue",
"default",
"do",
"double",
"else",
"enum",
"extern",
"float",
"for",
"goto",
"if",
"int",
"long",
"register",
"return",
"short",
"signed",
"sizeof",
"static",
"struct",
"switch",
"typedef",
"union",
"unsigned",
"void",
"volatile",
"while",
"#define",//extras
"#include",
"bool",
"'\\n'",//anomoly single quotes ie: "\n"
"'\\\\'",
"'\\\''",
"'\\b'",
"'\\0'"};

#define COLOUR_WORDS_C (int[COLOUR_WORDS_NUM]) {2,5,3,2,2,5,3,3,2,3,2,2,2,3,5,3,2,2,2,5,2,2,2,2,2,3,2,2,2,2,2,3,5,5,2,5,5,5,5,5}
#endif

