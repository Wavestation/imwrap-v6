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

import AppKit
import AVFoundation
import CImuseShim
import SwiftUI
import CoreMIDI

public enum OutputBackend: String, CaseIterable, Identifiable {
    case fluidSynth = "FluidSynth"
    case adlib = "AdLib OPL2"
    case mt32emu = "MT-32 Emulation"
    case coreMidi = "CoreMIDI Out"
    
    public var id: String { self.rawValue }
}

public struct MidiDestinationInfo: Identifiable, Hashable {
    public let id: MIDIUniqueID
    public let name: String
    
    public init(id: MIDIUniqueID, name: String) {
        self.id = id
        self.name = name
    }
}

extension ImuseAuthoringProfile: Hashable {}

// Structs
public struct TrackSummary: Identifiable, Hashable {
    public let id: String
    public let index: Int
    public let name: String
    public let eventCount: Int

    public init(id: String, index: Int, name: String, eventCount: Int) {
        self.id = id
        self.index = index
        self.name = name
        self.eventCount = eventCount
    }
}

public struct SoundSummary: Identifiable, Hashable {
    public let id: UInt16
    public let name: String
    public var tracks: [TrackSummary] = []
    
    public init(id: UInt16, name: String, tracks: [TrackSummary] = []) {
        self.id = id
        self.name = name
        self.tracks = tracks
    }
}

public struct EventSummary: Identifiable {
    public let id: Int
    public let tick: UInt32
    public let track: UInt16
    public let text: String
    
    public init(id: Int, tick: UInt32, track: UInt16, text: String) {
        self.id = id
        self.tick = tick
        self.track = track
        self.text = text
    }
}

public struct ActiveSoundStatus: Identifiable {
    public let id: UInt16
    public let track: UInt16
    public let beat: UInt16
    public let tick: UInt16
    
    public init(id: UInt16, track: UInt16, beat: UInt16, tick: UInt16) {
        self.id = id
        self.track = track
        self.beat = beat
        self.tick = tick
    }
}

@MainActor
public final class AuthoringViewModel: ObservableObject {
    @Published public var bankPath: String = ""
    @Published public var soundFontPath: String = ""
    @Published public var errorMessage: String = ""
    @Published public var sounds: [SoundSummary] = []
    @Published public var selectedSoundID: UInt16? = nil
    @Published public var events: [EventSummary] = []
    @Published public var activeSounds: [ActiveSoundStatus] = []
    @Published public var advanceTicksText: String = "480"
    @Published public var hookClassText: String = "0"
    @Published public var hookValueText: String = "2"
    @Published public var hookChannelText: String = "0"
    @Published public var previewEnabled: Bool = false
    @Published public var transportRunning: Bool = false
    @Published public var outputBackend: OutputBackend = .fluidSynth {
        didSet {
            handleBackendChange()
        }
    }
    @Published public var mt32RomDirPath: String = ""
    @Published public var selectedMidiDestinationID: MIDIUniqueID? = nil
    @Published public var midiDestinations: [MidiDestinationInfo] = []

    @Published public var selectedProfile: ImuseAuthoringProfile = ImuseAuthoringProfileGeneralMidi {
        didSet {
            updateEngineProfile()
            reloadSoundList()
        }
    }

    private var midiClient: MIDIClientRef = 0
    private var midiOutputPort: MIDIPortRef = 0

    private var bankHandle: OpaquePointer?
    private var engineHandle: OpaquePointer?
    private var transportTimer: Timer?
    private var tickAccumulator: Double = 0.0
    private var lastTransportTime: Double = 0.0
    private var audioEngine: AVAudioEngine?
    private var audioSourceNode: AVAudioSourceNode?
    private var didBootstrap = false

    public init() {
        configureDefaultAssetPaths()
        setupCoreMIDI()
        refreshMidiDestinations()
    }

    deinit {
        transportTimer?.invalidate()
        transportTimer = nil
        audioEngine?.stop()
        audioEngine = nil
        audioSourceNode = nil
        if let engineHandle {
            imuse_engine_destroy(engineHandle)
        }
        if let bankHandle {
            imuse_bank_destroy(bankHandle)
        }
        if midiOutputPort != 0 {
            MIDIPortDispose(midiOutputPort)
        }
        if midiClient != 0 {
            MIDIClientDispose(midiClient)
        }
    }

