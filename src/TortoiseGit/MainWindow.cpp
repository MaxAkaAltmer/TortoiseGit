#include "MainWindow.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("TortoiseGit");
    auto menu = menuBar()->addMenu("&File");
    menu->addAction("E&xit", this, &QWidget::close);
}
