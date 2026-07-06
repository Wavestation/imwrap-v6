#include "ExplorerWindow.h"

#include "IMWrapSysExDialog.h"

#include <QCheckBox>
#include <QDateTime>
#include <QComboBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QKeySequence>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include "imwrap/SmfSequence.h"
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "imwrap/IMWrapSysex.h"

namespace {

constexpr int kRoleNodeType = Qt::UserRole;
constexpr int kRoleSoundIndex = Qt::UserRole + 1;
constexpr int kRoleVariantIndex = Qt::UserRole + 2;
constexpr int kRoleTrackIndex = Qt::UserRole + 3;
constexpr int kLayoutVersion = 2;

QString SanitizeMetaText(const std::vector<uint8_t>& payload) {
    QString text = QString::fromLatin1(reinterpret_cast<const char*>(payload.data()), static_cast<int>(payload.size()));
    text.replace('\r', ' ');
    text.replace('\n', ' ');
    text.replace('\t', ' ');
    return text.trimmed();
}

QString FormatNoteName(uint8_t note) {
    static const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int octave = static_cast<int>(note / 12) - 1;
    return QString("%1%2").arg(kNames[note % 12]).arg(octave);
}

QString ControllerName(uint8_t controller) {
    switch (controller) {
    case 0: return "Bank Select MSB";
    case 1: return "Modulation";
    case 7: return "Channel Volume";
    case 10: return "Pan";
    case 11: return "Expression";
    case 32: return "Bank Select LSB";
    case 64: return "Sustain Pedal";
    case 91: return "Reverb Send";
    case 93: return "Chorus Send";
    case 98: return "NRPN LSB";
    case 99: return "NRPN MSB";
    case 100: return "RPN LSB";
    case 101: return "RPN MSB";
    case 120: return "All Sound Off";
    case 121: return "Reset All Controllers";
    case 123: return "All Notes Off";
    default:
        return QString();
    }
}

QString MidiEventTypeLabel(const imwrap::MidiEvent& event) {
    switch (event.type) {
    case imwrap::MidiEventType::Channel:
        switch (event.status & 0xF0) {
        case 0x80: return "Note Off";
        case 0x90: return (event.hasData2 && event.data2 == 0) ? "Note Off" : "Note On";
        case 0xA0: return "Poly Aftertouch";
        case 0xB0: return "Control Change";
        case 0xC0: return "Program Change";
        case 0xD0: return "Channel Pressure";
        case 0xE0: return "Pitch Bend";
        default: return "Channel";
        }
    case imwrap::MidiEventType::Meta:
        switch (event.metaType) {
        case 0x01: return "Meta Text";
        case 0x02: return "Copyright";
        case 0x03: return "Track Name";
        case 0x04: return "Instrument Name";
        case 0x05: return "Lyric";
        case 0x06: return "Marker";
        case 0x07: return "Cue Point";
        case 0x20: return "Channel Prefix";
        case 0x2F: return "End Of Track";
        case 0x51: return "Tempo";
        case 0x54: return "SMPTE Offset";
        case 0x58: return "Time Signature";
        case 0x59: return "Key Signature";
        default:
            return QString("Meta 0x%1").arg(event.metaType, 2, 16, QChar('0')).toUpper();
        }
    case imwrap::MidiEventType::SysEx:
        return "SysEx";
    case imwrap::MidiEventType::System:
        switch (event.status) {
        case 0xF1: return "MTC Quarter Frame";
        case 0xF2: return "Song Position";
        case 0xF3: return "Song Select";
        case 0xF6: return "Tune Request";
        case 0xF8: return "Timing Clock";
        case 0xFA: return "Start";
        case 0xFB: return "Continue";
        case 0xFC: return "Stop";
        case 0xFE: return "Active Sensing";
        case 0xFF: return "System Reset";
        default:
            return QString("System 0x%1").arg(event.status, 2, 16, QChar('0')).toUpper();
        }
    }

    return "Event";
}

imwrap::TargetProfile ProfileFromVariantKind(imwrap::VariantKind kind) {
    switch (kind) {
    case imwrap::VariantKind::Rol:
        return imwrap::TargetProfile::Mt32;
    case imwrap::VariantKind::Adl:
        return imwrap::TargetProfile::Adlib;
    case imwrap::VariantKind::Gmd:
    case imwrap::VariantKind::None:
    default:
        return imwrap::TargetProfile::GeneralMidi;
    }
}

imwrap::VariantKind VariantKindFromCombo(const QComboBox* combo) {
    if (!combo || combo->currentIndex() < 0) {
        return imwrap::VariantKind::Gmd;
    }
    return static_cast<imwrap::VariantKind>(combo->currentData().toInt());
}

int FindVariantComboIndex(const QComboBox* combo, imwrap::VariantKind kind) {
    if (!combo) {
        return -1;
    }

    for (int index = 0; index < combo->count(); ++index) {
        if (combo->itemData(index).toInt() == static_cast<int>(kind)) {
            return index;
        }
    }
    return -1;
}

QIcon TreeIconForVariantKind(imwrap::VariantKind kind) {
    switch (kind) {
    case imwrap::VariantKind::Gmd:
        return QIcon(":/imwrap/generalmidi.png");
    case imwrap::VariantKind::Rol:
        return QIcon(":/imwrap/mt32.png");
    case imwrap::VariantKind::Adl:
        return QIcon(":/imwrap/adlib.png");
    case imwrap::VariantKind::None:
    default:
        return QIcon(":/imwrap/default-event.png");
    }
}

QIcon EventIconForControlEvent(const imwrap::IMWrapControlEvent& event) {
    switch (event.type) {
    case imwrap::IMWrapSysexType::AllocatePart:
        return QIcon(":/imwrap/syx-allocate-part.png");
    case imwrap::IMWrapSysexType::ShutdownPart:
        return QIcon(":/imwrap/syx-shutdown.png");
    case imwrap::IMWrapSysexType::StartSong:
        return QIcon(":/imwrap/syx-startsong.png");
    case imwrap::IMWrapSysexType::AdlibPartInstrument:
    case imwrap::IMWrapSysexType::AdlibGlobalInstrument:
        return QIcon(":/imwrap/syx-adlib-instr.png");
    case imwrap::IMWrapSysexType::ParameterAdjust:
        return QIcon(":/imwrap/syx-parameter.png");
    case imwrap::IMWrapSysexType::HookJump:
        return QIcon(":/imwrap/syx-hookjump.png");
    case imwrap::IMWrapSysexType::HookGlobalTranspose:
    case imwrap::IMWrapSysexType::HookSetTranspose:
        return QIcon(":/imwrap/syx-hook-transpose.png");
    case imwrap::IMWrapSysexType::HookPartOnOff:
        return QIcon(":/imwrap/syx-hook-part-onoff.png");
    case imwrap::IMWrapSysexType::HookSetVolume:
        return QIcon(":/imwrap/syx-hook-volume.png");
    case imwrap::IMWrapSysexType::HookSetProgram:
        return QIcon(":/imwrap/syx-hook-program.png");
    case imwrap::IMWrapSysexType::Marker:
        return QIcon(":/imwrap/syx-marker.png");
    case imwrap::IMWrapSysexType::SetLoop:
    case imwrap::IMWrapSysexType::ClearLoop:
        return QIcon(":/imwrap/syx-loop.png");
    case imwrap::IMWrapSysexType::SetInstrument:
        return QIcon(":/imwrap/syx-roland.png");
    case imwrap::IMWrapSysexType::Unknown:
    default:
        return QIcon(":/imwrap/syx-unknown.png");
    }
}

QIcon EventIconForMidiEvent(const imwrap::MidiEvent& event, imwrap::IMWrapSysexDialect dialect) {
    switch (event.type) {
    case imwrap::MidiEventType::Channel:
        return QIcon(":/imwrap/default-event.png");
    case imwrap::MidiEventType::Meta:
        return QIcon(":/imwrap/filetypes.png");
    case imwrap::MidiEventType::System:
        return QIcon(":/imwrap/package_settings.png");
    case imwrap::MidiEventType::SysEx: {
        imwrap::IMWrapControlEvent controlEvent;
        if (imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &controlEvent, dialect, nullptr)) {
            return EventIconForControlEvent(controlEvent);
        }
        return QIcon(":/imwrap/syx-unknown.png");
    }
    }

    return QIcon(":/imwrap/default-event.png");
}

bool IsEndOfTrackEvent(const imwrap::MidiEvent& event) {
    return event.type == imwrap::MidiEventType::Meta && event.metaType == 0x2F;
}

} // namespace

ExplorerWindow::WinMMSink::~WinMMSink() {
    closeDevice();
}

bool ExplorerWindow::WinMMSink::openDevice(UINT deviceId) {
    closeDevice();
    return midiOutOpen(&hMidiOut_, deviceId, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
}

void ExplorerWindow::WinMMSink::closeDevice() {
    if (!hMidiOut_) {
        return;
    }
    midiOutReset(hMidiOut_);
    midiOutClose(hMidiOut_);
    hMidiOut_ = nullptr;
}

bool ExplorerWindow::WinMMSink::isAvailable() const {
    return hMidiOut_ != nullptr;
}

void ExplorerWindow::WinMMSink::onMidiMessage(uint16_t, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!hMidiOut_) {
        return;
    }

    DWORD message = status | (data1 << 8);
    if (hasData2) {
        message |= (data2 << 16);
    }
    midiOutShortMsg(hMidiOut_, message);
}

void ExplorerWindow::WinMMSink::onSysEx(uint16_t, imwrap::ByteView message) {
    if (!hMidiOut_ || message.empty()) {
        return;
    }

    std::vector<char> buffer;
    if (static_cast<uint8_t>(message.data()[0]) != 0xF0) {
        buffer.push_back(static_cast<char>(0xF0));
    }
    buffer.insert(buffer.end(), message.data(), message.data() + message.size());
    if (static_cast<uint8_t>(buffer.back()) != 0xF7) {
        buffer.push_back(static_cast<char>(0xF7));
    }

    MIDIHDR header;
    std::memset(&header, 0, sizeof(header));
    header.lpData = buffer.data();
    header.dwBufferLength = static_cast<DWORD>(buffer.size());

    if (midiOutPrepareHeader(hMidiOut_, &header, sizeof(header)) == MMSYSERR_NOERROR) {
        if (midiOutLongMsg(hMidiOut_, &header, sizeof(header)) == MMSYSERR_NOERROR) {
            int timeout = 100;
            while (!(header.dwFlags & MHDR_DONE) && timeout-- > 0) {
                Sleep(1);
            }
        }
        midiOutUnprepareHeader(hMidiOut_, &header, sizeof(header));
    }
}

ExplorerWindow::ExplorerWindow(QWidget* parent)
    : QMainWindow(parent) {
    engine_.setMidiSink(&midiSink_);
    transportTimer_ = new QTimer(this);
    transportTimer_->setInterval(16);
    connect(transportTimer_, &QTimer::timeout, this, &ExplorerWindow::onTimer);

    setupUi();
    refreshDevices();
    loadSettings();
    updateWindowTitle();
    updateUiState();
}

ExplorerWindow::~ExplorerWindow() {
    disablePreviewBackend();
}

