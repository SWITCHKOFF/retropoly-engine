// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/pc/audio_sdl.cpp
//
// HAL PC: аудио-заглушка (sprint_05_06.md, недели 9 и 12; Полный PCM-микс —
// post-MVP). SDL3 device-stream (mono 22.05 кГц, SDL_AUDIO_U8) с
// on-demand callback: data генерируется в audio-треде, управление beep
// — через SDL-атомики (главный поток не блокируется и не общается с
// audio-тредом блокировками).
//
// Формат сигнала: квадратная волна умеренной амплитуды (без float):
// центр U8 = 128, отклонение ±AUDIO_BEEP_AMPLITUDE; фаза накапливается
// в audio-треде (callback — единственный её писатель).
#include "hal/platform.h"

#include <SDL3/SDL.h>

#include <stdint.h>

/// Частота дискретизации заглушки, Гц (mono 22 кГц, диагностический beep).
static constexpr int AUDIO_SAMPLE_RATE = 22050;

/// Максимум байт, генерируемых callback-ом за один заход (стековый буфер).
static constexpr int AUDIO_CHUNK_BYTES = 256;

/// Максимальная принимаемая частота beep (вне диапазона — тишина, не crash).
static constexpr uint32_t AUDIO_BEEP_MAX_FREQUENCY = 20000u;

/// Отклонение от центра U8 (умеренная громкость diagnostic-сигнала).
static constexpr uint8_t AUDIO_BEEP_AMPLITUDE = 48u;

/// Поток воспроизведения (NULL — сервис не инициализирован).
static SDL_AudioStream* s_stream = nullptr;

/// Активная частота beep, Гц (0 — тишина). Управляет главный поток.
static SDL_AtomicInt s_beep_frequency;

/// Оставшиеся сэмплы активного beep. Единственный писатель — audio-тред.
static SDL_AtomicInt s_beep_samples_left;

/// Фаза квадратной волны внутри периода (сэмплы). Писатель — audio-тред.
static uint32_t s_wave_phase;

/// Заполнение буфера сэмплами и состояния beep (обновляет s_wave_phase).
/// Клип count до размера стекового буфера: инвариант становится доказуемым
/// (иначе GCC -O3 stringop-overflow false-positive на unroll); для
/// текущих вызовов клип — no-op (callback уже ограничил chunk).
//
// Известное ложное срабатывание GCC (vectorized unroll-хвост однобайтовых
// записей трактуется как 16-байтовый store за пределами доказуемого bound):
// подавляем -Wstringop-overflow точечно и локально, с объяснением.
// MSVC-путь не затрагивается (прагма под __GNUC__).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

static void hal_audio_fill(uint8_t* buffer, int count)
{
    if (count > AUDIO_CHUNK_BYTES) {
        count = AUDIO_CHUNK_BYTES;
    }
    const int frequency = SDL_GetAtomicInt(&s_beep_frequency);
    int samples_left = SDL_GetAtomicInt(&s_beep_samples_left);
    const bool tone_active = (frequency >= 1 &&
                              (uint32_t)frequency <= AUDIO_BEEP_MAX_FREQUENCY &&
                              samples_left > 0);

    for (int i = 0; i < count; ++i) {
        if (!tone_active || samples_left <= 0) {
            buffer[i] = 128u; // тишина: центр U8
            continue;
        }
        const uint32_t period = (uint32_t)(AUDIO_SAMPLE_RATE / frequency);
        buffer[i] = (s_wave_phase < period / 2u)
                        ? (uint8_t)(128u + AUDIO_BEEP_AMPLITUDE)
                        : (uint8_t)(128u - AUDIO_BEEP_AMPLITUDE);
        ++s_wave_phase;
        if (s_wave_phase >= period) {
            s_wave_phase = 0u;
        }
        --samples_left;
    }

    if (tone_active) {
        SDL_SetAtomicInt(&s_beep_samples_left, samples_left);
    }
}

/// On-demand генерация: пушим данные, пока стрим их запрашивает.
static void SDLCALL hal_audio_callback(void* userdata, SDL_AudioStream* stream,
                                       int additional_amount, int total_amount)
{
    (void)userdata;
    (void)total_amount;

    uint8_t buffer[AUDIO_CHUNK_BYTES];
    while (additional_amount > 0) {
        const int chunk = (additional_amount < AUDIO_CHUNK_BYTES)
                              ? additional_amount
                              : AUDIO_CHUNK_BYTES;
        hal_audio_fill(buffer, chunk);
        if (!SDL_PutAudioStreamData(stream, buffer, chunk)) {
            return; // прерывает request-цикл (без блокировок)
        }
        additional_amount -= chunk;
    }
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

bool Audio_Init(void)
{
    if (s_stream != nullptr) {
        return true; // идемпотентность
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_U8;
    spec.channels = 1;

    const SDL_AudioStreamCallback callback = hal_audio_callback;
    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                         &spec, callback, nullptr);
    if (s_stream == nullptr) {
        return false; // аудио-девайс недоступен — сервис молчит (не crash)
    }
    if (!SDL_ResumeAudioStreamDevice(s_stream)) {
        SDL_DestroyAudioStream(s_stream);
        s_stream = nullptr;
        return false;
    }

    SDL_SetAtomicInt(&s_beep_frequency, 0);
    SDL_SetAtomicInt(&s_beep_samples_left, 0);
    s_wave_phase = 0u;
    return true;
}

void Audio_Shutdown(void)
{
    if (s_stream == nullptr) {
        return;
    }
    SDL_SetAtomicInt(&s_beep_frequency, 0); // мгновенная тишина
    SDL_SetAtomicInt(&s_beep_samples_left, 0);
    SDL_DestroyAudioStream(s_stream);
    s_stream = nullptr;
}

void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms)
{
    if (s_stream == nullptr || frequency == 0u ||
        frequency > AUDIO_BEEP_MAX_FREQUENCY || duration_ms == 0u) {
        return;
    }

    const uint64_t samples =
        (uint64_t)duration_ms * (uint64_t)AUDIO_SAMPLE_RATE / 1000u;
    if (samples > (uint64_t)2147483647) {
        return; // переполнение int-атомики — отказ (чистая тишина)
    }

    // Атомарная последовательность: тишина → частота → длительность, чтобы
    // audio-тред читал согласованное состояние.
    SDL_SetAtomicInt(&s_beep_samples_left, 0);
    SDL_SetAtomicInt(&s_beep_frequency, (int)frequency);
    SDL_SetAtomicInt(&s_beep_samples_left, (int)samples);
}
