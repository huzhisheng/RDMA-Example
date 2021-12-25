#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>

#include "sock.h"
#include "ib.h"
#include "debug.h"
#include "config.h"

struct IBRes ib_res;

int connect_qp_server()
{
    int ret = 0;
    int sockfd = 0;
    int peer_sockfd = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);

    struct QPInfo local_qp_info;

    sockfd = sock_create_bind(config_info.sock_port);
    check(sockfd > 0, "Failed to create server socket.");
    listen(sockfd, 5);

    peer_sockfd = accept(sockfd, (struct sockaddr *)&peer_addr,
                         &peer_addr_len);
    check(peer_sockfd > 0, "Failed to create peer_sockfd");

    /* 查询RDMA设备上port的gid, 类似路由器端口的ip地址一样 */
    union ibv_gid my_gid;
    if (config_info.gid_idx >= 0)
    {
        ret = ibv_query_gid(ib_res.ctx, IB_PORT, config_info.gid_idx, &my_gid);
        if (ret)
        {
            fprintf(stderr, "could not get gid for port %d, index %d\n", IB_PORT, config_info.gid_idx);
            return ret;
        }
    }
    else
        memset(&my_gid, 0, sizeof(my_gid));

    local_qp_info.lid = ib_res.port_attr.lid;
    local_qp_info.qp_num = ib_res.qp->qp_num;
    local_qp_info.addr = (uintptr_t)ib_res.ib_buf;
    local_qp_info.rkey = ib_res.mr->rkey;

    memcpy(local_qp_info.gid, &my_gid, 16);

    /* get qp_info from client */
    ret = sock_get_qp_info(peer_sockfd, &ib_res.remote_qp_info);
    check(ret == 0, "Failed to get qp_info from client");

    /* send qp_info to client */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    check(ret == 0, "Failed to send qp_info to client");

    /* change send QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, &ib_res.remote_qp_info);
    check(ret == 0, "Failed to modify qp to rts");

    log(LOG_SUB_HEADER, "Start of IB Config");
    log("\tqp[%" PRIu32 "] <-> qp[%" PRIu32 "]",
        ib_res.qp->qp_num, ib_res.remote_qp_info.qp_num);
    log(LOG_SUB_HEADER, "End of IB Config");
    
    ib_res.remote_socket = peer_sockfd;

    char temp_char;
    /* 利用write & read socket的阻塞来进行同步 */
    if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    {
        fprintf(stderr, "sync error before RDMA ops\n");
    }

    close(peer_sockfd);
    close(sockfd);

    return 0;

error:
    if (peer_sockfd > 0)
    {
        close(peer_sockfd);
    }
    if (sockfd > 0)
    {
        close(sockfd);
    }

    return -1;
}

int connect_qp_client()
{
    int ret = 0;
    int peer_sockfd = 0;

    struct QPInfo local_qp_info;

    peer_sockfd = sock_create_connect(config_info.server_name,
                                      config_info.sock_port);
    check(peer_sockfd > 0, "Failed to create peer_sockfd");

    /* 查询RDMA设备上port的gid, 类似路由器端口的ip地址一样 */
    union ibv_gid my_gid;
    if (config_info.gid_idx >= 0)
    {
        ret = ibv_query_gid(ib_res.ctx, IB_PORT, config_info.gid_idx, &my_gid);
        if (ret)
        {
            fprintf(stderr, "could not get gid for port %d, index %d\n", IB_PORT, config_info.gid_idx);
            return ret;
        }
    }
    else
        memset(&my_gid, 0, sizeof(my_gid));

    local_qp_info.lid = ib_res.port_attr.lid;
    local_qp_info.qp_num = ib_res.qp->qp_num;
    local_qp_info.addr = (uintptr_t)ib_res.ib_buf;
    local_qp_info.rkey = ib_res.mr->rkey;
    memcpy(local_qp_info.gid, &my_gid, 16);

    /* send qp_info to server */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    check(ret == 0, "Failed to send qp_info to server");

    /* get qp_info from server */
    ret = sock_get_qp_info(peer_sockfd, &ib_res.remote_qp_info);
    check(ret == 0, "Failed to get qp_info from server");

    /* change QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, &ib_res.remote_qp_info);
    check(ret == 0, "Failed to modify qp to rts");

    log(LOG_SUB_HEADER, "IB Config");
    log("\tqp[%" PRIu32 "] <-> qp[%" PRIu32 "]",
        ib_res.qp->qp_num, ib_res.remote_qp_info.qp_num);
    log(LOG_SUB_HEADER, "End of IB Config");

    
    ib_res.remote_socket = peer_sockfd;

    char temp_char;
    /* 利用write & read socket的阻塞来进行同步 */
    if(sock_sync_data(ib_res.remote_socket, 1, "R", &temp_char))  /* just send a dummy char back and forth */
    {
        fprintf(stderr, "sync error before RDMA ops\n");
    }
    
    close(peer_sockfd);
    return 0;

error:
    if (peer_sockfd > 0)
    {
        close(peer_sockfd);
    }

    return -1;
}

int setup_ib()
{
    int ret = 0;
    struct ibv_device **dev_list = NULL;
    memset(&ib_res, 0, sizeof(struct IBRes));

    /* get IB device list */
    dev_list = ibv_get_device_list(NULL);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    ib_res.ctx = ibv_open_device(*dev_list);
    check(ib_res.ctx != NULL, "Failed to open ib device.");

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    check(ib_res.pd != NULL, "Failed to allocate protection domain.");

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    check(ret == 0, "Failed to query IB port information.");

    /* register mr */
    ib_res.ib_buf_size = config_info.msg_size * config_info.num_concurr_msgs;
    ib_res.ib_buf = (char *)memalign(4096, ib_res.ib_buf_size);
    check(ib_res.ib_buf != NULL, "Failed to allocate ib_buf");
    memset(ib_res.ib_buf, 0, ib_res.ib_buf_size); //初始将ib_buf清0

    ib_res.mr = ibv_reg_mr(ib_res.pd, (void *)ib_res.ib_buf,
                           ib_res.ib_buf_size,
                           IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_WRITE);
    check(ib_res.mr != NULL, "Failed to register mr");

    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    check(ret == 0, "Failed to query device");

    /* create cq */
    ib_res.cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe,
                              NULL, NULL, 0);
    check(ib_res.cq != NULL, "Failed to create cq");

    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ib_res.qp = ibv_create_qp(ib_res.pd, &qp_init_attr);
    check(ib_res.qp != NULL, "Failed to create qp");

    /* connect QP */
    if (config_info.is_server)
    {
        ret = connect_qp_server();
    }
    else
    {
        ret = connect_qp_client();
    }
    check(ret == 0, "Failed to connect qp");

    ibv_free_device_list(dev_list);
    return 0;

error:
    if (dev_list != NULL)
    {
        ibv_free_device_list(dev_list);
    }
    return -1;
}

void close_ib_connection()
{
    if (ib_res.qp != NULL)
    {
        ibv_destroy_qp(ib_res.qp);
    }

    if (ib_res.cq != NULL)
    {
        ibv_destroy_cq(ib_res.cq);
    }

    if (ib_res.mr != NULL)
    {
        ibv_dereg_mr(ib_res.mr);
    }

    if (ib_res.pd != NULL)
    {
        ibv_dealloc_pd(ib_res.pd);
    }

    if (ib_res.ctx != NULL)
    {
        ibv_close_device(ib_res.ctx);
    }

    if (ib_res.ib_buf != NULL)
    {
        free(ib_res.ib_buf);
    }
}
