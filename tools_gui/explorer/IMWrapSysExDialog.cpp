#include "IMWrapSysExDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include <iterator>
#include <sstream>

namespace {

struct MessageDescriptor {
    imwrap::IMWrapSysexType type;
    const char* label;
};

constexpr MessageDescriptor kMessages[] = {
    {imwrap::IMWrapSysexType::AllocatePart, "Allocate Part (0x00)"},
    {imwrap::IMWrapSysexType::ShutdownPart, "Shutdown Part (0x01)"},
    {imwrap::IMWrapSysexType::StartSong, "Start Song (0x02)"},
    {imwrap::IMWrapSysexType::AdlibPartInstrument, "AdLib Part Instrument (0x10)"},
    {imwrap::IMWrapSysexType::AdlibGlobalInstrument, "AdLib Global Instrument (0x11)"},
    {imwrap::IMWrapSysexType::ParameterAdjust, "Parameter Adjust (0x21)"},
    {imwrap::IMWrapSysexType::HookJump, "Hook Jump (0x30)"},
    {imwrap::IMWrapSysexType::HookGlobalTranspose, "Hook Global Transpose (0x31)"},
    {imwrap::IMWrapSysexType::HookPartOnOff, "Hook Part On/Off (0x32)"},
    {imwrap::IMWrapSysexType::HookSetVolume, "Hook Set Volume (0x33)"},
    {imwrap::IMWrapSysexType::HookSetProgram, "Hook Set Program (0x34)"},
    {imwrap::IMWrapSysexType::HookSetTranspose, "Hook Set Transpose (0x35)"},
    {imwrap::IMWrapSysexType::Marker, "Marker (0x40)"},
    {imwrap::IMWrapSysexType::SetLoop, "Set Loop (0x50)"},
    {imwrap::IMWrapSysexType::ClearLoop, "Clear Loop (0x51)"},
    {imwrap::IMWrapSysexType::SetInstrument, "Set Instrument (0x60)"},
};

bool IsSignedHookType(imwrap::IMWrapSysexType type) {
    return type == imwrap::IMWrapSysexType::HookGlobalTranspose ||
           type == imwrap::IMWrapSysexType::HookSetTranspose;
}

} // namespace

IMWrapSysExDialog::IMWrapSysExDialog(QWidget* parent)
    : QDialog(parent) {
    setupUi();
    setPositionFieldsVisible(false);
    updateFieldVisibility();
    updatePreview();
}

