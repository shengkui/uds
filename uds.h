/******************************************************************************
*
* FILENAME:
*     uds.h
*
* DESCRIPTION:
*     Define some structure for Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@gmail.com)
*     - Initial version 
*
******************************************************************************/
#ifndef _UDS_H_
#define _UDS_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


/*--------------------------------------------------------------
 * Common Definition for both client and server
 *--------------------------------------------------------------*/

/* The pathname of unix domain socket */
#define UDS_SOCK_PATH           "/tmp/uds_sock.1234"

/* The socket type we used */
#define UDS_SOCK_TYPE           SOCK_STREAM
//#define UDS_SOCK_TYPE         SOCK_SEQPACKET


/* The read/write buffer size of unix domain socket */
#define UDS_BUF_SIZE            512

/* The signature of the request/response packet */
#define UDS_SIGNATURE           0xDEADBEEF


/* Request type */
enum request_type {
    CMD_GET_VERSION = 1,        /* Get the version of server */
    CMD_GET_MSG,                /* Receive a message from server */
    CMD_PUT_MSG                 /* Send a message to server */
};


/* Common structure of request */
#define UDS_REQ_DATA_SIZE       128
typedef struct uds_request {
    uint32_t signature;                 /* Signature, shall be UDS_SIGNATURE */
    uint32_t command;                   /* Refer enum request_type */
    uint16_t data_len;                  /* The length of request data */
    uint16_t checksum;                  /* The checksum of this structure */
    uint8_t data[UDS_REQ_DATA_SIZE];    /* request data */
} __attribute__ ((packed)) uds_request_t;


/* The status code */
enum uds_status_code {
    STATUS_SUCCESS = 0,         /* Success */
    STATUS_INVALID_COMMAND,     /* Unknown request type */
    STATUS_ERROR                /* Generic error */
};


/* Common header of response */
typedef struct uds_response {
    uint32_t signature;         /* Signature, shall be UDS_SIGNATURE */
    uint32_t status;            /* Refer enum status_code */
    uint16_t data_len;          /* The length of response data */
    uint16_t checksum;          /* The checksum of the response structure */
} __attribute__ ((packed)) uds_response_t;


/* Response for CMD_GET_VERSION */
typedef struct uds_response_version {
    uds_response_t resp;        /* Common header of response */
    uint8_t major;              /* Major version */
    uint8_t minor;              /* Minor version */
} __attribute__ ((packed)) uds_response_version_t;


/* Response for CMD_GET_MSG */
#define UDS_GET_MSG_SIZE        256
typedef struct uds_response_get_msg {
    uds_response_t resp;            /* Common header of response */
    char data[UDS_GET_MSG_SIZE];    /* Data get from server */
} __attribute__ ((packed)) uds_response_get_msg_t;


/* Response for CMD_PUT_MSG */
typedef struct uds_response_put_msg {
    uds_response_t resp;        /* Common header of response */
} __attribute__ ((packed)) uds_response_put_msg_t;



/*--------------------------------------------------------------
 * Definition for client only
 *--------------------------------------------------------------*/

/* Keep the information of client */
typedef struct uds_client {
    int sockfd;         /* Socket fd of the client */
} uds_client_t;


uds_client_t *client_init();
uds_response_t *client_send_request(uds_client_t *, uds_request_t *);
void client_close(uds_client_t *);



/*--------------------------------------------------------------
 * Definition for server only
 *--------------------------------------------------------------*/

/* The maximum length of the queue of pending connections */
#define UDS_MAX_BACKLOG     10

/* The maxium count of client connected */
#define UDS_MAX_CLIENT      10

/* Keep the information of connection */
typedef struct s_connect {
    int inuse;                  /* 1: the connection structure is in-use; 0: free */
    int client_fd;              /* Socket fd of the connection */
    pthread_t thread_id;        /* The id of request handle thread */
    struct uds_server *serv;    /* The pointer of uds_server structure */
} s_connect_t;


typedef uds_response_t * (*request_handler_t) (uds_request_t *);

/* Keep the information of server */
typedef struct uds_server {
    int sockfd;                         /* Socket fd of the server */
    s_connect_t conn[UDS_MAX_CLIENT];   /* Connections managed by server */
    request_handler_t request_handler;  /* function pointer of the request handle */
} uds_server_t;


uds_server_t *server_init(request_handler_t req_handler);
int server_accept_request(uds_server_t *s);
void server_close(uds_server_t *s);


#endif /* _UDS_H_ */

