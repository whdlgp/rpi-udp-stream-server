#include "common_util.h"

int quit_flag;

void set_quit()
{
    quit_flag = 1;
}

int is_quit()
{
    return quit_flag;
}
