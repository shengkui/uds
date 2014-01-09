/******************************************************************************
*
* FILENAME:
*     server.c
*
* DESCRIPTION:
*     The example of server using Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@gmail.com)
*     - Initial version 
*
******************************************************************************/
#include <unistd.h>
#include <signal.h>
#include "uds.h"

volatile sig_atomic_t loop_flag = 1;


/*
 * Return the version of server.
 */
uds_response_t *cmd_get_version(void)
{
    uds_response_version_t *ver;

    printf("CMD_GET_VERSION\n");

    ver = malloc(sizeof(uds_response_version_t));
    if (ver != NULL) {
        ver->resp.status = STATUS_SUCCESS;
        ver->resp.data_len = 2;
        ver->major = 1;
        ver->minor = 0;
    }

    return (uds_response_t *)ver;
}


/*
 * Get a message string from server
 */
uds_response_t *cmd_get_msg(void)
{
    uds_response_get_msg_t *res;
    char *str = "This is a message from the server.";

    printf("CMD_GET_MSG\n");

    res = malloc(sizeof(uds_response_get_msg_t));
    if (res != NULL) {
        res->resp.status = STATUS_SUCCESS;
        res->resp.data_len = strlen(str);
        snprintf(res->data, UDS_GET_MSG_SIZE, "%s", str);
        res->data[UDS_GET_MSG_SIZE-1] = 0;
    }

    return (uds_response_t *)res;
}


/*
 * Send a message string to server
 */
uds_response_t *cmd_put_msg(uds_request_t *req)
{
    uds_response_put_msg_t *res;

    printf("CMD_PUT_MSG\n");

    printf("Message: %s\n", (char *)req->data);

    res = malloc(sizeof(uds_response_put_msg_t));
    if (res != NULL) {
        res->resp.status = STATUS_SUCCESS;
        res->resp.data_len = 0;
    }

    return (uds_response_t *)res;
}


/*
 * Unknown request type
 */
uds_response_t *cmd_unknown(uds_request_t *req)
{
    uds_response_t *res;

    printf("Unknown requester type\n");

    res = malloc(sizeof(uds_response_t));
    if (res != NULL) {
        res->status = STATUS_INVALID_COMMAND;
        res->data_len = 0;
    }

    return res;
}


/*
 * The handler to handle all requests from client
 */
uds_response_t *my_request_handler(uds_request_t *req)
{
    uds_response_t *resp = NULL;

    switch (req->command) {
    case CMD_GET_VERSION:
        resp = cmd_get_version();
        break;

    case CMD_GET_MSG:
        resp = cmd_get_msg();
        break;
        
    case CMD_PUT_MSG:
        resp = cmd_put_msg(req);
        break;
        
    default:
        resp = cmd_unknown(req);
        break;
    }

    return resp;
}


/*
 * When user press CTRL+C, quit the server process.
 */
void handler_sigint(int sig)
{
    loop_flag = 0;
}

void install_sig_handler()
{
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    act.sa_handler = handler_sigint;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, 0);
}


int main(void)
{
    uds_server_t *s;

    s = server_init(&my_request_handler);
    if (s == NULL) {
        printf("server: init error\n");
        return -1;
    }

    install_sig_handler();

    while (loop_flag) {
        server_accept_request(s);
    }

    server_close(s);
    return 0;
}

