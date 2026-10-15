/* Nativer Ersatz für den PALLADIX-PSM-AdLib-Treiber (098.HES, V1.3).
 *
 * Nachbau nach re/notes/sound_driver.md: Schattenregister wie im Treiber, PLX-Sequenzer (nur
 * Typ 0 = 9 melodische Kanäle, wie alle Spieldateien), Fade, Loop. Der Klang kommt von
 * Nuked-OPL3 (third_party/nuked-opl3, LGPL-2.1), betrieben im OPL2-kompatiblen Modus.
 * Der Sequenzer läuft samplegenau im SDL-Audio-Callback; die Slots des Spiels sperren das
 * Audiogerät, solange sie den Treiberzustand ändern.
 *
 * Der Spieltakt (DS:9014) hängt im Port nicht am Treiber (siehe ticks_get in rtlib.c), Slot 3
 * merkt sich nur den Zeiger. Testhilfe: MADTV_NOSOUND=1 schaltet die Audioausgabe ab. */
#include "sound.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../third_party/nuked-opl3/opl3.h"

#define PIT_HZ    1193182u
#define RATE      49716 /* native OPL-Rate: kein Resampling nötig */
#define NCH       9

static SDL_AudioDeviceID g_dev;
static opl3_chip g_chip;

/* Treiberzustand (Namen wie in der Spezifikation) */
static uint8_t shadow[256];
static uint8_t ch_vol[NCH];
static uint16_t ch_ptr[NCH];  /* Offset im Song, 0 = inaktiv */
static uint16_t ch_next[NCH]; /* absoluter Tick des nächsten Events */
static int active;
static uint8_t *song;         /* eigene Kopie der Songdaten */
static uint16_t song_len;
static uint16_t song_time;
static int16_t fade_rate;
static uint16_t fade_acc;
static int stopped, muted = 1;
static uint16_t tempo_div = 0x2e9a;
static uint8_t speed = 1;
static uint16_t loops_left;
static int16_t *loop_counter;
static void *counters;
static double samples_left;   /* bis zum nächsten Musik-Tick */
static int restarted;

static const uint8_t mod_op[NCH] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};
static const uint8_t car_op[NCH] = {0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15};
static const uint16_t fnum_tab[12] = {0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca,
                                      0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287};

/* Gepuffert schreiben: Nuked-OPL3 führt die Zugriffe im Abstand von 2 Samples aus, wie der
 * Originaltreiber mit seinen Warteschleifen. Sonst würden "Key-Off + Key-On" im selben Moment
 * beim Chip nie als Neuanschlag ankommen. */
static void opl_write(uint8_t reg, uint8_t val)
{
    shadow[reg] = val;
    OPL3_WriteRegBuffered(&g_chip, reg, val);
}

static int valid_op(int o)
{
    return (o & 7) < 6 && o < 0x16;
}

/* 0d14: Grundzustand aller Register (Werte aus opl_defaults) */
static void opl_reset(void)
{
    opl_write(0x01, 0x20);
    for (int reg = 0xff; reg >= 0; reg--) {
        int v = -1, o = reg & 0x1f;
        if (reg == 0x01)
            v = 0x20;
        else if (reg >= 0x02 && reg <= 0x04)
            v = 0;
        else if (reg == 0x08)
            v = 0x40;
        else if (reg >= 0x20 && reg <= 0x35 && valid_op(reg - 0x20))
            v = 0;
        else if (reg >= 0x40 && reg <= 0x55 && valid_op(reg - 0x40))
            v = 0x3f;
        else if (reg >= 0x60 && reg <= 0x75 && valid_op(reg - 0x60))
            v = 0xee;
        else if (reg >= 0x80 && reg <= 0x95 && valid_op(reg - 0x80))
            v = 0xee;
        else if (reg == 0xa7)
            v = 0x57;
        else if (reg == 0xa8)
            v = 0x02;
        else if (reg >= 0xb0 && reg <= 0xb6)
            v = 0x1f;
        else if (reg == 0xb7)
            v = 0x09;
        else if (reg == 0xb8)
            v = 0x0e;
        else if (reg == 0xbd)
            v = 0;
        else if (reg >= 0xc0 && reg <= 0xc8)
            v = 0;
        else if (reg >= 0xe0 && reg <= 0xf5 && valid_op(reg - 0xe0))
            v = 0;
        (void)o;
        if (v >= 0)
            opl_write((uint8_t)reg, (uint8_t)v);
    }
}

