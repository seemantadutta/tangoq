#include "preferences/dialog/dlgprefautodj.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QTimeZone>
#endif

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "library/autodj/autodjfeature.h"
#include "moc_dlgprefautodj.cpp"

namespace {

const ConfigKey kStateMonitorEnabledKey(
        QStringLiteral("[TangoQ]"), QStringLiteral("StateMonitorEnabled"));
const ConfigKey kStateMonitorPortKey(
        QStringLiteral("[TangoQ]"), QStringLiteral("StateMonitorPort"));
constexpr int kDefaultStateMonitorPort = 39087;

} // namespace

DlgPrefAutoDJ::DlgPrefAutoDJ(QWidget* pParent,
        UserSettingsPointer pConfig,
        AutoDJFeature* pAutoDJFeature)
        : DlgPreferencePage(pParent),
          m_pConfig(pConfig),
          m_pAutoDJFeature(pAutoDJFeature),
          m_pCortinaLengthControl(nullptr) {
    setupUi(this);

    connect(StateMonitorEnableCheckBox,
            &QCheckBox::toggled,
            this,
            &DlgPrefAutoDJ::slotStateMonitorToggled);

    // The minimum available for randomly-selected tracks
    MinimumAvailableSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "MinimumAvailable"), 20));
    connect(MinimumAvailableSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefAutoDJ::slotSetMinimumAvailable);

    // The auto-DJ replay-age for randomly-selected tracks
    RequeueIgnoreCheckBox->setChecked(m_pConfig->getValue(
            ConfigKey("[Auto DJ]", "UseIgnoreTime"), false));
    connect(RequeueIgnoreCheckBox,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefAutoDJ::slotToggleRequeueIgnore);
    /// TODO: Once we require at least Qt 6.7, remove this `setTimeZone` call
    /// and uncomment the corresponding declarations in the UI file instead.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    RequeueIgnoreTimeEdit->setTimeZone(QTimeZone::LocalTime);
#else
    RequeueIgnoreTimeEdit->setTimeSpec(Qt::LocalTime);
#endif
    RequeueIgnoreTimeEdit->setTime(
            QTime::fromString(
                    m_pConfig->getValue(
                            ConfigKey("[Auto DJ]", "IgnoreTime"), "23:59"),
                    RequeueIgnoreTimeEdit->displayFormat()));
    RequeueIgnoreTimeEdit->setEnabled(
            RequeueIgnoreCheckBox->checkState() == Qt::Checked);
    connect(RequeueIgnoreTimeEdit,
            &QTimeEdit::timeChanged,
            this,
            &DlgPrefAutoDJ::slotSetRequeueIgnoreTime);

    // Auto DJ random enqueue
    RandomQueueCheckBox->setChecked(m_pConfig->getValue(
            ConfigKey("[Auto DJ]", "EnableRandomQueue"), false));
    // 5-arbitrary
    RandomQueueMinimumSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowed"), 5));
    // "[Auto DJ], Requeue" is set by 'Repeat Playlist' toggle in DlgAutoDj GUI.
    // If it's checked un-check 'Random Queue'
    slotConsiderRepeatPlaylistState(
            m_pConfig->getValue<bool>(ConfigKey("[Auto DJ]", "Requeue")));
    slotToggleRandomQueue(
            m_pConfig->getValue<bool>(
                    ConfigKey("[Auto DJ]", "EnableRandomQueue"))
                    ? Qt::Checked
                    : Qt::Unchecked);
    // Be ready to enable and modify the minimum number and un/check the checkbox
    connect(RandomQueueCheckBox,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
#else
            &QCheckBox::stateChanged,
#endif
            this,
            &DlgPrefAutoDJ::slotToggleRandomQueue);
    connect(RandomQueueMinimumSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefAutoDJ::slotSetRandomQueueMin);

    // Tango DJ mode is hardcoded on in this build (see AutoDJProcessor), so there
    // is no enable/disable control here. The cortina settings below still apply.

    // Re-queue and random-track addition are stock Auto DJ behaviours the Tango
    // cursor queue never uses: it plays in order and stops at the end. Hide those
    // sections in this tango-only build. The controls above are still wired up so
    // the stock logic is intact and cleanly revertible.
    RequeueOptions->hide();
    AddRandomOptions->hide();

    // Default cortina length, used only to estimate the Tango set length / end
    // time (cortinas are faded out manually, so only this budget is counted).
    int cortinaLength =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaLength"), 45);
    CortinaLengthSpinBox->setValue(cortinaLength);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaLengthBuff"), cortinaLength);
    connect(CortinaLengthSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefAutoDJ::slotSetCortinaLength);
    // The cortina length can also be nudged live from the Auto DJ toolbar. Mirror
    // those changes into this (stop-only) field so it always shows the current
    // value, even while greyed out during a running set.
    m_pCortinaLengthControl = new ControlProxy(
            ConfigKey("[AutoDJ]", "cortina_length"), this);
    m_pCortinaLengthControl->connectValueChanged(this, [this](double v) {
        const int seconds = static_cast<int>(v);
        if (CortinaLengthSpinBox->value() != seconds) {
            CortinaLengthSpinBox->setValue(seconds);
        }
    });

    // Cortina transition mode: 0 = hard cut (current; cortina starts at full and
    // is faded out manually), 1 = Cortina Fade (the engine fades the cortina in
    // and out automatically over the times below).
    int cortinaFadeMode =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeMode"), 0);
    CortinaFadeModeComboBox->setCurrentIndex(cortinaFadeMode);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeModeBuff"), cortinaFadeMode);
    connect(CortinaFadeModeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &DlgPrefAutoDJ::slotSetCortinaFadeMode);

    // Cortina fade-in (X) and fade-out (Z); the hold time Y = cortina length - X
    // - Z is derived and shown read-only.
    int cortinaFadeIn =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeIn"), 5);
    CortinaFadeInSpinBox->setValue(cortinaFadeIn);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeInBuff"), cortinaFadeIn);
    connect(CortinaFadeInSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefAutoDJ::slotSetCortinaFadeIn);

    int cortinaFadeOut =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeOut"), 5);
    CortinaFadeOutSpinBox->setValue(cortinaFadeOut);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeOutBuff"), cortinaFadeOut);
    connect(CortinaFadeOutSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &DlgPrefAutoDJ::slotSetCortinaFadeOut);

    updateCortinaHoldLabel();
    updateCortinaFadeEnabled();

    setScrollSafeGuardForAllInputWidgets(this);
}

