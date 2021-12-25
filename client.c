#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

#include "debug.h"
#include "config.h"
#include "setup_ib.h"
#include "ib.h"
#include "client.h"
#include "sock.h"

int run_client()
{
    int num_concurr_msgs = config_info.num_concurr_msgs;
    int msg_size = config_info.msg_size;
    int num_wc = 20;
    bool stop = false;
    bool start_sending = false;

    struct ibv_qp *qp   = ib_res.qp;
    struct ibv_cq *cq   = ib_res.cq;
    struct ibv_wc *wc   = NULL;
    uint32_t lkey       = ib_res.mr->lkey;
    uint32_t rkey       = ib_res.remote_qp_info.rkey;
    uint32_t raddr      = ib_res.remote_qp_info.addr;
    char *buf_ptr       = ib_res.ib_buf;
    int buf_offset      = 0;
    size_t buf_size     = ib_res.ib_buf_size;

    /* pre-post recvs */
    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc != NULL, "client: failed to allocate wc");

    

    int i, n, ret, ops_count = 0;
    // for (i = 0; i < num_concurr_msgs; i++)
    // {
        ret = post_recv(0, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
        
        check(ret == 0, "client: failed to post recv");
        check(poll_completion(cq) == 0, "client: poll completion failed.");
    //     buf_offset = (buf_offset + msg_size) % buf_size;
    //     buf_ptr += buf_offset;
    // }

    // /* wait for start signal */
    // while (start_sending != true)
    // {
    //     do
    //     {
    //         n = ibv_poll_cq(cq, num_wc, wc);
    //     } while (n < 1);
    //     check(n > 0, "client: failed to poll cq");

    //     for (i = 0; i < n; i++)
    //     {
    //         if (wc[i].status != IBV_WC_SUCCESS)
    //         {
    //             check(0, "client: wc failed status: %s.",
    //                   ibv_wc_status_str(wc[i].status));
    //         }
    //         if (wc[i].opcode == IBV_WC_RECV)
    //         {
    //             /* post a receive */
    //             post_recv(msg_size, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
    //             buf_offset = (buf_offset + msg_size) % buf_size;
    //             buf_ptr += buf_offset;

    //             if (ntohl(wc[i].imm_data) == MSG_CTL_START)
    //             {
    //                 start_sending = true;
    //                 break;
    //             }
    //         }
    //     }
    // }

    char temp_char;
    /* Sync so we are sure server side has data ready before client tries to read it */
    if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    {
        fprintf(stderr, "sync error before RDMA ops\n");
    }

    /* First we read contens of server's buffer */
    ret = post_send(0, lkey, 0, 3, qp, buf_ptr);
    check (ret == 0, "client: failed to read.");
    check (poll_completion(cq) == 0, "client: poll completion failed.");
    fprintf(stdout, "Contents of client's buffer: '%s'\n", ib_res.ib_buf);

    log("client: ready to send");

    // while((*buf_ptr) != 'q')
    // {
        // fprintf(stdout, "Send text:\n");
        // fgets(buf_ptr, buf_size, stdin);
        char temp_str[16] = "123";
        strcpy(buf_ptr, temp_str);

        // ops_count += 1;
        //if ((ops_count % SIG_INTERVAL) == 0) {
            ret = post_write_signaled (msg_size, lkey, 0, qp, buf_ptr, raddr, rkey);
        //} else {
            //ret = post_write_unsignaled (msg_size, lkey, 0, qp, buf_ptr, raddr, rkey);
        //}
        check (ret == 0, "client: failed to write.");
        fprintf(stdout, "Sent text: %s", buf_ptr);

        check (poll_completion(cq) == 0, "client: poll completion failed.");
    
    // char sock_buf[64] = {'\0'};
    // int peer_sockfd = ib_res.remote_socket;
    // /* sync with clients */
    // n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");

    // n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    // }

    //char temp_char;
    /* Sync so we are sure server side has data ready before client tries to read it */
    if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    {
        fprintf(stderr, "sync error before RDMA ops\n");
    }
    free(wc);
    return 0;
error:
    if (wc != NULL)
    {
        free(wc);
    }
    return -1;
}
