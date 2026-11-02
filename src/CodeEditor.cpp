#include "CodeEditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QMimeData>
#include <QUrl>

LineNumberArea::LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

QSize LineNumberArea::sizeHint() const {
    return QSize(codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    codeEditor->lineNumberAreaPaintEvent(event);
}
// ----------------------------------

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::refreshLineNumberArea()
{
    updateLineNumberAreaWidth(0);
    lineNumberArea->update();
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::insertFromMimeData(const QMimeData *source)
{
    // Перехватываем drop файлов из файлового менеджера: вместо вставки
    // пути как текста — сигналим MainWindow, что нужно открыть файл.
    if (source && source->hasUrls()) {
        bool handled = false;
        for (const QUrl &url : source->urls()) {
            if (url.isLocalFile()) {
                emit fileDropped(url.toLocalFile());
                handled = true;
            }
        }
        if (handled) return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

void CodeEditor::refreshHighlights()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#2a2d2e"));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        selections.append(selection);
    }

    if (!m_lastSearch.isEmpty()) {
        QTextDocument *doc = document();
        QTextCursor cursor(doc);

        QTextCharFormat format;
        format.setBackground(QColor("#515c6a"));
        format.setForeground(Qt::white);

        QTextDocument::FindFlags flags;
        if (m_caseSensitive) flags |= QTextDocument::FindCaseSensitively;

        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(m_lastSearch, cursor, flags);
            if (!cursor.isNull()) {
                QTextEdit::ExtraSelection hs;
                hs.cursor = cursor;
                hs.format = format;
                selections.append(hs);
            }
        }
    }

    setExtraSelections(selections);
}

void CodeEditor::highlightCurrentLine()
{
    refreshHighlights();
}

void CodeEditor::highlightSearchMatches(const QString &text)
{
    m_lastSearch = text;
    refreshHighlights();
}

void CodeEditor::setSearchCaseSensitive(bool cs)
{
    if (m_caseSensitive == cs) return;
    m_caseSensitive = cs;
    refreshHighlights();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor("#1e1e1e"));

    painter.setFont(this->font());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor("#6e7681"));
            painter.drawText(0, top, lineNumberArea->width() - 8, fontMetrics().height(),
                             Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }

    painter.setPen(QColor("#333333"));
    painter.drawLine(lineNumberArea->width() - 1, event->rect().top(),
                     lineNumberArea->width() - 1, event->rect().bottom());
}
