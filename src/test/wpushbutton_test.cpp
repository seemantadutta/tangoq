#include "widget/wpushbutton.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QScopedPointer>
#include <QTestEventList>
#include <QWidget>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "mixxxtest.h"
#include "widget/controlwidgetconnection.h"

class WPushButtonTest : public MixxxTest {
  public:
    WPushButtonTest()
          : m_pGroup("[Channel1]") {
    }

  protected:
    void SetUp() override {
        m_pTouchShift.reset(new ControlPushButton(ConfigKey("[Controls]", "touch_shift")));
        m_pButton.reset(new WPushButton());
        m_pButton->setStates(2);
    }

    QScopedPointer<WPushButton> m_pButton;
    QScopedPointer<ControlPushButton> m_pTouchShift;
    QTestEventList m_Events;
    const char* m_pGroup;
};

TEST_F(WPushButtonTest, QuickPressNoLatchTest) {
    QScopedPointer<ControlPushButton> pPushControl(
        new ControlPushButton(ConfigKey("[Test]", "push")));
    pPushControl->setButtonMode(ControlPushButton::LONGPRESSLATCHING);

    m_pButton.reset(new WPushButton(NULL, ControlPushButton::LONGPRESSLATCHING,
                                    ControlPushButton::PUSH));
    m_pButton->setStates(2);
    m_pButton->addLeftConnection(
        new ControlParameterWidgetConnection(
            m_pButton.data(),
            pPushControl->getKey(), NULL,
            ControlParameterWidgetConnection::DIR_FROM_AND_TO_WIDGET,
            ControlParameterWidgetConnection::EMIT_ON_PRESS_AND_RELEASE));

    // This test can be flaky if the event simulator takes too long to deliver
    // the event.
    m_Events.addMousePress(Qt::LeftButton);
    m_Events.addDelay(100);
    m_Events.addMouseRelease(Qt::LeftButton);

    m_Events.simulate(m_pButton.data());

    ASSERT_EQ(0.0, m_pButton->getControlParameterLeft());
}

TEST_F(WPushButtonTest, LongPressLatchTest) {
    QScopedPointer<ControlPushButton> pPushControl(
        new ControlPushButton(ConfigKey("[Test]", "push")));
    pPushControl->setButtonMode(ControlPushButton::LONGPRESSLATCHING);

    m_pButton.reset(new WPushButton(NULL, ControlPushButton::LONGPRESSLATCHING,
                                    ControlPushButton::PUSH));
    m_pButton->setStates(2);
    m_pButton->addLeftConnection(
        new ControlParameterWidgetConnection(
            m_pButton.data(),
            pPushControl->getKey(), NULL,
            ControlParameterWidgetConnection::DIR_FROM_AND_TO_WIDGET,
            ControlParameterWidgetConnection::EMIT_ON_PRESS_AND_RELEASE));

    m_Events.addMousePress(Qt::LeftButton);
    m_Events.addDelay(1000);
    m_Events.addMouseRelease(Qt::LeftButton);

    m_Events.simulate(m_pButton.data());

    ASSERT_EQ(1.0, m_pButton->getControlParameterLeft());
}

TEST_F(WPushButtonTest, LiveModeRequiresSecondPressAndAllowsImmediateRecovery) {
    QScopedPointer<ControlPushButton> pPlayControl(
            new ControlPushButton(ConfigKey(m_pGroup, "play")));
    pPlayControl->setButtonMode(ControlPushButton::TOGGLE);
    pPlayControl->set(1.0);
    QScopedPointer<ControlObject> pLiveMode(
            new ControlObject(ConfigKey("[AutoDJ]", "live_mode")));
    QScopedPointer<ControlObject> pKeepQueue(
            new ControlObject(ConfigKey("[AutoDJ]", "keep_queue")));
    pLiveMode->set(1.0);
    pKeepQueue->set(1.0);

    m_pButton.reset(new WPushButton(
            nullptr, ControlPushButton::TOGGLE, ControlPushButton::PUSH));
    m_pButton->setStates(2);
    m_pButton->addLeftConnection(new ControlParameterWidgetConnection(
            m_pButton.data(),
            pPlayControl->getKey(),
            nullptr,
            ControlParameterWidgetConnection::DIR_FROM_AND_TO_WIDGET,
            ControlParameterWidgetConnection::EMIT_ON_PRESS));

    QTestEventList firstClick;
    firstClick.addMouseClick(Qt::LeftButton);
    firstClick.simulate(m_pButton.data());
    EXPECT_EQ(1.0, pPlayControl->get());

    QTestEventList secondClick;
    secondClick.addMouseClick(Qt::LeftButton);
    secondClick.simulate(m_pButton.data());
    EXPECT_EQ(0.0, pPlayControl->get());

    QTestEventList thirdClick;
    thirdClick.addMouseClick(Qt::LeftButton);
    thirdClick.simulate(m_pButton.data());
    EXPECT_EQ(1.0, pPlayControl->get());

    QScopedPointer<ControlPushButton> pInactivePlayControl(
            new ControlPushButton(ConfigKey("[Channel2]", "play")));
    pInactivePlayControl->setButtonMode(ControlPushButton::TOGGLE);
    pInactivePlayControl->set(0.0);
    QScopedPointer<WPushButton> pInactiveButton(new WPushButton(
            nullptr, ControlPushButton::TOGGLE, ControlPushButton::PUSH));
    pInactiveButton->setStates(2);
    pInactiveButton->addLeftConnection(new ControlParameterWidgetConnection(
            pInactiveButton.data(),
            pInactivePlayControl->getKey(),
            nullptr,
            ControlParameterWidgetConnection::DIR_FROM_AND_TO_WIDGET,
            ControlParameterWidgetConnection::EMIT_ON_PRESS));

    QTestEventList inactiveFirstClick;
    inactiveFirstClick.addMouseClick(Qt::LeftButton);
    inactiveFirstClick.simulate(pInactiveButton.data());
    EXPECT_EQ(0.0, pInactivePlayControl->get());

    QTestEventList inactiveSecondClick;
    inactiveSecondClick.addMouseClick(Qt::LeftButton);
    inactiveSecondClick.simulate(pInactiveButton.data());
    EXPECT_EQ(1.0, pInactivePlayControl->get());

    QTestEventList inactiveRecoveryClick;
    inactiveRecoveryClick.addMouseClick(Qt::LeftButton);
    inactiveRecoveryClick.simulate(pInactiveButton.data());
    EXPECT_EQ(0.0, pInactivePlayControl->get());
}

