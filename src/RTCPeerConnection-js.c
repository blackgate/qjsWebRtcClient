#include "RTCPeerConnection-js.h"
#include "RTCDataChannel-js.h"
#include "RTCTrack-js.h"
#include <rtc/rtc.h>
#include "event-queue.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

enum {
    RTC_PEER_CONNECTION_EVENTS_ONICECANDIDATE,
    RTC_PEER_CONNECTION_EVENTS_ONLOCALDESCRIPTION,
    RTC_PEER_CONNECTION_EVENTS_ONICEGATHERINGSTATECHANGE,
    RTC_PEER_CONNECTION_EVENTS_ONDATACHANNEL,
    RTC_PEER_CONNECTION_EVENTS_MAX,
};

typedef struct {
    char *cand;
    char *mid;
} Candidate;

typedef struct {
    char *type;
    char *sdp;
} SessionDescription;

typedef struct {
    const char *cname;
    const char *msid;
    rtcCodec codec;
    rtcDirection direction;
    rtcNalUnitSeparator nalUnitSeparator;
    int ssrc;        // Sincronization source id
    int payloadType; // 96 - 127 => dynamic
} AddTrackOptions;

typedef struct {
    JSContext *ctx;
    int peerConn;
    JSValue thisObj;
    JSValue events[RTC_PEER_CONNECTION_EVENTS_MAX];
} RTCPeerConnection_ClassData;

static RTCPeerConnection_ClassData* getRTCPeerConnectionClassData(JSValueConst this_val) {
    return JS_GetOpaque(this_val, RTCPeerConnection_Class.id);
}

static JSValue parseIceServerValue(JSContext *ctx, JSValue iceServerVal, const char** iceServer) {
    if (!JS_IsObject(iceServerVal)) 
        return JS_ThrowTypeError(ctx, "The iceServers item should be an object");
    JSValue urlsVal = JS_GetPropertyStr(ctx, iceServerVal, "urls");
    *iceServer = JS_ToCString(ctx, urlsVal);
    JS_FreeValue(ctx, urlsVal);
    return JS_UNDEFINED;
}

static JSValue parseIceServersValue(JSContext *ctx, JSValue iceServersVal, rtcConfiguration *config) {
    if (!JS_IsArray(ctx, iceServersVal)) 
        return JS_ThrowTypeError(ctx, "The iceServers property should be an array");
    uint32_t len = JS_GetArrayLength(ctx, iceServersVal);
    config->iceServers = js_mallocz(ctx, sizeof(char*[len]));
    config->iceServersCount = len;
    for (int i = 0 ; i < len ; i++) {
        JSValue iceServer = JS_GetPropertyUint32(ctx, iceServersVal, i);
        JSValue ret = parseIceServerValue(ctx, iceServer, &(config->iceServers[i]));
        if (JS_IsException(ret)) return ret;
        JS_FreeValue(ctx, iceServer);
    }
    return JS_UNDEFINED;
}

static JSValue parseConfigurationValue(JSContext *ctx, JSValue configVal, rtcConfiguration *config) {
    if (!JS_IsObject(configVal)) 
        return JS_ThrowTypeError(ctx, "The configuration parameter should be an object");
    JSValue iceServers = JS_GetPropertyStr(ctx, configVal, "iceServers");
    parseIceServersValue(ctx, iceServers, config);
    JS_FreeValue(ctx, iceServers);
    return JS_UNDEFINED;
}

static JSValue candidateToJSValue(JSContext *ctx, Candidate *candidate) {
    if (candidate == NULL) return JS_NULL;
    JSValue ret = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ret, "candidate", JS_NewString(ctx, candidate->cand));
    JS_SetPropertyStr(ctx, ret, "mid", JS_NewString(ctx, candidate->mid));
    return ret;
}

static void jsValueToCandidate(JSContext *ctx, JSValue obj, Candidate *candidate) {
    JSValue candJsVal = JS_GetPropertyStr(ctx, obj, "candidate");
    JSValue midJsVal = JS_GetPropertyStr(ctx, obj, "mid");
    candidate->cand = (char*)JS_ToCString(ctx, candJsVal);
    candidate->mid = (char*)JS_ToCString(ctx, midJsVal);
    JS_FreeValue(ctx, candJsVal);
    JS_FreeValue(ctx, midJsVal);
}

static void freeCandidate(Candidate *candidate) {
    free(candidate->cand);
    free(candidate->mid);
}

