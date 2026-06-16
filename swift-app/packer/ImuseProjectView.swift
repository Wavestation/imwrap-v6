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

import SwiftUI
import UniformTypeIdentifiers
import ImuseCoreSwift

// UI wrapper for editing a MIDI track inside a Format 2 Variant
struct ProjectTrack: Identifiable, Hashable {
    let id: UUID = UUID()
    var name: String
    var events: [SwiftMidiEvent] = []
    var sourceFileName: String = ""
}

@MainActor
class ProjectViewModel: ObservableObject {
    @Published var project: SwiftImsProject = SwiftImsProject()
    @Published var selectedSoundID: UInt16? = nil
    @Published var selectedVariantKind: String = "GMD "
    @Published var selectedTrackID: UUID? = nil

    @Published var loadedFilePath: String = ""
    @Published var statusMessage: String = ""
    @Published var isErrorStatus: Bool = false
    @Published var hasUnsavedChanges: Bool = false

    // UI binding wrappers for the selected sound
    @Published var soundName: String = ""
    @Published var soundIDText: String = ""

    // Selected variant track list
    @Published var variantTracks: [ProjectTrack] = []
    @Published var includeVariant: Bool = false
    @Published var includeMdhd: Bool = false

    // Division MIDI (préservée depuis le fichier importé)
    var midiDivision: UInt16 = 480

    // MDhd fields
    @Published var priority: Int = 90
    @Published var volume: Int = 127
    @Published var pan: Int = 0
    @Published var transpose: Int = 0
    @Published var detune: Int = 0
    @Published var speed: Int = 128

    var selectedSound: SwiftSound? {
        project.sounds.first(where: { $0.id == selectedSoundID })
    }

    // MARK: - Project lifecycle

    func newProject() {
        project = SwiftImsProject(sounds: [])
        selectedSoundID = nil
        loadedFilePath = ""
        hasUnsavedChanges = false
        soundName = ""
        soundIDText = ""
        variantTracks = []
        setStatus("Nouveau projet créé", isError: false)
    }

