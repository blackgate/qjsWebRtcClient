#include "WebSocketClient-js.h"
#include "RTCDataChannelBase-js.h"
#include <rtc/rtc.h>

static void WebSocketClient_Finalizer(JSRuntime *rt, JSValue val) {
    int id = getRTCDataChannelId(val);
    rtcDeleteWebSocket(id);
}

static JSValue WebSocketClient_getPath(JSContext *ctx, JSValueConst this_val) {
    int channelId = getRTCDataChannelId(this_val);
    int pathLen = rtcGetWebSocketPath(channelId, NULL, 0);
    if (pathLen < 0)
        JS_ThrowInternalError(ctx, "Error getting websocket path. Status code: %x", pathLen);
    char *path = js_malloc(ctx, pathLen + 1);
    rtcGetWebSocketPath(channelId, path, pathLen);
    return JS_NewString(ctx, path); 
}

static JSValue WebSocketClient_getRemoteAddress(JSContext *ctx, JSValueConst this_val) {
    int channelId = getRTCDataChannelId(this_val);
    int addrLen = rtcGetWebSocketRemoteAddress(channelId, NULL, 0);
    if (addrLen < 0)
        JS_ThrowInternalError(ctx, "Error getting websocket remote address. Status code: %x", addrLen);
    char *addr = js_malloc(ctx, addrLen + 1);
    rtcGetWebSocketRemoteAddress(channelId, addr, addrLen);
    return JS_NewString(ctx, addr); 
}

static JSCFunctionListEntry WebSocketClient_Methods[] = {
    JS_CGETSET_DEF("path", WebSocketClient_getPath, NULL),
    JS_CGETSET_DEF("remoteAddress", WebSocketClient_getRemoteAddress, NULL)
};

JSValue createWebSocketClient(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv) 
{
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid url argument");
    const char *url = JS_ToCString(ctx, argv[0]);
    JSValue ret;
    int id = rtcCreateWebSocket(url);
    if (id < 0) {
        ret = JS_ThrowTypeError(ctx, "Error creating websocket client. Status: %x", id);
        JS_FreeCString(ctx, url);
    } else {
        ret = createRTCDataChannelBaseClass(ctx, id, WebSocketClient_Finalizer);
        JS_SetPropertyFunctionList(ctx, ret, WebSocketClient_Methods, countof(WebSocketClient_Methods));
    }
    JS_FreeCString(ctx, url);
    return ret;
}