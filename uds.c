/******************************************************************************
*
* FILENAME:
*     uds.c
*
* DESCRIPTION:
*     Some API for Unix domain socket.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (shengkui.leng@gmail.com)
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
 *      Compute 16-bit One's Complement sum of data.
 *      NOTES: Before call this function, please set the checksum field to 0.
 *
 * PARAMETERS:
 *      data - The data to compute checksum
 *      len  - The length of data.
 *
 * RETURN:
 *      Checksum
 ******************************************************************************/
static uint16_t compute_checksum(void *data, size_t len)
{
    uint16_t *word;
    uint8_t *byte;
    size_t i;
    uint16_t chksum = 0;

    if (data == NULL) {
        return 0;
    }

    word = data;
    for (i = 0; i < len/2; i++) {
        chksum += word[i];
    }

    if (i & 1) {
        byte = data;
        chksum += byte[len-1];
    }
    return (~chksum);
}


/******************************************************************************
 * NAME:
 *      check_request_packet
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
static int check_request_packet(void *buf, size_t len)
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

    if (compute_checksum(buf, len)) {
        printf("invalid checksum of request data\n");
        return 0;
    }

    return 1;
}


/******************************************************************************
 * NAME:
 *      check_response_packet
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
static int check_response_packet(void *buf, size_t len)
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

    if (compute_checksum(buf, len)) {
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
 *      A thread routine function to receive the request from client,
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
    size_t len, reqlen, resplen;

    if (sc == NULL) {
        pthread_exit(0);
    }

    while (1) {
        /* Receive request from client */
        reqlen = read(sc->client_fd, buf, UDS_BUF_SIZE);
        if (reqlen <= 0) {
            close(sc->client_fd);
            sc->inuse = 0;
            break;
        }

        /* If not all of the data of request have been received, read again! */
        if (!check_request_packet(buf, reqlen)) {
            continue;
        }

        /* Process the request */
        req = (uds_request_t *)buf;
        resp = sc->serv->request_handler(req);
        if (resp == NULL) {
            resp = (uds_response_t *)buf;
            resp->status = STATUS_ERROR;
            resp->data_len = 0;
        }

        resplen = sizeof(uds_response_t) + resp->data_len;
        resp->signature = req->signature;
        resp->checksum = 0;
        resp->checksum = compute_checksum(resp, resplen);

        /* Send response */
        len = write(sc->client_fd, resp, resplen);
        if (resp != (uds_response_t *)buf) {
            free(resp);
        }
        if (len != resplen) {
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
 *      req_handler - The function pointer of a user defined requeste handler.
 *
 * RETURN:
 *      A pointer of server info.
 ******************************************************************************/
uds_server_t *server_init(request_handler_t req_handler)
{
    uds_server_t *s;
    struct sockaddr_un addr;
    int i, ret;

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

    s->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s->sockfd < 0) {
        perror("socket error");
        free(s);
        return NULL;
    }

    ret = bind(s->sockfd, (struct sockaddr *) &addr, sizeof(addr));
    if (ret != 0) {
        perror("bind error");
        close(s->sockfd);
        free(s);
        return NULL;
    }

    ret = listen(s->sockfd, UDS_MAX_BACKLOG);
    if (ret != 0) {
        perror("listen error");
        close(s->sockfd);
        free(s);
        return NULL;
    }

    return s;
}


/******************************************************************************
 * NAME:
 *      server_accept
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
int server_accept(uds_server_t *s)
{
    s_connect_t *sc;
    int cl, i;

    if (s == NULL) {
        printf("invalid parameter!\n");
        return -1;
    }

    cl = accept(s->sockfd, NULL, NULL);
    if (cl < 0) {
        return cl;
    }

    /* Find a slot for the connection */
    for (i = 0; i < UDS_MAX_CLIENT; i++) {
        if (!s->conn[i].inuse) {
            break;
        }
    }

    if (i >= UDS_MAX_CLIENT) {
        printf("Too many connections\n");
        close(cl);
        return -1;
    }

    /* Start a new thread to handle the resuest */
    sc = &s->conn[i];
    sc->inuse = 1;
    sc->client_fd = cl;
    pthread_create(&sc->thread_id, NULL, request_handle_routine, sc);

    return cl;
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
 *      client_connect
 *
 * DESCRIPTION: 
 *      Connect to the server
 *
 * PARAMETERS:
 *      None
 *
 * RETURN:
 *      A pointer of client info.
 ******************************************************************************/
uds_client_t *client_connect()
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

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
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
    size_t len, reqlen;

    if ((sc == NULL) || (req == NULL)) {
        printf("invalid parameter!\n");
        return NULL;
    }

    /* Send request */
    reqlen = sizeof(uds_request_t) + req->data_len;
    req->signature = UDS_SIGNATURE;
    req->checksum = 0;
    req->checksum = compute_checksum(req, reqlen);
    len = write(sc->sockfd, req, reqlen);
    if (len != reqlen) {
        printf("Send request error\n");
        return NULL;
    }

    /* Get response */
    len = read(sc->sockfd, buf, UDS_BUF_SIZE);
    if (len <= 0) {
        return NULL;
    }

    if (check_response_packet(buf, len)) {
        resp = malloc(len);
        if (resp) {
            memcpy(resp, buf, len);
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