void IMWrapSysExDialog::setupUi() {
    setWindowTitle("Edit iMWrap SysEx");
    resize(760, 560);

    auto* mainLayout = new QVBoxLayout(this);
    auto* contentLayout = new QHBoxLayout();
    mainLayout->addLayout(contentLayout, 1);

    auto* leftBox = new QGroupBox("iMWrap SysEx Fields");
    formLayout = new QFormLayout(leftBox);

    typeCombo = new QComboBox();
    for (const MessageDescriptor& message : kMessages) {
        typeCombo->addItem(QString::fromUtf8(message.label));
    }
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IMWrapSysExDialog::updateFieldVisibility);
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &IMWrapSysExDialog::updatePreview);
    formLayout->addRow("Type:", typeCombo);

    auto makeSpin = [this](int minValue, int maxValue, int defaultValue = 0) {
        auto* spin = new QSpinBox();
        spin->setRange(minValue, maxValue);
        spin->setValue(defaultValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &IMWrapSysExDialog::updatePreview);
        return spin;
    };

    partSpin = makeSpin(0, 15);
    channelSpin = makeSpin(0, 15);
    unknownSpin = makeSpin(0, 127);

    partOnCheck = new QCheckBox("Part On");
    connect(partOnCheck, &QCheckBox::stateChanged, this, &IMWrapSysExDialog::updatePreview);
    reverbCheck = new QCheckBox("Reverb");
    connect(reverbCheck, &QCheckBox::stateChanged, this, &IMWrapSysExDialog::updatePreview);
    prioritySpin = makeSpin(0, 255, 90);
    volumeSpin = makeSpin(0, 127, 127);
    panSpin = makeSpin(-64, 63, 0);
    percussionCheck = new QCheckBox("Percussion");
    connect(percussionCheck, &QCheckBox::stateChanged, this, &IMWrapSysExDialog::updatePreview);
    transposeSpin = makeSpin(-127, 127, 0);
    detuneSpin = makeSpin(-128, 127, 0);
    pitchbendSpin = makeSpin(0, 255, 2);
    programSpin = makeSpin(0, 127, 0);

    paramSpin = makeSpin(0, 65535, 0);
    paramValueSpin = makeSpin(0, 65535, 0);

    hookCmdSpin = makeSpin(0, 255, 0);
    targetTrackSpin = makeSpin(0, 65535, 0);
    targetBeatSpin = makeSpin(0, 65535, 0);
    targetTickSpin = makeSpin(0, 65535, 0);
    hookRelativeCheck = new QCheckBox("Relative");
    connect(hookRelativeCheck, &QCheckBox::stateChanged, this, &IMWrapSysExDialog::updatePreview);
    hookValueSpin = makeSpin(-128, 255, 0);

    markerHexEdit = new QTextEdit();
    markerHexEdit->setMaximumHeight(56);
    connect(markerHexEdit, &QTextEdit::textChanged, this, &IMWrapSysExDialog::updatePreview);

    loopCountSpin = makeSpin(0, 65535, 0);
    loopToBeatSpin = makeSpin(0, 65535, 0);
    loopToTickSpin = makeSpin(0, 65535, 0);
    loopFromBeatSpin = makeSpin(0, 65535, 0);
    loopFromTickSpin = makeSpin(0, 65535, 0);

    instrumentIdSpin = makeSpin(0, 65535, 0);
    measureSpin = makeSpin(1, 65535, 1);
    beatSpin = makeSpin(1, 256, 1);
    positionTickSpin = makeSpin(0, 65535, 0);

    formLayout->insertRow(0, "Tick:", positionTickSpin);
    formLayout->insertRow(0, "Beat:", beatSpin);
    formLayout->insertRow(0, "Measure:", measureSpin);

    adlibHexEdit = new QTextEdit();
    adlibHexEdit->setMaximumHeight(72);
    connect(adlibHexEdit, &QTextEdit::textChanged, this, &IMWrapSysExDialog::updatePreview);

    formLayout->addRow("Part:", partSpin);
    formLayout->addRow("Channel:", channelSpin);
    formLayout->addRow("Unknown:", unknownSpin);
    formLayout->addRow("", partOnCheck);
    formLayout->addRow("", reverbCheck);
    formLayout->addRow("Priority:", prioritySpin);
    formLayout->addRow("Volume:", volumeSpin);
    formLayout->addRow("Pan:", panSpin);
    formLayout->addRow("", percussionCheck);
    formLayout->addRow("Transpose:", transposeSpin);
    formLayout->addRow("Detune:", detuneSpin);
    formLayout->addRow("Pitchbend:", pitchbendSpin);
    formLayout->addRow("Program:", programSpin);

    formLayout->addRow("Parameter:", paramSpin);
    formLayout->addRow("Value:", paramValueSpin);

    formLayout->addRow("Hook Command:", hookCmdSpin);
    formLayout->addRow("Target Track:", targetTrackSpin);
    formLayout->addRow("Target Beat:", targetBeatSpin);
    formLayout->addRow("Target Tick:", targetTickSpin);
    formLayout->addRow("", hookRelativeCheck);
    formLayout->addRow("Hook Value:", hookValueSpin);

    formLayout->addRow("Marker Bytes:", markerHexEdit);

    formLayout->addRow("Loop Count:", loopCountSpin);
    formLayout->addRow("Loop To Beat:", loopToBeatSpin);
    formLayout->addRow("Loop To Tick:", loopToTickSpin);
    formLayout->addRow("Loop From Beat:", loopFromBeatSpin);
    formLayout->addRow("Loop From Tick:", loopFromTickSpin);

    formLayout->addRow("Instrument ID:", instrumentIdSpin);
    formLayout->addRow("AdLib Bytes:", adlibHexEdit);

    contentLayout->addWidget(leftBox, 1);

    auto* rightBox = new QGroupBox("Generated SysEx Preview");
    auto* rightLayout = new QVBoxLayout(rightBox);
    previewEdit = new QTextEdit();
    previewEdit->setReadOnly(true);
    previewEdit->setStyleSheet("font-family: monospace;");
    rightLayout->addWidget(previewEdit);
    contentLayout->addWidget(rightBox, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &IMWrapSysExDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &IMWrapSysExDialog::reject);
    mainLayout->addWidget(buttons);
}

