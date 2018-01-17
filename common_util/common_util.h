#define USE_DEBUG_MSG

#ifdef USE_DEBUG_MSG
#define DEBUG_ERR(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_MSG(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_ERR(...)
#define DEBUG_MSG(...)
#endif

void set_quit();
int is_quit();
