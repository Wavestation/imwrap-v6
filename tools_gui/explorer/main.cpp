#include <QApplication>

#include "ExplorerWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyle("windowsvista");

    ExplorerWindow window;
    window.resize(1400, 860);
    window.show();

    return app.exec();
}