    func loadImsFile() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.data]
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Ouvrir"
        if panel.runModal() == .OK, let url = panel.url {
            do {
                let bytes = [UInt8](try Data(contentsOf: url))
                if let loaded = ImsSerializer.loadIms(from: bytes) {
                    project = loaded
                    selectedSoundID = project.sounds.first?.id
                    loadedFilePath = url.path
                    hasUnsavedChanges = false
                    setStatus("IMS chargé – \(project.sounds.count) son(s)", isError: false)
                    if let firstId = selectedSoundID {
                        selectSound(id: firstId)
                    }
                } else {
                    setStatus("Échec du décodage IMS", isError: true)
                }
            } catch {
                setStatus("Erreur de lecture : \(error.localizedDescription)", isError: true)
            }
        }
    }

    /// Enregistrer (sans dialog si le chemin est connu)
    func saveImsFile() {
        syncSelectedSoundToModel()
        if !loadedFilePath.isEmpty {
            let url = URL(fileURLWithPath: loadedFilePath)
            let bytes = ImsSerializer.saveIms(project)
            do {
                try Data(bytes).write(to: url)
                hasUnsavedChanges = false
                setStatus("Enregistré", isError: false)
            } catch {
                setStatus("Erreur d'écriture : \(error.localizedDescription)", isError: true)
            }
        } else {
            saveImsFileAs()
        }
    }

    /// Enregistrer sous (toujours avec dialog)
    func saveImsFileAs() {
        syncSelectedSoundToModel()
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.data]
        panel.prompt = "Enregistrer"
        panel.nameFieldStringValue = loadedFilePath.isEmpty
            ? "banque.ims"
            : URL(fileURLWithPath: loadedFilePath).lastPathComponent
        if panel.runModal() == .OK, let url = panel.url {
            let bytes = ImsSerializer.saveIms(project)
            do {
                try Data(bytes).write(to: url)
                loadedFilePath = url.path
                hasUnsavedChanges = false
                setStatus("Enregistré sous \(url.lastPathComponent)", isError: false)
            } catch {
                setStatus("Erreur d'écriture : \(error.localizedDescription)", isError: true)
            }
        }
    }

    func setStatus(_ text: String, isError: Bool) {
        statusMessage = text
        isErrorStatus = isError
    }

    // MARK: - Sound management

    func selectSound(id: UInt16) {
        syncSelectedSoundToModel()
        selectedSoundID = id
        guard let sound = project.sounds.first(where: { $0.id == id }) else { return }
        soundName = sound.name
        soundIDText = "\(sound.id)"
        loadVariant(kind: selectedVariantKind)
    }

    func selectVariantKind(_ kind: String) {
        syncSelectedSoundToModel()
        selectedVariantKind = kind
        loadVariant(kind: kind)
    }

    private func loadVariant(kind: String) {
        guard let sound = selectedSound else { return }
        if let variant = sound.variants.first(where: { $0.kind == kind }) {
            includeVariant = true
            includeMdhd = variant.mdhd.present
            priority = Int(variant.mdhd.priority)
            volume = Int(variant.mdhd.volume)
            pan = Int(variant.mdhd.pan)
            transpose = Int(variant.mdhd.transpose)
            detune = Int(variant.mdhd.detune)
            speed = Int(variant.mdhd.speed)
            if !variant.smfData.isEmpty, let smf = MidiConverter.parseMidiFile(variant.smfData) {
                midiDivision = smf.division
                variantTracks = smf.tracks.enumerated().map { (index, track) in
                    ProjectTrack(name: "Piste \(index)", events: track.events, sourceFileName: "SMF Importé")
                }
            } else {
                variantTracks = []
            }
        } else {
            includeVariant = false
            includeMdhd = false
            variantTracks = []
        }
        selectedTrackID = variantTracks.first?.id
    }

    /// Applique les modifications du son courant au modèle projet sans sauvegarder sur disque
    func applyCurrentSoundChanges() {
        syncSelectedSoundToModel()
        hasUnsavedChanges = true
        setStatus("Modifications appliquées", isError: false)
    }

    func syncSelectedSoundToModel() {
        guard let id = selectedSoundID,
              let index = project.sounds.firstIndex(where: { $0.id == id }) else { return }

        if let newID = UInt16(soundIDText) {
            project.sounds[index].id = newID
            selectedSoundID = newID
        }
        project.sounds[index].name = soundName

        var variants = project.sounds[index].variants
        let varIndex = variants.firstIndex(where: { $0.kind == selectedVariantKind })

        if includeVariant {
            let md = SwiftMdhd(present: includeMdhd,
                               priority: UInt8(clamping: priority),
                               volume: UInt8(clamping: volume),
                               pan: Int8(clamping: pan),
                               transpose: Int8(clamping: transpose),
                               detune: Int8(clamping: detune),
                               speed: UInt8(clamping: speed))
            var smfData: [UInt8] = []
            if !variantTracks.isEmpty {
                let fileTracks = variantTracks.map { SwiftMidiTrack(events: $0.events) }
                let smfFile = SwiftMidiFile(format: 2, division: midiDivision, tracks: fileTracks)
                smfData = MidiConverter.serializeMidiFile(smfFile)
            }
            let newVariant = SwiftVariant(kind: selectedVariantKind, mdhd: md, smfData: smfData)
            if let idx = varIndex {
                variants[idx] = newVariant
            } else {
                variants.append(newVariant)
            }
        } else {
            if let idx = varIndex { variants.remove(at: idx) }
        }
        project.sounds[index].variants = variants
    }

    func addSound() {
        syncSelectedSoundToModel()
        let maxID = project.sounds.map { $0.id }.max() ?? 79
        let newID = maxID + 1
        let sound = SwiftSound(id: newID, name: "Son\(newID)", variants: [
            SwiftVariant(kind: "GMD ", mdhd: SwiftMdhd(present: true), smfData: [])
        ])
        project.sounds.append(sound)
        hasUnsavedChanges = true
        selectSound(id: newID)
        setStatus("Son \(newID) ajouté", isError: false)
    }

    func duplicateSound() {
        guard let id = selectedSoundID,
              let sound = project.sounds.first(where: { $0.id == id }) else { return }
        syncSelectedSoundToModel()
        let maxID = project.sounds.map { $0.id }.max() ?? 79
        let newID = maxID + 1
        let copy = SwiftSound(id: newID, name: "\(sound.name) (copie)", variants: sound.variants)
        project.sounds.append(copy)
        hasUnsavedChanges = true
        selectSound(id: newID)
        setStatus("Son dupliqué → ID \(newID)", isError: false)
    }

    func deleteSound() {
        guard let id = selectedSoundID else { return }
        syncSelectedSoundToModel()
        project.sounds.removeAll(where: { $0.id == id })
        selectedSoundID = project.sounds.first?.id
        hasUnsavedChanges = true
        if let nextId = selectedSoundID {
            selectSound(id: nextId)
        } else {
            soundName = ""
            soundIDText = ""
            variantTracks = []
        }
        setStatus("Son supprimé", isError: false)
    }

    func moveSoundUp() {
        guard let id = selectedSoundID,
              let index = project.sounds.firstIndex(where: { $0.id == id }),
              index > 0 else { return }
        project.sounds.swapAt(index, index - 1)
        hasUnsavedChanges = true
    }

    func moveSoundDown() {
        guard let id = selectedSoundID,
              let index = project.sounds.firstIndex(where: { $0.id == id }),
              index < project.sounds.count - 1 else { return }
        project.sounds.swapAt(index, index + 1)
        hasUnsavedChanges = true
    }

    // MARK: - Track management

    func addTrack() {
        let newTrack = ProjectTrack(name: "Piste \(variantTracks.count)", events: [], sourceFileName: "Vide")
        variantTracks.append(newTrack)
        selectedTrackID = newTrack.id
        hasUnsavedChanges = true
    }

    func deleteTrack() {
        guard let id = selectedTrackID else { return }
        variantTracks.removeAll(where: { $0.id == id })
        selectedTrackID = variantTracks.first?.id
        hasUnsavedChanges = true
    }

    func renameSelectedTrack(to newName: String) {
        guard let id = selectedTrackID,
              let index = variantTracks.firstIndex(where: { $0.id == id }) else { return }
        variantTracks[index].name = newName
        hasUnsavedChanges = true
    }

    func moveTrackUp() {
        guard let id = selectedTrackID,
              let index = variantTracks.firstIndex(where: { $0.id == id }),
              index > 0 else { return }
        variantTracks.swapAt(index, index - 1)
        hasUnsavedChanges = true
    }

    func moveTrackDown() {
        guard let id = selectedTrackID,
              let index = variantTracks.firstIndex(where: { $0.id == id }),
              index < variantTracks.count - 1 else { return }
        variantTracks.swapAt(index, index + 1)
        hasUnsavedChanges = true
    }

    // MARK: - MIDI import

    func importMidiForSelectedTrack() {
        guard let trackId = selectedTrackID,
              let trackIndex = variantTracks.firstIndex(where: { $0.id == trackId }) else {
            setStatus("Sélectionnez une piste d'abord", isError: true)
            return
        }
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "mid")!, UTType(filenameExtension: "midi")!]
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Importer"
        if panel.runModal() == .OK, let url = panel.url {
            do {
                let bytes = [UInt8](try Data(contentsOf: url))
                if let smf = MidiConverter.parseMidiFile(bytes) {
                    // Préserver la division du fichier source
                    midiDivision = smf.division

                    if smf.format == 2 {
                        // Format 2 → remplace TOUTES les pistes de la variante
                        variantTracks = smf.tracks.enumerated().map { (index, track) in
                            ProjectTrack(
                                name: index == 0
                                    ? url.deletingPathExtension().lastPathComponent
                                    : "\(url.deletingPathExtension().lastPathComponent) – Piste \(index)",
                                events: track.events,
                                sourceFileName: url.lastPathComponent
                            )
                        }
                        selectedTrackID = variantTracks.first?.id
                        hasUnsavedChanges = true
                        setStatus("Format 2 importé – \(variantTracks.count) piste(s) (div=\(smf.division))", isError: false)
                        return
                    }

                    var parsedTrack = SwiftMidiTrack()
                    var statusMsg = ""
                    if smf.format == 1 {
                        let merged = MidiConverter.mergeTracksToFormat0(smf)
                        parsedTrack = merged.tracks.first ?? SwiftMidiTrack()
                        statusMsg = "Format 1 fusionné en Format 0 (div=\(smf.division))"
                    } else {
                        parsedTrack = smf.tracks.first ?? SwiftMidiTrack()
                        statusMsg = "Format 0 importé (div=\(smf.division))"
                    }
                    variantTracks[trackIndex].events = parsedTrack.events
                    variantTracks[trackIndex].sourceFileName = url.lastPathComponent
                    variantTracks[trackIndex].name = url.deletingPathExtension().lastPathComponent
                    hasUnsavedChanges = true
                    setStatus(statusMsg, isError: false)
                } else {
                    setStatus("Échec du décodage MIDI", isError: true)
                }
            } catch {
                setStatus("Erreur de lecture : \(error.localizedDescription)", isError: true)
            }
        }
    }

    func importFormat2Midi() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "mid")!, UTType(filenameExtension: "midi")!]
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.allowsMultipleSelection = false
        panel.prompt = "Importer MIDI (tout format)"
        if panel.runModal() == .OK, let url = panel.url {
            do {
                let bytes = [UInt8](try Data(contentsOf: url))
                if let smf = MidiConverter.parseMidiFile(bytes) {
                    guard !smf.tracks.isEmpty else {
                        setStatus("Aucune piste dans le fichier MIDI", isError: true)
                        return
                    }
                    // Préserver la division
                    midiDivision = smf.division
                    variantTracks = smf.tracks.enumerated().map { (index, track) in
                        ProjectTrack(
                            name: index == 0
                                ? url.deletingPathExtension().lastPathComponent
                                : "\(url.deletingPathExtension().lastPathComponent) – Piste \(index)",
                            events: track.events,
                            sourceFileName: url.lastPathComponent
                        )
                    }
                    selectedTrackID = variantTracks.first?.id
                    hasUnsavedChanges = true
                    setStatus("Format \(smf.format) importé – \(variantTracks.count) piste(s) – div=\(smf.division)", isError: false)
                } else {
                    setStatus("Échec du décodage MIDI", isError: true)
                }
            } catch {
                setStatus("Erreur de lecture : \(error.localizedDescription)", isError: true)
            }
        }
    }
}

