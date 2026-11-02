#include "MainWindow.h"
#include "CodeEditor.h"
#include "SettingsDialog.h"
#include <QSplitter>
#include <QListWidget>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QPixmap>
#include <QTextStream>
#include <QStringConverter>
#include <QMessageBox>
#include <QDir>
#include <QCloseEvent>
#include <QStatusBar>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QToolBar>
#include <QLineEdit>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Hofpad");
    resize(1000, 700);
    setAcceptDrops(true);

    QAction *newTabAction = new QAction("Новая вкладка", this);
    newTabAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(newTabAction, &QAction::triggered, this, &MainWindow::addNewTab);

    QAction *openAction = new QAction("Открыть", this);
    openAction->setShortcut(QKeySequence("Ctrl+O"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    QAction *saveAction = new QAction("Сохранить", this);
    saveAction->setShortcut(QKeySequence("Ctrl+S"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction *saveAsAction = new QAction("Сохранить как...", this);
    saveAsAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);

    QAction *closeTabAction = new QAction("Закрыть", this);
    closeTabAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    QAction *undoAction = new QAction("Отменить", this);
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this](){ if(currentEditor()) currentEditor()->undo(); });

    QAction *redoAction = new QAction("Вернуть", this);
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this](){ if(currentEditor()) currentEditor()->redo(); });

    QAction *cutAction = new QAction("Вырезать", this);
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this](){ if(currentEditor()) currentEditor()->cut(); });

    QAction *copyAction = new QAction("Копировать", this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this](){ if(currentEditor()) currentEditor()->copy(); });

    QAction *pasteAction = new QAction("Вставить", this);
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this](){ if(currentEditor()) currentEditor()->paste(); });

    QAction *findAction = new QAction("Найти...", this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::showSearchBar);

    QMenu *fileMenu = menuBar()->addMenu("Файл");
    fileMenu->addAction(newTabAction);
    fileMenu->addAction(openAction);
    m_recentMenu = fileMenu->addMenu("Недавние файлы");
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction("Настройки...");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(closeTabAction);

    QMenu *editMenu = menuBar()->addMenu("Правка");
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);

    QMenu *searchMenu = menuBar()->addMenu("Поиск");
    searchMenu->addAction(findAction);

    QMenu *helpMenu = menuBar()->addMenu("Справка");
    QAction *aboutAction = helpMenu->addAction("О программе");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    m_toolBar = new QToolBar("Панель инструментов", this);
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, m_toolBar);

    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_toolBar->addAction(newTabAction);
    m_toolBar->addAction(openAction);
    m_toolBar->addAction(saveAction);

    m_toolBar->addSeparator();

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Найти...");
    m_searchInput->setMinimumWidth(180);
    m_searchInput->setMaximumWidth(260);
    m_toolBar->addWidget(m_searchInput);

    m_caseSensitiveAct = m_toolBar->addAction("Aa");
    m_caseSensitiveAct->setCheckable(true);
    m_caseSensitiveAct->setToolTip("Учитывать регистр");

    m_searchPrevAct = m_toolBar->addAction("▲");
    m_searchNextAct = m_toolBar->addAction("▼");

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(200);
    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::performSearch);

    connect(m_searchInput, &QLineEdit::textChanged, this, [this]() { m_searchTimer->start(); });
    connect(m_searchInput, &QLineEdit::returnPressed, this, &MainWindow::searchNext);
    connect(m_searchNextAct, &QAction::triggered, this, &MainWindow::searchNext);
    connect(m_searchPrevAct, &QAction::triggered, this, &MainWindow::searchPrev);

    connect(m_caseSensitiveAct, &QAction::toggled, this, [this](bool cs) {
        for (int i = 0; i < m_editors->count(); ++i) {
            if (auto *ed = qobject_cast<CodeEditor*>(m_editors->widget(i)))
                ed->setSearchCaseSensitive(cs);
        }
        if (currentEditor()) currentEditor()->highlightSearchMatches(m_searchInput->text());
    });

    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_sidebar = new QListWidget(m_splitter);
    m_sidebar->setMinimumWidth(150);
    m_sidebar->setMaximumWidth(400);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setContextMenuPolicy(Qt::CustomContextMenu);

    m_editors = new QStackedWidget(m_splitter);
    m_editors->setMinimumWidth(200);

    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_editors);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes(QList<int>() << 220 << 780);
    m_splitter->setHandleWidth(2);

    setCentralWidget(m_splitter);

    connect(m_sidebar, &QListWidget::currentRowChanged, m_editors, &QStackedWidget::setCurrentIndex);
    connect(m_sidebar, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        QListWidgetItem *item = m_sidebar->itemAt(pos);
        if (!item) return;
        QMenu contextMenu(this);
        QAction *closeAction = contextMenu.addAction("Закрыть");
        connect(closeAction, &QAction::triggered, this, [this, item]() {
            int row = m_sidebar->row(item);
            m_sidebar->setCurrentRow(row);
            closeCurrentTab();
        });
        contextMenu.exec(m_sidebar->mapToGlobal(pos));
    });

    m_statusCursor = new QLabel("Стр: 1, Стлб: 1");
    m_statusEncoding = new QLabel("UTF-8");
    statusBar()->addPermanentWidget(m_statusCursor);
    statusBar()->addPermanentWidget(m_statusEncoding);

    connect(m_editors, &QStackedWidget::currentChanged, this, [this](int) {
        updateStatusBar();
        if (CodeEditor *ed = currentEditor()) {
            if (!m_searchInput->text().isEmpty())
                ed->highlightSearchMatches(m_searchInput->text());
        }
    });

    QSettings settings;
    m_recentFiles = settings.value("files/recent").toStringList();
    updateRecentFilesMenu();

    bool remember = settings.value("session/remember", true).toBool();
    if (remember) {
        loadSession();
        if (m_editors->count() == 0) addNewTab();
    } else {
        addNewTab();
    }
}

