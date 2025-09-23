/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/gc.h"
#include "py/enum.h"

#if MICROPY_PY_GC && MICROPY_ENABLE_GC

// collect(): run a garbage collection
static mp_obj_t py_gc_collect(void) {
    gc_collect();
    #if MICROPY_PY_GC_COLLECT_RETVAL
    return MP_OBJ_NEW_SMALL_INT(MP_STATE_MEM(gc_collected));
    #else
    return mp_const_none;
    #endif
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_collect_obj, py_gc_collect);

// disable(): disable the garbage collector
static mp_obj_t gc_disable(void) {
    MP_STATE_MEM(gc_auto_collect_enabled) = 0;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_disable_obj, gc_disable);

// enable(): enable the garbage collector
static mp_obj_t gc_enable(void) {
    MP_STATE_MEM(gc_auto_collect_enabled) = 1;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_enable_obj, gc_enable);

static mp_obj_t gc_isenabled(void) {
    return mp_obj_new_bool(MP_STATE_MEM(gc_auto_collect_enabled));
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_isenabled_obj, gc_isenabled);

// mem_free(): return the number of bytes of available heap RAM
static mp_obj_t gc_mem_free(void) {
    gc_info_t info;
    gc_info(&info);
    #if MICROPY_GC_SPLIT_HEAP_AUTO
    // Include max_new_split value here as a more useful heuristic
    return MP_OBJ_NEW_SMALL_INT(info.free + info.max_new_split);
    #else
    return MP_OBJ_NEW_SMALL_INT(info.free);
    #endif
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_mem_free_obj, gc_mem_free);

// mem_alloc(): return the number of bytes of heap RAM that are allocated
static mp_obj_t gc_mem_alloc(void) {
    gc_info_t info;
    gc_info(&info);
    return MP_OBJ_NEW_SMALL_INT(info.used);
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_mem_alloc_obj, gc_mem_alloc);

#if CIRCUITPY_GC_TRACK_LIVE

#define CIRCUITPY_GC_TRACK_INT_RV_VOID(name)    \
    static mp_obj_t gc_mem_##name##_live(void) { \
        return MP_OBJ_NEW_SMALL_INT(gc_live_mem_##name()); \
    }   \
    MP_DEFINE_CONST_FUN_OBJ_0(gc_mem_##name##_live_obj, gc_mem_##name##_live);    \

CIRCUITPY_GC_TRACK_INT_RV_VOID(free)
CIRCUITPY_GC_TRACK_INT_RV_VOID(alloc)
CIRCUITPY_GC_TRACK_INT_RV_VOID(reset)
CIRCUITPY_GC_TRACK_INT_RV_VOID(sync)
CIRCUITPY_GC_TRACK_INT_RV_VOID(collect_sync)
CIRCUITPY_GC_TRACK_INT_RV_VOID(quick_free)
CIRCUITPY_GC_TRACK_INT_RV_VOID(quick_used)

#if 0
typedef enum {
    INFO_USED,
    INFO_ALLOCS,
    INFO_REALLOCS,
    INFO_FREES,
} live_mem_info_t;

extern const mp_obj_type_t live_mem_info_type;

MAKE_ENUM_VALUE(live_mem_info_type, info, USED, INFO_USED);
MAKE_ENUM_VALUE(live_mem_info_type, info, ALLOCS, INFO_ALLOCS);
MAKE_ENUM_VALUE(live_mem_info_type, info, REALLOCS, INFO_REALLOCS);
MAKE_ENUM_VALUE(live_mem_info_type, info, FREES, INFO_FREES);

MAKE_ENUM_MAP(live_mem_info) {
    MAKE_ENUM_MAP_ENTRY(info, USED),
    MAKE_ENUM_MAP_ENTRY(info, ALLOCS),
    MAKE_ENUM_MAP_ENTRY(info, REALLOCS),
    MAKE_ENUM_MAP_ENTRY(info, FREES),
};
static MP_DEFINE_CONST_DICT(live_mem_info_locals_dict, live_mem_info_locals_table);


MAKE_PRINTER(live_mem_info, live_mem_info);


MP_DEFINE_CONST_OBJ_TYPE(
    live_mem_info_type,
    MP_QSTR_LiveMemInfo,
    MP_TYPE_FLAG_NONE,
    print, live_mem_info_print,
    locals_dict, &live_mem_info_locals_dict
    );

const mp_obj_type_t live_mem_info_type;
#endif


static mp_obj_t gc_mem_info_live(void) {
    return gc_live_mem_info();
}
MP_DEFINE_CONST_FUN_OBJ_0(gc_mem_info_live_obj, gc_mem_info_live);

#endif

#if MICROPY_GC_ALLOC_THRESHOLD
static mp_obj_t gc_threshold(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        if (MP_STATE_MEM(gc_alloc_threshold) == (size_t)-1) {
            return MP_OBJ_NEW_SMALL_INT(-1);
        }
        return mp_obj_new_int(MP_STATE_MEM(gc_alloc_threshold) * MICROPY_BYTES_PER_GC_BLOCK);
    }
    mp_int_t val = mp_obj_get_int(args[0]);
    if (val < 0) {
        MP_STATE_MEM(gc_alloc_threshold) = (size_t)-1;
    } else {
        MP_STATE_MEM(gc_alloc_threshold) = val / MICROPY_BYTES_PER_GC_BLOCK;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gc_threshold_obj, 0, 1, gc_threshold);
#endif

static const mp_rom_map_elem_t mp_module_gc_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_gc) },
    { MP_ROM_QSTR(MP_QSTR_collect), MP_ROM_PTR(&gc_collect_obj) },
    { MP_ROM_QSTR(MP_QSTR_disable), MP_ROM_PTR(&gc_disable_obj) },
    { MP_ROM_QSTR(MP_QSTR_enable), MP_ROM_PTR(&gc_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_isenabled), MP_ROM_PTR(&gc_isenabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_free), MP_ROM_PTR(&gc_mem_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_alloc), MP_ROM_PTR(&gc_mem_alloc_obj) },
    #if CIRCUITPY_GC_TRACK_LIVE
    { MP_ROM_QSTR(MP_QSTR_mem_free_live), MP_ROM_PTR(&gc_mem_free_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_quick_free_live), MP_ROM_PTR(&gc_mem_quick_free_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_quick_used_live), MP_ROM_PTR(&gc_mem_quick_used_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_alloc_live), MP_ROM_PTR(&gc_mem_alloc_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_reset_live), MP_ROM_PTR(&gc_mem_reset_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_sync_live), MP_ROM_PTR(&gc_mem_sync_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_info_live), MP_ROM_PTR(&gc_mem_info_live_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_collect_sync_live), MP_ROM_PTR(&gc_mem_collect_sync_live_obj) },
    #endif
    #if MICROPY_GC_ALLOC_THRESHOLD
    { MP_ROM_QSTR(MP_QSTR_threshold), MP_ROM_PTR(&gc_threshold_obj) },
    #endif
};

static MP_DEFINE_CONST_DICT(mp_module_gc_globals, mp_module_gc_globals_table);

const mp_obj_module_t mp_module_gc = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_gc_globals,
};

MP_REGISTER_MODULE(MP_QSTR_gc, mp_module_gc);

#endif
