#include <QApplication>
#include <QIcon>

#include "ExplorerWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");
    app.setWindowIcon(QIcon(":/imwrap/explorer_gui.256.png"));

    ExplorerWindow window;
    window.setWindowIcon(app.windowIcon());
    window.resize(1400, 860);
    window.show();

    return app.exec();
}