void IMWrapSysExDialog::setEvent(const imwrap::IMWrapControlEvent& event) {
    typeCombo->setCurrentIndex(comboIndexForType(event.type));

    partSpin->setValue(event.part);
    channelSpin->setValue(event.channel);
    unknownSpin->setValue(event.unknown);
    partOnCheck->setChecked(event.partOn);
    reverbCheck->setChecked(event.reverb);
    prioritySpin->setValue(event.priority);
    volumeSpin->setValue(event.volume);
    panSpin->setValue(event.pan);
    percussionCheck->setChecked(event.percussion);
    transposeSpin->setValue(event.transpose);
    detuneSpin->setValue(event.detune);
    pitchbendSpin->setValue(event.pitchbendFactor);
    programSpin->setValue(event.program);
    paramSpin->setValue(event.param);
    paramValueSpin->setValue(event.paramValue);
    hookCmdSpin->setValue(event.hookCommand);
    targetTrackSpin->setValue(event.targetTrack);
    targetBeatSpin->setValue(event.targetBeat);
    targetTickSpin->setValue(event.targetTick);
    hookRelativeCheck->setChecked(event.relative);
    hookValueSpin->setValue(IsSignedHookType(event.type) ? event.signedValue : event.value);
    markerHexEdit->setPlainText(formatHexBytes(event.markerBytes));
    loopCountSpin->setValue(event.loopCount);
    loopToBeatSpin->setValue(event.loopToBeat);
    loopToTickSpin->setValue(event.loopToTick);
    loopFromBeatSpin->setValue(event.loopFromBeat);
    loopFromTickSpin->setValue(event.loopFromTick);
    instrumentIdSpin->setValue(event.instrument);
    adlibHexEdit->setPlainText(formatHexBytes(event.decodedBytes));

    updateFieldVisibility();
    updatePreview();
}

void IMWrapSysExDialog::setPositionFieldsVisible(bool visible) {
    auto setVisible = [this, visible](QWidget* widget) {
        widget->setVisible(visible);
        if (QLabel* label = qobject_cast<QLabel*>(formLayout->labelForField(widget))) {
            label->setVisible(visible);
        }
    };

    setVisible(measureSpin);
    setVisible(beatSpin);
    setVisible(positionTickSpin);
}

void IMWrapSysExDialog::setPositionMbt(int measure, int beat, int tick) {
    measureSpin->setValue(measure);
    beatSpin->setValue(beat);
    positionTickSpin->setValue(tick);
}

int IMWrapSysExDialog::positionMeasure() const {
    return measureSpin->value();
}

int IMWrapSysExDialog::positionBeat() const {
    return beatSpin->value();
}

int IMWrapSysExDialog::positionTick() const {
    return positionTickSpin->value();
}