bool ExplorerWindow::promptSaveIfDirty() {
    if (!dirty_) return true;

    QMessageBox::StandardButton res = QMessageBox::warning(this, "Unsaved Changes",
        "You have unsaved changes. Do you want to save them now?",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (res == QMessageBox::Cancel) {
        return false;
    }
    if (res == QMessageBox::Yes) {
        saveDocument();
        if (dirty_) return false;
    }
    return true;
}

void ExplorerWindow::closeEvent(QCloseEvent* event) {
    if (promptSaveIfDirty()) {
        saveSettings();
        QMainWindow::closeEvent(event);
    } else {
        event->ignore();
    }
}

void ExplorerWindow::setupUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QVBoxLayout(central);

    auto* fileBox = new QGroupBox("IMS Document");
    auto* fileLayout = new QHBoxLayout(fileBox);
    fileLayout->addWidget(new QLabel("File:"));
    filePathEdit_ = new QLineEdit();
    filePathEdit_->setReadOnly(true);
    fileLayout->addWidget(filePathEdit_, 1);

    auto* openBtn = new QPushButton("Open...");
    connect(openBtn, &QPushButton::clicked, this, &ExplorerWindow::openDocument);
    fileLayout->addWidget(openBtn);

    auto* saveBtn = new QPushButton("Save");
    connect(saveBtn, &QPushButton::clicked, this, &ExplorerWindow::saveDocument);
    fileLayout->addWidget(saveBtn);

    auto* saveAsBtn = new QPushButton("Save As...");
    connect(saveAsBtn, &QPushButton::clicked, this, &ExplorerWindow::saveDocumentAs);
    fileLayout->addWidget(saveAsBtn);

    rootLayout->addWidget(fileBox);

    mainSplitter_ = new QSplitter(Qt::Horizontal);
    rootLayout->addWidget(mainSplitter_, 1);

    auto* treePane = new QWidget();
    auto* treeLayout = new QVBoxLayout(treePane);
    treeLayout->addWidget(new QLabel("Sounds / Variants / Tracks"));
    contentTree_ = new QTreeWidget();
    contentTree_->setHeaderHidden(true);
    connect(contentTree_, &QTreeWidget::itemSelectionChanged, this, &ExplorerWindow::onTreeSelectionChanged);
    treeLayout->addWidget(contentTree_, 1);
    mainSplitter_->addWidget(treePane);

    auto* eventsPane = new QWidget();
    auto* eventsLayout = new QVBoxLayout(eventsPane);

    eventsLayout->addWidget(new QLabel("Track Events"));
    filterNonImwrapCheck_ = new QCheckBox("Hide non-iMWrap events");
    filterNonImwrapCheck_->setChecked(true);
    connect(filterNonImwrapCheck_, &QCheckBox::toggled, this, [this](bool) {
        rebuildEventTable();
        rebuildDetailPane();
        updateUiState();
    });
    eventsLayout->addWidget(filterNonImwrapCheck_);

    eventsTable_ = new QTableWidget(0, 5);
    eventsTable_->setHorizontalHeaderLabels({"#", "Delta", "M:B:T", "Type", "Description"});
    eventsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    eventsTable_->setIconSize(QSize(16, 16));
    eventsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    eventsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventsTable_->setColumnHidden(0, true);
    connect(eventsTable_, &QTableWidget::itemSelectionChanged, this, &ExplorerWindow::onEventSelectionChanged);
    connect(eventsTable_, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem*) {
        editSelectedEvent();
    });
    eventsLayout->addWidget(eventsTable_, 1);

    auto* actionRow = new QHBoxLayout();
    addEventBtn_ = new QPushButton("Add iMWrap Command...");
    connect(addEventBtn_, &QPushButton::clicked, this, &ExplorerWindow::addImwrapCommand);
    actionRow->addWidget(addEventBtn_);
    editEventBtn_ = new QPushButton("Edit Selected Event...");
    connect(editEventBtn_, &QPushButton::clicked, this, &ExplorerWindow::editSelectedEvent);
    actionRow->addWidget(editEventBtn_);
    importTracksBtn_ = new QPushButton("Import MIDI Tracks...");
    connect(importTracksBtn_, &QPushButton::clicked, this, &ExplorerWindow::importMidiTracks);
    actionRow->addWidget(importTracksBtn_);
    exportTrackMidiBtn_ = new QPushButton("Export MIDI...");
    connect(exportTrackMidiBtn_, &QPushButton::clicked, this, &ExplorerWindow::exportSelectedTrackMidi);
    actionRow->addWidget(exportTrackMidiBtn_);
    deleteTrackBtn_ = new QPushButton("Delete Track");
    connect(deleteTrackBtn_, &QPushButton::clicked, this, &ExplorerWindow::deleteSelectedTrack);
    actionRow->addWidget(deleteTrackBtn_);
    moveTrackUpBtn_ = new QPushButton("Move Track Up");
    connect(moveTrackUpBtn_, &QPushButton::clicked, this, &ExplorerWindow::moveTrackUp);
    actionRow->addWidget(moveTrackUpBtn_);
    moveTrackDownBtn_ = new QPushButton("Move Track Down");
    connect(moveTrackDownBtn_, &QPushButton::clicked, this, &ExplorerWindow::moveTrackDown);
    actionRow->addWidget(moveTrackDownBtn_);
    actionRow->addStretch(1);
    eventsLayout->addLayout(actionRow);
    mainSplitter_->addWidget(eventsPane);

    bottomSplitter_ = new QSplitter(Qt::Vertical);
    bottomSplitter_->setChildrenCollapsible(false);

    auto* detailBox = new QGroupBox("Event Details");
    auto* detailLayout = new QVBoxLayout(detailBox);
    detailsEdit_ = new QTextEdit();
    detailsEdit_->setReadOnly(true);
    detailsEdit_->setStyleSheet("font-family: monospace;");
    detailLayout->addWidget(detailsEdit_);
    bottomSplitter_->addWidget(detailBox);

    auto* selectionBox = new QGroupBox("Selection Properties");
    auto* selectionLayout = new QVBoxLayout(selectionBox);
    selectionSummaryLabel_ = new QLabel("Select a sound, variant, or track to edit packer properties.");
    selectionSummaryLabel_->setWordWrap(true);
    selectionLayout->addWidget(selectionSummaryLabel_);

    selectionPropsWidget_ = new QWidget();
    auto* selectionForm = new QFormLayout(selectionPropsWidget_);
    soundNameEdit_ = new QLineEdit();
    selectionForm->addRow("Sound Name:", soundNameEdit_);

    soundIdSpin_ = new QSpinBox();
    soundIdSpin_->setRange(0, 65535);
    selectionForm->addRow("Sound ID:", soundIdSpin_);

    variantKindCombo_ = new QComboBox();
    variantKindCombo_->addItem("General MIDI (GMD)", static_cast<int>(imwrap::VariantKind::Gmd));
    variantKindCombo_->addItem("Roland MT-32 (ROL)", static_cast<int>(imwrap::VariantKind::Rol));
    variantKindCombo_->addItem("AdLib (ADL)", static_cast<int>(imwrap::VariantKind::Adl));
    connect(variantKindCombo_, &QComboBox::currentIndexChanged, this, &ExplorerWindow::onVariantEditorKindChanged);
    selectionForm->addRow("Variant:", variantKindCombo_);

    includeVariantCheck_ = new QCheckBox("Include this variant");
    selectionForm->addRow("", includeVariantCheck_);

    includeMdhdCheck_ = new QCheckBox("Include MDhd");
    selectionForm->addRow("", includeMdhdCheck_);

    prioritySpin_ = new QSpinBox();
    prioritySpin_->setRange(0, 255);
    selectionForm->addRow("Priority:", prioritySpin_);

    volumeSpin_ = new QSpinBox();
    volumeSpin_->setRange(0, 127);
    selectionForm->addRow("Volume:", volumeSpin_);

    panSpin_ = new QSpinBox();
    panSpin_->setRange(-128, 127);
    selectionForm->addRow("Pan:", panSpin_);

    transposeSpin_ = new QSpinBox();
    transposeSpin_->setRange(-128, 127);
    selectionForm->addRow("Transpose:", transposeSpin_);

    detuneSpin_ = new QSpinBox();
    detuneSpin_->setRange(-128, 127);
    selectionForm->addRow("Detune:", detuneSpin_);

    speedSpin_ = new QSpinBox();
    speedSpin_->setRange(0, 255);
    selectionForm->addRow("Speed:", speedSpin_);

    selectionLayout->addWidget(selectionPropsWidget_);

    applySelectionBtn_ = new QPushButton("Apply Sound / Variant Changes");
    connect(applySelectionBtn_, &QPushButton::clicked, this, &ExplorerWindow::applySelectionProperties);
    selectionLayout->addWidget(applySelectionBtn_);
    bottomSplitter_->addWidget(selectionBox);

    auto* playbackBox = new QGroupBox("Playback");
    auto* playbackLayout = new QFormLayout(playbackBox);
    deviceCombo_ = new QComboBox();
    playbackLayout->addRow("MIDI Out:", deviceCombo_);

    profileCombo_ = new QComboBox();
    profileCombo_->addItem("General MIDI", static_cast<int>(imwrap::TargetProfile::GeneralMidi));
    profileCombo_->addItem("Roland MT-32", static_cast<int>(imwrap::TargetProfile::Mt32));
    profileCombo_->addItem("AdLib", static_cast<int>(imwrap::TargetProfile::Adlib));
    playbackLayout->addRow("Fallback Profile:", profileCombo_);

    snmModeCheck_ = new QCheckBox("SNM mode");
    connect(snmModeCheck_, &QCheckBox::toggled, this, [this](bool) {
        engine_.setCompatibilityProfile(currentCompatibilityProfile());
        rebuildEventTable();
        rebuildDetailPane();
        updateUiState();
    });
    playbackPositionLabel_ = new QLabel();
    auto* snmLayout = new QHBoxLayout();
    snmLayout->addWidget(snmModeCheck_);
    snmLayout->addWidget(playbackPositionLabel_);
    snmLayout->addStretch();
    playbackLayout->addRow("", snmLayout);

    auto* refreshBtn = new QPushButton("Refresh Devices");
    connect(refreshBtn, &QPushButton::clicked, this, &ExplorerWindow::refreshDevices);
    playbackLayout->addRow("", refreshBtn);

    previewBtn_ = new QPushButton("Enable Preview");
    connect(previewBtn_, &QPushButton::clicked, this, &ExplorerWindow::togglePreview);
    playbackLayout->addRow("", previewBtn_);

    playBtn_ = new QPushButton("Play Selected Sound");
    connect(playBtn_, &QPushButton::clicked, this, &ExplorerWindow::playSelectedSound);
    playbackLayout->addRow("", playBtn_);

    stopBtn_ = new QPushButton("Stop Selected Sound");
    connect(stopBtn_, &QPushButton::clicked, this, &ExplorerWindow::stopSelectedSound);
    playbackLayout->addRow("", stopBtn_);

    stopAllBtn_ = new QPushButton("Stop All");
    connect(stopAllBtn_, &QPushButton::clicked, this, &ExplorerWindow::stopAllSounds);
    playbackLayout->addRow("", stopAllBtn_);

    bottomSplitter_->addWidget(playbackBox);

    auto* hookBox = new QGroupBox("Manual Hooks");
    auto* hookLayout = new QFormLayout(hookBox);
    hookControlsWidget_ = new QWidget();
    auto* hookControlsLayout = new QHBoxLayout(hookControlsWidget_);
    hookControlsLayout->setContentsMargins(0, 0, 0, 0);

    hookClassCombo_ = new QComboBox();
    hookClassCombo_->addItems({
        "0 - Jump",
        "1 - Global Transpose",
        "2 - Part On/Off",
        "3 - Part Volume",
        "4 - Part Program",
        "5 - Part Transpose"
    });
    hookControlsLayout->addWidget(hookClassCombo_);

    hookValueSpin_ = new QSpinBox();
    hookValueSpin_->setRange(0, 255);
    hookControlsLayout->addWidget(hookValueSpin_);

    hookChannelSpin_ = new QSpinBox();
    hookChannelSpin_->setRange(0, 16);
    hookControlsLayout->addWidget(hookChannelSpin_);

    applyHookBtn_ = new QPushButton("Apply Hook");
    connect(applyHookBtn_, &QPushButton::clicked, this, &ExplorerWindow::applyHook);
    hookControlsLayout->addWidget(applyHookBtn_);

    hookLayout->addRow("Hook (C/V/Ch):", hookControlsWidget_);
    bottomSplitter_->addWidget(hookBox);

    bottomSplitter_->setStretchFactor(0, 1);
    bottomSplitter_->setStretchFactor(1, 2);
    bottomSplitter_->setStretchFactor(2, 1);
    bottomSplitter_->setStretchFactor(3, 1);

    mainSplitter_->addWidget(bottomSplitter_);
    mainSplitter_->setStretchFactor(0, 2);
    mainSplitter_->setStretchFactor(1, 3);
    mainSplitter_->setStretchFactor(2, 2);

    statusLabel_ = new QLabel("Ready.");
    rootLayout->addWidget(statusLabel_);

    auto* fileMenu = menuBar()->addMenu("&File");
    
    displayMbtAction_ = fileMenu->addAction("Display Position as M:B:T");
    displayMbtAction_->setCheckable(true);
    connect(displayMbtAction_, &QAction::toggled, this, &ExplorerWindow::onDisplayFormatChanged);

    fileMenu->addSeparator();
    fileMenu->addAction("Add New Song", this, &ExplorerWindow::addNewSong);
    fileMenu->addSeparator();

    fileMenu->addAction("&Open...", this, &ExplorerWindow::openDocument, QKeySequence::Open);
    fileMenu->addAction("&Save", this, &ExplorerWindow::saveDocument, QKeySequence::Save);
    fileMenu->addAction("Save &As...", this, &ExplorerWindow::saveDocumentAs, QKeySequence::SaveAs);
}

