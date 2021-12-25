#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

#include "debug.h"
#include "config.h"
#include "ib.h"
#include "client.h"
#include "sock.h"

int run_client()
{
    struct ibv_cq *cq   = ib_res.cq;
    char *buf_ptr       = ib_res.ib_buf;
    size_t buf_size     = ib_res.ib_buf_size;

    while((*buf_ptr) != 'q')
    {
        fprintf(stdout, "Send text:\n");
        check(fgets(buf_ptr, buf_size, stdin) != NULL, "client: fgets failed.");

        check (post_send(&ib_res, IBV_WR_RDMA_WRITE) == 0, "client: failed to write.");
        check (poll_completion(cq) == 0, "client: poll completion failed.");
        
        memset(ib_res.ib_buf, 0, ib_res.ib_buf_size);
        check (post_send(&ib_res, IBV_WR_RDMA_READ) == 0, "client: failed to read.");
        check (poll_completion(cq) == 0, "client: poll completion failed.");
        fprintf(stdout, "Read text: %s", buf_ptr);
    }

    return 0;
error:
    return -1;
}
