#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>

#include "debug.h"
#include "ib.h"
#include "config.h"
#include "server.h"
#include "sock.h"

int run_server()
{
    char *buf_ptr = ib_res.ib_buf;
    
    /* 一直循环显示服务器缓冲区, 直到开头字符为'q' */
    char prev_char = (*buf_ptr);
    while((*buf_ptr) != 'q'){
        while((*buf_ptr) == prev_char){
            sleep(1);
        }
        fprintf(stdout, "%s", buf_ptr);
        prev_char = (*buf_ptr);
    }

    return 0;
}
