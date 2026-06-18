#include "PackerWindow.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QHeaderView>
#include <QFormLayout>
#include <fstream>
#include <algorithm>
#include "imuse/SmfSequence.h"

static imuse::SmfTrack mergeTracksToFormat0(const imuse::SmfSequence &seq) {
    struct AbsEvent {
        uint32_t absTick;
        imuse::MidiEvent event;
    };
    std::vector<AbsEvent> allEvents;
    for (const auto &trk : seq.tracks) {
        uint32_t currentTick = 0;
        for (const auto &evt : trk.events) {
            currentTick += evt.delta;
            allEvents.push_back({currentTick, evt});
        }
    }
    
    std::stable_sort(allEvents.begin(), allEvents.end(), [](const AbsEvent &a, const AbsEvent &b) {
        return a.absTick < b.absTick;
    });
    
    imuse::SmfTrack outTrack;
    uint32_t lastTick = 0;
    for (const auto &absEvt : allEvents) {
        imuse::MidiEvent evt = absEvt.event;
        evt.delta = absEvt.absTick - lastTick;
        lastTick = absEvt.absTick;
        outTrack.events.push_back(evt);
    }
    return outTrack;
}

PackerWindow::PackerWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
}

PackerWindow::~PackerWindow() {
}

void PackerWindow::setupUi() {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);

    auto *splitter = new QSplitter(Qt::Horizontal);
    
    // Left: Sound list
    auto *leftWidget = new QWidget();
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->addWidget(new QLabel("Sons:"));
    soundList = new QListWidget();
    connect(soundList, &QListWidget::itemSelectionChanged, this, &PackerWindow::onSoundSelected);
    leftLayout->addWidget(soundList);
    
    auto *leftBtns = new QHBoxLayout();
    auto *addBtn = new QPushButton("+");
    auto *delBtn = new QPushButton("-");
    connect(addBtn, &QPushButton::clicked, this, &PackerWindow::addSound);
    connect(delBtn, &QPushButton::clicked, this, &PackerWindow::deleteSound);
    leftBtns->addWidget(addBtn);
    leftBtns->addWidget(delBtn);
    leftLayout->addLayout(leftBtns);
    
    splitter->addWidget(leftWidget);

    // Right: Tracks and Variant editing
    auto *rightWidget = new QWidget();
    auto *rightLayout = new QVBoxLayout(rightWidget);
    
    // Sound props
    soundPropsWidget = new QWidget();
    auto *soundPropsLayout = new QHBoxLayout(soundPropsWidget);
    soundPropsLayout->setContentsMargins(0, 0, 0, 0);
    soundPropsLayout->addWidget(new QLabel("Nom:"));
    soundNameEdit = new QLineEdit();
    soundPropsLayout->addWidget(soundNameEdit);
    soundPropsLayout->addWidget(new QLabel("ID:"));
    soundIdSpin = new QSpinBox();
    soundIdSpin->setRange(0, 65535);
    soundPropsLayout->addWidget(soundIdSpin);
    applyBtn = new QPushButton("Appliquer Modif. Son");
    connect(applyBtn, &QPushButton::clicked, this, &PackerWindow::applySoundChanges);
    soundPropsLayout->addWidget(applyBtn);
    rightLayout->addWidget(soundPropsWidget);

    // Variant selector
    variantKindCombo = new QComboBox();
    variantKindCombo->addItem("General MIDI (GMD)", QVariant(static_cast<int>(imuse::VariantKind::Gmd)));
    variantKindCombo->addItem("Roland MT-32 (ROL)", QVariant(static_cast<int>(imuse::VariantKind::Rol)));
    variantKindCombo->addItem("AdLib (ADL)", QVariant(static_cast<int>(imuse::VariantKind::Adl)));
    connect(variantKindCombo, &QComboBox::currentIndexChanged, this, &PackerWindow::onVariantKindChanged);
    rightLayout->addWidget(variantKindCombo);

    // Variant Editor
    auto *variantBox = new QGroupBox("Edition Variante");
    auto *vLayout = new QVBoxLayout(variantBox);
    
    includeVariantCheck = new QCheckBox("Inclure cette variante");
    vLayout->addWidget(includeVariantCheck);

    includeMdhdCheck = new QCheckBox("Inclure MDhd");
    vLayout->addWidget(includeMdhdCheck);

    auto *mdhdLayout = new QFormLayout();
    prioritySpin = new QSpinBox(); prioritySpin->setRange(0, 255);
    volumeSpin = new QSpinBox(); volumeSpin->setRange(0, 127);
    panSpin = new QSpinBox(); panSpin->setRange(-128, 127);
    transposeSpin = new QSpinBox(); transposeSpin->setRange(-128, 127);
    detuneSpin = new QSpinBox(); detuneSpin->setRange(-128, 127);
    speedSpin = new QSpinBox(); speedSpin->setRange(0, 255);
    mdhdLayout->addRow("Priorité:", prioritySpin);
    mdhdLayout->addRow("Volume:", volumeSpin);
    mdhdLayout->addRow("Panoramique:", panSpin);
    mdhdLayout->addRow("Transpose:", transposeSpin);
    mdhdLayout->addRow("Detune:", detuneSpin);
    mdhdLayout->addRow("Speed:", speedSpin);
    vLayout->addLayout(mdhdLayout);

    tracksTable = new QTableWidget(0, 3);
    tracksTable->setHorizontalHeaderLabels({"Nom", "Source", "Evénements"});
    tracksTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    vLayout->addWidget(tracksTable);
    
    importBtn = new QPushButton("Importer MIDI...");
    connect(importBtn, &QPushButton::clicked, this, &PackerWindow::importMidi);
    vLayout->addWidget(importBtn);
    
    rightLayout->addWidget(variantBox);
    
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter);

    statusLabel = new QLabel("Nouveau projet.");
    layout->addWidget(statusLabel);

    // Menu bar
    auto *fileMenu = menuBar()->addMenu("&Fichier");
    auto *newAct = fileMenu->addAction("&Nouveau Projet", this, &PackerWindow::newProject);
    newAct->setShortcut(QKeySequence::New);
    auto *openAct = fileMenu->addAction("&Ouvrir...", this, &PackerWindow::openProject);
    openAct->setShortcut(QKeySequence::Open);
    auto *saveAct = fileMenu->addAction("&Enregistrer", this, &PackerWindow::saveProject);
    saveAct->setShortcut(QKeySequence::Save);
    auto *saveAsAct = fileMenu->addAction("Enregistrer &Sous...", this, &PackerWindow::saveProjectAs);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    
    updateVariantUi();
}

