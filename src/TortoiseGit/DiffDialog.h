#pragma once
#include <QDialog>
#include <QWidget>

class DiffDialog : public QDialog {
Q_OBJECT
public:
    explicit DiffDialog(QWidget *parent = nullptr);
};
