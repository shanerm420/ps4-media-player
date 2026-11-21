/* host_test.c - runs the platform-independent audio path on a PC.
 *
 * Checks:
 *   1. flat EQ is bit-transparent
 *   2. a +12 dB band boost at 1 kHz gives ~4x amplitude on a 1 kHz tone
 *   3. a -12 dB cut gives ~0.25x
 *   4. bands above Nyquist are bypassed
 *   5. resampler 44.1k -> 48k keeps the pitch and the right output length
 *   6. volume -6.02 dB is ~0.5x
 *   7. the soft limiter keeps |x| < 1
 *   8. a real MP3 decodes to sane frames/format, with no NaN
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "audio_core.h"
#include "eq.h"
#include "mp3_source.h"
#include "resampler.h"
#include "subtitles.h"

#define SR 48000

static int g_fail;

static void check(int cond, const char *what, const char *detail)
{
    printf("%-58s %s%s%s\n", what, cond ? "PASS" : "FAIL  ",
           cond ? "" : " ", cond ? "" : detail);
    if (!cond)
        g_fail++;
}

/* rate matters: generating 1 kHz at the wrong rate is exactly the mistake this
 * test is here to catch, so it takes the sample rate explicitly. */
static void tone_at(float *buf, int frames, int ch, float hz, float amp, int rate)
{
    int i, c;
    for (i = 0; i < frames; i++) {
        float v = amp * sinf(2.0f * 3.14159265358979f * hz * (float)i / (float)rate);
        for (c = 0; c < ch; c++)
            buf[i * ch + c] = v;
    }
}

static void tone(float *buf, int frames, int ch, float hz, float amp)
{
    tone_at(buf, frames, ch, hz, amp, SR);
}

static float rms(const float *buf, int samples)
{
    double acc = 0.0;
    int i;
    for (i = 0; i < samples; i++)
        acc += (double)buf[i] * buf[i];
    return (float)sqrt(acc / (double)samples);
}

static float peak(const float *buf, int samples)
{
    float p = 0.0f;
    int i;
    for (i = 0; i < samples; i++) {
        float a = fabsf(buf[i]);
        if (a > p) p = a;
    }
    return p;
}

static void test_eq_flat(void)
{
    Eq eq;
    enum { N = 4096 };
    float a[N], b[N];

    eq_init(&eq, SR, 1);
    tone(a, N, 1, 1000.0f, 0.5f);
    memcpy(b, a, sizeof(a));
    eq_process(&eq, b, N);

    check(memcmp(a, b, sizeof(a)) == 0,
          "flat EQ is bit-transparent", "");
}

static void test_eq_boost(void)
{
    Eq eq;
    enum { N = 48000 };
    static float a[N], b[N];
    float r0, r1;

    eq_init(&eq, SR, 1);
    tone(a, N, 1, 1000.0f, 0.1f);
    memcpy(b, a, sizeof(a));
    r0 = rms(a + 12000, 24000);

    eq_set_band_db(&eq, 5, 12.0f);      /* band 5 = 1 kHz */
    eq_process(&eq, b, N);
    r1 = rms(b + 12000, 24000);

    check(fabsf(r1 / r0 - 3.981f) < 0.25f,
          "+12 dB @1k gives ~4.0x on a 1 kHz tone", "");
    printf("      measured gain: %.3fx\n", (double)(r1 / r0));
}

static void test_eq_cut(void)
{
    Eq eq;
    enum { N = 48000 };
    static float a[N];
    float r0, r1;

    eq_init(&eq, SR, 1);
    tone(a, N, 1, 1000.0f, 0.5f);
    r0 = rms(a + 12000, 24000);
    eq_set_band_db(&eq, 5, -12.0f);
    eq_process(&eq, a, N);
    r1 = rms(a + 12000, 24000);

    check(fabsf(r1 / r0 - 0.251f) < 0.03f,
          "-12 dB @1k gives ~0.25x on a 1 kHz tone", "");
    printf("      measured gain: %.3fx\n", (double)(r1 / r0));
}

