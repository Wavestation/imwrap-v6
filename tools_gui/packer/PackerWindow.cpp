#include "PackerWindow.h"
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QHeaderView>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <fstream>
#include <QInputDialog>
#include <algorithm>
#include "imwrap/SmfSequence.h"

static imwrap::SmfTrack mergeTracksToFormat0(const imwrap::SmfSequence &seq) {
    struct AbsEvent {
        uint32_t absTick;
        uint32_t order;
        imwrap::MidiEvent event;
    };
    std::vector<AbsEvent> allEvents;
    uint32_t order = 0;
    for (const auto &trk : seq.tracks) {
        uint32_t currentTick = 0;
        for (const auto &evt : trk.events) {
            currentTick += evt.delta;
            if (evt.type == imwrap::MidiEventType::Meta && evt.metaType == 0x2F) {
                continue;
            }
            allEvents.push_back({currentTick, order++, evt});
        }
    }
    
    std::stable_sort(allEvents.begin(), allEvents.end(), [](const AbsEvent &a, const AbsEvent &b) {
        if (a.absTick != b.absTick) {
            return a.absTick < b.absTick;
        }
        return a.order < b.order;
    });
    
    imwrap::SmfTrack outTrack;
    uint32_t lastTick = 0;
    for (const auto &absEvt : allEvents) {
        imwrap::MidiEvent evt = absEvt.event;
        evt.delta = absEvt.absTick - lastTick;
        lastTick = absEvt.absTick;
        outTrack.events.push_back(evt);
    }
    
    // Append a single clean End of Track event
    imwrap::MidiEvent endEvt;
    endEvt.type = imwrap::MidiEventType::Meta;
    endEvt.status = 0xFF;
    endEvt.metaType = 0x2F;
    endEvt.tick = lastTick;
    endEvt.delta = 0;
    outTrack.events.push_back(endEvt);
    
    return outTrack;
}

static QString formatSoundLabel(const ProjectSound &ps) {
    QStringList present;
    for (const auto &pv : ps.variants) {
        if (pv.includeVariant) {
            if (pv.kind == imwrap::VariantKind::Gmd) present.append("GMD");
            else if (pv.kind == imwrap::VariantKind::Rol) present.append("ROL");
            else if (pv.kind == imwrap::VariantKind::Adl) present.append("ADL");
        }
    }
    QStringList ordered;
    if (present.contains("GMD")) ordered.append("GMD");
    if (present.contains("ROL")) ordered.append("ROL");
    if (present.contains("ADL")) ordered.append("ADL");

    if (!ordered.isEmpty()) {
        return QString("%1: %2 [%3]").arg(ps.id).arg(QString::fromStdString(ps.name)).arg(ordered.join(", "));
    } else {
        return QString("%1: %2").arg(ps.id).arg(QString::fromStdString(ps.name));
    }
}

