#include <QApplication>
#include "PackerWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");

    PackerWindow window;
    window.setWindowTitle("iMUSE Packer Tool");
    window.resize(1000, 750);
    window.show();

    return app.exec();
}
