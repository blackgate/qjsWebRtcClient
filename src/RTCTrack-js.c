#include "RTCDataChannelBase-js.h"
#include "RTCTrack-js.h"
#include "event-queue.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void RTCTrack_Finalizer(JSRuntime *rt, JSValue val) {
    int trackId = getRTCDataChannelId(val);
    rtcDeleteTrack(trackId);
}

static JSValue RTCTrack_setStartTime(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    int trackId = getRTCDataChannelId(this_val);
    rtcStartTime startTime = {
        .seconds = 0,
        .since1970 = true,
        .timestamp = rand()
    };
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid argument");
    JS_ToFloat64(ctx, &startTime.seconds, argv[0]);
    if (rtcSetRtpConfigurationStartTime(trackId, &startTime) < 0)
        return JS_ThrowInternalError(ctx, "setStartTime failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_startRecording(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    int trackId = getRTCDataChannelId(this_val);
    if (rtcStartRtcpSenderReporterRecording(trackId) < 0)
        return JS_ThrowInternalError(ctx, "startRecording failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_setNeedsToReport(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    int trackId = getRTCDataChannelId(this_val);
    if (rtcSetNeedsToSendRtcpSr(trackId) < 0)
        return JS_ThrowInternalError(ctx, "setNeedsToReport failed");
    return JS_UNDEFINED;
}

static JSValue RTCTrack_secondsToTimestamp(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    int trackId = getRTCDataChannelId(this_val);
    double secs;
    uint32_t timestamp;
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid seconds argument");
    JS_ToFloat64(ctx, &secs, argv[0]);
    if (rtcTransformSecondsToTimestamp(trackId, secs, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "secondsToTimestamp failed");
    return JS_NewUint32(ctx, timestamp);
}

static JSValue RTCTrack_timestampToSeconds(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    int trackId = getRTCDataChannelId(this_val);
    double secs;
    uint32_t timestamp;
    if (argc == 0 || !JS_IsNumber(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid timestamp argument");
    JS_ToUint32(ctx, &timestamp, argv[0]);
    if (rtcTransformTimestampToSeconds(trackId, timestamp, &secs) < 0)
        return JS_ThrowInternalError(ctx, "timestampToSeconds failed");
    return JS_NewFloat64(ctx, secs);
}

static JSValue RTCTrack_getStartTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    int trackId = getRTCDataChannelId(this_val);
    uint32_t timestamp;
    if (rtcGetTrackStartTimestamp(trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get startTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_getPreviousReportedTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    int trackId = getRTCDataChannelId(this_val);
    uint32_t timestamp;
    if (rtcGetPreviousTrackSenderReportTimestamp(trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get previousReportedTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_getCurrentTimestamp(JSContext *ctx, JSValueConst this_val)
{   
    int trackId = getRTCDataChannelId(this_val);
    uint32_t timestamp;
    if (rtcGetCurrentTrackTimestamp(trackId, &timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to get currentTimestamp");
    return JS_NewUint32(ctx, timestamp); 
}

static JSValue RTCTrack_setCurrentTimestamp(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{   
    int trackId = getRTCDataChannelId(this_val);
    uint32_t timestamp;
    if (!JS_IsNumber(value))
        return JS_ThrowTypeError(ctx, "Invalid timestamp value");
    JS_ToUint32(ctx, &timestamp, value);
    if (rtcSetTrackRtpTimestamp(trackId, timestamp) < 0)
        return JS_ThrowInternalError(ctx, "Failed to set currentTimestamp");
    return JS_UNDEFINED; 
}

static JSCFunctionListEntry RTCTrack_Methods[] = {
    JS_CFUNC_DEF("setStartTime", 1, RTCTrack_setStartTime),
    JS_CFUNC_DEF("startRecording", 0, RTCTrack_startRecording),
    JS_CFUNC_DEF("setNeedsToReport", 0, RTCTrack_setNeedsToReport),
    JS_CFUNC_DEF("secondsToTimestamp", 1, RTCTrack_secondsToTimestamp),
    JS_CFUNC_DEF("timestampToSeconds", 1, RTCTrack_timestampToSeconds),
    JS_CGETSET_DEF("startTimestamp", RTCTrack_getStartTimestamp, NULL),
    JS_CGETSET_DEF("previousReportedTimestamp", RTCTrack_getPreviousReportedTimestamp, NULL),
    JS_CGETSET_DEF("currentTimestamp", RTCTrack_getCurrentTimestamp, RTCTrack_setCurrentTimestamp)
};

JSValue createRTCTrackClass(JSContext *ctx, int trackId) {
    JSValue obj = createRTCDataChannelBaseClass(ctx, trackId, RTCTrack_Finalizer);
    JS_SetPropertyFunctionList(ctx, obj, RTCTrack_Methods, countof(RTCTrack_Methods));
    return obj;
}