void IMWrapSysExDialog::updateFieldVisibility() {
    const imwrap::IMWrapSysexType type = typeForComboIndex(typeCombo->currentIndex());

    auto setVisible = [this](QWidget* widget, bool visible) {
        widget->setVisible(visible);
        if (QLabel* label = qobject_cast<QLabel*>(formLayout->labelForField(widget))) {
            label->setVisible(visible);
        }
    };

    setVisible(partSpin, false);
    setVisible(channelSpin, false);
    setVisible(unknownSpin, false);
    setVisible(partOnCheck, false);
    setVisible(reverbCheck, false);
    setVisible(prioritySpin, false);
    setVisible(volumeSpin, false);
    setVisible(panSpin, false);
    setVisible(percussionCheck, false);
    setVisible(transposeSpin, false);
    setVisible(detuneSpin, false);
    setVisible(pitchbendSpin, false);
    setVisible(programSpin, false);
    setVisible(paramSpin, false);
    setVisible(paramValueSpin, false);
    setVisible(hookCmdSpin, false);
    setVisible(targetTrackSpin, false);
    setVisible(targetBeatSpin, false);
    setVisible(targetTickSpin, false);
    setVisible(hookRelativeCheck, false);
    setVisible(hookValueSpin, false);
    setVisible(markerHexEdit, false);
    setVisible(loopCountSpin, false);
    setVisible(loopToBeatSpin, false);
    setVisible(loopToTickSpin, false);
    setVisible(loopFromBeatSpin, false);
    setVisible(loopFromTickSpin, false);
    setVisible(instrumentIdSpin, false);
    setVisible(adlibHexEdit, false);

    unknownSpin->setRange(type == imwrap::IMWrapSysexType::AllocatePart ? 0 : 0,
                          type == imwrap::IMWrapSysexType::AllocatePart ? 15 : 127);
    hookValueSpin->setRange(IsSignedHookType(type) ? -128 : 0, IsSignedHookType(type) ? 127 : 255);

    switch (type) {
    case imwrap::IMWrapSysexType::AllocatePart:
        setVisible(partSpin, true);
        setVisible(unknownSpin, true);
        setVisible(partOnCheck, true);
        setVisible(reverbCheck, true);
        setVisible(prioritySpin, true);
        setVisible(volumeSpin, true);
        setVisible(panSpin, true);
        setVisible(percussionCheck, true);
        setVisible(transposeSpin, true);
        setVisible(detuneSpin, true);
        setVisible(pitchbendSpin, true);
        setVisible(programSpin, true);
        break;
    case imwrap::IMWrapSysexType::ShutdownPart:
        setVisible(partSpin, true);
        break;
    case imwrap::IMWrapSysexType::AdlibPartInstrument:
        setVisible(partSpin, true);
        setVisible(unknownSpin, true);
        setVisible(adlibHexEdit, true);
        break;
    case imwrap::IMWrapSysexType::AdlibGlobalInstrument:
        setVisible(unknownSpin, true);
        setVisible(hookValueSpin, true);
        setVisible(programSpin, true);
        setVisible(adlibHexEdit, true);
        break;
    case imwrap::IMWrapSysexType::ParameterAdjust:
        setVisible(partSpin, true);
        setVisible(unknownSpin, true);
        setVisible(paramSpin, true);
        setVisible(paramValueSpin, true);
        break;
    case imwrap::IMWrapSysexType::HookJump:
        setVisible(unknownSpin, true);
        setVisible(hookCmdSpin, true);
        setVisible(targetTrackSpin, true);
        setVisible(targetBeatSpin, true);
        setVisible(targetTickSpin, true);
        break;
    case imwrap::IMWrapSysexType::HookGlobalTranspose:
        setVisible(unknownSpin, true);
        setVisible(hookCmdSpin, true);
        setVisible(hookRelativeCheck, true);
        setVisible(hookValueSpin, true);
        break;
    case imwrap::IMWrapSysexType::HookPartOnOff:
    case imwrap::IMWrapSysexType::HookSetVolume:
    case imwrap::IMWrapSysexType::HookSetProgram:
        setVisible(channelSpin, true);
        setVisible(hookCmdSpin, true);
        setVisible(hookValueSpin, true);
        break;
    case imwrap::IMWrapSysexType::HookSetTranspose:
        setVisible(channelSpin, true);
        setVisible(hookCmdSpin, true);
        setVisible(hookRelativeCheck, true);
        setVisible(hookValueSpin, true);
        break;
    case imwrap::IMWrapSysexType::Marker:
        setVisible(unknownSpin, true);
        setVisible(markerHexEdit, true);
        break;
    case imwrap::IMWrapSysexType::SetLoop:
        setVisible(unknownSpin, true);
        setVisible(loopCountSpin, true);
        setVisible(loopToBeatSpin, true);
        setVisible(loopToTickSpin, true);
        setVisible(loopFromBeatSpin, true);
        setVisible(loopFromTickSpin, true);
        break;
    case imwrap::IMWrapSysexType::SetInstrument:
        setVisible(channelSpin, true);
        setVisible(instrumentIdSpin, true);
        break;
    case imwrap::IMWrapSysexType::StartSong:
    case imwrap::IMWrapSysexType::ClearLoop:
    case imwrap::IMWrapSysexType::Unknown:
        break;
    }
}

