#include "watchview.h"
#include "watchview_ui.h"
#include "innertube.h"
#include "osutilities.h"
#include "settingsstore.h"
#include "ui/forms/mainwindow.h"
#include "ui/uiutilities.h"
#include <QDesktopServices>
#include <QScrollBar>

#ifdef USEMPV
#include <QRandomGenerator>
#endif

WatchView::~WatchView()
{
    delete ui;
    disconnect(MainWindow::topbar()->logo, &TubeLabel::clicked, this, nullptr);
    OSUtilities::toggleIdleSleep(false);
}

WatchView::WatchView(const QString& videoId, int progress, QWidget* parent) : QWidget(parent), ui(new Ui::WatchView)
{
    OSUtilities::toggleIdleSleep(true);
    MainWindow::topbar()->setVisible(false);
    MainWindow::topbar()->alwaysShow = false;

    ui->setupUi(this);

    InnertubeReply* next = InnerTube::instance().get<InnertubeEndpoints::Next>(videoId);
    connect(next, qOverload<InnertubeEndpoints::Next>(&InnertubeReply::finished), this, &WatchView::processNext);
    connect(next, &InnertubeReply::exception, this, &WatchView::loadFailed);

    InnertubeReply* player = InnerTube::instance().get<InnertubeEndpoints::Player>(videoId);
    connect(player, qOverload<InnertubeEndpoints::Player>(&InnertubeReply::finished), this, &WatchView::processPlayer);
    connect(player, &InnertubeReply::exception, this, &WatchView::loadFailed);

#ifdef USEMPV
    ui->media->play("https://www.youtube.com/watch?v=" + videoId);
    ui->media->seek(progress);
#else
    ui->wePlayer->setAuthStore(InnerTube::instance().authStore());
    ui->wePlayer->setContext(InnerTube::instance().context());
    ui->wePlayer->play(videoId, progress);
#endif

    connect(ui->channelLabel->text, &TubeLabel::customContextMenuRequested, this, &WatchView::showContextMenu);
    connect(ui->description, &TubeLabel::linkActivated, this, &WatchView::descriptionLinkActivated);
}

void WatchView::descriptionLinkActivated(const QString& url)
{
    QUrl qUrl(url);
    if (url.startsWith("http"))
    {
        QDesktopServices::openUrl(qUrl);
    }
    else if (url.startsWith("/channel"))
    {
        QString funnyPath = qUrl.path().replace("/channel/", "");
        emit navigateChannelRequested(funnyPath.left(funnyPath.indexOf('/')));
    }
    else if (url.startsWith("/watch"))
    {
        ui->scrollArea->verticalScrollBar()->setValue(0);
        QUrlQuery query(qUrl);
        if (query.queryItemValue("continuePlayback") == "1")
        {
            int progress = query.queryItemValue("t").replace("s", "").toInt();
#ifdef USEMPV
    ui->media->seek(progress);
#else
    ui->wePlayer->seek(progress);
#endif
        }
        else
        {
            hotLoadVideo(query.queryItemValue("v"), query.queryItemValue("t").toInt());
            ui->toggleShowMore();
        }
    }
    else
    {
        qDebug() << "Ran into unsupported description link:" << url;
    }
}

QString WatchView::generateFormattedDescription(const InnertubeObjects::InnertubeString& description)
{
    QString descriptionText;

    for (const InnertubeObjects::InnertubeRun& run : description.runs)
    {
        if (run.navigationEndpoint.isNull() || run.navigationEndpoint.isUndefined())
        {
            descriptionText += run.text;
            continue;
        }

        const QJsonObject navigationEndpoint = run.navigationEndpoint.toObject();
        QString href;
        QString runText = run.text;

        if (navigationEndpoint.contains("urlEndpoint"))
        {
            QUrl url(navigationEndpoint["urlEndpoint"]["url"].toString());
            QUrlQuery query(url);
            if (query.hasQueryItem("q"))
            {
                href = QUrl::fromPercentEncoding(query.queryItemValue("q").toUtf8());
            }
            else if (url.host() == "www.youtube.com" && url.path().startsWith("/channel"))
            {
                href = url.path();
                runText = url.toString().left(37) + "...";
            }
            else
            {
                href = url.toString();
            }
        }
        else
        {
            href = navigationEndpoint["commandMetadata"]["webCommandMetadata"]["url"].toString();
            if (navigationEndpoint.contains("watchEndpoint"))
                href += "&continuePlayback=" + QString::number(navigationEndpoint["watchEndpoint"]["continuePlayback"].toBool());
        }

        descriptionText += QStringLiteral("<a href=\"%1\">%2</a>").arg(href, runText);
    }

    return descriptionText.replace("\n", "<br>");
}

