/******************************************************************************
*
* FILENAME:
*     uds.c
*
* DESCRIPTION:
*     Define some APIs for Unix domain socket communication.
*
* REVISION(MM/DD/YYYY):
*     12/02/2013  Shengkui Leng (lengshengkui@outlook.com)
*     - Initial version 
*     03/17/2014  Shengkui Leng (lengshengkui@outlook.com)
*     - Define a common header for both request and response packets.
*     - Receive data in a loop to make sure all data of a packet received.
*     05/12/2015  Shengkui Leng (lengshengkui@outlook.com)
*     - Add timeout to client(wating for server to be ready)
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
 *      recv_data
 *
 * DESCRIPTION: 
 *      Read data from socket fd.
 *
 * PARAMETERS:
 *      sockfd - The socket fd
 *      buf    - The buffer to keep data
 *      len    - The length of buffer
 *      flags  - Flags pass to recv() function
 *
 * RETURN:
 *      Bytes received.
 ******************************************************************************/
static ssize_t recv_data(int sockfd, char *buf, ssize_t len, int flags)
{
    ssize_t bytes;
    ssize_t pos;
    int count;
    fd_set readfds, writefds;
    struct timeval timeout;


    pos = 0;
    do {
        bytes = recv(sockfd, buf+pos, len-pos, flags);
        if (bytes < 0) {
            perror("recv error");
            break;
        } else if (bytes == 0) {
            /* No data left, jump out */
            break;
        } else {
            pos += bytes;
            if (pos >= len) {
                /* The buffer is full, jump out */
                break;
            }
        }

        /*
         * Check if data is available from socket fd, count is 0 when no data
         * available. Make select wait up to 10 ms for incoming data.
         * NOTES: On Linux, select() modifies timeout to reflect the amount of time
         * not slept.
         */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10*1000;
        count = select(sockfd + 1, &readfds, &writefds, (fd_set *)0, &timeout);
        if (count <= 0)  {
            break;
        }
    } while (count > 0);

    return pos;
}


#if 0

/******************************************************************************
 * NAME:
 *      recv_packet
 *
 * DESCRIPTION: 
 *      Receive a command packet. Use the data_len field in the header to avoid
 *      "no message boundaries" issue in "SOCK_STREAM" type socket.
 *        (1) Receive the header of packet
 *        (2) Check the signature of packet
 *        (3) Get the data length of packet
 *        (4) Receive the data of packet
 *
 * PARAMETERS:
 *      sockfd - The socket fd
 *      buf    - The buffer to keep command packet
 *      len    - The length of buffer
 *      flags  - Flags pass to recv() function
 *
 * RETURN:
 *      Bytes received.
 ******************************************************************************/
