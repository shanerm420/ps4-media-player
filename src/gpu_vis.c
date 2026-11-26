/* ---------------------------------------------------------------- gpu_vis -- */
/* The GPU visualizer path: Piglet ES2, precompiled shader blobs, half-float
 * ping-pong and a bloom chain. Built on the calls the probe proved on this
 * console (see MANIFEST §7 and _scratch/gpu-test/bloom104):
 *
 *   modules   libScePigletv2VSH.sprx, libScePrecompiledShaders.sprx from
 *             "/<sandbox random word>/common/lib/"
 *   blobs     walked out of scePrecompiledShaderEntries ({ident, code, len}),
 *             uploaded with glShaderBinary(1, &id, 0, code, len) - format 0 is
 *             PSSL, which is why no GLSL compiler is needed here
 *   targets   GL_RGBA + GL_HALF_FLOAT_OES (R17: GL_RGBA16F does not exist in
 *             ES2, asking for it fails and looks like "no half float")
 *   feedback  ping-pong, because framebuffer fetch and float blending are absent
 *
 * Budget: the present path is 21.6-24.5 ms (R5), a bloom chain fits inside one
 * refresh (BREW00102: N = 0/1/4/16 all 16.65 ms), and the base PS4 is the floor
 * every number has to fit (MANIFEST §7 platform scope).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglplatform.h>

/* exported by libScePrecompiledShaders.sprx */
typedef struct { const char *ident; const void *code; unsigned len; } GvBlob;
extern const GvBlob scePrecompiledShaderEntries[];
extern int sceKernelLoadStartModule(const char *path, unsigned args, const void *argp,
                                    unsigned flags, const void *opt, int *pRes);
extern const char *sceKernelGetFsSandboxRandomWord(void);

/* the console window type, declared here because the toolchain header
 * only exposes it on the __ORBIS__ path (same layout: id, w, h, padding) */
typedef struct { unsigned uID, uWidth, uHeight, uPadding; } GvPglWindow;

#define GV_MAX_LOOKS 18

typedef struct { const char *name; float warp, glow, tint, speed; } GvLook;

/* one entry per style index, in vis_style_name() order */
static const GvLook g_look[GV_MAX_LOOKS] = {
    { "SPECTRUM",  0.10f, 0.55f, 0.20f, 1.00f },
    { "WAVEFORM",  0.25f, 0.35f, 0.55f, 0.80f },
    { "RADIAL",    0.55f, 0.70f, 0.10f, 0.90f },
    { "PULSE",     0.05f, 0.85f, 0.95f, 1.30f },
    { "MIRROR",    0.35f, 0.40f, 0.62f, 0.60f },
    { "BLOCKS",    0.60f, 0.50f, 0.30f, 1.10f },
    { "SCOPE",     0.15f, 0.75f, 0.45f, 1.20f },
    { "ORBIT",     0.40f, 0.60f, 0.70f, 0.70f },
    { "LIQUID",    0.30f, 0.65f, 0.50f, 0.85f },
    { "NEON",      0.45f, 0.80f, 0.85f, 1.00f },
    { "FLAG",      0.70f, 0.45f, 0.15f, 0.75f },
    { "DANCE",     0.50f, 0.90f, 0.05f, 1.40f },
    { "WORM",      0.20f, 0.55f, 0.40f, 0.65f },
    { "WINDMILL",  0.08f, 0.50f, 0.75f, 0.55f },
    { "DANCE 2",   0.65f, 0.70f, 0.35f, 1.25f },
    { "DANCE 3",   0.35f, 0.60f, 0.90f, 0.95f },
    { "RAVE DJ",   0.80f, 0.95f, 0.25f, 1.50f },
    { "TESSERACT", 0.90f, 0.55f, 0.60f, 1.05f }
};

static struct {
    EGLDisplay dpy; EGLSurface surf; EGLContext ctx;
    int up, w, h, ok;
    GLuint prog, rt[2], fb[2], bloom;
    GLint uTex, uWarp, uGlow, uTint, uTime;
    int look; float t;
} gv;

static GLuint gv_shader(GLenum type, const char *name)
{
    GLuint id = glCreateShader(type);
    int i;

    for (i = 0; scePrecompiledShaderEntries[i].ident; i++) {
        const GvBlob *b = &scePrecompiledShaderEntries[i];
        if (strcmp(b->ident, name) == 0) {
            glShaderBinary(1, &id, 0, b->code, (GLsizei)b->len);
            return id;
        }
    }
    fprintf(stderr, "gpu_vis: no blob %s\n", name);
    glDeleteShader(id);
    return 0;
}

static GLuint gv_program(const char *vs, const char *fs)
{
    GLuint p = glCreateProgram(), v = gv_shader(GL_VERTEX_SHADER, vs);
    GLuint f = gv_shader(GL_FRAGMENT_SHADER, fs);
    GLint linked = 0;

    if (!v || !f) return 0;
    glAttachShader(p, v); glAttachShader(p, f);
    glBindAttribLocation(p, 0, "pos");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &linked);
    if (!linked) { fprintf(stderr, "gpu_vis: link failed\n"); return 0; }
    return p;
}