MainWindow::~MainWindow() {}


void MainWindow::showSearchBar()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void MainWindow::performSearch()
{
    if (currentEditor())
        currentEditor()->highlightSearchMatches(m_searchInput->text());
}

void MainWindow::searchNext()
{
    CodeEditor *editor = currentEditor();
    if (!editor || m_searchInput->text().isEmpty()) return;

    QTextDocument::FindFlags flags;
    if (m_caseSensitiveAct->isChecked()) flags |= QTextDocument::FindCaseSensitively;

    QString searchText = m_searchInput->text();
    if (!editor->find(searchText, flags)) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        editor->setTextCursor(cursor);
        editor->find(searchText, flags);
    }
}

void MainWindow::searchPrev()
{
    CodeEditor *editor = currentEditor();
    if (!editor || m_searchInput->text().isEmpty()) return;

    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (m_caseSensitiveAct->isChecked()) flags |= QTextDocument::FindCaseSensitively;

    QString searchText = m_searchInput->text();
    if (!editor->find(searchText, flags)) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        editor->find(searchText, flags);
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings settings;
        settings.setValue("font/family", dialog.fontFamily());
        settings.setValue("font/size", dialog.fontSize());
        settings.setValue("editor/wordWrap", dialog.wordWrap());
        settings.setValue("session/remember", dialog.rememberSession());
        applySettings();
    }
}

void MainWindow::applySettingsTo(CodeEditor *editor)
{
    if (!editor) return;

    QSettings settings;
    QString fontFamily = settings.value("font/family", "Monospace").toString();
    int fontSize = settings.value("font/size", 14).toInt();
    bool wordWrap = settings.value("editor/wordWrap", false).toBool();

    QFont font(fontFamily);
    font.setPixelSize(fontSize);

    editor->setFont(font);
    editor->document()->setDefaultFont(font);
    editor->setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    editor->refreshLineNumberArea();
}

void MainWindow::applySettings()
{
    for (int i = 0; i < m_editors->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(m_editors->widget(i));
        applySettingsTo(editor);
    }
}