void PackerWindow::newProject() {
    projectSounds.clear();
    soundList->clear();
    currentFilePath = "";
    statusLabel->setText("Nouveau projet.");
    updateVariantUi();
}

void PackerWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, "Ouvrir", "", "Fichiers iMUSE (*.ims)");
    if (!path.isEmpty()) {
        loadImsToModel(path.toStdString());
        currentFilePath = path;
    }
}

void PackerWindow::saveProject() {
    if (currentFilePath.isEmpty()) {
        saveProjectAs();
    } else {
        saveModelToIms(currentFilePath.toStdString());
    }
}

void PackerWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(this, "Enregistrer Sous", "banque.ims", "Fichiers iMUSE (*.ims)");
    if (!path.isEmpty()) {
        currentFilePath = path;
        saveModelToIms(path.toStdString());
    }
}

void PackerWindow::loadImsToModel(const std::string &path) {
    imuse::ResourceBank bank;
    std::string err;
    if (!bank.openFromFile(path, &err)) {
        statusLabel->setText(QString("Erreur d'ouverture: %1").arg(QString::fromStdString(err)));
        return;
    }

    projectSounds.clear();
    for (uint16_t id : bank.soundIds()) {
        auto res = bank.loadSound(id);
        ProjectSound ps;
        ps.id = id;
        ps.name = res.name();
        
        std::vector<imuse::VariantKind> kinds = {imuse::VariantKind::Gmd, imuse::VariantKind::Rol, imuse::VariantKind::Adl};
        for (auto k : kinds) {
            if (res.hasVariant(k)) {
                auto view = res.variant(k);
                ProjectVariant pv;
                pv.kind = k;
                pv.includeVariant = true;
                pv.includeMdhd = view.mdhd.present;
                if (pv.includeMdhd) {
                    pv.mdhd.priority = view.mdhd.priority;
                    pv.mdhd.volume = view.mdhd.volume;
                    pv.mdhd.pan = view.mdhd.pan;
                    pv.mdhd.transpose = view.mdhd.transpose;
                    pv.mdhd.detune = view.mdhd.detune;
                    pv.mdhd.speed = view.mdhd.speed;
                }
                
                // Parse MIDI
                imuse::SmfSequence seq;
                if (imuse::SmfParser::Parse(view.smfData, &seq)) {
                    pv.division = seq.division;
                    int tIdx = 0;
                    for (const auto &trk : seq.tracks) {
                        ProjectTrack pt;
                        pt.name = "Piste " + std::to_string(tIdx++);
                        pt.sourceFileName = "Importé";
                        pt.events = trk.events;
                        pv.tracks.push_back(pt);
                    }
                }
                ps.variants.push_back(pv);
            }
        }
        projectSounds.push_back(ps);
    }

    soundList->clear();
    for (const auto &ps : projectSounds) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1: %2").arg(ps.id).arg(QString::fromStdString(ps.name)));
        item->setData(Qt::UserRole, ps.id);
        soundList->addItem(item);
    }
    statusLabel->setText(QString("Projet chargé: %1 sons").arg(projectSounds.size()));
}

