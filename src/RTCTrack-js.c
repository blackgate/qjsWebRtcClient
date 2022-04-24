#include "RTCTrack-js.h"
#include "event-queue.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    RTC_TRACK_EVENTS_ONOPEN,
    RTC_TRACK_EVENTS_ONCLOSE,
    RTC_TRACK_EVENTS_MAX,
};

typedef struct {
    JSContext *ctx;
    int trackId;
    JSValue thisObj;
    JSValue events[RTC_TRACK_EVENTS_MAX];
} RTCTrack_ClassData;

static RTCTrack_ClassData* getRTCTrackClassData(JSValueConst this_val) {
    return JS_GetOpaque(this_val, RTCTrack_Class.id);
}

static void RTCTrack_onOpen(JSContext *ctx, JSValue this_val, void *data) {
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    JSValue fn = state->events[RTC_TRACK_EVENTS_ONOPEN];
    if (JS_IsFunction(state->ctx, fn))
        JS_Call(state->ctx, fn, this_val, 0, NULL);
}

static void RTCTrack_onClose(JSContext *ctx, JSValue this_val, void *data) {
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    JSValue fn = state->events[RTC_TRACK_EVENTS_ONCLOSE];
    if (JS_IsFunction(state->ctx, fn))
        JS_Call(state->ctx, fn, this_val, 0, NULL);
    JS_FreeValue(ctx, state->thisObj); // we don't need to keep the object alive anymore
}

static void handleOnOpen(int trackId, void *ptr) {
    RTCTrack_ClassData *state = (RTCTrack_ClassData *)ptr;
    enqueueEvent(RTCTrack_onOpen, state->thisObj, NULL);
}

static void handleOnClose(int trackId, void *ptr) {
    RTCTrack_ClassData *state = (RTCTrack_ClassData *)ptr;
    enqueueEvent(RTCTrack_onClose, state->thisObj, NULL);
}

JSValue createRTCTrackClass(JSContext *ctx, int trackId) {
    JSValue obj = JS_NewObjectClass(ctx, RTCTrack_Class.id); 
    RTCTrack_ClassData *state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    state->trackId = trackId;
    state->thisObj = JS_DupValue(ctx, obj); // keep the object alive until it's closed
    for (int i = 0 ; i < RTC_TRACK_EVENTS_MAX; i++) 
        state->events[i] = JS_UNDEFINED;
    JS_SetOpaque(obj, state);
    rtcSetUserPointer(trackId, state);
    rtcSetOpenCallback(trackId, handleOnOpen);
    rtcSetClosedCallback(trackId, handleOnClose);
    return obj;
}

static void RTCTrack_Finalizer(JSRuntime *rt, JSValue val) {
    RTCTrack_ClassData *state = getRTCTrackClassData(val);
    for (int i = 0 ; i < RTC_TRACK_EVENTS_MAX; i++)
        JS_FreeValueRT(rt, state->events[i]);
    rtcDeleteTrack(state->trackId);
    js_free(state->ctx, state);
}

static void RTCTrack_GcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    RTCTrack_ClassData *state = getRTCTrackClassData(val);
    if (state) {
        for (int i = 0 ; i < RTC_TRACK_EVENTS_MAX; i++)
            JS_MarkValue(rt, state->events[i], mark_func);
    }
}