// MARK: - Main View

struct ImuseProjectView: View {
    @StateObject private var model = ProjectViewModel()
    @State private var showingRenameTrack = false
    @State private var renameTrackText = ""

    var body: some View {
        HSplitView {
            soundsSidebar
            if model.selectedSoundID != nil {
                mainEditorPanel
            } else {
                emptyPlaceholder
            }
        }
        .toolbar { toolbarContent }
        .safeAreaInset(edge: .bottom) { statusBar }
        .sheet(isPresented: $showingRenameTrack) { renameTrackSheet }
    }

    // MARK: Sidebar des sons
    private var soundsSidebar: some View {
        VStack(spacing: 0) {
            // En-tête + boutons d'action sur les sons
            HStack(spacing: 0) {
                Text("Sons – \(model.project.sounds.count)")
                    .font(.caption)
                    .fontWeight(.semibold)
                    .foregroundColor(.secondary)
                    .padding(.leading, 10)

                Spacer()

                // Boutons inline dans l'en-tête
                iconBtn("plus",         action: model.addSound,       tip: "Ajouter un son")
                iconBtn("minus",        action: model.deleteSound,     tip: "Supprimer",   disabled: model.selectedSoundID == nil)
                iconBtn("doc.on.doc",   action: model.duplicateSound,  tip: "Dupliquer",   disabled: model.selectedSoundID == nil)
                Color.secondary.opacity(0.25).frame(width: 1, height: 14).padding(.horizontal, 3)
                iconBtn("chevron.up",   action: model.moveSoundUp,     tip: "Monter",      disabled: !canMoveSoundUp)
                iconBtn("chevron.down", action: model.moveSoundDown,   tip: "Descendre",   disabled: !canMoveSoundDown)
            }
            .padding(.vertical, 5)
            .padding(.trailing, 4)
            .background(Color(NSColor.controlBackgroundColor))

            Divider()

            List(model.project.sounds, id: \.id) { sound in
                HStack {
                    Image(systemName: "music.note")
                        .foregroundColor(.blue)
                    VStack(alignment: .leading, spacing: 1) {
                        Text(sound.name.isEmpty ? "Son \(sound.id)" : sound.name)
                            .fontWeight(.medium)
                            .lineLimit(1)
                        Text("ID \(sound.id)  •  \(sound.variants.count) variante(s)")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                    Spacer()
                }
                .padding(.vertical, 2)
                .contentShape(Rectangle())
                .onTapGesture { model.selectSound(id: sound.id) }
                .listRowBackground(model.selectedSoundID == sound.id
                    ? Color.blue.opacity(0.15) : Color.clear)
            }
            .listStyle(.sidebar)
        }
        .frame(minWidth: 200, maxWidth: 280)
    }

    private var canMoveSoundUp: Bool {
        guard let id = model.selectedSoundID,
              let index = model.project.sounds.firstIndex(where: { $0.id == id }) else { return false }
        return index > 0
    }
    private var canMoveSoundDown: Bool {
        guard let id = model.selectedSoundID,
              let index = model.project.sounds.firstIndex(where: { $0.id == id }) else { return false }
        return index < model.project.sounds.count - 1
    }

    // MARK: Placeholder
    private var emptyPlaceholder: some View {
        VStack(spacing: 10) {
            Spacer()
            Image(systemName: "music.note.list")
                .font(.system(size: 40))
                .foregroundColor(.secondary)
            Text("Aucun son sélectionné")
                .font(.title3)
                .foregroundColor(.secondary)
            Text("Créez ou chargez une banque IMS,\npuis ajoutez un son avec le bouton +")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: Toolbar
    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItemGroup(placement: .primaryAction) {
            Button(action: model.newProject) {
                Label("Nouveau", systemImage: "doc.badge.plus")
            }
            .help("Nouveau projet vide")

            Button(action: model.loadImsFile) {
                Label("Ouvrir…", systemImage: "folder")
            }
            .help("Ouvrir un fichier IMS")

            Divider()

            Button(action: model.saveImsFile) {
                Label(
                    model.loadedFilePath.isEmpty ? "Enregistrer…" : "Enregistrer",
                    systemImage: model.hasUnsavedChanges ? "square.and.arrow.down.fill" : "square.and.arrow.down"
                )
            }
            .help(model.loadedFilePath.isEmpty
                  ? "Enregistrer sous…"
                  : "Enregistrer (\(URL(fileURLWithPath: model.loadedFilePath).lastPathComponent))")
            .keyboardShortcut("s", modifiers: .command)

            Button(action: model.saveImsFileAs) {
                Label("Enregistrer sous…", systemImage: "arrow.down.doc")
            }
            .help("Enregistrer sous un nouveau nom")
            .keyboardShortcut("s", modifiers: [.command, .shift])
        }
    }

    // MARK: Status bar
    private var statusBar: some View {
        HStack {
            HStack(spacing: 4) {
                if model.hasUnsavedChanges {
                    Circle().fill(Color.orange).frame(width: 7, height: 7)
                }
                Text(model.loadedFilePath.isEmpty ? "Nouveau projet" : model.loadedFilePath)
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .lineLimit(1)
            }
            Spacer()
            Text(model.statusMessage)
                .font(.caption2)
                .foregroundColor(model.isErrorStatus ? .red : .green)
                .lineLimit(1)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 4)
        .background(Color(NSColor.windowBackgroundColor))
    }

    // MARK: Sheet renommage piste
    private var renameTrackSheet: some View {
        VStack(spacing: 16) {
            Text("Renommer la piste")
                .font(.headline)
            TextField("Nom de la piste", text: $renameTrackText)
                .textFieldStyle(.roundedBorder)
                .frame(width: 280)
            HStack {
                Button("Annuler") { showingRenameTrack = false }
                    .keyboardShortcut(.cancelAction)
                Spacer()
                Button("Renommer") {
                    model.renameSelectedTrack(to: renameTrackText)
                    showingRenameTrack = false
                }
                .keyboardShortcut(.defaultAction)
                .disabled(renameTrackText.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .padding(24)
        .frame(width: 340)
    }

    // MARK: Panneau principal
    private var mainEditorPanel: some View {
        VStack(spacing: 0) {
            // Métadonnées du son
            HStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Nom du Son")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    TextField("Nom", text: $model.soundName)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 200)
                }
                VStack(alignment: .leading, spacing: 2) {
                    Text("ID du Son (UInt16)")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    TextField("ID", text: $model.soundIDText)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 80)
                }
                Spacer()
                Button {
                    model.applyCurrentSoundChanges()
                } label: {
                    Label("Appliquer", systemImage: "checkmark.circle.fill")
                }
                .buttonStyle(.borderedProminent)
                .tint(.blue)
                .help("Valider les modifications avant de changer de son")
            }
            .padding()
            .background(Color(NSColor.controlBackgroundColor).opacity(0.2))

            // Sélecteur de variante
            Picker("", selection: Binding(
                get: { model.selectedVariantKind },
                set: { model.selectVariantKind($0) }
            )) {
                Text("General MIDI (GMD)").tag("GMD ")
                Text("Roland MT-32 (ROL)").tag("ROL ")
            }
            .pickerStyle(.segmented)
            .padding(.horizontal)
            .padding(.top, 8)

            // Éditeur de variante
            ScrollView {
                VStack(spacing: 16) {
                    Toggle("Inclure cette variante dans le son", isOn: $model.includeVariant)
                        .fontWeight(.semibold)
                        .padding(.vertical, 4)
                        .frame(maxWidth: .infinity, alignment: .leading)

                    if model.includeVariant {
                        mdhdGroupBox
                        tracksGroupBox
                    } else {
                        Text("Cette variante n'est pas incluse pour le moment.")
                            .foregroundColor(.secondary)
                            .italic()
                            .padding(.vertical, 40)
                    }
                }
                .padding()
            }
        }
    }

    // MARK: GroupBox MDhd
    private var mdhdGroupBox: some View {
        GroupBox("En-tête MDhd – Paramètres globaux") {
            VStack(spacing: 12) {
                Toggle("Inclure l'en-tête MDhd personnalisé", isOn: $model.includeMdhd)
                    .frame(maxWidth: .infinity, alignment: .leading)
                if model.includeMdhd {
                    Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 10) {
                        GridRow {
                            Text("Priorité (0-255):")
                            TypeableStepper(title: "", value: $model.priority,   range: 0...255,    showSpacer: false)
                            Text("Volume (0-127):")
                            TypeableStepper(title: "", value: $model.volume,     range: 0...127,    showSpacer: false)
                        }
                        GridRow {
                            Text("Panoramique (-128–127):")
                            TypeableStepper(title: "", value: $model.pan,        range: -128...127, showSpacer: false)
                            Text("Transposition (-128–127):")
                            TypeableStepper(title: "", value: $model.transpose,  range: -128...127, showSpacer: false)
                        }
                        GridRow {
                            Text("Désaccordage (-128–127):")
                            TypeableStepper(title: "", value: $model.detune,     range: -128...127, showSpacer: false)
                            Text("Vitesse/Tempo (0-255):")
                            TypeableStepper(title: "", value: $model.speed,      range: 0...255,    showSpacer: false)
                        }
                    }
                    .padding(.vertical, 4)
                }
            }
        }
    }

    // MARK: GroupBox Pistes
    private var tracksGroupBox: some View {
        GroupBox("Assemblage des Pistes MIDI (Format 2)") {
            VStack(spacing: 10) {
                Text("iMUSE utilise le Format 2 pour stocker chaque segment ou boucle dans une piste MIDI distincte.")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)

                HStack(alignment: .top, spacing: 12) {
                    // Liste des pistes
                    VStack(spacing: 0) {
                        List(model.variantTracks) { track in
                            HStack {
                                Image(systemName: "music.note.list")
                                    .foregroundColor(.purple)
                                VStack(alignment: .leading) {
                                    Text(track.name)
                                        .fontWeight(.medium)
                                        .lineLimit(1)
                                    Text(track.sourceFileName)
                                        .font(.caption2)
                                        .foregroundColor(.secondary)
                                }
                                Spacer()
                                Text("\(track.events.count) ev")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                            }
                            .contentShape(Rectangle())
                            .onTapGesture { model.selectedTrackID = track.id }
                            .listRowBackground(model.selectedTrackID == track.id
                                ? Color.purple.opacity(0.12) : Color.clear)
                        }
                        .frame(height: 180)
                        .cornerRadius(4)
                        .overlay(RoundedRectangle(cornerRadius: 4).stroke(Color.secondary.opacity(0.2), lineWidth: 1))

                        // Actions pistes
                        HStack(spacing: 0) {
                            iconBtn("plus",           action: model.addTrack)
                            iconBtn("minus",          action: model.deleteTrack,    disabled: model.selectedTrackID == nil)
                            Color.secondary.opacity(0.3).frame(width: 1, height: 16).padding(.horizontal, 3)
                            iconBtn("chevron.up",     action: model.moveTrackUp,    disabled: !canMoveTrackUp)
                            iconBtn("chevron.down",   action: model.moveTrackDown,  disabled: !canMoveTrackDown)
                            iconBtn("pencil",         action: beginRenameTrack,     disabled: model.selectedTrackID == nil)
                            Spacer()
                            Button("Importer MIDI 2…") { model.importFormat2Midi() }
                                .buttonStyle(.borderless)
                                .font(.caption)
                                .padding(.trailing, 6)
                        }
                        .padding(.vertical, 2)
                        .background(Color(NSColor.controlBackgroundColor).opacity(0.5))
                    }
                    .frame(width: 290)

                    // Détail de la piste
                    trackDetailPanel
                }
            }
        }
    }