PackerWindow::PackerWindow(QWidget *parent)
    : QMainWindow(parent) {
    setupUi();
    setAcceptDrops(true);
    updateWindowTitle();
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
    leftLayout->addWidget(new QLabel("Sounds:"));
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
    soundPropsLayout->addWidget(new QLabel("Name:"));
    soundNameEdit = new QLineEdit();
    soundPropsLayout->addWidget(soundNameEdit);
    soundPropsLayout->addWidget(new QLabel("ID:"));
    soundIdSpin = new QSpinBox();
    soundIdSpin->setRange(0, 65535);
    soundPropsLayout->addWidget(soundIdSpin);
    applyBtn = new QPushButton("Apply Sound Changes");
    connect(applyBtn, &QPushButton::clicked, this, &PackerWindow::applySoundChanges);
    soundPropsLayout->addWidget(applyBtn);
    rightLayout->addWidget(soundPropsWidget);

    // Variant selector
    variantKindCombo = new QComboBox();
    variantKindCombo->addItem("General MIDI (GMD)", QVariant(static_cast<int>(imwrap::VariantKind::Gmd)));
    variantKindCombo->addItem("Roland MT-32 (ROL)", QVariant(static_cast<int>(imwrap::VariantKind::Rol)));
    variantKindCombo->addItem("AdLib (ADL)", QVariant(static_cast<int>(imwrap::VariantKind::Adl)));
    connect(variantKindCombo, &QComboBox::currentIndexChanged, this, &PackerWindow::onVariantKindChanged);
    rightLayout->addWidget(variantKindCombo);

    // Variant Editor
    auto *variantBox = new QGroupBox("Variant Editing");
    auto *vLayout = new QVBoxLayout(variantBox);
    
    includeVariantCheck = new QCheckBox("Include this variant");
    vLayout->addWidget(includeVariantCheck);

    includeMdhdCheck = new QCheckBox("Include MDhd");
    vLayout->addWidget(includeMdhdCheck);

    auto *mdhdLayout = new QFormLayout();
    prioritySpin = new QSpinBox(); prioritySpin->setRange(0, 255);
    volumeSpin = new QSpinBox(); volumeSpin->setRange(0, 127);
    panSpin = new QSpinBox(); panSpin->setRange(-128, 127);
    transposeSpin = new QSpinBox(); transposeSpin->setRange(-128, 127);
    detuneSpin = new QSpinBox(); detuneSpin->setRange(-128, 127);
    speedSpin = new QSpinBox(); speedSpin->setRange(0, 255);
    mdhdLayout->addRow("Priority:", prioritySpin);
    mdhdLayout->addRow("Volume:", volumeSpin);
    mdhdLayout->addRow("Pan:", panSpin);
    mdhdLayout->addRow("Transpose:", transposeSpin);
    mdhdLayout->addRow("Detune:", detuneSpin);
    mdhdLayout->addRow("Speed:", speedSpin);
    vLayout->addLayout(mdhdLayout);

    tracksTable = new QTableWidget(0, 3);
    tracksTable->setHorizontalHeaderLabels({"Name", "Source", "Events"});
    tracksTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tracksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tracksTable->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(tracksTable, &QTableWidget::itemSelectionChanged, this, &PackerWindow::onTrackSelectionChanged);
    connect(tracksTable, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
        onTrackSelectionChanged();
    });
    vLayout->addWidget(tracksTable);
    
    importBtn = new QPushButton("Import MIDI...");
    connect(importBtn, &QPushButton::clicked, this, &PackerWindow::importMidi);
    vLayout->addWidget(importBtn);

    exportTrackBtn = new QPushButton("Export MIDI...");
    connect(exportTrackBtn, &QPushButton::clicked, this, &PackerWindow::exportSelectedTrackMidi);
    vLayout->addWidget(exportTrackBtn);

    auto *trackBtnLayout = new QHBoxLayout();
    deleteTrackBtn = new QPushButton("Delete Track");
    connect(deleteTrackBtn, &QPushButton::clicked, this, &PackerWindow::deleteTrack);
    trackBtnLayout->addWidget(deleteTrackBtn);

    moveUpBtn = new QPushButton("Move Up");
    connect(moveUpBtn, &QPushButton::clicked, this, &PackerWindow::moveTrackUp);
    trackBtnLayout->addWidget(moveUpBtn);

    moveDownBtn = new QPushButton("Move Down");
    connect(moveDownBtn, &QPushButton::clicked, this, &PackerWindow::moveTrackDown);
    trackBtnLayout->addWidget(moveDownBtn);

    vLayout->addLayout(trackBtnLayout);
    
    rightLayout->addWidget(variantBox);
    
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter);

    statusLabel = new QLabel("New project.");
    layout->addWidget(statusLabel);

    // Menu bar
    auto *fileMenu = menuBar()->addMenu("&File");
    auto *newAct = fileMenu->addAction("&New Project", this, &PackerWindow::newProject);
    newAct->setShortcut(QKeySequence::New);
    auto *openAct = fileMenu->addAction("&Open...", this, &PackerWindow::openProject);
    openAct->setShortcut(QKeySequence::Open);
    auto *openXorAct = fileMenu->addAction("Open XORed Bank...", this, &PackerWindow::openXoredProject);
    openXorAct->setShortcut(QKeySequence("Ctrl+Shift+O"));
    auto *saveAct = fileMenu->addAction("&Save", this, &PackerWindow::saveProject);
    saveAct->setShortcut(QKeySequence::Save);
    auto *saveAsAct = fileMenu->addAction("Save &As...", this, &PackerWindow::saveProjectAs);
    saveAsAct->setShortcut(QKeySequence("Ctrl+Alt+S"));
    auto *saveAsXorAct = fileMenu->addAction("Save XORed Bank...", this, &PackerWindow::saveProjectAsXored);
    saveAsXorAct->setShortcut(QKeySequence("Ctrl+Shift+S"));
    
    updateVariantUi();
    updateWindowTitle();
}

