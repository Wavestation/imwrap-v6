#include "PlayerWindow.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QInputDialog>
#include <QDateTime>
#include <QCheckBox>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <algorithm>
#include "imwrap/IMWrapSysex.h"

namespace {

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

QString DescribePlayerMidiEvent(const imwrap::MidiEvent& event, imwrap::IMWrapSysexDialect dialect) {
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
            return QString("%1 bytes").arg(event.payload.size());
        }
    case imwrap::MidiEventType::SysEx: {
        imwrap::IMWrapControlEvent controlEvent;
        if (imwrap::DecodeIMWrapSysex(imwrap::ByteView(event.payload.data(), event.payload.size()), &controlEvent, dialect)) {
            return QString("iMWrap SysEx: %1").arg(QString::fromStdString(imwrap::DescribeIMWrapSysex(controlEvent)));
        }

        QStringList bytes;
        for (size_t index = 0; index < event.payload.size() && event.payload[index] != 0xF7; ++index) {
            bytes.append(QString("%1").arg(event.payload[index], 2, 16, QChar('0')).toUpper());
        }
        return bytes.isEmpty()
            ? "SysEx"
            : QString("Raw: %1").arg(bytes.join(' '));
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

} // namespace

PlayerWindow::WinMMSink::~WinMMSink() {
    closeDevice();
}

bool PlayerWindow::WinMMSink::openDevice(UINT deviceId) {
    closeDevice();
    if (midiOutOpen(&hMidiOut, deviceId, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return false;
    }
    return true;
}

void PlayerWindow::WinMMSink::closeDevice() {
    if (hMidiOut) {
        midiOutReset(hMidiOut);
        midiOutClose(hMidiOut);
        hMidiOut = nullptr;
    }
}

bool PlayerWindow::WinMMSink::isAvailable() const {
    return hMidiOut != nullptr;
}

void PlayerWindow::WinMMSink::onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) {
    if (!hMidiOut) return;
    DWORD msg = status | (data1 << 8);
    if (hasData2) {
        msg |= (data2 << 16);
    }
    midiOutShortMsg(hMidiOut, msg);
}

void PlayerWindow::WinMMSink::onSysEx(uint16_t soundId, imwrap::ByteView message) {
    if (!hMidiOut || message.empty()) return;
    
    std::vector<char> buffer;
    if (static_cast<uint8_t>(message.data()[0]) != 0xF0) {
        buffer.push_back(static_cast<char>(0xF0));
    }
    buffer.insert(buffer.end(), message.data(), message.data() + message.size());
    if (static_cast<uint8_t>(buffer.back()) != 0xF7) {
        buffer.push_back(static_cast<char>(0xF7));
    }
    
    MIDIHDR hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.lpData = buffer.data();
    hdr.dwBufferLength = static_cast<DWORD>(buffer.size());
    hdr.dwFlags = 0;
    
    if (midiOutPrepareHeader(hMidiOut, &hdr, sizeof(hdr)) == MMSYSERR_NOERROR) {
        if (midiOutLongMsg(hMidiOut, &hdr, sizeof(hdr)) == MMSYSERR_NOERROR) {
            int timeout = 100; // max 100ms
            while (!(hdr.dwFlags & MHDR_DONE) && timeout-- > 0) {
                Sleep(1);
            }
        }
        midiOutUnprepareHeader(hMidiOut, &hdr, sizeof(hdr));
    }
}

PlayerWindow::PlayerWindow(QWidget *parent) : QMainWindow(parent), previewEnabled(false), microAccumulator(0) {
    engine.setMidiSink(&midiSink);
    transportTimer = new QTimer(this);
    transportTimer->setInterval(16); // ~60fps
    connect(transportTimer, &QTimer::timeout, this, &PlayerWindow::onTimer);
    
    setupUi();
    refreshDevices();

    QSettings settings("imwrap", "IMWrapPlayerGui");
    QString lastBank = settings.value("lastBank", "").toString();
    if (!lastBank.isEmpty()) {
        bankPathEdit->setText(lastBank);
        loadBank();
    }
    QString lastDevice = settings.value("lastDevice", "").toString();
    if (!lastDevice.isEmpty()) {
        int idx = deviceCombo->findText(lastDevice);
        if (idx != -1) deviceCombo->setCurrentIndex(idx);
    }
    int lastProfile = settings.value("lastProfile", 0).toInt();
    if (lastProfile >= 0 && lastProfile < profileCombo->count()) {
        profileCombo->setCurrentIndex(lastProfile);
    }
    muntModeCheck->setChecked(settings.value("muntMode", false).toBool());
    snmModeCheck->setChecked(settings.value("snmMode", false).toBool());
    engine.setCompatibilityProfile(currentCompatibilityProfile());
    bool preview = settings.value("previewEnabled", false).toBool();
    if (preview) {
        togglePreview();
    }
}

PlayerWindow::~PlayerWindow() {
    disablePreviewBackend();
    QSettings settings("imwrap", "IMWrapPlayerGui");
    settings.setValue("lastBank", bankPathEdit->text());
    settings.setValue("lastDevice", deviceCombo->currentText());
    settings.setValue("lastProfile", profileCombo->currentIndex());
    settings.setValue("muntMode", muntModeCheck->isChecked());
    settings.setValue("snmMode", snmModeCheck->isChecked());
    settings.setValue("previewEnabled", previewEnabled);
}

bool PlayerWindow::ensurePreviewBackend(imwrap::TargetProfile profile, std::string* error) {
    if (profile == imwrap::TargetProfile::Adlib) {
        midiSink.closeDevice();
        if (!adlibSink.isAvailable() && !adlibSink.start(error)) {
            return false;
        }
        previewBackend = PreviewBackend::Adlib;
        engine.setMidiSink(&adlibSink);
        if (error) {
            error->clear();
        }
        return true;
    }

    if (profile == imwrap::TargetProfile::Mt32 && muntModeCheck->isChecked()) {
        midiSink.closeDevice();
        adlibSink.stop();
        if (!muntSink.isAvailable() && !muntSink.start(error)) {
            // Fall back to winmm
        } else {
            previewBackend = PreviewBackend::Munt;
            engine.setMidiSink(&muntSink);
            if (error) {
                error->clear();
            }
            return true;
        }
    }

    adlibSink.stop();
    muntSink.stop();
    if (deviceCombo->currentIndex() < 0) {
        if (error) {
            *error = "No MIDI output device is selected.";
        }
        return false;
    }

    const UINT devId = deviceCombo->currentData().toUInt();
    if (!midiSink.openDevice(devId)) {
        if (error) {
            *error = "Cannot open MIDI device.";
        }
        return false;
    }

    previewBackend = PreviewBackend::WinMM;
    engine.setMidiSink(&midiSink);
    if (error) {
        error->clear();
    }
    return true;
}

void PlayerWindow::disablePreviewBackend() {
    transportTimer->stop();
    adlibSink.stop();
    muntSink.stop();
    midiSink.closeDevice();
    previewBackend = PreviewBackend::None;
    engine.setMidiSink(nullptr);
}

void PlayerWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    auto *configBox = new QGroupBox("iMWrap Configuration");
    auto *configLayout = new QVBoxLayout(configBox);
    
