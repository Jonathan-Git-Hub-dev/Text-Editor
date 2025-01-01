#ifndef J_EXPAND_STRING_H
#define J_EXPAND_STRING_H

/*
        Written by: Jonathan Pincus

        Parameters: N/A

        Does: exanding array

        Retrurns: N/A
*/
struct expandString
{
        size_t max;
        size_t current;
        char *str;
};

//documentation in .c file for space reasons
bool makeES(struct expandString *e);
void clearES(struct expandString *e);
int appendES(struct expandString *e, char *str);

#endif
