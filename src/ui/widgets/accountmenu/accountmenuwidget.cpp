#include "accountmenuwidget.h"
#include "http.h"
#include "ui/forms/mainwindow.h"
#include "ui/views/viewcontroller.h"
#include <QApplication>

AccountMenuWidget::AccountMenuWidget(QWidget* parent)
    : QWidget(parent),
      accountLayout(new QVBoxLayout),
      accountNameLabel(new QLabel(this)),
      avatar(new QLabel(this)),
      handleLabel(new QLabel(this)),
      headerLayout(new QHBoxLayout),
      layout(new QVBoxLayout(this)),
      signOutLabel(new IconLabel("sign-out", "Sign out", QMargins(), QSize(24, 24), this)),
      switchAccountsLabel(new IconLabel("switch-accounts", "Switch account", QMargins(), QSize(24, 24), this)),
      yourChannelLabel(new IconLabel("your-channel", "Your channel", QMargins(), QSize(24, 24), this))
{
    accountNameLabel->setFont(QFont(qApp->font().toString(), -1, QFont::Bold));
    avatar->setFixedSize(64, 64);
    setAutoFillBackground(true);

    accountLayout->addWidget(accountNameLabel);
    accountLayout->addWidget(handleLabel);

    headerLayout->addWidget(avatar);
    headerLayout->addLayout(accountLayout, 1);

    layout->addLayout(headerLayout);
    layout->addWidget(switchAccountsLabel);
    layout->addWidget(yourChannelLabel);
    layout->addWidget(signOutLabel);
    layout->addStretch();
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(switchAccountsLabel, &IconLabel::clicked, this, &AccountMenuWidget::accountSwitcherRequested);
    connect(signOutLabel, &IconLabel::clicked, this, &AccountMenuWidget::triggerSignOut);
}

void AccountMenuWidget::initialize(const InnertubeEndpoints::AccountMenu& endpoint)
{
    accountNameLabel->setText(endpoint.response.header.accountName);
    handleLabel->setText(endpoint.response.header.channelHandle);

    HttpReply* avatarReply = Http::instance().get(QUrl(endpoint.response.header.accountPhotos[0].url));
    connect(avatarReply, &HttpReply::finished, this, &AccountMenuWidget::setAvatar);

    const QList<InnertubeObjects::CompactLink>& section = endpoint.response.sections[0];
    auto iter = std::ranges::find_if(section, [](const InnertubeObjects::CompactLink& link) { return link.iconType == "ACCOUNT_BOX"; });

    if (iter != endpoint.response.sections[0].end())
    {
        const QString channelId = iter->navigationEndpoint["browseEndpoint"]["browseId"].toString();
        connect(yourChannelLabel, &IconLabel::clicked, this, [this, channelId] {
            setVisible(false);
            ViewController::loadChannel(channelId);
            emit closeRequested();
        });
    }

    adjustSize();
    emit finishedInitializing();
}

void AccountMenuWidget::setAvatar(const HttpReply& reply)
{
    QPixmap pixmap;
    pixmap.loadFromData(reply.body());
    avatar->setPixmap(pixmap.scaled(avatar->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void AccountMenuWidget::triggerSignOut()
{
    setVisible(false);
    MainWindow::topbar()->signOut();
    emit closeRequested();
}