void MainWindow::saveSession()
{
    QSettings settings;
    if (!settings.value("session/remember", true).toBool()) {
        settings.remove("session/files");
        return;
    }

    QStringList openFiles;
    for (int i = 0; i < m_editors->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(m_editors->widget(i));
        if (editor) {
            QString path = editor->property("filePath").toString();
            if (!path.isEmpty()) openFiles.append(path);
        }
    }
    settings.setValue("session/files", openFiles);
}

void MainWindow::loadSession()
{
    QSettings settings;
    QStringList openFiles = settings.value("session/files").toStringList();
    for (const QString &path : openFiles) {
        openFilePath(path);
    }
}

void MainWindow::addToRecentFiles(const QString &filePath)
{
    if (m_recentFiles.contains(filePath)) m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();

    QSettings settings;
    settings.setValue("files/recent", m_recentFiles);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentMenu->clear();
    for (int i = 0; i < m_recentFiles.size(); ++i) {
        QString path = m_recentFiles[i];
        QAction *action = m_recentMenu->addAction(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            openFilePath(path);
        });
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl &url : urlList) {
            QString filePath = url.toLocalFile();
            if (!filePath.isEmpty()) openFilePath(filePath);
        }
        event->acceptProposedAction();
    }
}

void MainWindow::openFilePath(const QString &filePath)
{
    const QString canonical = QFileInfo(filePath).canonicalFilePath();
    if (!canonical.isEmpty()) {
        for (int i = 0; i < m_editors->count(); ++i) {
            CodeEditor *ed = qobject_cast<CodeEditor*>(m_editors->widget(i));
            if (!ed) continue;
            const QString openPath = ed->property("filePath").toString();
            if (openPath.isEmpty()) continue;
            if (QFileInfo(openPath).canonicalFilePath() == canonical) {
                m_sidebar->setCurrentRow(i);
                ed->setFocus();
                return;
            }
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл: " + filePath);
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    // Переиспользуем текущий таб, если он пустой и без пути — чтобы не
    // плодить "Untitled N" при открытии файла в свежем окне.
    CodeEditor *editor = currentEditor();
    const bool reuseCurrent = editor
        && editor->property("filePath").toString().isEmpty()
        && editor->document()->isEmpty()
        && !editor->document()->isModified();

    if (!reuseCurrent) {
        addNewTab();
        editor = currentEditor();
    }
    if (!editor) return;

    editor->setPlainText(content);
    editor->setProperty("filePath", filePath);
    editor->document()->setModified(false);

    QFileInfo fileInfo(filePath);
    editor->setProperty("tabName", fileInfo.fileName());
    updateTabTitle(editor);

    addToRecentFiles(filePath);

    if (!m_searchInput->text().isEmpty())
        editor->highlightSearchMatches(m_searchInput->text());

    editor->setFocus();
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Открыть файл", QDir::homePath(), "Все файлы (*.*)");
    if (!filePath.isEmpty()) openFilePath(filePath);
}

CodeEditor* MainWindow::currentEditor() const {
    return qobject_cast<CodeEditor*>(m_editors->currentWidget());
}

void MainWindow::addNewTab()
{
    m_tabCounter++;
    QString tabName = QString("Untitled %1").arg(m_tabCounter);
    m_sidebar->addItem(tabName);

    CodeEditor *newEditor = new CodeEditor(m_editors);
    newEditor->setPlaceholderText("Начни печатать...");
    newEditor->setProperty("tabName", tabName);
    newEditor->setProperty("filePath", "");
    newEditor->setSearchCaseSensitive(m_caseSensitiveAct->isChecked());

    connect(newEditor->document(), &QTextDocument::modificationChanged,
            this, [this, newEditor]() { updateTabTitle(newEditor); });
    connect(newEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    connect(newEditor, &CodeEditor::fileDropped, this, &MainWindow::openFilePath);

    m_editors->addWidget(newEditor);

    int newIndex = m_editors->count() - 1;
    m_sidebar->setCurrentRow(newIndex);

    applySettingsTo(newEditor);

    if (!m_searchInput->text().isEmpty())
        newEditor->highlightSearchMatches(m_searchInput->text());

    newEditor->setFocus();
}

bool MainWindow::saveToPath(CodeEditor *editor, const QString &filePath)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + filePath);
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << editor->toPlainText();

    if (!file.commit()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось записать файл: " + filePath);
        return false;
    }

    editor->setProperty("filePath", filePath);
    QFileInfo fileInfo(filePath);
    editor->setProperty("tabName", fileInfo.fileName());
    editor->document()->setModified(false);
    updateTabTitle(editor);
    addToRecentFiles(filePath);
    return true;
}

bool MainWindow::saveEditor(CodeEditor *editor)
{
    if (!editor) return false;

    QString filePath = editor->property("filePath").toString();
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(this, "Сохранить файл", QDir::homePath(), "Все файлы (*.*)");
        if (filePath.isEmpty()) return false;
    }
    return saveToPath(editor, filePath);
}

