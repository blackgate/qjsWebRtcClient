#ifndef __RTC_TRACK_JS_H
#define __RTC_TRACK_JS_H

#include "js-utils.h"
#include <rtc/rtc.h>

extern JSFullClassDef RTCTrack_Class;
JSValue createRTCTrackClass(JSContext *ctx, int trackId);

#endif