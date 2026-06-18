#include "MidiConfigWindow.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include <QLocale>
#include <QFileInfoList>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

AppStrings MidiConfigWindow::getStrings() {
    QLocale locale = QLocale::system();
    if (locale.language() == QLocale::French) {
        return {
            "Configuration MIDI iMUSE",
            "Pilote MIDI :",
            "Périphérique MIDI :",
            "Enregistrer",
            "Annuler",
            "FluidSynth (Synthétiseur Logiciel)",
            "AdLib (Émulation FM OPL3)",
            "Hardware General MIDI (Port Physique/Virtuel)",
            "Hardware Roland MT-32 (Port Physique/Virtuel)",
            "Configuration enregistrée avec succès.",
            "Aucun (Désactivé)",
            "Fichier de configuration cible :"
        };
    } else if (locale.language() == QLocale::Spanish) {
        return {
            "Configuración MIDI iMUSE",
            "Controlador MIDI:",
            "Dispositivo MIDI:",
            "Guardar",
            "Cancelar",
            "FluidSynth (Sintetizador de Software)",
            "AdLib (Emulación FM OPL3)",
            "Hardware General MIDI (Puerto Físico/Virtual)",
            "Hardware Roland MT-32 (Puerto Físico/Virtual)",
            "Configuración guardada correctamente.",
            "Ninguno (Desactivado)",
            "Archivo de configuración de destino:"
        };
    } else { // Default English
        return {
            "iMUSE MIDI Configurator",
            "MIDI Driver:",
            "MIDI Device:",
            "Save",
            "Cancel",
            "FluidSynth (Software Synthesizer)",
            "AdLib (FM OPL3 Emulation)",
            "Hardware General MIDI (Physical/Virtual Port)",
            "Hardware Roland MT-32 (Physical/Virtual Port)",
            "Configuration saved successfully.",
            "None (Disabled)",
            "Target Configuration File:"
        };
    }
}

QString MidiConfigWindow::getTargetConfigPath() {
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    QStringList filters;
    filters << "*.exe";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
    QString gameExe;
    for (const QFileInfo &fileInfo : list) {
        QString name = fileInfo.fileName().toLower();
        if (name != "setmidi.exe" && name != "imuse_sysex_gui.exe" && name != "imuse_player_gui.exe" && name != "imuse_packer_gui.exe") {
            gameExe = fileInfo.absoluteFilePath();
            break;
        }
    }
    if (gameExe.isEmpty()) {
        for (const QFileInfo &fileInfo : list) {
            if (fileInfo.fileName().toLower() != "setmidi.exe") {
                gameExe = fileInfo.absoluteFilePath();
                break;
            }
        }
    }
    if (!gameExe.isEmpty()) {
        QFileInfo fi(gameExe);
        return fi.path() + "/" + fi.completeBaseName() + ".imc";
    }
    return appDir + "/imuse.imc";
}

MidiConfigWindow::MidiConfigWindow(QWidget *parent) : QWidget(parent) {
    strings = getStrings();
    configFilePath = getTargetConfigPath();

    setupUI();
    populateMidiDevices();
    loadConfig();

    // Trigger initial state
    onDriverChanged(driverCombo->currentIndex());
}

MidiConfigWindow::~MidiConfigWindow() {
}