static ssize_t recv_packet(int sockfd, char *buf, ssize_t len, int flags)
{
    ssize_t bytes;
    ssize_t pos;
    int count;
    fd_set readfds, writefds;
    struct timeval timeout;
    uds_command_t *pkt = (uds_command_t *)buf;
    ssize_t header_len = sizeof(uds_command_t);

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(sockfd, &readfds);

    /* Receive the header of command packet first */
    pos = 0;
    while (pos < header_len) {
        bytes = recv(sockfd, buf+pos, header_len-pos, flags);
        if (bytes < 0) {
            perror("recv error");
            return 0;
        } else if (bytes == 0) {
            /* No data left, jump out */
            return pos;
        } else {
            pos += bytes;
            if (pos >= header_len) {
                /* All data of header received, jump out */
                break;
            }
        }

        /*
         * Check if data is available from socket fd, count is 0 when no data
         * available.
         * Make select wait up to 10 ms for incoming data.
         * NOTES: On Linux, select() modifies timeout to reflect the amount of time
         * not slept.
         */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10*1000;
        count = select(sockfd + 1, &readfds, &writefds, (fd_set *)0, &timeout);
        if (count <= 0)  {
            break;
        }
    } 

    /* Check the signature of command packet */
    if (pkt->signature != UDS_SIGNATURE) {
        printf("Error: invalid signature of packet\n");
        return 0;
    }

    /* Get the total length of command packet */
    if (len > pkt->data_len + header_len) {
        len = pkt->data_len + header_len;
    }

    /* Receive all data of command packet */
    while (pos < len) {
        bytes = recv(sockfd, buf+pos, len-pos, flags);
        if (bytes < 0) {
            perror("recv error");
            break;
        } else if (bytes == 0) {
            /* No data left, jump out */
            break;
        } else {
            pos += bytes;
            if (pos >= len) {
                /* All data received, jump out */
                break;
            }
        }

        /*
         * Check if data is available from socket fd, count is 0 when no data
         * available.
         * Make select wait up to 10 ms for incoming data.
         * NOTES: On Linux, select() modifies timeout to reflect the amount of time
         * not slept
         */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10*1000;
        count = select(sockfd + 1, &readfds, &writefds, (fd_set *)0, &timeout);
        if (count <= 0)  {
            break;
        }
    }

    return pos;
}
#endif


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

    word = (uint16_t *)buf;
    for (i = 0; i < len/2; i++) {
        sum += word[i];
    }

    /* If the length(bytes) of data buffer is an odd number, add the last byte. */
    if (len & 1) {
        byte = (uint8_t *)buf;
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
 *      verify_command_packet
 *
 * DESCRIPTION: 
 *      Verify the data integrity of the command packet.
 *
 * PARAMETERS:
 *      buf  - The data of command packet
 *      len  - The length of data
 *
 * RETURN:
 *      1 - OK, 0 - FAIL
 ******************************************************************************/
static int verify_command_packet(void *buf, size_t len)
{
    uds_command_t *pkt;
    
    if (buf == NULL) {
        return 0;
    }
    pkt = (uds_command_t *)buf;
    
    if (pkt->signature != UDS_SIGNATURE) {
        printf("Error: invalid signature of packet (0x%08X)\n", pkt->signature);
        return 0;
    }

    if (pkt->data_len + sizeof(uds_command_t) != len) {
        printf("Error: invalid length of packet (%ld:%ld)\n",
            pkt->data_len + sizeof(uds_command_t), len);
        return 0;
    }

    if (compute_checksum(buf, len) != 0) {
        printf("Error: invalid checksum of packet\n");
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
 *      arg - A pointer of connection info
 *
 * RETURN:
 *      None
 ******************************************************************************/
static void *request_handle_routine(void *arg)
{
    uds_connect_t *sc = (uds_connect_t *)arg;
    uds_command_t *req;
    uds_command_t *resp;
    uint8_t buf[UDS_BUF_SIZE];
    ssize_t bytes, req_len, resp_len;

    if (sc == NULL) {
        printf("Error: invalid argument of thread routine\n");
        pthread_exit(0);
    }

    while (1) {
        /* Receive request from client */
        //req_len = recv(sc->client_fd, buf, sizeof(buf), 0);
        req_len = recv_data(sc->client_fd, (char *)buf, sizeof(buf), 0);
        if (req_len <= 0) {
            close(sc->client_fd);
            sc->inuse = 0;
            break;
        }

        /* Check the integrity of the request packet */
        if (!verify_command_packet(buf, req_len)) {
            /* Discard invaid packet */
            continue;
        }

        /* Process the request */
        req = (uds_command_t *)buf;
        resp = sc->serv->request_handler(req);
        if (resp == NULL) {
            resp = (uds_command_t *)buf;   /* Use a local buffer */
            resp->status = STATUS_ERROR;
            resp->data_len = 0;
        }

        resp_len = sizeof(uds_command_t) + resp->data_len;
        resp->signature = req->signature;
        resp->checksum = 0;
        resp->checksum = compute_checksum(resp, resp_len);

        /* Send response */
        bytes = send(sc->client_fd, resp, resp_len, MSG_NOSIGNAL);
        if (resp != (uds_command_t *)buf) {    /* If NOT local buffer, free it */
            free(resp);
        }
        if (bytes != resp_len) {
            printf("Error: send response error\n");
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
 *      sock_path - The path of unix domain socket
 *      req_handler - The function pointer of a user-defined request handler.
 *
 * RETURN:
 *      A pointer of server info.
 ******************************************************************************/
uds_server_t *server_init(const char *sock_path, request_handler_t req_handler)
{
    uds_server_t *s;
    struct sockaddr_un addr;
    int i, rc;

    if (req_handler == NULL) {
        printf("Error: invalid parameter!\n");
        return NULL;
    }

    s = (uds_server_t *)malloc(sizeof(uds_server_t));
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

    unlink(sock_path);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);

    s->sockfd = socket(AF_UNIX, UDS_SOCK_TYPE, 0);
    if (s->sockfd < 0) {
        perror("socket error");
        free(s);
        return NULL;
    }

    /* Avoid "Address already in use" error in bind() */
    //int val = 1;
    //if (setsockopt(s->sockfd, SOL_SOCKET, SO_REUSEADDR, &val,
    //        sizeof(val)) == -1) {
    //    perror("setsockopt error");
    //    return NULL;
    //}

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
 *      s - A pointer of server info
 *
 * RETURN:
 *      0 - OK, Others - Error
 ******************************************************************************/
int server_accept_request(uds_server_t *s)
{
    uds_connect_t *sc;
    int cl, i;

    if (s == NULL) {
        printf("Error: invalid parameter!\n");
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
        printf("Error: too many connections\n");
        close(cl);
        return -1;
    }

    /* Start a new thread to handle the request */
    sc = &s->conn[i];
    sc->inuse = 1;
    sc->client_fd = cl;
    if (pthread_create(&sc->thread_id, NULL, request_handle_routine, sc) != 0) {
        perror("pthread_create error");
        close(cl);
        sc->inuse = 0;
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
 *      s - A pointer of server info
 *
 * RETURN:
 *      None
 ******************************************************************************/
void server_close(uds_server_t *s)
{
    int i;

    printf("Server closing\n");

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
 *      sock_path - The path of unix domain socket
 *      timeout   - Wait the server to be ready(in seconds) 
 *
 * RETURN:
 *      A pointer of client info.
 ******************************************************************************/
uds_client_t *client_init(const char *sock_path, int timeout)
{
    uds_client_t *sc;
    struct sockaddr_un addr;
    int fd, rc;

    sc = (uds_client_t *)malloc(sizeof(uds_client_t));
    if (sc == NULL) {
        perror("malloc error");
        return NULL;
    }
    memset(sc, 0, sizeof(uds_client_t));

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);
    fd = socket(AF_UNIX, UDS_SOCK_TYPE, 0);
    if (fd < 0) {
        perror("socket error");
        free(sc);
        return NULL;
    }
    sc->sockfd = fd;

    do {
        rc = connect(sc->sockfd, (struct sockaddr *)&addr, sizeof(addr));
        if (rc == 0) {
            break;
        } else {
            sleep(1);
        }
    } while (timeout-- > 0);
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
 *      c   - A pointer of client info
 *      req - The request to send
 *
 * RETURN:
 *      The response for the request. The caller need to free the memory.
 ******************************************************************************/
uds_command_t *client_send_request(uds_client_t *c, uds_command_t *req)
{
    uint8_t buf[UDS_BUF_SIZE];
    ssize_t bytes, req_len;

    if ((c == NULL) || (req == NULL)) {
        printf("Error: invalid parameter!\n");
        return NULL;
    }

    /* Send request */
    req_len = sizeof(uds_command_t) + req->data_len;
    req->signature = UDS_SIGNATURE;
    req->checksum = 0;
    req->checksum = compute_checksum(req, req_len);
    bytes = send(c->sockfd, req, req_len, MSG_NOSIGNAL);
    if (bytes != req_len) {
        perror("send error");
        return NULL;
    }

    /* Get response */
    //bytes = recv(c->sockfd, buf, sizeof(buf), 0);
    bytes = recv_data(c->sockfd, (char *)buf, sizeof(buf), 0);
    if (bytes <= 0) {
        printf("Error: receive response error\n");
        return NULL;
    }

    if (verify_command_packet(buf, bytes)) {
        uds_command_t *resp = (uds_command_t *)malloc(bytes);
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
 *      c - The pointer of client info
 *
 * RETURN:
 *      None
 ******************************************************************************/
void client_close(uds_client_t *c)
{
    if (c == NULL) {
        return;
    }

    close(c->sockfd);
    free(c);
}