void WatchView::hotLoadVideo(const QString& videoId, int progress)
{
    if (metadataUpdateTimer)
        metadataUpdateTimer->deleteLater();

#ifdef USEMPV
    ui->media->play("https://www.youtube.com/watch?v=" + videoId);
    ui->media->seek(progress);
    watchtimeTimer->deleteLater();
#else
    ui->wePlayer->play(videoId, progress);
#endif

    UIUtilities::clearLayout(ui->topLevelButtons);
    disconnect(ui->channelLabel->text, &TubeLabel::clicked, nullptr, nullptr);

    InnertubeReply* next = InnerTube::instance().get<InnertubeEndpoints::Next>(videoId);
    connect(next, qOverload<InnertubeEndpoints::Next>(&InnertubeReply::finished), this, &WatchView::processNext);
    connect(next, &InnertubeReply::exception, this, &WatchView::loadFailed);

    InnertubeReply* player = InnerTube::instance().get<InnertubeEndpoints::Player>(videoId);
    connect(player, qOverload<InnertubeEndpoints::Player>(&InnertubeReply::finished), this, &WatchView::processPlayer);
    connect(player, &InnertubeReply::exception, this, &WatchView::loadFailed);
}

void WatchView::likeOrDislike(bool like, const InnertubeObjects::ToggleButton& toggleButton)
{
    IconLabel* senderLabel = qobject_cast<IconLabel*>(sender());

    bool textIsNumber;
    int count = QLocale::system().toInt(senderLabel->textLabel->text(), &textIsNumber);

    if (senderLabel->textLabel->styleSheet().isEmpty()) // if untoggled
    {
        senderLabel->icon->setPixmap(like ? QPixmap(":/like-toggled.svg") : QPixmap(":/dislike-toggled.svg"));
        senderLabel->textLabel->setStyleSheet("color: #167ac6");
        if (textIsNumber)
            senderLabel->textLabel->setText(QLocale::system().toString(count + 1));

        const QJsonArray defaultCommands = toggleButton.defaultServiceEndpoint["commandExecutorCommand"]["commands"].toArray();
        QJsonValue defaultCommand = *std::find_if(defaultCommands.cbegin(), defaultCommands.cend(),
            [](const QJsonValue& v) { return v.toObject().contains("commandMetadata"); });
        InnerTube::instance().like(defaultCommand["likeEndpoint"], like);
    }
    else
    {
        if (like)
            senderLabel->icon->setPixmap(UIUtilities::preferDark() ? QPixmap(":/like-light.svg") : QPixmap(":/like.svg"));
        else
            senderLabel->icon->setPixmap(UIUtilities::preferDark() ? QPixmap(":/dislike-light.svg") : QPixmap(":/dislike.svg"));

        senderLabel->textLabel->setStyleSheet(QString());
        if (textIsNumber)
            senderLabel->textLabel->setText(QLocale::system().toString(count - 1));

        InnerTube::instance().like(toggleButton.toggledServiceEndpoint["likeEndpoint"], like);
    }
}