    private var canMoveTrackUp: Bool {
        guard let id = model.selectedTrackID,
              let index = model.variantTracks.firstIndex(where: { $0.id == id }) else { return false }
        return index > 0
    }
    private var canMoveTrackDown: Bool {
        guard let id = model.selectedTrackID,
              let index = model.variantTracks.firstIndex(where: { $0.id == id }) else { return false }
        return index < model.variantTracks.count - 1
    }

    private func beginRenameTrack() {
        guard let id = model.selectedTrackID,
              let trk = model.variantTracks.first(where: { $0.id == id }) else { return }
        renameTrackText = trk.name
        showingRenameTrack = true
    }

    @ViewBuilder
    private var trackDetailPanel: some View {
        if let trkId = model.selectedTrackID,
           let trk = model.variantTracks.first(where: { $0.id == trkId }) {
            VStack(alignment: .leading, spacing: 12) {
                Text("Piste sélectionnée")
                    .font(.headline)
                LabeledContent("Fichier source:") {
                    Text(trk.sourceFileName)
                        .foregroundColor(.secondary)
                }
                LabeledContent("Événements MIDI:") {
                    Text("\(trk.events.count)")
                        .foregroundColor(.secondary)
                }
                Divider()
                Button {
                    model.importMidiForSelectedTrack()
                } label: {
                    Label("Importer MIDI (Format 0/1)…", systemImage: "square.and.arrow.down.on.square")
                }
                .buttonStyle(.borderedProminent)
                .tint(.purple)
                Text("Le Format 1 est automatiquement fusionné en Format 0.")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding()
            .background(Color(NSColor.controlBackgroundColor).opacity(0.12))
            .cornerRadius(6)
        } else {
            VStack {
                Spacer()
                Text("Aucune piste sélectionnée.")
                    .foregroundColor(.secondary)
                    .italic()
                Spacer()
            }
            .frame(maxWidth: .infinity)
        }
    }

    // MARK: Bouton icône générique
    @ViewBuilder
    private func iconBtn(
        _ icon: String,
        action: @escaping () -> Void,
        tip: String = "",
        disabled: Bool = false
    ) -> some View {
        Button(action: action) {
            Image(systemName: icon)
                .frame(width: 20, height: 20)
        }
        .buttonStyle(.plain)
        .padding(5)
        .disabled(disabled)
        .help(tip)
    }
}

// MARK: - TypeableStepper

struct TypeableStepper: View {
    let title: String
    @Binding var value: Int
    let range: ClosedRange<Int>
    var showSpacer: Bool = false

    var body: some View {
        HStack(spacing: 8) {
            if !title.isEmpty { Text(title) }
            if showSpacer { Spacer() }
            TextField("", value: $value, format: .number)
                .textFieldStyle(.roundedBorder)
                .frame(width: 65)
                .multilineTextAlignment(.trailing)
                .onChange(of: value) { newValue in
                    if newValue < range.lowerBound { value = range.lowerBound }
                    else if newValue > range.upperBound { value = range.upperBound }
                }
            Stepper("", value: $value, in: range)
                .labelsHidden()
        }
    }
}
