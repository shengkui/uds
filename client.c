/******************************************************************************
*
* FILENAME:
*     client.c
*
* DESCRIPTION:
*     The example of client using Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@outlook.com)
*     - Initial version 
*
******************************************************************************/
#include "uds.h"


int main(void)
{
    uds_client_t *clnt;

    clnt = client_init();
    if (clnt == NULL) {
        printf("client: init error\n");
        return -1;
    }

    /********************** Get version of server ***********************/
    {
        uds_request_t req;
        uds_response_version_t *ver;

        req.command = CMD_GET_VERSION;
        req.data_len = 0;

        ver = (uds_response_version_t *)client_send_request(clnt, &req);
        if (ver == NULL) {
            printf("client: send request error\n");
            client_close(clnt);
            return -2;
        }

        if (ver->common.status == STATUS_SUCCESS) {
            printf("Version: %d.%d\n", ver->major, ver->minor);
        } else {
            printf("client: CMD_GET_VERSION error(%d)\n", ver->common.status);
        }

        free(ver);
    }

    /********************** Get message from server ***********************/
    {
        uds_request_t req;
        uds_response_get_msg_t *res;

        req.command = CMD_GET_MSG;
        req.data_len = 0;

        res = (uds_response_get_msg_t *)client_send_request(clnt, &req);
        if (res == NULL) {
            printf("client: send request error\n");
            client_close(clnt);
            return -3;
        }

        if (res->common.status == STATUS_SUCCESS) {
            printf("Message: %s\n", res->data);
        } else {
            printf("client: CMD_GET_MSG error(%d)\n", res->common.status);
        }

        free(res);
    }

    /********************** Put message to server ***********************/
    {
        uds_request_put_msg_t req;
        uds_response_put_msg_t *res;
        char str[] = "This is a message from client";

        req.common.command = CMD_PUT_MSG;
        req.common.data_len = strlen(str)+1;
        snprintf((char *)req.data, UDS_PUT_MSG_SIZE-1, "%s", str);
        req.data[UDS_PUT_MSG_SIZE-1] = 0;

        res = (uds_response_put_msg_t *)client_send_request(clnt, (uds_request_t *)&req);
        if (res == NULL) {
            printf("client: send request error\n");
            client_close(clnt);
            return -3;
        }

        if (res->common.status == STATUS_SUCCESS) {
            printf("client: CMD_PUT_MSG OK\n");
        } else {
            printf("client: CMD_PUT_MSG error(%d)\n", res->common.status);
        }

        free(res);
    }

    /********************** Send an unkown request to server ***********************/
    {
        uds_request_t req;
        uds_response_t *res;

        req.command = 0xFFFF;
        req.data_len = 0;

        res = (uds_response_t *)client_send_request(clnt, &req);
        if (res == NULL) {
            printf("client: send request error\n");
            client_close(clnt);
            return -3;
        }

        printf("client: response status(%d)\n", res->status);

        free(res);
    }

    client_close(clnt);
    return 0;
}