void ExplorerWindow::openDocument() {
    if (!promptSaveIfDirty()) return;

    const QString path = QFileDialog::getOpenFileName(this, "Open IMS", QString(), "iMWrap IMS (*.ims *.data)");
    if (path.isEmpty()) {
        return;
    }

    openDocumentPath(path, true);
}

void ExplorerWindow::saveDocument() {
    if (currentFilePath_.isEmpty()) {
        saveDocumentAs();
        return;
    }

    std::string error;
    if (!document_.saveToFile(currentFilePath_.toStdString(), &error)) {
        QMessageBox::critical(this, "Error", QString("Failed to save IMS file:\n%1").arg(QString::fromStdString(error)));
        return;
    }
    markDirty(false);

    dirty_ = false;
    updateWindowTitle();
    statusLabel_->setText(QString("Saved %1").arg(currentFilePath_));
}

void ExplorerWindow::saveDocumentAs() {
    const QString path = QFileDialog::getSaveFileName(this, "Save IMS As", currentFilePath_.isEmpty() ? "explorer.ims" : currentFilePath_, "iMWrap IMS (*.ims)");
    if (path.isEmpty()) {
        return;
    }

    std::string error;
    if (!document_.saveToFile(path.toStdString(), &error)) {
        QMessageBox::critical(this, "Error", QString("Failed to save IMS file:\n%1").arg(QString::fromStdString(error)));
        return;
    }
    markDirty(false);

    currentFilePath_ = path;
    filePathEdit_->setText(path);
    dirty_ = false;
    updateWindowTitle();
    statusLabel_->setText(QString("Saved %1").arg(path));
}

void ExplorerWindow::refreshDevices() {
    deviceCombo_->clear();
    const UINT numDevices = midiOutGetNumDevs();
    for (UINT index = 0; index < numDevices; ++index) {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            deviceCombo_->addItem(caps.szPname, index);
        }
    }
}

bool ExplorerWindow::ensurePreviewBackend(imwrap::TargetProfile profile, std::string* error) {
    if (profile == imwrap::TargetProfile::Adlib) {
        midiSink_.closeDevice();
        if (!adlibSink_.isAvailable() && !adlibSink_.start(error)) {
            return false;
        }
        previewBackend_ = PreviewBackend::Adlib;
        engine_.setMidiSink(&adlibSink_);
        if (error) {
            error->clear();
        }
        return true;
    }

    adlibSink_.stop();
    if (deviceCombo_->currentIndex() < 0) {
        if (error) {
            *error = "No MIDI output device is selected.";
        }
        return false;
    }

    const UINT deviceId = deviceCombo_->currentData().toUInt();
    if (!midiSink_.openDevice(deviceId)) {
        if (error) {
            *error = "Unable to open the selected MIDI output.";
        }
        return false;
    }

    previewBackend_ = PreviewBackend::WinMM;
    engine_.setMidiSink(&midiSink_);
    if (error) {
        error->clear();
    }
    return true;
}

void ExplorerWindow::disablePreviewBackend() {
    transportTimer_->stop();
    adlibSink_.stop();
    midiSink_.closeDevice();
    previewBackend_ = PreviewBackend::None;
    engine_.setMidiSink(nullptr);
}

void ExplorerWindow::togglePreview() {
    if (previewEnabled_) {
        disablePreviewBackend();
        engine_.resetMt32Initialization();
        previewEnabled_ = false;
        previewBtn_->setText("Enable Preview");
        statusLabel_->setText("Preview disabled.");
        return;
    }

    std::string error;
    if (!ensurePreviewBackend(effectiveProfileForSelection(), &error)) {
        QMessageBox::warning(this, "Preview", QString::fromStdString(error));
        return;
    }

    engine_.resetMt32Initialization();
    previewEnabled_ = true;
    previewBtn_->setText("Disable Preview");
    statusLabel_->setText(effectiveProfileForSelection() == imwrap::TargetProfile::Adlib
                              ? "Preview enabled with embedded AdLib audio."
                              : "Preview enabled.");
}

void ExplorerWindow::playSelectedSound() {
    const SelectionIndices selection = currentSelection();
    const uint16_t soundId = selectedSoundId();
    if (soundId == 0 && !selectedSound()) {
        return;
    }
    if (!previewEnabled_) {
        togglePreview();
        if (!previewEnabled_) {
            return;
        }
    }

    std::string error;
    if (!rebuildPreviewBank(&error)) {
        QMessageBox::critical(this, "Preview Bank", QString::fromStdString(error));
        return;
    }

    engine_.setTargetProfile(effectiveProfileForSelection());
    engine_.setNativeMt32Output(engine_.targetProfile() == imwrap::TargetProfile::Mt32);
    engine_.setCompatibilityProfile(currentCompatibilityProfile());
    if (!ensurePreviewBackend(engine_.targetProfile(), &error)) {
        QMessageBox::warning(this, "Playback", QString::fromStdString(error));
        disablePreviewBackend();
        engine_.resetMt32Initialization();
        previewEnabled_ = false;
        previewBtn_->setText("Enable Preview");
        statusLabel_->setText("Preview disabled.");
        return;
    }

    engine_.stopAllSounds();
    if (engine_.targetProfile() == imwrap::TargetProfile::Mt32) {
        engine_.resetMt32Initialization();
        engine_.initMt32();
    }
    if (!engine_.startSound(soundId)) {
        QMessageBox::warning(this, "Playback", "Unable to start the selected sound.");
        return;
    }

    if (selection.trackIndex >= 0) {
        const int16_t jumpArgs[5] = {
            static_cast<int16_t>(imwrap::Command::PlayerJump),
            static_cast<int16_t>(soundId),
            static_cast<int16_t>(selection.trackIndex),
            1,
            0
        };
        if (engine_.doCommand(5, jumpArgs) != 0) {
            QMessageBox::warning(this, "Playback", "Unable to jump to the selected track.");
            engine_.stopSound(soundId);
            return;
        }
    }

    lastTime_ = QDateTime::currentMSecsSinceEpoch();
    microAccumulator_ = 0.0;
    transportTimer_->start();
    if (selection.trackIndex >= 0) {
        statusLabel_->setText(QString("Playing sound %1, track %2").arg(soundId).arg(selection.trackIndex));
    } else {
        statusLabel_->setText(QString("Playing sound %1").arg(soundId));
    }
}

void ExplorerWindow::stopSelectedSound() {
    const uint16_t soundId = selectedSoundId();
    if (soundId == 0 && !selectedSound()) {
        return;
    }
    engine_.stopSound(soundId);
    updateUiState();
    statusLabel_->setText(QString("Stopped sound %1").arg(soundId));
}

void ExplorerWindow::stopAllSounds() {
    engine_.stopAllSounds();
    transportTimer_->stop();
    updateUiState();
    statusLabel_->setText("Stopped all sounds.");
}

void ExplorerWindow::importMidiTracks() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    if (!variant) {
        QMessageBox::information(this, "Import MIDI", "Select a variant or one of its tracks before importing MIDI.");
        return;
    }

    const QStringList paths = QFileDialog::getOpenFileNames(this, "Import MIDI", QString(), "MIDI files (*.mid *.midi)");
    if (paths.isEmpty()) {
        return;
    }

    const SelectionIndices selection = currentSelection();
    for (const QString& path : paths) {
        std::string error;
        if (!imwrap::gui::ImportMidiFileToVariant(path.toStdString(), variant, &error)) {
            QMessageBox::warning(this, "Import MIDI", QString::fromStdString(error));
            return;
        }
    }

    markDirty();
    rebuildTree();
    selectNode(selection.soundIndex, selection.variantIndex, static_cast<int>(variant->tracks.size() - 1));
    statusLabel_->setText(QString("Imported %1 MIDI file(s).").arg(paths.size()));
}

void ExplorerWindow::deleteSelectedTrack() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    const SelectionIndices selection = currentSelection();
    if (!variant || selection.trackIndex < 0 || selection.trackIndex >= static_cast<int>(variant->tracks.size())) {
        return;
    }

    variant->tracks.erase(variant->tracks.begin() + selection.trackIndex);
    markDirty();
    rebuildTree();
    if (!variant->tracks.empty()) {
        const int nextTrackIndex = (selection.trackIndex < static_cast<int>(variant->tracks.size()))
            ? selection.trackIndex
            : static_cast<int>(variant->tracks.size() - 1);
        selectNode(selection.soundIndex, selection.variantIndex,
                   nextTrackIndex);
    } else {
        selectNode(selection.soundIndex, selection.variantIndex);
    }
    statusLabel_->setText("Deleted track.");
}

void ExplorerWindow::moveTrackUp() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    const SelectionIndices selection = currentSelection();
    if (!variant || selection.trackIndex <= 0 || selection.trackIndex >= static_cast<int>(variant->tracks.size())) {
        return;
    }

    std::swap(variant->tracks[selection.trackIndex], variant->tracks[selection.trackIndex - 1]);
    markDirty();
    rebuildTree();
    selectNode(selection.soundIndex, selection.variantIndex, selection.trackIndex - 1);
    statusLabel_->setText("Moved track up.");
}

void ExplorerWindow::moveTrackDown() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    const SelectionIndices selection = currentSelection();
    if (!variant || selection.trackIndex < 0 || selection.trackIndex >= static_cast<int>(variant->tracks.size() - 1)) {
        return;
    }

    std::swap(variant->tracks[selection.trackIndex], variant->tracks[selection.trackIndex + 1]);
    markDirty();
    rebuildTree();
    selectNode(selection.soundIndex, selection.variantIndex, selection.trackIndex + 1);
    statusLabel_->setText("Moved track down.");
}

void ExplorerWindow::onTreeSelectionChanged() {
    rebuildEventTable();
    rebuildDetailPane();
    rebuildSelectionProperties();
    updateUiState();
}