    public func browseBank() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.data]
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Choisir"
        if panel.runModal() == .OK, let url = panel.url {
            bankPath = url.path
            loadBank()
        }
    }

    public func browseSoundFont() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.data]
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Choisir"
        if panel.runModal() == .OK, let url = panel.url {
            soundFontPath = url.path
        }
    }

    public func bootstrapIfPossible() {
        guard !didBootstrap else {
            return
        }
        didBootstrap = true

        if !bankPath.isEmpty {
            loadBank()
        }
        if !soundFontPath.isEmpty && engineHandle != nil {
            enablePreview()
        }
    }

    public func browseMt32RomDir() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.prompt = "Choisir dossier ROMs"
        if panel.runModal() == .OK, let url = panel.url {
            mt32RomDirPath = url.path
        }
    }

    public func loadBank() {
        stopTransport()
        cleanupHandles()

        let bank = imuse_bank_create()
        var errorBuffer = [CChar](repeating: 0, count: 512)
        let ok = bankPath.withCString { pathPtr in
            imuse_bank_load(bank, pathPtr, &errorBuffer, errorBuffer.count)
        }

        guard ok == 1 else {
            errorMessage = String(cString: errorBuffer)
            imuse_bank_destroy(bank)
            return
        }

        let engine = imuse_engine_create(bank)
        imuse_engine_set_profile(engine, selectedProfile)
        let nativeMt32 = (outputBackend == .mt32emu || (outputBackend == .coreMidi && selectedProfile == ImuseAuthoringProfileMt32)) ? 1 : 0
        imuse_engine_set_native_mt32_output(engine, Int32(nativeMt32))
        
        // Setup Swift callbacks for CoreMIDI
        imuse_engine_set_callbacks(engine, { (userData, soundId, status, data1, data2) in
            guard let userData else { return }
            let viewModel = Unmanaged<AuthoringViewModel>.fromOpaque(userData).takeUnretainedValue()
            viewModel.sendMidiShortMessage(status: status, data1: data1, data2: data2)
        }, { (userData, soundId, dataPtr, length) in
            guard let userData, let dataPtr else { return }
            let viewModel = Unmanaged<AuthoringViewModel>.fromOpaque(userData).takeUnretainedValue()
            let bytes = Array(UnsafeBufferPointer(start: dataPtr, count: Int(length)))
            viewModel.sendMidiSysExMessage(data: bytes)
        }, Unmanaged.passUnretained(self).toOpaque())

        bankHandle = bank
        engineHandle = engine
        previewEnabled = false
        errorMessage = ""
        UserDefaults.standard.set(bankPath, forKey: "LastLoadedImsPath")
        reloadSoundList()
        refreshActiveSounds()
    }

    public func enablePreview() {
        guard let engineHandle else {
            errorMessage = "Charge d'abord une banque iMUSE."
            return
        }
        
        handleBackendChange()
        
        switch outputBackend {
        case .fluidSynth:
            previewEnabled = (audioEngine != nil)
        case .adlib:
            previewEnabled = (audioEngine != nil)
        case .mt32emu:
            previewEnabled = (audioEngine != nil)
        case .coreMidi:
            previewEnabled = (midiOutputPort != 0)
        }
    }

    public func disablePreview() {
        stopTransport()
        stopAudioOutput()
        disableCoreMIDIOutput()
        guard let engineHandle else {
            previewEnabled = false
            return
        }
        imuse_engine_disable_fluidsynth(engineHandle)
        imuse_engine_disable_mt32(engineHandle)
        imuse_engine_disable_adlib(engineHandle)
        previewEnabled = false
    }

    public func reloadSoundList() {
        guard let bankHandle else { return }
        let count = imuse_bank_sound_count(bankHandle)
        var newSounds: [SoundSummary] = []
        newSounds.reserveCapacity(count)

        for index in 0..<count {
            let soundID = imuse_bank_sound_id_at(bankHandle, index)
            var nameBuffer = [CChar](repeating: 0, count: 256)
            _ = imuse_bank_sound_name(bankHandle, soundID, &nameBuffer, nameBuffer.count)
            let name = String(cString: nameBuffer)
            
            // Load tracks
            let trackCount = imuse_bank_track_count(bankHandle, soundID, selectedProfile)
            var tracks: [TrackSummary] = []
            tracks.reserveCapacity(trackCount)
            for tIndex in 0..<trackCount {
                var trackNameBuffer = [CChar](repeating: 0, count: 256)
                var eventCount: Int = 0
                let ok = imuse_bank_track_summary(bankHandle, soundID, selectedProfile, tIndex, &trackNameBuffer, trackNameBuffer.count, &eventCount)
                if ok == 1 {
                    let trackName = String(cString: trackNameBuffer)
                    tracks.append(TrackSummary(id: "\(soundID)-\(tIndex)", index: Int(tIndex), name: trackName, eventCount: eventCount))
                }
            }
            
            newSounds.append(SoundSummary(id: soundID, name: name.isEmpty ? "Sound \(soundID)" : name, tracks: tracks))
        }

        sounds = newSounds
        if selectedSoundID == nil || !sounds.contains(where: { $0.id == selectedSoundID }) {
            selectedSoundID = sounds.first?.id
        }
        reloadSelectedEvents()
    }

    private func updateEngineProfile() {
        if let engineHandle {
            imuse_engine_set_profile(engineHandle, selectedProfile)
            let nativeMt32 = (outputBackend == .mt32emu || (outputBackend == .coreMidi && selectedProfile == ImuseAuthoringProfileMt32)) ? 1 : 0
            imuse_engine_set_native_mt32_output(engineHandle, Int32(nativeMt32))
        }
    }

    public func reloadSelectedEvents() {
        guard let bankHandle, let soundID = selectedSoundID else {
            events = []
            return
        }

        let count = imuse_bank_event_count(bankHandle, soundID, selectedProfile)
        var newEvents: [EventSummary] = []
        newEvents.reserveCapacity(count)

        for index in 0..<count {
            var tick: UInt32 = 0
            var track: UInt16 = 0
            var textBuffer = [CChar](repeating: 0, count: 256)
            let ok = imuse_bank_event_summary(bankHandle, soundID, selectedProfile, index, &tick, &track, &textBuffer, textBuffer.count)
            if ok == 1 {
                newEvents.append(EventSummary(id: Int(index), tick: tick, track: track, text: String(cString: textBuffer)))
            }
        }

        events = newEvents
    }

    public func startSelectedSound() {
        guard let engineHandle, let soundID = selectedSoundID else {
            return
        }

        if imuse_engine_start_sound(engineHandle, soundID) == 0 {
            errorMessage = "Impossible de demarrer le son \(soundID)."
        } else {
            errorMessage = ""
            if previewEnabled {
                startTransport()
            }
        }
        refreshActiveSounds()
    }

    public func stopSelectedSound() {
        guard let engineHandle, let soundID = selectedSoundID else {
            return
        }
        imuse_engine_stop_sound(engineHandle, soundID)
        refreshActiveSounds()
        if activeSounds.isEmpty {
            stopTransport()
        }
    }

    public func stopAllSounds() {
        guard let engineHandle else {
            return
        }
        imuse_engine_stop_all(engineHandle)
        refreshActiveSounds()
        stopTransport()
    }

    public func advanceByText() {
        guard let ticks = UInt32(advanceTicksText) else {
            errorMessage = "La valeur de ticks est invalide."
            return
        }
        advance(ticks: ticks)
    }

    public func advance(ticks: UInt32) {
        guard let engineHandle else {
            return
        }
        imuse_engine_advance(engineHandle, ticks)
        refreshActiveSounds()
        if activeSounds.isEmpty {
            stopTransport()
        }
    }

    public func applyHook() {
        guard let engineHandle, let soundID = selectedSoundID else {
            return
        }
        guard let hookClass = UInt8(hookClassText), let hookValue = UInt8(hookValueText), let hookChannel = UInt8(hookChannelText) else {
            errorMessage = "Les parametres de hook sont invalides."
            return
        }

        if imuse_engine_set_hook(engineHandle, soundID, hookClass, hookValue, hookChannel) == 0 {
            errorMessage = "Le hook n'a pas pu etre applique."
        } else {
            errorMessage = ""
        }
        refreshActiveSounds()
    }

    public func refreshActiveSounds() {
        guard let engineHandle else {
            activeSounds = []
            return
        }

        let count = imuse_engine_active_sound_count(engineHandle)
        var newActive: [ActiveSoundStatus] = []
        newActive.reserveCapacity(count)

        for index in 0..<count {
            let soundID = imuse_engine_active_sound_id_at(engineHandle, index)
            var track: UInt16 = 0
            var beat: UInt16 = 0
            var tick: UInt16 = 0
            if imuse_engine_get_location(engineHandle, soundID, &track, &beat, &tick) == 1 {
                newActive.append(ActiveSoundStatus(id: soundID, track: track, beat: beat, tick: tick))
            }
        }

        activeSounds = newActive
    }

    public func toggleTransport() {
        if transportRunning {
            stopTransport()
            return
        }

        if activeSounds.isEmpty {
            startSelectedSound()
        } else {
            startTransport()
        }
    }

    private func startTransport() {
        guard engineHandle != nil else {
            return
        }

        if transportTimer == nil {
            tickAccumulator = 0.0
            lastTransportTime = CACurrentMediaTime()
            let timer = Timer(timeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
                Task { @MainActor [weak self] in
                    guard let self else { return }
                    let now = CACurrentMediaTime()
                    var elapsed = now - self.lastTransportTime
                    if elapsed > 0.1 {
                        elapsed = 0.1
                    }
                    self.lastTransportTime = now
                    self.transportStep(elapsed: elapsed)
                }
            }
            transportTimer = timer
            RunLoop.main.add(timer, forMode: .common)
        }
        transportRunning = true
    }

    private func stopTransport() {
        transportTimer?.invalidate()
        transportTimer = nil
        transportRunning = false
        tickAccumulator = 0.0
    }

    private func startAudioOutput() {
        guard audioEngine == nil, let previewEngineHandle = engineHandle else {
            return
        }

        let backend = outputBackend
        let engine = AVAudioEngine()
        let format = AVAudioFormat(commonFormat: .pcmFormatFloat32,
                                   sampleRate: 44_100,
                                   channels: 2,
                                   interleaved: false)

        let sourceNode = AVAudioSourceNode { _, _, frameCount, audioBufferList -> OSStatus in
            let buffers = UnsafeMutableAudioBufferListPointer(audioBufferList)
            if buffers.count >= 2,
               let leftData = buffers[0].mData,
               let rightData = buffers[1].mData {
                let left = leftData.assumingMemoryBound(to: Float.self)
                let right = rightData.assumingMemoryBound(to: Float.self)
                switch backend {
                case .mt32emu:
                    imuse_engine_render_mt32(previewEngineHandle, frameCount, left, right)
                case .adlib:
                    imuse_engine_render_adlib(previewEngineHandle, frameCount, left, right)
                default:
                    imuse_engine_render_fluidsynth(previewEngineHandle, frameCount, left, right)
                }
            } else if buffers.count == 1,
                      let monoData = buffers[0].mData {
                let mono = monoData.assumingMemoryBound(to: Float.self)
                switch backend {
                case .mt32emu:
                    imuse_engine_render_mt32(previewEngineHandle, frameCount, mono, mono)
                case .adlib:
                    imuse_engine_render_adlib(previewEngineHandle, frameCount, mono, mono)
                default:
                    imuse_engine_render_fluidsynth(previewEngineHandle, frameCount, mono, mono)
                }
            }
            return 0
        }

        engine.attach(sourceNode)
        engine.connect(sourceNode, to: engine.mainMixerNode, format: format)

        do {
            try engine.start()
            audioEngine = engine
            audioSourceNode = sourceNode
        } catch {
            audioEngine = nil
            audioSourceNode = nil
            previewEnabled = false
            errorMessage = "Impossible de demarrer l'audio Swift: \(error.localizedDescription)"
        }
    }

    private func stopAudioOutput() {
        audioEngine?.stop()
        if let sourceNode = audioSourceNode {
            audioEngine?.detach(sourceNode)
        }
        audioSourceNode = nil
        audioEngine = nil
    }

    private func transportStep(elapsed: Double) {
        guard let engineHandle else {
            stopTransport()
            return
        }

        let ticksPerSecond = imuse_engine_ticks_per_second(engineHandle)
        let exactTicks = (ticksPerSecond * elapsed) + tickAccumulator
        let wholeTicks = UInt32(exactTicks.rounded(.down))
        tickAccumulator = exactTicks - Double(wholeTicks)

        if wholeTicks > 0 {
            imuse_engine_advance(engineHandle, wholeTicks)
            refreshActiveSounds()
        }

        if activeSounds.isEmpty {
            stopTransport()
        }
    }

    private func configureDefaultAssetPaths() {
        let fileManager = FileManager.default

        // 1. Remember last loaded IMS path instead of defaulting to openquest-lite
        if let lastIms = UserDefaults.standard.string(forKey: "LastLoadedImsPath"),
           fileManager.fileExists(atPath: lastIms) {
            bankPath = lastIms
        } else {
            bankPath = ""
        }

        // 2. SoundFont path default
        let bundledSoundFont = Bundle.main.path(forResource: "arachno", ofType: "sf2")
        let workspaceSoundFont = "/Users/komasami/Dev/scumm-tools/imuse-v6/baka/arachno.sf2"

        if let bundledSoundFont {
            soundFontPath = bundledSoundFont
        } else if fileManager.fileExists(atPath: workspaceSoundFont) {
            soundFontPath = workspaceSoundFont
        }

        // 3. Search for MT-32 ROMs in:
        // A. Same directory as the .app bundle (parent of main bundle URL)
        let appDirUrl = Bundle.main.bundleURL.deletingLastPathComponent()
        let controlRomExistsInAppDir = fileManager.fileExists(atPath: appDirUrl.appendingPathComponent("MT32_CONTROL.ROM").path) ||
                                       fileManager.fileExists(atPath: appDirUrl.appendingPathComponent("mt32_control.rom").path) ||
                                       fileManager.fileExists(atPath: appDirUrl.appendingPathComponent("MT32_CONTROL.rom").path)
        
        if controlRomExistsInAppDir {
            mt32RomDirPath = appDirUrl.path
        } else {
            // B. Check inside bundle resources
            if let resourcePath = Bundle.main.resourcePath,
               (fileManager.fileExists(atPath: (resourcePath as NSString).appendingPathComponent("MT32_CONTROL.ROM")) ||
                fileManager.fileExists(atPath: (resourcePath as NSString).appendingPathComponent("mt32_control.rom"))) {
                mt32RomDirPath = resourcePath
            } else {
                // C. Fallback to workspace path
                let workspaceRomDir = "/Users/komasami/Dev/scumm-tools/imuse-v6/baka/roms"
                if fileManager.fileExists(atPath: workspaceRomDir) {
                    mt32RomDirPath = workspaceRomDir
                }
            }
        }
    }

    private func cleanupHandles() {
        stopTransport()
        stopAudioOutput()
        disableCoreMIDIOutput()
        previewEnabled = false
        if let engineHandle {
            imuse_engine_destroy(engineHandle)
            self.engineHandle = nil
        }
        if let bankHandle {
            imuse_bank_destroy(bankHandle)
            self.bankHandle = nil
        }
        sounds = []
        events = []
        activeSounds = []
        selectedSoundID = nil
    }

    // CoreMIDI & Backend Management Helpers
    
    private func setupCoreMIDI() {
        guard midiClient == 0 else { return }
        var client = MIDIClientRef()
        let status = MIDIClientCreateWithBlock("iMUSE MIDI Client" as CFString, &client, nil)
        if status == noErr {
            midiClient = client
            var port = MIDIPortRef()
            let portStatus = MIDIOutputPortCreate(midiClient, "iMUSE Output Port" as CFString, &port)
            if portStatus == noErr {
                midiOutputPort = port
            }
        }
    }
    
    public func refreshMidiDestinations() {
        let count = MIDIGetNumberOfDestinations()
        var dests: [MidiDestinationInfo] = []
        for i in 0..<count {
            let endpoint = MIDIGetDestination(i)
            let name = getMIDIObjectStringProperty(endpoint, kMIDIPropertyDisplayName) ?? getMIDIObjectStringProperty(endpoint, kMIDIPropertyName) ?? "Destination \(i)"
            let uniqueID = getMIDIObjectIntegerProperty(endpoint, kMIDIPropertyUniqueID) ?? Int32(i)
            dests.append(MidiDestinationInfo(id: uniqueID, name: name))
        }
        self.midiDestinations = dests
        if selectedMidiDestinationID == nil || !midiDestinations.contains(where: { $0.id == selectedMidiDestinationID }) {
            selectedMidiDestinationID = midiDestinations.first?.id
        }
    }
    
    private func getDestinationRef(for uniqueID: MIDIUniqueID) -> MIDIEndpointRef? {
        let count = MIDIGetNumberOfDestinations()
        for i in 0..<count {
            let endpoint = MIDIGetDestination(i)
            if getMIDIObjectIntegerProperty(endpoint, kMIDIPropertyUniqueID) == uniqueID {
                return endpoint
            }
        }
        return nil
    }
    
    public func sendMidiShortMessage(status: UInt8, data1: UInt8, data2: UInt8) {
        guard outputBackend == .coreMidi,
              let destID = selectedMidiDestinationID,
              let destRef = getDestinationRef(for: destID),
              midiOutputPort != 0 else {
            return
        }
        
        var packetList = MIDIPacketList()
        let packetPtr = MIDIPacketListInit(&packetList)
        
        let type = status & 0xF0
        let bytes: [UInt8]
        if type == 0xC0 || type == 0xD0 { // Program Change or Channel Pressure
            bytes = [status, data1]
        } else {
            bytes = [status, data1, data2]
        }
        
        _ = MIDIPacketListAdd(&packetList, 1024, packetPtr, 0, bytes.count, bytes)
        MIDISend(midiOutputPort, destRef, &packetList)
    }
    
    public func sendMidiSysExMessage(data: [UInt8]) {
        guard outputBackend == .coreMidi,
              let destID = selectedMidiDestinationID,
              let destRef = getDestinationRef(for: destID),
              midiOutputPort != 0 else {
            return
        }
        
        let bufferSize = MemoryLayout<MIDIPacketList>.size + data.count + 64
        let buffer = UnsafeMutableRawPointer.allocate(byteCount: bufferSize, alignment: MemoryLayout<MIDIPacketList>.alignment)
        defer { buffer.deallocate() }
        
        let packetList = buffer.bindMemory(to: MIDIPacketList.self, capacity: 1)
        let packetPtr = MIDIPacketListInit(packetList)
        _ = MIDIPacketListAdd(packetList, bufferSize, packetPtr, 0, data.count, data)
        MIDISend(midiOutputPort, destRef, packetList)
    }
    
    private func getMIDIObjectStringProperty(_ obj: MIDIObjectRef, _ propertyID: CFString) -> String? {
        var param: Unmanaged<CFString>?
        let status = MIDIObjectGetStringProperty(obj, propertyID, &param)
        if status == noErr, let value = param?.takeRetainedValue() {
            return value as String
        }
        return nil
    }
    
    private func getMIDIObjectIntegerProperty(_ obj: MIDIObjectRef, _ propertyID: CFString) -> Int32? {
        var value: Int32 = 0
        let status = MIDIObjectGetIntegerProperty(obj, propertyID, &value)
        if status == noErr {
            return value
        }
        return nil
    }
    
    private func enableCoreMIDIOutput() {
        setupCoreMIDI()
        refreshMidiDestinations()
    }
    
    private func disableCoreMIDIOutput() {
    }
    
    public func handleBackendChange() {
        guard let engineHandle else { return }
        
        // 1. Disable all first
        imuse_engine_disable_fluidsynth(engineHandle)
        imuse_engine_disable_mt32(engineHandle)
        imuse_engine_disable_adlib(engineHandle)
        disableCoreMIDIOutput()
        
        let nativeMt32 = (outputBackend == .mt32emu || (outputBackend == .coreMidi && selectedProfile == ImuseAuthoringProfileMt32)) ? 1 : 0
        imuse_engine_set_native_mt32_output(engineHandle, Int32(nativeMt32))
        
        // 2. Enable selected backend
        switch outputBackend {
        case .fluidSynth:
            if !soundFontPath.isEmpty {
                var errorBuffer = [CChar](repeating: 0, count: 512)
                let ok = soundFontPath.withCString { pathPtr in
                    imuse_engine_enable_fluidsynth(engineHandle, pathPtr, &errorBuffer, errorBuffer.count)
                }
                if ok == 1 {
                    errorMessage = ""
                    startAudioOutput()
                } else {
                    errorMessage = String(cString: errorBuffer)
                    stopAudioOutput()
                }
            } else {
                errorMessage = "Veuillez choisir une SoundFont."
                stopAudioOutput()
            }

        case .adlib:
            var errorBuffer = [CChar](repeating: 0, count: 512)
            let ok = imuse_engine_enable_adlib(engineHandle, &errorBuffer, errorBuffer.count)
            if ok == 1 {
                errorMessage = ""
                startAudioOutput()
            } else {
                errorMessage = String(cString: errorBuffer)
                stopAudioOutput()
            }
            
        case .mt32emu:
            if !mt32RomDirPath.isEmpty {
                var errorBuffer = [CChar](repeating: 0, count: 512)
                let ok = mt32RomDirPath.withCString { pathPtr in
                    imuse_engine_enable_mt32(engineHandle, pathPtr, &errorBuffer, errorBuffer.count)
                }
                if ok == 1 {
                    errorMessage = ""
                    startAudioOutput()
                } else {
                    errorMessage = String(cString: errorBuffer)
                    stopAudioOutput()
                }
            } else {
                errorMessage = "Veuillez choisir le dossier des ROMs MT-32."
                stopAudioOutput()
            }
            
        case .coreMidi:
            errorMessage = ""
            stopAudioOutput()
            enableCoreMIDIOutput()
        }
    }
}