void DlgPrefAutoDJ::slotSetCortinaLength(int seconds) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaLengthBuff"), seconds);
    // The hold time depends on the cortina length, so keep its readout current.
    updateCortinaHoldLabel();
}

void DlgPrefAutoDJ::slotSetCortinaFadeMode(int index) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeModeBuff"), index);
    updateCortinaFadeEnabled();
}

void DlgPrefAutoDJ::slotSetCortinaFadeIn(int seconds) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeInBuff"), seconds);
    updateCortinaHoldLabel();
}

void DlgPrefAutoDJ::slotSetCortinaFadeOut(int seconds) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeOutBuff"), seconds);
    updateCortinaHoldLabel();
}

void DlgPrefAutoDJ::updateCortinaHoldLabel() {
    const int cortinaLength = CortinaLengthSpinBox->value();
    const int hold = cortinaLength -
            CortinaFadeInSpinBox->value() - CortinaFadeOutSpinBox->value();
    if (hold >= 0) {
        CortinaHoldLabel->setText(tr("Cortina hold time: %1 s").arg(hold));
    } else {
        // Fade-in + fade-out exceed the cortina length. The engine clamps at
        // runtime, but surface it so the user can correct the values.
        CortinaHoldLabel->setText(
                tr("Cortina hold time: 0 s — fade-in + fade-out exceed the "
                   "cortina length (%1 s)")
                        .arg(cortinaLength));
    }
}

void DlgPrefAutoDJ::updateCortinaFadeEnabled() {
    // The fade-in/out inputs only apply in Cortina Fade mode, and (like the
    // other Tango set-timing inputs) only while Auto DJ is stopped.
    const bool autoDJRunning =
            ControlObject::get(ConfigKey("[AutoDJ]", "enabled")) > 0.0;
    const bool enabled =
            CortinaFadeModeComboBox->currentIndex() == 1 && !autoDJRunning;
    CortinaFadeInLabel->setEnabled(enabled);
    CortinaFadeInSpinBox->setEnabled(enabled);
    CortinaFadeOutLabel->setEnabled(enabled);
    CortinaFadeOutSpinBox->setEnabled(enabled);
    CortinaHoldLabel->setEnabled(enabled);
}

void DlgPrefAutoDJ::slotUpdate() {
    const bool autoDJRunning =
            ControlObject::get(ConfigKey("[AutoDJ]", "enabled")) > 0.0;
    // The cortina length feeds the set-length estimate, which is only recomputed
    // when Auto DJ is stopped; lock it while a set is running.
    CortinaLengthSpinBox->setEnabled(!autoDJRunning);
    // Cortina transition mode and its fade inputs likewise change behavior that
    // is only safe to alter while Auto DJ is stopped.
    CortinaFadeModeComboBox->setEnabled(!autoDJRunning);
    updateCortinaFadeEnabled();
    m_stateMonitorConfigValid = true;
    const bool monitorEnabled =
            m_pAutoDJFeature && m_pAutoDJFeature->semanticMonitorEnabled();
    StateMonitorEnableCheckBox->setChecked(monitorEnabled);
    StateMonitorPortSpinBox->setValue(monitorEnabled
                    ? m_pAutoDJFeature->semanticMonitorPort()
                    : m_pConfig->getValue(
                              kStateMonitorPortKey, kDefaultStateMonitorPort));
    updateStateMonitorStatus();
}

