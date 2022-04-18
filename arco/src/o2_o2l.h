/* o2_o2l.h -- compatibility macros for O2 and O2lite
 *
 * Roger B. Dannenberg
 * Dec 2021
 */

#ifdef IS_O2_CLIENT
// O2 definitions

#define HANDLER_ARGS O2message_ptr msg, char *types, \
                     O2arg_ptr *argv, int argc, void *user_data

#else 
// O2lite definitions

#define HANDLER_ARGS o2l_msg_ptr msg, const char *types, void *data, void *info
#define METHOD_NEW(ad, ts, full, h, inf) o2l_method_new(ad, ts, full, h, inf)

#define GET_ERROR() o2l_get_error()
#define GET_MSG_TIMESTAMP() o2l_get_timestamp()
#define GET_TIME() o2l_get_time()
#define GET_DOUBLE() o2l_get_double()
#define GET_FLOAT() o2l_get_float()
#define GET_INT32() o2l_get_int32()
#define GET_STRING() o2l_get_string()

#define SEND_START(addr, tim, ts, tcp) o2l_send_start(addr, tim, ts, tcp)
#define SEND_FINISH() o2l_send();
#endif