bool MainWindow::saveFile()
{
    return saveEditor(currentEditor());
}

bool MainWindow::saveFileAs()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return false;

    QString current = editor->property("filePath").toString();
    QString filePath = QFileDialog::getSaveFileName(
        this, "Сохранить как",
        current.isEmpty() ? QDir::homePath() : current,
        "Все файлы (*.*)");
    if (filePath.isEmpty()) return false;

    return saveToPath(editor, filePath);
}

void MainWindow::updateTabTitle(CodeEditor *editor)
{
    int index = m_editors->indexOf(editor);
    if (index == -1) return;

    QString tabName = editor->property("tabName").toString();
    if (editor->document()->isModified()) {
        tabName += " *";
    }
    if (QListWidgetItem *item = m_sidebar->item(index))
        item->setText(tabName);
}

bool MainWindow::maybeSave(CodeEditor *editor)
{
    if (!editor->document()->isModified()) return true;

    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, "Hofpad",
                 "Документ был изменен.\nХотите сохранить изменения?",
                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (ret == QMessageBox::Save)   return saveEditor(editor);
    if (ret == QMessageBox::Cancel) return false;
    return true;
}

void MainWindow::closeCurrentTab()
{
    int index = m_editors->currentIndex();
    if (index == -1) return;

    CodeEditor *editor = qobject_cast<CodeEditor*>(m_editors->widget(index));
    if (!editor) return;

    if (maybeSave(editor)) {
        removeTab(index);
    }
}

void MainWindow::removeTab(int index)
{
    QWidget *editor = m_editors->widget(index);
    if (editor) {
        m_editors->removeWidget(editor);
        delete editor;
    }

    if (QListWidgetItem *item = m_sidebar->takeItem(index))
        delete item;

    if (m_editors->count() == 0) {
        this->close();
        return;
    }

    const int newRow = qBound(0, index, m_editors->count() - 1);
    m_sidebar->setCurrentRow(newRow);
    m_editors->setCurrentIndex(newRow);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const int originalIndex = m_editors->currentIndex();

    for (int i = 0; i < m_editors->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(m_editors->widget(i));
        if (editor) {
            m_sidebar->setCurrentRow(i);
            if (!maybeSave(editor)) {
                m_sidebar->setCurrentRow(originalIndex);
                event->ignore();
                return;
            }
        }
    }
    saveSession();
    event->accept();
}

void MainWindow::updateStatusBar()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber() + 1;
    int col = cursor.columnNumber() + 1;

    m_statusCursor->setText(QString("Стр: %1, Стлб: %2").arg(line).arg(col));
}

void MainWindow::showAboutDialog()
{
    QString aboutText = QString(
        "<h2>Hofpad</h2>"
        "<p><b>Версия:</b> 0.1.0 Linyphiidae</p>"
        "<p><b>Разработчик:</b> AlecNadrov</p>"
        "<p><b>Лицензия:</b> Open Source (GPLv3)</p>"
        "<br>"
        "<p><i>Легковесный текстовый редактор</i></p>"
    );

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("О программе");
    msgBox.setText(aboutText);
    QPixmap logo(":/hofpadwhite.png");
    if (!logo.isNull()) {
        msgBox.setIconPixmap(logo.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    msgBox.exec();
}
