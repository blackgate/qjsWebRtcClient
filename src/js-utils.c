#include <string.h>
#include "js-utils.h"

static int initFullClassInternal(JSContext *ctx, JSModuleDef *m, JSFullClassDef *fullDef, JSValue baseProto) {
    JSValue proto, obj;
    const char *className = fullDef->def.class_name;
    JSCFunctionListEntry *fns = fullDef->funcs;
    JSContructorDef *constructor = &fullDef->constructor;
    JS_NewClassID(&fullDef->id);
    JS_NewClass(JS_GetRuntime(ctx), fullDef->id, &fullDef->def);
    proto = JS_IsNull(baseProto) ? JS_NewObject(ctx) : JS_NewObjectProto(ctx, baseProto);
    JS_SetPropertyFunctionList(ctx, proto, fns, countof(fns));
    JS_SetClassProto(ctx, fullDef->id, proto);
    obj = JS_NewCFunction2(ctx, constructor->fn, className, constructor->args_count, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, className, obj);
    return 0;
}

int initFullClass(JSContext *ctx, JSModuleDef *m, JSFullClassDef *fullDef) {
    return initFullClassInternal(ctx, m, fullDef, JS_NULL);
}

int initFullSubClass(JSContext *ctx, JSModuleDef *m, JSFullClassDef *fullDef, JSClassID baseClass) {
    JSValue baseProto = JS_GetClassProto(ctx, baseClass);
    return initFullClassInternal(ctx, m, fullDef, baseProto);
}

void JS_CopyToCStringMax(JSContext *ctx, JSValue val, char* dest, size_t max_len) {
    size_t len;
    const char* src = JS_ToCStringLen(ctx, &len, val);
    len = MIN(len, max_len);
    strncpy(dest, src, len);
    dest[len] = '\0';
    JS_FreeCString(ctx, src);
}

uint32_t JS_GetArrayLength(JSContext *ctx, JSValue array) {
    JSValue len = JS_GetPropertyStr(ctx, array, "length");
    uint32_t res;
    JS_ToUint32(ctx, &res, len);
    JS_FreeValue(ctx, len);
    return res;
}