TEST_F(WPushButtonTest, GrabDisplayStateRendersAnotherStateAndRestores) {
    // The LIVE stop guard drains from the playing look to the paused look, both
    // grabbed from the button itself. The [displayValue] QSS selectors are only
    // re-read on polish, so check the off-state grab really shows state 0 and
    // that the button returns to its real state afterwards.
    ControlObject control(ConfigKey("[Test]", "display_state"));
    control.set(1.0);
    auto* pConnection = new ControlParameterWidgetConnection(m_pButton.data(),
            control.getKey(),
            nullptr,
            ControlParameterWidgetConnection::DIR_TO_WIDGET,
            ControlParameterWidgetConnection::EMIT_NEVER);
    m_pButton->addConnection(pConnection);
    m_pButton->setDisplayConnection(pConnection);
    pConnection->Init();
    m_pButton->resize(20, 20);
    m_pButton->setStyleSheet(QStringLiteral(
            "WPushButton[displayValue=\"0\"] { background-color: #000000; }"
            "WPushButton[displayValue=\"1\"] { background-color: #b24c12; }"));
    m_pButton->ensurePolished();
    ASSERT_EQ(1, m_pButton->readDisplayValue());

    const QImage off = m_pButton->grabDisplayState(0).toImage();
    EXPECT_EQ(QColor(QStringLiteral("#000000")), off.pixelColor(10, 10));

    EXPECT_EQ(1, m_pButton->readDisplayValue());
    const QImage on = m_pButton->grab().toImage();
    EXPECT_EQ(QColor(QStringLiteral("#b24c12")), on.pixelColor(10, 10));
}

TEST_F(WPushButtonTest, GrabDisplayStateIncludesStackedIndicators) {
    // A deck play button is transparent and sits over an indicator button that
    // paints the "playing" fill. Its snapshots must show the whole stack, with
    // the indicator in the requested state too, or only the edges drain.
    ControlObject control(ConfigKey("[Test]", "stacked_state"));
    control.set(1.0);
    QWidget host;
    host.resize(20, 20);
    const auto makeLayer = [&host, &control](const QString& name) {
        auto* pButton = new WPushButton(&host);
        pButton->setObjectName(name);
        pButton->setStates(2);
        auto* pConnection = new ControlParameterWidgetConnection(pButton,
                control.getKey(),
                nullptr,
                ControlParameterWidgetConnection::DIR_TO_WIDGET,
                ControlParameterWidgetConnection::EMIT_NEVER);
        pButton->addConnection(pConnection);
        pButton->setDisplayConnection(pConnection);
        pConnection->Init();
        pButton->setGeometry(0, 0, 20, 20);
        return pButton;
    };
    makeLayer(QStringLiteral("Indicator"));
    WPushButton* pPlay = makeLayer(QStringLiteral("Play"));
    host.setStyleSheet(QStringLiteral(
            "#Indicator[displayValue=\"0\"] { background-color: #000000; }"
            "#Indicator[displayValue=\"1\"] { background-color: #b24c12; }"));
    host.show();
    host.ensurePolished();

    EXPECT_EQ(QColor(QStringLiteral("#b24c12")),
            pPlay->grabDisplayState(-1).toImage().pixelColor(10, 10));
    EXPECT_EQ(QColor(QStringLiteral("#000000")),
            pPlay->grabDisplayState(0).toImage().pixelColor(10, 10));
    // Both layers are back in their real state afterwards.
    EXPECT_EQ(QColor(QStringLiteral("#b24c12")),
            pPlay->grabDisplayState(-1).toImage().pixelColor(10, 10));
}