    auto *bankLayout = new QHBoxLayout();
    bankLayout->addWidget(new QLabel("Bank File (.ims):"));
    bankPathEdit = new QLineEdit();
    bankLayout->addWidget(bankPathEdit);
    auto *browseBankBtn = new QPushButton("Browse...");
    connect(browseBankBtn, &QPushButton::clicked, this, &PlayerWindow::browseBank);
    bankLayout->addWidget(browseBankBtn);
    auto *browseXoredBankBtn = new QPushButton("Open XORed...");
    connect(browseXoredBankBtn, &QPushButton::clicked, this, &PlayerWindow::browseXoredBank);
    bankLayout->addWidget(browseXoredBankBtn);
    configLayout->addLayout(bankLayout);

    auto *backendLayout = new QHBoxLayout();
    backendLayout->addWidget(new QLabel("MIDI Out (WinMM):"));
    deviceCombo = new QComboBox();
    backendLayout->addWidget(deviceCombo);

    backendLayout->addWidget(new QLabel("Render Type:"));
    profileCombo = new QComboBox();
    profileCombo->addItem("General MIDI", static_cast<int>(imwrap::TargetProfile::GeneralMidi));
    profileCombo->addItem("Roland MT-32", static_cast<int>(imwrap::TargetProfile::Mt32));
    profileCombo->addItem("AdLib", static_cast<int>(imwrap::TargetProfile::Adlib));
    connect(profileCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx >= 0) {
            auto profile = static_cast<imwrap::TargetProfile>(profileCombo->itemData(idx).toInt());
            engine.setTargetProfile(profile);
            engine.setNativeMt32Output(profile == imwrap::TargetProfile::Mt32);
        }
    });
    backendLayout->addWidget(profileCombo);
    
    muntModeCheck = new QCheckBox("Use MT-32 emulator");
    backendLayout->addWidget(muntModeCheck);

    snmModeCheck = new QCheckBox("SNM mode");
    connect(snmModeCheck, &QCheckBox::toggled, this, [this](bool) {
        engine.setCompatibilityProfile(currentCompatibilityProfile());
        updateUiState();
    });
    backendLayout->addWidget(snmModeCheck);
    
    previewBtn = new QPushButton("Enable Preview");
    connect(previewBtn, &QPushButton::clicked, this, &PlayerWindow::togglePreview);
    backendLayout->addWidget(previewBtn);
    configLayout->addLayout(backendLayout);

    layout->addWidget(configBox);

    auto *split = new QHBoxLayout();
    
    // Left: Sound List
    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel("Sounds:"));
    soundTree = new QTreeWidget();
    soundTree->setHeaderHidden(true);
    connect(soundTree, &QTreeWidget::itemSelectionChanged, this, &PlayerWindow::updateUiState);
    leftLayout->addWidget(soundTree);
    split->addLayout(leftLayout, 1);

    // Center: Events
    auto *centerLayout = new QVBoxLayout();
    centerLayout->addWidget(new QLabel("Sound Events:"));
    eventsTable = new QTableWidget(0, 4);
    eventsTable->setHorizontalHeaderLabels({"Tick", "Track", "Type", "Description"});
    eventsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    centerLayout->addWidget(eventsTable);
    split->addLayout(centerLayout, 2);

    // Right: Active & Controls
    auto *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(new QLabel("Active Sounds:"));
    activeTable = new QTableWidget(0, 4);
    activeTable->setHorizontalHeaderLabels({"ID", "Track", "Beat", "Tick"});
    activeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rightLayout->addWidget(activeTable);
    
    auto *controlsLayout = new QHBoxLayout();
    playBtn = new QPushButton("▶ Play");
    connect(playBtn, &QPushButton::clicked, this, &PlayerWindow::playSound);
    controlsLayout->addWidget(playBtn);
    stopBtn = new QPushButton("⏹ Stop");
    connect(stopBtn, &QPushButton::clicked, this, &PlayerWindow::stopSound);
    controlsLayout->addWidget(stopBtn);
    stopAllBtn = new QPushButton("⏹ Stop All");
    connect(stopAllBtn, &QPushButton::clicked, this, &PlayerWindow::stopAllSounds);
    controlsLayout->addWidget(stopAllBtn);
    rightLayout->addLayout(controlsLayout);
    
    // Hooks and Advance
    auto *hookBox = new QGroupBox("iMWrap Control");
    auto *hookLayout = new QFormLayout(hookBox);
    hookClassCombo = new QComboBox();
    hookClassCombo->addItems({
        "0 - Jump",
        "1 - Global Transpose",
        "2 - Part On/Off",
        "3 - Part Volume",
        "4 - Part Program",
        "5 - Part Transpose"
    });
    hookValueSpin = new QSpinBox(); hookValueSpin->setRange(0, 255);
    hookChannelSpin = new QSpinBox(); hookChannelSpin->setRange(0, 16);
    auto *applyHookBtn = new QPushButton("Apply Hook");
    connect(applyHookBtn, &QPushButton::clicked, this, &PlayerWindow::applyHook);

    auto *hookHBox = new QHBoxLayout();
    hookHBox->addWidget(hookClassCombo); hookHBox->addWidget(hookValueSpin); hookHBox->addWidget(hookChannelSpin); hookHBox->addWidget(applyHookBtn);
    hookLayout->addRow("Hook (C/V/Ch):", hookHBox);
    
    advanceSpin = new QSpinBox(); advanceSpin->setRange(0, 99999); advanceSpin->setValue(480);
    auto *advBtn = new QPushButton("Advance (Ticks)");
    connect(advBtn, &QPushButton::clicked, this, &PlayerWindow::advanceTicks);
    auto *advHBox = new QHBoxLayout();
    advHBox->addWidget(advanceSpin); advHBox->addWidget(advBtn);
    hookLayout->addRow("Manual Advance:", advHBox);
    rightLayout->addWidget(hookBox);

    split->addLayout(rightLayout, 2);

    layout->addLayout(split, 1);

    statusLabel = new QLabel("Ready.");
    layout->addWidget(statusLabel);
    
    updateUiState();
}

