#include "userlib.h"

int main(int argc, char* argv[])
{
    printf("\nchild arguments: ");
    for(int i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
    return 0;
}
