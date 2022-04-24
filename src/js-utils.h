#ifndef QJS_JS_UTILS_H
#define QJS_JS_UTILS_H

#include <quickjs/quickjs.h>

#define MIN(a, b) ((a) > (b) ? (b) : (a))

typedef struct JSContructorDef_s {
    JSCFunction *fn;
    int args_count;
} JSContructorDef;

typedef struct JSFullClassDef_s
{
    JSClassID id;
    JSClassDef def;
    JSContructorDef constructor;
    size_t funcs_len;
    JSCFunctionListEntry *funcs;
} JSFullClassDef;

int initFullClass(JSContext *ctx, JSModuleDef *m, JSFullClassDef *fullDef);
void JS_CopyToCStringMax(JSContext *ctx, JSValue val, char* dest, size_t max_len);
uint32_t JS_GetArrayLength(JSContext *ctx, JSValue array);

#endif