static void fromJsRtcSessionDescription(JSContext *ctx, JSValue desc, SessionDescription *sessionDesc) {
    JSValue typeJsVal = JS_GetPropertyStr(ctx, desc, "type");
    JSValue sdpJsVal = JS_GetPropertyStr(ctx, desc, "sdp");
    sessionDesc->type = (char*)JS_ToCString(ctx, typeJsVal);
    sessionDesc->sdp = (char*)JS_ToCString(ctx, sdpJsVal);
    JS_FreeValue(ctx, typeJsVal);
    JS_FreeValue(ctx, sdpJsVal);
}

static JSValue toJsRtcSessionDescription(JSContext *ctx, SessionDescription *sessionDesc) {
    JSValue desc = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, desc, "type", JS_NewString(ctx, sessionDesc->type));
    JS_SetPropertyStr(ctx, desc, "sdp", JS_NewString(ctx, sessionDesc->sdp));
    return desc;
}

static void freeSessionDescription(SessionDescription *sessionDesc) {
    free(sessionDesc->type);
    free(sessionDesc->sdp);
}

static void RTCPeerConnection_onIceCandidate(JSContext *ctx, JSValue this_val, void *data) {
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    Candidate *candidate = (Candidate *)data;
    JSValue fn = state->events[RTC_PEER_CONNECTION_EVENTS_ONICECANDIDATE];
    if (JS_IsFunction(ctx, fn)) { 
        JSValue candidateVal = candidateToJSValue(ctx, candidate);
        JS_Call(ctx, fn, this_val, 1, &candidateVal);
        JS_FreeValue(ctx, candidateVal);
    }
    freeCandidate(candidate);
}

static void RTCPeerConnection_onLocalDescription(JSContext *ctx, JSValue this_val, void *data) {
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    SessionDescription *desc = (SessionDescription *)data;
    JSValue fn = state->events[RTC_PEER_CONNECTION_EVENTS_ONLOCALDESCRIPTION];
    if (JS_IsFunction(ctx, fn)) { 
        JSValue descVal = toJsRtcSessionDescription(ctx, desc);
        JS_Call(ctx, fn, this_val, 1, &descVal);
        JS_FreeValue(ctx, descVal);
    }
    freeSessionDescription(desc);
}

static JSValue gatheringStateToJsValue(JSContext *ctx, rtcGatheringState state) {
    switch (state)
    {
        case RTC_GATHERING_NEW:
            return JS_NewString(ctx, "new");
        case RTC_GATHERING_INPROGRESS:
            return JS_NewString(ctx, "gathering");
        case RTC_GATHERING_COMPLETE:
            return JS_NewString(ctx, "complete");
        default:
            return JS_NULL;
    }
}

static void RTCPeerConnection_onIceGatheringStateChange(JSContext *ctx, JSValue this_val, void *data) {
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    rtcGatheringState *pGatheringState = (rtcGatheringState *)data;
    JSValue fn = state->events[RTC_PEER_CONNECTION_EVENTS_ONICEGATHERINGSTATECHANGE];
    if (JS_IsFunction(ctx, fn)) { 
        JSValue arg = gatheringStateToJsValue(ctx, *pGatheringState);
        JS_Call(ctx, fn, this_val, 1, &arg);
        JS_FreeValue(ctx, arg);
    }
}

static void RTCPeerConnection_onDataChannel(JSContext *ctx, JSValue this_val, void *data) {
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    int *channelId = data;
    JSValue fn = state->events[RTC_PEER_CONNECTION_EVENTS_ONDATACHANNEL];
    if (JS_IsFunction(ctx, fn)) { 
        JSValue channelVal = createRTCDataChannelClass(ctx, *channelId);
        JS_Call(ctx, fn, this_val, 1, &channelVal);
        JS_FreeValue(ctx, channelVal);
    }
}

static void handleOnIceCandidate(int pc, const char *cand, const char *mid, void *ptr) {
    RTCPeerConnection_ClassData *state = (RTCPeerConnection_ClassData *)ptr;
    Candidate *candidate = NULL;
    if (cand != NULL) {
        candidate = malloc(sizeof(Candidate));
        candidate->cand = strdup(cand);
        candidate->mid = strdup(mid);
    }
    enqueueEvent(RTCPeerConnection_onIceCandidate, state->thisObj, (void *)candidate);
}

static void handleOnLocalDescription(int pc, const char *sdp, const char *type, void *ptr) {
    RTCPeerConnection_ClassData *state = (RTCPeerConnection_ClassData *)ptr;
    SessionDescription *desc = malloc(sizeof(SessionDescription));
    desc->type = strdup(type);
    desc->sdp = strdup(sdp);
    enqueueEvent(RTCPeerConnection_onLocalDescription, state->thisObj, (void *)desc);
}

