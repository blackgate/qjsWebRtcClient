#ifndef __RTC_DATA_CHANNEL_BASE_JS_H
#define __RTC_DATA_CHANNEL_BASE_JS_H

#include "js-utils.h"

extern JSFullClassDef RTCDataChannelBase_Class;
int getRTCDataChannelId(JSValueConst this_val);
JSValue createRTCDataChannelBaseClass(JSContext *ctx, int channelId, JSClassFinalizer *finalizer);

#endif