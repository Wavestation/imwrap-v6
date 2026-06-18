#include <QApplication>
#include "PlayerWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");

    PlayerWindow window;
    window.setWindowTitle("iMUSE Player Tool");
    window.resize(900, 700);
    window.show();

    return app.exec();
}