static void test_eq_nyquist(void)
{
    Eq eq;
    enum { N = 2048 };
    float a[N], b[N];

    eq_init(&eq, 8000, 1);              /* 16 kHz band is above 4 kHz Nyquist */
    eq_set_band_db(&eq, 9, 12.0f);
    tone(a, N, 1, 500.0f, 0.3f);
    memcpy(b, a, sizeof(a));
    eq_process(&eq, b, N);
    check(memcmp(a, b, sizeof(a)) == 0,
          "band above Nyquist is bypassed (8 kHz stream)", "");
}

static int count_zero_crossings(const float *buf, int frames)
{
    int i, n = 0;
    for (i = 1; i < frames; i++)
        if ((buf[i - 1] <= 0.0f && buf[i] > 0.0f))
            n++;
    return n;
}

static void test_resampler(void)
{
    Resampler rs;
    enum { IN = 44100 };
    static float in[IN], out[IN * 2];
    int outn, crossings;
    float seconds;

    rs_init(&rs, 44100, 48000, 1);
    tone_at(in, IN, 1, 1000.0f, 0.5f, 44100);   /* 1 s of 1 kHz at 44.1 kHz */
    outn = rs_process(&rs, in, IN, out, IN * 2);

    seconds   = (float)outn / 48000.0f;
    crossings = count_zero_crossings(out, outn);

    check(abs(outn - 48000) < 200,
          "44.1k -> 48k produces ~1 s of output", "");
    printf("      output frames: %d (%.4f s)\n", outn, (double)seconds);
    check(abs(crossings - 1000) <= 3,
          "pitch preserved (zero crossings ~1000)", "");
    printf("      zero crossings: %d\n", crossings);
}

static void test_volume(void)
{
    Volume v;
    enum { N = 64 };
    float a[N];
    int i;

    vol_init(&v);
    vol_set_db(&v, -6.0206f);
    for (i = 0; i < N; i++)
        a[i] = 0.5f;
    audio_apply_gain(a, N, v.gain);
    check(fabsf(v.gain - 0.5f) < 0.002f,
          "-6.02 dB gives 0.5x gain", "");
}

static void test_limiter(void)
{
    float a[4] = { 0.0f, 0.50f, 2.0f, -3.0f };
    int16_t s[4];

    audio_soft_limit(a, 4);
    /* Full scale is allowed; what must never happen is a boost pushing the
     * signal past it, or the 16-bit conversion wrapping. */
    check(peak(a, 4) <= 1.0f + 1e-6f,
          "soft limiter holds full scale at 1.0", "");
    /* Below the 0.90 knee the signal must be untouched. */
    check(a[1] == 0.50f, "limiter is transparent below the knee", "");

    a[0] = 2.0f;
    a[1] = -3.0f;
    a[2] = 0.5f;
    a[3] = -0.5f;
    audio_to_s16(a, s, 4);
    check(s[0] == 32767 && s[1] == -32767 && s[2] == 16384,
          "s16 conversion clamps instead of wrapping", "");
}

static void test_mp3(const char *path)
{
    Mp3Source s;
    static float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
    int frames, total = 0, guard = 0;
    int bad = 0, hz = 0, ch = 0;
    int rc = mp3_open(&s, path);

    check(rc == 0, "MP3 opens", path);
    if (rc != 0)
        return;

    while (guard++ < 100000) {
        int i;
        frames = mp3_read(&s, pcm, 1152);
        if (frames <= 0) {
            if (s.finished)
                break;
            continue;
        }
        hz = s.hz;
        ch = s.channels;
        total += frames;
        for (i = 0; i < frames * ch; i++)
            if (!isfinite(pcm[i]))
                bad++;
    }

    printf("      decoded %d frames, %d Hz, %d ch, est %.2f s\n",
           total, hz, ch, mp3_duration_est(&s));
    check(total > 0, "MP3 decodes to audio frames", "");
    check(hz > 0 && ch > 0, "stream format reported", "");
    check(bad == 0, "no NaN/inf in decoded samples", "");
    {
        /* Padding frames used to zero the format out; this locks that down. */
        double d = mp3_duration_est(&s);
        char   buf[64];
        snprintf(buf, sizeof(buf), "duration %.3f s", d);
        check(d > 0.5 && d < 2.0, "duration is sane for a 1 s file", buf);
    }
    mp3_close(&s);
}