static JSValue RTCTrack_EventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue RTCTrack_EventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static JSValue RTCTrack_send(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    size_t len;
    char *val = JS_GetArrayBuffer(ctx, &len, argv[0]);
    if (rtcSendMessage(state->trackId, val, len) < 0)
        return JS_ThrowInternalError(ctx, "Error sending data");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_setStartTime(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    rtcStartTime startTime = {
        .seconds = 0,
        .since1970 = true,
        .timestamp = rand()
    };
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid argument");
    JS_ToFloat64(ctx, &startTime.seconds, argv[0]);
    if (rtcSetRtpConfigurationStartTime(state->trackId, &startTime) < 0)
        return JS_ThrowInternalError(ctx, "setStartTime failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_startRecording(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    if (rtcStartRtcpSenderReporterRecording(state->trackId) < 0)
        return JS_ThrowInternalError(ctx, "startRecording failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_setNeedsToReport(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    if (rtcSetNeedsToSendRtcpSr(state->trackId) < 0)
        return JS_ThrowInternalError(ctx, "setNeedsToReport failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_secondsToTimestamp(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    double secs;
    uint32_t timestamp;
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid seconds argument");
    JS_ToFloat64(ctx, &secs, argv[0]);
    if (rtcTransformSecondsToTimestamp(state->trackId, secs, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "secondsToTimestamp failed");
    return JS_NewUint32(ctx, timestamp);
}

static JSValue RTCTrack_timestampToSeconds(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    double secs;
    uint32_t timestamp;
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid timestamp argument");
    JS_ToUint32(ctx, &timestamp, argv[0]);
    if (rtcTransformTimestampToSeconds(state->trackId, timestamp, &secs) < 0)
        return JS_ThrowInternalError(ctx, "timestampToSeconds failed");
    return JS_NewFloat64(ctx, secs);
}

static JSValue RTCTrack_getStartTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    uint32_t timestamp;
    if (rtcGetTrackStartTimestamp(state->trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get startTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_getPreviousReportedTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    uint32_t timestamp;
    if (rtcGetPreviousTrackSenderReportTimestamp(state->trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get previousReportedTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_getCurrentTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    uint32_t timestamp;
    if (rtcGetCurrentTrackTimestamp(state->trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get currentTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_setCurrentTimestamp(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{   
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    uint32_t timestamp;
    if (!JS_IsNumber(value))
        return JS_ThrowTypeError(ctx, "Invalid timestamp value");
    JS_ToUint32(ctx, &timestamp, value);
    if (rtcSetTrackRtpTimestamp(state->trackId, timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to set currentTimestamp");
    return JS_UNDEFINED; 
}

static JSValue RTCTrack_isOpen(JSContext *ctx, JSValueConst this_val)
{   
    RTCTrack_ClassData *state = getRTCTrackClassData(this_val);
    int isOpen = rtcIsOpen(state->trackId);
    return JS_NewBool(ctx, isOpen); 
}

static JSCFunctionListEntry RTCTrack_Methods[] = {
    JS_CFUNC_DEF("send", 1, RTCTrack_send),
    JS_CFUNC_DEF("setStartTime", 1, RTCTrack_setStartTime),
    JS_CFUNC_DEF("startRecording", 0, RTCTrack_startRecording),
    JS_CFUNC_DEF("setNeedsToReport", 0, RTCTrack_setNeedsToReport),
    JS_CFUNC_DEF("secondsToTimestamp", 1, RTCTrack_secondsToTimestamp),
    JS_CFUNC_DEF("timestampToSeconds", 1, RTCTrack_timestampToSeconds),
    JS_CGETSET_DEF("startTimestamp", RTCTrack_getStartTimestamp, NULL),
    JS_CGETSET_DEF("previousReportedTimestamp", RTCTrack_getPreviousReportedTimestamp, NULL),
    JS_CGETSET_DEF("currentTimestamp", RTCTrack_getCurrentTimestamp, RTCTrack_setCurrentTimestamp),
    JS_CGETSET_DEF("isOpen", RTCTrack_isOpen, NULL),
    JS_CGETSET_MAGIC_DEF("onopen", 
        RTCTrack_EventGet, 
        RTCTrack_EventSet, 
        RTC_TRACK_EVENTS_ONOPEN),
    JS_CGETSET_MAGIC_DEF("onclose", 
        RTCTrack_EventGet, 
        RTCTrack_EventSet, 
        RTC_TRACK_EVENTS_ONCLOSE)
};

JSFullClassDef RTCTrack_Class = {
    .def = {
        .class_name = "RTCTrack",
        .finalizer = RTCTrack_Finalizer,
        .gc_mark = RTCTrack_GcMark,
    },
    .constructor = { NULL, 0 },
    .funcs_len = sizeof(RTCTrack_Methods),
    .funcs = RTCTrack_Methods
};