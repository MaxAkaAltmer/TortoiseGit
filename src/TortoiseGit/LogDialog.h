#pragma once
#include <QDialog>
#include <QWidget>

class LogDialog : public QDialog {
Q_OBJECT
public:
    explicit LogDialog(QWidget *parent = nullptr);
};
