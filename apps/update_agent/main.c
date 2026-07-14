#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile int running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

int main()
{
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    while(running)
    {
        FILE* fp = fopen("/proc/update","r");
        if(!fp) 
        {
            perror("fopen");
            return 1;
        }
        double uptime = 0;
        fscanf(fp,"%lf",&uptime);
        fclose(fp);
    
        sleep(1);
    }
    return 0;

}
