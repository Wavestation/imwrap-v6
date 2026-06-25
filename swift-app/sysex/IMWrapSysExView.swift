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

import SwiftUI

enum SysExType: String, CaseIterable, Identifiable {
    case allocatePart = "Allocate Part (0x00)"
    case shutdownPart = "Shutdown Part (0x01)"
    case startSong = "Start Song (0x02)"
    case adlibPartInstrument = "AdLib Part Instrument (0x10)"
    case adlibGlobalInstrument = "AdLib Global Instrument (0x11)"
    case parameterAdjust = "Parameter Adjust (0x21)"
    case hookJump = "Hook Jump (0x30)"
    case hookGlobalTranspose = "Hook Global Transpose (0x31)"
    case hookPartOnOff = "Hook Part On/Off (0x32)"
    case hookSetVolume = "Hook Set Volume (0x33)"
    case hookSetProgram = "Hook Set Program (0x34)"
    case hookSetTranspose = "Hook Set Transpose (0x35)"
    case marker = "Marker (0x40)"
    case setLoop = "Set Loop (0x50)"
    case clearLoop = "Clear Loop (0x51)"
    case setInstrument = "Set Instrument (0x60)"
    
    var id: String { self.rawValue }
    
    var code: UInt8 {
        switch self {
        case .allocatePart: return 0x00
        case .shutdownPart: return 0x01
        case .startSong: return 0x02
        case .adlibPartInstrument: return 0x10
        case .adlibGlobalInstrument: return 0x11
        case .parameterAdjust: return 0x21
        case .hookJump: return 0x30
        case .hookGlobalTranspose: return 0x31
        case .hookPartOnOff: return 0x32
        case .hookSetVolume: return 0x33
        case .hookSetProgram: return 0x34
        case .hookSetTranspose: return 0x35
        case .marker: return 0x40
        case .setLoop: return 0x50
        case .clearLoop: return 0x51
        case .setInstrument: return 0x60
        }
    }
}

struct TypeableStepper: View {
    let title: String
    @Binding var value: Int
    let range: ClosedRange<Int>
    var showSpacer: Bool = true
    
    var body: some View {
        HStack(spacing: 8) {
            if !title.isEmpty {
                Text(title)
            }
            if showSpacer {
                Spacer()
            }
            TextField("", value: $value, format: .number)
                .textFieldStyle(.roundedBorder)
                .frame(width: 65)
                .multilineTextAlignment(.trailing)
                .onChange(of: value) { newValue in
                    if newValue < range.lowerBound {
                        value = range.lowerBound
                    } else if newValue > range.upperBound {
                        value = range.upperBound
                    }
                }
            Stepper("", value: $value, in: range)
                .labelsHidden()
        }
    }
}

struct IMWrapSysExView: View {
    @State private var activeTab: String = "generator"
    
    // Parser State
    @State private var hexInput: String = ""
    @State private var parseError: String = ""
    @State private var decodedMessage: DecodedSysEx? = nil
    
    // Generator General State
    @State private var selectedType: SysExType = .allocatePart
    @State private var generatedHex: String = ""
    
    // Fields
    @State private var part: Int = 0
    @State private var channel: Int = 0
    @State private var unknown: Int = 0
    
    // Allocate Part Specifics
    @State private var partOn: Bool = true
    @State private var reverb: Bool = false
    @State private var priority: Int = 90
    @State private var volume: Int = 127
    @State private var pan: Int = 64
    @State private var percussion: Bool = false
    @State private var transpose: Int = 0
    @State private var detune: Int = 0
    @State private var pitchbend: Int = 2
    @State private var program: Int = 0
    
    // Parameter Adjust
    @State private var param: Int = 0
    @State private var paramValue: Int = 0
    
    // Hook Jump
    @State private var hookCmd: Int = 0
    @State private var targetTrack: Int = 0
    @State private var targetBeat: Int = 0
    @State private var targetTick: Int = 0
    
    // Hook Transpose / General
    @State private var hookRelative: Bool = false
    @State private var hookValue: Int = 0
    
    // Marker
    @State private var markerText: String = "Intro"
    
    // Set Loop
    @State private var loopCount: Int = 0
    @State private var loopToBeat: Int = 0
    @State private var loopToTick: Int = 0
    @State private var loopFromBeat: Int = 0
    @State private var loopFromTick: Int = 0
    
    // Set Instrument
    @State private var instrumentID: Int = 0
    
    // AdLib Hex inputs
    @State private var adlibHex: String = ""
    
