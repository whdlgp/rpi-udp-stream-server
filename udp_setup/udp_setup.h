#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <sys/time.h>

#include "../common_util/common_util.h"

#define COMMAND_BUFSIZE 20
#define SERVER_COMMAND_PORT 50000
#define SERVER_STREAM_PORT 50001
#define CLIENT_COMMAND_PORT 50000
#define CLIENT_STREAM_PORT 50001

void udp_server_setup();
void udp_server_close();
int udp_receive_command();
int udp_check_command(const char* cmd);
void udp_send_stream(uint8_t* buf, uint32_t len);
