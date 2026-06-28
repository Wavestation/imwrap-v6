#pragma once

#include <QCloseEvent>
#include <QMainWindow>

#include <windows.h>
#include <mmsystem.h>

#include "imwrap/IMWrapEngine.h"
#include "imwrap/MidiSink.h"
#include "ImsProject.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QSplitter;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

class ExplorerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ExplorerWindow(QWidget* parent = nullptr);
    ~ExplorerWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openDocument();
    void saveDocument();
    void saveDocumentAs();
    void refreshDevices();
    void togglePreview();
    void playSelectedSound();
    void stopSelectedSound();
    void stopAllSounds();
    void importMidiTracks();
    void deleteSelectedTrack();
    void moveTrackUp();
    void moveTrackDown();
    void onTreeSelectionChanged();
    void onEventSelectionChanged();
    void onVariantEditorKindChanged();
    void applySelectionProperties();
    void applyHook();
    void addImwrapCommand();
    void editSelectedEvent();
    void onTimer();

private:
    enum class NodeType : int {
        Sound = 1,
        Variant = 2,
        Track = 3
    };

    struct SelectionIndices {
        int soundIndex = -1;
        int variantIndex = -1;
        int trackIndex = -1;
    };

    struct MbtPosition {
        int measure = 1;
        int beat = 1;
        int tick = 0;
    };

    struct TimeSignatureSegment {
        uint32_t startTick = 0;
        int startMeasure = 1;
        int startBeat = 1;
        int startTickInBeat = 0;
        int numerator = 4;
        int denominator = 4;
        uint32_t ticksPerBeat = 480;
    };

    class WinMMSink : public imwrap::MidiSink {
    public:
        ~WinMMSink() override;

        bool openDevice(UINT deviceId);
        void closeDevice();
        bool isAvailable() const override;

        void onMidiMessage(uint16_t soundId, uint8_t status, uint8_t data1, bool hasData2, uint8_t data2) override;
        void onSysEx(uint16_t soundId, imwrap::ByteView message) override;

    private:
        HMIDIOUT hMidiOut_ = nullptr;
    };

    void setupUi();
    void rebuildTree();
    void rebuildEventTable();
    void rebuildDetailPane();
    void rebuildSelectionProperties();
    void updateUiState();
    void updateWindowTitle();
    void markDirty(bool dirty = true);
    void loadSettings();
    void saveSettings() const;
    bool openDocumentPath(const QString& path, bool showErrorDialogs = true);
    void selectNode(int soundIndex, int variantIndex = -1, int trackIndex = -1);
    int selectedEventModelIndex() const;
    void selectVisibleEventByModelIndex(int modelIndex);

    bool rebuildPreviewBank(std::string* error = nullptr);
    imwrap::TargetProfile effectiveProfileForSelection() const;
    uint16_t selectedSoundId() const;
    SelectionIndices currentSelection() const;
    imwrap::gui::ProjectSound* selectedSound();
    imwrap::gui::ProjectVariant* selectedVariant();
    imwrap::gui::ProjectTrack* selectedTrack();
    const imwrap::gui::ProjectTrack* selectedTrack() const;
    std::vector<TimeSignatureSegment> buildTimeSignatureSegments(const imwrap::gui::ProjectVariant& variant) const;
    MbtPosition tickToMbt(const std::vector<TimeSignatureSegment>& segments, uint32_t tick) const;
    bool mbtToTick(const std::vector<TimeSignatureSegment>& segments, const MbtPosition& mbt, uint32_t* outTick) const;
    QString formatMbtPosition(const std::vector<TimeSignatureSegment>& segments, uint32_t tick) const;
    void normalizeTrackEvents(imwrap::gui::ProjectTrack* track) const;
    int insertEventIntoTrack(imwrap::gui::ProjectTrack* track, imwrap::MidiEvent event) const;
    int replaceEventInTrack(imwrap::gui::ProjectTrack* track, int modelIndex, imwrap::MidiEvent event) const;

    static QString formatPayloadHex(const std::vector<uint8_t>& payload);
    static QString describeMidiEvent(const imwrap::MidiEvent& event, bool* editableImwrapSysEx = nullptr);
    static std::vector<uint8_t> encodeSmfPayload(const imwrap::IMWrapControlEvent& event);
    static bool isRecognizedImwrapEvent(const imwrap::MidiEvent& event);

    QLineEdit* filePathEdit_ = nullptr;
    QTreeWidget* contentTree_ = nullptr;
    QTableWidget* eventsTable_ = nullptr;
    QTextEdit* detailsEdit_ = nullptr;
    QCheckBox* filterNonImwrapCheck_ = nullptr;
    QComboBox* deviceCombo_ = nullptr;
    QComboBox* profileCombo_ = nullptr;
    QWidget* selectionPropsWidget_ = nullptr;
    QWidget* hookControlsWidget_ = nullptr;
    QLabel* selectionSummaryLabel_ = nullptr;
    QLineEdit* soundNameEdit_ = nullptr;
    QSpinBox* soundIdSpin_ = nullptr;
    QComboBox* variantKindCombo_ = nullptr;
    QCheckBox* includeVariantCheck_ = nullptr;
    QCheckBox* includeMdhdCheck_ = nullptr;
    QSpinBox* prioritySpin_ = nullptr;
    QSpinBox* volumeSpin_ = nullptr;
    QSpinBox* panSpin_ = nullptr;
    QSpinBox* transposeSpin_ = nullptr;
    QSpinBox* detuneSpin_ = nullptr;
    QSpinBox* speedSpin_ = nullptr;
    QPushButton* applySelectionBtn_ = nullptr;
    QComboBox* hookClassCombo_ = nullptr;
    QSpinBox* hookValueSpin_ = nullptr;
    QSpinBox* hookChannelSpin_ = nullptr;
    QPushButton* applyHookBtn_ = nullptr;
    QPushButton* addEventBtn_ = nullptr;
    QPushButton* previewBtn_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* stopAllBtn_ = nullptr;
    QPushButton* editEventBtn_ = nullptr;
    QPushButton* importTracksBtn_ = nullptr;
    QPushButton* deleteTrackBtn_ = nullptr;
    QPushButton* moveTrackUpBtn_ = nullptr;
    QPushButton* moveTrackDownBtn_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTimer* transportTimer_ = nullptr;
    QSplitter* mainSplitter_ = nullptr;
    QSplitter* bottomSplitter_ = nullptr;

    QString currentFilePath_;
    bool dirty_ = false;
    bool previewEnabled_ = false;
    bool updatingSelectionProps_ = false;
    qint64 lastTime_ = 0;
    double tickAccumulator_ = 0.0;
    std::vector<int> visibleEventIndices_;

    imwrap::gui::ImsProjectDocument document_;
    imwrap::ResourceBank previewBank_;
    imwrap::IMWrapEngine engine_;
    WinMMSink midiSink_;
};
