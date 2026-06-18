#ifndef MIDICONFIGWINDOW_H
#define MIDICONFIGWINDOW_H

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

struct AppStrings {
    QString windowTitle;
    QString driverLabel;
    QString deviceLabel;
    QString saveBtn;
    QString cancelBtn;
    QString driverFluid;
    QString driverAdLib;
    QString driverHwGM;
    QString driverHwMT32;
    QString statusSaved;
    QString noDevice;
    QString targetLabelText;
};

class MidiConfigWindow : public QWidget {
    Q_OBJECT
public:
    explicit MidiConfigWindow(QWidget *parent = nullptr);
    ~MidiConfigWindow();

private slots:
    void onDriverChanged(int index);
    void onSave();
    void onCancel();

private:
    void populateMidiDevices();
    void loadConfig();
    QString getTargetConfigPath();
    AppStrings getStrings();
    void setupUI();

    QComboBox *driverCombo = nullptr;
    QComboBox *deviceCombo = nullptr;
    QLabel *driverLabel = nullptr;
    QLabel *deviceLabel = nullptr;
    QLabel *targetLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QPushButton *saveBtn = nullptr;
    QPushButton *cancelBtn = nullptr;

    AppStrings strings;
    QString configFilePath;
};

#endif // MIDICONFIGWINDOW_H