public struct ImusePlayerView: View {
    @StateObject private var model = AuthoringViewModel()
    @State private var expandedSoundIDs: Set<UInt16> = []

    public init() {}

    public var body: some View {
        NavigationSplitView {
            VStack(alignment: .leading, spacing: 16) {
                // Header
                HStack(spacing: 8) {
                    Image(systemName: "music.quarternote.3")
                        .font(.title2)
                        .foregroundStyle(.blue)
                    Text("Lecteur iMUSE")
                        .font(.headline)
                        .fontWeight(.bold)
                    Spacer()
                    if model.previewEnabled {
                        Text("ONLINE")
                            .font(.caption2)
                            .fontWeight(.bold)
                            .padding(.horizontal, 6)
                            .padding(.vertical, 2)
                            .background(Color.green.opacity(0.2))
                            .foregroundStyle(.green)
                            .cornerRadius(4)
                    }
                }
                .padding(.bottom, 8)

                // Files Config
                VStack(alignment: .leading, spacing: 8) {
                    Text("Banque de sons (.ims)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    HStack {
                        TextField("Chemin...", text: $model.bankPath)
                            .textFieldStyle(.roundedBorder)
                        Button(action: model.browseBank) {
                            Image(systemName: "doc.badge.plus")
                        }
                        .help("Parcourir")
                        Button("Charger") {
                            model.loadBank()
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }
                .padding(10)
                .background(Color(NSColor.controlBackgroundColor).opacity(0.5))
                .cornerRadius(8)

                // Output Backend Config
                VStack(alignment: .leading, spacing: 8) {
                    Text("Sortie Audio & MIDI")
                        .font(.caption)
                        .fontWeight(.semibold)
                        .foregroundStyle(.secondary)
                    
                    Picker("Sortie", selection: $model.outputBackend) {
                        ForEach(OutputBackend.allCases) { backend in
                            Text(backend.rawValue).tag(backend)
                        }
                    }
                    .pickerStyle(.segmented)
                    .padding(.bottom, 4)
                    
                    if model.outputBackend == .fluidSynth {
                        Text("SoundFont (.sf2)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        HStack {
                            TextField("Chemin...", text: $model.soundFontPath)
                                .textFieldStyle(.roundedBorder)
                            Button(action: model.browseSoundFont) {
                                Image(systemName: "music.note.list")
                            }
                            .help("Parcourir SF2")
                        }
                    } else if model.outputBackend == .adlib {
                        HStack(spacing: 6) {
                            Image(systemName: "waveform.circle")
                                .foregroundStyle(.orange)
                            Text("Émulation OPL2 (AdLib) — aucune configuration requise")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        .padding(.vertical, 2)
                    } else if model.outputBackend == .mt32emu {
                        Text("Dossier des ROMs MT-32")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        HStack {
                            TextField("Dossier...", text: $model.mt32RomDirPath)
                                .textFieldStyle(.roundedBorder)
                            Button(action: model.browseMt32RomDir) {
                                Image(systemName: "folder.badge.plus")
                            }
                            .help("Parcourir dossier ROMs")
                        }
                    } else if model.outputBackend == .coreMidi {
                        Text("Périphérique de sortie CoreMIDI")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        HStack {
                            Picker("Destination", selection: $model.selectedMidiDestinationID) {
                                if model.midiDestinations.isEmpty {
                                    Text("Aucune destination").tag(MIDIUniqueID?.none)
                                } else {
                                    ForEach(model.midiDestinations) { dest in
                                        Text(dest.name).tag(MIDIUniqueID?.some(dest.id))
                                    }
                                }
                            }
                            .pickerStyle(.menu)
                            
                            Button(action: model.refreshMidiDestinations) {
                                Image(systemName: "arrow.clockwise")
                            }
                            .help("Rafraîchir les destinations")
                        }
                    }
                    
                    HStack {
                        Spacer()
                        if model.previewEnabled {
                            Button(role: .destructive, action: model.disablePreview) {
                                Label("Désactiver", systemImage: "power")
                            }
                            .buttonStyle(.borderedProminent)
                            .tint(.red)
                        } else {
                            Button(action: model.enablePreview) {
                                Label("Activer", systemImage: "power")
                            }
                            .buttonStyle(.borderedProminent)
                            .tint(.green)
                        }
                    }
                    .padding(.top, 4)
                }
                .padding(10)
                .background(Color(NSColor.controlBackgroundColor).opacity(0.5))
                .cornerRadius(8)

                Divider()

                Text("Sons de la banque")
                    .font(.caption)
                    .fontWeight(.semibold)
                    .foregroundStyle(.secondary)

                List(model.sounds) { sound in
                    DisclosureGroup(
                        isExpanded: Binding(
                            get: { self.expandedSoundIDs.contains(sound.id) },
                            set: { isExpanded in
                                if isExpanded {
                                    self.expandedSoundIDs.insert(sound.id)
                                } else {
                                    self.expandedSoundIDs.remove(sound.id)
                                }
                            }
                        ),
                        content: {
                            ForEach(sound.tracks) { track in
                                HStack {
                                    Image(systemName: "music.note")
                                        .foregroundStyle(.secondary)
                                    Text(track.name)
                                        .font(.caption)
                                    Spacer()
                                    Text("\(track.eventCount) evts")
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                                .padding(.leading, 8)
                            }
                        },
                        label: {
                            Button(action: {
                                model.selectedSoundID = sound.id
                            }) {
                                HStack {
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text(sound.name)
                                            .font(.body)
                                            .fontWeight(model.selectedSoundID == sound.id ? .bold : .regular)
                                        Text("ID \(sound.id) • \(sound.tracks.count) pistes")
                                            .font(.caption2)
                                            .foregroundStyle(.secondary)
                                    }
                                    Spacer()
                                    if model.activeSounds.contains(where: { $0.id == sound.id }) {
                                        Circle()
                                            .fill(Color.green)
                                            .frame(width: 8, height: 8)
                                    }
                                }
                            }
                            .buttonStyle(.plain)
                        }
                    )
                }
                .listStyle(.sidebar)
                .cornerRadius(8)
                .onChange(of: model.selectedSoundID) { _ in
                    model.reloadSelectedEvents()
                }

                if !model.errorMessage.isEmpty {
                    Text(model.errorMessage)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(Color.red.opacity(0.1))
                        .cornerRadius(6)
                }
            }
            .padding()
            .navigationSplitViewColumnWidth(min: 320, ideal: 360, max: 400)
        } detail: {
            VStack(alignment: .leading, spacing: 16) {
                // Sound details header
                if let selectedID = model.selectedSoundID,
                   let sound = model.sounds.first(where: { $0.id == selectedID }) {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(sound.name)
                                .font(.title)
                                .fontWeight(.bold)
                            Text("Sound ID: \(sound.id)")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        }
                        Spacer()
                    }
                    .padding(.bottom, 8)
                } else {
                    Text("Aucun son sélectionné")
                        .font(.title)
                        .fontWeight(.bold)
                        .foregroundStyle(.secondary)
                        .padding(.bottom, 8)
                }

                // Transport Panel
                GroupBox {
                    HStack(spacing: 20) {
                        Button(action: model.startSelectedSound) {
                            Label("Lancer", systemImage: "play.circle.fill")
                                .font(.title3)
                        }
                        .buttonStyle(.bordered)
                        .tint(.green)

                        Button(action: model.stopSelectedSound) {
                            Label("Arrêter", systemImage: "stop.circle.fill")
                                .font(.title3)
                        }
                        .buttonStyle(.bordered)
                        .tint(.red)

                        Button(action: model.stopAllSounds) {
                            Label("Tout couper", systemImage: "backward.end.circle.fill")
                                .font(.title3)
                        }
                        .buttonStyle(.bordered)

                        Spacer()

                        Button(action: model.toggleTransport) {
                            Label(model.transportRunning ? "Pause" : "Play (Temps Réel)",
                                  systemImage: model.transportRunning ? "pause.fill" : "play.fill")
                                .fontWeight(.bold)
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(.blue)
                        .disabled(!model.previewEnabled)
                    }
                    .padding(8)
                } label: {
                    Label("Contrôle du Transport", systemImage: "playpause.fill")
                        .foregroundColor(.blue)
                }

                // Configuration et Profil MIDI
                HStack(alignment: .top, spacing: 16) {
                    VStack(alignment: .leading, spacing: 12) {
                        GroupBox("Profil de rendu MIDI") {
                            Picker("Profil", selection: $model.selectedProfile) {
                                Text("General MIDI").tag(ImuseAuthoringProfileGeneralMidi)
                                Text("Roland MT-32").tag(ImuseAuthoringProfileMt32)
                            }
                            .pickerStyle(.segmented)
                            .padding(.vertical, 4)
                        }

                        GroupBox("Pas à Pas (Ticks)") {
                            HStack {
                                TextField("Ticks", text: $model.advanceTicksText)
                                    .textFieldStyle(.roundedBorder)
                                    .frame(width: 80)
                                Button("Avancer") { model.advanceByText() }
                                Button("+480") { model.advance(ticks: 480) }
                                Button("+3840") { model.advance(ticks: 3840) }
                            }
                            .padding(.vertical, 4)
                        }
                    }
                    .frame(maxWidth: .infinity)

                    GroupBox("Déclencheurs iMUSE (Hooks)") {
                        VStack(spacing: 8) {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("Classe")
                                        .font(.caption2)
                                    TextField("", text: $model.hookClassText)
                                        .textFieldStyle(.roundedBorder)
                                }
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("Valeur")
                                        .font(.caption2)
                                    TextField("", text: $model.hookValueText)
                                        .textFieldStyle(.roundedBorder)
                                }
                                VStack(alignment: .leading, spacing: 2) {
                                    Text("Canal")
                                        .font(.caption2)
                                    TextField("", text: $model.hookChannelText)
                                        .textFieldStyle(.roundedBorder)
                                }
                            }
                            Button("Appliquer le Hook", action: model.applyHook)
                                .buttonStyle(.borderedProminent)
                                .frame(maxWidth: .infinity)
                        }
                        .padding(.vertical, 4)
                    }
                    .frame(maxWidth: .infinity)
                }

                // Active Sounds list
                GroupBox("Lecture en cours") {
                    if model.activeSounds.isEmpty {
                        Text("Aucun canal iMUSE actif.")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                            .frame(maxWidth: .infinity, minHeight: 60, alignment: .center)
                    } else {
                        VStack(spacing: 6) {
                            ForEach(model.activeSounds) { sound in
                                HStack {
                                    Image(systemName: "waveform")
                                        .foregroundStyle(.green)
                                    Text("Son \(sound.id)")
                                        .fontWeight(.semibold)
                                    Spacer()
                                    Text("Piste \(sound.track)")
                                        .font(.caption)
                                        .padding(.horizontal, 6)
                                        .padding(.vertical, 2)
                                        .background(Color.blue.opacity(0.15))
                                        .cornerRadius(4)
                                    Text("Mesure \(sound.beat)")
                                        .font(.caption)
                                    Text("Tick \(sound.tick)")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                                .padding(.vertical, 4)
                                .padding(.horizontal, 8)
                                .background(Color(NSColor.controlBackgroundColor).opacity(0.3))
                                .cornerRadius(6)
                            }
                        }
                        .frame(maxHeight: 150)
                    }
                }

                // Events List
                GroupBox("Événements iMUSE & SysEx du son sélectionné") {
                    if model.events.isEmpty {
                        Text("Aucun événement iMUSE détecté pour ce profil.")
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                            .frame(maxWidth: .infinity, minHeight: 80, alignment: .center)
                    } else {
                        List(model.events) { event in
                            HStack(alignment: .top) {
                                Image(systemName: "music.note.list")
                                    .foregroundStyle(event.text.contains("SysEx") ? .purple : .blue)
                                    .padding(.top, 2)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(event.text)
                                        .font(.system(.body, design: .monospaced))
                                    Text("Piste \(event.track) • Tick \(event.tick)")
                                        .font(.caption2)
                                        .foregroundStyle(.secondary)
                                }
                            }
                            .padding(.vertical, 2)
                        }
                        .frame(minHeight: 180)
                        .cornerRadius(6)
                    }
                }

                Spacer()
            }
            .padding()
            .frame(minWidth: 600)
        }
        .task {
            model.bootstrapIfPossible()
        }
    }
}