void ExplorerWindow::onEventSelectionChanged() {
    rebuildDetailPane();
    updateUiState();
}

void ExplorerWindow::onVariantEditorKindChanged() {
    if (updatingSelectionProps_) {
        return;
    }
    rebuildSelectionProperties();
    updateUiState();
}

void ExplorerWindow::applySelectionProperties() {
    const SelectionIndices selection = currentSelection();
    imwrap::gui::ProjectSound* sound = selectedSound();
    if (!sound) {
        return;
    }

    const uint16_t newSoundId = static_cast<uint16_t>(soundIdSpin_->value());
    for (int soundIndex = 0; soundIndex < static_cast<int>(document_.sounds().size()); ++soundIndex) {
        if (soundIndex == selection.soundIndex) {
            continue;
        }
        if (document_.sounds()[static_cast<std::size_t>(soundIndex)].id == newSoundId) {
            QMessageBox::warning(this, "Invalid Sound ID",
                                 QString("Sound ID %1 is already used by another sound.").arg(newSoundId));
            return;
        }
    }

    sound->name = soundNameEdit_->text().trimmed().toStdString();
    sound->id = newSoundId;

    imwrap::VariantKind editorKind = VariantKindFromCombo(variantKindCombo_);
    if (selection.variantIndex >= 0 && selection.variantIndex < static_cast<int>(sound->variants.size())) {
        editorKind = sound->variants[static_cast<std::size_t>(selection.variantIndex)].kind;
    }

    imwrap::gui::ProjectVariant* variant = document_.findVariant(sound, editorKind);
    bool createdVariant = false;
    if (!variant && includeVariantCheck_->isChecked()) {
        variant = document_.ensureVariant(sound, editorKind);
        createdVariant = (variant != nullptr);
    }

    if (variant) {
        variant->includeVariant = includeVariantCheck_->isChecked();
        variant->includeMdhd = includeMdhdCheck_->isChecked();
        variant->mdhd.present = variant->includeMdhd;
        variant->mdhd.priority = static_cast<uint8_t>(prioritySpin_->value());
        variant->mdhd.volume = static_cast<uint8_t>(volumeSpin_->value());
        variant->mdhd.pan = static_cast<int8_t>(panSpin_->value());
        variant->mdhd.transpose = static_cast<int8_t>(transposeSpin_->value());
        variant->mdhd.detune = static_cast<int8_t>(detuneSpin_->value());
        variant->mdhd.speed = static_cast<uint8_t>(speedSpin_->value());
    }

    int targetVariantIndex = -1;
    int targetTrackIndex = -1;
    if (selection.variantIndex >= 0) {
        targetVariantIndex = selection.variantIndex;
        targetTrackIndex = selection.trackIndex;
    } else if (createdVariant) {
        for (int variantIndex = 0; variantIndex < static_cast<int>(sound->variants.size()); ++variantIndex) {
            if (sound->variants[static_cast<std::size_t>(variantIndex)].kind == editorKind) {
                targetVariantIndex = variantIndex;
                break;
            }
        }
    }

    markDirty();
    rebuildTree();
    selectNode(selection.soundIndex, targetVariantIndex, targetTrackIndex);
    statusLabel_->setText("Updated sound / variant properties.");
}

void ExplorerWindow::applyHook() {
    imwrap::gui::ProjectSound* sound = selectedSound();
    if (!sound) {
        return;
    }
    if (!engine_.isSoundActive(sound->id)) {
        QMessageBox::information(this, "Apply Hook", "Start the selected sound before applying manual hooks.");
        return;
    }

    const int16_t args[5] = {
        static_cast<int16_t>(imwrap::Command::PlayerSetHook),
        static_cast<int16_t>(sound->id),
        static_cast<int16_t>(hookClassCombo_->currentIndex()),
        static_cast<int16_t>(hookValueSpin_->value()),
        static_cast<int16_t>(hookChannelSpin_->value())
    };

    if (engine_.doCommand(5, args) != 0) {
        QMessageBox::warning(this, "Apply Hook", "The hook command could not be applied to the selected sound.");
        return;
    }

    statusLabel_->setText(
        QString("Applied hook %1 / %2 / %3 to sound %4.")
            .arg(hookClassCombo_->currentIndex())
            .arg(hookValueSpin_->value())
            .arg(hookChannelSpin_->value())
            .arg(sound->id));
}

void ExplorerWindow::addImwrapCommand() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    imwrap::gui::ProjectTrack* track = selectedTrack();
    if (!variant || !track) {
        return;
    }

    IMWrapSysExDialog dialog(this);
    dialog.setWindowTitle("Add iMWrap Command");
    dialog.setPositionFieldsVisible(true);

    const std::vector<TimeSignatureSegment> segments = buildTimeSignatureSegments(*variant);
    MbtPosition defaultPosition;
    const int modelIndex = selectedEventModelIndex();
    if (modelIndex >= 0 && modelIndex < static_cast<int>(track->events.size())) {
        defaultPosition = tickToMbt(segments, track->events[static_cast<std::size_t>(modelIndex)].tick);
    } else {
        uint32_t lastTick = 0;
        for (const imwrap::MidiEvent& event : track->events) {
            if (!IsEndOfTrackEvent(event)) {
                lastTick = (std::max)(lastTick, event.tick);
            }
        }
        defaultPosition = tickToMbt(segments, lastTick);
    }
    dialog.setPositionMbt(defaultPosition.measure, defaultPosition.beat, defaultPosition.tick);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    uint32_t insertTick = 0;
    const MbtPosition requestedPosition = {
        dialog.positionMeasure(),
        dialog.positionBeat(),
        dialog.positionTick()
    };
    if (!mbtToTick(segments, requestedPosition, &insertTick)) {
        QMessageBox::warning(this, "Invalid Position",
                             QString("The requested M:B:T position %1:%2:%3 is not valid for the current time-signature map.")
                                 .arg(requestedPosition.measure)
                                 .arg(requestedPosition.beat)
                                 .arg(requestedPosition.tick));
        return;
    }

    imwrap::MidiEvent midiEvent;
    midiEvent.type = imwrap::MidiEventType::SysEx;
    midiEvent.status = 0xF0;
    midiEvent.tick = insertTick;
    midiEvent.payload = encodeSmfPayload(dialog.event());

    const int insertedModelIndex = insertEventIntoTrack(track, std::move(midiEvent));
    markDirty();
    rebuildEventTable();
    selectVisibleEventByModelIndex(insertedModelIndex);
    statusLabel_->setText(
        QString("Added iMWrap command at %1.")
            .arg(formatMbtPosition(segments, insertTick)));
}

void ExplorerWindow::editSelectedEvent() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    imwrap::gui::ProjectTrack* track = selectedTrack();
    if (!variant || !track) {
        return;
    }

    const int modelIndex = selectedEventModelIndex();
    if (modelIndex < 0 || modelIndex >= static_cast<int>(track->events.size())) {
        return;
    }

    imwrap::MidiEvent& midiEvent = track->events[static_cast<std::size_t>(modelIndex)];
    if (midiEvent.type != imwrap::MidiEventType::SysEx) {
        QMessageBox::information(this, "Edit Event", "The selected event is not a SysEx event.");
        return;
    }

    imwrap::IMWrapControlEvent controlEvent;
    std::string error;
    if (!imwrap::DecodeIMWrapSysex(imwrap::ByteView(midiEvent.payload.data(), midiEvent.payload.size()), &controlEvent,
                                   currentSysexDialect(), &error)) {
        QMessageBox::information(this, "Edit Event", QString("The selected SysEx is not a recognized iMWrap message.\n\n%1").arg(QString::fromStdString(error)));
        return;
    }

    IMWrapSysExDialog dialog(this);
    dialog.setPositionFieldsVisible(true);
    dialog.setEvent(controlEvent);
    const std::vector<TimeSignatureSegment> segments = buildTimeSignatureSegments(*variant);
    const MbtPosition currentPosition = tickToMbt(segments, midiEvent.tick);
    dialog.setPositionMbt(currentPosition.measure, currentPosition.beat, currentPosition.tick);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    uint32_t updatedTick = 0;
    const MbtPosition requestedPosition = {
        dialog.positionMeasure(),
        dialog.positionBeat(),
        dialog.positionTick()
    };
    if (!mbtToTick(segments, requestedPosition, &updatedTick)) {
        QMessageBox::warning(this, "Invalid Position",
                             QString("The requested M:B:T position %1:%2:%3 is not valid for the current time-signature map.")
                                 .arg(requestedPosition.measure)
                                 .arg(requestedPosition.beat)
                                 .arg(requestedPosition.tick));
        return;
    }

    imwrap::MidiEvent updatedEvent = midiEvent;
    updatedEvent.type = imwrap::MidiEventType::SysEx;
    updatedEvent.status = 0xF0;
    updatedEvent.tick = updatedTick;
    updatedEvent.payload = encodeSmfPayload(dialog.event());

    const int updatedModelIndex = replaceEventInTrack(track, modelIndex, std::move(updatedEvent));
    markDirty();
    rebuildEventTable();
    selectVisibleEventByModelIndex(updatedModelIndex);
    statusLabel_->setText(
        QString("Updated iMWrap SysEx event at %1.")
            .arg(formatMbtPosition(segments, updatedTick)));
}

void ExplorerWindow::onTimer() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    double elapsedSeconds = (now - lastTime_) / 1000.0;
    if (elapsedSeconds > 0.1) {
        elapsedSeconds = 0.1;
    }
    lastTime_ = now;

    double exactMicros = elapsedSeconds * 1000000.0;
    exactMicros += microAccumulator_;
    uint32_t deltaMicros = static_cast<uint32_t>(exactMicros);
    microAccumulator_ = exactMicros - deltaMicros;

    if (deltaMicros > 0) {
        engine_.advanceMicroseconds(deltaMicros);
    }

    if (engine_.activeSoundIds().empty()) {
        playbackPositionLabel_->setText("");
        transportTimer_->stop();
    } else {
        uint16_t soundId = engine_.activeSoundIds().front();
        uint16_t track = 0, beat = 0, tick = 0;
        if (engine_.getPlaybackLocation(soundId, &track, &beat, &tick)) {
            playbackPositionLabel_->setText(QString("Track: %1 | Pos: %2:%3").arg(track).arg(beat).arg(tick));
        }
    }

    updateUiState();
}