void PackerWindow::setDirty(bool dirty) {
    if (isDirty_ != dirty) {
        isDirty_ = dirty;
        updateWindowTitle();
    }
}

void PackerWindow::updateWindowTitle() {
    QString title = "iMWrap Packer";
    if (!currentFilePath.isEmpty()) {
        title += " - " + QFileInfo(currentFilePath).fileName();
    }
    if (isDirty_) {
        title += "*";
    }
    setWindowTitle(title);
}

bool PackerWindow::promptSaveIfDirty() {
    if (!isDirty_) return true;

    QMessageBox::StandardButton res = QMessageBox::warning(this, "Unsaved Changes",
        "You have unsaved changes. Do you want to save them now?",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (res == QMessageBox::Cancel) {
        return false;
    }
    if (res == QMessageBox::Yes) {
        saveProject();
        if (isDirty_) return false;
    }
    return true;
}

void PackerWindow::closeEvent(QCloseEvent *event) {
    if (promptSaveIfDirty()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void PackerWindow::newProject() {
    if (!promptSaveIfDirty()) return;
    projectSounds.clear();
    soundList->clear();
    currentFilePath = "";
    statusLabel->setText("New project.");
    updateVariantUi();
    setDirty(false);
}

void PackerWindow::openProject() {
    if (!promptSaveIfDirty()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open", "", "iMWrap Files (*.ims)");
    if (!path.isEmpty()) {
        loadImsToModel(path.toStdString());
        currentFilePath = path;
        setDirty(false);
    }
}

void PackerWindow::openXoredProject() {
    if (!promptSaveIfDirty()) return;
    QString path = QFileDialog::getOpenFileName(this, "Open XORed Bank", "", "iMWrap KOG (*.kog)");
    if (path.isEmpty()) return;

    bool ok;
    int key = QInputDialog::getInt(this, "XOR Encryption", "Enter XOR Key (0-255):", 39, 0, 255, 1, &ok);
    if (!ok) return;

    std::ifstream in(path.toStdString(), std::ios::binary);
    if (!in) {
        QMessageBox::critical(this, "Error", "Failed to open file for reading.");
        return;
    }
    
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        if (key != 0) {
            for (size_t i = 0; i < buffer.size(); ++i) {
                buffer[i] ^= static_cast<uint8_t>(key);
            }
        }
        
        imwrap::ResourceBank bank;
        std::string err;
        if (!bank.openFromMemory(std::move(buffer), &err)) {
            statusLabel->setText(QString("Open error: %1").arg(QString::fromStdString(err)));
            return;
        }

        projectSounds.clear();
        for (uint16_t id : bank.soundIds()) {
            auto res = bank.loadSound(id);
            ProjectSound ps;
            ps.id = id;
            ps.name = res.name();
            
            std::vector<imwrap::VariantKind> kinds = {imwrap::VariantKind::Gmd, imwrap::VariantKind::Rol, imwrap::VariantKind::Adl};
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
                    
                    imwrap::SmfSequence seq;
                    if (imwrap::SmfParser::Parse(view.smfData, &seq)) {
                        pv.division = seq.division;
                        int tIdx = 0;
                        for (const auto &trk : seq.tracks) {
                            ProjectTrack pt;
                            pt.name = "Track " + std::to_string(tIdx++);
                            pt.sourceFileName = "Imported";
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
        for (const auto &ps2 : projectSounds) {
            QListWidgetItem *item = new QListWidgetItem(formatSoundLabel(ps2));
            item->setData(Qt::UserRole, ps2.id);
            soundList->addItem(item);
        }
        if (soundList->count() > 0) {
            soundList->setCurrentRow(0);
        }
        
        currentFilePath = path;
        setDirty(false);
    } else {
        QMessageBox::critical(this, "Error", "Failed to read file.");
    }
}


void PackerWindow::saveProject() {
    if (currentFilePath.isEmpty()) {
        saveProjectAs();
    } else {
        saveModelToIms(currentFilePath.toStdString());
        setDirty(false);
    }
}

void PackerWindow::saveProjectAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save As", "bank.ims", "iMWrap Files (*.ims)");
    if (!path.isEmpty()) {
        currentFilePath = path;
        saveModelToIms(path.toStdString());
        setDirty(false);
    }
}

void PackerWindow::saveProjectAsXored() {
    QString path = QFileDialog::getSaveFileName(this, "Save XORed Bank As", "bank.kog", "iMWrap KOG (*.kog)");
    if (path.isEmpty()) return;

    bool ok;
    int key = QInputDialog::getInt(this, "XOR Encryption", "Enter XOR Key (0-255):", 39, 0, 255, 1, &ok);
    if (!ok) return;

    // We can use a temporary file, then read it, xor it, and write it back.
    // Or we can save to memory, but ImsWriter only has writeFile().
    // So we use a temporary file.
    
    std::string tempPath = path.toStdString() + ".tmp";
    saveModelToIms(tempPath);
    
    std::ifstream in(tempPath, std::ios::binary);
    if (!in) {
        QMessageBox::critical(this, "Error", "Failed to read temp file.");
        return;
    }
    
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        QMessageBox::critical(this, "Error", "Failed to read temp file.");
        in.close();
        return;
    }
    in.close();
    std::remove(tempPath.c_str());
    
    if (key != 0) {
        for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] ^= static_cast<uint8_t>(key);
        }
    }
    
    std::ofstream out(path.toStdString(), std::ios::binary);
    if (!out || !out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size())) {
        QMessageBox::critical(this, "Error", "Failed to write XORed file.");
        return;
    }

    currentFilePath = path;
    setDirty(false);
}

void PackerWindow::loadImsToModel(const std::string &path) {
    imwrap::ResourceBank bank;
    std::string err;
    if (!bank.openFromFile(path, &err)) {
        statusLabel->setText(QString("Open error: %1").arg(QString::fromStdString(err)));
        return;
    }

    projectSounds.clear();
    for (uint16_t id : bank.soundIds()) {
        auto res = bank.loadSound(id);
        ProjectSound ps;
        ps.id = id;
        ps.name = res.name();
        
        std::vector<imwrap::VariantKind> kinds = {imwrap::VariantKind::Gmd, imwrap::VariantKind::Rol, imwrap::VariantKind::Adl};
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
                imwrap::SmfSequence seq;
                if (imwrap::SmfParser::Parse(view.smfData, &seq)) {
                    pv.division = seq.division;
                    int tIdx = 0;
                    for (const auto &trk : seq.tracks) {
                        ProjectTrack pt;
                        pt.name = "Track " + std::to_string(tIdx++);
                        pt.sourceFileName = "Imported";
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
        QListWidgetItem *item = new QListWidgetItem(formatSoundLabel(ps));
        item->setData(Qt::UserRole, ps.id);
        soundList->addItem(item);
    }
    statusLabel->setText(QString("Project loaded: %1 sounds").arg(projectSounds.size()));
}

void PackerWindow::saveModelToIms(const std::string &path) {
    std::vector<imwrap::SoundBankInput> inputs;
    for (const auto &ps : projectSounds) {
        imwrap::SoundBankInput sbi;
        sbi.soundId = ps.id;
        sbi.name = ps.name;
        
        for (const auto &pv : ps.variants) {
            if (!pv.includeVariant) continue;
            imwrap::VariantSource vs;
            vs.kind = pv.kind;
            vs.includeMdhd = pv.includeMdhd;
            vs.mdhd = pv.mdhd;
            
            imwrap::SmfSequence seq;
            seq.format = 2; // iMUSE uses format 2
            seq.division = pv.division;
            seq.trackCount = static_cast<uint16_t>(pv.tracks.size());
            for (const auto &pt : pv.tracks) {
                imwrap::SmfTrack t;
                t.events = pt.events;
                seq.tracks.push_back(t);
            }
            
            std::string err;
            if (!imwrap::SmfSerializer::Serialize(seq, &vs.smfData, &err)) {
                QMessageBox::warning(this, "Error", QString::fromStdString("SMF serialization error: " + err));
                return;
            }
            sbi.variants.push_back(vs);
        }
        inputs.push_back(sbi);
    }
    
    imwrap::ImsWriter writer;
    std::string err;
    if (writer.writeFile(path, inputs, &err)) {
        statusLabel->setText(QString("Project saved: %1").arg(QString::fromStdString(path)));
    } else {
        QMessageBox::critical(this, "Error", QString::fromStdString("Save error: " + err));
    }
}

void PackerWindow::addSound() {
    uint16_t newId = 0;
    for (const auto &ps : projectSounds) {
        if (ps.id >= newId) newId = ps.id + 1;
    }
    ProjectSound ps;
    ps.id = newId;
    ps.name = "NewSound";
    projectSounds.push_back(ps);
    
    QListWidgetItem *item = new QListWidgetItem(formatSoundLabel(ps));
    item->setData(Qt::UserRole, ps.id);
    soundList->addItem(item);
    soundList->setCurrentRow(soundList->count() - 1);
    setDirty(true);
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
    setDirty(true);
}

void PackerWindow::applySoundChanges() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            ps.name = soundNameEdit->text().toStdString();
            ps.id = soundIdSpin->value();
            
            imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
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
            
            soundList->selectedItems().first()->setText(formatSoundLabel(ps));
            soundList->selectedItems().first()->setData(Qt::UserRole, ps.id);
            setDirty(true);
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
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
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
    
    onTrackSelectionChanged();
}

void PackerWindow::importMidi() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
    QStringList paths = QFileDialog::getOpenFileNames(this, "Import MIDI", "", "MIDI Files (*.mid *.midi)");
    if (paths.isEmpty()) return;
    
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
            
            bool firstFile = true;
            
            for (const QString &path : paths) {
                std::ifstream in(path.toStdString(), std::ios::binary);
                if (!in) continue;
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                
                imwrap::SmfSequence seq;
                std::string err;
                if (!imwrap::SmfParser::Parse(imwrap::ByteView(data.data(), data.size()), &seq, &err)) {
                    QMessageBox::warning(this, "Error", QString::fromStdString("Invalid MIDI file (" + path.toStdString() + "): " + err));
                    continue;
                }
                
                if (firstFile && varPtr->tracks.empty()) {
                    varPtr->division = seq.division;
                    firstFile = false;
                }
                
                QFileInfo fi(path);
                if (seq.format == 1) {
                    imwrap::SmfTrack merged = mergeTracksToFormat0(seq);
                    ProjectTrack pt;
                    pt.name = paths.size() == 1 ? fi.baseName().toStdString() + " (Merged)" : fi.baseName().toStdString();
                    pt.sourceFileName = fi.fileName().toStdString();
                    pt.events = merged.events;
                    varPtr->tracks.push_back(pt);
                } else {
                    int tIdx = 0;
                    for (const auto &trk : seq.tracks) {
                        ProjectTrack pt;
                        pt.name = seq.tracks.size() == 1 ? fi.baseName().toStdString() : fi.baseName().toStdString() + " (T" + std::to_string(tIdx++) + ")";
                        pt.sourceFileName = fi.fileName().toStdString();
                        pt.events = trk.events;
                        varPtr->tracks.push_back(pt);
                    }
                }
            }
            
            updateVariantUi();
            soundList->selectedItems().first()->setText(formatSoundLabel(ps));
            setDirty(true);
            break;
        }
    }
}

