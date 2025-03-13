#ifndef NAPI_HELPERS_H
#define NAPI_HELPERS_H

#include <node_api.h>
#include <stdio.h>
#include <stdlib.h>

#define NAPI_CALL(env, call)                                      \
  do {                                                            \
    napi_status status = (call);                                  \
    if (status != napi_ok) {                                      \
      const napi_extended_error_info* error_info = NULL;          \
      napi_get_last_error_info((env), &error_info);               \
      bool is_pending;                                            \
      napi_is_exception_pending((env), &is_pending);              \
      if (!is_pending) {                                          \
        const char* message = (error_info->error_message == NULL) \
            ? "unknown error"                                     \
            : error_info->error_message;                          \
        napi_throw_error((env), NULL, message);                   \
        return NULL;                                              \
      }                                                           \
    }                                                             \
  } while(0)

#define NAPI_FATAL_IF_FAILED(status, location, message)           \
  do {                                                            \
    if ((status) != napi_ok) {                                    \
      fprintf(stderr,                                             \
              "FATAL ERROR in %s %s: %d\n", location, message, status); \
      exit(1);                                                    \
    }                                                             \
  } while(0)

napi_value napi_get_boolean(napi_env env, bool value);
napi_value napi_create_uint32(napi_env env, uint32_t value);
napi_value napi_create_int32(napi_env env, int32_t value);
napi_value napi_create_double(napi_env env, double value);

#endif // NAPI_HELPERS_H 