void DlgPrefAutoDJ::slotApply() {
    const bool monitorEnabled = StateMonitorEnableCheckBox->isChecked();
    const int monitorPort = StateMonitorPortSpinBox->value();
    QString error;
    m_stateMonitorConfigValid = m_pAutoDJFeature &&
            m_pAutoDJFeature->setSemanticMonitorEnabled(
                    monitorEnabled, static_cast<quint16>(monitorPort), &error);
    if (m_stateMonitorConfigValid) {
        m_pConfig->setValue(kStateMonitorEnabledKey, monitorEnabled);
        m_pConfig->setValue(kStateMonitorPortKey, monitorPort);
        updateStateMonitorStatus();
    } else {
        StateMonitorStatusLabel->setText(error.isEmpty()
                        ? tr("The state monitor is unavailable.")
                        : error);
    }

    // Copy from Buffer to actual values
    //  Route cortina length through the live control so the AutoDJProcessor
    //  persists it to [Auto DJ],CortinaLength and the cockpit readout updates
    //  immediately.
    ControlObject::set(ConfigKey("[AutoDJ]", "cortina_length"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "CortinaLengthBuff"), 45));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeMode"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "CortinaFadeModeBuff"), 0));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeIn"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "CortinaFadeInBuff"), 5));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeOut"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "CortinaFadeOutBuff"), 5));
    m_pConfig->setValue(ConfigKey("[Auto DJ]","MinimumAvailable"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "MinimumAvailableBuff"), 20));

    m_pConfig->setValue(ConfigKey("[Auto DJ]", "IgnoreTime"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "IgnoreTimeBuff"), "23:59"));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "UseIgnoreTime"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "UseIgnoreTimeBuff"), false));

    m_pConfig->setValue(ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowed"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowedBuff"), 5));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueue"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"), false));
}

void DlgPrefAutoDJ::slotCancel() {
    // Load actual values and reset Buffer Values where ever needed
    int cortinaLength =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaLength"), 45);
    CortinaLengthSpinBox->setValue(cortinaLength);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaLengthBuff"), cortinaLength);

    int cortinaFadeMode =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeMode"), 0);
    CortinaFadeModeComboBox->setCurrentIndex(cortinaFadeMode);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeModeBuff"), cortinaFadeMode);

    int cortinaFadeIn =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeIn"), 5);
    CortinaFadeInSpinBox->setValue(cortinaFadeIn);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeInBuff"), cortinaFadeIn);

    int cortinaFadeOut =
            m_pConfig->getValue(ConfigKey("[Auto DJ]", "CortinaFadeOut"), 5);
    CortinaFadeOutSpinBox->setValue(cortinaFadeOut);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeOutBuff"), cortinaFadeOut);

    updateCortinaHoldLabel();
    updateCortinaFadeEnabled();

    m_stateMonitorConfigValid = true;
    const bool monitorEnabled =
            m_pAutoDJFeature && m_pAutoDJFeature->semanticMonitorEnabled();
    StateMonitorEnableCheckBox->setChecked(monitorEnabled);
    StateMonitorPortSpinBox->setValue(monitorEnabled
                    ? m_pAutoDJFeature->semanticMonitorPort()
                    : m_pConfig->getValue(
                              kStateMonitorPortKey, kDefaultStateMonitorPort));
    updateStateMonitorStatus();

    MinimumAvailableSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "MinimumAvailable"), 20));

    RequeueIgnoreTimeEdit->setTime(
            QTime::fromString(
                    m_pConfig->getValue(
                            ConfigKey("[Auto DJ]", "IgnoreTime"), "23:59"),
                    RequeueIgnoreTimeEdit->displayFormat()));
    RequeueIgnoreCheckBox->setChecked(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "UseIgnoreTime"), false));
    RequeueIgnoreTimeEdit->setEnabled(
            RequeueIgnoreCheckBox->checkState() == Qt::Checked);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "UseIgnoreTimeBuff"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "UseIgnoreTime"), false));

    RandomQueueMinimumSpinBox->setValue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowed"), 5));
    RandomQueueCheckBox->setChecked(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "EnableRandomQueue"), false));
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"),
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "EnableRandomQueue"), false));
    slotToggleRandomQueue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "EnableRandomQueue"), false)
                    ? Qt::Checked
                    : Qt::Unchecked);
    slotToggleRandomQueue(
            m_pConfig->getValue(
                    ConfigKey("[Auto DJ]", "Requeue"), false)
                    ? Qt::Checked
                    : Qt::Unchecked);
}