void PackerWindow::deleteTrack() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
    int row = tracksTable->currentRow();
    if (row < 0) return;
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    if (row < static_cast<int>(pv.tracks.size())) {
                        pv.tracks.erase(pv.tracks.begin() + row);
                        updateVariantUi();
                        setDirty(true);
                        
                        if (!pv.tracks.empty()) {
                            int newRow = std::min(row, static_cast<int>(pv.tracks.size() - 1));
                            tracksTable->selectRow(newRow);
                            tracksTable->setCurrentCell(newRow, 0);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}

void PackerWindow::moveTrackUp() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
    int row = tracksTable->currentRow();
    if (row <= 0) return;
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    if (row < static_cast<int>(pv.tracks.size())) {
                        std::swap(pv.tracks[row], pv.tracks[row - 1]);
                        updateVariantUi();
                        setDirty(true);
                        tracksTable->selectRow(row - 1);
                        tracksTable->setCurrentCell(row - 1, 0);
                    }
                    break;
                }
            }
            break;
        }
    }
}

void PackerWindow::moveTrackDown() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
    int row = tracksTable->currentRow();
    if (row < 0) return;
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    if (row < static_cast<int>(pv.tracks.size() - 1)) {
                        std::swap(pv.tracks[row], pv.tracks[row + 1]);
                        updateVariantUi();
                        setDirty(true);
                        tracksTable->selectRow(row + 1);
                        tracksTable->setCurrentCell(row + 1, 0);
                    }
                    break;
                }
            }
            break;
        }
    }
}

