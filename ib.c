#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include "ib.h"
#include "debug.h"
#include "config.h"

int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo *remote_qp_info)
{
	uint32_t target_qp_num 	= remote_qp_info->qp_num;
	uint16_t target_lid		= remote_qp_info->lid;
	uint8_t *dgid			= remote_qp_info->gid;
	int ret = 0;

	/* change QP state to INIT */
	{
		struct ibv_qp_attr qp_attr = {
			.qp_state = IBV_QPS_INIT,
			.pkey_index = 0,
			.port_num = IB_PORT,
			.qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
							   IBV_ACCESS_REMOTE_READ |
							   IBV_ACCESS_REMOTE_ATOMIC |
							   IBV_ACCESS_REMOTE_WRITE,
		};

		ret = ibv_modify_qp(qp, &qp_attr,
							IBV_QP_STATE | IBV_QP_PKEY_INDEX |
								IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
		check(ret == 0, "Failed to modify qp to INIT.");
	}

	/* Change QP state to RTR */
	{
		struct ibv_qp_attr qp_attr = {
			.qp_state = IBV_QPS_RTR,
			.path_mtu = IB_MTU,
			.dest_qp_num = target_qp_num,
			.rq_psn = 0,
			.max_dest_rd_atomic = 1,
			.min_rnr_timer = 0x12,
			.ah_attr.is_global = 0,
			.ah_attr.dlid = target_lid,
			.ah_attr.sl = IB_SL,
			.ah_attr.src_path_bits = 0,
			.ah_attr.port_num = IB_PORT,
		};

		if (target_lid == 0)
		{
			qp_attr.ah_attr.is_global = 1;
			qp_attr.ah_attr.port_num = IB_PORT; /* Must */
			qp_attr.ah_attr.grh.sgid_index = config_info.gid_idx;
			memcpy(&qp_attr.ah_attr.grh.dgid, dgid, 16);
			qp_attr.ah_attr.grh.hop_limit = 0xFF;
			qp_attr.ah_attr.grh.flow_label = 0;
			qp_attr.ah_attr.grh.traffic_class = 0;
		}

		ret = ibv_modify_qp(qp, &qp_attr,
							IBV_QP_STATE | IBV_QP_AV |
								IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
								IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
								IBV_QP_MIN_RNR_TIMER);
		check(ret == 0, "Failed to change qp to rtr.");
	}

	/* Change QP state to RTS */
	{
		struct ibv_qp_attr qp_attr = {
			.qp_state = IBV_QPS_RTS,
			.timeout = 14,
			.retry_cnt = 7,
			.rnr_retry = 7,
			.sq_psn = 0,
			.max_rd_atomic = 1,
		};

		ret = ibv_modify_qp(qp, &qp_attr,
							IBV_QP_STATE | IBV_QP_TIMEOUT |
								IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
								IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
		check(ret == 0, "Failed to modify qp to RTS.");
	}

	return 0;
error:
	return -1;
}

int poll_completion(struct ibv_cq *cq)
{
	struct ibv_wc wc;
    unsigned long start_time_msec;
    unsigned long cur_time_msec;
    struct timeval cur_time;
    int poll_result;
    int rc = 0;
    /* poll the completion for a while before giving up of doing it .. */
    gettimeofday(&cur_time, NULL);
    start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    do
    {
        poll_result = ibv_poll_cq(cq, 1, &wc);
        gettimeofday(&cur_time, NULL);
        cur_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    }
    while((poll_result == 0) && ((cur_time_msec - start_time_msec) < MAX_POLL_CQ_TIMEOUT));
 
    if(poll_result < 0)
    {
        /* poll CQ failed */
        fprintf(stderr, "poll CQ failed\n");
        rc = 1;
    }
    else if(poll_result == 0)
    {
        /* the CQ is empty */
        fprintf(stderr, "completion wasn't found in the CQ after timeout\n");
        rc = 1;
    }
    else
    {
        /* CQE found */
        fprintf(stdout, "completion was found in CQ with status 0x%x\n", wc.status);
        /* check the completion status (here we don't care about the completion opcode */
        if(wc.status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", 
					wc.status, wc.vendor_err);
            rc = 1;
        }
    }
    return rc;
}

/******************************************************************************
* Function: post_send
*
* Input:
* res: pointer to resources structure
* opcode: IBV_WR_SEND, IBV_WR_RDMA_READ or IBV_WR_RDMA_WRITE
*
* Output: none
*
* Returns: 0 on success, error code on failure
*
* Description: This function will create and post a send work request
******************************************************************************/
int post_send(struct IBRes *ib_res, int opcode)
{
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc = 0;

    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_buf;
    sge.length = ib_res->ib_buf_size;
    sge.lkey = ib_res->mr->lkey;

    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = opcode;
    sr.send_flags = IBV_SEND_SIGNALED;
    if(opcode != IBV_WR_SEND)
    {
        sr.wr.rdma.remote_addr = ib_res->remote_qp_info.addr;
        sr.wr.rdma.rkey = ib_res->remote_qp_info.rkey;
    }

    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(ib_res->qp, &sr, &bad_wr);
    if(rc)
    {
        fprintf(stderr, "failed to post SR\n");
    }
    else
    {
        switch(opcode)
        {
        case IBV_WR_SEND:
            fprintf(stdout, "Send Request was posted\n");
            break;
        case IBV_WR_RDMA_READ:
            fprintf(stdout, "RDMA Read Request was posted\n");
            break;
        case IBV_WR_RDMA_WRITE:
            fprintf(stdout, "RDMA Write Request was posted\n");
            break;
        default:
            fprintf(stdout, "Unknown Request was posted\n");
            break;
        }
    }
    return rc;
}

/******************************************************************************
* Function: post_receive
*
* Input:
* res: pointer to resources structure
*
* Output: none
*
* Returns: 0 on success, error code on failure
*
* Description: post RR to be prepared for incoming messages
*
******************************************************************************/
int post_receive(struct IBRes *ib_res)
{
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc = 0;

    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_buf;
    sge.length = ib_res->ib_buf_size;
    sge.lkey = ib_res->mr->lkey;

    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;

    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(ib_res->qp, &rr, &bad_wr);
    if(rc)
    {
        fprintf(stderr, "failed to post RR\n");
    }
    else
    {
        fprintf(stdout, "Receive Request was posted\n");
    }
    return rc;
}