void PackerWindow::saveModelToIms(const std::string &path) {
    std::vector<imuse::SoundBankInput> inputs;
    for (const auto &ps : projectSounds) {
        imuse::SoundBankInput sbi;
        sbi.soundId = ps.id;
        sbi.name = ps.name;
        
        for (const auto &pv : ps.variants) {
            if (!pv.includeVariant) continue;
            imuse::VariantSource vs;
            vs.kind = pv.kind;
            vs.includeMdhd = pv.includeMdhd;
            vs.mdhd = pv.mdhd;
            
            imuse::SmfSequence seq;
            seq.format = 2; // iMUSE uses format 2
            seq.division = pv.division;
            seq.trackCount = static_cast<uint16_t>(pv.tracks.size());
            for (const auto &pt : pv.tracks) {
                imuse::SmfTrack t;
                t.events = pt.events;
                seq.tracks.push_back(t);
            }
            
            std::string err;
            if (!imuse::SmfSerializer::Serialize(seq, &vs.smfData, &err)) {
                QMessageBox::warning(this, "Erreur", QString::fromStdString("Erreur sérialisation SMF: " + err));
                return;
            }
            sbi.variants.push_back(vs);
        }
        inputs.push_back(sbi);
    }
    
    imuse::ImsWriter writer;
    std::string err;
    if (writer.writeFile(path, inputs, &err)) {
        statusLabel->setText(QString("Projet sauvegardé: %1").arg(QString::fromStdString(path)));
    } else {
        QMessageBox::critical(this, "Erreur", QString::fromStdString("Erreur sauvegarde: " + err));
    }
}

void PackerWindow::addSound() {
    uint16_t newId = 0;
    for (const auto &ps : projectSounds) {
        if (ps.id >= newId) newId = ps.id + 1;
    }
    ProjectSound ps;
    ps.id = newId;
    ps.name = "NouveauSon";
    projectSounds.push_back(ps);
    
    QListWidgetItem *item = new QListWidgetItem(QString("%1: %2").arg(ps.id).arg(QString::fromStdString(ps.name)));
    item->setData(Qt::UserRole, ps.id);
    soundList->addItem(item);
    soundList->setCurrentItem(item);
}

void PackerWindow::deleteSound() {
    if (soundList->selectedItems().isEmpty()) return;
    int row = soundList->currentRow();
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    
    for (auto it = projectSounds.begin(); it != projectSounds.end(); ++it) {
        if (it->id == id) {
            projectSounds.erase(it);
            break;
        }
    }
    delete soundList->takeItem(row);
}

void PackerWindow::applySoundChanges() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            ps.name = soundNameEdit->text().toStdString();
            ps.id = soundIdSpin->value();
            
            imuse::VariantKind currentKind = static_cast<imuse::VariantKind>(variantKindCombo->currentData().toInt());
            bool found = false;
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    found = true;
                    pv.includeVariant = includeVariantCheck->isChecked();
                    pv.includeMdhd = includeMdhdCheck->isChecked();
                    pv.mdhd.priority = prioritySpin->value();
                    pv.mdhd.volume = volumeSpin->value();
                    pv.mdhd.pan = panSpin->value();
                    pv.mdhd.transpose = transposeSpin->value();
                    pv.mdhd.detune = detuneSpin->value();
                    pv.mdhd.speed = speedSpin->value();
                    break;
                }
            }
            if (!found && includeVariantCheck->isChecked()) {
                ProjectVariant pv;
                pv.kind = currentKind;
                pv.includeVariant = true;
                pv.includeMdhd = includeMdhdCheck->isChecked();
                pv.mdhd.priority = prioritySpin->value();
                pv.mdhd.volume = volumeSpin->value();
                pv.mdhd.pan = panSpin->value();
                pv.mdhd.transpose = transposeSpin->value();
                pv.mdhd.detune = detuneSpin->value();
                pv.mdhd.speed = speedSpin->value();
                ps.variants.push_back(pv);
            }
            
            soundList->selectedItems().first()->setText(QString("%1: %2").arg(ps.id).arg(QString::fromStdString(ps.name)));
            soundList->selectedItems().first()->setData(Qt::UserRole, ps.id);
            break;
        }
    }
}

