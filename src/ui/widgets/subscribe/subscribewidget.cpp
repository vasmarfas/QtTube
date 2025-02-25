#include "subscribewidget.h"
#include "stores/settingsstore.h"
#include <QtNetwork/QtNetwork>

constexpr const char* subscribersCountStyle = R"(
    border: 1px solid #555;
    font-size: 11px;
    line-height: 24px;
    padding: 0 6px 0 4.5px;
    border-radius: 2px;
    text-align: center;
)";

SubscribeWidget::SubscribeWidget(QWidget* parent)
    : QWidget(parent),
      layout(new QHBoxLayout(this)),
      notificationBell(new NotificationBell(this)),
      subscribeLabel(new SubscribeLabel(this)),
      subscribersCountLabel(new TubeLabel(this))
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(subscribeLabel);

    notificationBell->setVisible(false);
    layout->addWidget(notificationBell);

    subscribersCountLabel->setFixedHeight(24);
    subscribersCountLabel->setStyleSheet(subscribersCountStyle);
    layout->addWidget(subscribersCountLabel);

    connect(subscribeLabel, &SubscribeLabel::subscribeStatusChanged, this, [this](bool subscribed)
    {
        notificationBell->setVisualNotificationState(NotificationBell::NotificationState::Personalized);
        notificationBell->setVisible(subscribed);
    });
}

void SubscribeWidget::setSubscribeButton(const InnertubeObjects::SubscribeButton& subscribeButton)
{
    bool showBell = subscribeButton.subscribed && !subscribeButton.notificationPreferenceButton.states.isEmpty();
    subscribeLabel->setSubscribeButton(subscribeButton);
    notificationBell->setVisible(showBell);
    if (showBell)
        notificationBell->setNotificationPreferenceButton(subscribeButton.notificationPreferenceButton);
}

void SubscribeWidget::setSubscriberCount(const QString& subscriberCountText, const QString& channelId)
{
    if (!SettingsStore::instance()->fullSubs)
    {
        subscribersCountLabel->setText(subscriberCountText.left(subscriberCountText.lastIndexOf(" ")));
        subscribersCountLabel->adjustSize();
        return;
    }

    // QNetworkAccessManager needs to be used here due to a bug with the http library
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    manager->setTransferTimeout(2000);

    QNetworkReply* reply = manager->get(QNetworkRequest("https://api.socialcounts.org/youtube-live-subscriber-count/" + channelId));
    connect(reply, &QNetworkReply::finished, this, [this, reply, subscriberCountText] {
        reply->deleteLater();
        reply->manager()->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            subscribersCountLabel->setText(subscriberCountText.left(subscriberCountText.lastIndexOf(" ")));
            subscribersCountLabel->adjustSize();
        }
        else
        {
            int subs = QJsonDocument::fromJson(reply->readAll())["est_sub"].toInt();
            subscribersCountLabel->setText(QLocale::system().toString(subs));
            subscribersCountLabel->adjustSize();
        }
    });
}