void ExplorerWindow::rebuildTree() {
    contentTree_->clear();

    for (int soundIndex = 0; soundIndex < static_cast<int>(document_.sounds().size()); ++soundIndex) {
        const imwrap::gui::ProjectSound& sound = document_.sounds()[static_cast<std::size_t>(soundIndex)];
        auto* soundItem = new QTreeWidgetItem(contentTree_);
        soundItem->setText(0, QString::fromStdString(imwrap::gui::FormatSoundLabel(sound)));
        soundItem->setIcon(0, QIcon(":/imwrap/sound.png"));
        soundItem->setData(0, kRoleNodeType, static_cast<int>(NodeType::Sound));
        soundItem->setData(0, kRoleSoundIndex, soundIndex);
        soundItem->setExpanded(true);

        for (int variantIndex = 0; variantIndex < static_cast<int>(sound.variants.size()); ++variantIndex) {
            const imwrap::gui::ProjectVariant& variant = sound.variants[static_cast<std::size_t>(variantIndex)];
            auto* variantItem = new QTreeWidgetItem(soundItem);
            QStringList flags;
            flags << (variant.includeVariant ? "included" : "excluded");
            if (variant.includeMdhd) {
                flags << "MDhd";
            }
            variantItem->setText(0, QString("%1 [%2] (%3 tracks)")
                .arg(imwrap::gui::VariantDisplayName(variant.kind))
                .arg(flags.join(", "))
                .arg(variant.tracks.size()));
            variantItem->setIcon(0, TreeIconForVariantKind(variant.kind));
            variantItem->setData(0, kRoleNodeType, static_cast<int>(NodeType::Variant));
            variantItem->setData(0, kRoleSoundIndex, soundIndex);
            variantItem->setData(0, kRoleVariantIndex, variantIndex);
            variantItem->setExpanded(true);

            for (int trackIndex = 0; trackIndex < static_cast<int>(variant.tracks.size()); ++trackIndex) {
                const imwrap::gui::ProjectTrack& track = variant.tracks[static_cast<std::size_t>(trackIndex)];
                auto* trackItem = new QTreeWidgetItem(variantItem);
                trackItem->setText(0, QString("#%1 %2 (%3 events)")
                    .arg(trackIndex)
                    .arg(QString::fromStdString(track.name))
                    .arg(track.events.size()));
                trackItem->setIcon(0, QIcon(":/imwrap/track.png"));
                trackItem->setData(0, kRoleNodeType, static_cast<int>(NodeType::Track));
                trackItem->setData(0, kRoleSoundIndex, soundIndex);
                trackItem->setData(0, kRoleVariantIndex, variantIndex);
                trackItem->setData(0, kRoleTrackIndex, trackIndex);
            }
        }
    }

    if (contentTree_->topLevelItemCount() > 0 && !contentTree_->currentItem()) {
        contentTree_->setCurrentItem(contentTree_->topLevelItem(0));
    }
}

void ExplorerWindow::rebuildEventTable() {
    const imwrap::gui::ProjectTrack* track = selectedTrack();
    const imwrap::gui::ProjectVariant* variant = selectedVariant();
    const int selectedModelIndex = selectedEventModelIndex();
    visibleEventIndices_.clear();
    if (!track || !variant) {
        eventsTable_->clearContents();
        eventsTable_->setRowCount(0);
        return;
    }
    const std::vector<TimeSignatureSegment> segments = buildTimeSignatureSegments(*variant);

    eventsTable_->clearContents();
    int visibleRow = 0;
    for (int modelIndex = 0; modelIndex < static_cast<int>(track->events.size()); ++modelIndex) {
        const imwrap::MidiEvent& event = track->events[static_cast<std::size_t>(modelIndex)];
        if (filterNonImwrapCheck_->isChecked() && !isRecognizedImwrapEvent(event)) {
            continue;
        }

        visibleEventIndices_.push_back(modelIndex);
        ++visibleRow;
    }

    eventsTable_->setRowCount(visibleRow);
    for (int visibleIndex = 0; visibleIndex < static_cast<int>(visibleEventIndices_.size()); ++visibleIndex) {
        const int modelIndex = visibleEventIndices_[static_cast<std::size_t>(visibleIndex)];
        const imwrap::MidiEvent& event = track->events[static_cast<std::size_t>(modelIndex)];
        eventsTable_->setItem(visibleIndex, 0, new QTableWidgetItem(QString::number(modelIndex)));
        eventsTable_->setItem(visibleIndex, 1, new QTableWidgetItem(QString::number(event.delta)));
        eventsTable_->setItem(visibleIndex, 2, new QTableWidgetItem(formatMbtPosition(segments, event.tick)));
        eventsTable_->setItem(visibleIndex, 3, new QTableWidgetItem(MidiEventTypeLabel(event)));
        auto* descriptionItem = new QTableWidgetItem(describeMidiEvent(event, nullptr));
        descriptionItem->setIcon(EventIconForMidiEvent(event, currentSysexDialect()));
        eventsTable_->setItem(visibleIndex, 4, descriptionItem);
    }

    if (selectedModelIndex >= 0) {
        selectVisibleEventByModelIndex(selectedModelIndex);
    } else if (eventsTable_->rowCount() > 0) {
        eventsTable_->setCurrentCell(0, 0);
    }
}

void ExplorerWindow::rebuildDetailPane() {
    imwrap::gui::ProjectTrack* track = selectedTrack();
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    const int modelIndex = selectedEventModelIndex();
    if (!track || !variant) {
        detailsEdit_->setPlainText("Select a track event to inspect it.");
        return;
    }
    if (eventsTable_->rowCount() == 0) {
        detailsEdit_->setPlainText("No visible events in this track with the current filter.");
        return;
    }
    if (modelIndex < 0 || modelIndex >= static_cast<int>(track->events.size())) {
        detailsEdit_->setPlainText("Select a track event to inspect it.");
        return;
    }

    const imwrap::MidiEvent& event = track->events[static_cast<std::size_t>(modelIndex)];
    const std::vector<TimeSignatureSegment> segments = buildTimeSignatureSegments(*variant);
    bool editable = false;
    const QString description = describeMidiEvent(event, &editable);
    const QString typeLabel = MidiEventTypeLabel(event);

    QStringList lines;
    lines << QString("Type: %1").arg(typeLabel);
    lines << QString("Description: %1").arg(description);
    lines << QString("Position (M:B:T): %1").arg(formatMbtPosition(segments, event.tick));
    lines << QString("Status: 0x%1").arg(event.status, 2, 16, QChar('0')).toUpper();
    lines << QString("Delta: %1").arg(event.delta);
    lines << QString("Tick: %1").arg(event.tick);
    if (!event.payload.empty()) {
        lines << QString("Payload: %1").arg(formatPayloadHex(event.payload));
    }

    if (event.type == imwrap::MidiEventType::SysEx) {
        imwrap::IMWrapControlEvent controlEvent;
        std::string error;
        if (imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &controlEvent,
                                      currentSysexDialect(), &error)) {
            lines << "";
            lines << QString("Decoded: %1").arg(QString::fromStdString(imwrap::DescribeIMWrapSysex(controlEvent)));
        } else if (!error.empty()) {
            lines << "";
            lines << QString("Decode error: %1").arg(QString::fromStdString(error));
        }
    }

    if (editable) {
        lines << "";
        lines << "This event can be edited with the dedicated iMWrap SysEx dialog.";
    }

    detailsEdit_->setPlainText(lines.join('\n'));
}

void ExplorerWindow::rebuildSelectionProperties() {
    imwrap::gui::ProjectSound* sound = selectedSound();
    imwrap::gui::ProjectVariant* selectedVariantPtr = selectedVariant();
    const imwrap::MdhdData defaults = imwrap::MdhdData::Defaults();

    updatingSelectionProps_ = true;

    if (!sound) {
        selectionSummaryLabel_->setText("Select a sound, variant, or track to edit packer properties.");
        soundNameEdit_->clear();
        soundIdSpin_->setValue(0);
        includeVariantCheck_->setChecked(false);
        includeMdhdCheck_->setChecked(false);
        prioritySpin_->setValue(defaults.priority);
        volumeSpin_->setValue(defaults.volume);
        panSpin_->setValue(defaults.pan);
        transposeSpin_->setValue(defaults.transpose);
        detuneSpin_->setValue(defaults.detune);
        speedSpin_->setValue(defaults.speed);
        updatingSelectionProps_ = false;
        return;
    }

    soundNameEdit_->setText(QString::fromStdString(sound->name));
    soundIdSpin_->setValue(sound->id);

    imwrap::VariantKind editorKind = selectedVariantPtr
        ? selectedVariantPtr->kind
        : VariantKindFromCombo(variantKindCombo_);
    const int comboIndex = FindVariantComboIndex(variantKindCombo_, editorKind);
    if (comboIndex >= 0) {
        variantKindCombo_->setCurrentIndex(comboIndex);
    }

    const imwrap::gui::ProjectVariant* editorVariant = selectedVariantPtr
        ? selectedVariantPtr
        : document_.findVariant(sound, editorKind);

    if (selectedVariantPtr) {
        selectionSummaryLabel_->setText(
            QString("Editing sound %1 and variant %2 from the current tree selection.")
                .arg(sound->id)
                .arg(imwrap::gui::VariantShortName(selectedVariantPtr->kind)));
    } else if (editorVariant) {
        selectionSummaryLabel_->setText(
            QString("Editing sound %1. Variant %2 is loaded from the sound view.")
                .arg(sound->id)
                .arg(imwrap::gui::VariantShortName(editorKind)));
    } else {
        selectionSummaryLabel_->setText(
            QString("Editing sound %1. Variant %2 does not exist yet and will be created when applied if enabled.")
                .arg(sound->id)
                .arg(imwrap::gui::VariantShortName(editorKind)));
    }

    if (editorVariant) {
        includeVariantCheck_->setChecked(editorVariant->includeVariant);
        includeMdhdCheck_->setChecked(editorVariant->includeMdhd);
        prioritySpin_->setValue(editorVariant->mdhd.priority);
        volumeSpin_->setValue(editorVariant->mdhd.volume);
        panSpin_->setValue(editorVariant->mdhd.pan);
        transposeSpin_->setValue(editorVariant->mdhd.transpose);
        detuneSpin_->setValue(editorVariant->mdhd.detune);
        speedSpin_->setValue(editorVariant->mdhd.speed);
    } else {
        includeVariantCheck_->setChecked(false);
        includeMdhdCheck_->setChecked(false);
        prioritySpin_->setValue(defaults.priority);
        volumeSpin_->setValue(defaults.volume);
        panSpin_->setValue(defaults.pan);
        transposeSpin_->setValue(defaults.transpose);
        detuneSpin_->setValue(defaults.detune);
        speedSpin_->setValue(defaults.speed);
    }

    updatingSelectionProps_ = false;
}

void ExplorerWindow::updateUiState() {
    const bool hasDocument = !document_.empty();
    const bool hasTrack = selectedTrack() != nullptr;
    const bool hasVariant = selectedVariant() != nullptr;
    const bool hasSound = selectedSound() != nullptr;
    const bool hasActiveSelectedSound = hasSound && engine_.isSoundActive(selectedSoundId());
    bool editable = false;
    const SelectionIndices selection = currentSelection();

    const int modelIndex = selectedEventModelIndex();
    if (hasTrack && modelIndex >= 0 && modelIndex < static_cast<int>(selectedTrack()->events.size())) {
        describeMidiEvent(selectedTrack()->events[static_cast<std::size_t>(modelIndex)], &editable);
    }

    contentTree_->setEnabled(hasDocument);
    eventsTable_->setEnabled(hasTrack);
    selectionPropsWidget_->setEnabled(hasSound);
    hookControlsWidget_->setEnabled(hasSound);
    applySelectionBtn_->setEnabled(hasSound);
    applyHookBtn_->setEnabled(hasActiveSelectedSound);
    variantKindCombo_->setEnabled(hasSound && !hasVariant);
    snmModeCheck_->setEnabled(engine_.activeSoundIds().empty());
    playBtn_->setEnabled(hasSound);
    stopBtn_->setEnabled(hasSound);
    stopAllBtn_->setEnabled(!engine_.activeSoundIds().empty());
    addEventBtn_->setEnabled(hasTrack);
    editEventBtn_->setEnabled(editable);
    importTracksBtn_->setEnabled(hasVariant);
    exportTrackMidiBtn_->setEnabled(hasTrack);
    deleteTrackBtn_->setEnabled(hasTrack);
    moveTrackUpBtn_->setEnabled(hasTrack && selection.trackIndex > 0);
    moveTrackDownBtn_->setEnabled(hasTrack && selectedVariant() &&
        selection.trackIndex >= 0 &&
        selection.trackIndex < static_cast<int>(selectedVariant()->tracks.size() - 1));
}

void ExplorerWindow::updateWindowTitle() {
    const QString baseTitle = "iMWrap Explorer";
    const QString filePart = currentFilePath_.isEmpty() ? "Untitled" : currentFilePath_;
    setWindowTitle(QString("%1 - %2%3").arg(baseTitle, filePart, dirty_ ? " *" : ""));
}

