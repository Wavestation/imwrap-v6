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

#include "imuse/ImuseEngine.h"
#include "imuse/ResourceBank.h"
#include "imuse/MidiSink.h"

class QTreeWidget;
class QTreeWidgetItem;

class PlayerWindow : public QMainWindow {
    Q_OBJECT

public:
    PlayerWindow(QWidget *parent = nullptr);
    ~PlayerWindow();

private slots:
    void browseBank();
    void loadBank();
    void togglePreview();
    void playSound();
    void stopSound();
    void stopAllSounds();
    void onTimer();
    void applyHook();
    void advanceTicks();

private:
    void setupUi();
    void refreshDevices();
    void updateUiState();

    QLineEdit *bankPathEdit;
    QComboBox *deviceCombo;
    QComboBox *profileCombo;
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

    imuse::ResourceBank bank;
    imuse::ImuseEngine engine;
    
    class WinMMSink : public imuse::MidiSink {
    public:
        HMIDIOUT hMidiOut = nullptr;
        std::thread sysexThread;
        std::mutex sysexMutex;
        std::queue<std::vector<char>*> sysexQueue;
        std::atomic<bool> running;
        
        WinMMSink();
        ~WinMMSink();
        bool openDevice(UINT deviceId);
        void closeDevice();
        void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override;
        void onSysEx(uint16_t soundId, imuse::ByteView message) override;
    };
    
    WinMMSink midiSink;

    QTimer *transportTimer;
    qint64 lastTime;
    double tickAccumulator;
    bool previewEnabled;
};
