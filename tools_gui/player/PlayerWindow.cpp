#include "PlayerWindow.h"
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QTreeWidget>
#include <QSettings>
#include <QHeaderView>
#include <QDateTime>
#include <QFormLayout>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <sstream>
#include <QFormLayout>
#include <sstream>
#include <iomanip>
#include "imwrap/IMWrapSysex.h"

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

PlayerWindow::PlayerWindow(QWidget *parent) : QMainWindow(parent), previewEnabled(false), tickAccumulator(0) {
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
    bool preview = settings.value("previewEnabled", false).toBool();
    if (preview) {
        togglePreview();
    }
}

PlayerWindow::~PlayerWindow() {
    QSettings settings("imwrap", "IMWrapPlayerGui");
    settings.setValue("lastBank", bankPathEdit->text());
    settings.setValue("lastDevice", deviceCombo->currentText());
    settings.setValue("lastProfile", profileCombo->currentIndex());
    settings.setValue("previewEnabled", previewEnabled);
}

void PlayerWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    auto *configBox = new QGroupBox("Configuration iMWrap");
    auto *configLayout = new QVBoxLayout(configBox);
    
    auto *bankLayout = new QHBoxLayout();
    bankLayout->addWidget(new QLabel("Fichier Banque (.ims):"));
    bankPathEdit = new QLineEdit();
    bankLayout->addWidget(bankPathEdit);
    auto *browseBankBtn = new QPushButton("Parcourir...");
    connect(browseBankBtn, &QPushButton::clicked, this, &PlayerWindow::browseBank);
    bankLayout->addWidget(browseBankBtn);
    configLayout->addLayout(bankLayout);

    auto *backendLayout = new QHBoxLayout();
    backendLayout->addWidget(new QLabel("Midi Out (WinMM):"));
    deviceCombo = new QComboBox();
    backendLayout->addWidget(deviceCombo);

    backendLayout->addWidget(new QLabel("Type de Rendu:"));
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
    
    previewBtn = new QPushButton("Activer la Préécoute");
    connect(previewBtn, &QPushButton::clicked, this, &PlayerWindow::togglePreview);
    backendLayout->addWidget(previewBtn);
    configLayout->addLayout(backendLayout);

    layout->addWidget(configBox);

    auto *split = new QHBoxLayout();
    
    // Left: Sound List
    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(new QLabel("Sons:"));
    soundTree = new QTreeWidget();
    soundTree->setHeaderHidden(true);
    connect(soundTree, &QTreeWidget::itemSelectionChanged, this, &PlayerWindow::updateUiState);
    leftLayout->addWidget(soundTree);
    split->addLayout(leftLayout, 1);

    // Center: Events
    auto *centerLayout = new QVBoxLayout();
    centerLayout->addWidget(new QLabel("Evénements du Son:"));
    eventsTable = new QTableWidget(0, 3);
    eventsTable->setHorizontalHeaderLabels({"Tick", "Track", "Description"});
    eventsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    centerLayout->addWidget(eventsTable);
    split->addLayout(centerLayout, 2);

    // Right: Active & Controls
    auto *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(new QLabel("Sons Actifs:"));
    activeTable = new QTableWidget(0, 4);
    activeTable->setHorizontalHeaderLabels({"ID", "Track", "Beat", "Tick"});
    activeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rightLayout->addWidget(activeTable);
    
    auto *controlsLayout = new QHBoxLayout();
    playBtn = new QPushButton("▶ Jouer");
    connect(playBtn, &QPushButton::clicked, this, &PlayerWindow::playSound);
    controlsLayout->addWidget(playBtn);
    stopBtn = new QPushButton("⏹ Stopper");
    connect(stopBtn, &QPushButton::clicked, this, &PlayerWindow::stopSound);
    controlsLayout->addWidget(stopBtn);
    stopAllBtn = new QPushButton("⏹ Tout Stopper");
    connect(stopAllBtn, &QPushButton::clicked, this, &PlayerWindow::stopAllSounds);
    controlsLayout->addWidget(stopAllBtn);
    rightLayout->addLayout(controlsLayout);
    
    // Hooks and Advance
    auto *hookBox = new QGroupBox("Contrôle iMWrap");
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
    auto *applyHookBtn = new QPushButton("Appliquer Hook");
    connect(applyHookBtn, &QPushButton::clicked, this, &PlayerWindow::applyHook);

    auto *hookHBox = new QHBoxLayout();
    hookHBox->addWidget(hookClassCombo); hookHBox->addWidget(hookValueSpin); hookHBox->addWidget(hookChannelSpin); hookHBox->addWidget(applyHookBtn);
    hookLayout->addRow("Hook (C/V/Ch):", hookHBox);
    
    advanceSpin = new QSpinBox(); advanceSpin->setRange(0, 99999); advanceSpin->setValue(480);
    auto *advBtn = new QPushButton("Avancer (Ticks)");
    connect(advBtn, &QPushButton::clicked, this, &PlayerWindow::advanceTicks);
    auto *advHBox = new QHBoxLayout();
    advHBox->addWidget(advanceSpin); advHBox->addWidget(advBtn);
    hookLayout->addRow("Avance Manuelle:", advHBox);
    rightLayout->addWidget(hookBox);

    split->addLayout(rightLayout, 2);

    layout->addLayout(split, 1);

    statusLabel = new QLabel("Prêt.");
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
    QString path = QFileDialog::getOpenFileName(this, "Ouvrir IMS", "", "iMWrap Banks (*.ims *.data)");
    if (!path.isEmpty()) {
        bankPathEdit->setText(path);
        loadBank();
    }
}

