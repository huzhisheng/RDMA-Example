#ifndef IB_H_
#define IB_H_

#include <inttypes.h>
#include <sys/types.h>
#include <endian.h>
#include <byteswap.h>
#include <infiniband/verbs.h>
#include <arpa/inet.h>

extern struct IBRes ib_res;

#define IB_MTU			IBV_MTU_4096
#define IB_PORT			1
#define IB_SL			0
#define IB_WR_ID_STOP		0xE000000000000000
#define NUM_WARMING_UP_OPS      500000
#define TOT_NUM_OPS             5000000
#define SIG_INTERVAL            1000
#define MAX_POLL_CQ_TIMEOUT     20000

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll (uint64_t x) {return bswap_64(x); }
static inline uint64_t ntohll (uint64_t x) {return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll (uint64_t x) {return x; }
static inline uint64_t ntohll (uint64_t x) {return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

struct QPInfo {
    // uint16_t lid;
    // uint32_t qp_num;

    uint64_t    addr;       /* Buffer address */
    uint32_t    rkey;       /* Remote key */
    uint32_t    qp_num;     /* QP number */
    uint16_t    lid;        /* LID of the IB port */
    uint8_t     gid[16];    /* gid */

}__attribute__ ((packed));

enum MsgType {
    MSG_CTL_START = 0,
    MSG_CTL_STOP,
    MSG_REGULAR,
};

struct IBRes {
    struct ibv_context		*ctx;
    struct ibv_pd		    *pd;
    struct ibv_mr		    *mr;
    struct ibv_cq		    *cq;
    struct ibv_qp		    *qp;
    struct ibv_port_attr	port_attr;
    struct ibv_device_attr	dev_attr;
    struct QPInfo	        remote_qp_info;
    int                     remote_socket;

    char   *ib_buf;
    size_t  ib_buf_size;
};

int modify_qp_to_rts (struct ibv_qp *qp, struct QPInfo *remote_qp_info);

// int post_send (uint32_t req_size, uint32_t lkey, uint64_t wr_id, 
// 	       uint32_t imm_data, struct ibv_qp *qp, char *buf);

// int post_recv (uint32_t req_size, uint32_t lkey, uint64_t wr_id, 
// 	       struct ibv_qp *qp, char *buf);

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
						struct ibv_qp *qp, char *buf,
						uint64_t raddr, uint32_t rkey);

// int post_write_unsignaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
// 						  struct ibv_qp *qp, char *buf,
// 						  uint64_t raddr, uint32_t rkey);

int poll_completion(struct ibv_cq *cq);

// int post_read(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
// 						struct ibv_qp *qp, char *buf,
// 						uint64_t raddr, uint32_t rkey);

int post_send(struct IBRes *ib_res, int opcode);

int post_receive(struct IBRes *ib_res);

int  setup_ib ();
void close_ib_connection ();

int  connect_qp_server ();
int  connect_qp_client ();

#endif /*ib.h*/