void PackerWindow::onTrackSelectionChanged() {
    bool hasSelection = !soundList->selectedItems().isEmpty();
    bool hasTrackSelected = hasSelection && !tracksTable->selectedItems().isEmpty();
    int row = tracksTable->currentRow();
    exportTrackBtn->setEnabled(hasTrackSelected);
    deleteTrackBtn->setEnabled(hasTrackSelected);
    moveUpBtn->setEnabled(hasTrackSelected && row > 0);
    moveDownBtn->setEnabled(hasTrackSelected && row < tracksTable->rowCount() - 1);
}

void PackerWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        bool hasMidi = false;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                QString path = url.toLocalFile().toLower();
                if (path.endsWith(".mid") || path.endsWith(".midi")) {
                    hasMidi = true;
                    break;
                }
            }
        }
        if (hasMidi) {
            event->acceptProposedAction();
        }
    }
}

void PackerWindow::dropEvent(QDropEvent *event) {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());

    ProjectVariant *varPtr = nullptr;
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
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
            
            bool firstFile = varPtr->tracks.empty();
            for (const QUrl &url : event->mimeData()->urls()) {
                if (!url.isLocalFile()) continue;
                QString path = url.toLocalFile();
                if (!path.toLower().endsWith(".mid") && !path.toLower().endsWith(".midi")) continue;
                
                std::ifstream in(path.toStdString(), std::ios::binary);
                if (!in) continue;
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                
                imwrap::SmfSequence seq;
                std::string err;
                if (!imwrap::SmfParser::Parse(imwrap::ByteView(data.data(), data.size()), &seq, &err)) {
                    QMessageBox::warning(this, "Error", QString::fromStdString("Invalid MIDI file (" + path.toStdString() + "): " + err));
                    continue;
                }
                
                if (firstFile) {
                    varPtr->division = seq.division;
                    firstFile = false;
                }
                
                QFileInfo fi(path);
                if (seq.format == 1) {
                    imwrap::SmfTrack merged = mergeTracksToFormat0(seq);
                    ProjectTrack pt;
                    pt.name = fi.baseName().toStdString();
                    pt.sourceFileName = fi.fileName().toStdString();
                    pt.events = merged.events;
                    varPtr->tracks.push_back(pt);
                } else {
                    int tIdx = 0;
                    for (const auto &trk : seq.tracks) {
                        ProjectTrack pt;
                        pt.name = seq.tracks.size() == 1 ? fi.baseName().toStdString() : fi.baseName().toStdString() + " (T" + std::to_string(tIdx++) + ")";
                        pt.sourceFileName = fi.fileName().toStdString();
                        pt.events = trk.events;
                        varPtr->tracks.push_back(pt);
                    }
                }
            }
            
            updateVariantUi();
            soundList->selectedItems().first()->setText(formatSoundLabel(ps));
            setDirty(true);
            break;
        }
    }
}