/* 0cb9 */
static void set_level(int ch, uint8_t v)
{
    opl_write((uint8_t)(0x40 + car_op[ch]), v);
    if (shadow[0xc0 + ch] & 1)
        opl_write((uint8_t)(0x40 + mod_op[ch]), (uint8_t)((shadow[0x40 + mod_op[ch]] & 0xc0) | (v & 0x3f)));
}

static void key_off(int ch)
{
    if (shadow[0xb0 + ch] & 0x20)
        opl_write((uint8_t)(0xb0 + ch), (uint8_t)(shadow[0xb0 + ch] & ~0x20));
}

/* 0c64 */
static void opl_silence(void)
{
    for (int ch = 0; ch < NCH; ch++) {
        key_off(ch);
        set_level(ch, (uint8_t)(ch_vol[ch] | 0x3f));
    }
}

/* 0ca0 */
static void opl_restore_levels(void)
{
    for (int ch = 0; ch < NCH; ch++)
        set_level(ch, ch_vol[ch]);
}

static uint16_t rd16(uint16_t off)
{
    return off + 1u < song_len ? (uint16_t)(song[off] | song[off + 1] << 8) : 0;
}

static uint8_t rd8(uint16_t off)
{
    return off < song_len ? song[off] : 0;
}

/* 1431 */
static void song_parse(void)
{
    opl_write(0xbd, 0);
    speed = rd8(4);
    uint16_t div = rd16(5);
    if (div >= 100)
        tempo_div = div;
    active = 0;
    for (int ch = 0; ch < NCH; ch++) {
        ch_ptr[ch] = rd16((uint16_t)(7 + 2 * ch));
        if (ch_ptr[ch])
            active++;
        ch_vol[ch] = 0x3f;
        ch_next[ch] = 0;
    }
    song_time = 0;
}

static void song_end(void)
{
    if (loop_counter)
        (*loop_counter)++;
    if (--loops_left != 0) {
        song_parse();
        restarted = 1;
    } else {
        stopped = 1;
    }
}

/* Event eines Kanals ausführen; 0 = Kanalende bzw. ungültiges Kommando */
static int exec_event(int ch, uint8_t cmd, uint16_t *p)
{
    if (cmd == 0 || (cmd & 0x80))
        return 0;
    if (cmd & 0x01) { /* Instrument (0e12 -> 0b8a) */
        uint16_t i = rd16(*p);
        *p += 2;
        ch_vol[ch] = rd8((uint16_t)(i + 8));
        for (int k = 0; k < 5; k++) {
            static const uint8_t base[5] = {0x20, 0x40, 0x60, 0x80, 0xe0};
            opl_write((uint8_t)(base[k] + mod_op[ch]), rd8((uint16_t)(i + 2 + k)));
        }
        opl_write((uint8_t)(0xc0 + ch), rd8((uint16_t)(i + 1)));
        for (int k = 0; k < 5; k++) {
            static const uint8_t base[5] = {0x20, 0x40, 0x60, 0x80, 0xe0};
            opl_write((uint8_t)(base[k] + car_op[ch]), rd8((uint16_t)(i + 7 + k)));
        }
    }
    if (cmd & 0x02) { /* Lautstärke */
        ch_vol[ch] = rd8(*p);
        *p += 1;
        set_level(ch, ch_vol[ch]);
    }
    if (cmd & 0x04) /* Key-Off */
        opl_write((uint8_t)(0xb0 + ch), (uint8_t)(shadow[0xb0 + ch] & ~0x20));
    if (cmd & 0x38) { /* Frequenz / Key-On (0e99) */
        uint8_t keyon = (uint8_t)((shadow[0xb0 + ch] | cmd) & 0x20);
        uint16_t w;
        if (cmd & 0x08) {
            uint8_t i = (uint8_t)(rd8(*p) / 2);
            *p += 1;
            w = (uint16_t)(fnum_tab[i % 12] | (i / 12) << 10);
        } else if (cmd & 0x10) {
            w = rd16(*p);
            *p += 2;
        } else {
            w = (uint16_t)(shadow[0xa0 + ch] | shadow[0xb0 + ch] << 8);
        }
        opl_write((uint8_t)(0xa0 + ch), (uint8_t)(w & 0xff));
        opl_write((uint8_t)(0xb0 + ch), (uint8_t)((w >> 8) | keyon));
    }
    if (cmd & 0x40) { /* Tempo */
        uint16_t div = rd16(*p);
        *p += 2;
        if (div >= 100)
            tempo_div = div;
    }
    return 1;
}

