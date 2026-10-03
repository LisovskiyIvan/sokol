/*
    LLM maintained.

    sokol_gfx_wgpu_timing_test.c

    Headless regression test for the WGPU GPU-timing callback lifetime:
    slot-owned static meta records plus the userdata2 generation token.
    No GPU, no window, no allocator dependence.

    Standalone only, not part of the normal macOS suite: needs
    <webgpu/webgpu.h>, so it is skipped unless WEBGPU_INCLUDE_DIR points
    at the dir containing it. Direct command from this directory:

        clang -I../../.. -I.. \
            -I<dir-containing-webgpu> -std=gnu11 -Wall -Wextra \
            -Werror -Wsign-conversion -Wstrict-prototypes \
            -ffunction-sections -fdata-sections -Wl,-dead_strip \
            sokol_gfx_wgpu_timing_test.c -o wgpu-timing-test \
            && ./wgpu-timing-test

    Only two WebGPU symbols are stubbed below (the only ones the map
    callback references); everything else is dead-stripped at link.
*/
#ifndef SOKOL_WGPU
#define SOKOL_WGPU
#endif
#ifndef SOKOL_IMPL
#define SOKOL_IMPL
#endif
#include "sokol_gfx.h"
#include "utest.h"
#include <stdlib.h>
#include <string.h>

#define T(b) EXPECT_TRUE(b)

// fake buffer handles, never dereferenced
#define FAKE_BUF(n) ((WGPUBuffer)(uintptr_t)(0x1000u + (unsigned)(n)))

//-- tiny link stubs (callback path only) -------------------------------------
static uint64_t stub_ts[256];
static int stub_ts_n;
static int stub_getrange_calls;
static WGPUBuffer stub_getrange_buf;
static int stub_unmap_calls;
static WGPUBuffer stub_unmap_buf;

const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
    (void)offset; (void)size;
    stub_getrange_calls++;
    stub_getrange_buf = buffer;
    if (stub_ts_n <= 0) {
        return 0;
    }
    return stub_ts;
}

void wgpuBufferUnmap(WGPUBuffer buffer) {
    stub_unmap_calls++;
    stub_unmap_buf = buffer;
}

//-- tracked sg allocator: the meta path must never touch it ------------------
static int alloc_calls;
static int free_calls;

static void* track_alloc(size_t size, void* user_data) {
    (void)user_data;
    alloc_calls++;
    return malloc(size ? size : 1);
}

static void track_free(void* ptr, void* user_data) {
    (void)user_data;
    free_calls++;
    free(ptr);
}

//-- simulated contexts (no device exists, statics are driven directly) -------
static void sim_fresh(uint32_t gen) {
    memset(&_sg, 0, sizeof(_sg));
    _sg.valid = true;
    _sg.desc.allocator.alloc_fn = track_alloc;
    _sg.desc.allocator.free_fn = track_free;
    _sg.desc.allocator.user_data = 0;
    memset(_sg_gpu_timing_wgpu_metas, 0, sizeof(_sg_gpu_timing_wgpu_metas));
    for (int i = 0; i < _SG_GPU_TIMING_WGPU_RING; i++) {
        _sg_gpu_timing_wgpu_state[i] = 0;
        _sg_gpu_timing_wgpu_serial[i] = 0;
        _sg_gpu_timing_wgpu_read[i] = 0;
    }
    _sg_gpu_timing_wgpu_gen = gen;
    _sg_gpu_timing_wgpu_enabled = true;
    _sg_gpu_timing_wgpu_pending = 0;
    _sg_gpu_timing_wgpu_pub_frame = 0;
    _sg_gpu_timing_wgpu_pub_ms = -1.0f;
    for (int p = 0; p < SG_MAX_GPU_TIMING_SCOPES; p++) {
        _sg_gpu_timing_wgpu_pub_pms[p] = -1.0f;
        _sg_gpu_timing_wgpu_pub_pframe[p] = 0;
    }
    stub_ts_n = 0;
    stub_getrange_calls = 0;
    stub_unmap_calls = 0;
}

