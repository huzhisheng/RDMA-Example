#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

#include "debug.h"
#include "ib.h"
#include "setup_ib.h"
#include "config.h"
#include "server.h"

int run_server()
{
    // int num_wc = 20;
    // struct ibv_qp *qp = ib_res.qp;
    // struct ibv_cq *cq = ib_res.cq;
    // struct ibv_wc *wc = NULL;
    // uint32_t lkey = ib_res.mr->lkey;
    char *buf_ptr = ib_res.ib_buf;
    char *buf_end = ib_res.ib_buf + (ib_res.ib_buf_size - 1);   //指向缓冲区最后一个char, 避免printf字符串长度溢出
    // int ret, n;
    // bool stop = false;

    // /* pre-post recvs */
    // wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    // check(wc != NULL, "server: failed to allocate wc");

    // /* 给client发送一条START消息 */
    // ret = post_send(0, lkey, 0, MSG_CTL_START, qp, buf_ptr);
    // check(ret == 0, "server: failed to signal the client to start");

    // /* 等待START消息被接受 */
    // do
    // {
    //     n = ibv_poll_cq(cq, num_wc, wc);
    // } while (n < 1);
    // check(n > 0, "server: failed to signal the client to start");
    
    /* 一直循环显示服务器缓冲区, 直到开头字符为'q' */
    while((*buf_ptr) != 'q'){
        while((*buf_ptr) == 0){
        }
        *buf_end = '\0';
        fprintf(stdout, "%s\n", buf_ptr);
    }

    // free(wc);
    return 0;
error:
    // if (wc != NULL)
    // {
    //     free(wc);
    // }
    return -1;
}
