#ifndef WEBRTC_JS_CLIENT_H
#define WEBRTC_JS_CLIENT_H

#define JS_SHARED_LIBRARY

#include "js-utils.h"

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_WEBRTC_CLIENT_MODULE js_init_module
#else
#define JS_INIT_WEBRTC_CLIENT_MODULE js_init_module_webrtc_client
#endif

JSModuleDef *JS_INIT_WEBRTC_CLIENT_MODULE(JSContext *ctx, const char *module_name);

#endif