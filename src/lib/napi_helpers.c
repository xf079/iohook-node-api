#include "napi_helpers.h"

napi_value napi_get_boolean(napi_env env, bool value) {
  napi_value result;
  napi_status status = napi_get_boolean(env, value, &result);
  NAPI_FATAL_IF_FAILED(status, "napi_get_boolean", "napi_get_boolean");
  return result;
}

napi_value napi_create_uint32(napi_env env, uint32_t value) {
  napi_value result;
  napi_status status = napi_create_uint32(env, value, &result);
  NAPI_FATAL_IF_FAILED(status, "napi_create_uint32", "napi_create_uint32");
  return result;
}

napi_value napi_create_int32(napi_env env, int32_t value) {
  napi_value result;
  napi_status status = napi_create_int32(env, value, &result);
  NAPI_FATAL_IF_FAILED(status, "napi_create_int32", "napi_create_int32");
  return result;
}

napi_value napi_create_double(napi_env env, double value) {
  napi_value result;
  napi_status status = napi_create_double(env, value, &result);
  NAPI_FATAL_IF_FAILED(status, "napi_create_double", "napi_create_double");
  return result;
} 