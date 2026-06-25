/* ==========================================================================
 *
 * iMWrap v6 - A modern iMWrap implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef CIMWRAP_SHIM_H
#define CIMWRAP_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IMWrapBankHandle IMWrapBankHandle;
typedef struct IMWrapEngineHandle IMWrapEngineHandle;

typedef enum IMWrapAuthoringProfile {
    IMWrapAuthoringProfileGeneralMidi = 0,
    IMWrapAuthoringProfileMt32 = 1,
    IMWrapAuthoringProfileAdlib = 2
} IMWrapAuthoringProfile;

IMWrapBankHandle *imwrap_bank_create(void);
void imwrap_bank_destroy(IMWrapBankHandle *handle);
int imwrap_bank_load(IMWrapBankHandle *handle, const char *path, char *errorBuffer, size_t errorBufferSize);
size_t imwrap_bank_sound_count(const IMWrapBankHandle *handle);
uint16_t imwrap_bank_sound_id_at(const IMWrapBankHandle *handle, size_t index);
int imwrap_bank_sound_name(const IMWrapBankHandle *handle, uint16_t soundId, char *buffer, size_t bufferSize);
size_t imwrap_bank_event_count(const IMWrapBankHandle *handle, uint16_t soundId, IMWrapAuthoringProfile profile);
int imwrap_bank_event_summary(const IMWrapBankHandle *handle, uint16_t soundId, IMWrapAuthoringProfile profile, size_t index, uint32_t *tick, uint16_t *track, char *buffer, size_t bufferSize);
size_t imwrap_bank_track_count(const IMWrapBankHandle *handle, uint16_t soundId, IMWrapAuthoringProfile profile);
int imwrap_bank_track_summary(const IMWrapBankHandle *handle, uint16_t soundId, IMWrapAuthoringProfile profile, size_t trackIndex, char *nameBuffer, size_t nameBufferSize, size_t *eventCount);

IMWrapEngineHandle *imwrap_engine_create(const IMWrapBankHandle *bankHandle);
void imwrap_engine_destroy(IMWrapEngineHandle *handle);
void imwrap_engine_set_profile(IMWrapEngineHandle *handle, IMWrapAuthoringProfile profile);
void imwrap_engine_set_native_mt32_output(IMWrapEngineHandle *handle, int enabled);
int imwrap_engine_start_sound(IMWrapEngineHandle *handle, uint16_t soundId);
void imwrap_engine_stop_sound(IMWrapEngineHandle *handle, uint16_t soundId);
void imwrap_engine_stop_all(IMWrapEngineHandle *handle);
void imwrap_engine_advance(IMWrapEngineHandle *handle, uint32_t deltaTicks);
double imwrap_engine_ticks_per_second(const IMWrapEngineHandle *handle);
int imwrap_engine_get_sound_status(const IMWrapEngineHandle *handle, uint16_t soundId);
size_t imwrap_engine_active_sound_count(const IMWrapEngineHandle *handle);
uint16_t imwrap_engine_active_sound_id_at(const IMWrapEngineHandle *handle, size_t index);
int imwrap_engine_get_location(const IMWrapEngineHandle *handle, uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick);
int imwrap_engine_set_hook(IMWrapEngineHandle *handle, uint16_t soundId, uint8_t hookClass, uint8_t value, uint8_t channel);
int imwrap_engine_enable_fluidsynth(IMWrapEngineHandle *handle, const char *soundFontPath, char *errorBuffer, size_t errorBufferSize);
void imwrap_engine_render_fluidsynth(IMWrapEngineHandle *handle, uint32_t frameCount, float *left, float *right);
void imwrap_engine_disable_fluidsynth(IMWrapEngineHandle *handle);

typedef void (*imwrap_midi_message_callback_t)(void *userData, uint16_t soundId, uint8_t status, uint8_t data1, uint8_t data2);
typedef void (*imwrap_sysex_callback_t)(void *userData, uint16_t soundId, const uint8_t *data, uint32_t length);

void imwrap_engine_set_callbacks(IMWrapEngineHandle *handle,
                                imwrap_midi_message_callback_t midiCallback,
                                imwrap_sysex_callback_t sysexCallback,
                                void *userData);

int imwrap_engine_enable_mt32(IMWrapEngineHandle *handle, const char *romDir, char *errorBuffer, size_t errorBufferSize);
void imwrap_engine_render_mt32(IMWrapEngineHandle *handle, uint32_t frameCount, float *left, float *right);
void imwrap_engine_disable_mt32(IMWrapEngineHandle *handle);

int imwrap_engine_enable_adlib(IMWrapEngineHandle *handle, char *errorBuffer, size_t errorBufferSize);
void imwrap_engine_render_adlib(IMWrapEngineHandle *handle, uint32_t frameCount, float *left, float *right);
void imwrap_engine_disable_adlib(IMWrapEngineHandle *handle);

void imwrap_register_roland_timbre_mapping(const char *name, uint8_t gmProgram);
void imwrap_clear_roland_timbre_mappings(void);

#ifdef __cplusplus
}
#endif

#endif
