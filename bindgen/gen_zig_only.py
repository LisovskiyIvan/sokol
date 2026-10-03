# LLM maintained.
# Zig-only bindings generation: the Zig slice of gen_all.py's task list.
#
# gen_all.py also regenerates all other language bindings, but those repos
# are not needed here. Run from this directory with a sokol-zig clone at
# ./sokol-zig:
#
#   cd bindgen
#   ln -s ../../sokol-zig sokol-zig    # or: git clone <fork> sokol-zig
#   PYTHONPATH=. python3 gen_zig_only.py
#
# Requires Python 3.12+ (f-strings in gen_zig.py) and clang in PATH.
# Keep the task list in sync with gen_all.py when a header is added.
import gen_zig

tasks = [
    ['../sokol_log.h',              'slog_',     []],
    ['../sokol_gfx.h',              'sg_',       []],
    ['../sokol_app.h',              'sapp_',     []],
    ['../sokol_glue.h',             'sglue_',    ['sg_']],
    ['../sokol_time.h',             'stm_',      []],
    ['../sokol_audio.h',            'saudio_',   []],
    ['../sokol_fetch.h',            'sfetch_',   []],
    ['../util/sokol_gl.h',          'sgl_',      ['sg_']],
    ['../util/sokol_debugtext.h',   'sdtx_',     ['sg_']],
    ['../util/sokol_shape.h',       'sshape_',   ['sg_']],
    ['../util/sokol_framebuffer.h', 'sfb_',      ['sg_']],
    ['../util/sokol_letterbox.h',   'slbx_',     []],
    ['../util/sokol_cmdbuf.h',      'scb_',      ['sg_']],
    ['../util/sokol_imgui.h',       'simgui_',   ['sg_', 'sapp_']],
    ['../util/sokol_gfx_imgui.h',   'sgimgui_',  []],
    ['../util/sokol_app_imgui.h',   'sappimgui_', ['sapp_']],
]

module_names = {
    'slog_':      'log',
    'sg_':        'gfx',
    'sapp_':      'app',
    'sargs_':     'args',
    'stm_':       'time',
    'saudio_':    'audio',
    'sgl_':       'gl',
    'sdtx_':      'debugtext',
    'sshape_':    'shape',
    'sglue_':     'glue',
    'sfetch_':    'fetch',
    'simgui_':    'imgui',
    'sgimgui_':   'gfximgui',
    'sappimgui_': 'appimgui',
    'snk_':       'nuklear',
    'smemtrack_': 'memtrack',
    'sfb_':       'framebuffer',
    'slbx_':      'letterbox',
    'scb_':       'cmdbuf',
}

gen_zig.prepare()
for task in tasks:
    gen_zig.gen({
        'c_header_path': task[0],
        'c_prefix': task[1],
        'dep_c_prefixes': task[2],
        'module_names': module_names,
    })
