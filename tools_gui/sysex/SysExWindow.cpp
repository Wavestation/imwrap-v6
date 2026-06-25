#include "SysExWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QSettings>
#include <QTextBrowser>
#include <sstream>
#include <iomanip>
#include "imwrap/IMWrapSysex.h"

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
    tabs->addTab(createGuideTab(), "Guide du Compositeur");
    mainLayout->addWidget(tabs);
}

QWidget* SysExWindow::createGeneratorTab() {
    auto *widget = new QWidget();
    auto *layout = new QHBoxLayout(widget);

    // Left Column: Parameters
    auto *leftBox = new QGroupBox("Type de Message iMWrap");
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
    auto *rightBox = new QGroupBox("SysEx iMWrap Généré");
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
    auto *unknownLabel = qobject_cast<QLabel *>(formLayout->labelForField(unknownSpin));
    
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
    std::vector<uint8_t> params;
    uint8_t b0 = static_cast<uint8_t>(((unknownSpin->value() & 0x0F) << 4) |
                                      (partOnCheck->isChecked() ? 0x01 : 0) |
                                      (reverbCheck->isChecked() ? 0x02 : 0));
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
        decoderOutput->setText("ID manufacturier de compatibilité absent (valeur attendue : 7D).");
        return;
    }
    
    imwrap::IMWrapControlEvent event;
    std::string err;
    if (!imwrap::DecodeIMWrapSysex(imwrap::ByteView(payload.data(), payload.size()), &event, &err)) {
        decoderOutput->setText(QString("Erreur de décodage: %1").arg(QString::fromStdString(err)));
        return;
    }
    
    QString desc = QString::fromStdString(imwrap::DescribeIMWrapSysex(event));
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

    const char* markdownText = R"MARKDOWN(# Guide du Compositeur iMWrap
## Intégration et Utilisation des Messages SysEx dans MOTU Digital Performer

Ce guide est destiné aux compositeurs et concepteurs sonores qui intègrent des instructions interactives **iMUSE** dans leurs séquences MIDI de jeux d'aventure (comme dans les moteurs de jeux d'aventure utilisant la librairie `imwrap-v6` ou ScummVM). Il détaille la structure des messages SysEx iMUSE, les principales commandes, et explique comment utiliser l'outil **Générateur de SysEx iMWrap** en le reliant à **MOTU Digital Performer (DP)** via **loopMIDI**.

---

## 1. Comprendre la Structure des Messages SysEx iMUSE

Tous les messages iMUSE sont transmis sous la forme de messages système exclusifs (SysEx) standard MIDI.

### Format général d'un message iMUSE :
```text
F0 7D [Code_Commande] [ID_Partie_Logique] [Données_en_Nibbles...] F7
```

- **`F0`** : Indicateur de début de message exclusif.
- **`7D`** : ID Manufacturier. iMUSE utilise l'ID standard `0x7D` (réservé aux projets éducatifs et de recherche) comme identifiant.
- **`Code_Commande`** : Spécifie l'action à réaliser (ex: `0x00` pour Allocate Part).
- **`ID_Partie_Logique`** : Identifie la partie (le canal virtuel/track de la séquence) ciblée par l'instruction. Les valeurs vont de `0x00` à `0x0F` (canaux 1 à 16).
- **`Données_en_Nibbles`** : Les paramètres associés à la commande. iMUSE sépare chaque octet de données (8 bits) en **deux demi-octets (nibles de 4 bits)** transmis consécutivement (poids fort puis poids faible), chaque nibble occupant les 4 bits inférieurs d'un octet MIDI valide.
  *Exemple* : Pour envoyer la valeur `0x5A` (90 en décimal), on transmettra deux octets : `0x05` suivi de `0x0A`.
- **`F7`** : Indicateur de fin de message exclusif (EOX).

---

## 2. Tableau des Commandes iMUSE les plus Courantes

### A. Allocate Part (`0x00`)
Cette commande configure et active une partie virtuelle (canal iMUSE). Elle initialise l'instrument, la priorité relative, le volume, la panoramique, la transposition et le comportement du pitch bend.
*Format d'encodage des nibbles de paramètres (16 nibbles / 8 octets décodés)* :
1. **Octet 0** : Bit 0 = Partie active (1=Oui, 0=Non) | Bit 1 = Reverb active (1=Oui, 0=Non).
2. **Octet 1** : Priorité de la partie (0 à 255).
3. **Octet 2** : Volume de la partie (0 à 127).
4. **Octet 3** : Panoramique (0 à 127, centré à 64).
5. **Octet 4** : Bit 7 = Percussion (1=Oui, 0=Non) | Bits 0-6 = Transposition (-127 à +127, encodée en complément à deux).
6. **Octet 5** : Désaccordage (Detune, -128 à 127).
7. **Octet 6** : Facteur de Pitch Bend (1 à 12 demi-tons).
8. **Octet 7** : Numéro de programme (Instrument de base, 0 à 127).

