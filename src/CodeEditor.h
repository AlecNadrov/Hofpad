#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QWidget>
#include <QString>

class CodeEditor;

class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *codeEditor;
};

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    void highlightSearchMatches(const QString &text);
    void setSearchCaseSensitive(bool cs);
    void refreshLineNumberArea();

signals:
    void fileDropped(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void refreshHighlights();

    QWidget *lineNumberArea;
    QString  m_lastSearch;
    bool     m_caseSensitive = false;
};

#endif // CODEEDITOR_H
