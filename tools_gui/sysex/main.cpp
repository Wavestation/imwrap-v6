#include <QApplication>
#include <QIcon>
#include "SysExWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Pour un look natif Windows
    app.setStyle("windowsvista");
    app.setWindowIcon(QIcon(":/imwrap/sysex_generator_gui.ico"));

    SysExWindow window;
    window.setWindowTitle("iMWrap SysEx Tool");
    window.setWindowIcon(app.windowIcon());
    window.resize(800, 600);
    window.show();

    return app.exec();
}