void WatchView::processNext(const InnertubeEndpoints::Next& endpoint)
{
    InnertubeEndpoints::NextResponse nextResp = endpoint.response;
    channelId = nextResp.secondaryInfo.subscribeButton.channelId;

    ui->channelLabel->setInfo(nextResp.secondaryInfo.owner.title.text, nextResp.secondaryInfo.owner.badges);
    connect(ui->channelLabel->text, &TubeLabel::clicked, this, std::bind(&WatchView::navigateChannelRequested, this, channelId));

    ui->subscribeWidget->setSubscribeButton(nextResp.secondaryInfo.subscribeButton);
    ui->subscribeWidget->setSubscriberCount(nextResp.secondaryInfo.owner.subscriberCountText.text, nextResp.secondaryInfo.subscribeButton.channelId);
    ui->subscribeWidget->subscribersCountLabel()->setVisible(true);
    ui->viewCount->setText(nextResp.primaryInfo.viewCount.text);

    for (const InnertubeObjects::MenuFlexibleItem& fi : nextResp.primaryInfo.videoActions.flexibleItems)
    {
        if (fi.topLevelButton.iconType == "CONTENT_CUT" || fi.topLevelButton.iconType == "MONEY_HEART") // will almost definitely never be implemented
            continue;

        QString labelText = fi.topLevelButton.text.text;
        if (labelText == "Save") // to replicate Hitchhiker style
            labelText = "Add to";

        IconLabel* label = new IconLabel(
            fi.topLevelButton.iconType.toLower(),
            ui->topLevelButtons->count() > 0 ? QMargins(15, 0, 0, 0) : QMargins(5, 0, 0, 0)
        );
        label->setText(labelText);
        ui->topLevelButtons->addWidget(label);
    }

    IconLabel* shareLabel = new IconLabel("share", ui->topLevelButtons->count() > 0 ? QMargins(15, 0, 0, 0) : QMargins(5, 0, 0, 0));
    shareLabel->setText("Share");
    ui->topLevelButtons->addWidget(shareLabel);

    ui->topLevelButtons->addStretch();

    ui->topLevelButtons->addWidget(ui->likeLabel);
    connect(ui->likeLabel, &IconLabel::clicked, this, std::bind(&WatchView::likeOrDislike, this, true, nextResp.primaryInfo.videoActions.likeButton));
    if (nextResp.primaryInfo.videoActions.likeButton.isToggled)
    {
        ui->likeLabel->icon->setPixmap(QPixmap(":/like-toggled.svg"));
        ui->likeLabel->textLabel->setStyleSheet("color: #167ac6");
    }

    ui->topLevelButtons->addWidget(ui->dislikeLabel);
    connect(ui->dislikeLabel, &IconLabel::clicked, this, std::bind(&WatchView::likeOrDislike, this, false, nextResp.primaryInfo.videoActions.dislikeButton));
    if (nextResp.primaryInfo.videoActions.dislikeButton.isToggled)
    {
        ui->dislikeLabel->icon->setPixmap(QPixmap(":/dislike-toggled.svg"));
        ui->dislikeLabel->textLabel->setStyleSheet("color: #167ac6");
    }

    QList<InnertubeObjects::GenericThumbnail> channelIcons = nextResp.secondaryInfo.owner.thumbnails;
    if (!channelIcons.isEmpty())
    {
        InnertubeObjects::GenericThumbnail bestThumb = *std::find_if(channelIcons.cbegin(), channelIcons.cend(), [](const auto& t) { return t.width >= 48; });
        HttpReply* reply = Http::instance().get(bestThumb.url);
        connect(reply, &HttpReply::finished, this, &WatchView::setChannelIcon);
    }

    if (SettingsStore::instance().returnDislikes)
    {
        HttpReply* reply = Http::instance().get("https://returnyoutubedislikeapi.com/votes?videoId=" + nextResp.videoId);
        connect(reply, &HttpReply::finished, this, &WatchView::updateRatings);
    }
    else
    {
        ui->likeLabel->setText(nextResp.primaryInfo.videoActions.likeButton.defaultText.text);
    }

    QString dateText = nextResp.primaryInfo.dateText.text;
    if (!dateText.startsWith("Premier") && !dateText.startsWith("Stream") && !dateText.startsWith("Start") && !dateText.startsWith("Sched"))
        dateText = "Published on " + dateText;
    ui->date->setText(dateText);

    ui->description->setText(generateFormattedDescription(nextResp.secondaryInfo.description));
}

void WatchView::processPlayer(const InnertubeEndpoints::Player& endpoint)
{
    InnertubeEndpoints::PlayerResponse playerResp = endpoint.response;
    ui->titleLabel->setText(playerResp.videoDetails.title);

#ifdef USEMPV
    if (SettingsStore::instance().playbackTracking)
        reportPlayback(playerResp);

    if (SettingsStore::instance().watchtimeTracking)
    {
        watchtimeTimer = new QTimer(this);
        watchtimeTimer->setInterval(5000);
        connect(watchtimeTimer, &QTimer::timeout, this, std::bind(&WatchView::reportWatchtime, this, playerResp, ui->media->position));
        watchtimeTimer->start();
    }
#else
    ui->wePlayer->setPlayerResponse(playerResp);
#endif

    if (playerResp.videoDetails.isLive)
    {
        metadataUpdateTimer = new QTimer(this);
        metadataUpdateTimer->setInterval(60000);
        connect(metadataUpdateTimer, &QTimer::timeout, this, [playerResp, this]
        {
            try
            {
                auto updatedMetadata = InnerTube::instance().getBlocking<InnertubeEndpoints::UpdatedMetadata>(playerResp.videoDetails.videoId);
                updateMetadata(updatedMetadata.response);
            }
            catch (const InnertubeException& ie)
            {
                qDebug() << "InnertubeException on UpdateMetadata. Stream/premiere likely ended. Killing update timer.";
                metadataUpdateTimer->deleteLater();
            }
        });
        metadataUpdateTimer->start();
    }
}

