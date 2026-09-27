#include "ui/settingsdialog.h"

#include "core/settingskeys.h"
#include "platform/autostart.h"
#include "platform/hotkey/globalhotkey.h"
#include "platform/hotkey/keyformat.h"
#include "ui/theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const GlobalHotkey *hotkey, QWidget *parent)
    : QDialog(parent)
    , m_hotkey(hotkey)
{
    setWindowTitle(tr("Deskout Settings"));
    setMinimumWidth(520);

    auto *tabs = new QTabWidget;
    tabs->addTab(buildGeneralTab(), tr("General"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (apply())
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::apply);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    load();
}

QWidget *SettingsDialog::buildGeneralTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *startup = new QGroupBox(tr("Startup"));
    auto *startupLayout = new QVBoxLayout(startup);
    m_autoStart = new QCheckBox(tr("Start Deskout automatically when I log in"));
    m_autoStart->setEnabled(AutoStart::isSupported());
    startupLayout->addWidget(m_autoStart);
    layout->addWidget(startup);

    auto *shortcut = new QGroupBox(tr("Pause / resume everything"));
    auto *shortcutLayout = new QVBoxLayout(shortcut);
    m_hotkeyEnabled = new QCheckBox(tr("Enable system-wide shortcut"));
    shortcutLayout->addWidget(m_hotkeyEnabled);

    auto *editRow = new QHBoxLayout;
    m_hotkeyEdit = new QKeySequenceEdit;
    m_hotkeyReset = new QPushButton(tr("Reset to default"));
    editRow->addWidget(m_hotkeyEdit, 1);
    editRow->addWidget(m_hotkeyReset);
    shortcutLayout->addLayout(editRow);

    m_hotkeyStatus = new QLabel;
    m_hotkeyStatus->setWordWrap(true);
    m_hotkeyStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    shortcutLayout->addWidget(m_hotkeyStatus);
    layout->addWidget(shortcut);

    connect(m_hotkeyReset, &QPushButton::clicked, this, [this] {
        m_hotkeyEdit->setKeySequence(QKeySequence(QLatin1String(SettingsKeys::HotkeyDefaultSequence)));
        validateShortcut();
    });
    // Global shortcuts are a single key combination; drop any extra chords.
    connect(m_hotkeyEdit, &QKeySequenceEdit::editingFinished, this, [this] {
        const QKeySequence seq = m_hotkeyEdit->keySequence();
        if (seq.count() > 1)
            m_hotkeyEdit->setKeySequence(QKeySequence(seq[0]));
        validateShortcut();
    });
    connect(m_hotkeyEnabled, &QCheckBox::toggled, m_hotkeyEdit, &QWidget::setEnabled);
    connect(m_hotkeyEnabled, &QCheckBox::toggled, m_hotkeyReset, &QWidget::setEnabled);

    auto *appearance = new QGroupBox(tr("Appearance"));
    auto *appearanceLayout = new QFormLayout(appearance);
    m_theme = new QComboBox;
    m_theme->addItem(tr("Match system"), int(Theme::Mode::System));
    m_theme->addItem(tr("Light"), int(Theme::Mode::Light));
    m_theme->addItem(tr("Dark"), int(Theme::Mode::Dark));
    appearanceLayout->addRow(tr("Theme"), m_theme);
    layout->addWidget(appearance);

    layout->addStretch(1);
    return page;
}

void SettingsDialog::load()
{
    QSettings settings;
    m_autoStart->setChecked(AutoStart::isEnabled());

    const bool hotkeyOn = settings.value(SettingsKeys::HotkeyEnabled, true).toBool();
    m_hotkeyEnabled->setChecked(hotkeyOn);
    m_hotkeyEdit->setEnabled(hotkeyOn);
    m_hotkeyReset->setEnabled(hotkeyOn);
    m_hotkeyEdit->setKeySequence(QKeySequence(
        settings.value(SettingsKeys::HotkeySequence, QLatin1String(SettingsKeys::HotkeyDefaultSequence))
            .toString()));

    m_theme->setCurrentIndex(m_theme->findData(int(Theme::savedMode())));
    refreshHotkeyStatus();
}

bool SettingsDialog::apply()
{
    if (m_hotkeyEnabled->isChecked()) {
        QString reason;
        const QKeySequence seq = m_hotkeyEdit->keySequence();
        if (!KeyFormat::isUsableGlobalShortcut(seq.isEmpty() ? QKeyCombination() : seq[0], &reason)) {
            QMessageBox::warning(this, tr("Shortcut"), reason);
            return false;
        }
    }

    if (m_autoStart->isChecked() != AutoStart::isEnabled()) {
        QString error;
        if (!AutoStart::setEnabled(m_autoStart->isChecked(), &error)) {
            QMessageBox::warning(this, tr("Launch at login"), error);
            m_autoStart->setChecked(AutoStart::isEnabled());
        }
    }

    QSettings settings;
    settings.setValue(SettingsKeys::HotkeyEnabled, m_hotkeyEnabled->isChecked());
    settings.setValue(SettingsKeys::HotkeySequence,
                      m_hotkeyEdit->keySequence().toString(QKeySequence::PortableText));
    Theme::saveMode(Theme::Mode(m_theme->currentData().toInt()));

    Q_EMIT applied();
    refreshHotkeyStatus();
    return true;
}

void SettingsDialog::validateShortcut()
{
    QString reason;
    const QKeySequence seq = m_hotkeyEdit->keySequence();
    if (!KeyFormat::isUsableGlobalShortcut(seq.isEmpty() ? QKeyCombination() : seq[0], &reason))
        m_hotkeyStatus->setText(reason);
    else
        m_hotkeyStatus->setText(tr("Press Apply to register %1.")
                                    .arg(seq.toString(QKeySequence::NativeText)));
}

void SettingsDialog::refreshHotkeyStatus()
{
    if (!m_hotkeyEnabled->isChecked()) {
        m_hotkeyStatus->setText(tr("Shortcut disabled."));
    } else if (m_hotkey->isRegistered()) {
        m_hotkeyStatus->setText(tr("Active: %1 via %2.")
                                    .arg(m_hotkey->shortcut().toString(QKeySequence::NativeText),
                                         m_hotkey->backendName()));
    } else {
        m_hotkeyStatus->setText(tr("Not active (%1): %2")
                                    .arg(m_hotkey->backendName(), m_hotkey->lastError()));
    }
}
