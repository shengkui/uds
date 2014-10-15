/******************************************************************************
*
* FILENAME:
*     uds.c
*
* DESCRIPTION:
*     Define some APIs for Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@outlook.com)
*     - Initial version 
*
******************************************************************************/
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "uds.h"


/******************************************************************************
 * NAME:
 *      compute_checksum
 *
 * DESCRIPTION: 
 *      Compute 16-bit One's Complement sum of data. (The algorithm comes from
 *      RFC-1071)
 *      NOTES: Before call this function, please set the checksum field to 0.
 *
 * PARAMETERS:
 *      buf  - The data buffer
 *      len  - The length of data(bytes).
 *
 * RETURN:
 *      Checksum
 ******************************************************************************/
static uint16_t compute_checksum(void *buf, ssize_t len)
{
    uint16_t *word;
    uint8_t *byte;
    ssize_t i;
    unsigned long sum = 0;

    if (!buf) {
        return 0;
    }

    word = buf;
    for (i = 0; i < len/2; i++) {
        sum += word[i];
    }

    /* If the length(bytes) of data buffer is an odd number, add the last byte. */
    if (len & 1) {
        byte = buf;
        sum += byte[len-1];
    }

    /* Take only 16 bits out of the sum and add up the carries */
    while (sum>>16) {
        sum = (sum>>16) + (sum&0xFFFF);
    }

    return (uint16_t)(~sum);
}


/******************************************************************************
 * NAME:
 *      validate_request_packet
 *
 * DESCRIPTION: 
 *      Verify the integrity of the request data.
 *
 * PARAMETERS:
 *      data - The data of request
 *      len  - The length of data.
 *
 * RETURN:
 *      1 - OK, 0 - FAIL
 ******************************************************************************/
static int validate_request_packet(void *buf, ssize_t len)
{
    uds_request_t *req;
    
    if (buf == NULL) {
        return 0;
    }
    req = buf;
    
    if (req->signature != UDS_SIGNATURE) {
        printf("invalid signature of request data\n");
        return 0;
    }

    if (req->data_len + sizeof(uds_request_t) != len) {
        printf("invalid length of request data\n");
        return 0;
    }

    if (compute_checksum(buf, len) != 0) {
        printf("invalid checksum of request data\n");
        return 0;
    }

    return 1;
}


/******************************************************************************
 * NAME:
 *      validate_response_packet
 *
 * DESCRIPTION: 
 *      Verify the integrity of the response data.
 *
 * PARAMETERS:
 *      data - The data of response
 *      len  - The length of data.
 *
 * RETURN:
 *      1 - OK, 0 - FAIL
 ******************************************************************************/
static int validate_response_packet(void *buf, ssize_t len)
{
    uds_response_t *resp;

    if (buf == NULL) {
        return 0;
    }
    resp = buf;
    
    if (resp->signature != UDS_SIGNATURE) {
        printf("invalid signature of response data\n");
        return 0;
    }

    if (resp->data_len + sizeof(uds_response_t) != len) {
        printf("invalid length of response data\n");
        return 0;
    }

    if (compute_checksum(buf, len) != 0) {
        printf("invalid checksum of response data\n");
        return 0;
    }


    return 1;
}


/******************************************************************************
 * NAME:
 *      request_handle_routine
 *
 * DESCRIPTION: 
 *      A thread function to receive the request from client,
 *      handle the request and send response back to client.
 *
 * PARAMETERS:
 *      arg - A pointer of connection info(client).
 *
 * RETURN:
 *      None
 ******************************************************************************/
static void *request_handle_routine(void *arg)
{
    s_connect_t *sc = arg;
    uds_request_t *req;
    uds_response_t *resp;
    uint8_t buf[UDS_BUF_SIZE];
    ssize_t bytes, req_len, resp_len;

    if (sc == NULL) {
        pthread_exit(0);
    }

    while (1) {
        /* Receive request from client */
        req_len = recv(sc->client_fd, buf, UDS_BUF_SIZE, 0);
        if (req_len <= 0) {
            close(sc->client_fd);
            sc->inuse = 0;
            break;
        }

        /* Check the integrity of the request packet */
        if (!validate_request_packet(buf, req_len)) {
            continue;
        }

        /* Process the request */
        req = (uds_request_t *)buf;
        resp = sc->serv->request_handler(req);
        if (resp == NULL) {
            resp = (uds_response_t *)buf;   /* Use a local buffer */
            resp->status = STATUS_ERROR;
            resp->data_len = 0;
        }

        resp_len = sizeof(uds_response_t) + resp->data_len;
        resp->signature = req->signature;
        resp->checksum = 0;
        resp->checksum = compute_checksum(resp, resp_len);

        /* Send response */
        bytes = send(sc->client_fd, resp, resp_len, MSG_NOSIGNAL);
        if (resp != (uds_response_t *)buf) {    /* If NOT local buffer, free it */
            free(resp);
        }
        if (bytes != resp_len) {
            close(sc->client_fd);
            sc->inuse = 0;
            break;
        }
    }

    pthread_exit(0);
}


/******************************************************************************
 * NAME:
 *      server_init
 *
 * DESCRIPTION: 
 *      Do some initialzation work for server.
 *
 * PARAMETERS:
 *      req_handler - The function pointer of a user-defined request handler.
 *
 * RETURN:
 *      A pointer of server info.
 ******************************************************************************/
