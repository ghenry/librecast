#ifndef __LIBRECAST_ERRORS_H__
#define __LIBRECAST_ERRORS_H__ 1

#include <errno.h>

#define ERROR_CODES(X)                                                        \
	X(0, ERROR_SUCCESS,             "Success")                            \
	X(1, ERROR_FAILURE,             "Failure")                            \
	X(2, ERROR_PID_OPEN,            "Failed to open pidfile")             \
	X(3, ERROR_PID_READFAIL,        "Failed to read pidfile")             \
	X(4, ERROR_PID_INVALID,         "Invalid pid")                        \
	X(5, ERROR_ALREADY_RUNNING,     "Daemon already running")             \
	X(6, ERROR_PID_WRITEFAIL,       "Failed to write to pidfile")         \
	X(7, ERROR_DAEMON_FAILURE,      "Failed to daemonize")                \
	X(8, ERROR_CONFIG_NOTNUMERIC,   "Numeric config value not numeric")   \
	X(9, ERROR_CONFIG_BOUNDS,       "Numeric config value out of bounds") \
	X(10, ERROR_CONFIG_BOOLEAN,     "Invalid boolean config value")       \
	X(11, ERROR_CONFIG_READFAIL,    "Unable to read config file")         \
	X(12, ERROR_CONFIG_INVALID,     "Error in config file")               \
	X(13, ERROR_MALLOC,             "Memory allocation error")            \
	X(14, ERROR_INVALID_ARGS,       "Invalid command line options")       \
	X(15, ERROR_DAEMON_STOPPED,     "Daemon not running")                 \
	X(16, ERROR_NET_SEND,           "Error sending data")                 \
	X(17, ERROR_NET_RECV,           "Error receiving data")               \
	X(18, ERROR_NET_SOCKOPT,        "Error setting socket options")       \
	X(19, ERROR_CMD_INVALID,        "Invalid Command received")           \
	X(20, ERROR_SOCKET_CREATE,      "Unable to create unix socket")       \
	X(21, ERROR_SOCKET_CONNECT,     "Unable to connect to unix socket")

#define ERROR_MSG(code, name, msg) case code: return msg;
#define ERROR_ENUM(code, name, msg) name = code,
enum {
	ERROR_CODES(ERROR_ENUM)
};

/* return human readable error message for e */
char *error_msg(int e);

/* print human readable error, using errsv (errno) or progam defined (e) code */
void print_error(int e, int errsv, char *errstr);

#endif /* __LIBRECAST_ERRORS_H__ */