    var body: some View {
        VStack(spacing: 0) {
            // Tab Selector
            Picker("", selection: $activeTab) {
                Text("Générateur de SysEx").tag("generator")
                Text("Décodeur de SysEx").tag("decoder")
            }
            .pickerStyle(.segmented)
            .padding()
            
            if activeTab == "generator" {
                generatorView
            } else {
                decoderView
            }
        }
        .frame(minWidth: 800, minHeight: 600)
        .onAppear {
            updateGeneratedHex()
        }
    }
    
    private var generatorView: some View {
        HSplitView {
            // Left Column: Parameters Form
            VStack {
                Form {
                    Section(header: Text("Type de Message iMWrap")) {
                        Picker("Type", selection: $selectedType) {
                            ForEach(SysExType.allCases) { type in
                                Text(type.rawValue).tag(type)
                            }
                        }
                        .onChange(of: selectedType) { _ in updateGeneratedHex() }
                    }
                    
                    Section(header: Text("Paramètres")) {
                        // Contextual fields
                        if showPartField {
                            TypeableStepper(title: "Part (0-15)", value: $part, range: 0...15)
                                .onChange(of: part) { _ in updateGeneratedHex() }
                        }
                        if showChannelField {
                            TypeableStepper(title: "Canal MIDI (0-15)", value: $channel, range: 0...15)
                                .onChange(of: channel) { _ in updateGeneratedHex() }
                        }
                        if showUnknownField {
                            TypeableStepper(title: "Unknown (0-127)", value: $unknown, range: 0...127)
                                .onChange(of: unknown) { _ in updateGeneratedHex() }
                        }
                        
                        // Type specific controls
                        typeSpecificFields
                    }
                }
                .formStyle(.grouped)
            }
            .frame(maxWidth: .infinity)
            
            // Right Column: Output Screen
            VStack(spacing: 20) {
                GroupBox("SysEx iMWrap Généré") {
                    VStack(alignment: .leading, spacing: 12) {
                        Text(generatedHex)
                            .font(.system(.title3, design: .monospaced))
                            .fontWeight(.bold)
                            .foregroundColor(.purple)
                            .multilineTextAlignment(.leading)
                            .padding()
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .background(Color(NSColor.controlBackgroundColor))
                            .cornerRadius(8)
                        
                        HStack {
                            Button(action: copyToClipboard) {
                                Label("Copier dans le presse-papiers", systemImage: "doc.on.doc")
                            }
                            .buttonStyle(.borderedProminent)
                            .tint(.purple)
                            
                            Spacer()
                            
                            Text("\(generatedHex.split(separator: " ").count) octets")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding(.vertical, 8)
                }
                
                GroupBox("Structure du Message") {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text("F0").bold().foregroundColor(.gray)
                            Text("Début de SysEx MIDI")
                        }
                        HStack {
                            Text("7D").bold().foregroundColor(.blue)
                            Text("ID Manufacturier (héritage iMUSE/SCUMM)")
                        }
                        HStack {
                            Text(String(format: "%02X", selectedType.code)).bold().foregroundColor(.red)
                            Text("Code de Commande (\(selectedType.rawValue))")
                        }
                        HStack {
                            Text("...").bold()
                            Text("Données / Nibbles utiles")
                        }
                        HStack {
                            Text("F7").bold().foregroundColor(.gray)
                            Text("Fin de SysEx MIDI")
                        }
                    }
                    .font(.subheadline)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                
                Spacer()
            }
            .padding()
            .frame(width: 380)
        }
    }
    
    private var decoderView: some View {
        VStack(spacing: 16) {
            GroupBox("Saisie du Message Hexadécimal") {
                VStack(spacing: 12) {
                    TextEditor(text: $hexInput)
                        .font(.system(.body, design: .monospaced))
                        .frame(height: 80)
                        .cornerRadius(6)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6)
                                .stroke(Color.secondary.opacity(0.2), lineWidth: 1)
                        )
                        .onChange(of: hexInput) { _ in parseHex() }
                    
                    HStack {
                        Button("Effacer") {
                            hexInput = ""
                            decodedMessage = nil
                            parseError = ""
                        }
                        Spacer()
                        if !parseError.isEmpty {
                            Text(parseError)
                                .foregroundColor(.red)
                                .font(.caption)
                        }
                    }
                }
                .padding(.vertical, 4)
            }
            .padding([.horizontal, .top])
            
            if let msg = decodedMessage {
                GroupBox("Détails du message SysEx décodé") {
                    VStack(alignment: .leading, spacing: 16) {
                        HStack {
                            Image(systemName: "music.note.list")
                                .font(.title)
                                .foregroundColor(.purple)
                            
                            VStack(alignment: .leading) {
                                Text(msg.typeName)
                                    .font(.title2)
                                    .fontWeight(.bold)
                                Text(String(format: "Code de commande iMWrap: 0x%02X", msg.code))
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                        }
                        
                        Divider()
                        
                        ScrollView {
                            LazyVGrid(columns: [GridItem(.fixed(180), alignment: .trailing), GridItem(.flexible(), alignment: .leading)], spacing: 10) {
                                ForEach(msg.details, id: \.key) { detail in
                                    Text(detail.key)
                                        .fontWeight(.semibold)
                                        .foregroundColor(.secondary)
                                    
                                    Text(detail.value)
                                        .font(.system(.body, design: .monospaced))
                                        .padding(.horizontal, 8)
                                        .padding(.vertical, 4)
                                        .background(Color.purple.opacity(0.08))
                                        .cornerRadius(4)
                                }
                            }
                            .padding(.vertical, 8)
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
                }
                .padding([.horizontal, .bottom])
            } else {
                Spacer()
                Text("Collez un SysEx hexadécimal ci-dessus pour afficher le décodage détaillé.")
                    .foregroundColor(.secondary)
                    .italic()
                Spacer()
            }
        }
    }
    
    // Form Visibility Helpers
    private var showPartField: Bool {
        switch selectedType {
        case .allocatePart, .shutdownPart, .adlibPartInstrument, .parameterAdjust:
            return true
        default:
            return false
        }
    }
    
    private var showChannelField: Bool {
        switch selectedType {
        case .hookPartOnOff, .hookSetVolume, .hookSetProgram, .hookSetTranspose, .setInstrument:
            return true
        default:
            return false
        }
    }
    
    private var showUnknownField: Bool {
        switch selectedType {
        case .allocatePart, .adlibPartInstrument, .adlibGlobalInstrument, .parameterAdjust, .hookJump, .hookGlobalTranspose, .marker, .setLoop:
            return true
        default:
            return false
        }
    }
    
    @ViewBuilder
    private var typeSpecificFields: some View {
        switch selectedType {
        case .allocatePart:
            Toggle("Partie Active (PartOn)", isOn: $partOn)
                .onChange(of: partOn) { _ in updateGeneratedHex() }
            Toggle("Reverb Active", isOn: $reverb)
                .onChange(of: reverb) { _ in updateGeneratedHex() }
            
            HStack {
                Text("Priorité (0-255):")
                Spacer()
                TextField("", value: $priority, format: .number)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 65)
                    .multilineTextAlignment(.trailing)
                    .onChange(of: priority) { newValue in
                        if newValue < 0 { priority = 0 }
                        else if newValue > 255 { priority = 255 }
                        updateGeneratedHex()
                    }
                Slider(value: Binding(get: { Double(priority) }, set: { priority = Int($0) }), in: 0...255)
                    .frame(width: 120)
            }.onChange(of: priority) { _ in updateGeneratedHex() }
            
            HStack {
                Text("Volume (0-127):")
                Spacer()
                TextField("", value: $volume, format: .number)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 65)
                    .multilineTextAlignment(.trailing)
                    .onChange(of: volume) { newValue in
                        if newValue < 0 { volume = 0 }
                        else if newValue > 127 { volume = 127 }
                        updateGeneratedHex()
                    }
                Slider(value: Binding(get: { Double(volume) }, set: { volume = Int($0) }), in: 0...127)
                    .frame(width: 120)
            }.onChange(of: volume) { _ in updateGeneratedHex() }
            
            HStack {
                Text("Panoramique (0-128):")
                Spacer()
                TextField("", value: $pan, format: .number)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 65)
                    .multilineTextAlignment(.trailing)
                    .onChange(of: pan) { newValue in
                        if newValue < 0 { pan = 0 }
                        else if newValue > 128 { pan = 128 }
                        updateGeneratedHex()
                    }
                Slider(value: Binding(get: { Double(pan) }, set: { pan = Int($0) }), in: 0...128)
                    .frame(width: 120)
            }.onChange(of: pan) { _ in updateGeneratedHex() }
            Text(pan == 128 ? "128 (Défaut/Percussion)" : "\(pan)").font(.caption2).foregroundColor(.secondary)
            
            Toggle("Percussion", isOn: $percussion)
                .onChange(of: percussion) { _ in updateGeneratedHex() }
            
            TypeableStepper(title: "Transposition (-127..127)", value: $transpose, range: -127...127)
                .onChange(of: transpose) { _ in updateGeneratedHex() }
            
            TypeableStepper(title: "Désaccordage (-128..127)", value: $detune, range: -128...127)
                .onChange(of: detune) { _ in updateGeneratedHex() }
            
            TypeableStepper(title: "Pitchbend Factor (0-255)", value: $pitchbend, range: 0...255)
                .onChange(of: pitchbend) { _ in updateGeneratedHex() }
            
            TypeableStepper(title: "Programme (0-127)", value: $program, range: 0...127)
                .onChange(of: program) { _ in updateGeneratedHex() }
            
        case .parameterAdjust:
            TypeableStepper(title: "Paramètre (0-65535)", value: $param, range: 0...65535)
                .onChange(of: param) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Valeur (0-65535)", value: $paramValue, range: 0...65535)
                .onChange(of: paramValue) { _ in updateGeneratedHex() }
            
        case .hookJump:
            TypeableStepper(title: "Commande Hook (0-255)", value: $hookCmd, range: 0...255)
                .onChange(of: hookCmd) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Piste cible (0-65535)", value: $targetTrack, range: 0...65535)
                .onChange(of: targetTrack) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Mesure cible (0-65535)", value: $targetBeat, range: 0...65535)
                .onChange(of: targetBeat) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Tick cible (0-65535)", value: $targetTick, range: 0...65535)
                .onChange(of: targetTick) { _ in updateGeneratedHex() }
            
        case .hookGlobalTranspose:
            TypeableStepper(title: "Commande Hook (0-255)", value: $hookCmd, range: 0...255)
                .onChange(of: hookCmd) { _ in updateGeneratedHex() }
            Toggle("Transposition Relative", isOn: $hookRelative)
                .onChange(of: hookRelative) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Valeur signée (-128..127)", value: $hookValue, range: -128...127)
                .onChange(of: hookValue) { _ in updateGeneratedHex() }
            
        case .hookPartOnOff, .hookSetVolume, .hookSetProgram:
            TypeableStepper(title: "Commande Hook (0-255)", value: $hookCmd, range: 0...255)
                .onChange(of: hookCmd) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Valeur (0-255)", value: $hookValue, range: 0...255)
                .onChange(of: hookValue) { _ in updateGeneratedHex() }
            
        case .hookSetTranspose:
            TypeableStepper(title: "Commande Hook (0-255)", value: $hookCmd, range: 0...255)
                .onChange(of: hookCmd) { _ in updateGeneratedHex() }
            Toggle("Transposition Relative", isOn: $hookRelative)
                .onChange(of: hookRelative) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Valeur signée (-128..127)", value: $hookValue, range: -128...127)
                .onChange(of: hookValue) { _ in updateGeneratedHex() }
            
        case .marker:
            TextField("Texte du Marqueur", text: $markerText)
                .textFieldStyle(.roundedBorder)
                .onChange(of: markerText) { _ in updateGeneratedHex() }
            
        case .setLoop:
            TypeableStepper(title: "Nombre de Boucles (0-65535)", value: $loopCount, range: 0...65535)
                .onChange(of: loopCount) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Aller à (Mesure)", value: $loopToBeat, range: 0...65535)
                .onChange(of: loopToBeat) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Aller à (Tick)", value: $loopToTick, range: 0...65535)
                .onChange(of: loopToTick) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Depuis (Mesure)", value: $loopFromBeat, range: 0...65535)
                .onChange(of: loopFromBeat) { _ in updateGeneratedHex() }
            TypeableStepper(title: "Depuis (Tick)", value: $loopFromTick, range: 0...65535)
                .onChange(of: loopFromTick) { _ in updateGeneratedHex() }
            
        case .setInstrument:
            TypeableStepper(title: "Instrument ID (0-65535)", value: $instrumentID, range: 0...65535)
                .onChange(of: instrumentID) { _ in updateGeneratedHex() }
            
        case .adlibPartInstrument, .adlibGlobalInstrument:
            if selectedType == .adlibGlobalInstrument {
                TypeableStepper(title: "Valeur (0-127)", value: $hookValue, range: 0...127)
                    .onChange(of: hookValue) { _ in updateGeneratedHex() }
                TypeableStepper(title: "Programme (0-127)", value: $program, range: 0...127)
                    .onChange(of: program) { _ in updateGeneratedHex() }
            }
            VStack(alignment: .leading, spacing: 4) {
                Text("Registres AdLib (Hexadécimal)")
                TextEditor(text: $adlibHex)
                    .font(.system(.body, design: .monospaced))
                    .frame(height: 60)
                    .overlay(
                        RoundedRectangle(cornerRadius: 4)
                            .stroke(Color.secondary.opacity(0.2), lineWidth: 1)
                    )
                    .onChange(of: adlibHex) { _ in updateGeneratedHex() }
            }
            
        default:
            EmptyView()
        }
    }
    
    private func updateGeneratedHex() {
        var bytes: [UInt8] = []
        
        switch selectedType {
        case .allocatePart:
            bytes = generateAllocatePart()
        case .shutdownPart:
            bytes = [0x7D, 0x01, UInt8(part & 0x0F)]
        case .startSong:
            bytes = [0x7D, 0x02]
        case .parameterAdjust:
            let nibs = encodeNibbles([
                UInt8((param >> 8) & 0xFF), UInt8(param & 0xFF),
                UInt8((paramValue >> 8) & 0xFF), UInt8(paramValue & 0xFF)
            ])
            bytes = [0x7D, 0x21, UInt8(part & 0x0F), UInt8(unknown & 0x7F)] + nibs
        case .hookJump:
            let nibs = encodeNibbles([
                UInt8(hookCmd & 0xFF),
                UInt8((targetTrack >> 8) & 0xFF), UInt8(targetTrack & 0xFF),
                UInt8((targetBeat >> 8) & 0xFF), UInt8(targetBeat & 0xFF),
                UInt8((targetTick >> 8) & 0xFF), UInt8(targetTick & 0xFF)
            ])
            bytes = [0x7D, 0x30, UInt8(unknown & 0x7F)] + nibs
        case .hookGlobalTranspose:
            let nibs = encodeNibbles([
                UInt8(hookCmd & 0xFF),
                hookRelative ? 1 : 0,
                UInt8(bitPattern: Int8(hookValue))
            ])
            bytes = [0x7D, 0x31, UInt8(unknown & 0x7F)] + nibs
        case .hookPartOnOff, .hookSetVolume, .hookSetProgram:
            let nibs = encodeNibbles([
                UInt8(hookCmd & 0xFF),
                UInt8(hookValue & 0xFF)
            ])
            bytes = [0x7D, selectedType.code, UInt8(channel & 0x0F)] + nibs
        case .hookSetTranspose:
            let nibs = encodeNibbles([
                UInt8(hookCmd & 0xFF),
                hookRelative ? 1 : 0,
                UInt8(bitPattern: Int8(hookValue))
            ])
            bytes = [0x7D, 0x35, UInt8(channel & 0x0F)] + nibs
        case .marker:
            let textBytes = Array(markerText.utf8)
            bytes = [0x7D, 0x40, UInt8(unknown & 0x7F)] + textBytes
        case .setLoop:
            let nibs = encodeNibbles([
                UInt8((loopCount >> 8) & 0xFF), UInt8(loopCount & 0xFF),
                UInt8((loopToBeat >> 8) & 0xFF), UInt8(loopToBeat & 0xFF),
                UInt8((loopToTick >> 8) & 0xFF), UInt8(loopToTick & 0xFF),
                UInt8((loopFromBeat >> 8) & 0xFF), UInt8(loopFromBeat & 0xFF),
                UInt8((loopFromTick >> 8) & 0xFF), UInt8(loopFromTick & 0xFF)
            ])
            bytes = [0x7D, 0x50, UInt8(unknown & 0x7F)] + nibs
        case .clearLoop:
            bytes = [0x7D, 0x51]
        case .setInstrument:
            let n1 = UInt8((instrumentID >> 12) & 0x0F)
            let n2 = UInt8((instrumentID >> 8) & 0x0F)
            let n3 = UInt8((instrumentID >> 4) & 0x0F)
            let n4 = UInt8(instrumentID & 0x0F)
            bytes = [0x7D, 0x60, UInt8(channel & 0x0F), n1, n2, n3, n4]
        case .adlibPartInstrument, .adlibGlobalInstrument:
            let registerBytes = parseHexBytes(adlibHex)
            let nibs = encodeNibbles(registerBytes)
            if selectedType == .adlibPartInstrument {
                bytes = [0x7D, 0x10, UInt8(part & 0x0F), UInt8(unknown & 0x7F)] + nibs
            } else {
                bytes = [0x7D, 0x11, UInt8(unknown & 0x7F), UInt8(hookValue & 0x7F), UInt8(program & 0x7F)] + nibs
            }
        }
        
        let fullBytes = [0xF0] + bytes + [0xF7]
        generatedHex = fullBytes.map { String(format: "%02X", $0) }.joined(separator: " ")
    }
    
    private func generateAllocatePart() -> [UInt8] {
        var payload: [UInt8] = []
        payload.append(UInt8(part & 0x0F))
        payload.append(UInt8(unknown & 0x7F))
        
        var params: [UInt8] = []
        let b0 = (partOn ? 0x01 : 0) | (reverb ? 0x02 : 0)
        params.append(UInt8(b0))
        params.append(UInt8(priority & 0xFF))
        params.append(UInt8(volume & 0xFF))
        params.append(UInt8(pan & 0xFF))
        
        let b4 = (percussion ? 0x80 : 0) | (UInt8(bitPattern: Int8(transpose)) & 0x7F)
        params.append(b4)
        params.append(UInt8(bitPattern: Int8(detune)))
        params.append(UInt8(pitchbend & 0xFF))
        params.append(UInt8(program & 0xFF))
        
        for b in params {
            payload.append((b >> 4) & 0x0F)
            payload.append(b & 0x0F)
        }
        
        return [0x7D, 0x00] + payload
    }
    
    private func encodeNibbles(_ bytes: [UInt8]) -> [UInt8] {
        var nibbles: [UInt8] = []
        for b in bytes {
            nibbles.append((b >> 4) & 0x0F)
            nibbles.append(b & 0x0F)
        }
        return nibbles
    }
    
    private func parseHexBytes(_ hex: String) -> [UInt8] {
        let clean = hex.replacingOccurrences(of: " ", with: "")
                        .replacingOccurrences(of: ",", with: "")
                        .replacingOccurrences(of: "\n", with: "")
                        .replacingOccurrences(of: "\r", with: "")
        
        var bytes: [UInt8] = []
        var index = clean.startIndex
        while index < clean.endIndex {
            let nextIndex = clean.index(index, offsetBy: 2, limitedBy: clean.endIndex) ?? clean.endIndex
            let substr = clean[index..<nextIndex]
            if let b = UInt8(substr, radix: 16) {
                bytes.append(b)
            }
            index = nextIndex
        }
        return bytes
    }
    
    private func copyToClipboard() {
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(generatedHex, forType: .string)
    }
    
    private func parseHex() {
        let cleanHex = hexInput.replacingOccurrences(of: " ", with: "")
                             .replacingOccurrences(of: ",", with: "")
                             .replacingOccurrences(of: "\n", with: "")
                             .replacingOccurrences(of: "\r", with: "")
        
        if cleanHex.isEmpty {
            decodedMessage = nil
            parseError = ""
            return
        }
        
        var bytes: [UInt8] = []
        var index = cleanHex.startIndex
        while index < cleanHex.endIndex {
            let nextIndex = cleanHex.index(index, offsetBy: 2, limitedBy: cleanHex.endIndex) ?? cleanHex.endIndex
            let substr = cleanHex[index..<nextIndex]
            guard let b = UInt8(substr, radix: 16) else {
                parseError = "Erreur de format hexadécimal."
                decodedMessage = nil
                return
            }
            bytes.append(b)
            index = nextIndex
        }
        
        var start = 0
        var end = bytes.count
        if start < end && bytes[start] == 0xF0 { start += 1 }
        if end > start && bytes[end - 1] == 0xF7 { end -= 1 }
        
        let body = Array(bytes[start..<end])
        
        if body.isEmpty {
            parseError = "Message vide."
            decodedMessage = nil
            return
        }
        
        if body[0] != 0x7D {
            parseError = "ID manufacturier de compatibilité absent (valeur attendue : 7D)."
            decodedMessage = nil
            return
        }
        
        if body.count < 2 {
            parseError = "Message trop court."
            decodedMessage = nil
            return
        }
        
        let code = body[1]
        let payload = Array(body[2...])
        
        var dec = DecodedSysEx(typeName: "Inconnu", code: code)
        parseError = ""
        
        switch code {
        case 0x00:
            dec.typeName = "Allocate Part (0x00)"
            guard payload.count == 18 else {
                parseError = "Taille invalide pour Allocate Part (attendus: 18 octets de payload, reçus: \(payload.count))."
                decodedMessage = nil
                return
            }
            let part = payload[0] & 0x0F
            let unknown = payload[1] & 0x7F
            let nibs = Array(payload[2...])
            let decBytes = decodeNibbles(nibs)
            
            let partOn = (decBytes[0] & 0x01) != 0
            let reverb = (decBytes[0] & 0x02) != 0
            let priority = decBytes[1]
            let volume = decBytes[2]
            let pan = decBytes[3]
            let percussion = (decBytes[4] & 0x80) != 0
            let transpose = Int8(bitPattern: decBytes[4] & 0x7F)
            let detune = Int8(bitPattern: decBytes[5])
            let pitchbend = decBytes[6]
            let program = decBytes[7]
            
            dec.details = [
                ("Part", "\(part)"),
                ("Unknown", "\(unknown)"),
                ("PartOn (Active)", "\(partOn)"),
                ("Reverb", "\(reverb)"),
                ("Priorité", "\(priority)"),
                ("Volume", "\(volume)"),
                ("Panoramique", pan == 128 ? "128 (Par défaut / Percussions)" : "\(pan)"),
                ("Percussion", "\(percussion)"),
                ("Transposition", "\(transpose)"),
                ("Detune", "\(detune)"),
                ("Pitchbend Factor", "\(pitchbend)"),
                ("Programme (Instrument)", "\(program)")
            ]
            
        case 0x01:
            dec.typeName = "Shutdown Part (0x01)"
            guard payload.count == 1 else {
                parseError = "Shutdown Part doit contenir exactement 1 octet de payload."
                decodedMessage = nil
                return
            }
            dec.details = [
                ("Part", "\(payload[0] & 0x0F)")
            ]
            
        case 0x02:
            dec.typeName = "Start Song (0x02)"
            dec.details = []
            
        case 0x10, 0x11:
            let isPart = (code == 0x10)
            dec.typeName = isPart ? "AdLib Part Instrument (0x10)" : "AdLib Global Instrument (0x11)"
            let headerSize = isPart ? 2 : 3
            guard payload.count >= headerSize else {
                parseError = "Payload trop court."
                decodedMessage = nil
                return
            }
            if isPart {
                let part = payload[0] & 0x0F
                let unknown = payload[1] & 0x7F
                let decBytes = decodeNibbles(Array(payload[2...]))
                dec.details = [
                    ("Part", "\(part)"),
                    ("Unknown", "\(unknown)"),
                    ("Registres AdLib (Décodes)", decBytes.map { String(format: "%02X", $0) }.joined(separator: " "))
                ]
            } else {
                let unknown = payload[0] & 0x7F
                let val = payload[1] & 0x7F
                let prog = payload[2] & 0x7F
                let decBytes = decodeNibbles(Array(payload[3...]))
                dec.details = [
                    ("Unknown", "\(unknown)"),
                    ("Valeur", "\(val)"),
                    ("Programme", "\(prog)"),
                    ("Registres AdLib (Décodes)", decBytes.map { String(format: "%02X", $0) }.joined(separator: " "))
                ]
            }
            
        case 0x21:
            dec.typeName = "Parameter Adjust (0x21)"
            guard payload.count == 10 else {
                parseError = "Parameter Adjust doit contenir 10 octets de payload."
                decodedMessage = nil
                return
            }
            let part = payload[0] & 0x0F
            let unknown = payload[1] & 0x7F
            let decBytes = decodeNibbles(Array(payload[2...]))
            let param = UInt16(decBytes[0]) << 8 | UInt16(decBytes[1])
            let val = UInt16(decBytes[2]) << 8 | UInt16(decBytes[3])
            dec.details = [
                ("Part", "\(part)"),
                ("Unknown", "\(unknown)"),
                ("Paramètre ID", "\(param)"),
                ("Valeur", "\(val)")
            ]
            
        case 0x30:
            dec.typeName = "Hook Jump (0x30)"
            guard payload.count == 15 else {
                parseError = "Hook Jump doit contenir 15 octets de payload."
                decodedMessage = nil
                return
            }
            let unknown = payload[0] & 0x7F
            let decBytes = decodeNibbles(Array(payload[1...]))
            let cmd = decBytes[0]
            let track = UInt16(decBytes[1]) << 8 | UInt16(decBytes[2])
            let beat = UInt16(decBytes[3]) << 8 | UInt16(decBytes[4])
            let tick = UInt16(decBytes[5]) << 8 | UInt16(decBytes[6])
            
            dec.details = [
                ("Unknown", "\(unknown)"),
                ("Commande Hook", "\(cmd)"),
                ("Piste cible", "\(track)"),
                ("Mesure cible", "\(beat)"),
                ("Tick cible", "\(tick)")
            ]
            
        case 0x31:
            dec.typeName = "Hook Global Transpose (0x31)"
            guard payload.count == 7 else {
                parseError = "Global Transpose doit contenir 7 octets."
                decodedMessage = nil
                return
            }
            let unknown = payload[0] & 0x7F
            let decBytes = decodeNibbles(Array(payload[1...]))
            dec.details = [
                ("Unknown", "\(unknown)"),
                ("Commande Hook", "\(decBytes[0])"),
                ("Transposition Relative", "\(decBytes[1] != 0)"),
                ("Valeur", "\(Int8(bitPattern: decBytes[2]))")
            ]
            
        case 0x32, 0x33, 0x34:
            dec.typeName = (code == 0x32) ? "Hook Part On/Off (0x32)" :
                           (code == 0x33) ? "Hook Set Volume (0x33)" : "Hook Set Program (0x34)"
            guard payload.count == 5 else {
                parseError = "Hook de canal doit contenir 5 octets."
                decodedMessage = nil
                return
            }
            let chan = payload[0] & 0x0F
            let decBytes = decodeNibbles(Array(payload[1...]))
            dec.details = [
                ("Canal MIDI", "\(chan)"),
                ("Commande Hook", "\(decBytes[0])"),
                ("Valeur", "\(decBytes[1])")
            ]
            
        case 0x35:
            dec.typeName = "Hook Set Transpose (0x35)"
            guard payload.count == 7 else {
                parseError = "Set Transpose doit contenir 7 octets."
                decodedMessage = nil
                return
            }
            let chan = payload[0] & 0x0F
            let decBytes = decodeNibbles(Array(payload[1...]))
            dec.details = [
                ("Canal MIDI", "\(chan)"),
                ("Commande Hook", "\(decBytes[0])"),
                ("Transposition Relative", "\(decBytes[1] != 0)"),
                ("Valeur", "\(Int8(bitPattern: decBytes[2]))")
            ]
            
        case 0x40:
            dec.typeName = "Marker (0x40)"
            guard payload.count >= 1 else {
                parseError = "Marker vide."
                decodedMessage = nil
                return
            }
            let unknown = payload[0] & 0x7F
            let text = String(bytes: Array(payload[1...]), encoding: .utf8) ?? "<Caractères non UTF-8>"
            dec.details = [
                ("Unknown", "\(unknown)"),
                ("Texte du Marqueur", text)
            ]
            
        case 0x50:
            dec.typeName = "Set Loop (0x50)"
            guard payload.count == 21 else {
                parseError = "Set Loop doit contenir 21 octets."
                decodedMessage = nil
                return
            }
            let unknown = payload[0] & 0x7F
            let decBytes = decodeNibbles(Array(payload[1...]))
            let count = UInt16(decBytes[0]) << 8 | UInt16(decBytes[1])
            let toBeat = UInt16(decBytes[2]) << 8 | UInt16(decBytes[3])
            let toTick = UInt16(decBytes[4]) << 8 | UInt16(decBytes[5])
            let fromBeat = UInt16(decBytes[6]) << 8 | UInt16(decBytes[7])
            let fromTick = UInt16(decBytes[8]) << 8 | UInt16(decBytes[9])
            
            dec.details = [
                ("Unknown", "\(unknown)"),
                ("Nombre de Boucles", count == 0 ? "Boucle Infinie (0)" : "\(count)"),
                ("Aller à (Mesure)", "\(toBeat)"),
                ("Aller à (Tick)", "\(toTick)"),
                ("Depuis (Mesure)", "\(fromBeat)"),
                ("Depuis (Tick)", "\(fromTick)")
            ]
            
        case 0x51:
            dec.typeName = "Clear Loop (0x51)"
            dec.details = []
            
        case 0x60:
            dec.typeName = "Set Instrument (0x60)"
            guard payload.count == 5 else {
                parseError = "Set Instrument doit contenir 5 octets."
                decodedMessage = nil
                return
            }
            let chan = payload[0] & 0x0F
            let inst = Int(payload[1] & 0x0F) << 12 |
                       Int(payload[2] & 0x0F) << 8 |
                       Int(payload[3] & 0x0F) << 4 |
                       Int(payload[4] & 0x0F)
            dec.details = [
                ("Canal MIDI", "\(chan)"),
                ("Instrument ID", "\(inst)")
            ]
            
        default:
            dec.typeName = "Inconnu (Code 0x\(String(format: "%02X", code)))"
            dec.details = [
                ("Données brutes (Hex)", payload.map { String(format: "%02X", $0) }.joined(separator: " "))
            ]
        }
        
        decodedMessage = dec
    }
    
    private func decodeNibbles(_ nibbles: [UInt8]) -> [UInt8] {
        var bytes: [UInt8] = []
        for i in stride(from: 0, to: nibbles.count, by: 2) {
            if i + 1 < nibbles.count {
                bytes.append(((nibbles[i] & 0x0F) << 4) | (nibbles[i+1] & 0x0F))
            }
        }
        return bytes
    }
}

struct SysExDetail: Identifiable {
    var id: String { key }
    let key: String
    let value: String
}

struct SysExDetailList: ExpressibleByArrayLiteral, RandomAccessCollection {
    typealias ArrayLiteralElement = (String, String)
    typealias Index = Int
    typealias Element = SysExDetail
    
    let items: [SysExDetail]
    
    init(arrayLiteral elements: (String, String)...) {
        self.items = elements.map { SysExDetail(key: $0.0, value: $0.1) }
    }
    
    var startIndex: Int { items.startIndex }
    var endIndex: Int { items.endIndex }
    subscript(position: Int) -> SysExDetail { items[position] }
}

struct DecodedSysEx {
    var typeName: String
    var code: UInt8
    var details: SysExDetailList = []
}