void PlayerWindow::loadBank() {
    std::string err;
    if (bank.openFromFile(bankPathEdit->text().toStdString(), &err)) {
        statusLabel->setText(QString("Banque chargée: %1").arg(bankPathEdit->text()));
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
    } else {
        statusLabel->setText(QString("Erreur: %1").arg(QString::fromStdString(err)));
    }
}

void PlayerWindow::togglePreview() {
    if (previewEnabled) {
        transportTimer->stop();
        midiSink.closeDevice();
        engine.resetMt32Initialization();
        previewEnabled = false;
        previewBtn->setText("Activer la Préécoute");
        statusLabel->setText("Préécoute désactivée.");
    } else {
        if (deviceCombo->currentIndex() >= 0) {
            UINT devId = deviceCombo->currentData().toUInt();
            if (midiSink.openDevice(devId)) {
                engine.resetMt32Initialization();
                previewEnabled = true;
                previewBtn->setText("Désactiver la Préécoute");
                statusLabel->setText("Préécoute activée.");
                if (engine.targetProfile() == imwrap::TargetProfile::Mt32) {
                    engine.initMt32();
                }
            } else {
                QMessageBox::warning(this, "Erreur", "Impossible d'ouvrir le périphérique MIDI.");
            }
        }
    }
}

void PlayerWindow::playSound() {
    if (soundTree->selectedItems().isEmpty()) return;
    uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
    engine.stopAllSounds();
    engine.startSound(id);
    
    if (previewEnabled && !transportTimer->isActive()) {
        lastTime = QDateTime::currentMSecsSinceEpoch();
        tickAccumulator = 0;
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
    
    double tps = engine.transportTicksPerSecond();
    double ticks = (tps * elapsedSecs) + tickAccumulator;
    uint32_t wholeTicks = static_cast<uint32_t>(ticks);
    tickAccumulator = ticks - wholeTicks;
    
    if (wholeTicks > 0) {
        engine.advanceAll(wholeTicks);
        updateUiState();
    }
    
    if (engine.activeSoundIds().empty()) {
        transportTimer->stop();
    }
}

void PlayerWindow::updateUiState() {
    // Update Events Table
    if (!soundTree->selectedItems().isEmpty()) {
        uint16_t id = soundTree->selectedItems().first()->data(0, Qt::UserRole).toUInt();
        auto res = bank.loadSound(id);
        
        struct SimpleEvent { uint32_t tick; uint16_t track; std::string text; };
        std::vector<SimpleEvent> seqEvents;
        
        imwrap::SmfSequence seq;
        if (res.valid() && imwrap::SmfParser::Parse(res.selectVariant(engine.targetProfile()).smfData, &seq)) {
            uint16_t tIdx = 0;
            for (const auto &trk : seq.tracks) {
                uint32_t absTick = 0;
                for (const auto &evt : trk.events) {
                    absTick += evt.delta;
                    if (evt.type == imwrap::MidiEventType::Meta && evt.metaType == 0x06) {
                        seqEvents.push_back({absTick, tIdx, std::string(evt.payload.begin(), evt.payload.end())});
                    } else if (evt.type == imwrap::MidiEventType::SysEx) {
                        imwrap::IMWrapControlEvent ctrlEvt;
                        if (imwrap::DecodeIMWrapSysex(imwrap::ByteView(evt.payload.data(), evt.payload.size()), &ctrlEvt)) {
                            std::string desc = imwrap::DescribeIMWrapSysex(ctrlEvt);
                            seqEvents.push_back({absTick, tIdx, "iMWrap SysEx: " + desc});
                        } else {
                            // Fallback raw hex for other SysEx
                            std::stringstream ss;
                            ss << "SysEx (raw): ";
                            for (size_t k = 0; k < evt.payload.size() && evt.payload[k] != 0xF7; k++) {
                                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)evt.payload[k] << " ";
                            }
                            seqEvents.push_back({absTick, tIdx, ss.str()});
                        }
                    }
                }
                tIdx++;
            }
        }
        
        eventsTable->setRowCount(seqEvents.size());
        for (size_t i = 0; i < seqEvents.size(); ++i) {
            eventsTable->setItem(i, 0, new QTableWidgetItem(QString::number(seqEvents[i].tick)));
            eventsTable->setItem(i, 1, new QTableWidgetItem(QString::number(seqEvents[i].track)));
            eventsTable->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(seqEvents[i].text)));
        }
    } else {
        eventsTable->setRowCount(0);
    }
    
    // Update Active Sounds Table
    auto activeIds = engine.activeSoundIds();
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
