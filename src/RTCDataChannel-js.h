#ifndef __RTC_DATA_CHANNEL_JS_H
#define __RTC_DATA_CHANNEL_JS_H

#include "js-utils.h"
#include <rtc/rtc.h>

extern JSFullClassDef RTCDataChannel_Class;
JSValue createRTCDataChannelClass(JSContext *ctx, int channelId);

#endif