#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Настройки");
    resize(400, 250);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QFormLayout *formLayout = new QFormLayout();

    m_fontCombo = new QComboBox(this);

    const QStringList allFonts = QFontDatabase::families();
    QStringList monoFonts;
    for (const QString &fontFamily : allFonts) {
        if (QFontDatabase::isFixedPitch(fontFamily)) {
            monoFonts << fontFamily;
        }
    }
    if (monoFonts.isEmpty()) {
        monoFonts << QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }
    m_fontCombo->addItems(monoFonts);

    m_sizeSpin = new QSpinBox(this);
    m_sizeSpin->setRange(8, 72);
    m_sizeSpin->setSuffix(" px");

    m_wrapCheck = new QCheckBox("Включить перенос длинных строк", this);

    m_sessionCheck = new QCheckBox("Запоминать открытые вкладки при выходе", this);

    formLayout->addRow("Шрифт кода:", m_fontCombo);
    formLayout->addRow("Размер шрифта:", m_sizeSpin);
    formLayout->addRow("", m_wrapCheck);
    formLayout->addRow("", m_sessionCheck);
    mainLayout->addLayout(formLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    QSettings settings;

    const QString savedFont = settings.value("font/family", "Monospace").toString();
    if (m_fontCombo->findText(savedFont) < 0) {
        m_fontCombo->insertItem(0, savedFont);
    }
    m_fontCombo->setCurrentText(savedFont);

    m_sizeSpin->setValue(settings.value("font/size", 14).toInt());
    m_wrapCheck->setChecked(settings.value("editor/wordWrap", false).toBool());
    m_sessionCheck->setChecked(settings.value("session/remember", true).toBool());
}

QString SettingsDialog::fontFamily() const { return m_fontCombo->currentText(); }
int SettingsDialog::fontSize() const { return m_sizeSpin->value(); }
bool SettingsDialog::wordWrap() const { return m_wrapCheck->isChecked(); }
bool SettingsDialog::rememberSession() const { return m_sessionCheck->isChecked(); }
