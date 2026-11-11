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
#include "DiffViewerWidget.h"

#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QVBoxLayout>

#ifdef TGIT_HAVE_SCINTILLA
#    include <Qsci/qscilexerdiff.h>
#    include <Qsci/qsciscintilla.h>
#endif

#include "CrossPlatform/Logging.h"

namespace DiffViewerDetails
{
    // Interface of the two patch views below. It is not a moc class on purpose:
    // both implementations live in this translation unit.
    class IPatchView : public QWidget
    {
    public:
        using QWidget::QWidget;

        virtual void SetPatch(const QString& patch) = 0;
        virtual QString Patch() const = 0;
    };
} // namespace DiffViewerDetails

namespace
{
    // Colours shared by both backends, so that enabling or disabling Scintilla
    // does not change how a patch looks. They follow the diff colours of
    // TortoiseUDiff on Windows (green additions, red deletions).
    struct DiffColors
    {
        QColor fileHeader{QStringLiteral("#0550ae")};
        QColor hunk{QStringLiteral("#8250df")};
        QColor addedText{QStringLiteral("#1a7f37")};
        QColor addedPaper{QStringLiteral("#dafbe1")};
        QColor deletedText{QStringLiteral("#cf222e")};
        QColor deletedPaper{QStringLiteral("#ffebe9")};
        QColor meta{QStringLiteral("#6e7781")};
    };

    const DiffColors& Colors()
    {
        static const DiffColors colors;
        return colors;
    }

    QFont PatchFont()
    {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    }

    // ------------------------------------------------------------------
    // Qt fallback: QPlainTextEdit plus a highlighter for the patch syntax
    // ------------------------------------------------------------------

    class PatchHighlighter : public QSyntaxHighlighter
    {
    public:
        explicit PatchHighlighter(QTextDocument* document)
            : QSyntaxHighlighter(document)
        {
            m_fileFormat.setForeground(Colors().fileHeader);
            m_fileFormat.setFontWeight(QFont::Bold);
            m_hunkFormat.setForeground(Colors().hunk);
            m_hunkFormat.setFontWeight(QFont::Bold);
            m_addedFormat.setForeground(Colors().addedText);
            m_addedFormat.setBackground(Colors().addedPaper);
            m_deletedFormat.setForeground(Colors().deletedText);
            m_deletedFormat.setBackground(Colors().deletedPaper);
            m_metaFormat.setForeground(Colors().meta);
            m_metaFormat.setFontItalic(true);
        }

    protected:
        void highlightBlock(const QString& text) override
        {
            // The prefixes are matched by hand instead of with a rule list:
            // the file headers "--- "/"+++ " have to win over the plain
            // addition/deletion lines.
            if (IsFileHeader(text))
                setFormat(0, text.size(), m_fileFormat);
            else if (text.startsWith(QLatin1String("@@")))
                setFormat(0, text.size(), m_hunkFormat);
            else if (text.startsWith(QLatin1Char('+')))
                setFormat(0, text.size(), m_addedFormat);
            else if (text.startsWith(QLatin1Char('-')))
                setFormat(0, text.size(), m_deletedFormat);
            else if (text.startsWith(QLatin1Char('\\'))) // "\ No newline at end of file"
                setFormat(0, text.size(), m_metaFormat);
        }

    private:
        static bool IsFileHeader(const QString& text)
        {
            static const char* const prefixes[] = {
                "diff ", "index ", "rename ", "copy ", "new file", "deleted file",
                "old mode", "new mode", "similarity ", "Binary files", "GIT binary patch",
            };
            for (const char* prefix : prefixes)
                if (text.startsWith(QLatin1String(prefix)))
                    return true;
            return text.startsWith(QLatin1String("--- ")) || text.startsWith(QLatin1String("+++ "));
        }

        QTextCharFormat m_fileFormat;
        QTextCharFormat m_hunkFormat;
        QTextCharFormat m_addedFormat;
        QTextCharFormat m_deletedFormat;
        QTextCharFormat m_metaFormat;
    };

    class PlainPatchView : public DiffViewerDetails::IPatchView
    {
    public:
        explicit PlainPatchView(QWidget* parent)
            : DiffViewerDetails::IPatchView(parent)
        {
            auto* layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);

