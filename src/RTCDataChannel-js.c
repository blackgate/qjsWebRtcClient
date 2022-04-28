#include "RTCDataChannel-js.h"
#include "RTCDataChannelBase-js.h"
#include <rtc/rtc.h>

static void RTCDataChannel_Finalizer(JSRuntime *rt, JSValue val) {
    int channelId = getRTCDataChannelId(val);
    rtcDeleteDataChannel(channelId);
}

static JSValue RTCDataChannel_getLabel(JSContext *ctx, JSValueConst this_val) {
    int channelId = getRTCDataChannelId(this_val);
    int labelLen = rtcGetDataChannelLabel(channelId, NULL, 0);
    if (labelLen < 0)
        JS_ThrowInternalError(ctx, "Error getting data channel label. Status code: %x", labelLen);
    char *label = js_malloc(ctx, labelLen + 1);
    rtcGetDataChannelLabel(channelId, label, labelLen);
    return JS_NewString(ctx, label); 
}

static JSCFunctionListEntry RTCDataChannel_Methods[] = {
    JS_CGETSET_DEF("label", RTCDataChannel_getLabel, NULL)
};

JSValue createRTCDataChannelClass(JSContext *ctx, int channelId) {
    JSValue obj = createRTCDataChannelBaseClass(ctx, channelId, RTCDataChannel_Finalizer);
    JS_SetPropertyFunctionList(ctx, obj, RTCDataChannel_Methods, countof(RTCDataChannel_Methods));
    return obj;
}