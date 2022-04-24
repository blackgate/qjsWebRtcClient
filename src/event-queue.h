#include <quickjs/quickjs.h>

typedef void(*eventHandler)(JSContext *ctx, JSValue obj, void *data);

void enqueueEvent(eventHandler handler, JSValue obj, void *data);
void flushEvents(JSContext *ctx);
void initEventQueue(JSContext *ctx);