uds_server_t *server_init(request_handler_t req_handler)
{
    uds_server_t *s;
    struct sockaddr_un addr;
    int i, rc;

    if (req_handler == NULL) {
        printf("invalid parameter!\n");
        return NULL;
    }

    s = malloc(sizeof(uds_server_t));
    if (s == NULL) {
        perror("malloc error");
        return NULL;
    }

    memset(s, 0, sizeof(uds_server_t));
    for (i = 0; i < UDS_MAX_CLIENT; i++) {
        s->conn[i].serv = s;
    }

    /* Setup request handler */
    s->request_handler = req_handler;

    unlink(UDS_SOCK_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, UDS_SOCK_PATH);

    s->sockfd = socket(AF_UNIX, UDS_SOCK_TYPE, 0);
    if (s->sockfd < 0) {
        perror("socket error");
        free(s);
        return NULL;
    }

    rc = bind(s->sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if (rc != 0) {
        perror("bind error");
        close(s->sockfd);
        free(s);
        return NULL;
    }

    rc = listen(s->sockfd, UDS_MAX_BACKLOG);
    if (rc != 0) {
        perror("listen error");
        close(s->sockfd);
        free(s);
        return NULL;
    }

    return s;
}


/******************************************************************************
 * NAME:
 *      server_accept_request
 *
 * DESCRIPTION: 
 *      Accept a request from client and start a new thread to process it.
 *
 * PARAMETERS:
 *      s - A pointer of server info.
 *
 * RETURN:
 *      0 - OK, Others - Error
 ******************************************************************************/
int server_accept_request(uds_server_t *s)
{
    s_connect_t *sc;
    int cl, i;

    if (s == NULL) {
        printf("invalid parameter!\n");
        return -1;
    }

    cl = accept(s->sockfd, NULL, NULL);
    if (cl < 0) {
        perror("accept error");
        return -1;
    }

    /* Find a slot for the connection */
    for (i = 0; i < UDS_MAX_CLIENT; i++) {
        if (!s->conn[i].inuse) {
            break;
        }
    }

    if (i >= UDS_MAX_CLIENT) {
        printf("too many connections\n");
        close(cl);
        return -1;
    }

    /* Start a new thread to handle the resuest */
    sc = &s->conn[i];
    sc->inuse = 1;
    sc->client_fd = cl;
    if (pthread_create(&sc->thread_id, NULL, request_handle_routine, sc) != 0) {
        perror("pthread_create error");
        close(cl);
        sc->inuse = 1;
        return -1;
    }

    return 0;
}


/******************************************************************************
 * NAME:
 *      server_close
 *
 * DESCRIPTION: 
 *      Close the socket fd and free memory.
 *
 * PARAMETERS:
 *      s - A pointer of server info.
 *
 * RETURN:
 *      None
 ******************************************************************************/
void server_close(uds_server_t *s)
{
    int i;

    if (s == NULL) {
        return;
    }

    for (i = 0; i < UDS_MAX_CLIENT; i++) {
        if (s->conn[i].inuse) {
            pthread_join(s->conn[i].thread_id, NULL);
            close(s->conn[i].client_fd);
        }
    }

    close(s->sockfd);
    free(s);
}


/******************************************************************************
 * NAME:
 *      client_init
 *
 * DESCRIPTION: 
 *      Init client and connect to the server
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      A pointer of client info.
 ******************************************************************************/
uds_client_t *client_init()
{
    uds_client_t *sc;
    struct sockaddr_un addr;
    int fd, rc;

    sc = malloc(sizeof(uds_client_t));
    if (sc == NULL) {
        perror("malloc error");
        return NULL;
    }
    memset(sc, 0, sizeof(uds_client_t));

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, UDS_SOCK_PATH);

    fd = socket(AF_UNIX, UDS_SOCK_TYPE, 0);
    if (fd < 0) {
        perror("socket error");
        free(sc);
        return NULL;
    }
    sc->sockfd = fd;

    rc = connect(sc->sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc != 0) {
        perror("connect error");
        close(sc->sockfd);
        free(sc);
        return NULL;
    }

    return sc;
}


/******************************************************************************
 * NAME:
 *      client_send_request
 *
 * DESCRIPTION: 
 *      Send a request to server, and get the response.
 *
 * PARAMETERS:
 *      sc  - A pointer of client info
 *      req - The request to send
 *
 * RETURN:
 *      The response of the request. The caller need to free the memory.
 ******************************************************************************/
uds_response_t *client_send_request(uds_client_t *sc, uds_request_t *req)
{
    uds_response_t *resp;
    uint8_t buf[UDS_BUF_SIZE];
    ssize_t bytes, req_len;

    if ((sc == NULL) || (req == NULL)) {
        printf("invalid parameter!\n");
        return NULL;
    }

    /* Send request */
    req_len = sizeof(uds_request_t) + req->data_len;
    req->signature = UDS_SIGNATURE;
    req->checksum = 0;
    req->checksum = compute_checksum(req, req_len);
    bytes = send(sc->sockfd, req, req_len, MSG_NOSIGNAL);
    if (bytes != req_len) {
        perror("send request error\n");
        return NULL;
    }

    /* Get response */
    bytes = recv(sc->sockfd, buf, UDS_BUF_SIZE, 0);
    if (bytes <= 0) {
        perror("receive response error\n");
        return NULL;
    }

    if (validate_response_packet(buf, bytes)) {
        resp = malloc(bytes);
        if (resp) {
            memcpy(resp, buf, bytes);
        } else {
            perror("malloc error");
        }
        return resp;
    }

    return NULL;
}


/******************************************************************************
 * NAME:
 *      client_close
 *
 * DESCRIPTION: 
 *      Close the client socket fd and free memory.
 *
 * PARAMETERS:
 *      sc - The pointer of client info.
 *
 * RETURN:
 *      None
 ******************************************************************************/
void client_close(uds_client_t *sc)
{
    if (sc == NULL) {
        return;
    }

    close(sc->sockfd);
    free(sc);
}

