#include <gtest/gtest.h>

#include <QTestEventList>
#include <QScopedPointer>

#include "mixxxtest.h"
#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "widget/wpushbutton.h"
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