void PlayerWindow::refreshDevices() {
    deviceCombo->clear();
    UINT numDevs = midiOutGetNumDevs();
    for (UINT i = 0; i < numDevs; i++) {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            deviceCombo->addItem(caps.szPname, i);
        }
    }
}

void PlayerWindow::browseBank() {
    QString path = QFileDialog::getOpenFileName(this, "Open IMS Bank", "", "iMWrap Files (*.ims)");
    if (!path.isEmpty()) {
        bankPathEdit->setText(path);
        loadBank();
    }
}

void PlayerWindow::browseXoredBank() {
    QString path = QFileDialog::getOpenFileName(this, "Open XORed Bank", "", "iMWrap KOG (*.kog)");
    if (path.isEmpty()) return;

    bool ok;
    int key = QInputDialog::getInt(this, "XOR Encryption", "Enter XOR Key (0-255):", 39, 0, 255, 1, &ok);
    if (!ok) return;

    std::ifstream in(path.toStdString(), std::ios::binary);
    if (!in) {
        QMessageBox::critical(this, "Error", "Failed to open file for reading.");
        return;
    }
    
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        if (buffer.size() >= 11) {
            bool isKogx = (buffer[0] == 'K' && buffer[1] == 'O' && buffer[2] == 'G' && buffer[3] == 'X');
            bool isFelonia = (buffer[buffer.size() - 7] == 'F' && buffer[buffer.size() - 6] == 'E' &&
                              buffer[buffer.size() - 5] == 'L' && buffer[buffer.size() - 4] == 'O' &&
                              buffer[buffer.size() - 3] == 'N' && buffer[buffer.size() - 2] == 'I' &&
                              buffer[buffer.size() - 1] == 'A');
            if (isKogx && isFelonia) {
                if (key != 0) {
                    std::uint32_t state = 0x9E3779B9u ^ static_cast<std::uint32_t>(key);
                    for (size_t i = 4; i < buffer.size() - 7; ++i) {
                        state ^= state << 13;
                        state ^= state >> 17;
                        state ^= state << 5;
                        buffer[i] ^= static_cast<std::uint8_t>(state);
                    }
                }
                buffer.erase(buffer.end() - 7, buffer.end());
                buffer.erase(buffer.begin(), buffer.begin() + 4);
            } else {
                QMessageBox::critical(this, "Error", "Invalid KOGX file: Missing magic headers.");
                return;
            }
        } else {
            QMessageBox::critical(this, "Error", "Invalid KOGX file: File too small.");
            return;
        }
        
        std::string err;
        if (bank.openFromMemory(std::move(buffer), &err)) {
            bankPathEdit->setText(path + " (XORed In Memory)");
            loadBank();
        } else {
            QMessageBox::critical(this, "Error", QString::fromStdString(err));
        }
    } else {
        QMessageBox::critical(this, "Error", "Failed to read file.");
    }
}

