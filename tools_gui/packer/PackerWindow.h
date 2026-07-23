#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QSplitter>
#include <QTableWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <vector>
#include <string>

#include "imwrap/ImsWriter.h"
#include "imwrap/SmfSequence.h"
#include "imwrap/ResourceBank.h"

struct ProjectTrack {
    std::string name;
    std::string sourceFileName;
    std::vector<imwrap::MidiEvent> events;
};

struct ProjectVariant {
    imwrap::VariantKind kind = imwrap::VariantKind::Gmd;
    bool includeVariant = false;
    bool includeMdhd = false;
    imwrap::MdhdData mdhd = imwrap::MdhdData::Defaults();
    uint16_t division = 480;
    std::vector<ProjectTrack> tracks;
};

struct ProjectSound {
    uint16_t id = 0;
    std::string name;
    std::vector<ProjectVariant> variants;
};

class PackerWindow : public QMainWindow {
    Q_OBJECT

public:
    PackerWindow(QWidget *parent = nullptr);
    ~PackerWindow();

private slots:
    void newProject();
    void openProject();
    void openXoredProject();
    void saveProject();
    void saveProjectAs();
    void saveProjectAsXored();
    
    void onSoundSelected();
    void onVariantKindChanged();
    void applySoundChanges();
    void addSound();
    void deleteSound();
    void importMidi();
    void deleteTrack();
    void moveTrackUp();
    void moveTrackDown();
    void onTrackSelectionChanged();
    void exportSelectedTrackMidi();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    void updateWindowTitle();
    void setDirty(bool dirty);
    bool promptSaveIfDirty();
    void loadImsToModel(const std::string &path);
    void updateVariantUi();
    void saveModelToIms(const std::string &path);

    QListWidget *soundList;
    QTableWidget *tracksTable;
    QLabel *statusLabel;
    
    QLineEdit *soundNameEdit;
    QSpinBox *soundIdSpin;
    QComboBox *variantKindCombo;
    QCheckBox *includeVariantCheck;
    QCheckBox *includeMdhdCheck;
    
    QSpinBox *prioritySpin, *volumeSpin, *panSpin, *transposeSpin, *detuneSpin, *speedSpin;

    QPushButton *applyBtn;
    QPushButton *importBtn;
    QPushButton *exportTrackBtn;
    QPushButton *deleteTrackBtn;
    QPushButton *moveUpBtn;
    QPushButton *moveDownBtn;
    QGroupBox *variantBox;
    QWidget *soundPropsWidget;

    std::vector<ProjectSound> projectSounds;
    QString currentFilePath;
    bool isDirty_ = false;
};