static void handleOnIceGatheringStateChange(int pc, rtcGatheringState state, void *ptr) {
    RTCPeerConnection_ClassData *classState = (RTCPeerConnection_ClassData *)ptr;
    rtcGatheringState *pGatheringState = malloc(sizeof(rtcGatheringState));
    *pGatheringState = state;
    enqueueEvent(RTCPeerConnection_onIceGatheringStateChange, classState->thisObj, (void *)pGatheringState);
}

static void handleOnDataChannel(int pc, int dc, void *ptr) {
    RTCPeerConnection_ClassData *classState = (RTCPeerConnection_ClassData *)ptr;
    int *channelId = malloc(sizeof(int));
    *channelId = dc;
    enqueueEvent(RTCPeerConnection_onDataChannel, classState->thisObj, (void *)channelId);
}

static JSValue RTCPeerConnection_GetInternalConnection(
    JSContext *ctx, int *pcId,
    int argc, JSValueConst *argv) 
{
    rtcConfiguration config;
    int pc;
    memset(&config, 0, sizeof(rtcConfiguration));
    if (argc > 0) {
        JSValue out = parseConfigurationValue(ctx, argv[0], &config);
        if (JS_IsException(out)) return out;
    }
    config.disableAutoNegotiation = true;
    pc = rtcCreatePeerConnection(&config);
    if (pc < 0)
        return JS_ThrowInternalError(ctx, "Peer connection creation failed. Status code: %x", pc);
    *pcId = pc;
    return JS_UNDEFINED;
}

static JSValue RTCPeerConnection_Constructor(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    JSValue obj = JS_NewObjectClass(ctx, RTCPeerConnection_Class.id);
    RTCPeerConnection_ClassData *state;
    state = js_mallocz(ctx, sizeof(*state));
    state->ctx = ctx;
    JSValue res = RTCPeerConnection_GetInternalConnection(ctx, &state->peerConn, argc, argv);
    if (JS_IsException(res)) {
        js_free(ctx, state);
        JS_FreeValue(ctx, obj);
        return res;
    }
    for (int i = 0 ; i < RTC_PEER_CONNECTION_EVENTS_MAX; i++) 
        state->events[i] = JS_UNDEFINED;
    rtcSetUserPointer(state->peerConn, state);
    rtcSetLocalCandidateCallback(state->peerConn, handleOnIceCandidate);
    rtcSetLocalDescriptionCallback(state->peerConn, handleOnLocalDescription);
    rtcSetGatheringStateChangeCallback(state->peerConn, handleOnIceGatheringStateChange);
    rtcSetDataChannelCallback(state->peerConn, handleOnDataChannel);
    state->thisObj = obj;
    JS_SetOpaque(obj, state);
    return obj;
}

static void RTCPeerConnection_Finalizer(JSRuntime *rt, JSValue val) 
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(val);
    if (state) {
        rtcDeletePeerConnection(state->peerConn);
        for (int i = 0 ; i < RTC_PEER_CONNECTION_EVENTS_MAX; i++)
            JS_FreeValueRT(rt, state->events[i]);
        js_free(state->ctx, state);
    }
}

static void RTCPeerConnection_GcMark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) 
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(val);
    if (state) {
        for (int i = 0 ; i < RTC_PEER_CONNECTION_EVENTS_MAX; i++)
            JS_MarkValue(rt, state->events[i], mark_func);
    }
}

static JSValue RTCPeerConnection_createOffer(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    int status = rtcSetLocalDescription(state->peerConn, "offer");
    if (status < 0)
        return JS_ThrowInternalError(ctx, "Error creating the offer. Status code %x", status);
    return JS_UNDEFINED; 
}

static JSValue RTCPeerConnection_createAnswer(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    int status = rtcSetLocalDescription(state->peerConn, "answer");
    if (status < 0)
        return JS_ThrowInternalError(ctx, "Error creating the answer. Status code %x", status);
    return JS_UNDEFINED; 
}

