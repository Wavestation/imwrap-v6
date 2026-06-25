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

Imports System
Imports System.Runtime.InteropServices
Imports System.Text

Namespace AgsIMWrap

    Public Enum IMWrapAuthoringProfile As Integer
        GeneralMidi = 0
        Mt32 = 1
        Adlib = 2
    End Enum

    Public NotInheritable Class IMWrapShim
        Private Const DLL_NAME As String = "imwrap_shim.dll"

        Private Sub New()
        End Sub

        ' Callbacks
        <UnmanagedFunctionPointer(CallingConvention.Cdecl)>
        Public Delegate Sub MidiMessageCallback(ByVal userData As IntPtr, ByVal soundId As UShort, ByVal status As Byte, ByVal data1 As Byte, ByVal data2 As Byte)

        <UnmanagedFunctionPointer(CallingConvention.Cdecl)>
        Public Delegate Sub SysExCallback(ByVal userData As IntPtr, ByVal soundId As UShort, ByVal data As IntPtr, ByVal length As UInteger)

        ' Bank Functions
        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_create", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankCreate() As IntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_destroy", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub BankDestroy(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_load", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankLoad(ByVal handle As IntPtr, <MarshalAs(UnmanagedType.LPStr)> ByVal path As String, ByVal errorBuffer As StringBuilder, ByVal errorBufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_sound_count", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankSoundCount(ByVal handle As IntPtr) As UIntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_sound_id_at", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankSoundIdAt(ByVal handle As IntPtr, ByVal index As UIntPtr) As UShort
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_sound_name", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankSoundName(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal buffer As StringBuilder, ByVal bufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_event_count", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankEventCount(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal profile As IMWrapAuthoringProfile) As UIntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_event_summary", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankEventSummary(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal profile As IMWrapAuthoringProfile, ByVal index As UIntPtr, <Out> ByRef tick As UInteger, <Out> ByRef track As UShort, ByVal buffer As StringBuilder, ByVal bufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_track_count", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankTrackCount(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal profile As IMWrapAuthoringProfile) As UIntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_bank_track_summary", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function BankTrackSummary(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal profile As IMWrapAuthoringProfile, ByVal trackIndex As UIntPtr, ByVal nameBuffer As StringBuilder, ByVal nameBufferSize As UIntPtr, <Out> ByRef eventCount As UIntPtr) As Integer
        End Function

        ' Engine Functions
        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_create", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineCreate(ByVal bankHandle As IntPtr) As IntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_destroy", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineDestroy(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_set_profile", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineSetProfile(ByVal handle As IntPtr, ByVal profile As IMWrapAuthoringProfile)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_set_native_mt32_output", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineSetNativeMt32Output(ByVal handle As IntPtr, ByVal enabled As Integer)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_start_sound", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineStartSound(ByVal handle As IntPtr, ByVal soundId As UShort) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_stop_sound", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineStopSound(ByVal handle As IntPtr, ByVal soundId As UShort)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_stop_all", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineStopAll(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_advance", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineAdvance(ByVal handle As IntPtr, ByVal deltaTicks As UInteger)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_ticks_per_second", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineTicksPerSecond(ByVal handle As IntPtr) As Double
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_get_sound_status", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineGetSoundStatus(ByVal handle As IntPtr, ByVal soundId As UShort) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_active_sound_count", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineActiveSoundCount(ByVal handle As IntPtr) As UIntPtr
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_active_sound_id_at", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineActiveSoundIdAt(ByVal handle As IntPtr, ByVal index As UIntPtr) As UShort
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_get_location", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineGetLocation(ByVal handle As IntPtr, ByVal soundId As UShort, <Out> ByRef track As UShort, <Out> ByRef beat As UShort, <Out> ByRef tick As UShort) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_set_hook", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineSetHook(ByVal handle As IntPtr, ByVal soundId As UShort, ByVal hookClass As Byte, ByVal value As Byte, ByVal channel As Byte) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_enable_fluidsynth", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineEnableFluidsynth(ByVal handle As IntPtr, <MarshalAs(UnmanagedType.LPStr)> ByVal soundFontPath As String, ByVal errorBuffer As StringBuilder, ByVal errorBufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_render_fluidsynth", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineRenderFluidsynth(ByVal handle As IntPtr, ByVal frameCount As UInteger, <In, Out> ByVal left() As Single, <In, Out> ByVal right() As Single)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_disable_fluidsynth", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineDisableFluidsynth(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_set_callbacks", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineSetCallbacks(ByVal handle As IntPtr, ByVal midiCallback As MidiMessageCallback, ByVal sysexCallback As SysExCallback, ByVal userData As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_enable_mt32", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineEnableMt32(ByVal handle As IntPtr, <MarshalAs(UnmanagedType.LPStr)> ByVal romDir As String, ByVal errorBuffer As StringBuilder, ByVal errorBufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_render_mt32", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineRenderMt32(ByVal handle As IntPtr, ByVal frameCount As UInteger, <In, Out> ByVal left() As Single, <In, Out> ByVal right() As Single)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_disable_mt32", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineDisableMt32(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_enable_adlib", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Function EngineEnableAdlib(ByVal handle As IntPtr, ByVal errorBuffer As StringBuilder, ByVal errorBufferSize As UIntPtr) As Integer
        End Function

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_render_adlib", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineRenderAdlib(ByVal handle As IntPtr, ByVal frameCount As UInteger, <In, Out> ByVal left() As Single, <In, Out> ByVal right() As Single)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_engine_disable_adlib", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub EngineDisableAdlib(ByVal handle As IntPtr)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_register_roland_timbre_mapping", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub RegisterRolandTimbreMapping(<MarshalAs(UnmanagedType.LPStr)> ByVal name As String, ByVal gmProgram As Byte)
        End Sub

        <DllImport(DLL_NAME, EntryPoint:="imwrap_clear_roland_timbre_mappings", CallingConvention:=CallingConvention.Cdecl)>
        Public Shared Sub ClearRolandTimbreMappings()
        End Sub
    End Class
End Namespace