void PlayerWindow::loadBank() {
    if (bankPathEdit->text().endsWith("(XORed In Memory)")) {
        // Already loaded in memory by browseXoredBank, just update UI
    } else {
        std::string err;
        if (!bank.openFromFile(bankPathEdit->text().toStdString(), &err)) {
            statusLabel->setText(QString("Error: %1").arg(QString::fromStdString(err)));
            return;
        }
    }

    statusLabel->setText(QString("Bank loaded: %1").arg(bankPathEdit->text()));
    engine.setResourceBank(&bank);
    
    soundTree->clear();
    for (uint16_t id : bank.soundIds()) {
        auto res = bank.loadSound(id);
        QTreeWidgetItem *soundItem = new QTreeWidgetItem(soundTree);
        soundItem->setText(0, QString("%1: %2").arg(id).arg(QString::fromStdString(res.name())));
        soundItem->setData(0, Qt::UserRole, id);
        
        // Add tracks as children
        imwrap::SmfSequence seq;
        if (res.valid() && imwrap::SmfParser::Parse(res.selectVariant(engine.targetProfile()).smfData, &seq)) {
            uint16_t tIdx = 0;
            for (const auto &trk : seq.tracks) {
                QTreeWidgetItem *trackItem = new QTreeWidgetItem(soundItem);
                trackItem->setText(0, QString("Track %1 (%2 evts)").arg(tIdx).arg(trk.events.size()));
                trackItem->setData(0, Qt::UserRole, id); // Keep parent ID
                tIdx++;
            }
        }
    }
    updateUiState();
}

