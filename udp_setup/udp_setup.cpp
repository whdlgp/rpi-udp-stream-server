#include "udp_setup.h"

static int server_command_socket;
static int server_stream_socket;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static socklen_t client_addr_len;
static int command_len;
static char command_buf[COMMAND_BUFSIZE];

void udp_server_setup()
{
    DEBUG_MSG("bind socket for command and stream\n");
    server_command_socket = socket(AF_INET, SOCK_DGRAM, 0);
    server_stream_socket = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_COMMAND_PORT);
    if(bind(server_command_socket
            , (struct sockaddr*)&server_addr
            , sizeof(server_addr)) < 0)
    {
        DEBUG_ERR("command socket bind error\n");
        exit(0);
    }


    server_addr.sin_port = htons(SERVER_STREAM_PORT);
    if(bind(server_stream_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        DEBUG_ERR(" stream socket bind error\n");
        exit(0);
    }
}

void udp_server_close()
{
    close(server_command_socket);
    close(server_stream_socket);
}

int udp_receive_command()
{
    client_addr_len = sizeof(client_addr);
    if((command_len = recvfrom(server_command_socket
                    , command_buf
                    , COMMAND_BUFSIZE
                    , 0
                    , (struct sockaddr*)&client_addr
                    , &client_addr_len)) < 0)
    {
        DEBUG_ERR("receive command error\n");
        return 0;
    }

    DEBUG_MSG("command received : %d\n", command_len);

    return 1;
}

int udp_check_command(const char* cmd)
{
    return (!strncmp(cmd, command_buf, strlen(cmd)));
}

void udp_send_stream(uint8_t* buf, uint32_t len)
{
    client_addr.sin_port = htons(CLIENT_STREAM_PORT);
    if(sendto(server_stream_socket
            , buf
            , len
            , 0
            , (struct sockaddr*)&client_addr
            , sizeof(client_addr)) < 0)
    {
        DEBUG_ERR("stream send error\n");
        exit(0);
    }
}
