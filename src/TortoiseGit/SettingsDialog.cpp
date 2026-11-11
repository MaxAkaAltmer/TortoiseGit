// TortoiseGit - Linux GUI for Git
// Copyright (C) 2025 - TortoiseGit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "AppSettings.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));

    auto* form = new QFormLayout();

    auto* pathRow = new QHBoxLayout();
    m_repoPathEdit = new QLineEdit(QString::fromStdString(AppSettings::DefaultRepoPath()), this);
    m_repoPathEdit->setPlaceholderText(QDir::homePath());
    auto* browseButton = new QPushButton(tr("&Browse..."), this);
    pathRow->addWidget(m_repoPathEdit);
    pathRow->addWidget(browseButton);
    form->addRow(tr("Default repository path:"), pathRow);

    m_logLimitSpin = new QSpinBox(this);
    m_logLimitSpin->setRange(0, 100000);
    m_logLimitSpin->setSingleStep(50);
    m_logLimitSpin->setValue(AppSettings::LogLimit());
    m_logLimitSpin->setSpecialValueText(tr("All"));
    form->addRow(tr("Commits to load in the log:"), m_logLimitSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::SaveSettings);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::BrowseRepoPath);
}

void SettingsDialog::BrowseRepoPath()
{
    QString start = m_repoPathEdit->text();
    if (start.isEmpty() || !QDir(start).exists())
        start = QDir::homePath();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Default Repository Path"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty())
        m_repoPathEdit->setText(dir);
}

void SettingsDialog::SaveSettings()
{
    AppSettings::SetDefaultRepoPath(m_repoPathEdit->text().trimmed().toStdString());
    AppSettings::SetLogLimit(m_logLimitSpin->value());
    accept();
}
