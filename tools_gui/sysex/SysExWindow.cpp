#include "SysExWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTextBrowser>
#include <sstream>
#include <iomanip>
#include "imwrap/IMWrapSysex.h"
#include "OplSbi.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

bool ReadFileBytes(const QString& path, std::vector<uint8_t>* out, QString* error) {
    if (!out) {
        if (error) {
            *error = "Internal error: missing output buffer.";
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    const QByteArray data = file.readAll();
    out->assign(data.begin(), data.end());
    return true;
}

bool WriteFileBytes(const QString& path, const std::vector<uint8_t>& bytes, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    const qint64 written = file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    if (written != static_cast<qint64>(bytes.size())) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    return true;
}

} // namespace

SysExWindow::SysExWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
    populateMidiDevices();
    updateFieldVisibility();
    updateGeneratedHex();
}

SysExWindow::~SysExWindow() {
#ifdef Q_OS_WIN
    if (hMidiOut) {
        midiOutClose(static_cast<HMIDIOUT>(hMidiOut));
        hMidiOut = nullptr;
    }
#endif
}

void SysExWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createGeneratorTab(), "SysEx Generator");
    tabs->addTab(createDecoderTab(), "SysEx Decoder");
    tabs->addTab(createGuideTab(), "Composer's Guide");
    mainLayout->addWidget(tabs);
}

