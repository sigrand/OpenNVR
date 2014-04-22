#include <stdio.h>
#include <unistd.h>
#include "iostat.h"

int main()
{
    Run_iostat();

    int q=0;

    while(q<2)
    {
        usleep(20 * 1000000);

        stat_io * stat_io_ = 0;
        int stat_io_size = 0;

        Get_stat_io(& stat_io_, & stat_io_size);

        int i;

        for(i=0;i<stat_io_size;i++)
        {
            printf("========================================\n");
            printf("stat_io_[i].valid = %d\n", stat_io_[i].valid);
            printf("stat_io_[i].devname = %s\n", stat_io_[i].devname);
            printf("stat_io_[i].rrqm_s = %f\n", stat_io_[i].rrqm_s);
            printf("stat_io_[i].wrqm_s = %f\n", stat_io_[i].wrqm_s);
            printf("stat_io_[i].r_s = %f\n", stat_io_[i].r_s);
            printf("stat_io_[i].w_s = %f\n", stat_io_[i].w_s);
            printf("stat_io_[i].rkB_s = %f\n", stat_io_[i].rkB_s);
            printf("stat_io_[i].wkB_s = %f\n", stat_io_[i].wkB_s);
            printf("stat_io_[i].avgrq_sz = %f\n", stat_io_[i].avgrq_sz);
            printf("stat_io_[i].avgqu_sz = %f\n", stat_io_[i].avgqu_sz);
            printf("stat_io_[i].await = %f\n", stat_io_[i].await);
            printf("stat_io_[i].r_await = %f\n", stat_io_[i].r_await);
            printf("stat_io_[i].w_await = %f\n", stat_io_[i].w_await);
            printf("stat_io_[i].svctm = %f\n", stat_io_[i].svctm);
            printf("stat_io_[i].util = %f\n", stat_io_[i].util);
        }

        Free_stat_io(& stat_io_);

        q++;
    }

    Close_iostat();

    return 0;
}