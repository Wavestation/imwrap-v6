#include "SysExWindow.h"
#include <QApplication>
#include <QClipboard>
#include <sstream>
#include <iomanip>
#include "imuse/ImuseSysex.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

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
    tabs->addTab(createGeneratorTab(), "Générateur de SysEx");
    tabs->addTab(createDecoderTab(), "Décodeur de SysEx");
    mainLayout->addWidget(tabs);
}

QWidget* SysExWindow::createGeneratorTab() {
    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);

    // Left Column: Parameters
    auto *leftBox = new QGroupBox("Type de Message iMUSE");
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
    
    partOnCheck = new QCheckBox("Partie Active"); partOnCheck->setChecked(true);
    connect(partOnCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    reverbCheck = new QCheckBox("Reverb Active");
    connect(reverbCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    percussionCheck = new QCheckBox("Percussion");
    connect(percussionCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    
    prioritySpin = makeSpin(0, 255, 90);
    volumeSpin = makeSpin(0, 127, 127);
    panSpin = makeSpin(0, 128, 64);
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
    
    hookRelativeCheck = new QCheckBox("Transposition Relative");
    connect(hookRelativeCheck, &QCheckBox::stateChanged, this, &SysExWindow::updateGeneratedHex);
    hookValueSpin = makeSpin(-128, 127);
    
    markerTextEdit = new QLineEdit("Intro");
    connect(markerTextEdit, &QLineEdit::textChanged, this, &SysExWindow::updateGeneratedHex);
    
    loopCountSpin = makeSpin(0, 65535);
    loopToBeatSpin = makeSpin(0, 65535);
    loopToTickSpin = makeSpin(0, 65535);
    loopFromBeatSpin = makeSpin(0, 65535);
    loopFromTickSpin = makeSpin(0, 65535);
    
    instrumentIdSpin = makeSpin(0, 65535);
    
    adlibHexEdit = new QTextEdit();
    adlibHexEdit->setMaximumHeight(60);
    connect(adlibHexEdit, &QTextEdit::textChanged, this, &SysExWindow::updateGeneratedHex);

    formLayout->addRow("Part (0-15):", partSpin);
    formLayout->addRow("Canal MIDI (0-15):", channelSpin);
    formLayout->addRow("Unknown (0-127):", unknownSpin);
    
    formLayout->addRow("", partOnCheck);
    formLayout->addRow("", reverbCheck);
    formLayout->addRow("Priorité:", prioritySpin);
    formLayout->addRow("Volume:", volumeSpin);
    formLayout->addRow("Panoramique:", panSpin);
    formLayout->addRow("", percussionCheck);
    formLayout->addRow("Transposition:", transposeSpin);
    formLayout->addRow("Désaccordage:", detuneSpin);
    formLayout->addRow("Pitchbend Factor:", pitchbendSpin);
    formLayout->addRow("Programme:", programSpin);
    
    formLayout->addRow("Paramètre:", paramSpin);
    formLayout->addRow("Valeur:", paramValueSpin);
    
    formLayout->addRow("Commande Hook:", hookCmdSpin);
    formLayout->addRow("Piste cible:", targetTrackSpin);
    formLayout->addRow("Mesure cible:", targetBeatSpin);
    formLayout->addRow("Tick cible:", targetTickSpin);
    formLayout->addRow("", hookRelativeCheck);
    formLayout->addRow("Valeur Hook:", hookValueSpin);
    
    formLayout->addRow("Texte Marqueur:", markerTextEdit);
    
    formLayout->addRow("Nombre de Boucles:", loopCountSpin);
    formLayout->addRow("Aller à (Mesure):", loopToBeatSpin);
    formLayout->addRow("Aller à (Tick):", loopToTickSpin);
    formLayout->addRow("Depuis (Mesure):", loopFromBeatSpin);
    formLayout->addRow("Depuis (Tick):", loopFromTickSpin);
    
    formLayout->addRow("Instrument ID:", instrumentIdSpin);
    formLayout->addRow("Registres AdLib:", adlibHexEdit);

    layout->addWidget(leftBox, 1);

    // Right Column: Output
    auto *rightBox = new QGroupBox("SysEx iMUSE Généré");
    auto *rightLayout = new QVBoxLayout(rightBox);

    generatedHexDisplay = new QTextEdit();
    generatedHexDisplay->setReadOnly(true);
    generatedHexDisplay->setStyleSheet("font-family: monospace; font-weight: bold; color: purple; font-size: 14pt;");
    rightLayout->addWidget(generatedHexDisplay);

    auto *copyBtn = new QPushButton("Copier dans le presse-papiers");
    connect(copyBtn, &QPushButton::clicked, this, &SysExWindow::copyToClipboard);
    rightLayout->addWidget(copyBtn);

    // Direct MIDI Out GroupBox
    auto *midiBox = new QGroupBox("Envoi MIDI Direct");
    auto *midiLayout = new QFormLayout(midiBox);
    
    midiCombo = new QComboBox();
    midiLayout->addRow("Périphérique :", midiCombo);
    connect(midiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SysExWindow::onMidiDeviceChanged);
    
    sendMidiBtn = new QPushButton("Envoyer le SysEx");
    connect(sendMidiBtn, &QPushButton::clicked, this, &SysExWindow::sendMidiSysEx);
    midiLayout->addRow("", sendMidiBtn);
    
    rightLayout->addWidget(midiBox);

    layout->addWidget(rightBox, 1);
    return widget;
}

QWidget* SysExWindow::createDecoderTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    
    auto *inputBox = new QGroupBox("Saisie du Message Hexadécimal");
    auto *inputLayout = new QVBoxLayout(inputBox);
    hexInput = new QTextEdit();
    connect(hexInput, &QTextEdit::textChanged, this, &SysExWindow::parseHex);
    inputLayout->addWidget(hexInput);
    
    decoderOutput = new QLabel("Collez un SysEx hexadécimal ci-dessus pour afficher le décodage détaillé.");
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
    
    // Hide all
    setVisible(partSpin, false); setVisible(channelSpin, false); setVisible(unknownSpin, false);
    setVisible(partOnCheck, false); setVisible(reverbCheck, false); setVisible(prioritySpin, false);
    setVisible(volumeSpin, false); setVisible(panSpin, false); setVisible(percussionCheck, false);
    setVisible(transposeSpin, false); setVisible(detuneSpin, false); setVisible(pitchbendSpin, false);
    setVisible(programSpin, false); setVisible(paramSpin, false); setVisible(paramValueSpin, false);
    setVisible(hookCmdSpin, false); setVisible(targetTrackSpin, false); setVisible(targetBeatSpin, false);
    setVisible(targetTickSpin, false); setVisible(hookRelativeCheck, false); setVisible(hookValueSpin, false);
    setVisible(markerTextEdit, false); setVisible(loopCountSpin, false); setVisible(loopToBeatSpin, false);
    setVisible(loopToTickSpin, false); setVisible(loopFromBeatSpin, false); setVisible(loopFromTickSpin, false);
    setVisible(instrumentIdSpin, false); setVisible(adlibHexEdit, false);

    // Show appropriate fields
    if (idx == 0) { // Allocate
        setVisible(partSpin, true); setVisible(unknownSpin, true);
        setVisible(partOnCheck, true); setVisible(reverbCheck, true);
        setVisible(prioritySpin, true); setVisible(volumeSpin, true); setVisible(panSpin, true);
        setVisible(percussionCheck, true); setVisible(transposeSpin, true); setVisible(detuneSpin, true);
        setVisible(pitchbendSpin, true); setVisible(programSpin, true);
    } else if (idx == 1) { // Shutdown
        setVisible(partSpin, true);
    } else if (idx == 3) { // AdLib Part
        setVisible(partSpin, true); setVisible(unknownSpin, true); setVisible(adlibHexEdit, true);
    } else if (idx == 4) { // AdLib Global
        setVisible(unknownSpin, true); setVisible(hookValueSpin, true); setVisible(programSpin, true); setVisible(adlibHexEdit, true);
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
        setVisible(unknownSpin, true); setVisible(markerTextEdit, true);
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
    std::vector<uint8_t> payload;
    payload.push_back(partSpin->value() & 0x0F);
    payload.push_back(unknownSpin->value() & 0x7F);

    std::vector<uint8_t> params;
    uint8_t b0 = (partOnCheck->isChecked() ? 0x01 : 0) | (reverbCheck->isChecked() ? 0x02 : 0);
    params.push_back(b0);
    params.push_back(prioritySpin->value() & 0xFF);
    params.push_back(volumeSpin->value() & 0xFF);
    params.push_back(panSpin->value() & 0xFF);
    uint8_t b4 = (percussionCheck->isChecked() ? 0x80 : 0) | ((int8_t)transposeSpin->value() & 0x7F);
    params.push_back(b4);
    params.push_back((int8_t)detuneSpin->value());
    params.push_back(pitchbendSpin->value() & 0xFF);
    params.push_back(programSpin->value() & 0xFF);

    std::vector<uint8_t> nibs = encodeNibbles(params);
    std::vector<uint8_t> res = {0x7D, 0x00};
    res.insert(res.end(), payload.begin(), payload.end());
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
        bytes = {0x7D, 0x40, static_cast<uint8_t>(unknownSpin->value() & 0x7F)};
        auto mTxt = markerTextEdit->text().toUtf8();
        bytes.insert(bytes.end(), mTxt.begin(), mTxt.end());
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

void SysExWindow::parseHex() {
    QString text = hexInput->toPlainText().simplified().remove(" ");
    if (text.isEmpty()) {
        decoderOutput->setText("Collez un SysEx hexadécimal ci-dessus.");
        return;
    }
    
    auto bytes = parseHexBytes(text);
    if (bytes.size() < 2) {
        decoderOutput->setText("Message trop court.");
        return;
    }
    
    int start = 0, end = bytes.size();
    if (bytes[0] == 0xF0) start++;
    if (bytes.back() == 0xF7) end--;
    
    std::vector<uint8_t> payload(bytes.begin() + start, bytes.begin() + end);
    if (payload.empty() || payload[0] != 0x7D) {
        decoderOutput->setText("ID manufacturier iMUSE absent (devrait être 7D).");
        return;
    }
    
    imuse::ImuseControlEvent event;
    std::string err;
    if (!imuse::DecodeImuseSysex(imuse::ByteView(payload.data(), payload.size()), &event, &err)) {
        decoderOutput->setText(QString("Erreur de décodage: %1").arg(QString::fromStdString(err)));
        return;
    }
    
    QString desc = QString::fromStdString(imuse::DescribeImuseSysex(event));
    decoderOutput->setText("Décodage réussi:\n\n" + desc);
}

void SysExWindow::copyToClipboard() {
    QApplication::clipboard()->setText(generatedHexDisplay->toPlainText());
}

void SysExWindow::populateMidiDevices() {
    if (!midiCombo) return;
    midiCombo->clear();
    midiCombo->addItem("Aucun (Désactivé)");

#ifdef Q_OS_WIN
    UINT numDevs = midiOutGetNumDevs();
    for (UINT i = 0; i < numDevs; ++i) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            midiCombo->addItem(QString::fromWCharArray(caps.szPname), i);
        }
    }
#endif
    sendMidiBtn->setEnabled(false);
}

void SysExWindow::onMidiDeviceChanged(int index) {
#ifdef Q_OS_WIN
    if (hMidiOut) {
        midiOutClose(static_cast<HMIDIOUT>(hMidiOut));
        hMidiOut = nullptr;
    }

    if (index > 0) {
        UINT devId = midiCombo->itemData(index).toUInt();
        HMIDIOUT hOut = nullptr;
        MMRESULT res = midiOutOpen(&hOut, devId, 0, 0, CALLBACK_NULL);
        if (res == MMSYSERR_NOERROR) {
            hMidiOut = hOut;
            sendMidiBtn->setEnabled(true);
        } else {
            sendMidiBtn->setEnabled(false);
        }
    } else {
        sendMidiBtn->setEnabled(false);
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
        midiOutLongMsg(hOut, &hdr, sizeof(hdr));
        while (!(hdr.dwFlags & MHDR_DONE)) {
            Sleep(1);
        }
        midiOutUnprepareHeader(hOut, &hdr, sizeof(hdr));
    }
#endif
}