/* 0ef5 */
static void dispatch_melodic(void)
{
    int n = active;
    for (int ch = 0; ch < NCH && n > 0; ch++) {
        if (!ch_ptr[ch])
            continue;
        n--;
        if (ch_next[ch] > song_time)
            continue;
        uint16_t p = ch_ptr[ch];
        uint8_t cmd = rd8(p++);
        if (cmd != 0x80 && !exec_event(ch, cmd, &p)) {
            ch_ptr[ch] = ch_next[ch] = 0;
            active--;
            key_off(ch);
            if (active == 0) {
                song_end();
                return;
            }
            continue;
        }
        ch_next[ch] = (uint16_t)(ch_next[ch] + rd8(p++));
        ch_ptr[ch] = p;
    }
    if (++song_time == 0)
        song_end();
}

/* 13d3 */
static void fade_step(void)
{
    int att = fade_acc >> 8;
    for (int ch = 0; ch < NCH; ch++) {
        uint8_t v = ch_vol[ch];
        if ((v & 0x3f) == 0x3f)
            continue;
        int tl = (v & 0x3f) + att;
        if (tl > 0x3f)
            tl = 0x3f;
        set_level(ch, (uint8_t)((v & 0xc0) | tl));
    }
    if (fade_acc == 0) {
        fade_rate = 0;
    } else if (fade_acc > 0x3f00) {
        fade_rate = 0;
        stopped = 1;
        opl_silence();
    } else {
        int32_t a = (int32_t)fade_acc + fade_rate;
        fade_acc = (uint16_t)(a < 0 ? 0 : a);
    }
}

/* 13aa */
static void music_tick(void)
{
    if (active == 0)
        return;
    do {
        restarted = 0;
        dispatch_melodic();
    } while (restarted);
    if (fade_rate)
        fade_step();
}

static double samples_per_tick(void)
{
    return (double)RATE * tempo_div * (speed ? speed : 1) / PIT_HZ;
}

/* Testhilfe MADTV_WAVOUT=datei.wav: Audioausgabe zusätzlich als WAV mitschreiben */
static FILE *g_wav;
static uint32_t g_wav_bytes;

static void wav_header(void)
{
    uint32_t rate = RATE, brate = RATE * 4, sz = g_wav_bytes;
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                     0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    uint32_t riff = sz + 36;
    memcpy(h + 4, &riff, 4);
    memcpy(h + 24, &rate, 4);
    memcpy(h + 28, &brate, 4);
    memcpy(h + 40, &sz, 4);
    fseek(g_wav, 0, SEEK_SET);
    fwrite(h, 1, 44, g_wav);
    fseek(g_wav, 0, SEEK_END);
    fflush(g_wav);
}

/* Ausgabepegel: die Emulation liegt mit Spitzen um -20 dBFS deutlich unter Vollaussteuerung */
#define GAIN 3

static int16_t amplify(int16_t v)
{
    int32_t a = (int32_t)v * GAIN;
    return (int16_t)(a > 32767 ? 32767 : a < -32768 ? -32768 : a);
}

static void audio_callback(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    for (int i = 0; i < frames; i++) {
        if (samples_left <= 0) {
            if (song && active && !stopped && !muted)
                music_tick();
            samples_left += samples_per_tick();
        }
        samples_left -= 1;
        int16_t s[4];
        OPL3_Generate(&g_chip, s);
        out[i * 2] = amplify(s[0]);
        out[i * 2 + 1] = amplify(s[1]);
    }
    if (g_wav) {
        fwrite(stream, 1, (size_t)len, g_wav);
        g_wav_bytes += (uint32_t)len;
        wav_header();
    }
}

static void lock(void)
{
    if (g_dev)
        SDL_LockAudioDevice(g_dev);
}

static void unlock(void)
{
    if (g_dev)
        SDL_UnlockAudioDevice(g_dev);
}

static int32_t status(void)
{
    return (muted == 0) | (stopped << 1) | ((fade_rate < 0) << 2) | ((fade_rate > 0) << 3);
}

