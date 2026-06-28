#include <QApplication>
#include <QIcon>
#include "PlayerWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");
    app.setWindowIcon(QIcon(":/imwrap/player_gui.ico"));

    PlayerWindow window;
    window.setWindowTitle("iMWrap Player Tool");
    window.setWindowIcon(app.windowIcon());
    window.resize(900, 700);
    window.show();

    return app.exec();
}
