/* Passenger voices with Windows speech (SAPI 5) - see voice.h. Own code. */
#define COBJMACROS
#include <windows.h>
#include <sapi.h>
#include <stdio.h>
#include <string.h>
#include "voice.h"

/* the GUIDs from sapi.h, written here so no extra import library is needed */
static const GUID clsid_sp_voice = {0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}};
static const GUID iid_isp_voice = {0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}};

#define VOICE_QUEUE 4
#define VOICE_CHARS 1024

static VoiceLogFn vlog;
static SRWLOCK vlock = SRWLOCK_INIT;
static HANDLE wake;
static volatile LONG started, failed, want_stop, volume = 100, volume_dirty = 1;
static wchar_t queue[VOICE_QUEUE][VOICE_CHARS];
static int queue_count;

/* the text as SAPI XML: the voice chosen by language and gender, its pitch and speed, the text escaped */
static void make_xml(wchar_t *out, size_t cap, const char *utf8, int german, int female, int pitch, int rate) {
    wchar_t text[400];
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, text, (int)(sizeof(text) / sizeof(text[0])));
    if (n <= 0) text[0] = 0;
    size_t len = (size_t)_snwprintf(out, cap, L"<voice required=\"Language=%ls\" optional=\"Gender=%ls\"><pitch absmiddle=\"%d\"><rate absspeed=\"%d\">",
                                    german ? L"407" : L"409", female ? L"Female" : L"Male", pitch, rate);
    if (len >= cap) { out[0] = 0; return; }
    for (const wchar_t *p = text; *p && len + 8 < cap; ++p) {
        const wchar_t *rep = *p == L'&' ? L"&amp;" : *p == L'<' ? L"&lt;" : *p == L'>' ? L"&gt;" : NULL;
        if (rep) while (*rep) out[len++] = *rep++;
        else out[len++] = *p;
    }
    out[len] = 0;
    if (len + 32 < cap) wcscat(out, L"</rate></pitch></voice>");
}

static DWORD WINAPI voice_thread(void *arg) {
    (void)arg;
    HRESULT init = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ISpVoice *v = NULL;
    HRESULT hr = CoCreateInstance(&clsid_sp_voice, NULL, CLSCTX_ALL, &iid_isp_voice, (void **)&v);
    if (FAILED(hr) || !v) {
        InterlockedExchange(&failed, 1);
        if (vlog) vlog("voices: Windows speech is not available (0x%08lx)", (unsigned long)hr);
        if (SUCCEEDED(init)) CoUninitialize();
        return 0;
    }
    if (vlog) vlog("voices: Windows speech ready");
    static wchar_t line[VOICE_CHARS];
    for (;;) {
        WaitForSingleObject(wake, INFINITE);
        if (InterlockedExchange(&want_stop, 0)) ISpVoice_Speak(v, NULL, SPF_PURGEBEFORESPEAK, NULL);
        if (InterlockedExchange(&volume_dirty, 0)) ISpVoice_SetVolume(v, (USHORT)volume);
        for (;;) {
            int have = 0;
            AcquireSRWLockExclusive(&vlock);
            if (queue_count > 0) {
                memcpy(line, queue[0], sizeof(line));
                memmove(queue[0], queue[1], sizeof(queue[0]) * (size_t)(VOICE_QUEUE - 1));
                queue_count--;
                have = 1;
            }
            ReleaseSRWLockExclusive(&vlock);
            if (!have) break;
            /* async: SAPI queues the lines and says them one after the other */
            hr = ISpVoice_Speak(v, line, SPF_ASYNC | SPF_IS_XML, NULL);
            if (FAILED(hr) && vlog) vlog("voices: speaking failed (0x%08lx)", (unsigned long)hr);
        }
    }
}

int voice_start(VoiceLogFn log) {
    if (failed) return 0;
    if (InterlockedCompareExchange(&started, 1, 0) != 0) return 1;
    vlog = log;
    wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE t = wake ? CreateThread(NULL, 0, voice_thread, NULL, 0, NULL) : NULL;
    if (!t) {
        InterlockedExchange(&failed, 1);
        if (vlog) vlog("voices: could not start the speech thread");
        return 0;
    }
    CloseHandle(t);
    return 1;
}

void voice_say(const char *utf8, int german, int female, int pitch, int rate) {
    if (!started || failed || !utf8 || !utf8[0]) return;
    AcquireSRWLockExclusive(&vlock);
    if (queue_count == VOICE_QUEUE) { /* drop the oldest that wasn't said yet */
        memmove(queue[0], queue[1], sizeof(queue[0]) * (size_t)(VOICE_QUEUE - 1));
        queue_count--;
    }
    make_xml(queue[queue_count++], VOICE_CHARS, utf8, german, female, pitch, rate);
    ReleaseSRWLockExclusive(&vlock);
    SetEvent(wake);
}

void voice_stop(void) {
    if (!started) return;
    AcquireSRWLockExclusive(&vlock);
    queue_count = 0;
    ReleaseSRWLockExclusive(&vlock);
    InterlockedExchange(&want_stop, 1);
    SetEvent(wake);
}

void voice_volume(int v) {
    InterlockedExchange(&volume, v < 0 ? 0 : v > 100 ? 100 : v);
    InterlockedExchange(&volume_dirty, 1);
    if (started) SetEvent(wake);
}
