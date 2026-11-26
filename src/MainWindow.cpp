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
#include <QTabWidget>
#include <QTextBrowser>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QPushButton>
#include <algorithm>

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

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_sidebar->count()) return;
        QListWidgetItem *item = m_sidebar->item(row);
        QObject *obj = item->data(Qt::UserRole).value<QObject*>();
        if (CodeEditor *ed = qobject_cast<CodeEditor*>(obj)) {
            m_editors->setCurrentWidget(ed);
        }
    });
    
    connect(m_sidebar, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        QListWidgetItem *item = m_sidebar->itemAt(pos);
        if (!item) return;
        
        QObject *obj = item->data(Qt::UserRole).value<QObject*>();
        CodeEditor *ed = qobject_cast<CodeEditor*>(obj);
        if (!ed) return;

        QMenu contextMenu(this);
        
        bool isPinned = ed->property("isPinned").toBool();
        QAction *pinAction = contextMenu.addAction(isPinned ? "Открепить вкладку" : "Закрепить вкладку");
        connect(pinAction, &QAction::triggered, this, [this, ed]() {
            bool newState = !ed->property("isPinned").toBool();
            ed->setProperty("isPinned", newState);
            updateTabTitle(ed);
        });

        QAction *closeAction = contextMenu.addAction("Закрыть");
        connect(closeAction, &QAction::triggered, this, [this, item]() {
            m_sidebar->setCurrentItem(item);
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
        if (m_sidebar->count() == 0) addNewTab();
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
    
    if (wordWrap) {
        editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    
    editor->refreshLineNumberArea();
}

void MainWindow::applySettings()
{
    for (int i = 0; i < m_editors->count(); ++i) {
        CodeEditor *editor = qobject_cast<CodeEditor*>(m_editors->widget(i));
        applySettingsTo(editor);
    }
}

void MainWindow::sortTabs()
{
    QListWidgetItem *current = m_sidebar->currentItem();
    QList<QListWidgetItem*> items;
    for (int i = 0; i < m_sidebar->count(); ++i) items.append(m_sidebar->item(i));

    std::stable_sort(items.begin(), items.end(), [](QListWidgetItem *a, QListWidgetItem *b) {
        bool pinA = a->data(Qt::UserRole + 1).toBool();
        bool pinB = b->data(Qt::UserRole + 1).toBool();
        return pinA > pinB;
    });

    m_sidebar->blockSignals(true);
    while (m_sidebar->count() > 0) m_sidebar->takeItem(0);
    for (QListWidgetItem *item : items) m_sidebar->addItem(item);
    m_sidebar->blockSignals(false);

    if (current) m_sidebar->setCurrentItem(current);
}

void MainWindow::saveSession()
{
    QSettings settings;
    if (!settings.value("session/remember", true).toBool()) {
        settings.remove("session/files_json");
        settings.remove("session/files");
        return;
    }

    QJsonArray sessionArray;
    for (int i = 0; i < m_sidebar->count(); ++i) {
        QListWidgetItem *item = m_sidebar->item(i);
        QObject *obj = item->data(Qt::UserRole).value<QObject*>();
        CodeEditor *editor = qobject_cast<CodeEditor*>(obj);
        if (editor) {
            QString path = editor->property("filePath").toString();
            if (!path.isEmpty()) {
                QJsonObject fileObj;
                fileObj["path"] = path;
                fileObj["pinned"] = editor->property("isPinned").toBool();
                sessionArray.append(fileObj);
            }
        }
    }
    QJsonDocument doc(sessionArray);
    settings.setValue("session/files_json", doc.toJson(QJsonDocument::Compact));
}

void MainWindow::loadSession()
{
    QSettings settings;
    QByteArray jsonData = settings.value("session/files_json").toByteArray();
    
    if (!jsonData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        QJsonArray sessionArray = doc.array();
        for (const QJsonValue &value : sessionArray) {
            QJsonObject fileObj = value.toObject();
            QString path = fileObj["path"].toString();
            bool pinned = fileObj["pinned"].toBool();
            
            openFilePath(path);
            
            CodeEditor *ed = currentEditor();
            if (ed && ed->property("filePath").toString() == path) {
                ed->setProperty("isPinned", pinned);
                updateTabTitle(ed);
            }
        }
    } else {
        QStringList openFiles = settings.value("session/files").toStringList();
        for (const QString &path : openFiles) {
            openFilePath(path);
        }
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
        for (int i = 0; i < m_sidebar->count(); ++i) {
            QObject *obj = m_sidebar->item(i)->data(Qt::UserRole).value<QObject*>();
            CodeEditor *ed = qobject_cast<CodeEditor*>(obj);
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
    QListWidgetItem *item = m_sidebar->currentItem();
    if (!item) return nullptr;
    QObject *obj = item->data(Qt::UserRole).value<QObject*>();
    return qobject_cast<CodeEditor*>(obj);
}

void MainWindow::addNewTab()
{
    m_tabCounter++;
    QString tabName = QString("Untitled %1").arg(m_tabCounter);
    
    CodeEditor *newEditor = new CodeEditor(m_editors);
    newEditor->setPlaceholderText("Начни печатать...");
    newEditor->setProperty("tabName", tabName);
    newEditor->setProperty("filePath", "");
    newEditor->setProperty("isPinned", false);
    newEditor->setSearchCaseSensitive(m_caseSensitiveAct->isChecked());

    connect(newEditor->document(), &QTextDocument::modificationChanged,
            this, [this, newEditor]() { updateTabTitle(newEditor); });
    connect(newEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    connect(newEditor, &CodeEditor::fileDropped, this, &MainWindow::openFilePath);

    m_editors->addWidget(newEditor);

    QListWidgetItem *item = new QListWidgetItem(tabName);
    item->setData(Qt::UserRole, QVariant::fromValue(static_cast<QObject*>(newEditor)));
    item->setData(Qt::UserRole + 1, false);
    m_sidebar->addItem(item);

    m_sidebar->setCurrentItem(item);

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
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < m_sidebar->count(); ++i) {
        QObject *obj = m_sidebar->item(i)->data(Qt::UserRole).value<QObject*>();
        if (obj == editor) {
            item = m_sidebar->item(i);
            break;
        }
    }
    if (!item) return;

    QString tabName = editor->property("tabName").toString();
    bool isPinned = editor->property("isPinned").toBool();
    
    if (isPinned) {
        tabName = "● " + tabName; 
    }

    if (editor->document()->isModified()) {
        tabName += " *";
    }
    item->setText(tabName);
    
    item->setData(Qt::UserRole + 1, isPinned);
    sortTabs();
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
    QListWidgetItem *item = m_sidebar->currentItem();
    if (!item) return;

    QObject *obj = item->data(Qt::UserRole).value<QObject*>();
    CodeEditor *editor = qobject_cast<CodeEditor*>(obj);
    if (!editor) return;

    if (maybeSave(editor)) {
        m_editors->removeWidget(editor);
        delete editor;
        
        int row = m_sidebar->row(item);
        delete m_sidebar->takeItem(row);

        if (m_sidebar->count() == 0) {
            this->close();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QListWidgetItem *originalItem = m_sidebar->currentItem();

    for (int i = 0; i < m_sidebar->count(); ++i) {
        QListWidgetItem *item = m_sidebar->item(i);
        QObject *obj = item->data(Qt::UserRole).value<QObject*>();
        CodeEditor *editor = qobject_cast<CodeEditor*>(obj);
        if (editor) {
            m_sidebar->setCurrentItem(item);
            if (!maybeSave(editor)) {
                m_sidebar->setCurrentItem(originalItem);
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
    QDialog dialog(this);
    dialog.setWindowTitle("О программе");
    dialog.resize(500, 450);

    dialog.setStyleSheet(R"(
        QDialog { background-color: #1e1e1e; }
        QTabWidget::pane { border: 1px solid #333333; background-color: #1e1e1e; }
        QTabBar::tab { background-color: #2a2d2e; color: #969696; padding: 8px 16px; border: 1px solid #333333; border-bottom: none; }
        QTabBar::tab:selected { background-color: #1e1e1e; color: #ffffff; }
        QTextBrowser { background-color: transparent; color: #d4d4d4; border: none; }
        QLabel { color: #d4d4d4; }
        QPushButton { background-color: #3c3c3c; color: #ffffff; border: 1px solid #555555; padding: 4px 12px; border-radius: 4px; }
        QPushButton:hover { background-color: #4c4c4c; }
    )");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *iconLabel = new QLabel();
    QPixmap logo(":/hofpadwhite.png");
    if (!logo.isNull()) {
        iconLabel->setPixmap(logo.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QLabel *titleLabel = new QLabel(QString("<h2 style='margin: 0;'>Hofpad %1</h2>").arg(QCoreApplication::applicationVersion()));
    titleLabel->setStyleSheet("color: #ffffff;");
    
    headerLayout->addWidget(iconLabel);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    QTabWidget *tabs = new QTabWidget(&dialog);
    
    QWidget *infoTab = new QWidget();
    QVBoxLayout *infoLayout = new QVBoxLayout(infoTab);
    QLabel *infoLabel = new QLabel(
        "<p align='center'><b>Легковесный текстовый редактор</b></p>"
        "<p align='center'>\"Linyphiidae\"</p>"
        "<p align='center'>Использует C++17 и Qt 6</p>"
        "<p align='center'><b>Разработчик:</b> AlecNadrov</p>"
        "<p align='center'><a href='https://github.com/AlecNadrov/Hofpad'>https://github.com/AlecNadrov/Hofpad</a></p>"
    );
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addStretch();
    infoLayout->addWidget(infoLabel);
    infoLayout->addStretch();
   
    QWidget *shortcutsTab = new QWidget();
    QVBoxLayout *shortcutsLayout = new QVBoxLayout(shortcutsTab);
    QTextBrowser *shortcutsBrowser = new QTextBrowser();
    shortcutsBrowser->setHtml(
        "<h3>Работа с файлами</h3>"
        "<ul>"
        "<li><b>Ctrl + T</b> — Новая вкладка</li>"
        "<li><b>Ctrl + O</b> — Открыть файл</li>"
        "<li><b>Ctrl + S</b> — Сохранить</li>"
        "<li><b>Ctrl + Shift + S</b> — Сохранить как...</li>"
        "<li><b>Ctrl + W</b> — Закрыть текущую вкладку</li>"
        "</ul>"
        "<h3>Редактирование</h3>"
        "<ul>"
        "<li><b>Ctrl + Z</b> — Отменить</li>"
        "<li><b>Ctrl + Y</b> / <b>Ctrl + Shift + Z</b> — Вернуть</li>"
        "<li><b>Ctrl + X</b> — Вырезать</li>"
        "<li><b>Ctrl + C</b> — Копировать</li>"
        "<li><b>Ctrl + V</b> — Вставить</li>"
        "</ul>"
        "<h3>Поиск</h3>"
        "<ul>"
        "<li><b>Ctrl + F</b> — Перейти к строке поиска</li>"
        "<li><b>Enter</b> (в строке поиска) — Найти далее</li>"
        "<li><b>Shift + Enter</b> (в строке поиска) — Найти назад</li>"
        "</ul>"
    );
    shortcutsLayout->addWidget(shortcutsBrowser);

    QWidget *licenseTab = new QWidget();
    QVBoxLayout *licenseLayout = new QVBoxLayout(licenseTab);
    QTextBrowser *licenseBrowser = new QTextBrowser();
    licenseBrowser->setPlainText("This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.");
    licenseLayout->addWidget(licenseBrowser);

    tabs->addTab(infoTab, "Информация");
    tabs->addTab(shortcutsTab, "Горячие клавиши");
    tabs->addTab(licenseTab, "Лицензия");
    
    layout->addWidget(tabs);

    QPushButton *closeBtn = new QPushButton("Закрыть");
    closeBtn->setFixedWidth(100);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    dialog.exec();
}