> *Note* : Notre moteur révisé applique dynamiquement toutes ces valeurs (volume, pan, etc.) et recharge le timbre personnalisé Roland MT-32 associé à la partie, même si celle-ci est déjà en cours de lecture.

### B. Shutdown Part (`0x01`)
Désactive et réinitialise une partie virtuelle. Elle éteint toutes les notes actives sur le canal physique associé.
```text
F0 7D 01 [ID_Partie] F7
```

### C. Parameter Adjust (`0x21`)
Modifie dynamiquement les paramètres iMUSE internes ou les contrôles MIDI standard pour une partie en cours de lecture.

### D. Hook Jump (`0x30`)
Permet d'insérer un saut conditionnel (Smart Jump) dans la séquence de lecture MIDI. Le moteur de jeu peut déclencher cette transition à la fin d'une mesure ou d'un battement pour synchroniser la musique avec les actions du joueur.
```text
F0 7D 30 [ID_Partie] [Nibbles de Commande, Piste, Mesure et Tick cible] F7
```

### E. Marker (`0x40`)
Envoie un marqueur textuel au moteur de script du jeu pour lui notifier qu'un point précis de la musique a été atteint (ex: déclencher une animation ou un dialogue sur un battement précis).
```text
F0 7D 40 [ID_Partie] [Texte du Marqueur en ASCII] F7
```

### F. Set Loop (`0x50`) & Clear Loop (`0x51`)
Définit ou annule une boucle de lecture dynamique sur une section de la séquence (indiquée en mesures et ticks).

---

## 3. Configuration et Routage via loopMIDI

Pour simplifier la composition et tester en temps réel vos commandes SysEx directement dans votre projet de musique, vous pouvez relier l'outil **Générateur de SysEx iMWrap** à **Digital Performer** en passant par des câbles MIDI virtuels.

### Étape 1 : Créer le port virtuel dans loopMIDI
1. Lancez le logiciel **loopMIDI**.
2. Dans le panneau de configuration, cliquez sur le bouton `+` pour ajouter un nouveau port.
3. Nommez-le par exemple : `iMWrap Generator`.
4. loopMIDI crée un port d'entrée et de sortie MIDI virtuel visible par toutes vos applications.

### Étape 2 : Connecter le Générateur de SysEx
1. Lancez le Générateur de SysEx (`imwrap_sysex_gui.exe`).
2. Dans la section **Envoi MIDI Direct** (à droite), repérez la liste déroulante **Périphérique**.
3. Sélectionnez le port virtuel créé : `iMWrap Generator`.
4. Désormais, chaque clic sur le bouton **Envoyer le SysEx** transmettra la trame MIDI sur ce canal virtuel.

### Étape 3 : Configurer Digital Performer pour l'enregistrement en temps réel
Si vous préférez enregistrer en temps réel vos manipulations ou vos commandes iMUSE dans Digital Performer :
1. Ouvrez Digital Performer.
2. Allez dans **Setup > Input Filter...** et assurez-vous que la case **System Exclusive** est bien cochée.
3. Dans votre projet DP, créez une nouvelle piste MIDI.
4. Réglez l'entrée MIDI de cette piste sur `iMWrap Generator`.
5. Armez la piste MIDI en enregistrement.
6. Lancez l'enregistrement dans DP, puis cliquez sur **Envoyer le SysEx** dans le Générateur.
7. DP enregistre précisément le message SysEx généré sous la forme d'un événement dans la piste !

---

## 4. Insérer manuellement des SysEx iMUSE dans Digital Performer

Si vous souhaitez saisir ou copier-coller manuellement les messages hexadécimaux iMUSE dans vos pistes MIDI existantes :

1. Dans le Générateur de SysEx, configurez vos curseurs pour obtenir le message désiré.
2. Cliquez sur le bouton **Copier dans le presse-papiers**.
3. Dans Digital Performer, ouvrez l'**Event List** de la piste MIDI ciblée.
4. Placez votre curseur de lecture à la position temporelle exacte (Mesure|Temps|Tick) où l'instruction doit s'exécuter.
5. Insérez un nouvel événement de type **System Exclusive (SysEx)**.
6. Une boîte de dialogue d'édition s'ouvre. Double-cliquez sur l'événement SysEx pour ouvrir l'éditeur.
7. **Collez** le texte hexadécimal copié depuis le Générateur.
8. Validez. Le message sera exécuté par le moteur iMUSE.

> **Recommandation pour le Roland MT-32** :
> Lorsque vous utilisez des timbres personnalisés Roland, veillez à ce que les messages SysEx Roland (les dumps de timbres) soient positionnés au moins **50 ticks** avant la commande `Allocate Part` ou les premières notes de musique.
)MARKDOWN";

    browser->setMarkdown(markdownText);
    return browser;
}
