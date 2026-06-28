#pragma once

#include <QDialog>

#include "imwrap/IMWrapSysex.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QSpinBox;
class QTextEdit;

class IMWrapSysExDialog : public QDialog {
    Q_OBJECT

public:
    explicit IMWrapSysExDialog(QWidget* parent = nullptr);

    void setEvent(const imwrap::IMWrapControlEvent& event);
    imwrap::IMWrapControlEvent event() const { return resultEvent_; }
    void setPositionFieldsVisible(bool visible);
    void setPositionMbt(int measure, int beat, int tick);
    int positionMeasure() const;
    int positionBeat() const;
    int positionTick() const;

private slots:
    void updateFieldVisibility();
    void updatePreview();

protected:
    void accept() override;

private:
    void setupUi();
    bool buildEvent(imwrap::IMWrapControlEvent* out, std::string* error) const;
    static QString formatHexBytes(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> parseHexBytes(const QString& text);
    static int comboIndexForType(imwrap::IMWrapSysexType type);
    static imwrap::IMWrapSysexType typeForComboIndex(int index);

    QComboBox* typeCombo = nullptr;
    QFormLayout* formLayout = nullptr;

    QSpinBox* partSpin = nullptr;
    QSpinBox* channelSpin = nullptr;
    QSpinBox* unknownSpin = nullptr;

    QCheckBox* partOnCheck = nullptr;
    QCheckBox* reverbCheck = nullptr;
    QSpinBox* prioritySpin = nullptr;
    QSpinBox* volumeSpin = nullptr;
    QSpinBox* panSpin = nullptr;
    QCheckBox* percussionCheck = nullptr;
    QSpinBox* transposeSpin = nullptr;
    QSpinBox* detuneSpin = nullptr;
    QSpinBox* pitchbendSpin = nullptr;
    QSpinBox* programSpin = nullptr;

    QSpinBox* paramSpin = nullptr;
    QSpinBox* paramValueSpin = nullptr;

    QSpinBox* hookCmdSpin = nullptr;
    QSpinBox* targetTrackSpin = nullptr;
    QSpinBox* targetBeatSpin = nullptr;
    QSpinBox* targetTickSpin = nullptr;
    QCheckBox* hookRelativeCheck = nullptr;
    QSpinBox* hookValueSpin = nullptr;

    QTextEdit* markerHexEdit = nullptr;

    QSpinBox* loopCountSpin = nullptr;
    QSpinBox* loopToBeatSpin = nullptr;
    QSpinBox* loopToTickSpin = nullptr;
    QSpinBox* loopFromBeatSpin = nullptr;
    QSpinBox* loopFromTickSpin = nullptr;

    QSpinBox* instrumentIdSpin = nullptr;
    QSpinBox* measureSpin = nullptr;
    QSpinBox* beatSpin = nullptr;
    QSpinBox* positionTickSpin = nullptr;

    QTextEdit* adlibHexEdit = nullptr;
    QTextEdit* previewEdit = nullptr;

    imwrap::IMWrapControlEvent resultEvent_;
};
