#include "RTCDataChannel-js.h"
#include "event-queue.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    RTC_DATACHANNEL_EVENTS_ONOPEN,
    RTC_DATACHANNEL_EVENTS_ONCLOSE,
    RTC_DATACHANNEL_EVENTS_ONMESSAGE,
    RTC_DATACHANNEL_EVENTS_MAX,
};

typedef struct {
    int isBinary; 
    char* pMsg; 
    int pMsgLen;
} RTCDataChannel_Message;

typedef struct {
    JSContext *ctx;
    int channelId;
    JSValue thisObj;
    JSValue events[RTC_DATACHANNEL_EVENTS_MAX];
} RTCDataChannel_ClassData;

static RTCDataChannel_ClassData* getRTCDataChannelClassData(JSValueConst this_val) {
    return JS_GetOpaque(this_val, RTCDataChannel_Class.id);
}

static void RTCDataChannel_onOpen(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONOPEN];
    if (JS_IsFunction(state->ctx, fn))
        JS_Call(state->ctx, fn, this_val, 0, NULL);
}

static void RTCDataChannel_onClose(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONCLOSE];
    if (JS_IsFunction(state->ctx, fn))
        JS_Call(state->ctx, fn, this_val, 0, NULL);
}

static void RTCDataChannel_onMessage(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    RTCDataChannel_Message *msg = (RTCDataChannel_Message *)data;
    JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONMESSAGE];
    if (JS_IsFunction(state->ctx, fn)) {
        JSValue param = msg->isBinary 
            ? JS_NewArrayBuffer(state->ctx, msg->pMsg, msg->pMsgLen, NULL, NULL, 0)
            : JS_NewStringLen(state->ctx,  msg->pMsg, msg->pMsgLen);
        JS_Call(state->ctx, fn, this_val, 1, &param);
        JS_FreeValue(state->ctx, param);   
    }
    free(msg->pMsg);
}

static void handleOnOpen(int channelId, void *ptr) {
    RTCDataChannel_ClassData *state = (RTCDataChannel_ClassData *)ptr;
    enqueueEvent(RTCDataChannel_onOpen, state->thisObj, NULL);
}

static void handleOnClose(int channelId, void *ptr) {
    RTCDataChannel_ClassData *state = (RTCDataChannel_ClassData *)ptr;
    enqueueEvent(RTCDataChannel_onClose, state->thisObj, NULL);
}

static void handleOnMessage(int id, const char *message, int size, void *ptr) {
    RTCDataChannel_ClassData *state = (RTCDataChannel_ClassData *)ptr;
    RTCDataChannel_Message *msg = malloc(sizeof(RTCDataChannel_Message));
    msg->isBinary = size >= 0;
    msg->pMsgLen = size < 0 ? strlen(message) + 1 : size;
    msg->pMsg = malloc(msg->pMsgLen);
    memcpy(msg->pMsg, message, msg->pMsgLen);
    enqueueEvent(RTCDataChannel_onMessage, state->thisObj, msg);
}

JSValue createRTCDataChannelClass(JSContext *ctx, int channelId) {
    JSValue obj = JS_NewObjectClass(ctx, RTCDataChannel_Class.id); 
    RTCDataChannel_ClassData *state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->channelId = channelId;
    state->thisObj = obj;
    for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++) 
        state->events[i] = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    rtcSetUserPointer(channelId, state);
    rtcSetOpenCallback(channelId, handleOnOpen);
    rtcSetClosedCallback(channelId, handleOnClose);
    rtcSetMessageCallback(channelId, handleOnMessage);
    return obj;
}

static void RTCDataChannel_Finalizer(JSRuntime *rt, JSValue val) {
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(val);
    for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++)
        JS_FreeValueRT(rt, state->events[i]);
    rtcDeleteDataChannel(state->channelId);
    js_free(state->ctx, state);
}

static void RTCDataChannel_GcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(val);
    if (state) {
        for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++)
            JS_MarkValue(rt, state->events[i], mark_func);
    }
}

static JSValue RTCDataChannel_EventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue RTCDataChannel_EventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static JSValue RTCDataChannel_send(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    int isString;
    char *val;
    size_t len;
    isString = JS_IsString(argv[0]);
    val = isString 
        ? (char *)JS_ToCString(ctx, argv[0]) 
        : (char *)JS_GetArrayBuffer(ctx, &len, argv[0]);
    if (rtcSendMessage(state->channelId, val, isString ? -1 : len) < 0)
        return JS_ThrowInternalError(ctx, "Error sending data");
    if (isString) JS_FreeCString(ctx, val);
    return JS_UNDEFINED;
}

static JSValue RTCDataChannel_getLabel(JSContext *ctx, JSValueConst this_val)
{   
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    int labelLen = rtcGetDataChannelLabel(state->channelId, NULL, 0);
    if (labelLen < 0)
        JS_ThrowInternalError(ctx, "Error getting data channel label. Status code: %x", labelLen);
    char *label = js_malloc(ctx, labelLen + 1);
    rtcGetDataChannelLabel(state->channelId, label, labelLen);
    return JS_NewString(ctx, label); 
}

static JSValue RTCDataChannel_isOpen(JSContext *ctx, JSValueConst this_val)
{   
    RTCDataChannel_ClassData *state = getRTCDataChannelClassData(this_val);
    int isOpen = rtcIsOpen(state->channelId);
    return JS_NewBool(ctx, isOpen); 
}

static JSCFunctionListEntry RTCDataChannel_Methods[] = {
    JS_CFUNC_DEF("send", 1, RTCDataChannel_send),
    JS_CGETSET_DEF("label", RTCDataChannel_getLabel, NULL),
    JS_CGETSET_DEF("isOpen", RTCDataChannel_isOpen, NULL),
    JS_CGETSET_MAGIC_DEF("onopen", 
        RTCDataChannel_EventGet, 
        RTCDataChannel_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONOPEN),
    JS_CGETSET_MAGIC_DEF("onclose", 
        RTCDataChannel_EventGet, 
        RTCDataChannel_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONCLOSE),
    JS_CGETSET_MAGIC_DEF("onmessage", 
        RTCDataChannel_EventGet, 
        RTCDataChannel_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONMESSAGE)
};

JSFullClassDef RTCDataChannel_Class = {
    .def = {
        .class_name = "RTCDataChannel",
        .finalizer = RTCDataChannel_Finalizer,
        .gc_mark = RTCDataChannel_GcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(RTCDataChannel_Methods),
    .funcs = RTCDataChannel_Methods
};