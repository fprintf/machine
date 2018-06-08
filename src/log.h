#ifndef CBOT_LOG_HEADER__H__
#define CBOT_LOG_HEADER__H__

/* Debug logging, noop if turned off */
#define log_debugf(fmt, ...) fprintf(stderr, "%s(%u): " fmt "\n", __FILE__, __LINE__,##__VA_ARGS__)
#ifdef NDEBUG
#    define log_debug(...) log_debugf(__VA_ARGS__)
#else
#    define log_debug(...) 
#endif

#endif
