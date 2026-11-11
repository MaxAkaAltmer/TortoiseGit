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
#pragma once

#include <QString>
#include <QWidget>

namespace DiffViewerDetails
{
    class IPatchView;
}

// Read-only view of a unified patch, used by the Diff dialog and by the commit
// diff of the Log dialog.
//
// The rendering has two backends behind one API:
//  - Scintilla (QScintilla) with the diff lexer, line numbers and the TortoiseGit
//    diff colours. It is used when the QScintilla development package was found
//    at configure time (see cmake/DetectQScintilla.cmake);
//  - a QPlainTextEdit with a diff highlighter everywhere else.
// Both show the same colours and use the fixed-pitch system font, so the choice
// of the backend only changes the widget, not the reading experience.
class DiffViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiffViewerWidget(QWidget* parent = nullptr);
    ~DiffViewerWidget() override;

    DiffViewerWidget(const DiffViewerWidget&) = delete;
    DiffViewerWidget& operator=(const DiffViewerWidget&) = delete;

    // Replaces the content of the view; the scroll position is reset.
    void SetPatch(const QString& patch);
    void Clear();

    // The patch currently shown, e.g. for copying it to the clipboard.
    QString Patch() const;

    // Name of the backend in use ("Scintilla" or "Qt"), logged once per process.
    static QString BackendName();

private:
    DiffViewerDetails::IPatchView* m_view = nullptr;
};