void ExplorerWindow::markDirty(bool dirty) {
    dirty_ = dirty;
    updateWindowTitle();
}

void ExplorerWindow::loadSettings() {
    QSettings settings("imwrap", "IMWrapExplorer");

    if (displayMbtAction_) {
        displayMbtAction_->setChecked(settings.value("ui/displayMbt", false).toBool());
    }

    const int profileIndex = settings.value("playback/profileIndex", 0).toInt();
    if (profileIndex >= 0 && profileIndex < profileCombo_->count()) {
        profileCombo_->setCurrentIndex(profileIndex);
    }
    snmModeCheck_->setChecked(settings.value("playback/snmMode", false).toBool());
    engine_.setCompatibilityProfile(currentCompatibilityProfile());

    const bool filterNonImwrap = settings.value("events/filterNonImwrap", true).toBool();
    filterNonImwrapCheck_->setChecked(filterNonImwrap);

    const QString midiDeviceName = settings.value("playback/midiDeviceName").toString();
    if (!midiDeviceName.isEmpty()) {
        const int deviceIndex = deviceCombo_->findText(midiDeviceName);
        if (deviceIndex >= 0) {
            deviceCombo_->setCurrentIndex(deviceIndex);
        }
    }

    if (settings.contains("window/geometry")) {
        restoreGeometry(settings.value("window/geometry").toByteArray());
    }
    const int storedLayoutVersion = settings.value("window/layoutVersion", 0).toInt();
    bool restoredMainSplitter = false;
    bool restoredBottomSplitter = false;
    if (storedLayoutVersion == kLayoutVersion) {
        if (settings.contains("window/mainSplitter")) {
            restoredMainSplitter = mainSplitter_->restoreState(settings.value("window/mainSplitter").toByteArray());
        }
        if (settings.contains("window/bottomSplitter")) {
            restoredBottomSplitter = bottomSplitter_->restoreState(settings.value("window/bottomSplitter").toByteArray());
        }
    }

    if (!restoredMainSplitter) {
        mainSplitter_->setSizes({280, 520, 360});
    }
    if (!restoredBottomSplitter) {
        bottomSplitter_->setSizes({120, 260, 180, 140});
    }

    const QString lastFilePath = settings.value("document/lastFilePath").toString();
    if (!lastFilePath.isEmpty()) {
        openDocumentPath(lastFilePath, false);
    }
}

void ExplorerWindow::saveSettings() const {
    QSettings settings("imwrap", "IMWrapExplorer");
    if (displayMbtAction_) {
        settings.setValue("ui/displayMbt", displayMbtAction_->isChecked());
    }
    settings.setValue("document/lastFilePath", currentFilePath_);
    settings.setValue("playback/midiDeviceName", deviceCombo_->currentText());
    settings.setValue("playback/profileIndex", profileCombo_->currentIndex());
    settings.setValue("playback/snmMode", snmModeCheck_->isChecked());
    settings.setValue("events/filterNonImwrap", filterNonImwrapCheck_->isChecked());
    settings.setValue("window/layoutVersion", kLayoutVersion);
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/mainSplitter", mainSplitter_->saveState());
    settings.setValue("window/bottomSplitter", bottomSplitter_->saveState());
}

bool ExplorerWindow::openDocumentPath(const QString& path, bool showErrorDialogs) {
    if (path.isEmpty()) {
        return false;
    }

    stopAllSounds();

    std::string error;
    if (!document_.loadFromFile(path.toStdString(), &error)) {
        if (showErrorDialogs) {
            QMessageBox::critical(this, "Open Failed", QString::fromStdString(error));
        } else {
            statusLabel_->setText(QString("Unable to reopen last file: %1").arg(path));
        }
        return false;
    }

    currentFilePath_ = path;
    filePathEdit_->setText(path);
    dirty_ = false;
    rebuildTree();
    rebuildEventTable();
    rebuildDetailPane();
    rebuildSelectionProperties();
    updateWindowTitle();
    updateUiState();
    statusLabel_->setText(QString("Loaded %1").arg(path));
    return true;
}

void ExplorerWindow::selectNode(int soundIndex, int variantIndex, int trackIndex) {
    for (int topLevelIndex = 0; topLevelIndex < contentTree_->topLevelItemCount(); ++topLevelIndex) {
        QTreeWidgetItem* soundItem = contentTree_->topLevelItem(topLevelIndex);
        if (soundItem->data(0, kRoleSoundIndex).toInt() != soundIndex) {
            continue;
        }

        if (variantIndex < 0) {
            contentTree_->setCurrentItem(soundItem);
            return;
        }

        for (int variantChildIndex = 0; variantChildIndex < soundItem->childCount(); ++variantChildIndex) {
            QTreeWidgetItem* variantItem = soundItem->child(variantChildIndex);
            if (variantItem->data(0, kRoleVariantIndex).toInt() != variantIndex) {
                continue;
            }

            if (trackIndex < 0) {
                contentTree_->setCurrentItem(variantItem);
                return;
            }

            for (int trackChildIndex = 0; trackChildIndex < variantItem->childCount(); ++trackChildIndex) {
                QTreeWidgetItem* trackItem = variantItem->child(trackChildIndex);
                if (trackItem->data(0, kRoleTrackIndex).toInt() == trackIndex) {
                    contentTree_->setCurrentItem(trackItem);
                    return;
                }
            }

            contentTree_->setCurrentItem(variantItem);
            return;
        }

        contentTree_->setCurrentItem(soundItem);
        return;
    }
}

int ExplorerWindow::selectedEventModelIndex() const {
    const int visibleRow = eventsTable_->currentRow();
    if (visibleRow < 0 || visibleRow >= static_cast<int>(visibleEventIndices_.size())) {
        return -1;
    }
    return visibleEventIndices_[static_cast<std::size_t>(visibleRow)];
}

void ExplorerWindow::selectVisibleEventByModelIndex(int modelIndex) {
    for (int visibleRow = 0; visibleRow < static_cast<int>(visibleEventIndices_.size()); ++visibleRow) {
        if (visibleEventIndices_[static_cast<std::size_t>(visibleRow)] == modelIndex) {
            eventsTable_->setCurrentCell(visibleRow, 0);
            return;
        }
    }

    if (eventsTable_->rowCount() > 0 && eventsTable_->currentRow() < 0) {
        eventsTable_->setCurrentCell(0, 0);
    }
}

bool ExplorerWindow::rebuildPreviewBank(std::string* error) {
    std::vector<uint8_t> bytes;
    if (!document_.buildBankBytes(&bytes, error)) {
        return false;
    }

    if (!previewBank_.openFromMemory(std::move(bytes), error)) {
        return false;
    }

    engine_.setResourceBank(&previewBank_);
    return true;
}

imwrap::TargetProfile ExplorerWindow::effectiveProfileForSelection() const {
    if (QTreeWidgetItem* item = contentTree_->currentItem()) {
        const QVariant soundIndexData = item->data(0, kRoleSoundIndex);
        const QVariant variantIndexData = item->data(0, kRoleVariantIndex);
        const int soundIndex = soundIndexData.isValid() ? soundIndexData.toInt() : -1;
        const int variantIndex = variantIndexData.isValid() ? variantIndexData.toInt() : -1;
        if (soundIndex >= 0 && soundIndex < static_cast<int>(document_.sounds().size())) {
            const imwrap::gui::ProjectSound& sound = document_.sounds()[static_cast<std::size_t>(soundIndex)];
            if (variantIndex >= 0 && variantIndex < static_cast<int>(sound.variants.size())) {
                return ProfileFromVariantKind(sound.variants[static_cast<std::size_t>(variantIndex)].kind);
            }
        }
    }

    return static_cast<imwrap::TargetProfile>(profileCombo_->currentData().toInt());
}

uint16_t ExplorerWindow::selectedSoundId() const {
    if (SelectionIndices selection = currentSelection(); selection.soundIndex >= 0 &&
        selection.soundIndex < static_cast<int>(document_.sounds().size())) {
        return document_.sounds()[static_cast<std::size_t>(selection.soundIndex)].id;
    }
    return 0;
}

ExplorerWindow::SelectionIndices ExplorerWindow::currentSelection() const {
    SelectionIndices selection;
    QTreeWidgetItem* item = contentTree_->currentItem();
    if (!item) {
        return selection;
    }

    const QVariant soundIndexData = item->data(0, kRoleSoundIndex);
    const QVariant variantIndexData = item->data(0, kRoleVariantIndex);
    const QVariant trackIndexData = item->data(0, kRoleTrackIndex);
    selection.soundIndex = soundIndexData.isValid() ? soundIndexData.toInt() : -1;
    selection.variantIndex = variantIndexData.isValid() ? variantIndexData.toInt() : -1;
    selection.trackIndex = trackIndexData.isValid() ? trackIndexData.toInt() : -1;
    return selection;
}

imwrap::gui::ProjectSound* ExplorerWindow::selectedSound() {
    const SelectionIndices selection = currentSelection();
    if (selection.soundIndex < 0 || selection.soundIndex >= static_cast<int>(document_.sounds().size())) {
        return nullptr;
    }
    return &document_.sounds()[static_cast<std::size_t>(selection.soundIndex)];
}

imwrap::gui::ProjectVariant* ExplorerWindow::selectedVariant() {
    imwrap::gui::ProjectSound* sound = selectedSound();
    const SelectionIndices selection = currentSelection();
    if (!sound || selection.variantIndex < 0 || selection.variantIndex >= static_cast<int>(sound->variants.size())) {
        return nullptr;
    }
    return &sound->variants[static_cast<std::size_t>(selection.variantIndex)];
}

imwrap::gui::ProjectTrack* ExplorerWindow::selectedTrack() {
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    const SelectionIndices selection = currentSelection();
    if (!variant || selection.trackIndex < 0 || selection.trackIndex >= static_cast<int>(variant->tracks.size())) {
        return nullptr;
    }
    return &variant->tracks[static_cast<std::size_t>(selection.trackIndex)];
}

const imwrap::gui::ProjectTrack* ExplorerWindow::selectedTrack() const {
    const SelectionIndices selection = currentSelection();
    if (selection.soundIndex < 0 || selection.soundIndex >= static_cast<int>(document_.sounds().size())) {
        return nullptr;
    }
    const imwrap::gui::ProjectSound& sound = document_.sounds()[static_cast<std::size_t>(selection.soundIndex)];
    if (selection.variantIndex < 0 || selection.variantIndex >= static_cast<int>(sound.variants.size())) {
        return nullptr;
    }
    const imwrap::gui::ProjectVariant& variant = sound.variants[static_cast<std::size_t>(selection.variantIndex)];
    if (selection.trackIndex < 0 || selection.trackIndex >= static_cast<int>(variant.tracks.size())) {
        return nullptr;
    }
    return &variant.tracks[static_cast<std::size_t>(selection.trackIndex)];
}

