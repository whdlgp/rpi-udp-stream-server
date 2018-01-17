#include "../udp_setup/udp_setup.h"
#include "../openmax/h264.h"
#include "../common_util/common_util.h"
#include "app_timeout.h"

#include <pthread.h>

static void* stream_thread(void* arg)
{
    printf("let's send video!\n");
            
    OMX_BUFFERHEADERTYPE* frame_buffer;

    omx_h264_init();
    
    while(!is_quit())
    {
        frame_buffer = fill_frame_buffer();

        udp_send_stream(frame_buffer->pBuffer
                        , frame_buffer->nFilledLen);

        if (is_timeout())
            break;
    }

    omx_h264_deinit();

    DEBUG_MSG("stream thread ended\n");
    pthread_exit((void *) 0); // user-requested-stop
}

int main(int argc, char** argv)
{
    udp_server_setup();
    pthread_t stream_tid;
    int stream_thread_status;

    while(1)
    {
        if(udp_receive_command())
        {
            if(udp_check_command("VIDEO_REQUEST"))
            {
                DEBUG_MSG("set timeout %d ms\n", TIMEOUT_MS);
                set_timeout();

                DEBUG_MSG("create thread for stream\n");
                
                if(pthread_create(&stream_tid
                                  , NULL
                                  , stream_thread
                                  , NULL) != 0)
                    DEBUG_ERR("Error while creating stream_stread\n");
            }
            else if(udp_check_command("SET_TIMEOUT"))
            {
                DEBUG_MSG("set timeout %d ms\n", TIMEOUT_MS);
                set_timeout();
            }
            else if(udp_check_command("QUIT_SERVER"))
            {
                DEBUG_MSG("quit_request received\n");
                set_quit();
                DEBUG_MSG("wait until stream thread join\n");
                pthread_join(stream_tid, (void **)&stream_thread_status);

                break;
            }
        }
    }

    DEBUG_MSG("close and shutdown server\n");
    udp_server_close();

    return 0;
}
