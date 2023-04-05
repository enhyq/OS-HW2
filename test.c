#include <string.h>
#include <stdio.h>
#include <stdlib.h>
char s[1000];
int main(void) {
    if (gets(s) != NULL) {
        if(strchr(s, 'x') && strchr(s, 'z')) {
            fprintf(stderr, "CRASH!\n");
            exit(2);
        }
        else {
            printf(":D\n");
        }
        
    }

    return 0;
}