std::vector<ExplorerWindow::TimeSignatureSegment> ExplorerWindow::buildTimeSignatureSegments(const imwrap::gui::ProjectVariant& variant) const {
    struct RawSignatureEvent {
        uint32_t tick = 0;
        int numerator = 4;
        int denominator = 4;
        int trackIndex = 0;
        int eventIndex = 0;
    };

    std::vector<RawSignatureEvent> rawEvents;
    for (int trackIndex = 0; trackIndex < static_cast<int>(variant.tracks.size()); ++trackIndex) {
        const imwrap::gui::ProjectTrack& track = variant.tracks[static_cast<std::size_t>(trackIndex)];
        for (int eventIndex = 0; eventIndex < static_cast<int>(track.events.size()); ++eventIndex) {
            const imwrap::MidiEvent& event = track.events[static_cast<std::size_t>(eventIndex)];
            if (event.type != imwrap::MidiEventType::Meta || event.metaType != 0x58 || event.payload.size() < 2) {
                continue;
            }

            const int numerator = static_cast<int>(event.payload[0]);
            const uint8_t denominatorShift = event.payload[1];
            if (numerator <= 0 || denominatorShift > 7) {
                continue;
            }

            rawEvents.push_back({
                event.tick,
                numerator,
                1 << denominatorShift,
                trackIndex,
                eventIndex
            });
        }
    }

    std::stable_sort(rawEvents.begin(), rawEvents.end(), [](const RawSignatureEvent& lhs, const RawSignatureEvent& rhs) {
        if (lhs.tick != rhs.tick) {
            return lhs.tick < rhs.tick;
        }
        if (lhs.trackIndex != rhs.trackIndex) {
            return lhs.trackIndex < rhs.trackIndex;
        }
        return lhs.eventIndex < rhs.eventIndex;
    });

    const uint32_t division = (std::max<uint32_t>)(1u, variant.division);
    auto makeSegment = [division](uint32_t tick, const MbtPosition& mbt, int numerator, int denominator) {
        TimeSignatureSegment segment;
        segment.startTick = tick;
        segment.startMeasure = mbt.measure;
        segment.startBeat = mbt.beat;
        segment.startTickInBeat = mbt.tick;
        segment.numerator = (std::max)(1, numerator);
        segment.denominator = (std::max)(1, denominator);
        segment.ticksPerBeat = (std::max<uint32_t>)(1u, (division * 4u) / static_cast<uint32_t>(segment.denominator));
        return segment;
    };

    std::vector<TimeSignatureSegment> segments;
    segments.push_back(makeSegment(0, MbtPosition{}, 4, 4));

    for (const RawSignatureEvent& rawEvent : rawEvents) {
        const MbtPosition startMbt = tickToMbt(segments, rawEvent.tick);
        const TimeSignatureSegment segment = makeSegment(rawEvent.tick, startMbt, rawEvent.numerator, rawEvent.denominator);
        if (!segments.empty() && segments.back().startTick == rawEvent.tick) {
            segments.back() = segment;
        } else {
            segments.push_back(segment);
        }
    }

    return segments;
}

ExplorerWindow::MbtPosition ExplorerWindow::tickToMbt(const std::vector<TimeSignatureSegment>& segments, uint32_t tick) const {
    if (segments.empty()) {
        const uint32_t ticksPerBeat = 480;
        const uint32_t ticksPerMeasure = 4 * ticksPerBeat;
        return {
            static_cast<int>(tick / ticksPerMeasure) + 1,
            static_cast<int>((tick % ticksPerMeasure) / ticksPerBeat) + 1,
            static_cast<int>(tick % ticksPerBeat)
        };
    }

    const TimeSignatureSegment* segment = &segments.front();
    for (const TimeSignatureSegment& candidate : segments) {
        if (candidate.startTick > tick) {
            break;
        }
        segment = &candidate;
    }

    const uint64_t offsetTicks = tick - segment->startTick;
    const uint64_t totalTicksInBeat = static_cast<uint64_t>(segment->startTickInBeat) + offsetTicks;
    const uint64_t beatAdvance = totalTicksInBeat / segment->ticksPerBeat;
    const uint64_t tickInBeat = totalTicksInBeat % segment->ticksPerBeat;
    const uint64_t totalBeats = static_cast<uint64_t>(segment->startBeat - 1) + beatAdvance;

    return {
        segment->startMeasure + static_cast<int>(totalBeats / segment->numerator),
        static_cast<int>(totalBeats % segment->numerator) + 1,
        static_cast<int>(tickInBeat)
    };
}

bool ExplorerWindow::mbtToTick(const std::vector<TimeSignatureSegment>& segments, const MbtPosition& mbt, uint32_t* outTick) const {
    if (!outTick || mbt.measure < 1 || mbt.beat < 1 || mbt.tick < 0) {
        return false;
    }

    auto compareMbt = [](const MbtPosition& lhs, const MbtPosition& rhs) {
        if (lhs.measure != rhs.measure) {
            return lhs.measure < rhs.measure ? -1 : 1;
        }
        if (lhs.beat != rhs.beat) {
            return lhs.beat < rhs.beat ? -1 : 1;
        }
        if (lhs.tick != rhs.tick) {
            return lhs.tick < rhs.tick ? -1 : 1;
        }
        return 0;
    };

    const std::vector<TimeSignatureSegment> fallbackSegments = segments.empty()
        ? std::vector<TimeSignatureSegment>{{TimeSignatureSegment{0, 1, 1, 0, 4, 4, 480}}}
        : segments;

    int segmentIndex = -1;
    for (int index = 0; index < static_cast<int>(fallbackSegments.size()); ++index) {
        const TimeSignatureSegment& segment = fallbackSegments[static_cast<std::size_t>(index)];
        const MbtPosition segmentStart = {segment.startMeasure, segment.startBeat, segment.startTickInBeat};
        if (compareMbt(mbt, segmentStart) < 0) {
            break;
        }
        segmentIndex = index;
    }

    if (segmentIndex < 0) {
        return false;
    }

    const TimeSignatureSegment& segment = fallbackSegments[static_cast<std::size_t>(segmentIndex)];
    if (mbt.beat > segment.numerator) {
        return false;
    }

    const uint64_t ticksPerMeasure = static_cast<uint64_t>(segment.numerator) * segment.ticksPerBeat;
    const uint64_t startTicksInMeasure =
        static_cast<uint64_t>(segment.startBeat - 1) * segment.ticksPerBeat +
        static_cast<uint64_t>(segment.startTickInBeat);
    const uint64_t targetTicksInMeasure =
        static_cast<uint64_t>(mbt.beat - 1) * segment.ticksPerBeat +
        static_cast<uint64_t>(mbt.tick);

    uint64_t offsetTicks = 0;
    if (mbt.measure == segment.startMeasure) {
        if (targetTicksInMeasure < startTicksInMeasure) {
            return false;
        }
        offsetTicks = targetTicksInMeasure - startTicksInMeasure;
    } else if (mbt.measure > segment.startMeasure) {
        offsetTicks =
            (ticksPerMeasure - startTicksInMeasure) +
            static_cast<uint64_t>(mbt.measure - segment.startMeasure - 1) * ticksPerMeasure +
            targetTicksInMeasure;
    } else {
        return false;
    }

    const uint64_t absoluteTick = static_cast<uint64_t>(segment.startTick) + offsetTicks;
    if (segmentIndex + 1 < static_cast<int>(fallbackSegments.size()) &&
        absoluteTick >= fallbackSegments[static_cast<std::size_t>(segmentIndex + 1)].startTick) {
        return false;
    }

    *outTick = static_cast<uint32_t>(absoluteTick);
    return true;
}

QString ExplorerWindow::formatMbtPosition(const std::vector<TimeSignatureSegment>& segments, uint32_t tick) const {
    const MbtPosition mbt = tickToMbt(segments, tick);
    if (displayMbtAction_ && displayMbtAction_->isChecked()) {
        return QString("%1:%2:%3").arg(mbt.measure).arg(mbt.beat).arg(mbt.tick);
    }
    
    uint64_t cumulativeBeats = 0;
    if (segments.empty()) {
        const uint32_t ticksPerBeat = 480;
        cumulativeBeats = tick / ticksPerBeat;
    } else {
        uint32_t currentTick = 0;
        for (size_t i = 0; i < segments.size(); ++i) {
            const TimeSignatureSegment& seg = segments[i];
            uint32_t nextTick = (i + 1 < segments.size()) ? segments[i+1].startTick : tick;
            if (nextTick > tick) {
                nextTick = tick;
            }
            if (nextTick > currentTick) {
                uint32_t ticksInSeg = nextTick - currentTick;
                cumulativeBeats += ticksInSeg / seg.ticksPerBeat;
                currentTick = nextTick;
            }
        }
    }
    return QString("%1:%2").arg(cumulativeBeats + 1).arg(mbt.tick);
}

void ExplorerWindow::normalizeTrackEvents(imwrap::gui::ProjectTrack* track) const {
    if (!track) {
        return;
    }

    struct OrderedEvent {
        imwrap::MidiEvent event;
        int originalIndex = 0;
    };

    std::vector<OrderedEvent> orderedEvents;
    orderedEvents.reserve(track->events.size());

    uint32_t endOfTrackTick = 0;
    bool hadEndOfTrack = false;
    for (int index = 0; index < static_cast<int>(track->events.size()); ++index) {
        const imwrap::MidiEvent& event = track->events[static_cast<std::size_t>(index)];
        if (IsEndOfTrackEvent(event)) {
            hadEndOfTrack = true;
            endOfTrackTick = (std::max)(endOfTrackTick, event.tick);
            continue;
        }

        endOfTrackTick = (std::max)(endOfTrackTick, event.tick);
        orderedEvents.push_back({event, index});
    }

    std::stable_sort(orderedEvents.begin(), orderedEvents.end(), [](const OrderedEvent& lhs, const OrderedEvent& rhs) {
        if (lhs.event.tick != rhs.event.tick) {
            return lhs.event.tick < rhs.event.tick;
        }
        return lhs.originalIndex < rhs.originalIndex;
    });

    std::vector<imwrap::MidiEvent> normalizedEvents;
    normalizedEvents.reserve(orderedEvents.size() + 1);

    uint32_t previousTick = 0;
    for (OrderedEvent& orderedEvent : orderedEvents) {
        orderedEvent.event.delta = orderedEvent.event.tick - previousTick;
        previousTick = orderedEvent.event.tick;
        normalizedEvents.push_back(std::move(orderedEvent.event));
    }

    imwrap::MidiEvent endOfTrack;
    endOfTrack.type = imwrap::MidiEventType::Meta;
    endOfTrack.status = 0xFF;
    endOfTrack.metaType = 0x2F;
    endOfTrack.tick = hadEndOfTrack ? endOfTrackTick : previousTick;
    endOfTrack.delta = endOfTrack.tick - previousTick;
    normalizedEvents.push_back(std::move(endOfTrack));

    track->events = std::move(normalizedEvents);
}

int ExplorerWindow::insertEventIntoTrack(imwrap::gui::ProjectTrack* track, imwrap::MidiEvent event) const {
    if (!track) {
        return -1;
    }

    const uint32_t insertedTick = event.tick;
    const uint8_t insertedStatus = event.status;
    const uint8_t insertedMetaType = event.metaType;
    const bool insertedHasData1 = event.hasData1;
    const bool insertedHasData2 = event.hasData2;
    const uint8_t insertedData1 = event.data1;
    const uint8_t insertedData2 = event.data2;
    const imwrap::MidiEventType insertedType = event.type;
    const std::vector<uint8_t> insertedPayload = event.payload;

    track->events.push_back(std::move(event));
    normalizeTrackEvents(track);

    int insertedIndex = -1;
    for (int index = 0; index < static_cast<int>(track->events.size()); ++index) {
        const imwrap::MidiEvent& currentEvent = track->events[static_cast<std::size_t>(index)];
        if (currentEvent.type == insertedType &&
            currentEvent.tick == insertedTick &&
            currentEvent.status == insertedStatus &&
            currentEvent.metaType == insertedMetaType &&
            currentEvent.hasData1 == insertedHasData1 &&
            currentEvent.hasData2 == insertedHasData2 &&
            currentEvent.data1 == insertedData1 &&
            currentEvent.data2 == insertedData2 &&
            currentEvent.payload == insertedPayload) {
            insertedIndex = index;
        }
    }

    return insertedIndex;
}

