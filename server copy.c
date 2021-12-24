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
    int num_concurr_msgs = config_info.num_concurr_msgs;
    int msg_size = config_info.msg_size;
    int num_wc = 20;
    bool stop = false;

    struct ibv_qp *qp = ib_res.qp;
    struct ibv_cq *cq = ib_res.cq;
    struct ibv_wc *wc = NULL;
    uint32_t lkey = ib_res.mr->lkey;
    char *buf_ptr = ib_res.ib_buf;
    int buf_offset = 0;
    size_t buf_size = ib_res.ib_buf_size;

    /* pre-post recvs */
    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc != NULL, "server: failed to allocate wc");

    int i, n, ret;
    for (i = 0; i < num_concurr_msgs; i++)
    {
        ret = post_recv(msg_size, lkey, (uint64_t)buf_ptr, qp, buf_ptr);
        check(ret == 0, "server: failed to post recv");
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
    }

    /* signal the client to start */
    ret = post_send(0, lkey, 0, MSG_CTL_START, qp, buf_ptr);
    check(ret == 0, "server: failed to signal the client to start");

    while (stop != true)
    {
        /* poll cq */
        n = ibv_poll_cq(cq, num_wc, wc);
        if (n < 0)
        {
            check(0, "server: Failed to poll cq");
        }

        for (i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                if (wc[i].opcode == IBV_WC_SEND)
                {
                    check(0, "server: send failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
                else
                {
                    check(0, "server: recv failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
            }

            if (wc[i].opcode == IBV_WC_RECV)
            {
                /* echo the message back */
                char *msg_ptr = (char *)wc[i].wr_id;
                post_send(msg_size, lkey, 0, MSG_REGULAR, qp, msg_ptr);

                /* post a new receive */
                post_recv(msg_size, lkey, wc[i].wr_id, qp, msg_ptr);
            }
        }
    }

    /* signal the client to stop */
    ret = post_send(0, lkey, IB_WR_ID_STOP, MSG_CTL_STOP, qp, ib_res.ib_buf);
    check(ret == 0, "server: failed to signal the client to stop");

    stop = false;
    while (stop != true)
    {
        /* poll cq */
        n = ibv_poll_cq(cq, num_wc, wc);
        if (n < 0)
        {
            check(0, "server: Failed to poll cq");
        }

        for (i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                if (wc[i].opcode == IBV_WC_SEND)
                {
                    check(0, "server: send failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
                else
                {
                    check(0, "server: recv failed status: %s",
                          ibv_wc_status_str(wc[i].status));
                }
            }

            if (wc[i].opcode == IBV_WC_SEND)
            {
                if (wc[i].wr_id == IB_WR_ID_STOP)
                {
                    stop = true;
                    break;
                }
            }
        }
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
