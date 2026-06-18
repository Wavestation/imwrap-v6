#include <QApplication>
#include "SysExWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Pour un look natif Windows
    app.setStyle("windowsvista");

    SysExWindow window;
    window.setWindowTitle("iMUSE SysEx Tool");
    window.resize(800, 600);
    window.show();

    return app.exec();
}
