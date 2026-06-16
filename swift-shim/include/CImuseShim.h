/* ==========================================================================
 *
 * iMWRAP V6 - A modern iMuse implementation attempt with Adventure Game Studio Companion Plugin
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

#ifndef CIMUSE_SHIM_H
#define CIMUSE_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ImuseBankHandle ImuseBankHandle;
typedef struct ImuseEngineHandle ImuseEngineHandle;

typedef enum ImuseAuthoringProfile {
    ImuseAuthoringProfileGeneralMidi = 0,
    ImuseAuthoringProfileMt32 = 1
} ImuseAuthoringProfile;

ImuseBankHandle *imuse_bank_create(void);
void imuse_bank_destroy(ImuseBankHandle *handle);
int imuse_bank_load(ImuseBankHandle *handle, const char *path, char *errorBuffer, size_t errorBufferSize);
size_t imuse_bank_sound_count(const ImuseBankHandle *handle);
uint16_t imuse_bank_sound_id_at(const ImuseBankHandle *handle, size_t index);
int imuse_bank_sound_name(const ImuseBankHandle *handle, uint16_t soundId, char *buffer, size_t bufferSize);
size_t imuse_bank_event_count(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile);
int imuse_bank_event_summary(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile, size_t index, uint32_t *tick, uint16_t *track, char *buffer, size_t bufferSize);
size_t imuse_bank_track_count(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile);
int imuse_bank_track_summary(const ImuseBankHandle *handle, uint16_t soundId, ImuseAuthoringProfile profile, size_t trackIndex, char *nameBuffer, size_t nameBufferSize, size_t *eventCount);

ImuseEngineHandle *imuse_engine_create(const ImuseBankHandle *bankHandle);
void imuse_engine_destroy(ImuseEngineHandle *handle);
void imuse_engine_set_profile(ImuseEngineHandle *handle, ImuseAuthoringProfile profile);
void imuse_engine_set_native_mt32_output(ImuseEngineHandle *handle, int enabled);
int imuse_engine_start_sound(ImuseEngineHandle *handle, uint16_t soundId);
void imuse_engine_stop_sound(ImuseEngineHandle *handle, uint16_t soundId);
void imuse_engine_stop_all(ImuseEngineHandle *handle);
void imuse_engine_advance(ImuseEngineHandle *handle, uint32_t deltaTicks);
double imuse_engine_ticks_per_second(const ImuseEngineHandle *handle);
int imuse_engine_get_sound_status(const ImuseEngineHandle *handle, uint16_t soundId);
size_t imuse_engine_active_sound_count(const ImuseEngineHandle *handle);
uint16_t imuse_engine_active_sound_id_at(const ImuseEngineHandle *handle, size_t index);
int imuse_engine_get_location(const ImuseEngineHandle *handle, uint16_t soundId, uint16_t *track, uint16_t *beat, uint16_t *tick);
int imuse_engine_set_hook(ImuseEngineHandle *handle, uint16_t soundId, uint8_t hookClass, uint8_t value, uint8_t channel);
int imuse_engine_enable_fluidsynth(ImuseEngineHandle *handle, const char *soundFontPath, char *errorBuffer, size_t errorBufferSize);
void imuse_engine_render_fluidsynth(ImuseEngineHandle *handle, uint32_t frameCount, float *left, float *right);
void imuse_engine_disable_fluidsynth(ImuseEngineHandle *handle);

typedef void (*imuse_midi_message_callback_t)(void *userData, uint16_t soundId, uint8_t status, uint8_t data1, uint8_t data2);
typedef void (*imuse_sysex_callback_t)(void *userData, uint16_t soundId, const uint8_t *data, uint32_t length);

void imuse_engine_set_callbacks(ImuseEngineHandle *handle,
                                imuse_midi_message_callback_t midiCallback,
                                imuse_sysex_callback_t sysexCallback,
                                void *userData);

int imuse_engine_enable_mt32(ImuseEngineHandle *handle, const char *romDir, char *errorBuffer, size_t errorBufferSize);
void imuse_engine_render_mt32(ImuseEngineHandle *handle, uint32_t frameCount, float *left, float *right);
void imuse_engine_disable_mt32(ImuseEngineHandle *handle);

void imuse_register_roland_timbre_mapping(const char *name, uint8_t gmProgram);
void imuse_clear_roland_timbre_mappings(void);

#ifdef __cplusplus
}
#endif

#endif