void WatchView::resizeEvent(QResizeEvent* event)
{
    if (!ui->primaryInfoWrapper || !event->oldSize().isValid()) return;

    const QSize playerSize = ui->calcPlayerSize(size());
    ui->description->setFixedWidth(playerSize.width());
    ui->menuWrapper->setFixedWidth(playerSize.width());
    ui->primaryInfoWrapper->setFixedWidth(playerSize.width());
    ui->showMoreLabel->setFixedWidth(playerSize.width());
    ui->titleLabel->setFixedWidth(playerSize.width());

#ifdef USEMPV
    ui->media->videoWidget()->setFixedSize(playerSize);
#else
    ui->wePlayer->setFixedSize(playerSize);
#endif

    ui->scrollArea->setFixedSize(event->size());
}

void WatchView::setChannelIcon(const HttpReply& reply)
{
    QPixmap pixmap;
    pixmap.loadFromData(reply.body());
    ui->channelIcon->setPixmap(pixmap.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

void WatchView::showContextMenu(const QPoint& pos)
{
    QMenu* menu = new QMenu(this);

    QAction* copyUrlAction = new QAction("Copy channel page URL", this);
    connect(copyUrlAction, &QAction::triggered, this, std::bind(&UIUtilities::copyToClipboard, "https://www.youtube.com/channel/" + channelId));

    menu->addAction(copyUrlAction);
    menu->popup(ui->channelLabel->text->mapToGlobal(pos));
}

void WatchView::updateMetadata(const InnertubeEndpoints::UpdatedMetadataResponse& resp)
{
    ui->description->setText(generateFormattedDescription(resp.description));
    ui->titleLabel->setText(resp.title.text);
    ui->viewCount->setText(resp.viewCount);

    QString dateText = resp.dateText;
    if (!dateText.startsWith("Premier") && !dateText.startsWith("Stream") && !dateText.startsWith("Start") && !dateText.startsWith("Sched"))
        dateText = "Published on " + dateText;
    ui->date->setText(dateText);

    if (SettingsStore::instance().returnDislikes)
    {
        HttpReply* reply = Http::instance().get("https://returnyoutubedislikeapi.com/votes?videoId=" + resp.videoId);
        connect(reply, &HttpReply::finished, this, &WatchView::updateRatings);
    }
    else
    {
        ui->likeLabel->setText(resp.likeDefaultText);
    }
}

void WatchView::updateRatings(const HttpReply& reply)
{
    QJsonDocument doc = QJsonDocument::fromJson(reply.body());
    int dislikes = doc["dislikes"].toInt();
    int likes = doc["likes"].toInt();
    if (likes != 0 || dislikes != 0)
    {
        ui->likeBar->setMaximum(likes + dislikes);
        ui->likeBar->setValue(likes);
    }
    ui->likeBar->setVisible(true);

    ui->dislikeLabel->setText(QLocale::system().toString(dislikes));
    ui->likeLabel->setText(QLocale::system().toString(likes));
}

#ifdef USEMPV // MPV backend exclusive methods
QString WatchView::getCpn()
{
    QString out;
    constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for (int i = 0; i < 16; i++)
        out += chars[QRandomGenerator::global()->bounded((int)chars.size())];
    return out;
}

void WatchView::reportPlayback(const InnertubeEndpoints::PlayerResponse& playerResp)
{
    InnertubeClient itc = InnerTube::instance().context()->client;

    QUrlQuery playbackQuery(QUrl(playerResp.playbackTracking.videostatsPlaybackUrl));
    QUrl outPlaybackUrl("https://www.youtube.com/api/stats/playback");
    QUrlQuery outPlaybackQuery;

    QList<QPair<QString, QString>> map =
    {
        { "ns", "yt" },
        { "el", "detailpage" },
        { "cpn", getCpn() },
        { "ver", "2" },
        { "fmt", "243" },
        { "fs", "0" },
        { "rt", QString::number(QRandomGenerator::global()->bounded(191) + 10) },
        { "euri", "" },
        { "lact", QString::number(QRandomGenerator::global()->bounded(7001) + 1000) },
        { "cl", playbackQuery.queryItemValue("cl") },
        { "mos", "0" },
        { "volume", "100" },
        { "cbr", itc.browserName },
        { "cbrver", itc.browserVersion },
        { "c", itc.clientName },
        { "cver", itc.clientVersion },
        { "cplayer", "UNIPLAYER" },
        { "cos", itc.osName },
        { "cosver", itc.osVersion },
        { "cplatform", itc.platform },
        { "hl", itc.hl + "_" + itc.gl },
        { "cr", itc.gl },
        { "uga", playbackQuery.queryItemValue("uga") },
        { "len", playbackQuery.queryItemValue("len") },
        { "fexp", playbackQuery.queryItemValue("fexp") },
        { "rtn", "4" },
        { "afmt", "251" },
        { "muted", "0" },
        { "docid", playbackQuery.queryItemValue("docid") },
        { "ei", playbackQuery.queryItemValue("ei") },
        { "plid", playbackQuery.queryItemValue("plid") },
        { "sdetail", playbackQuery.queryItemValue("sdetail") },
        { "of", playbackQuery.queryItemValue("of") },
        { "vm", playbackQuery.queryItemValue("vm") }
    };

    outPlaybackQuery.setQueryItems(map);
    outPlaybackUrl.setQuery(outPlaybackQuery);
    Http::instance().get(outPlaybackUrl);
}

void WatchView::reportWatchtime(const InnertubeEndpoints::PlayerResponse& playerResp, long long position)
{
    InnertubeClient itc = InnerTube::instance().context()->client;

    QUrlQuery watchtimeQuery(QUrl(playerResp.playbackTracking.videostatsWatchtimeUrl));
    QUrl outWatchtimeUrl("https://www.youtube.com/api/stats/watchtime");
    QUrlQuery outWatchtimeQuery;
    QString rt = QString::number(QRandomGenerator::global()->bounded(191) + 10);
    QString posStr = QString::number(position);

    QList<QPair<QString, QString>> map =
    {
        { "ns", "yt" },
        { "el", "detailpage" },
        { "cpn", getCpn() },
        { "ver", "2" },
        { "fmt", "243" },
        { "fs", "0" },
        { "rt", rt },
        { "euri", "" },
        { "lact", QString::number(QRandomGenerator::global()->bounded(7001) + 1000) },
        { "cl", watchtimeQuery.queryItemValue("cl") },
        { "state", "playing" },
        { "volume", "100" },
        { "subscribed", watchtimeQuery.queryItemValue("subscribed") },
        { "cbr", itc.browserName },
        { "cbrver", itc.browserVersion },
        { "c", itc.clientName },
        { "cver", itc.clientVersion },
        { "cplayer", "UNIPLAYER" },
        { "cos", itc.osName },
        { "cosver", itc.osVersion },
        { "cplatform", itc.platform },
        { "hl", itc.hl + "_" + itc.gl },
        { "cr", itc.gl },
        { "uga", watchtimeQuery.queryItemValue("uga") },
        { "len", watchtimeQuery.queryItemValue("len") },
        { "afmt", "251" },
        { "idpj", "-1" },
        { "ldpj", "-10" },
        { "rti", rt },
        { "st", posStr },
        { "et", posStr },
        { "muted", "0" },
        { "docid", watchtimeQuery.queryItemValue("docid") },
        { "ei", watchtimeQuery.queryItemValue("ei") },
        { "plid", watchtimeQuery.queryItemValue("plid") },
        { "sdetail", watchtimeQuery.queryItemValue("sdetail") },
        { "of", watchtimeQuery.queryItemValue("of") },
        { "vm", watchtimeQuery.queryItemValue("vm") }
    };

    outWatchtimeQuery.setQueryItems(map);
    outWatchtimeUrl.setQuery(outWatchtimeQuery);
    Http::instance().get(outWatchtimeUrl);
}
#endif // MPV backend exclusive methods
