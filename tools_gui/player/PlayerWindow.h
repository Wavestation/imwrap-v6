#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QTimer>
#include <QSpinBox>
#include <windows.h>
#include <mmsystem.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <string>

#include "imwrap/IMWrapEngine.h"
#include "imwrap/IMWrapSysex.h"
#include "imwrap/ResourceBank.h"
#include "imwrap/MidiSink.h"
#include "AdlibPreviewOutput.h"
#include "MuntPreviewOutput.h"

class QCheckBox;
class QTreeWidget;
class QTreeWidgetItem;

class PlayerWindow : public QMainWindow {
    Q_OBJECT

public:
    PlayerWindow(QWidget *parent = nullptr);
    ~PlayerWindow();

private slots:
    void browseBank();
    void browseXoredBank();
    void loadBank();
    void togglePreview();
    void playSound();
    void stopSound();
    void stopAllSounds();
    void onTimer();
    void applyHook();
    void advanceTicks();

private:
    enum class PreviewBackend {
        None,
        WinMM,
        Adlib,
        Munt
    };

    void setupUi();
    void refreshDevices();
    void updateUiState();
    bool ensurePreviewBackend(imwrap::TargetProfile profile, std::string* error = nullptr);
    void disablePreviewBackend();
    imwrap::IMWrapEngine::CompatibilityProfile currentCompatibilityProfile() const;
    imwrap::IMWrapSysexDialect currentSysexDialect() const;

    QLineEdit *bankPathEdit;
    QComboBox *deviceCombo;
    QComboBox *profileCombo;
    QCheckBox *snmModeCheck;
    QCheckBox *muntModeCheck;
    QTreeWidget *soundTree;
    QPushButton *previewBtn;
    QPushButton *playBtn;
    QPushButton *stopBtn;
    QPushButton *stopAllBtn;
    QLabel *statusLabel;
    
    QTableWidget *eventsTable;
    QTableWidget *activeTable;

    QComboBox *hookClassCombo;
    QSpinBox *hookValueSpin;
    QSpinBox *hookChannelSpin;
    QSpinBox *advanceSpin;

    imwrap::ResourceBank bank;
    imwrap::IMWrapEngine engine;
    
    class WinMMSink : public imwrap::MidiSink {
    public:
        HMIDIOUT hMidiOut = nullptr;
        
        WinMMSink() = default;
        ~WinMMSink();
        bool openDevice(UINT deviceId);
        void closeDevice();
        bool isAvailable() const override;
        void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override;
        void onSysEx(uint16_t soundId, imwrap::ByteView message) override;
    };
    
    WinMMSink midiSink;
    imwrap::gui::AdlibPreviewOutput adlibSink;
    imwrap::gui::MuntPreviewOutput muntSink;
    PreviewBackend previewBackend = PreviewBackend::None;

    QTimer *transportTimer;
    qint64 lastTime;
    double microAccumulator;
    bool previewEnabled;
};