QWidget* SysExWindow::createGeneratorTab() {
    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);

    // Left Column: Parameters
    auto *leftBox = new QGroupBox("iMWrap Message Type");
    formLayout = new QFormLayout(leftBox);

    typeCombo = new QComboBox();
    typeCombo->addItems({
        "Allocate Part (0x00)", "Shutdown Part (0x01)", "Start Song (0x02)",
        "AdLib Part Instrument (0x10)", "AdLib Global Instrument (0x11)",
        "Parameter Adjust (0x21)", "Hook Jump (0x30)", "Hook Global Transpose (0x31)",
        "Hook Part On/Off (0x32)", "Hook Set Volume (0x33)", "Hook Set Program (0x34)",
        "Hook Set Transpose (0x35)", "Marker (0x40)", "Set Loop (0x50)",
        "Clear Loop (0x51)", "Set Instrument (0x60)"
    });
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SysExWindow::updateFieldVisibility);
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SysExWindow::updateGeneratedHex);
    formLayout->addRow("Type:", typeCombo);

    // Spinbox macro for convenience
    auto makeSpin = [&](int min, int max, int val = 0) {
        auto *s = new QSpinBox(); s->setRange(min, max); s->setValue(val);
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, &SysExWindow::updateGeneratedHex);
        return s;
    };

    partSpin = makeSpin(0, 15);
    channelSpin = makeSpin(0, 15);
    unknownSpin = makeSpin(0, 127);
    
    partOnCheck = new QCheckBox("Part Enabled"); partOnCheck->setChecked(true);
    connect(partOnCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    reverbCheck = new QCheckBox("Reverb Enabled");
    connect(reverbCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    percussionCheck = new QCheckBox("Percussion");
    connect(percussionCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    
    prioritySpin = makeSpin(0, 255, 90);
    volumeSpin = makeSpin(0, 127, 127);
    panSpin = makeSpin(-64, 63, 0);
    transposeSpin = makeSpin(-127, 127, 0);
    detuneSpin = makeSpin(-128, 127, 0);
    pitchbendSpin = makeSpin(0, 255, 2);
    programSpin = makeSpin(0, 127, 0);
    
    paramSpin = makeSpin(0, 65535);
    paramValueSpin = makeSpin(0, 65535);
    
    hookCmdSpin = makeSpin(0, 255);
    targetTrackSpin = makeSpin(0, 65535);
    targetBeatSpin = makeSpin(0, 65535);
    targetTickSpin = makeSpin(0, 65535);
    
    hookRelativeCheck = new QCheckBox("Relative Transposition");
    connect(hookRelativeCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    hookValueSpin = makeSpin(-128, 127);
    
    markerValueSpin = makeSpin(0, 127, 0);
    
    loopCountSpin = makeSpin(0, 65535);
    loopToBeatSpin = makeSpin(0, 65535);
    loopToTickSpin = makeSpin(0, 65535);
    loopFromBeatSpin = makeSpin(0, 65535);
    loopFromTickSpin = makeSpin(0, 65535);
    
    instrumentIdSpin = makeSpin(0, 65535);
    
    adlibHexEdit = new QTextEdit();
    adlibHexEdit->setMaximumHeight(60);
    connect(adlibHexEdit, &QTextEdit::textChanged, this, &SysExWindow::updateGeneratedHex);
    adlibEditorWidget = new QWidget();
    auto *adlibLayout = new QVBoxLayout(adlibEditorWidget);
    adlibLayout->setContentsMargins(0, 0, 0, 0);
    adlibLayout->setSpacing(6);
    adlibLayout->addWidget(adlibHexEdit);
    auto *adlibButtonsLayout = new QHBoxLayout();
    auto *importSbiBtn = new QPushButton("Import SBI...");
    connect(importSbiBtn, &QPushButton::clicked, this, &SysExWindow::importAdlibSbi);
    adlibButtonsLayout->addWidget(importSbiBtn);
    auto *exportSbiBtn = new QPushButton("Export SBI...");
    connect(exportSbiBtn, &QPushButton::clicked, this, &SysExWindow::exportAdlibSbi);
    adlibButtonsLayout->addWidget(exportSbiBtn);
    adlibButtonsLayout->addStretch(1);
    adlibLayout->addLayout(adlibButtonsLayout);

    formLayout->addRow("Part (0-15):", partSpin);
    formLayout->addRow("MIDI Channel (0-15):", channelSpin);
    formLayout->addRow("Unknown (0-127):", unknownSpin);
    
    formLayout->addRow("", partOnCheck);
    formLayout->addRow("", reverbCheck);
    formLayout->addRow("Priority:", prioritySpin);
    formLayout->addRow("Volume:", volumeSpin);
    formLayout->addRow("Pan (-64..63):", panSpin);
    formLayout->addRow("", percussionCheck);
    formLayout->addRow("Transpose:", transposeSpin);
    formLayout->addRow("Detune:", detuneSpin);
    formLayout->addRow("Pitchbend Factor:", pitchbendSpin);
    formLayout->addRow("Program:", programSpin);
    
    formLayout->addRow("Parameter:", paramSpin);
    formLayout->addRow("Value:", paramValueSpin);
    
    formLayout->addRow("Hook Command:", hookCmdSpin);
    formLayout->addRow("Target Track:", targetTrackSpin);
    formLayout->addRow("Target Beat:", targetBeatSpin);
    formLayout->addRow("Target Tick:", targetTickSpin);
    formLayout->addRow("", hookRelativeCheck);
    formLayout->addRow("Hook Value:", hookValueSpin);
    
    formLayout->addRow("Marker Value (0-127):", markerValueSpin);
    
    formLayout->addRow("Loop Count:", loopCountSpin);
    formLayout->addRow("Jump to (Beat):", loopToBeatSpin);
    formLayout->addRow("Jump to (Tick):", loopToTickSpin);
    formLayout->addRow("From (Beat):", loopFromBeatSpin);
    formLayout->addRow("From (Tick):", loopFromTickSpin);
    
    formLayout->addRow("Instrument ID:", instrumentIdSpin);
    formLayout->addRow("AdLib Registers:", adlibEditorWidget);

    layout->addWidget(leftBox, 1);

    // Right Column: Output
    auto *rightBox = new QGroupBox("Generated iMWrap SysEx");
    auto *rightLayout = new QVBoxLayout(rightBox);

    generatedHexDisplay = new QTextEdit();
    generatedHexDisplay->setReadOnly(true);
    generatedHexDisplay->setStyleSheet("font-family: monospace; font-weight: bold; color: purple; font-size: 14pt;");
    rightLayout->addWidget(generatedHexDisplay);

    auto *copyBtn = new QPushButton("Copy to Clipboard");
    connect(copyBtn, &QPushButton::clicked, this, &SysExWindow::copyToClipboard);
    rightLayout->addWidget(copyBtn);

    // Direct MIDI Out GroupBox
    auto *midiBox = new QGroupBox("Direct MIDI Send");
    auto *midiLayout = new QFormLayout(midiBox);
    
    midiCombo = new QComboBox();
    midiLayout->addRow("Device:", midiCombo);
    connect(midiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SysExWindow::onMidiDeviceChanged);
    
    sendMidiBtn = new QPushButton("Send SysEx");
    connect(sendMidiBtn, &QPushButton::clicked, this, &SysExWindow::sendMidiSysEx);
    midiLayout->addRow("", sendMidiBtn);
    
    rightLayout->addWidget(midiBox);

    layout->addWidget(rightBox, 1);
    return widget;
}

QWidget* SysExWindow::createDecoderTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    
    auto *inputBox = new QGroupBox("Hexadecimal Message Input");
    auto *inputLayout = new QVBoxLayout(inputBox);
    hexInput = new QTextEdit();
    connect(hexInput, &QTextEdit::textChanged, this, &SysExWindow::parseHex);
    inputLayout->addWidget(hexInput);
    
    decoderOutput = new QLabel("Paste a SysEx hex message above to view detailed decoding.");
    decoderOutput->setWordWrap(true);
    decoderOutput->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    decoderOutput->setStyleSheet("font-family: monospace;");
    
    layout->addWidget(inputBox, 1);
    layout->addWidget(decoderOutput, 2);
    return widget;
}

void SysExWindow::updateFieldVisibility() {
    int idx = typeCombo->currentIndex();
    auto setVisible = [this](QWidget *w, bool v) {
        w->setVisible(v);
        if(formLayout->labelForField(w)) formLayout->labelForField(w)->setVisible(v);
    };
    auto *unknownLabel = qobject_cast<QLabel *>(formLayout->labelForField(unknownSpin));
    
    // Hide all
    setVisible(partSpin, false); setVisible(channelSpin, false); setVisible(unknownSpin, false);
    setVisible(partOnCheck, false); setVisible(reverbCheck, false); setVisible(prioritySpin, false);
    setVisible(volumeSpin, false); setVisible(panSpin, false); setVisible(percussionCheck, false);
    setVisible(transposeSpin, false); setVisible(detuneSpin, false); setVisible(pitchbendSpin, false);
    setVisible(programSpin, false); setVisible(paramSpin, false); setVisible(paramValueSpin, false);
    setVisible(hookCmdSpin, false); setVisible(targetTrackSpin, false); setVisible(targetBeatSpin, false);
    setVisible(targetTickSpin, false); setVisible(hookRelativeCheck, false); setVisible(hookValueSpin, false);
    setVisible(markerValueSpin, false); setVisible(loopCountSpin, false); setVisible(loopToBeatSpin, false);
    setVisible(loopToTickSpin, false); setVisible(loopFromBeatSpin, false); setVisible(loopFromTickSpin, false);
    setVisible(instrumentIdSpin, false); setVisible(adlibEditorWidget, false);

    // Show appropriate fields
    if (idx == 0) { // Allocate
        unknownSpin->setRange(0, 15);
        if (unknownLabel) unknownLabel->setText("Unknown nibble (0-15):");
        setVisible(partSpin, true); setVisible(unknownSpin, true);
        setVisible(partOnCheck, true); setVisible(reverbCheck, true);
        setVisible(prioritySpin, true); setVisible(volumeSpin, true); setVisible(panSpin, true);
        setVisible(percussionCheck, true); setVisible(transposeSpin, true); setVisible(detuneSpin, true);
        setVisible(pitchbendSpin, true); setVisible(programSpin, true);
    } else {
        unknownSpin->setRange(0, 127);
        if (unknownLabel) unknownLabel->setText("Unknown (0-127):");
    }

    if (idx == 1) { // Shutdown
        setVisible(partSpin, true);
    } else if (idx == 3) { // AdLib Part
        setVisible(partSpin, true); setVisible(unknownSpin, true); setVisible(adlibEditorWidget, true);
    } else if (idx == 4) { // AdLib Global
        setVisible(unknownSpin, true); setVisible(hookValueSpin, true); setVisible(programSpin, true); setVisible(adlibEditorWidget, true);
    } else if (idx == 5) { // Param Adjust
        setVisible(partSpin, true); setVisible(unknownSpin, true); setVisible(paramSpin, true); setVisible(paramValueSpin, true);
    } else if (idx == 6) { // Hook Jump
        setVisible(unknownSpin, true); setVisible(hookCmdSpin, true);
        setVisible(targetTrackSpin, true); setVisible(targetBeatSpin, true); setVisible(targetTickSpin, true);
    } else if (idx == 7) { // Hook Global Transpose
        setVisible(unknownSpin, true); setVisible(hookCmdSpin, true); setVisible(hookRelativeCheck, true); setVisible(hookValueSpin, true);
    } else if (idx == 8 || idx == 9 || idx == 10) { // Hook OnOff/Vol/Prog
        setVisible(channelSpin, true); setVisible(hookCmdSpin, true); setVisible(hookValueSpin, true);
    } else if (idx == 11) { // Hook Transpose
        setVisible(channelSpin, true); setVisible(hookCmdSpin, true); setVisible(hookRelativeCheck, true); setVisible(hookValueSpin, true);
    } else if (idx == 12) { // Marker
        setVisible(unknownSpin, true); setVisible(markerValueSpin, true);
    } else if (idx == 13) { // Set Loop
        setVisible(unknownSpin, true); setVisible(loopCountSpin, true); setVisible(loopToBeatSpin, true); setVisible(loopToTickSpin, true);
        setVisible(loopFromBeatSpin, true); setVisible(loopFromTickSpin, true);
    } else if (idx == 15) { // Set Instrument
        setVisible(channelSpin, true); setVisible(instrumentIdSpin, true);
    }
}

std::vector<uint8_t> SysExWindow::encodeNibbles(const std::vector<uint8_t>& bytes) {
    std::vector<uint8_t> nibbles;
    for (uint8_t b : bytes) {
        nibbles.push_back((b >> 4) & 0x0F);
        nibbles.push_back(b & 0x0F);
    }
    return nibbles;
}

std::vector<uint8_t> SysExWindow::parseHexBytes(const QString& hex) {
    QString clean = hex;
    clean.remove(" "); clean.remove(","); clean.remove("\n"); clean.remove("\r");
    std::vector<uint8_t> res;
    for (int i = 0; i < clean.length(); i += 2) {
        bool ok;
        res.push_back(clean.mid(i, 2).toUInt(&ok, 16));
    }
    return res;
}

std::vector<uint8_t> SysExWindow::generateAllocatePart() {
    std::vector<uint8_t> params;
    uint8_t b0 = static_cast<uint8_t>(((unknownSpin->value() & 0x0F) << 4) |
                                      (partOnCheck->isChecked() ? 0x01 : 0) |
                                      (reverbCheck->isChecked() ? 0x02 : 0));
    params.push_back(b0);
    params.push_back(prioritySpin->value() & 0xFF);
    params.push_back(volumeSpin->value() & 0xFF);
    params.push_back(static_cast<uint8_t>(static_cast<int8_t>(panSpin->value())));
    uint8_t b4 = (percussionCheck->isChecked() ? 0x80 : 0) | ((int8_t)transposeSpin->value() & 0x7F);
    params.push_back(b4);
    params.push_back((int8_t)detuneSpin->value());
    params.push_back(pitchbendSpin->value() & 0xFF);
    params.push_back(programSpin->value() & 0xFF);

    std::vector<uint8_t> nibs = encodeNibbles(params);
    std::vector<uint8_t> res = {0x7D, 0x00, static_cast<uint8_t>(partSpin->value() & 0x0F)};
    res.insert(res.end(), nibs.begin(), nibs.end());
    return res;
}

void SysExWindow::updateGeneratedHex() {
    std::vector<uint8_t> bytes;
    int idx = typeCombo->currentIndex();

    if (idx == 0) { // Allocate
        bytes = generateAllocatePart();
    } else if (idx == 1) { // Shutdown
        bytes = {0x7D, 0x01, static_cast<uint8_t>(partSpin->value() & 0x0F)};
    } else if (idx == 2) { // Start Song
        bytes = {0x7D, 0x02};
    } else if (idx == 3) { // AdLib Part
        auto regs = parseHexBytes(adlibHexEdit->toPlainText());
        auto nibs = encodeNibbles(regs);
        bytes = {0x7D, 0x10, static_cast<uint8_t>(partSpin->value() & 0x0F), static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 4) { // AdLib Global
        auto regs = parseHexBytes(adlibHexEdit->toPlainText());
        auto nibs = encodeNibbles(regs);
        bytes = {0x7D, 0x11, static_cast<uint8_t>(unknownSpin->value() & 0x7F), static_cast<uint8_t>(hookValueSpin->value() & 0x7F), static_cast<uint8_t>(programSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 5) { // Parameter Adjust
        auto nibs = encodeNibbles({static_cast<uint8_t>((paramSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(paramSpin->value() & 0xFF),
                                  static_cast<uint8_t>((paramValueSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(paramValueSpin->value() & 0xFF)});
        bytes = {0x7D, 0x21, static_cast<uint8_t>(partSpin->value() & 0x0F), static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 6) { // Hook Jump
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF),
                                   static_cast<uint8_t>((targetTrackSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(targetTrackSpin->value() & 0xFF),
                                   static_cast<uint8_t>((targetBeatSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(targetBeatSpin->value() & 0xFF),
                                   static_cast<uint8_t>((targetTickSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(targetTickSpin->value() & 0xFF)});
        bytes = {0x7D, 0x30, static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 7) { // Hook Global Transpose
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF), static_cast<uint8_t>(hookRelativeCheck->isChecked() ? 1 : 0), static_cast<uint8_t>((int8_t)hookValueSpin->value())});
        bytes = {0x7D, 0x31, static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 8) { // Hook Part OnOff
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF), static_cast<uint8_t>(hookValueSpin->value() & 0xFF)});
        bytes = {0x7D, 0x32, static_cast<uint8_t>(channelSpin->value() & 0x0F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 9) { // Hook Set Volume
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF), static_cast<uint8_t>(hookValueSpin->value() & 0xFF)});
        bytes = {0x7D, 0x33, static_cast<uint8_t>(channelSpin->value() & 0x0F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 10) { // Hook Set Program
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF), static_cast<uint8_t>(hookValueSpin->value() & 0xFF)});
        bytes = {0x7D, 0x34, static_cast<uint8_t>(channelSpin->value() & 0x0F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 11) { // Hook Transpose
        auto nibs = encodeNibbles({static_cast<uint8_t>(hookCmdSpin->value() & 0xFF), static_cast<uint8_t>(hookRelativeCheck->isChecked() ? 1 : 0), static_cast<uint8_t>((int8_t)hookValueSpin->value())});
        bytes = {0x7D, 0x35, static_cast<uint8_t>(channelSpin->value() & 0x0F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 12) { // Marker
        bytes = {0x7D, 0x40, static_cast<uint8_t>(unknownSpin->value() & 0x7F),
                 static_cast<uint8_t>(markerValueSpin->value() & 0x7F)};
    } else if (idx == 13) { // Set Loop
        auto nibs = encodeNibbles({static_cast<uint8_t>((loopCountSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(loopCountSpin->value() & 0xFF),
                                   static_cast<uint8_t>((loopToBeatSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(loopToBeatSpin->value() & 0xFF),
                                   static_cast<uint8_t>((loopToTickSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(loopToTickSpin->value() & 0xFF),
                                   static_cast<uint8_t>((loopFromBeatSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(loopFromBeatSpin->value() & 0xFF),
                                   static_cast<uint8_t>((loopFromTickSpin->value() >> 8) & 0xFF), static_cast<uint8_t>(loopFromTickSpin->value() & 0xFF)});
        bytes = {0x7D, 0x50, static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        bytes.insert(bytes.end(), nibs.begin(), nibs.end());
    } else if (idx == 14) { // Clear Loop
        bytes = {0x7D, 0x51};
    } else if (idx == 15) { // Set Instrument
        uint16_t instr = instrumentIdSpin->value();
        bytes = {0x7D, 0x60, static_cast<uint8_t>(channelSpin->value() & 0x0F), 
                 static_cast<uint8_t>((instr >> 12) & 0x0F), static_cast<uint8_t>((instr >> 8) & 0x0F),
                 static_cast<uint8_t>((instr >> 4) & 0x0F), static_cast<uint8_t>(instr & 0x0F)};
    } else {
        bytes = {0x7D, 0xFF};
    }

    QString hex = "F0";
    for (uint8_t b : bytes) {
        hex += QString(" %1").arg(b, 2, 16, QChar('0')).toUpper();
    }
    hex += " F7";
    generatedHexDisplay->setPlainText(hex);
}

void SysExWindow::importAdlibSbi() {
    const QString path = QFileDialog::getOpenFileName(this, "Import SBI", QString(), "Sound Blaster Instrument (*.sbi)");
    if (path.isEmpty()) {
        return;
    }

    std::vector<uint8_t> fileBytes;
    QString fileError;
    if (!ReadFileBytes(path, &fileBytes, &fileError)) {
        QMessageBox::warning(this, "Import SBI", QString("Unable to read the SBI file.\n\n%1").arg(fileError));
        return;
    }

    imwrap::gui::SbiTimbre timbre;
    std::string decodeError;
    if (!imwrap::gui::DecodeSbi(fileBytes, &timbre, &decodeError)) {
        QMessageBox::warning(this, "Import SBI", QString::fromStdString(decodeError));
        return;
    }

    QStringList parts;
    for (uint8_t byte : timbre.imuseInstrument) {
        parts.append(QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
    }
    adlibHexEdit->setPlainText(parts.join(' '));
}

void SysExWindow::exportAdlibSbi() {
    const std::vector<uint8_t> adlibBytes = parseHexBytes(adlibHexEdit->toPlainText());
    std::vector<uint8_t> sbiBytes;
    std::string encodeError;

    const QString path = QFileDialog::getSaveFileName(this, "Export SBI", "instrument.sbi", "Sound Blaster Instrument (*.sbi)");
    if (path.isEmpty()) {
        return;
    }

    if (!imwrap::gui::EncodeSbi(adlibBytes, QFileInfo(path).completeBaseName().toStdString(), &sbiBytes, &encodeError)) {
        QMessageBox::warning(this, "Export SBI", QString::fromStdString(encodeError));
        return;
    }

    QString fileError;
    if (!WriteFileBytes(path, sbiBytes, &fileError)) {
        QMessageBox::warning(this, "Export SBI", QString("Unable to save the SBI file.\n\n%1").arg(fileError));
        return;
    }

    if (imwrap::gui::ImuseAdlibHasExtendedData(adlibBytes)) {
        QMessageBox::information(this, "Export SBI",
                                 "The SBI file stores the ScummVM-compatible 11-byte OPL core only.\n"
                                 "Extended iMUSE-only AdLib bytes were not exported.");
    }
}

void SysExWindow::parseHex() {
    QString text = hexInput->toPlainText().simplified().remove(" ");
    if (text.isEmpty()) {
        decoderOutput->setText("Paste a SysEx hex message above.");
        return;
    }
    
    auto bytes = parseHexBytes(text);
    if (bytes.size() < 2) {
        decoderOutput->setText("Message too short.");
        return;
    }
    
    int start = 0, end = bytes.size();
    if (bytes[0] == 0xF0) start++;
    if (bytes.back() == 0xF7) end--;
    
    std::vector<uint8_t> payload(bytes.begin() + start, bytes.begin() + end);
    if (payload.empty() || payload[0] != 0x7D) {
        decoderOutput->setText("Missing compatibility manufacturer ID (expected: 7D).");
        return;
    }
    
    imwrap::IMWrapControlEvent event;
    std::string err;
    if (!imwrap::DecodeIMWrapSysex(imwrap::ByteView(payload.data(), payload.size()), &event, &err)) {
        decoderOutput->setText(QString("Decoding error: %1").arg(QString::fromStdString(err)));
        return;
    }
    
    QString desc = QString::fromStdString(imwrap::DescribeIMWrapSysex(event));
    decoderOutput->setText("Decoding successful:\n\n" + desc);
}

void SysExWindow::copyToClipboard() {
    QApplication::clipboard()->setText(generatedHexDisplay->toPlainText());
}

void SysExWindow::populateMidiDevices() {
    if (!midiCombo) return;
    midiCombo->clear();
    midiCombo->addItem("None (Disabled)");

#ifdef Q_OS_WIN
    UINT numDevs = midiOutGetNumDevs();
    for (UINT i = 0; i < numDevs; ++i) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            midiCombo->addItem(QString::fromWCharArray(caps.szPname), i);
        }
    }
#endif

    QSettings settings("imwrap", "IMWrapSysExGui");
    QString savedDevice = settings.value("midi_device").toString();
    if (!savedDevice.isEmpty()) {
        int idx = midiCombo->findText(savedDevice);
        if (idx >= 0) {
            midiCombo->setCurrentIndex(idx);
            return;
        }
    }
    midiCombo->setCurrentIndex(0);
    sendMidiBtn->setEnabled(false);
}

void SysExWindow::onMidiDeviceChanged(int index) {
#ifdef Q_OS_WIN
    if (hMidiOut) {
        midiOutClose(static_cast<HMIDIOUT>(hMidiOut));
        hMidiOut = nullptr;
    }

    QSettings settings("imwrap", "IMWrapSysExGui");
    if (index > 0) {
        UINT devId = midiCombo->itemData(index).toUInt();
        HMIDIOUT hOut = nullptr;
        MMRESULT res = midiOutOpen(&hOut, devId, 0, 0, CALLBACK_NULL);
        if (res == MMSYSERR_NOERROR) {
            hMidiOut = hOut;
            sendMidiBtn->setEnabled(true);
            settings.setValue("midi_device", midiCombo->itemText(index));
        } else {
            sendMidiBtn->setEnabled(false);
        }
    } else {
        sendMidiBtn->setEnabled(false);
        settings.setValue("midi_device", "");
    }
#else
    (void)index;
#endif
}

void SysExWindow::sendMidiSysEx() {
#ifdef Q_OS_WIN
    if (!hMidiOut) return;

    QString hexStr = generatedHexDisplay->toPlainText();
    auto bytes = parseHexBytes(hexStr);
    if (bytes.empty()) return;

    MIDIHDR hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.lpData = reinterpret_cast<LPSTR>(bytes.data());
    hdr.dwBufferLength = bytes.size();
    hdr.dwBytesRecorded = bytes.size();

    HMIDIOUT hOut = static_cast<HMIDIOUT>(hMidiOut);

    if (midiOutPrepareHeader(hOut, &hdr, sizeof(hdr)) == MMSYSERR_NOERROR) {
        if (midiOutLongMsg(hOut, &hdr, sizeof(hdr)) == MMSYSERR_NOERROR) {
            int timeout = 500; // max 500ms
            while (!(hdr.dwFlags & MHDR_DONE) && timeout-- > 0) {
                Sleep(1);
            }
        }
        midiOutUnprepareHeader(hOut, &hdr, sizeof(hdr));
    }
#endif
}

QWidget* SysExWindow::createGuideTab() {
    auto *browser = new QTextBrowser(this);
    browser->setReadOnly(true);
    
    // Set a clean stylesheet for styling headers, code blocks, alerts, etc.
    browser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #fdfdfd;"
        "  color: #333333;"
        "  font-family: 'Segoe UI', Arial, sans-serif;"
        "  font-size: 11pt;"
        "  padding: 15px;"
        "}"
    );

    const char* markdownText = R"MARKDOWN(# iMWrap Composer Guide
## Integration and Usage of SysEx Messages in MOTU Digital Performer

This guide is designed for composers and sound designers who integrate **iMUSE** interactive commands into their MIDI sequences for adventure games (such as those using adventure game engines with the `imwrap-v6` library or ScummVM). It details the structure of iMUSE SysEx messages, main commands, and explains how to use the **iMWrap SysEx Generator** tool by linking it to **MOTU Digital Performer (DP)** via **loopMIDI**.

---

## 1. Understanding the Structure of iMUSE SysEx Messages

All iMUSE messages are transmitted as standard MIDI System Exclusive (SysEx) messages.

### General format of an iMUSE message:
```text
F0 7D [Command_Code] [Logical_Part_ID] [Nibbles_Data...] F7
```

- **`F0`**: Exclusive message start indicator.
- **`7D`**: Manufacturer ID. iMUSE uses the standard ID `0x7D` (reserved for educational and research projects) as its identifier.
- **`Command_Code`**: Specifies the action to perform (e.g., `0x00` for Allocate Part).
- **`Logical_Part_ID`**: Identifies the part (the virtual channel/track of the sequence) targeted by the instruction. Values range from `0x00` to `0x0F` (channels 1 to 16).
- **`Nibbles_Data`**: The parameters associated with the command. iMUSE splits each data byte (8-bit) into **two half-bytes (4-bit nibbles)** transmitted consecutively (most significant first, then least significant), with each nibble occupying the lower 4 bits of a valid MIDI byte.
  *Example*: To send the value `0x5A` (90 in decimal), two bytes are transmitted: `0x05` followed by `0x0A`.
- **`F7`**: End of Exclusive (EOX) indicator.

---

## 2. Table of Most Common iMUSE Commands

### A. Allocate Part (`0x00`)
This command configures and activates a virtual part (iMUSE channel). It initializes the instrument, relative priority, volume, pan, transpose, and pitch bend behavior.
*Parameter nibble encoding format (16 nibbles / 8 decoded bytes)*:
1. **Byte 0**: Bit 0 = Part enabled (1=Yes, 0=No) | Bit 1 = Reverb enabled (1=Yes, 0=No).
2. **Byte 1**: Part priority (0 to 255).
3. **Byte 2**: Part volume (0 to 127).
4. **Byte 3**: Signed pan (-64 to +63, centered at 0).
5. **Byte 4**: Bit 7 = Percussion (1=Yes, 0=No) | Bits 0-6 = Transpose (-127 to +127, encoded in two's complement).
6. **Byte 5**: Detune (-128 to 127).
7. **Byte 6**: Pitch Bend Factor (1 to 12 semitones).
8. **Byte 7**: Program number (Base instrument, 0 to 127).

> *Note*: Our revised engine dynamically applies all these values (volume, pan, etc.) and reloads the custom Roland MT-32 timbre associated with the part, even if the part is already playing.

### B. Shutdown Part (`0x01`)
Deactivates and resets a virtual part. It turns off all active notes on the associated physical channel.
```text
F0 7D 01 [Part_ID] F7
```

### C. Parameter Adjust (`0x21`)
Dynamically changes internal iMUSE parameters or standard MIDI controls for a playing part.

### D. Hook Jump (`0x30`)
Allows inserting a conditional jump (Smart Jump) into the MIDI playback sequence. The game engine can trigger this transition at the end of a measure or beat to synchronize the music with player actions.
```text
F0 7D 30 [Part_ID] [Target Command, Track, Measure, and Tick Nibbles] F7
```

### E. Marker (`0x40`)
Sends a numeric marker (a single byte of data) to the game's script engine to notify it that a specific point in the music has been reached (e.g., to trigger an animation or dialogue on a specific beat).
```text
F0 7D 40 [Part_ID] [1-byte Marker Value] F7
```

### F. Set Loop (`0x50`) & Clear Loop (`0x51`)
Defines or clears a dynamic playback loop on a section of the sequence (specified in measures and ticks).

---

## 3. Configuration and Routing via loopMIDI

To simplify composition and test your SysEx commands in real-time directly within your music project, you can connect the **iMWrap SysEx Generator** tool to **Digital Performer** using virtual MIDI cables.

### Step 1: Create the virtual port in loopMIDI
1. Start the **loopMIDI** software.
2. In the configuration panel, click the `+` button to add a new port.
3. Name it, for example: `iMWrap Generator`.
4. loopMIDI creates a virtual MIDI input and output port visible to all your applications.

### Step 2: Connect the SysEx Generator
1. Launch the SysEx Generator (`imwrap_sysex_gui.exe`).
2. In the **Direct MIDI Send** section (on the right), locate the **Device** drop-down list.
3. Select the created virtual port: `iMWrap Generator`.
4. Now, every click on the **Send SysEx** button will transmit the MIDI message over this virtual channel.

### Step 3: Configure Digital Performer for real-time recording
If you prefer to record your actions or iMUSE commands in real-time in Digital Performer:
1. Open Digital Performer.
2. Go to **Setup > Input Filter...** and ensure the **System Exclusive** checkbox is checked.
3. In your DP project, create a new MIDI track.
4. Set the MIDI input of this track to `iMWrap Generator`.
5. Arm the MIDI track for recording.
6. Start recording in DP, then click **Send SysEx** in the Generator.
7. DP records the generated SysEx message precisely as an event on the track!

---

## 4. Manually Inserting iMUSE SysEx in Digital Performer

If you want to manually type or copy-paste hexadecimal iMUSE messages into your existing MIDI tracks:

1. In the SysEx Generator, configure your sliders to get the desired message.
2. Click the **Copy to Clipboard** button.
3. In Digital Performer, open the **Event List** of the target MIDI track.
4. Place your playback cursor at the exact time position (Measure|Beat|Tick) where the instruction should execute.
5. Insert a new **System Exclusive (SysEx)** event.
6. An edit dialog opens. Double-click the SysEx event to open the editor.
7. **Paste** the hexadecimal text copied from the Generator.
8. Confirm. The message will be executed by the iMUSE engine.

> **Recommendation for Roland MT-32**:
> When using custom Roland timbres, make sure that the Roland SysEx messages (timbre dumps) are placed at least **50 ticks** before the `Allocate Part` command or the first musical notes.
)MARKDOWN";

    browser->setMarkdown(markdownText);
    return browser;
}
