#include <stdlib.h>
#include <string.h>
#include <node_api.h>
#include <uiohook.h>
#include "napi_helpers.h"
#include "uiohook_worker.h"

static napi_threadsafe_function threadsafe_fn = NULL;
static bool is_worker_running = false;

void dispatch_proc(uiohook_event* const event) {
  if (threadsafe_fn == NULL) return;

  uiohook_event* copied_event = malloc(sizeof(uiohook_event));
  memcpy(copied_event, event, sizeof(uiohook_event));
  if (copied_event->type == EVENT_MOUSE_DRAGGED) {
    copied_event->type = EVENT_MOUSE_MOVED;
  }

  napi_status status = napi_call_threadsafe_function(threadsafe_fn, copied_event, napi_tsfn_nonblocking);
  if (status == napi_closing) {
    threadsafe_fn = NULL;
    free(copied_event);
    return;
  }
  NAPI_FATAL_IF_FAILED(status, "dispatch_proc", "napi_call_threadsafe_function");
}

napi_value uiohook_to_js_event(napi_env env, uiohook_event* event) {
  napi_status status;

  napi_value event_obj;
  status = napi_create_object(env, &event_obj);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_object");

  napi_value e_type;
  status = napi_create_uint32(env, event->type, &e_type);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_uint32");

  napi_value e_altKey;
  status = napi_get_boolean(env, (event->mask & (MASK_ALT)), &e_altKey);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_get_boolean");

  napi_value e_ctrlKey;
  status = napi_get_boolean(env, (event->mask & (MASK_CTRL)), &e_ctrlKey);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_get_boolean");

  napi_value e_metaKey;
  status = napi_get_boolean(env, (event->mask & (MASK_META)), &e_metaKey);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_get_boolean");

  napi_value e_shiftKey;
  status = napi_get_boolean(env, (event->mask & (MASK_SHIFT)), &e_shiftKey);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_get_boolean");

  napi_value e_time;
  status = napi_create_double(env, (double)event->time, &e_time);
  NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_double");

  if (event->type == EVENT_KEY_PRESSED || event->type == EVENT_KEY_RELEASED) {
    napi_value e_keycode;
    status = napi_create_uint32(env, event->data.keyboard.keycode, &e_keycode);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_uint32");

    napi_property_descriptor descriptors[] = {
      { "type",     NULL, NULL, NULL, NULL, e_type,     napi_enumerable, NULL },
      { "time",     NULL, NULL, NULL, NULL, e_time,     napi_enumerable, NULL },
      { "altKey",   NULL, NULL, NULL, NULL, e_altKey,   napi_enumerable, NULL },
      { "ctrlKey",  NULL, NULL, NULL, NULL, e_ctrlKey,  napi_enumerable, NULL },
      { "metaKey",  NULL, NULL, NULL, NULL, e_metaKey,  napi_enumerable, NULL },
      { "shiftKey", NULL, NULL, NULL, NULL, e_shiftKey, napi_enumerable, NULL },
      { "keycode",  NULL, NULL, NULL, NULL, e_keycode,  napi_enumerable, NULL },
    };
    status = napi_define_properties(env, event_obj, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_define_properties");
    return event_obj;
  }
  else if (event->type == EVENT_MOUSE_MOVED || event->type == EVENT_MOUSE_PRESSED || event->type == EVENT_MOUSE_RELEASED || event->type == EVENT_MOUSE_CLICKED) {
    napi_value e_x;
    status = napi_create_int32(env, event->data.mouse.x, &e_x);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_int32");

    napi_value e_y;
    status = napi_create_int32(env, event->data.mouse.y, &e_y);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_int32");

    napi_value e_button;
    status = napi_create_uint32(env, event->data.mouse.button, &e_button);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_uint32");

    napi_value e_clicks;
    status = napi_create_uint32(env, event->data.mouse.clicks, &e_clicks);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_uint32");

    napi_property_descriptor descriptors[] = {
      { "type",     NULL, NULL, NULL, NULL, e_type,     napi_enumerable, NULL },
      { "time",     NULL, NULL, NULL, NULL, e_time,     napi_enumerable, NULL },
      { "altKey",   NULL, NULL, NULL, NULL, e_altKey,   napi_enumerable, NULL },
      { "ctrlKey",  NULL, NULL, NULL, NULL, e_ctrlKey,  napi_enumerable, NULL },
      { "metaKey",  NULL, NULL, NULL, NULL, e_metaKey,  napi_enumerable, NULL },
      { "shiftKey", NULL, NULL, NULL, NULL, e_shiftKey, napi_enumerable, NULL },
      { "x",        NULL, NULL, NULL, NULL, e_x,        napi_enumerable, NULL },
      { "y",        NULL, NULL, NULL, NULL, e_y,        napi_enumerable, NULL },
      { "button",   NULL, NULL, NULL, NULL, e_button,   napi_enumerable, NULL },
      { "clicks",   NULL, NULL, NULL, NULL, e_clicks,   napi_enumerable, NULL },
    };
    status = napi_define_properties(env, event_obj, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_define_properties");
    return event_obj;
  }
  else if (event->type == EVENT_MOUSE_WHEEL) {
    napi_value e_x;
    status = napi_create_int32(env, event->data.wheel.x, &e_x);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_int32");

    napi_value e_y;
    status = napi_create_int32(env, event->data.wheel.y, &e_y);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_create_int32");

    napi_value e_clicks;
    e_clicks = napi_create_uint32(env, event->data.wheel.clicks);
    
    napi_value e_amount;
    e_amount = napi_create_uint32(env, event->data.wheel.amount);
    
    napi_value e_direction;
    e_direction = napi_create_uint32(env, event->data.wheel.direction);
    
    napi_value e_rotation;
    e_rotation = napi_create_int32(env, event->data.wheel.rotation);

    napi_property_descriptor descriptors[] = {
      { "type",      NULL, NULL, NULL, NULL, e_type,      napi_enumerable, NULL },
      { "time",      NULL, NULL, NULL, NULL, e_time,      napi_enumerable, NULL },
      { "altKey",    NULL, NULL, NULL, NULL, e_altKey,    napi_enumerable, NULL },
      { "ctrlKey",   NULL, NULL, NULL, NULL, e_ctrlKey,   napi_enumerable, NULL },
      { "metaKey",   NULL, NULL, NULL, NULL, e_metaKey,   napi_enumerable, NULL },
      { "shiftKey",  NULL, NULL, NULL, NULL, e_shiftKey,  napi_enumerable, NULL },
      { "x",         NULL, NULL, NULL, NULL, e_x,         napi_enumerable, NULL },
      { "y",         NULL, NULL, NULL, NULL, e_y,         napi_enumerable, NULL },
      { "clicks",    NULL, NULL, NULL, NULL, e_clicks,    napi_enumerable, NULL },
      { "amount",    NULL, NULL, NULL, NULL, e_amount,    napi_enumerable, NULL },
      { "direction", NULL, NULL, NULL, NULL, e_direction, napi_enumerable, NULL },
      { "rotation",  NULL, NULL, NULL, NULL, e_rotation,  napi_enumerable, NULL },
    };
    status = napi_define_properties(env, event_obj, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    NAPI_FATAL_IF_FAILED(status, "uiohook_to_js_event", "napi_define_properties");
    return event_obj;
  }

  return event_obj;
}

void tsfn_callback(napi_env env, napi_value js_callback, void* context, void* data) {
  uiohook_event* event = (uiohook_event*)data;

  if (env != NULL) {
    napi_value js_event = uiohook_to_js_event(env, event);
    napi_value undefined;
    napi_status status = napi_get_undefined(env, &undefined);
    NAPI_FATAL_IF_FAILED(status, "tsfn_callback", "napi_get_undefined");

    status = napi_call_function(env, undefined, js_callback, 1, &js_event, NULL);
    if (status != napi_ok) {
      fprintf(stderr, "ERROR in tsfn_callback: napi_call_function failed with %d\n", status);
    }
  }

  free(event);
}

void tsfn_finalize(napi_env env, void* finalize_data, void* finalize_hint) {
  // Nothing to do here
}

napi_value start(napi_env env, napi_callback_info info) {
  napi_status status;

  if (is_worker_running) {
    napi_throw_error(env, NULL, "Worker is already running");
    return NULL;
  }

  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  NAPI_CALL(env, status);

  if (argc < 1) {
    napi_throw_error(env, NULL, "Expected 1 argument");
    return NULL;
  }

  napi_valuetype valuetype;
  status = napi_typeof(env, argv[0], &valuetype);
  NAPI_CALL(env, status);

  if (valuetype != napi_function) {
    napi_throw_error(env, NULL, "Expected a function");
    return NULL;
  }

  status = napi_create_threadsafe_function(
    env,
    argv[0],
    NULL,
    NULL,
    10,
    1,
    NULL,
    tsfn_finalize,
    NULL,
    tsfn_callback,
    &threadsafe_fn
  );
  NAPI_CALL(env, status);

  int result = uiohook_worker_start(dispatch_proc);
  if (result != UIOHOOK_SUCCESS) {
    napi_release_threadsafe_function(threadsafe_fn, napi_tsfn_abort);
    threadsafe_fn = NULL;

    char error_message[128];
    snprintf(error_message, sizeof(error_message), "Failed to start worker: %d", result);
    napi_throw_error(env, NULL, error_message);
    return NULL;
  }

  is_worker_running = true;

  return NULL;
}

napi_value stop(napi_env env, napi_callback_info info) {
  if (!is_worker_running) {
    napi_throw_error(env, NULL, "Worker is not running");
    return NULL;
  }

  int result = uiohook_worker_stop();
  if (result != UIOHOOK_SUCCESS) {
    char error_message[128];
    snprintf(error_message, sizeof(error_message), "Failed to stop worker: %d", result);
    napi_throw_error(env, NULL, error_message);
    return NULL;
  }

  if (threadsafe_fn != NULL) {
    napi_release_threadsafe_function(threadsafe_fn, napi_tsfn_abort);
    threadsafe_fn = NULL;
  }

  is_worker_running = false;

  return NULL;
}

napi_value key_tap(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  NAPI_CALL(env, status);

  if (argc < 1) {
    napi_throw_error(env, NULL, "Expected at least 1 argument");
    return NULL;
  }

  uint32_t keycode;
  status = napi_get_value_uint32(env, argv[0], &keycode);
  NAPI_CALL(env, status);

  uint32_t toggle = 0; // Default to tap
  if (argc >= 2) {
    status = napi_get_value_uint32(env, argv[1], &toggle);
    NAPI_CALL(env, status);
  }

  // Call libuiohook's post_event functions to simulate key press/release
  if (toggle == 0) { // Tap
    // Press and release
    uiohook_event event = {
      .type = EVENT_KEY_PRESSED,
      .data.keyboard.keycode = keycode
    };
    hook_post_event(&event);

    event.type = EVENT_KEY_RELEASED;
    hook_post_event(&event);
  } else if (toggle == 1) { // Down
    uiohook_event event = {
      .type = EVENT_KEY_PRESSED,
      .data.keyboard.keycode = keycode
    };
    hook_post_event(&event);
  } else if (toggle == 2) { // Up
    uiohook_event event = {
      .type = EVENT_KEY_RELEASED,
      .data.keyboard.keycode = keycode
    };
    hook_post_event(&event);
  }

  return NULL;
}

napi_value init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor descriptors[] = {
    { "start", NULL, start, NULL, NULL, NULL, napi_default, NULL },
    { "stop", NULL, stop, NULL, NULL, NULL, napi_default, NULL },
    { "keyTap", NULL, key_tap, NULL, NULL, NULL, napi_default, NULL },
  };
  status = napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
  NAPI_CALL(env, status);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init) 