void DlgPrefAutoDJ::slotResetToDefaults() {
    StateMonitorEnableCheckBox->setChecked(false);
    StateMonitorPortSpinBox->setValue(kDefaultStateMonitorPort);

    CortinaLengthSpinBox->setValue(45);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaLengthBuff"), 45);

    // Cortina Fade (index 1) is the shipped default: applyFirstRunDefaults()
    // seeds CortinaFadeMode = 1, so Restore Defaults must match it rather than
    // silently reverting cortinas to hard cut.
    CortinaFadeModeComboBox->setCurrentIndex(1);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeModeBuff"), 1);
    CortinaFadeInSpinBox->setValue(5);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeInBuff"), 5);
    CortinaFadeOutSpinBox->setValue(5);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "CortinaFadeOutBuff"), 5);

    updateCortinaHoldLabel();
    updateCortinaFadeEnabled();

    // Re-queue tracks in AutoDJ
    MinimumAvailableSpinBox->setValue(20);

    RequeueIgnoreTimeEdit->setTime(QTime::fromString(
            "23:59", RequeueIgnoreTimeEdit->displayFormat()));
    RequeueIgnoreCheckBox->setChecked(false);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "UseIgnoreTimeBuff"), false);
    RequeueIgnoreTimeEdit->setEnabled(false);

    RandomQueueMinimumSpinBox->setValue(5);
    RandomQueueCheckBox->setChecked(false);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"), false);
    RandomQueueMinimumSpinBox->setEnabled(false);
    RandomQueueCheckBox->setEnabled(true);
}

bool DlgPrefAutoDJ::okayToClose() const {
    return m_stateMonitorConfigValid;
}

void DlgPrefAutoDJ::slotStateMonitorToggled(bool enabled) {
    StateMonitorPortLabel->setEnabled(enabled);
    StateMonitorPortSpinBox->setEnabled(enabled);
    m_stateMonitorConfigValid = true;
    updateStateMonitorStatus();
}

void DlgPrefAutoDJ::updateStateMonitorStatus() {
    const bool running =
            m_pAutoDJFeature && m_pAutoDJFeature->semanticMonitorEnabled();
    if (StateMonitorEnableCheckBox->isChecked() != running) {
        StateMonitorStatusLabel->setText(
                StateMonitorEnableCheckBox->isChecked()
                        ? tr("Stopped; will start when applied")
                        : tr("Listening; will stop when applied"));
        return;
    }
    if (!running) {
        StateMonitorStatusLabel->setText(tr("Stopped"));
        return;
    }
    StateMonitorStatusLabel->setText(
            tr("Listening at %1")
                    .arg(m_pAutoDJFeature->semanticMonitorUrls().join(", ")));
}

void DlgPrefAutoDJ::slotSetMinimumAvailable(int a_iValue) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "MinimumAvailableBuff"), a_iValue);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefAutoDJ::slotToggleRequeueIgnore(Qt::CheckState buttonState) {
#else
void DlgPrefAutoDJ::slotToggleRequeueIgnore(int buttonState) {
#endif
    bool checked = buttonState == Qt::Checked;
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "UseIgnoreTimeBuff"), checked);
    RequeueIgnoreTimeEdit->setEnabled(checked);
}

void DlgPrefAutoDJ::slotSetRequeueIgnoreTime(const QTime& a_rTime) {
    QString str = a_rTime.toString(RequeueIgnoreTimeEdit->displayFormat());
    m_pConfig->set(ConfigKey("[Auto DJ]", "IgnoreTimeBuff"), str);
}

void DlgPrefAutoDJ::slotSetRandomQueueMin(int a_iValue) {
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "RandomQueueMinimumAllowedBuff"), a_iValue);
}

void DlgPrefAutoDJ::slotConsiderRepeatPlaylistState(bool enable) {
    if (enable) {
        // Requeue is enabled
        RandomQueueCheckBox->setChecked(false);
        // ToDo(ronso0): Redundant? If programmatic checkbox change is signaled
        // to slotToggleRandomQueue
        RandomQueueMinimumSpinBox->setEnabled(false);
        m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"),
                false);
    } else {
        RandomQueueMinimumSpinBox->setEnabled(
                m_pConfig->getValue(
                        ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"), false));
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void DlgPrefAutoDJ::slotToggleRandomQueue(Qt::CheckState buttonState) {
#else
void DlgPrefAutoDJ::slotToggleRandomQueue(int buttonState) {
#endif
    bool enable = buttonState == Qt::Checked;
    // Toggle the option to select minimum tracks
    RandomQueueMinimumSpinBox->setEnabled(enable);
    m_pConfig->setValue(ConfigKey("[Auto DJ]", "EnableRandomQueueBuff"),
            enable);
}