/* Slot 0: Treiber initialisieren (p1 = Port, im Port ohne Bedeutung) */
int32_t psm_slot_0(int16_t p1, int16_t p2, int16_t p3, int16_t p4)
{
    (void)p1; (void)p2; (void)p3; (void)p4;
    OPL3_Reset(&g_chip, RATE);
    active = 0;
    stopped = 0;
    muted = 1;
    opl_reset();
    const char *ns = getenv("MADTV_NOSOUND");
    if (!(ns && atoi(ns)) && SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = RATE;
        want.format = AUDIO_S16SYS;
        want.channels = 2;
        want.samples = 1024;
        want.callback = audio_callback;
        g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        const char *wav = getenv("MADTV_WAVOUT");
        if (g_dev && wav) {
            g_wav = fopen(wav, "wb");
            if (g_wav)
                wav_header();
        }
        if (g_dev)
            SDL_PauseAudioDevice(g_dev, 0);
    }
    return 0;
}

/* Slot 1: Shutdown */
int32_t psm_slot_1(void)
{
    if (g_dev) {
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
    }
    opl_reset();
    free(song);
    song = NULL;
    return 0;
}

/* Slot 2: Treibername (wird vom Spiel nicht benutzt) */
int32_t psm_slot_2(void)
{
    return 0;
}

/* Slot 3: Zählerstruktur des Spiels registrieren (Port: Takt läuft unabhängig) */
int32_t psm_slot_3(void *tick_block)
{
    counters = tick_block;
    return 0;
}

int32_t psm_slot_4(void)
{
    return 0;
}

/* Slot 5: Musik ein/aus */
int32_t psm_slot_5(int16_t on)
{
    lock();
    int32_t st = status();
    muted = !(on & 1);
    if (muted)
        opl_silence();
    else if (!stopped)
        opl_restore_levels();
    unlock();
    return st;
}

/* Slot 6: Song übernehmen (Kopie, da das Spiel seinen Puffer später freigibt); Musik bleibt gestoppt */
int32_t psm_slot_6(void *data, uint16_t len)
{
    const uint8_t *d = data;
    if (len < 7 + 2 * NCH || d[3] > 2)
        return -4;
    lock();
    int was_muted = muted;
    muted = 1;
    opl_silence();
    free(song);
    song = malloc(len);
    if (song) {
        memcpy(song, d, len);
        song_len = len;
        song_parse();
    } else {
        active = 0;
    }
    stopped = 1; /* Typ 0 = Musik: gestoppt bis Slot 9 */
    loops_left = 0;
    samples_left = 0;
    muted = was_muted;
    unlock();
    return 0;
}

int32_t psm_slot_7(void)
{
    return 0;
}

int32_t psm_slot_8(void)
{
    return song_len;
}

/* Slot 9: Starten/Stoppen/Ein- und Ausblenden (dur in 1/100 s) */
int32_t psm_slot_9(int16_t flags, int16_t dur)
{
    lock();
    if (flags & 0x0c) {
        flags &= 0x0c;
        uint32_t t = (uint32_t)tempo_div * (uint32_t)(speed * 64u) / 11930u;
        if (t > 0xffff)
            t = 0xffff;
        uint32_t r = 0;
        if (dur && ((t << 8) / (uint16_t)dur) < 0x3f00)
            r = (t << 8) / (uint16_t)dur;
        if (r == 0) {
            if (!(flags & 4))
                flags |= 2;
            goto immediate;
        }
        int16_t rr = (int16_t)r;
        if (flags & 4)
            rr = (int16_t)-rr;
        int16_t old = fade_rate;
        fade_rate = rr;
        if (old == 0)
            fade_acc = rr < 0 ? 0x3f00 : 0x0001;
        if (!(flags & 8)) {
            int was = stopped;
            stopped = 0;
            if (was)
                opl_restore_levels();
        }
    } else {
immediate:
        fade_rate = 0;
        fade_acc = 0;
        stopped = (flags & 2) >> 1;
        if (stopped)
            opl_silence();
        else
            opl_restore_levels();
    }
    int32_t st = status();
    unlock();
    return st;
}

/* Slot 10: Soundeffekte – nur für SFX-Banken, im Spiel unbenutzt */
int32_t psm_slot_10(void)
{
    return -5;
}

int32_t psm_slot_11(void)
{
    return status();
}

/* Slot 12: Song mit Wiederholungszähler neu starten (im Spiel unbenutzt) */
int32_t psm_slot_12(void)
{
    return -5;
}
