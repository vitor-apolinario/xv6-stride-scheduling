#include "types.h"
#include "stat.h"
#include "user.h"

void
ocupacpu(void)
{
    int i, pid;
    for(i = 1; i < 10; i++){
        pid = fork((i*10));
        if(pid == 0)
            for(;;);
    }            
    while(1){
        pid = wait();
    }
}

int
main(){
    ocupacpu();
    exit();
}