void IMWrapSysExDialog::updatePreview() {
    imwrap::IMWrapControlEvent event;
    std::string error;
    if (!buildEvent(&event, &error)) {
        previewEdit->setPlainText(QString("Invalid: %1").arg(QString::fromStdString(error)));
        return;
    }

    std::vector<uint8_t> bytes;
    if (!imwrap::EncodeIMWrapSysex(event, &bytes)) {
        previewEdit->setPlainText("Failed to encode event.");
        return;
    }

    previewEdit->setPlainText(formatHexBytes(bytes));
}

void IMWrapSysExDialog::accept() {
    std::string error;
    if (!buildEvent(&resultEvent_, &error)) {
        QMessageBox::warning(this, "Invalid SysEx", QString::fromStdString(error));
        return;
    }
    QDialog::accept();
}

bool IMWrapSysExDialog::buildEvent(imwrap::IMWrapControlEvent* out, std::string* error) const {
    if (!out) {
        if (error) {
            *error = "output event pointer is null";
        }
        return false;
    }

    imwrap::IMWrapControlEvent event;
    event.type = typeForComboIndex(typeCombo->currentIndex());

    switch (event.type) {
    case imwrap::IMWrapSysexType::AllocatePart:
        event.hasPart = true;
        event.part = static_cast<uint8_t>(partSpin->value());
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.partOn = partOnCheck->isChecked();
        event.reverb = reverbCheck->isChecked();
        event.priority = static_cast<uint8_t>(prioritySpin->value());
        event.volume = static_cast<uint8_t>(volumeSpin->value());
        event.pan = static_cast<int8_t>(panSpin->value());
        event.percussion = percussionCheck->isChecked();
        event.transpose = static_cast<int8_t>(transposeSpin->value());
        event.detune = static_cast<int8_t>(detuneSpin->value());
        event.pitchbendFactor = static_cast<uint8_t>(pitchbendSpin->value());
        event.program = static_cast<uint8_t>(programSpin->value());
        break;
    case imwrap::IMWrapSysexType::ShutdownPart:
        event.hasPart = true;
        event.part = static_cast<uint8_t>(partSpin->value());
        break;
    case imwrap::IMWrapSysexType::StartSong:
        break;
    case imwrap::IMWrapSysexType::AdlibPartInstrument:
        event.hasPart = true;
        event.part = static_cast<uint8_t>(partSpin->value());
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.decodedBytes = parseHexBytes(adlibHexEdit->toPlainText());
        break;
    case imwrap::IMWrapSysexType::AdlibGlobalInstrument:
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.value = static_cast<uint8_t>(hookValueSpin->value());
        event.program = static_cast<uint8_t>(programSpin->value());
        event.decodedBytes = parseHexBytes(adlibHexEdit->toPlainText());
        break;
    case imwrap::IMWrapSysexType::ParameterAdjust:
        event.hasPart = true;
        event.part = static_cast<uint8_t>(partSpin->value());
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.param = static_cast<uint16_t>(paramSpin->value());
        event.paramValue = static_cast<uint16_t>(paramValueSpin->value());
        break;
    case imwrap::IMWrapSysexType::HookJump:
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.hookCommand = static_cast<uint8_t>(hookCmdSpin->value());
        event.targetTrack = static_cast<uint16_t>(targetTrackSpin->value());
        event.targetBeat = static_cast<uint16_t>(targetBeatSpin->value());
        event.targetTick = static_cast<uint16_t>(targetTickSpin->value());
        break;
    case imwrap::IMWrapSysexType::HookGlobalTranspose:
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.hookCommand = static_cast<uint8_t>(hookCmdSpin->value());
        event.relative = hookRelativeCheck->isChecked();
        event.signedValue = static_cast<int8_t>(hookValueSpin->value());
        break;
    case imwrap::IMWrapSysexType::HookPartOnOff:
    case imwrap::IMWrapSysexType::HookSetVolume:
    case imwrap::IMWrapSysexType::HookSetProgram:
        event.hasChannel = true;
        event.channel = static_cast<uint8_t>(channelSpin->value());
        event.hookCommand = static_cast<uint8_t>(hookCmdSpin->value());
        event.value = static_cast<uint8_t>(hookValueSpin->value());
        break;
    case imwrap::IMWrapSysexType::HookSetTranspose:
        event.hasChannel = true;
        event.channel = static_cast<uint8_t>(channelSpin->value());
        event.hookCommand = static_cast<uint8_t>(hookCmdSpin->value());
        event.relative = hookRelativeCheck->isChecked();
        event.signedValue = static_cast<int8_t>(hookValueSpin->value());
        break;
    case imwrap::IMWrapSysexType::Marker:
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.markerBytes = parseHexBytes(markerHexEdit->toPlainText());
        if (event.markerBytes.empty()) {
            if (error) {
                *error = "marker payload cannot be empty";
            }
            return false;
        }
        break;
    case imwrap::IMWrapSysexType::SetLoop:
        event.unknown = static_cast<uint8_t>(unknownSpin->value());
        event.loopCount = static_cast<uint16_t>(loopCountSpin->value());
        event.loopToBeat = static_cast<uint16_t>(loopToBeatSpin->value());
        event.loopToTick = static_cast<uint16_t>(loopToTickSpin->value());
        event.loopFromBeat = static_cast<uint16_t>(loopFromBeatSpin->value());
        event.loopFromTick = static_cast<uint16_t>(loopFromTickSpin->value());
        break;
    case imwrap::IMWrapSysexType::ClearLoop:
        break;
    case imwrap::IMWrapSysexType::SetInstrument:
        event.hasChannel = true;
        event.channel = static_cast<uint8_t>(channelSpin->value());
        event.instrument = static_cast<uint16_t>(instrumentIdSpin->value());
        break;
    case imwrap::IMWrapSysexType::Unknown:
        if (error) {
            *error = "unknown SysEx types are not editable with this dialog";
        }
        return false;
    }

    *out = std::move(event);
    return true;
}

