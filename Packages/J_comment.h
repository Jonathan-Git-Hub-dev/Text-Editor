#ifndef J_COMMENT_H
#define J_COMMENT_H

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: holds the "index" of the start and end of a multiline comment

        Retrurns: N/A
*/
struct comment
{
        size_t a;
        size_t b;
};

//documentation in .c file for space reasons
void constructorS(struct comment **ptr, size_t a);
bool halfS(struct stackItem *end);
void addRestS(struct stackItem **end, size_t b);

#endif