int ExplorerWindow::replaceEventInTrack(imwrap::gui::ProjectTrack* track, int modelIndex, imwrap::MidiEvent event) const {
    if (!track || modelIndex < 0 || modelIndex >= static_cast<int>(track->events.size())) {
        return -1;
    }

    const uint32_t updatedTick = event.tick;
    const uint8_t updatedStatus = event.status;
    const uint8_t updatedMetaType = event.metaType;
    const bool updatedHasData1 = event.hasData1;
    const bool updatedHasData2 = event.hasData2;
    const uint8_t updatedData1 = event.data1;
    const uint8_t updatedData2 = event.data2;
    const imwrap::MidiEventType updatedType = event.type;
    const std::vector<uint8_t> updatedPayload = event.payload;

    track->events[static_cast<std::size_t>(modelIndex)] = std::move(event);
    normalizeTrackEvents(track);

    int updatedIndex = -1;
    for (int index = 0; index < static_cast<int>(track->events.size()); ++index) {
        const imwrap::MidiEvent& currentEvent = track->events[static_cast<std::size_t>(index)];
        if (currentEvent.type == updatedType &&
            currentEvent.tick == updatedTick &&
            currentEvent.status == updatedStatus &&
            currentEvent.metaType == updatedMetaType &&
            currentEvent.hasData1 == updatedHasData1 &&
            currentEvent.hasData2 == updatedHasData2 &&
            currentEvent.data1 == updatedData1 &&
            currentEvent.data2 == updatedData2 &&
            currentEvent.payload == updatedPayload) {
            updatedIndex = index;
        }
    }

    return updatedIndex;
}

QString ExplorerWindow::formatPayloadHex(const std::vector<uint8_t>& payload) {
    QStringList parts;
    for (uint8_t byte : payload) {
        parts.append(QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
    }
    return parts.join(' ');
}

QString ExplorerWindow::describeMidiEvent(const imwrap::MidiEvent& event, bool* editableImwrapSysEx) const {
    if (editableImwrapSysEx) {
        *editableImwrapSysEx = false;
    }

    switch (event.type) {
    case imwrap::MidiEventType::Channel: {
        const uint8_t channel = static_cast<uint8_t>(event.status & 0x0F);
        switch (event.status & 0xF0) {
        case 0x80:
            return QString("Ch %1, note %2 (%3), velocity %4")
                .arg(channel + 1)
                .arg(event.data1)
                .arg(FormatNoteName(event.data1))
                .arg(event.hasData2 ? QString::number(event.data2) : "-");
        case 0x90:
            if (event.hasData2 && event.data2 == 0) {
                return QString("Ch %1, note %2 (%3), velocity 0")
                    .arg(channel + 1)
                    .arg(event.data1)
                    .arg(FormatNoteName(event.data1));
            }
            return QString("Ch %1, note %2 (%3), velocity %4")
                .arg(channel + 1)
                .arg(event.data1)
                .arg(FormatNoteName(event.data1))
                .arg(event.hasData2 ? QString::number(event.data2) : "-");
        case 0xA0:
            return QString("Ch %1, note %2 (%3), pressure %4")
                .arg(channel + 1)
                .arg(event.data1)
                .arg(FormatNoteName(event.data1))
                .arg(event.hasData2 ? QString::number(event.data2) : "-");
        case 0xB0: {
            const QString controllerName = ControllerName(event.data1);
            return controllerName.isEmpty()
                ? QString("Ch %1, controller %2, value %3")
                    .arg(channel + 1)
                    .arg(event.data1)
                    .arg(event.hasData2 ? QString::number(event.data2) : "-")
                : QString("Ch %1, controller %2 (%3), value %4")
                    .arg(channel + 1)
                    .arg(event.data1)
                    .arg(controllerName)
                    .arg(event.hasData2 ? QString::number(event.data2) : "-");
        }
        case 0xC0:
            return QString("Ch %1, program %2")
                .arg(channel + 1)
                .arg(event.data1);
        case 0xD0:
            return QString("Ch %1, pressure %2")
                .arg(channel + 1)
                .arg(event.data1);
        case 0xE0: {
            const int bend = static_cast<int>((static_cast<uint16_t>(event.data2) << 7) | event.data1) - 8192;
            return QString("Ch %1, bend %2 (LSB=%3 MSB=%4)")
                .arg(channel + 1)
                .arg(bend)
                .arg(event.data1)
                .arg(event.hasData2 ? QString::number(event.data2) : "-");
        }
        default:
            return QString("Ch %1, data1=%2 data2=%3")
                .arg(channel + 1)
                .arg(event.hasData1 ? QString::number(event.data1) : "-")
                .arg(event.hasData2 ? QString::number(event.data2) : "-");
        }
    }
    case imwrap::MidiEventType::Meta:
        switch (event.metaType) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            return SanitizeMetaText(event.payload);
        case 0x20:
            return event.payload.size() == 1
                ? QString("Channel %1").arg(static_cast<int>(event.payload[0]) + 1)
                : QString("Malformed channel prefix (%1 bytes)").arg(event.payload.size());
        case 0x2F:
            return "End of track";
        case 0x51:
            if (event.payload.size() == 3) {
                const uint32_t microsPerQuarter =
                    (static_cast<uint32_t>(event.payload[0]) << 16) |
                    (static_cast<uint32_t>(event.payload[1]) << 8) |
                    static_cast<uint32_t>(event.payload[2]);
                const double bpm = microsPerQuarter > 0 ? (60000000.0 / static_cast<double>(microsPerQuarter)) : 0.0;
                return QString("%1 us/qn (%2 BPM)")
                    .arg(microsPerQuarter)
                    .arg(QString::number(bpm, 'f', 2));
            }
            return QString("Malformed tempo (%1 bytes)").arg(event.payload.size());
        case 0x54:
            return event.payload.size() == 5
                ? QString("%1:%2:%3 frame %4 subframe %5")
                    .arg(event.payload[0])
                    .arg(event.payload[1])
                    .arg(event.payload[2])
                    .arg(event.payload[3])
                    .arg(event.payload[4])
                : QString("Malformed SMPTE offset (%1 bytes)").arg(event.payload.size());
        case 0x58:
            if (event.payload.size() == 4) {
                const int numerator = event.payload[0];
                const int denominator = 1 << event.payload[1];
                return QString("%1/%2, clocks=%3, 32nds=%4")
                    .arg(numerator)
                    .arg(denominator)
                    .arg(event.payload[2])
                    .arg(event.payload[3]);
            }
            return QString("Malformed time signature (%1 bytes)").arg(event.payload.size());
        case 0x59:
            if (event.payload.size() == 2) {
                const int sharpsFlats = static_cast<int8_t>(event.payload[0]);
                return QString("%1, %2")
                    .arg(sharpsFlats == 0 ? "0 accidentals" : QString("%1 accidentals").arg(sharpsFlats))
                    .arg(event.payload[1] == 0 ? "major" : "minor");
            }
            return QString("Malformed key signature (%1 bytes)").arg(event.payload.size());
        default:
            return QString("%1 bytes")
                .arg(event.payload.size());
        }
    case imwrap::MidiEventType::SysEx: {
        imwrap::IMWrapControlEvent controlEvent;
        if (imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &controlEvent,
                                      currentSysexDialect(), nullptr)) {
            if (editableImwrapSysEx) {
                *editableImwrapSysEx = true;
            }
            return QString("iMWrap SysEx: %1").arg(QString::fromStdString(imwrap::DescribeIMWrapSysex(controlEvent)));
        }
        return QString("SysEx (%1 bytes)").arg(event.payload.size());
    }
    case imwrap::MidiEventType::System:
        if (event.status == 0xF2 && event.payload.size() == 2) {
            const uint16_t position = static_cast<uint16_t>(event.payload[0] | (static_cast<uint16_t>(event.payload[1]) << 7));
            return QString("Position %1").arg(position);
        }
        if ((event.status == 0xF1 || event.status == 0xF3) && event.payload.size() == 1) {
            return QString("Value %1").arg(event.payload[0]);
        }
        return event.payload.empty()
            ? QString("No payload")
            : QString("%1 bytes").arg(event.payload.size());
    }

    return "Event";
}

std::vector<uint8_t> ExplorerWindow::encodeSmfPayload(const imwrap::IMWrapControlEvent& event) {
    std::vector<uint8_t> fullBytes;
    if (!imwrap::EncodeIMWrapSysex(event, &fullBytes)) {
        return {};
    }

    if (!fullBytes.empty() && fullBytes.front() == 0xF0) {
        fullBytes.erase(fullBytes.begin());
    }
    return fullBytes;
}

bool ExplorerWindow::isRecognizedImwrapEvent(const imwrap::MidiEvent& event) const {
    if (event.type != imwrap::MidiEventType::SysEx || event.payload.empty()) {
        return false;
    }

    imwrap::IMWrapControlEvent controlEvent;
    return imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &controlEvent,
                                     currentSysexDialect(), nullptr);
}

imwrap::IMWrapEngine::CompatibilityProfile ExplorerWindow::currentCompatibilityProfile() const {
    return (snmModeCheck_ && snmModeCheck_->isChecked())
        ? imwrap::IMWrapEngine::CompatibilityProfile::Snm
        : imwrap::IMWrapEngine::CompatibilityProfile::GenericV6;
}

imwrap::IMWrapSysexDialect ExplorerWindow::currentSysexDialect() const {
    return (snmModeCheck_ && snmModeCheck_->isChecked())
        ? imwrap::IMWrapSysexDialect::Snm
        : imwrap::IMWrapSysexDialect::GenericV6;
}

void ExplorerWindow::onDisplayFormatChanged() {
    rebuildEventTable();
    rebuildDetailPane();
}

void ExplorerWindow::addNewSong() {
    uint16_t maxId = 0;
    for (const auto& sound : document_.sounds()) {
        if (sound.id > maxId) {
            maxId = sound.id;
        }
    }
    
    imwrap::gui::ProjectSound newSound;
    newSound.id = maxId + 1;
    newSound.name = "New Song";
    
    document_.sounds().push_back(std::move(newSound));
    markDirty(true);
    rebuildTree();
    selectNode(static_cast<int>(document_.sounds().size()) - 1);
}

void ExplorerWindow::exportSelectedTrackMidi() {
    imwrap::gui::ProjectTrack* track = selectedTrack();
    if (!track) return;
    
    QString path = QFileDialog::getSaveFileName(this, "Export MIDI", QString::fromStdString(track->name) + ".mid", "MIDI Files (*.mid *.midi)");
    if (path.isEmpty()) return;
    
    imwrap::SmfSequence seq;
    seq.format = 0;
    seq.division = 480;
    
    imwrap::gui::ProjectVariant* variant = selectedVariant();
    if (variant) {
        seq.division = variant->division;
    }
    
    imwrap::SmfTrack smfTrack;
    smfTrack.events = track->events;
    seq.tracks.push_back(std::move(smfTrack));
    
    std::vector<uint8_t> outBytes;
    std::string error;
    if (imwrap::SmfSerializer::Serialize(seq, &outBytes, &error)) {
        FILE* f = fopen(path.toStdString().c_str(), "wb");
        if (f) {
            fwrite(outBytes.data(), 1, outBytes.size(), f);
            fclose(f);
            statusLabel_->setText(QString("Exported track to %1").arg(path));
        } else {
            QMessageBox::critical(this, "Error", "Failed to open file for writing.");
        }
    } else {
        QMessageBox::critical(this, "Error", QString("Failed to serialize MIDI:\n%1").arg(error.c_str()));
    }
}