            m_edit = new QPlainTextEdit(this);
            m_edit->setReadOnly(true);
            m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
            m_edit->setFont(PatchFont());
            m_highlighter = new PatchHighlighter(m_edit->document());

            layout->addWidget(m_edit);
        }

        void SetPatch(const QString& patch) override
        {
            m_edit->setPlainText(patch);
            m_edit->verticalScrollBar()->setValue(0);
        }

        QString Patch() const override { return m_edit->toPlainText(); }

    private:
        QPlainTextEdit* m_edit      = nullptr;
        PatchHighlighter* m_highlighter = nullptr;
    };

#ifdef TGIT_HAVE_SCINTILLA
    // ------------------------------------------------------------------
    // Scintilla backend: the diff lexer of QScintilla does the colouring
    // ------------------------------------------------------------------

    class ScintillaPatchView : public DiffViewerDetails::IPatchView
    {
    public:
        explicit ScintillaPatchView(QWidget* parent)
            : DiffViewerDetails::IPatchView(parent)
        {
            auto* layout = new QVBoxLayout(this);
            layout->setContentsMargins(0, 0, 0, 0);

            const QFont font = PatchFont();

            m_edit = new QsciScintilla(this);
            m_edit->setUtf8(true);
            m_edit->setReadOnly(true);
            m_edit->setWrapMode(QsciScintilla::WrapNone);
            m_edit->setCaretLineVisible(false);
            m_edit->setFont(font);
            m_edit->setMarginsFont(font);
            // Scintilla starts with three margins: line numbers, a symbol
            // margin and the fold margin. A patch has nothing to fold (the diff
            // lexer does not report fold levels), so only the numbers stay.
            m_edit->setMarginType(0, QsciScintilla::NumberMargin);
            m_edit->setMarginWidth(0, QStringLiteral("000000"));
            m_edit->setMarginWidth(1, 0);
            m_edit->setMarginWidth(2, 0);

            auto* lexer = new QsciLexerDiff(m_edit);
            lexer->setDefaultFont(font);
            lexer->setColor(Colors().meta, QsciLexerDiff::Default);
            lexer->setColor(Colors().meta, QsciLexerDiff::Comment);
            lexer->setColor(Colors().fileHeader, QsciLexerDiff::Command);
            lexer->setColor(Colors().fileHeader, QsciLexerDiff::Header);
            lexer->setColor(Colors().hunk, QsciLexerDiff::Position);
            lexer->setColor(Colors().deletedText, QsciLexerDiff::LineRemoved);
            lexer->setPaper(Colors().deletedPaper, QsciLexerDiff::LineRemoved);
            lexer->setColor(Colors().addedText, QsciLexerDiff::LineAdded);
            lexer->setPaper(Colors().addedPaper, QsciLexerDiff::LineAdded);
            lexer->setColor(Colors().hunk, QsciLexerDiff::LineChanged);
            m_edit->setLexer(lexer);

            layout->addWidget(m_edit);
        }

        void SetPatch(const QString& patch) override
        {
            m_edit->setText(patch);
            m_edit->setFirstVisibleLine(0);
        }

        QString Patch() const override { return m_edit->text(); }

    private:
        QsciScintilla* m_edit = nullptr;
    };
#endif // TGIT_HAVE_SCINTILLA
} // namespace

DiffViewerWidget::DiffViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef TGIT_HAVE_SCINTILLA
    m_view = new ScintillaPatchView(this);
#else
    m_view = new PlainPatchView(this);
#endif
    layout->addWidget(m_view);

    static bool backendLogged = false;
    if (!backendLogged)
    {
        backendLogged = true;
        CLogging::LogMessage(CLogging::LogLevel::Info,
                             "Diff viewer backend: " + BackendName().toStdString());
    }
}

DiffViewerWidget::~DiffViewerWidget() = default;

void DiffViewerWidget::SetPatch(const QString& patch)
{
    m_view->SetPatch(patch);
}

void DiffViewerWidget::Clear()
{
    m_view->SetPatch(QString());
}

QString DiffViewerWidget::Patch() const
{
    return m_view->Patch();
}

QString DiffViewerWidget::BackendName()
{
#ifdef TGIT_HAVE_SCINTILLA
    return QStringLiteral("Scintilla");
#else
    return QStringLiteral("Qt");
#endif
}