static JSValue RTCPeerConnection_setRemoteDescription(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{   
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    SessionDescription sessionDesc;
    fromJsRtcSessionDescription(ctx, argv[0], &sessionDesc);
    int status = rtcSetRemoteDescription(state->peerConn, sessionDesc.sdp, sessionDesc.type);
    if (status < 0)
        return JS_ThrowInternalError(ctx, "Error setting remote description. Status code: %x", status);
    //freeSessionDescription(&sessionDesc);
    return JS_UNDEFINED; 
}

static JSValue RTCPeerConnection_addIceCandidate(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{   
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    Candidate candidate;
    jsValueToCandidate(ctx, argv[0], &candidate);
    int status = rtcAddRemoteCandidate(state->peerConn, candidate.cand, candidate.mid);
    if (status < 0)
        return JS_ThrowInternalError(ctx, "Error adding ice candidate. Status code: %x", status);
    //freeCandidate(&candidate);
    return JS_UNDEFINED; 
}

static JSValue RTCPeerConnection_getLocalDescription(JSContext *ctx, JSValueConst this_val)
{   
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    SessionDescription sessionDesc;
    int sdpLen = rtcGetLocalDescription(state->peerConn, NULL, 0);
    int typeLen = rtcGetLocalDescriptionType(state->peerConn, NULL, 0);
    if (sdpLen < 0)
        return JS_ThrowInternalError(ctx, "Error getting local description. Status code: %x", sdpLen);
    if (typeLen < 0)
        return JS_ThrowInternalError(ctx, "Error getting local description type. Status code: %x", typeLen);
    sessionDesc.sdp = js_malloc(ctx, sdpLen + 1);
    sessionDesc.type = js_malloc(ctx, typeLen + 1);
    rtcGetLocalDescription(state->peerConn, sessionDesc.sdp, sdpLen);
    rtcGetLocalDescriptionType(state->peerConn, sessionDesc.type, typeLen);
    return toJsRtcSessionDescription(ctx, &sessionDesc); 
}

static JSValue RTCPeerConnection_createDataChannel(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    const char *name = JS_ToCString(ctx, argv[0]);
    int channelId = rtcCreateDataChannel(state->peerConn, name);
    if (channelId < 0)
        return JS_ThrowInternalError(ctx, "Error creating data channel. Status code: %x", channelId);
    //JS_FreeCString(ctx, name);
    return createRTCDataChannelClass(ctx, channelId);
}

static int JS_GetInt32Prop(JSContext *ctx, JSValueConst thisObj, const char *prop, int *res) {
    JSValue val = JS_GetPropertyStr(ctx, thisObj, prop);
    int status = JS_ToInt32(ctx, res, val);
    JS_FreeValue(ctx, val);
    return status;
}

static const char* JS_GetCStringProp(JSContext *ctx, JSValueConst thisObj, const char *prop) {
    JSValue val = JS_GetPropertyStr(ctx, thisObj, prop);
    const char *str = JS_ToCString(ctx, val);
    JS_FreeValue(ctx, val);
    return str;
}

static JSValue fromJsAddTrackOptions(JSContext *ctx, JSValue val, AddTrackOptions *opts) {
    if ((opts->cname = JS_GetCStringProp(ctx, val, "cname")) == NULL)
        return JS_ThrowTypeError(ctx, "Invalid cname value");
    if ((opts->msid = JS_GetCStringProp(ctx, val, "msid")) == NULL)
        return JS_ThrowTypeError(ctx, "Invalid msid value");
    if (JS_GetInt32Prop(ctx, val, "codec",(int*) &opts->codec))
        return JS_ThrowTypeError(ctx, "Invalid codec value");
    if (JS_GetInt32Prop(ctx, val, "direction", (int*)&opts->direction))
        return JS_ThrowTypeError(ctx, "Invalid direction value");
    if (JS_GetInt32Prop(ctx, val, "nalUnitSeparator", (int*)&opts->nalUnitSeparator))
        return JS_ThrowTypeError(ctx, "Invalid nalUnitSeparator value");
    if (JS_GetInt32Prop(ctx, val, "ssrc", &opts->ssrc))
        return JS_ThrowTypeError(ctx, "Invalid ssrc value");
    if (JS_GetInt32Prop(ctx, val, "payloadType", &opts->payloadType))
        return JS_ThrowTypeError(ctx, "Invalid payloadType value");
}

static int addTrackWithOptions(int peerConn, AddTrackOptions *opts) {
    rtcTrackInit trackInit = { 
        .codec = opts->codec, 
        .direction = opts->direction,
        .payloadType = opts->payloadType, 
        .ssrc = opts->ssrc, 
        .mid = opts->cname,
        .name = opts->cname, 
        .msid = opts->msid, 
        .trackId = opts->cname
    };
    return rtcAddTrackEx(peerConn, &trackInit);
}

static int setTrackPacketizationHandler(int trackId, AddTrackOptions *opts) {
    rtcPacketizationHandlerInit handler = {
        .payloadType = opts->payloadType,
        .clockRate = 90 * 1000, // default rtp clockrate
        .cname = opts->cname,
        .nalSeparator = opts->nalUnitSeparator, // RTC_NAL_SEPARATOR_LENGTH,
        .ssrc = opts->ssrc,
        .sequenceNumber = 0,
        .maxFragmentSize = 0,
        .timestamp = rand()
    };
    return rtcSetH264PacketizationHandler(trackId, &handler);
}

static JSValue RTCPeerConnection_addTrack(
    JSContext *ctx, JSValueConst this_val,
    int argc, JSValueConst *argv)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    AddTrackOptions opts;
    JSValue convRes;
    int trackId;
    if (argc == 0 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "Invalid argument");
    convRes = fromJsAddTrackOptions(ctx, argv[0], &opts);
    if (JS_IsException(convRes))
        return convRes;
    trackId = addTrackWithOptions(state->peerConn, &opts);
    if (trackId < 0)
        return JS_ThrowInternalError(ctx, "Error adding track. Status code: %x", trackId);
    if (setTrackPacketizationHandler(trackId, &opts) < 0)
        return JS_ThrowInternalError(ctx, "Error setting up packetization handler");
    rtcChainRtcpSrReporter(trackId);
    rtcChainRtcpNackResponder(trackId, 128);
    return createRTCTrackClass(ctx, trackId);
}

static JSValue RTCPeerConnection_EventGet(
    JSContext *ctx, JSValueConst this_val, int magic)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    return JS_DupValue(ctx, state->events[magic]);
}

