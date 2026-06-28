#include <QApplication>
#include <QIcon>
#include "PackerWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");
    app.setWindowIcon(QIcon(":/imwrap/packer_gui.ico"));

    PackerWindow window;
    window.setWindowTitle("iMWrap Packer Tool");
    window.setWindowIcon(app.windowIcon());
    window.resize(1000, 750);
    window.show();

    return app.exec();
}
