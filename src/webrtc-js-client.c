#include "webrtc-js-client.h"
#include "RTCPeerConnection-js.h"
#include "RTCDataChannel-js.h"
#include "RTCDataChannelBase-js.h"
#include "WebSocketClient-js.h"
#include "RTCTrack-js.h"
#include "event-queue.h"

#define JS_DEF_FLAG(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE)

static const JSCFunctionListEntry webrtc_global_funcs[] = {
    JS_DEF_FLAG(RTC_NAL_SEPARATOR_LENGTH),
    JS_DEF_FLAG(RTC_NAL_SEPARATOR_LONG_START_SEQUENCE),
    JS_DEF_FLAG(RTC_NAL_SEPARATOR_SHORT_START_SEQUENCE),
    JS_DEF_FLAG(RTC_NAL_SEPARATOR_START_SEQUENCE),
    JS_DEF_FLAG(RTC_DIRECTION_SENDONLY),
    JS_DEF_FLAG(RTC_DIRECTION_RECVONLY),
    JS_DEF_FLAG(RTC_DIRECTION_SENDRECV),
    JS_DEF_FLAG(RTC_DIRECTION_INACTIVE),
    JS_DEF_FLAG(RTC_DIRECTION_UNKNOWN),
    JS_DEF_FLAG(RTC_CODEC_H264),
    JS_DEF_FLAG(RTC_CODEC_OPUS),
    JS_DEF_FLAG(RTC_CODEC_VP8),
    JS_DEF_FLAG(RTC_CODEC_VP9),
    JS_CFUNC_DEF("createWebSocketClient", 1, createWebSocketClient)
};

static int init(JSContext *ctx, JSModuleDef *m) {
    JS_SetModuleExportList(ctx, m, webrtc_global_funcs, countof(webrtc_global_funcs));
    initFullClass(ctx, m, &RTCPeerConnection_Class);
    initFullClass(ctx, m, &RTCDataChannelBase_Class);
    return 0;
}

JSModuleDef *JS_INIT_WEBRTC_CLIENT_MODULE(JSContext *ctx, const char *module_name) {
    JSModuleDef *m;
    rtcInitLogger(RTC_LOG_DEBUG, NULL);
    m = JS_NewCModule(ctx, module_name, init);
    if (!m) return NULL;
    initEventQueue(ctx);
    JS_AddModuleExportList(ctx, m, webrtc_global_funcs, countof(webrtc_global_funcs));
    JS_AddModuleExport(ctx, m, RTCPeerConnection_Class.def.class_name);
    return m;
}