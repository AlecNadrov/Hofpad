#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QSpinBox;
class QCheckBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString fontFamily() const;
    int fontSize() const;
    bool wordWrap() const;
    bool rememberSession() const;

private:
    QComboBox *m_fontCombo;
    QSpinBox *m_sizeSpin;
    QCheckBox *m_wrapCheck;
    QCheckBox *m_sessionCheck;
};

#endif // SETTINGSDIALOG_H