void PackerWindow::onSoundSelected() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    
    for (const auto &ps : projectSounds) {
        if (ps.id == id) {
            soundNameEdit->setText(QString::fromStdString(ps.name));
            soundIdSpin->setValue(ps.id);
            break;
        }
    }
    updateVariantUi();
}

void PackerWindow::onVariantKindChanged() {
    updateVariantUi();
}

void PackerWindow::updateVariantUi() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imuse::VariantKind currentKind = static_cast<imuse::VariantKind>(variantKindCombo->currentData().toInt());
    
    bool found = false;
    for (const auto &ps : projectSounds) {
        if (ps.id == id) {
            for (const auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    found = true;
                    includeVariantCheck->setChecked(pv.includeVariant);
                    includeMdhdCheck->setChecked(pv.includeMdhd);
                    prioritySpin->setValue(pv.mdhd.priority);
                    volumeSpin->setValue(pv.mdhd.volume);
                    panSpin->setValue(pv.mdhd.pan);
                    transposeSpin->setValue(pv.mdhd.transpose);
                    detuneSpin->setValue(pv.mdhd.detune);
                    speedSpin->setValue(pv.mdhd.speed);
                    
                    tracksTable->setRowCount(pv.tracks.size());
                    for (size_t i = 0; i < pv.tracks.size(); ++i) {
                        tracksTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(pv.tracks[i].name)));
                        tracksTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(pv.tracks[i].sourceFileName)));
                        tracksTable->setItem(i, 2, new QTableWidgetItem(QString::number(pv.tracks[i].events.size())));
                    }
                    break;
                }
            }
            break;
        }
    }
    
    if (!found) {
        includeVariantCheck->setChecked(false);
        includeMdhdCheck->setChecked(false);
        tracksTable->setRowCount(0);
    }
    
    bool hasSelection = !soundList->selectedItems().isEmpty();
    soundPropsWidget->setEnabled(hasSelection);
    variantKindCombo->setEnabled(hasSelection);
    includeVariantCheck->setEnabled(hasSelection);
    includeMdhdCheck->setEnabled(hasSelection);
    prioritySpin->setEnabled(hasSelection);
    volumeSpin->setEnabled(hasSelection);
    panSpin->setEnabled(hasSelection);
    transposeSpin->setEnabled(hasSelection);
    detuneSpin->setEnabled(hasSelection);
    speedSpin->setEnabled(hasSelection);
    importBtn->setEnabled(hasSelection);
}

void PackerWindow::importMidi() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imuse::VariantKind currentKind = static_cast<imuse::VariantKind>(variantKindCombo->currentData().toInt());
    
    QString path = QFileDialog::getOpenFileName(this, "Importer MIDI", "", "Fichiers MIDI (*.mid *.midi)");
    if (path.isEmpty()) return;
    
    std::ifstream in(path.toStdString(), std::ios::binary);
    if (!in) return;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    
    imuse::SmfSequence seq;
    std::string err;
    if (!imuse::SmfParser::Parse(imuse::ByteView(data.data(), data.size()), &seq, &err)) {
        QMessageBox::warning(this, "Erreur", QString::fromStdString("Fichier MIDI invalide: " + err));
        return;
    }
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            ProjectVariant *varPtr = nullptr;
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    varPtr = &pv;
                    break;
                }
            }
            if (!varPtr) {
                ProjectVariant pv;
                pv.kind = currentKind;
                pv.includeVariant = true;
                ps.variants.push_back(pv);
                varPtr = &ps.variants.back();
            }
            
            varPtr->division = seq.division;
            varPtr->tracks.clear();
            
            QFileInfo fi(path);
            if (seq.format == 1) {
                // Flatten format 1 into format 0
                imuse::SmfTrack merged = mergeTracksToFormat0(seq);
                ProjectTrack pt;
                pt.name = fi.baseName().toStdString() + " (Merged)";
                pt.sourceFileName = fi.fileName().toStdString();
                pt.events = merged.events;
                varPtr->tracks.push_back(pt);
            } else {
                int tIdx = 0;
                for (const auto &trk : seq.tracks) {
                    ProjectTrack pt;
                    pt.name = fi.baseName().toStdString() + " (T" + std::to_string(tIdx++) + ")";
                    pt.sourceFileName = fi.fileName().toStdString();
                    pt.events = trk.events;
                    varPtr->tracks.push_back(pt);
                }
            }
            
            updateVariantUi();
            break;
        }
    }
}
