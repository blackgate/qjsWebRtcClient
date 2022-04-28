#include "RTCDataChannelBase-js.h"
#include "event-queue.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rtc/rtc.h>

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
} RTCDataChannelBase_Message;

typedef struct {
    JSContext *ctx;
    int channelId;
    JSValue thisObj;
    JSValue events[RTC_DATACHANNEL_EVENTS_MAX];
    JSClassFinalizer *finalizer;
} RTCDataChannelBase_ClassData;

static RTCDataChannelBase_ClassData* getRTCDataChannelClassData(JSValueConst this_val) {
    return JS_GetOpaque(this_val, RTCDataChannelBase_Class.id);
}

int getRTCDataChannelId(JSValueConst this_val) {
    return getRTCDataChannelClassData(this_val)->channelId;
}

static void RTCDataChannelBase_onOpen(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    printf("opening conection\n");
    if (state) {
        JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONOPEN];
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, this_val, 0, NULL);
    }
    
}

static void RTCDataChannelBase_onClose(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    if (state) {
        JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONCLOSE];
        if (JS_IsFunction(state->ctx, fn))
            JS_Call(state->ctx, fn, this_val, 0, NULL);
    }
}

static void RTCDataChannelBase_onMessage(JSContext *ctx, JSValue this_val, void *data) {
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    RTCDataChannelBase_Message *msg = (RTCDataChannelBase_Message *)data;
    if (state) {
        JSValue fn = state->events[RTC_DATACHANNEL_EVENTS_ONMESSAGE];
        if (JS_IsFunction(state->ctx, fn)) {
            JSValue param = msg->isBinary 
                ? JS_NewArrayBuffer(state->ctx, msg->pMsg, msg->pMsgLen, NULL, NULL, 0)
                : JS_NewStringLen(state->ctx,  msg->pMsg, msg->pMsgLen);
            JS_Call(state->ctx, fn, this_val, 1, &param);
            JS_FreeValue(state->ctx, param);   
        }
    }
    free(msg->pMsg);
}

static void handleOnOpen(int channelId, void *ptr) {
    RTCDataChannelBase_ClassData *state = (RTCDataChannelBase_ClassData *)ptr;
    enqueueEvent(RTCDataChannelBase_onOpen, state->thisObj, NULL);
}

static void handleOnClose(int channelId, void *ptr) {
    RTCDataChannelBase_ClassData *state = (RTCDataChannelBase_ClassData *)ptr;
    enqueueEvent(RTCDataChannelBase_onClose, state->thisObj, NULL);
}

static void handleOnMessage(int id, const char *message, int size, void *ptr) {
    RTCDataChannelBase_ClassData *state = (RTCDataChannelBase_ClassData *)ptr;
    RTCDataChannelBase_Message *msg = malloc(sizeof(RTCDataChannelBase_Message));
    msg->isBinary = size >= 0;
    msg->pMsgLen = size < 0 ? strlen(message) + 1 : size;
    msg->pMsg = malloc(msg->pMsgLen);
    memcpy(msg->pMsg, message, msg->pMsgLen);
    enqueueEvent(RTCDataChannelBase_onMessage, state->thisObj, msg);
}

static void RTCDataChannelBase_Finalizer(JSRuntime *rt, JSValue val) {
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(val);
    for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++)
        JS_FreeValueRT(rt, state->events[i]);
    state->finalizer(rt, val);
    js_free(state->ctx, state);
    printf("freeing data channel\n");
}

static void RTCDataChannelBase_GcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(val);
    if (state) {
        for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++)
            JS_MarkValue(rt, state->events[i], mark_func);
    }
}

static JSValue RTCDataChannelBase_EventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue RTCDataChannelBase_EventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static JSValue RTCDataChannelBase_send(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
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

static JSValue RTCDataChannelBase_isOpen(JSContext *ctx, JSValueConst this_val)
{   
    RTCDataChannelBase_ClassData *state = getRTCDataChannelClassData(this_val);
    int isOpen = rtcIsOpen(state->channelId);
    return JS_NewBool(ctx, isOpen); 
}

static JSCFunctionListEntry RTCDataChannelBase_Methods[] = {
    JS_CFUNC_DEF("send", 1, RTCDataChannelBase_send),
    JS_CGETSET_DEF("isOpen", RTCDataChannelBase_isOpen, NULL),
    JS_CGETSET_MAGIC_DEF("onopen", 
        RTCDataChannelBase_EventGet, 
        RTCDataChannelBase_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONOPEN),
    JS_CGETSET_MAGIC_DEF("onclose", 
        RTCDataChannelBase_EventGet, 
        RTCDataChannelBase_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONCLOSE),
    JS_CGETSET_MAGIC_DEF("onmessage", 
        RTCDataChannelBase_EventGet, 
        RTCDataChannelBase_EventSet, 
        RTC_DATACHANNEL_EVENTS_ONMESSAGE)
};

JSFullClassDef RTCDataChannelBase_Class = {
    .def = {
        .class_name = "RTCDataChannelBase",
        .finalizer = RTCDataChannelBase_Finalizer,
        .gc_mark = RTCDataChannelBase_GcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(RTCDataChannelBase_Methods),
    .funcs = RTCDataChannelBase_Methods
};

JSValue createRTCDataChannelBaseClass(JSContext *ctx, int channelId, JSClassFinalizer *finalizer) {
    JSValue obj = JS_NewObjectClass(ctx, RTCDataChannelBase_Class.id); 
    RTCDataChannelBase_ClassData *state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->channelId = channelId;
    state->thisObj = obj;
    state->finalizer = finalizer;
    for (int i = 0 ; i < RTC_DATACHANNEL_EVENTS_MAX; i++) 
        state->events[i] = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    rtcSetUserPointer(channelId, state);
    rtcSetOpenCallback(channelId, handleOnOpen);
    rtcSetClosedCallback(channelId, handleOnClose);
    rtcSetMessageCallback(channelId, handleOnMessage);
    return obj;
}