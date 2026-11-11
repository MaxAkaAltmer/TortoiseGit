#pragma once
#include <QDialog>
#include <QWidget>

class SettingsDialog : public QDialog {
Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
};
