#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <vector>

class SysExWindow : public QMainWindow {
    Q_OBJECT

public:
    SysExWindow(QWidget *parent = nullptr);
    ~SysExWindow() override;

private slots:
    void updateGeneratedHex();
    void parseHex();
    void copyToClipboard();
    void updateFieldVisibility();
    void onMidiDeviceChanged(int index);
    void sendMidiSysEx();

private:
    void populateMidiDevices();
    void setupUi();
    QWidget* createGeneratorTab();
    QWidget* createDecoderTab();

    // Generator UI Elements
    QComboBox *typeCombo;
    
    // Form Layout
    QFormLayout *formLayout;
    QWidget *paramsWidget;

    // Common fields
    QSpinBox *partSpin;
    QSpinBox *channelSpin;
    QSpinBox *unknownSpin;
    
    // Type specific
    QCheckBox *partOnCheck;
    QCheckBox *reverbCheck;
    QSpinBox *prioritySpin;
    QSpinBox *volumeSpin;
    QSpinBox *panSpin;
    QCheckBox *percussionCheck;
    QSpinBox *transposeSpin;
    QSpinBox *detuneSpin;
    QSpinBox *pitchbendSpin;
    QSpinBox *programSpin;
    
    // Parameter Adjust
    QSpinBox *paramSpin;
    QSpinBox *paramValueSpin;
    
    // Hook
    QSpinBox *hookCmdSpin;
    QSpinBox *targetTrackSpin;
    QSpinBox *targetBeatSpin;
    QSpinBox *targetTickSpin;
    QCheckBox *hookRelativeCheck;
    QSpinBox *hookValueSpin;
    
    // Marker
    QLineEdit *markerTextEdit;
    
    // Loop
    QSpinBox *loopCountSpin;
    QSpinBox *loopToBeatSpin;
    QSpinBox *loopToTickSpin;
    QSpinBox *loopFromBeatSpin;
    QSpinBox *loopFromTickSpin;
    
    // Instrument
    QSpinBox *instrumentIdSpin;
    
    // Adlib
    QTextEdit *adlibHexEdit;
    
    // Display
    QTextEdit *generatedHexDisplay;
    
    // Decoder UI Elements
    QTextEdit *hexInput;
    QLabel *decoderOutput;
    
    // MIDI Direct Send Elements
    QComboBox *midiCombo = nullptr;
    QPushButton *sendMidiBtn = nullptr;
    void *hMidiOut = nullptr;

    // Helpers
    std::vector<uint8_t> generateAllocatePart();
    std::vector<uint8_t> encodeNibbles(const std::vector<uint8_t>& bytes);
    std::vector<uint8_t> parseHexBytes(const QString& hex);
};