void PlayerWindow::togglePreview() {
    if (previewEnabled) {
        disablePreviewBackend();
        engine.resetMt32Initialization();
        previewEnabled = false;
        previewBtn->setText("Enable Preview");
        statusLabel->setText("Preview disabled.");
    } else {
        std::string error;
        if (!ensurePreviewBackend(engine.targetProfile(), &error)) {
            QMessageBox::warning(this, "Error", QString::fromStdString(error));
            return;
        }
        engine.resetMt32Initialization();
        previewEnabled = true;
        previewBtn->setText("Disable Preview");
        statusLabel->setText(engine.targetProfile() == imwrap::TargetProfile::Adlib
                                 ? "Preview enabled with embedded AdLib audio."
                                 : "Preview enabled.");
    }
}

void PlayerWindow::playSound() {
    if (soundTree->selectedItems().isEmpty()) return;
    uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
    engine.setCompatibilityProfile(currentCompatibilityProfile());
    if (!previewEnabled) {
        togglePreview();
        if (!previewEnabled) {
            return;
        }
    }

    std::string error;
    if (!ensurePreviewBackend(engine.targetProfile(), &error)) {
        QMessageBox::warning(this, "Error", QString::fromStdString(error));
        disablePreviewBackend();
        engine.resetMt32Initialization();
        previewEnabled = false;
        previewBtn->setText("Enable Preview");
        statusLabel->setText("Preview disabled.");
        return;
    }

    engine.stopAllSounds();
    if (engine.targetProfile() == imwrap::TargetProfile::Mt32) {
        engine.resetMt32Initialization();
        engine.initMt32();
    }
    engine.startSound(id);

    if (!transportTimer->isActive()) {
        lastTime = QDateTime::currentMSecsSinceEpoch();
        microAccumulator = 0;
        transportTimer->start();
    }
    updateUiState();
}

void PlayerWindow::stopSound() {
    if (soundTree->selectedItems().isEmpty()) return;
    uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
    engine.stopSound(id);
    updateUiState();
}

void PlayerWindow::stopAllSounds() {
    engine.stopAllSounds();
    updateUiState();
}

void PlayerWindow::applyHook() {
    if (soundTree->selectedItems().isEmpty()) return;
    uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
    // iMWrap command: 0x010C is PlayerSetHook
    int16_t args[5] = {0x010C, (int16_t)id, (int16_t)hookClassCombo->currentIndex(), (int16_t)hookValueSpin->value(), (int16_t)hookChannelSpin->value()};
    engine.doCommand(5, args);
}

void PlayerWindow::advanceTicks() {
    engine.advanceAll(advanceSpin->value());
    updateUiState();
}