void MidiConfigWindow::setupUI() {
    setWindowTitle(strings.windowTitle);
    setMinimumSize(420, 260);

    // Apply Premium Dark Theme Stylesheet
    setStyleSheet(
        "QWidget {"
        "    background-color: #1a1a24;"
        "    color: #e2e8f0;"
        "    font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;"
        "    font-size: 13px;"
        "}"
        "QComboBox {"
        "    background-color: #2d2d3d;"
        "    border: 1px solid #4a5568;"
        "    border-radius: 5px;"
        "    padding: 6px 12px;"
        "    color: #e2e8f0;"
        "    min-height: 22px;"
        "}"
        "QComboBox:hover {"
        "    border: 1px solid #3182ce;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #2d2d3d;"
        "    selection-background-color: #3182ce;"
        "    selection-color: white;"
        "    border: 1px solid #4a5568;"
        "}"
        "QPushButton {"
        "    background-color: #3182ce;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 5px;"
        "    padding: 8px 18px;"
        "    font-weight: bold;"
        "    min-height: 18px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2b6cb0;"
        "}"
        "QPushButton#cancelBtn {"
        "    background-color: #4a5568;"
        "}"
        "QPushButton#cancelBtn:hover {"
        "    background-color: #2d3748;"
        "}"
        "QLabel#targetLabel {"
        "    color: #a0aec0;"
        "    font-size: 11px;"
        "}"
        "QLabel#statusLabel {"
        "    color: #48bb78;"
        "    font-weight: bold;"
        "}"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Driver Selection
    QVBoxLayout *driverLayout = new QVBoxLayout();
    driverLayout->setSpacing(6);
    driverLabel = new QLabel(strings.driverLabel, this);
    driverCombo = new QComboBox(this);
    driverCombo->addItem(strings.driverFluid, 0);
    driverCombo->addItem(strings.driverAdLib, 1);
    driverCombo->addItem(strings.driverHwGM, 2);
    driverCombo->addItem(strings.driverHwMT32, 3);
    driverLayout->addWidget(driverLabel);
    driverLayout->addWidget(driverCombo);
    mainLayout->addLayout(driverLayout);

    // Device Selection
    QVBoxLayout *deviceLayout = new QVBoxLayout();
    deviceLayout->setSpacing(6);
    deviceLabel = new QLabel(strings.deviceLabel, this);
    deviceCombo = new QComboBox(this);
    deviceLayout->addWidget(deviceLabel);
    deviceLayout->addWidget(deviceCombo);
    mainLayout->addLayout(deviceLayout);

    // Spacer
    mainLayout->addStretch();

    // Target configuration path label
    targetLabel = new QLabel(this);
    targetLabel->setObjectName("targetLabel");
    QFileInfo fi(configFilePath);
    targetLabel->setText(strings.targetLabelText + " <b>" + fi.fileName() + "</b>");
    targetLabel->setToolTip(configFilePath);
    mainLayout->addWidget(targetLabel);

    // Status Label
    statusLabel = new QLabel(this);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusLabel);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch();

    saveBtn = new QPushButton(strings.saveBtn, this);
    cancelBtn = new QPushButton(strings.cancelBtn, this);
    cancelBtn->setObjectName("cancelBtn");

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(driverCombo, &QComboBox::currentIndexChanged, this, &MidiConfigWindow::onDriverChanged);
    connect(saveBtn, &QPushButton::clicked, this, &MidiConfigWindow::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &MidiConfigWindow::onCancel);
}

void MidiConfigWindow::populateMidiDevices() {
    deviceCombo->clear();
    deviceCombo->addItem(strings.noDevice);

#ifdef Q_OS_WIN
    UINT numDevs = midiOutGetNumDevs();
    for (UINT i = 0; i < numDevs; ++i) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            deviceCombo->addItem(QString::fromWCharArray(caps.szPname));
        }
    }
#endif
}

void MidiConfigWindow::loadConfig() {
    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.beginGroup("MIDI");
    
    QVariant drvVal = settings.value("Driver");
    if (drvVal.isValid()) {
        int drv = drvVal.toInt();
        int idx = driverCombo->findData(drv);
        if (idx >= 0) {
            driverCombo->setCurrentIndex(idx);
        }
    }

    QVariant devVal = settings.value("Device");
    if (devVal.isValid()) {
        QString dev = devVal.toString();
        int idx = deviceCombo->findText(dev);
        if (idx >= 0) {
            deviceCombo->setCurrentIndex(idx);
        }
    }
    
    settings.endGroup();
}

void MidiConfigWindow::onDriverChanged(int index) {
    int driverType = driverCombo->itemData(index).toInt();
    // Hardware GM (2) or Hardware MT-32 (3) require device selection
    bool isHardware = (driverType == 2 || driverType == 3);
    deviceLabel->setEnabled(isHardware);
    deviceCombo->setEnabled(isHardware);
}

void MidiConfigWindow::onSave() {
    QSettings settings(configFilePath, QSettings::IniFormat);
    settings.beginGroup("MIDI");

    int driverType = driverCombo->itemData(driverCombo->currentIndex()).toInt();
    settings.setValue("Driver", driverType);

    if (driverType == 2 || driverType == 3) {
        settings.setValue("Device", deviceCombo->currentText());
    } else {
        settings.setValue("Device", "");
    }

    settings.endGroup();
    settings.sync();

    statusLabel->setText(strings.statusSaved);
    
    // Close window shortly after saving
    QCoreApplication::exit(0);
}

void MidiConfigWindow::onCancel() {
    close();
}
