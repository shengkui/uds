/******************************************************************************
*
* FILENAME:
*     uds.h
*
* DESCRIPTION:
*     Define some structure for Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@outlook.com)
*     - Initial version 
*     12/11/2014  Shengkui Leng (lengshengkui@outlook.com)
*     - Define a common header for both request and response packets.
*
******************************************************************************/
#ifndef _UDS_H_
#define _UDS_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*--------------------------------------------------------------
 * Definition for both client and server
 *--------------------------------------------------------------*/

/* The pathname of unix domain socket */
#define UDS_SOCK_PATH           "/tmp/uds_sock.1234"

/* The socket type we used */
#define UDS_SOCK_TYPE           SOCK_STREAM
//#define UDS_SOCK_TYPE         SOCK_SEQPACKET


/* The read/write buffer size of socket */
#define UDS_BUF_SIZE            1024

/* The signature of the request/response packet */
#define UDS_SIGNATURE           0xDEADBEEF

/* Make a structure 1-byte aligned */
#define BYTE_ALIGNED            __attribute__((packed))


/* Request type */
enum uds_command_type {
    CMD_GET_VERSION = 0x8001,   /* Get the version of server */
    CMD_GET_MESSAGE,            /* Receive a message from server */
    CMD_PUT_MESSAGE             /* Send a message to server */
};

/* The status code */
enum uds_status_code {
    STATUS_SUCCESS = 0,         /* Success */
    STATUS_INVALID_COMMAND,     /* Unknown request type */
    STATUS_ERROR                /* Generic error */
};

/* Common header of both request/response packets */
typedef struct uds_command {
    uint32_t signature;         /* Signature, shall be UDS_SIGNATURE */
    union {
        uint32_t command;       /* Request type, refer uds_command_type */
        uint32_t status;        /* Status code of response, refer uds_status_code */
    };
    uint32_t data_len;          /* The data length of packet */

    uint16_t checksum;          /* The checksum of the packet */
} BYTE_ALIGNED uds_command_t;


/* Response for CMD_GET_VERSION */
typedef struct uds_response_version {
    uds_command_t common;       /* Common header of response */
    uint8_t major;              /* Major version */
    uint8_t minor;              /* Minor version */
} BYTE_ALIGNED uds_response_version_t;


/* Response for CMD_GET_MESSAGE */
#define UDS_GET_MSG_SIZE        256
typedef struct uds_response_get_msg {
    uds_command_t common;           /* Common header of response */
    char data[UDS_GET_MSG_SIZE];    /* Data from server to client */
} BYTE_ALIGNED uds_response_get_msg_t;


/* Request for CMD_PUT_MESSAGE */
#define UDS_PUT_MSG_SIZE       256
typedef struct uds_request_put_msg {
    uds_command_t common;           /* Common header of request */
    char data[UDS_PUT_MSG_SIZE];    /* Data from client to server */
} BYTE_ALIGNED uds_request_put_msg_t;


/*--------------------------------------------------------------
 * Definition for client only
 *--------------------------------------------------------------*/

/* Keep the information of client */
typedef struct uds_client {
    int sockfd;         /* Socket fd of the client */
} uds_client_t;


uds_client_t *client_init(const char *sock_path);
uds_command_t *client_send_request(uds_client_t *c, uds_command_t *req);
void client_close(uds_client_t *s);



/*--------------------------------------------------------------
 * Definition for server only
 *--------------------------------------------------------------*/

/* The maximum length of the queue of pending connections */
#define UDS_MAX_BACKLOG     10

/* The maxium count of client connected */
#define UDS_MAX_CLIENT      10

typedef uds_command_t * (*request_handler_t) (uds_command_t *);

/* Keep the information of connection */
typedef struct uds_connect {
    int inuse;                  /* 1: the connection structure is in-use; 0: free */
    int client_fd;              /* Socket fd of the connection */
    pthread_t thread_id;        /* The thread id of request handler */
    struct uds_server *serv;    /* The pointer of uds_server who own the connection */
} uds_connect_t;

/* Keep the information of server */
typedef struct uds_server {
    int sockfd;                         /* Socket fd of the server */
    uds_connect_t conn[UDS_MAX_CLIENT]; /* Connections managed by server */
    request_handler_t request_handler;  /* Function pointer of the request handle */
} uds_server_t;


uds_server_t *server_init(const char *sock_path, request_handler_t req_handler);
int server_accept_request(uds_server_t *s);
void server_close(uds_server_t *s);


#endif /* _UDS_H_ */