/* Writes the two sidecar files, then checks that the parser finds them by
 * stem, cleans markup, times cues the same way in SRT and VTT, and that the
 * on/off cycle behaves. Paths live under build/ so nothing ships in data/. */
static void test_subtitles(void)
{
    static const char srt[] =
        "1\r\n"
        "00:00:01,000 --> 00:00:03,000\r\n"
        "<i>Hello</i> world\r\n"
        "\r\n"
        "2\r\n"
        "00:00:04,500 --> 00:00:06,000\r\n"
        "Second line\r\n"
        "line two\r\n"
        "\r\n"
        "3\r\n"
        "00:00:07,000 --> 00:00:09,000\r\n"
        "{\\an8}Top caption\r\n";
    static const char vtt[] =
        "WEBVTT\r\n"
        "\r\n"
        "cue-1\r\n"
        "00:10.000 --> 00:12.000 align:start\r\n"
        "Vtt <b>cue</b>\r\n";
    FILE *f;
    const char *t;
    int n;

    mkdir("build", 0755);

    f = fopen("build/subs.mp4", "wb");
    if (f) fclose(f);
    f = fopen("build/subs.en.srt", "wb");
    if (f) { fwrite(srt, 1, sizeof(srt) - 1, f); fclose(f); }
    f = fopen("build/subs.vtt", "wb");
    if (f) { fwrite(vtt, 1, sizeof(vtt) - 1, f); fclose(f); }

    n = sub_open("build/subs.mp4");
    check(n == 2, "subtitle loader finds both sidecar files", "");
    check(sub_enabled() == 1, "subtitles start enabled when files exist", "");

    t = sub_text_at(2.0);
    check(t && strcmp(t, "Hello world") == 0, "SRT cue shows, <i> stripped",
          t ? t : "(null)");
    t = sub_text_at(5.0);
    check(t && strcmp(t, "Second line\nline two") == 0,
          "SRT multi-line cue keeps its break", t ? t : "(null)");
    t = sub_text_at(8.0);
    check(t && strcmp(t, "Top caption") == 0, "{\\an8} override is dropped",
          t ? t : "(null)");
    check(sub_text_at(0.5) == NULL, "no cue before the first timestamp", "");
    check(sub_text_at(3.5) == NULL, "gap between cues shows nothing", "");

    /* second source, then off */
    check(sub_cycle() == 1 && sub_index() == 1, "cycle moves to the next file", "");
    t = sub_text_at(11.0);
    check(t && strcmp(t, "Vtt cue") == 0, "WebVTT cue parses (MM:SS.mmm)",
          t ? t : "(null)");
    check(sub_cycle() == 0 && sub_enabled() == 0, "cycle past the last file is off", "");
    check(sub_text_at(11.0) == NULL, "off hides every cue", "");

    sub_close();
    check(sub_count() == 0, "sub_close releases the files", "");
}

int main(int argc, char **argv)
{
    const char *mp3 = (argc > 1) ? argv[1] : "data/test.mp3";

    printf("Ultimate PS4 Media Player - host audio path tests\n\n");
    test_eq_flat();
    test_eq_boost();
    test_eq_cut();
    test_eq_nyquist();
    test_resampler();
    test_volume();
    test_limiter();
    test_mp3(mp3);
    test_subtitles();

    printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL TESTS PASSED",
           g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
