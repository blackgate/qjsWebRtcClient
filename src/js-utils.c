#include <string.h>
#include "js-utils.h"

int initFullClass(JSContext *ctx, JSModuleDef *m, JSFullClassDef *fullDef) {
    JSValue proto, obj;
    const char *className = fullDef->def.class_name;
    JSContructorDef *constructor = &fullDef->constructor;
    JS_NewClassID(&fullDef->id);
    JS_NewClass(JS_GetRuntime(ctx), fullDef->id, &fullDef->def);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, fullDef->funcs, fullDef->funcs_len / sizeof(fullDef->funcs[0]));
    JS_SetClassProto(ctx, fullDef->id, proto);
    obj = JS_NewCFunction2(ctx, constructor->fn, className, constructor->args_count, JS_CFUNC_constructor, 0);
    JS_SetModuleExport(ctx, m, className, obj);
    return 0;
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