// mirrors the on_commit record fill for one submitted frame
static void sim_submit(int slot, uint32_t gen, uint32_t serial, uint32_t frame,
    int natives, int scope, WGPUBuffer buf)
{
    _sg_gpu_timing_wgpu_meta_t* m = &_sg_gpu_timing_wgpu_metas[slot];
    memset(m, 0, sizeof(*m));
    m->gen = gen;
    m->slot = slot;
    m->serial = serial;
    m->frame = frame;
    m->native_count = natives;
    m->frame_first = 0;
    m->frame_last = natives;
    m->scope_has[scope] = true;
    m->scope_first[scope] = 0;
    m->scope_last[scope] = natives;
    m->buf = buf;
    _sg_gpu_timing_wgpu_serial[slot] = serial;
    _sg_gpu_timing_wgpu_state[slot] = 2;    // map pending
    _sg_gpu_timing_wgpu_read[slot] = buf;
}

static void call_mapped(WGPUMapAsyncStatus status, int slot, uint32_t token) {
    const WGPUStringView msg = { 0, 0 };
    _sg_gpu_timing_wgpu_on_mapped(status, msg,
        &_sg_gpu_timing_wgpu_metas[slot], (void*)(uintptr_t)token);
}

static void set_ts2(uint64_t a, uint64_t b) {
    stub_ts[0] = a; stub_ts[1] = b;
    stub_ts_n = 2;
}

//== stale generation: invalid context ========================================
UTEST(sokol_gfx_wgpu_timing, stale_token_invalid_context) {
    sim_fresh(5);
    sim_submit(0, 5, 11, 9, 1, 3, FAKE_BUF(1));
    // shutdown: context dead, generation bumped (teardown order)
    _sg.valid = false;
    _sg_gpu_timing_wgpu_gen = 6;
    const int gpu_calls = stub_getrange_calls + stub_unmap_calls;
    const int heap_calls = alloc_calls + free_calls;
    uint8_t snapshot[sizeof(_sg_gpu_timing_wgpu_metas)];
    memcpy(snapshot, _sg_gpu_timing_wgpu_metas, sizeof(snapshot));
    call_mapped(WGPUMapAsyncStatus_Success, 0, 5);
    // token-first guard: nothing dereferenced, freed, unmapped or published
    T(stub_getrange_calls == gpu_calls);
    T(stub_unmap_calls == gpu_calls);
    T((alloc_calls + free_calls) == heap_calls);
    T(0 == memcmp(snapshot, _sg_gpu_timing_wgpu_metas, sizeof(snapshot)));
    T(_sg_gpu_timing_wgpu_state[0] == 2);
    T(_sg_gpu_timing_wgpu_serial[0] == 11);
    T(_sg_gpu_timing_wgpu_pub_frame == 0);
    T(sg_query_gpu_frame_ms() < 0.0f);   // invalid context stays fail-closed
}

//== stale generation: new context, new allocator, reused slot ================
UTEST(sokol_gfx_wgpu_timing, stale_token_new_context) {
    // old submission, then a new context reuses slot 0 with a new serial
    sim_fresh(5);
    sim_submit(0, 5, 11, 9, 1, 3, FAKE_BUF(1));
    const int heap_before = alloc_calls + free_calls;
    sim_fresh(6);   // new context, same tracked allocator stand-in
    sim_submit(0, 6, 41, 11, 1, 3, FAKE_BUF(2));
    uint8_t snapshot[sizeof(_sg_gpu_timing_wgpu_metas)];
    memcpy(snapshot, _sg_gpu_timing_wgpu_metas, sizeof(snapshot));
    // late callback of the dead submission, token names gen 5
    call_mapped(WGPUMapAsyncStatus_Success, 0, 5);
    T(stub_getrange_calls == 0);
    T(stub_unmap_calls == 0);
    T((alloc_calls + free_calls) == heap_before);
    T(0 == memcmp(snapshot, _sg_gpu_timing_wgpu_metas, sizeof(snapshot)));
    T(_sg_gpu_timing_wgpu_state[0] == 2);
    T(_sg_gpu_timing_wgpu_pub_frame == 0);
    T(sg_query_gpu_scope_ms(3) < 0.0f);
    T(sg_query_gpu_scope_frame_index(3) == 0);
    // control: the live submission with the live token still publishes
    set_ts2(1000, 9000);
    call_mapped(WGPUMapAsyncStatus_Success, 0, 6);
    T(stub_unmap_calls == 1);
    T(stub_unmap_buf == FAKE_BUF(2));
    T(_sg_gpu_timing_wgpu_pub_frame == 11);
}

