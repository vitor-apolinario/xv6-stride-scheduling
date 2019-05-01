#include "types.h"
#include "stat.h"
#include "user.h"

void
ocupacpu(void)
{
    int i, pid;

    pid = fork(10);
    pid = fork(20);
    pid = fork(5);
    pid = fork(25);
    pid = fork(40);

    if(!pid)
        for(i=0; i < 50; i++)
            //printf(1, "%d\n", i);
    exit();
}

int
main()
{
    ocupacpu();
    exit();
}