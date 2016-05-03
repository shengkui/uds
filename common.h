/******************************************************************************
*
* FILENAME:
*     common.h
*
* DESCRIPTION:
*     Define some structures for client and server.
*
* REVISION(MM/DD/YYYY):
*     05/03/2016  Shengkui Leng (lengshengkui@outlook.com)
*     - Initial version 
*
******************************************************************************/
#ifndef _COMMON_H_
#define _COMMON_H_
#include "uds.h"


/*--------------------------------------------------------------
 * Definition for both client and server
 *--------------------------------------------------------------*/
#define UDS_SOCK_PATH           "/tmp/uds.1234"

/* Extra status code, refer STATUS_ERROR defined in uds.h */
#define STATUS_INIT_ERROR       (STATUS_ERROR+1)    /* Server/client init error */
#define STATUS_INVALID_COMMAND  (STATUS_ERROR+2)    /* Unkown request type */


/* Request type */
enum uds_command_type {
    CMD_GET_VERSION = 0x8001,   /* Get the version of server */
    CMD_GET_MESSAGE,            /* Receive a message from server */
    CMD_PUT_MESSAGE,            /* Send a message to server */

    CMD_UNKNOWN                 /* */
};


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


#endif /* _COMMON_H_ */