//== fresh success publishes frame + scope ====================================
UTEST(sokol_gfx_wgpu_timing, fresh_success_publish) {
    sim_fresh(6);
    sim_submit(2, 6, 42, 11, 1, 3, FAKE_BUF(3));
    set_ts2(1000, 9000);    // 8000 ns = 0.008 ms
    call_mapped(WGPUMapAsyncStatus_Success, 2, 6);
    T(stub_getrange_calls == 1);
    T(stub_getrange_buf == FAKE_BUF(3));
    T(stub_unmap_calls == 1);
    T(stub_unmap_buf == FAKE_BUF(3));
    T(_sg_gpu_timing_wgpu_state[2] == 0);
    const float fms = sg_query_gpu_frame_ms();
    T((fms > 0.0079f) && (fms < 0.0081f));
    T(sg_query_gpu_frame_index() == 11);
    const float sms = sg_query_gpu_scope_ms(3);
    T((sms > 0.0079f) && (sms < 0.0081f));
    T(sg_query_gpu_scope_frame_index(3) == 11);
    T(sg_query_gpu_scope_ms(7) < 0.0f);
}

//== measured zero stays a valid zero =========================================
UTEST(sokol_gfx_wgpu_timing, valid_zero) {
    sim_fresh(6);
    sim_submit(1, 6, 43, 12, 1, 3, FAKE_BUF(4));
    set_ts2(7000, 7000);    // quantized zero, still a real measurement
    call_mapped(WGPUMapAsyncStatus_Success, 1, 6);
    T(sg_query_gpu_frame_ms() == 0.0f);
    T(sg_query_gpu_frame_index() == 12);
    T(sg_query_gpu_scope_ms(3) == 0.0f);
    T(sg_query_gpu_scope_frame_index(3) == 12);
}

//== older frame never overwrites newer =======================================
UTEST(sokol_gfx_wgpu_timing, old_frame_rejected) {
    sim_fresh(6);
    sim_submit(0, 6, 44, 12, 1, 3, FAKE_BUF(5));
    set_ts2(7000, 7000);
    call_mapped(WGPUMapAsyncStatus_Success, 0, 6);
    T(sg_query_gpu_frame_index() == 12);
    // late arrival of an older frame of the same generation
    sim_submit(1, 6, 45, 9, 1, 3, FAKE_BUF(6));
    set_ts2(1000, 9000);
    call_mapped(WGPUMapAsyncStatus_Success, 1, 6);
    T(stub_unmap_calls == 2);    // mapped + released normally...
    T(_sg_gpu_timing_wgpu_state[1] == 0);
    T(sg_query_gpu_frame_index() == 12);    // ...but publishes nothing
    T(sg_query_gpu_frame_ms() == 0.0f);
}

//== aborted map frees the slot, publishes nothing ============================
UTEST(sokol_gfx_wgpu_timing, aborted_no_publish) {
    sim_fresh(6);
    sim_submit(3, 6, 46, 13, 1, 3, FAKE_BUF(7));
    call_mapped(WGPUMapAsyncStatus_Aborted, 3, 6);
    T(stub_getrange_calls == 0);
    T(stub_unmap_calls == 0);
    T(_sg_gpu_timing_wgpu_state[3] == 0);
    T(sg_query_gpu_frame_ms() < 0.0f);
    T(sg_query_gpu_frame_index() == 0);
}

//== null record is a silent no-op ============================================
UTEST(sokol_gfx_wgpu_timing, null_meta_noop) {
    sim_fresh(6);
    const WGPUStringView msg = { 0, 0 };
    _sg_gpu_timing_wgpu_on_mapped(WGPUMapAsyncStatus_Success, msg, 0, (void*)(uintptr_t)6u);
    T(stub_getrange_calls == 0);
    T(stub_unmap_calls == 0);
    T(sg_query_gpu_frame_ms() < 0.0f);
}

//== the meta path never touches the sg allocator =============================
UTEST(sokol_gfx_wgpu_timing, no_allocator_use) {
    sim_fresh(6);
    const int alloc_before = alloc_calls;
    const int free_before = free_calls;
    sim_submit(0, 6, 47, 14, 1, 3, FAKE_BUF(8));
    set_ts2(1000, 9000);
    call_mapped(WGPUMapAsyncStatus_Success, 0, 6);
    T(stub_unmap_calls == 1);
    T(alloc_calls == alloc_before);
    T(free_calls == free_before);
}

UTEST_MAIN()
