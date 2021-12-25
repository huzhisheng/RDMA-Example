#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>

#include "debug.h"
#include "ib.h"
#include "setup_ib.h"
#include "config.h"
#include "server.h"
#include "sock.h"

int run_server()
{
    // int num_wc = 20;
    struct ibv_qp *qp = ib_res.qp;
    struct ibv_cq *cq = ib_res.cq;
    // struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res.mr->lkey;
    char *buf_ptr = ib_res.ib_buf;
    char *buf_end = ib_res.ib_buf + (ib_res.ib_buf_size - 1);   //指向缓冲区最后一个char, 避免printf字符串长度溢出
    int ret, n;
    // bool stop = false;

    // /* pre-post recvs */
    // wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    // check(wc != NULL, "server: failed to allocate wc");

    

    // /* 给client发送一条START消息 */
    // ret = post_send(0, lkey, 0, MSG_CTL_START, qp, buf_ptr);
    // check(post_send(&ib_res, IBV_WR_SEND) == 0, "server: failed to signal the client to start");
    // check(poll_completion(cq) == 0, "server: poll completion failed.");
    
    // char temp_char;
    // /* Sync so we are sure server side has data ready before client tries to read it */
    // if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    // {
    //     fprintf(stderr, "sync error before RDMA ops\n");
    // }

    //ret = post_recv(0, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
        
    // check(post_receive(&ib_res) == 0, "client: failed to post recv");
    // check(poll_completion(cq) == 0, "client: poll completion failed.");
    // /* 等待START消息被接受 */
    // do
    // {
    //     n = ibv_poll_cq(cq, num_wc, wc);
    // } while (n < 1);
    // check(n > 0, "server: failed to signal the client to start");
    
    /* 一直循环显示服务器缓冲区, 直到开头字符为'q' */
    char prev_char = (*buf_ptr);
    while((*buf_ptr) != 'q'){
        while((*buf_ptr) == prev_char){
            sleep(1);
        }
        *buf_end = '\0';
        fprintf(stdout, "%s", buf_ptr);
        prev_char = (*buf_ptr);
    }
    
    //sleep(10);

    

    // char sock_buf[64] = {'\0'};
    // int peer_sockfd = ib_res.remote_socket;
    // /* sync with clients */
    // n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");

    // n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");

    // free(wc);

    //char temp_char;
    /* Sync so we are sure server side has data ready before client tries to read it */
    // if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    // {
    //     fprintf(stderr, "sync error before RDMA ops\n");
    // }

    return 0;
error:
    // if (wc != NULL)
    // {
    //     free(wc);
    // }
    return -1;
}
