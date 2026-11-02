#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>

class QSplitter;
class QListWidget;
class QStackedWidget;
class QLabel;
class QMenu;
class QLineEdit;
class QToolBar;
class QAction;
class QTimer;
class CodeEditor;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void addNewTab();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void closeCurrentTab();
    void updateStatusBar();
    void showAboutDialog();
    void openSettings();

    void showSearchBar();
    void performSearch();
    void searchNext();
    void searchPrev();

private:
    QSplitter *m_splitter;
    QListWidget *m_sidebar;
    QStackedWidget *m_editors;
    int m_tabCounter = 0;

    QLabel *m_statusCursor;
    QLabel *m_statusEncoding;

    QMenu *m_recentMenu;
    QStringList m_recentFiles;

    QToolBar *m_toolBar;
    QLineEdit *m_searchInput;
    QAction *m_searchNextAct;
    QAction *m_searchPrevAct;
    QAction *m_caseSensitiveAct;
    QTimer  *m_searchTimer;

    CodeEditor* currentEditor() const;
    void updateTabTitle(CodeEditor *editor);
    bool maybeSave(CodeEditor *editor);
    bool saveEditor(CodeEditor *editor);
    bool saveToPath(CodeEditor *editor, const QString &filePath);
    void removeTab(int index);
    void openFilePath(const QString &filePath);

    void applySettings();
    void applySettingsTo(CodeEditor *editor);
    void addToRecentFiles(const QString &filePath);
    void updateRecentFilesMenu();
    void loadSession();
    void saveSession();
};

#endif // MAINWINDOW_H
