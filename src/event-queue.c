#include "event-queue.h"
#include <quickjs/quickjs-libc.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    eventHandler handler;
    JSValue obj;
    void *data;
} eventData;

typedef struct s_eventQueueNode {
    eventData *value;
    struct s_eventQueueNode *next;
} eventQueueNode;

typedef struct {
    eventQueueNode *head;
    eventQueueNode *tail;
    pthread_mutex_t lock;
    int eventPipe[2];
} eventQueue;

static eventQueue *eventQueuePtr = NULL;

static void freeEventData(eventData *event) {
    free(event->data);
    free(event);
}

static eventData *createEventData(eventHandler handler, JSValue obj, void *data) {
    eventData *event = malloc(sizeof(eventData));
    event->handler = handler;
    event->obj = obj;
    event->data = data;
    return event;
}

static eventQueueNode *createEventQueueNode(eventData *value) {
    eventQueueNode *eventNode = malloc(sizeof(eventQueueNode));
    eventNode->value = value;
    eventNode->next = NULL;
    return eventNode;
}

static void enqueueEventInternal(eventQueue *eventQueue, eventData *event) {
    eventQueueNode *queueNode = createEventQueueNode(event);
    pthread_mutex_lock(&eventQueue->lock);
    if (eventQueue->head == NULL)
        eventQueue->head = queueNode;
    else
        eventQueue->tail->next = queueNode;
    eventQueue->tail = queueNode;
    write(eventQueue->eventPipe[1], "1", 1); // dummy value
    pthread_mutex_unlock(&eventQueue->lock);
}

static int hasEvents(eventQueue *eventQueue) {
    return eventQueue->head != NULL;
}

static eventData *dequeueEvent(eventQueue *eventQueue) {
    eventQueueNode *eventNode;
    eventData *event = NULL;
    pthread_mutex_lock(&eventQueue->lock);
    eventNode = eventQueue->head;
    if (eventNode != NULL) {
        eventQueue->head = eventNode->next;
        event = eventNode->value;
        free(eventNode);
    }
    pthread_mutex_unlock(&eventQueue->lock);
    return event;
}

static eventQueue *createEventQueue() {
    eventQueue *queue;
    queue = malloc(sizeof(*queue));
    queue->head = NULL;
    queue->tail = NULL;
    if (pipe(queue->eventPipe) == -1) {
        perror("error creating event queue pipe");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        perror("event queue mutex init has failed\n");
        exit(EXIT_FAILURE);
    }
    return queue;
}

static void freeEventQueue(eventQueue *eventQueue) {
    eventData *event;
    while ((event = dequeueEvent(eventQueue)) != NULL) {
        freeEventData(event);
    }
    free(eventQueue);
}

void flushEventPipe() {
    char buf[10];
    size_t bufLen = sizeof(buf);
    while (read(eventQueuePtr->eventPipe[0], buf, bufLen) == bufLen) {}
}

static JSValue pollEvents(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    eventData *event;
    flushEventPipe();
    while (hasEvents(eventQueuePtr)) {
        event = dequeueEvent(eventQueuePtr);
        if (JS_VALUE_GET_PTR(event->obj) != NULL && JS_IsLiveObject(JS_GetRuntime(ctx), event->obj)) {
            event->handler(ctx, event->obj, event->data);
        }
        freeEventData(event);
    }
}

static JSValue initEventQueueJob(JSContext *ctx, int argc, JSValueConst *argv) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue os = JS_GetPropertyStr(ctx, global, "os");
    JSValue setReadHandler = JS_GetPropertyStr(ctx, os, "setReadHandler");
    JSValue pollEventsFn = JS_NewCFunction(ctx, pollEvents, "", 0);
    JSValue setReadHandlerArgs[] = { JS_NewInt32(ctx, eventQueuePtr->eventPipe[0]), pollEventsFn };
    JS_Call(ctx, setReadHandler, JS_UNDEFINED, 2, setReadHandlerArgs);
    JS_FreeValue(ctx, os);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, setReadHandler);
    JS_FreeValue(ctx, pollEventsFn);
}

void initEventQueue(JSContext *ctx) {
    if (eventQueuePtr != NULL) return;
    eventQueuePtr = createEventQueue();
    JS_EnqueueJob(ctx, initEventQueueJob, 0, NULL);
}

void enqueueEvent(eventHandler handler, JSValue obj, void *data) {
    eventData *event = createEventData(handler, obj, data);
    enqueueEventInternal(eventQueuePtr, event);
}