static JSValue RTCPeerConnection_EventSet(
    JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
    RTCPeerConnection_ClassData *state = getRTCPeerConnectionClassData(this_val);
    JSValue ev = state->events[magic];
    if (!JS_IsUndefined(ev)) JS_FreeValue(ctx, ev);
    if (JS_IsFunction(ctx, value))
        state->events[magic] = JS_DupValue(ctx, value);
    else
        state->events[magic] = JS_UNDEFINED;
    return JS_UNDEFINED;
}

static JSCFunctionListEntry RTCPeerConnection_Methods[] = {
    JS_CFUNC_DEF("createOffer", 0, RTCPeerConnection_createOffer),
    JS_CFUNC_DEF("createAnswer", 0, RTCPeerConnection_createAnswer),
    JS_CGETSET_DEF("localDescription", RTCPeerConnection_getLocalDescription, NULL),
    JS_CFUNC_DEF("setRemoteDescription", 1, RTCPeerConnection_setRemoteDescription),
    JS_CFUNC_DEF("addIceCandidate", 1, RTCPeerConnection_addIceCandidate),
    JS_CFUNC_DEF("createDataChannel", 1, RTCPeerConnection_createDataChannel),
    JS_CFUNC_DEF("addTrack", 1, RTCPeerConnection_addTrack),
    JS_CGETSET_MAGIC_DEF("onicecandidate", 
        RTCPeerConnection_EventGet, 
        RTCPeerConnection_EventSet, 
        RTC_PEER_CONNECTION_EVENTS_ONICECANDIDATE),
    JS_CGETSET_MAGIC_DEF("onlocaldescription", 
        RTCPeerConnection_EventGet, 
        RTCPeerConnection_EventSet, 
        RTC_PEER_CONNECTION_EVENTS_ONLOCALDESCRIPTION),
    JS_CGETSET_MAGIC_DEF("onicegatheringstatechange", 
        RTCPeerConnection_EventGet, 
        RTCPeerConnection_EventSet, 
        RTC_PEER_CONNECTION_EVENTS_ONICEGATHERINGSTATECHANGE),
    JS_CGETSET_MAGIC_DEF("ondatachannel", 
        RTCPeerConnection_EventGet, 
        RTCPeerConnection_EventSet, 
        RTC_PEER_CONNECTION_EVENTS_ONDATACHANNEL)
};

JSFullClassDef RTCPeerConnection_Class = {
    .def = {
        .class_name = "RTCPeerConnection",
        .finalizer = RTCPeerConnection_Finalizer,
        .gc_mark = RTCPeerConnection_GcMark,
    },
    .constructor = { RTCPeerConnection_Constructor, .args_count = 1 },
    .funcs_len = sizeof(RTCPeerConnection_Methods),
    .funcs = RTCPeerConnection_Methods
};