void PackerWindow::exportSelectedTrackMidi() {
    if (soundList->selectedItems().isEmpty()) return;
    uint16_t id = soundList->selectedItems().first()->data(Qt::UserRole).toUInt();
    imwrap::VariantKind currentKind = static_cast<imwrap::VariantKind>(variantKindCombo->currentData().toInt());
    
    int row = tracksTable->currentRow();
    if (row < 0) return;
    
    for (auto &ps : projectSounds) {
        if (ps.id == id) {
            for (auto &pv : ps.variants) {
                if (pv.kind == currentKind) {
                    if (row < static_cast<int>(pv.tracks.size())) {
                        ProjectTrack &track = pv.tracks[row];
                        QString path = QFileDialog::getSaveFileName(this, "Export MIDI", QString::fromStdString(track.name) + ".mid", "MIDI Files (*.mid *.midi)");
                        if (path.isEmpty()) return;
                        
                        imwrap::SmfSequence seq;
                        seq.format = 0;
                        seq.division = pv.division;
                        
                        imwrap::SmfTrack smfTrack;
                        smfTrack.events = track.events;
                        seq.tracks.push_back(std::move(smfTrack));
                        
                        std::vector<uint8_t> outBytes;
                        std::string err;
                        if (imwrap::SmfSerializer::Serialize(seq, &outBytes, &err)) {
                            FILE* f = fopen(path.toStdString().c_str(), "wb");
                            if (f) {
                                fwrite(outBytes.data(), 1, outBytes.size(), f);
                                fclose(f);
                                statusLabel->setText(QString("Exported track to %1").arg(path));
                            } else {
                                QMessageBox::critical(this, "Error", "Failed to open file for writing.");
                            }
                        } else {
                            QMessageBox::critical(this, "Error", QString("Failed to serialize MIDI:\n%1").arg(err.c_str()));
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}