QString IMWrapSysExDialog::formatHexBytes(const std::vector<uint8_t>& bytes) {
    QStringList parts;
    for (uint8_t byte : bytes) {
        parts.append(QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
    }
    return parts.join(' ');
}

std::vector<uint8_t> IMWrapSysExDialog::parseHexBytes(const QString& text) {
    QString clean = text;
    clean.remove(' ');
    clean.remove(',');
    clean.remove('\n');
    clean.remove('\r');
    clean.remove('\t');

    std::vector<uint8_t> bytes;
    for (int index = 0; index + 1 < clean.size(); index += 2) {
        bool ok = false;
        const uint8_t value = static_cast<uint8_t>(clean.mid(index, 2).toUInt(&ok, 16));
        if (ok) {
            bytes.push_back(value);
        }
    }
    return bytes;
}

int IMWrapSysExDialog::comboIndexForType(imwrap::IMWrapSysexType type) {
    for (int index = 0; index < static_cast<int>(std::size(kMessages)); ++index) {
        if (kMessages[index].type == type) {
            return index;
        }
    }
    return 0;
}

imwrap::IMWrapSysexType IMWrapSysExDialog::typeForComboIndex(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kMessages))) {
        return imwrap::IMWrapSysexType::AllocatePart;
    }
    return kMessages[index].type;
}
