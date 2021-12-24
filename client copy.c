#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

#include "debug.h"
#include "config.h"
#include "setup_ib.h"
#include "ib.h"
#include "client.h"

int run_client()
{
    int num_concurr_msgs = config_info.num_concurr_msgs;
    int msg_size = config_info.msg_size;
    int num_wc = 20;
    bool stop = false;
    bool start_sending = false;

    struct ibv_qp *qp = ib_res.qp;
    struct ibv_cq *cq = ib_res.cq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res.mr->lkey;
    char *buf_ptr = ib_res.ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res.ib_buf_size;

    /* pre-post recvs */
    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc != NULL, "client: failed to allocate wc");

    int i, n, ret;
    for (i = 0; i < num_concurr_msgs; i++)
    {
        ret = post_recv(msg_size, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
        check(ret == 0, "client: failed to post recv");
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
    }

    /* wait for start signal */
    while (start_sending != true)
    {
        do
        {
            n = ibv_poll_cq(cq, num_wc, wc);
        } while (n < 1);
        check(n > 0, "client: failed to poll cq");

        for (i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                check(0, "client: wc failed status: %s.",
                      ibv_wc_status_str(wc[i].status));
            }
            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* post a receive */
                post_recv(msg_size, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
                buf_offset = (buf_offset + msg_size) % buf_size;
                buf_ptr += buf_offset;

                if (ntohl(wc[i].imm_data) == MSG_CTL_START)
                {
                    start_sending = true;
                    break;
                }
            }
        }
    }
    log("client: ready to send");

    /* pre-post sends */
    buf_ptr = ib_res.ib_buf;
    for (i = 0; i < num_concurr_msgs; i++)
    {
        ret = post_send(msg_size, lkey, 0, MSG_REGULAR, qp, buf_ptr);
        check(ret == 0, "client: failed to post send");
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
    }

    while (stop != true)
    {
        /* poll cq */
        n = ibv_poll_cq(cq, num_wc, wc);
        if (n < 0)
        {
            check(0, "client: Failed to poll cq");
        }

        for (i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                if (wc[i].opcode == IBV_WC_SEND)
                {
                    check(0, "client: send failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
                else
                {
                    check(0, "client: recv failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
            }

            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* echo the message back */
                char *msg_ptr = (char *)wc[i].wr_id;
                post_send(msg_size, lkey, 0, MSG_REGULAR, qp, msg_ptr);

                /* post a new receive */
                post_recv(msg_size, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
                buf_offset = (buf_offset + msg_size) % buf_size;
                buf_ptr += buf_offset;
            }
        } /* loop through all wc */
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
