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

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace AgsImuse
{
    public enum ImuseAuthoringProfile : int
    {
        GeneralMidi = 0,
        Mt32 = 1
    }

    public static class ImuseShim
    {
        private const string DLL_NAME = "imuse_shim.dll";

        // Callbacks
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void MidiMessageCallback(IntPtr userData, ushort soundId, byte status, byte data1, byte data2);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SysExCallback(IntPtr userData, ushort soundId, IntPtr data, uint length);

        // Bank Functions
        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_create", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr BankCreate();

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_destroy", CallingConvention = CallingConvention.Cdecl)]
        public static extern void BankDestroy(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_load", CallingConvention = CallingConvention.Cdecl)]
        public static extern int BankLoad(IntPtr handle, [MarshalAs(UnmanagedType.LPStr)] string path, StringBuilder errorBuffer, UIntPtr errorBufferSize);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_sound_count", CallingConvention = CallingConvention.Cdecl)]
        public static extern UIntPtr BankSoundCount(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_sound_id_at", CallingConvention = CallingConvention.Cdecl)]
        public static extern ushort BankSoundIdAt(IntPtr handle, UIntPtr index);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_sound_name", CallingConvention = CallingConvention.Cdecl)]
        public static extern int BankSoundName(IntPtr handle, ushort soundId, StringBuilder buffer, UIntPtr bufferSize);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_event_count", CallingConvention = CallingConvention.Cdecl)]
        public static extern UIntPtr BankEventCount(IntPtr handle, ushort soundId, ImuseAuthoringProfile profile);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_event_summary", CallingConvention = CallingConvention.Cdecl)]
        public static extern int BankEventSummary(IntPtr handle, ushort soundId, ImuseAuthoringProfile profile, UIntPtr index, out uint tick, out ushort track, StringBuilder buffer, UIntPtr bufferSize);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_track_count", CallingConvention = CallingConvention.Cdecl)]
        public static extern UIntPtr BankTrackCount(IntPtr handle, ushort soundId, ImuseAuthoringProfile profile);

        [DllImport(DLL_NAME, EntryPoint = "imuse_bank_track_summary", CallingConvention = CallingConvention.Cdecl)]
        public static extern int BankTrackSummary(IntPtr handle, ushort soundId, ImuseAuthoringProfile profile, UIntPtr trackIndex, StringBuilder nameBuffer, UIntPtr nameBufferSize, out UIntPtr eventCount);

        // Engine Functions
        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_create", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr EngineCreate(IntPtr bankHandle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_destroy", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineDestroy(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_set_profile", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineSetProfile(IntPtr handle, ImuseAuthoringProfile profile);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_set_native_mt32_output", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineSetNativeMt32Output(IntPtr handle, int enabled);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_start_sound", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineStartSound(IntPtr handle, ushort soundId);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_stop_sound", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineStopSound(IntPtr handle, ushort soundId);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_stop_all", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineStopAll(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_advance", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineAdvance(IntPtr handle, uint deltaTicks);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_ticks_per_second", CallingConvention = CallingConvention.Cdecl)]
        public static extern double EngineTicksPerSecond(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_get_sound_status", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineGetSoundStatus(IntPtr handle, ushort soundId);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_active_sound_count", CallingConvention = CallingConvention.Cdecl)]
        public static extern UIntPtr EngineActiveSoundCount(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_active_sound_id_at", CallingConvention = CallingConvention.Cdecl)]
        public static extern ushort EngineActiveSoundIdAt(IntPtr handle, UIntPtr index);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_get_location", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineGetLocation(IntPtr handle, ushort soundId, out ushort track, out ushort beat, out ushort tick);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_set_hook", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineSetHook(IntPtr handle, ushort soundId, byte hookClass, byte value, byte channel);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_enable_fluidsynth", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineEnableFluidsynth(IntPtr handle, [MarshalAs(UnmanagedType.LPStr)] string soundFontPath, StringBuilder errorBuffer, UIntPtr errorBufferSize);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_render_fluidsynth", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineRenderFluidsynth(IntPtr handle, uint frameCount, [In, Out] float[] left, [In, Out] float[] right);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_disable_fluidsynth", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineDisableFluidsynth(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_set_callbacks", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineSetCallbacks(IntPtr handle, MidiMessageCallback midiCallback, SysExCallback sysexCallback, IntPtr userData);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_enable_mt32", CallingConvention = CallingConvention.Cdecl)]
        public static extern int EngineEnableMt32(IntPtr handle, [MarshalAs(UnmanagedType.LPStr)] string romDir, StringBuilder errorBuffer, UIntPtr errorBufferSize);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_render_mt32", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineRenderMt32(IntPtr handle, uint frameCount, [In, Out] float[] left, [In, Out] float[] right);

        [DllImport(DLL_NAME, EntryPoint = "imuse_engine_disable_mt32", CallingConvention = CallingConvention.Cdecl)]
        public static extern void EngineDisableMt32(IntPtr handle);

        [DllImport(DLL_NAME, EntryPoint = "imuse_register_roland_timbre_mapping", CallingConvention = CallingConvention.Cdecl)]
        public static extern void RegisterRolandTimbreMapping([MarshalAs(UnmanagedType.LPStr)] string name, byte gmProgram);

        [DllImport(DLL_NAME, EntryPoint = "imuse_clear_roland_timbre_mappings", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ClearRolandTimbreMappings();
    }
}