static void gv_target(int i, int w, int h)
{
    glGenTextures(1, &gv.rt[i]);
    glBindTexture(GL_TEXTURE_2D, gv.rt[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_HALF_FLOAT_OES, NULL);          /* R17 */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &gv.fb[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, gv.fb[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           gv.rt[i], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "gpu_vis: target %d incomplete\n", i);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int gpu_vis_init(int w, int h)
{
    EGLint cfgattr[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                         EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_SAMPLE_BUFFERS, 0,
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig cfg; EGLint n = 0, major, minor, mret;
    char path[256];

    snprintf(path, sizeof path, "/%s/common/lib/libScePigletv2VSH.sprx",
             sceKernelGetFsSandboxRandomWord());
    if (sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &mret) < 0) return 0;
    snprintf(path, sizeof path, "/%s/common/lib/libScePrecompiledShaders.sprx",
             sceKernelGetFsSandboxRandomWord());
    if (sceKernelLoadStartModule(path, 0, NULL, 0, NULL, &mret) < 0) return 0;

    gv.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (gv.dpy == EGL_NO_DISPLAY || !eglInitialize(gv.dpy, &major, &minor)) return 0;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return 0;
    if (!eglChooseConfig(gv.dpy, cfgattr, &cfg, 1, &n) || n < 1) return 0;
    gv.ctx = eglCreateContext(gv.dpy, cfg, EGL_NO_CONTEXT, ctxattr);
    if (gv.ctx == EGL_NO_CONTEXT) return 0;
    /* 12b: the console's own video-out window, not SDL's. uID must be 0-7. */
    if (gv.surf == EGL_NO_SURFACE) {
        static GvPglWindow win;
        EGLint sattr[] = { EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE };

        win.uID = 0;
        win.uWidth = (unsigned)w;
        win.uHeight = (unsigned)h;
        win.uPadding = 0;
        gv.surf = eglCreateWindowSurface(gv.dpy, cfg,
                                         (EGLNativeWindowType)&win, sattr);
        if (gv.surf == EGL_NO_SURFACE) return 0;
    }
    if (!eglMakeCurrent(gv.dpy, gv.surf, gv.surf, gv.ctx)) return 0;

    gv.w = w; gv.h = h;
    gv.prog = gv_program("texmap/v_2.vert", "texmap/f_2.frag");
    if (!gv.prog) return 0;
    gv.bloom = gv_program("bright/v_2.vert", "bright/f_2.frag");
    gv_target(0, w, h);
    gv_target(1, w / 4, h / 4);
    gv.uTex  = glGetUniformLocation(gv.prog, "tex");
    gv.uWarp = glGetUniformLocation(gv.prog, "warp");
    gv.uGlow = glGetUniformLocation(gv.prog, "glow");
    gv.uTint = glGetUniformLocation(gv.prog, "tint");
    gv.uTime = glGetUniformLocation(gv.prog, "time");
    gv.ok = 1;
    return 1;
}

void gpu_vis_set_surface(EGLSurface s) { gv.surf = s; }

void gpu_vis_select(int style)
{
    if (style >= 0 && style < GV_MAX_LOOKS) gv.look = style;
}

int gpu_vis_look_count(void) { return GV_MAX_LOOKS; }

const char *gpu_vis_look_name(int i)
{
    return (i >= 0 && i < GV_MAX_LOOKS) ? g_look[i].name : "";
}

/* one frame: scene into rt0, bright into rt1, bloom back over rt0 (ping-pong),
 * then rt0 to the screen. Every pass is the same quad through a blob. */
void gpu_vis_frame(float dt, float level)
{
    const GvLook *k;
    GLuint src = 0;

    if (!gv.ok) return;
    gv.t += dt;
    k = &g_look[gv.look];

    glBindFramebuffer(GL_FRAMEBUFFER, gv.fb[0]);
    glViewport(0, 0, gv.w, gv.h);
    glUseProgram(gv.prog);
    glUniform1f(gv.uWarp, k->warp * (0.5f + level));
    glUniform1f(gv.uGlow, k->glow);
    glUniform1f(gv.uTint, k->tint);
    glUniform1f(gv.uTime, gv.t * k->speed);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (gv.bloom) {
        glBindFramebuffer(GL_FRAMEBUFFER, gv.fb[1]);
        glViewport(0, 0, gv.w / 4, gv.h / 4);
        glUseProgram(gv.bloom);
        glBindTexture(GL_TEXTURE_2D, gv.rt[0]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindFramebuffer(GL_FRAMEBUFFER, gv.fb[0]);
        glViewport(0, 0, gv.w, gv.h);
        glUseProgram(gv.prog);
        glBindTexture(GL_TEXTURE_2D, gv.rt[1]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        src = gv.rt[0];
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gv.w, gv.h);
    glUseProgram(gv.prog);
    glBindTexture(GL_TEXTURE_2D, src);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(gv.dpy, gv.surf);
}

void gpu_vis_shutdown(void)
{
    if (gv.ctx != EGL_NO_CONTEXT) {
        eglMakeCurrent(gv.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(gv.dpy, gv.ctx);
        eglTerminate(gv.dpy);
    }
    gv.ok = 0;
}