void PlayerWindow::onTimer() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double elapsedSecs = (now - lastTime) / 1000.0;
    if (elapsedSecs > 0.1) elapsedSecs = 0.1; // Limit spike
    lastTime = now;
    
    double exactMicros = elapsedSecs * 1000000.0;
    exactMicros += microAccumulator;
    uint32_t deltaMicros = static_cast<uint32_t>(exactMicros);
    microAccumulator = exactMicros - deltaMicros;

    if (deltaMicros > 0) {
        engine.advanceMicroseconds(deltaMicros);
    }
    updateUiState();
    
    if (engine.activeSoundIds().empty()) {
        transportTimer->stop();
    }
}

void PlayerWindow::updateUiState() {
    // Update Events Table
    const imwrap::IMWrapSysexDialect dialect = currentSysexDialect();
    if (!soundTree->selectedItems().isEmpty()) {
        uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
        auto res = bank.loadSound(id);
        
        struct SimpleEvent {
            uint32_t tick;
            uint16_t track;
            QString type;
            QString text;
        };
        std::vector<SimpleEvent> seqEvents;
        
        imwrap::SmfSequence seq;
        if (res.valid() && imwrap::SmfParser::Parse(res.selectVariant(engine.targetProfile()).smfData, &seq)) {
            uint16_t tIdx = 0;
            for (const auto &trk : seq.tracks) {
                uint32_t absTick = 0;
                for (const auto &evt : trk.events) {
                    absTick += evt.delta;
                    seqEvents.push_back({
                        absTick,
                        tIdx,
                        MidiEventTypeLabel(evt),
                        DescribePlayerMidiEvent(evt, dialect)
                    });
                }
                tIdx++;
            }
        }

        std::stable_sort(seqEvents.begin(), seqEvents.end(), [](const SimpleEvent& lhs, const SimpleEvent& rhs) {
            if (lhs.tick != rhs.tick) {
                return lhs.tick < rhs.tick;
            }
            return lhs.track < rhs.track;
        });
        
        eventsTable->setRowCount(seqEvents.size());
        for (size_t i = 0; i < seqEvents.size(); ++i) {
            eventsTable->setItem(i, 0, new QTableWidgetItem(QString::number(seqEvents[i].tick)));
            eventsTable->setItem(i, 1, new QTableWidgetItem(QString::number(seqEvents[i].track)));
            eventsTable->setItem(i, 2, new QTableWidgetItem(seqEvents[i].type));
            eventsTable->setItem(i, 3, new QTableWidgetItem(seqEvents[i].text));
        }
    } else {
        eventsTable->setRowCount(0);
    }
    
    // Update Active Sounds Table
    auto activeIds = engine.activeSoundIds();
    snmModeCheck->setEnabled(activeIds.empty());
    activeTable->setRowCount(activeIds.size());
    for (size_t i = 0; i < activeIds.size(); ++i) {
        uint16_t id = activeIds[i];
        uint16_t track = 0, beat = 0, tick = 0;
        engine.getPlaybackLocation(id, &track, &beat, &tick);
        
        activeTable->setItem(i, 0, new QTableWidgetItem(QString::number(id)));
        activeTable->setItem(i, 1, new QTableWidgetItem(QString::number(track)));
        activeTable->setItem(i, 2, new QTableWidgetItem(QString::number(beat)));
        activeTable->setItem(i, 3, new QTableWidgetItem(QString::number(tick)));
    }
}

imwrap::IMWrapEngine::CompatibilityProfile PlayerWindow::currentCompatibilityProfile() const {
    return (snmModeCheck && snmModeCheck->isChecked())
        ? imwrap::IMWrapEngine::CompatibilityProfile::Snm
        : imwrap::IMWrapEngine::CompatibilityProfile::GenericV6;
}

imwrap::IMWrapSysexDialect PlayerWindow::currentSysexDialect() const {
    return (snmModeCheck && snmModeCheck->isChecked())
        ? imwrap::IMWrapSysexDialect::Snm
        : imwrap::IMWrapSysexDialect::GenericV6;
}
