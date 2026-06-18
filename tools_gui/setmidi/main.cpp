#include <QApplication>
#include "MidiConfigWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MidiConfigWindow w;
    w.show();
    return app.exec();
}
