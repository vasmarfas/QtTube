#ifndef SETTINGSSTORE_H
#define SETTINGSSTORE_H
#include <mutex>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

class SettingsStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString appStyle MEMBER appStyle NOTIFY appStyleChanged)
    Q_PROPERTY(bool condensedViews MEMBER condensedViews NOTIFY condensedViewsChanged)
    Q_PROPERTY(bool darkTheme MEMBER darkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(bool deArrow MEMBER deArrow NOTIFY deArrowChanged)
    Q_PROPERTY(bool deArrowThumbs MEMBER deArrowThumbs NOTIFY deArrowThumbsChanged)
    Q_PROPERTY(bool deArrowTitles MEMBER deArrowTitles NOTIFY deArrowTitlesChanged)
    Q_PROPERTY(bool disable60Fps MEMBER disable60Fps NOTIFY disable60FpsChanged)
    Q_PROPERTY(bool disablePlayerInfoPanels MEMBER disablePlayerInfoPanels NOTIFY disablePlayerInfoPanelsChanged)
    Q_PROPERTY(int filterLength MEMBER filterLength NOTIFY filterLengthChanged)
    Q_PROPERTY(bool filterLengthEnabled MEMBER filterLengthEnabled NOTIFY filterLengthEnabledChanged)
    Q_PROPERTY(QStringList filteredChannels MEMBER filteredChannels NOTIFY filteredChannelsChanged)
    Q_PROPERTY(QStringList filteredTerms MEMBER filteredTerms NOTIFY filteredTermsChanged)
    Q_PROPERTY(bool fullSubs MEMBER fullSubs NOTIFY fullSubsChanged)
    Q_PROPERTY(bool h264Only MEMBER h264Only NOTIFY h264OnlyChanged)
    Q_PROPERTY(bool hideShorts MEMBER hideShorts NOTIFY hideShortsChanged)
    Q_PROPERTY(bool hideStreams MEMBER hideStreams NOTIFY hideStreamsChanged)
    Q_PROPERTY(bool homeShelves MEMBER homeShelves NOTIFY homeShelvesChanged)
    Q_PROPERTY(bool playbackTracking MEMBER playbackTracking NOTIFY playbackTrackingChanged)
    Q_PROPERTY(PlayerQuality preferredQuality MEMBER preferredQuality NOTIFY preferredQualityChanged)
    Q_PROPERTY(int preferredVolume MEMBER preferredVolume NOTIFY preferredVolumeChanged)
    Q_PROPERTY(bool restoreAnnotations MEMBER restoreAnnotations NOTIFY restoreAnnotationsChanged)
    Q_PROPERTY(bool returnDislikes MEMBER returnDislikes NOTIFY returnDislikesChanged)
    Q_PROPERTY(bool showSBToasts MEMBER showSBToasts NOTIFY showSBToastsChanged)
    Q_PROPERTY(QStringList sponsorBlockCategories MEMBER sponsorBlockCategories NOTIFY sponsorBlockCategoriesChanged)
    Q_PROPERTY(bool watchtimeTracking MEMBER watchtimeTracking NOTIFY watchtimeTrackingChanged)
public:
    // QStandardPaths::AppConfigLocation appears to not work in a static context, so we have to make it ourselves :(
    static inline const QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QDir::separator() + "QtTube"
                                            + QDir::separator() + "settings.ini";

    enum class PlayerQuality { Auto, HighRes, HD2160, HD1440, HD1080, HD720, Large, Medium, Small, Tiny };
    Q_ENUM(PlayerQuality)

    QString appStyle;
    bool condensedViews;
    bool darkTheme;
    bool deArrow;
    bool deArrowThumbs;
    bool deArrowTitles;
    bool disable60Fps;
    bool disablePlayerInfoPanels;
    int filterLength;
    bool filterLengthEnabled;
    QStringList filteredChannels;
    QStringList filteredTerms;
    bool fullSubs;
    bool h264Only;
    bool hideShorts;
    bool hideStreams;
    bool homeShelves;
    bool playbackTracking;
    PlayerQuality preferredQuality;
    int preferredVolume;
    bool restoreAnnotations;
    bool returnDislikes;
    bool showSBToasts;
    QStringList sponsorBlockCategories;
    bool watchtimeTracking;

    static SettingsStore* instance();
    explicit SettingsStore(QObject* parent = nullptr) : QObject(parent) {}

    void initializeFromSettingsFile();
    void saveToSettingsFile();
    bool strHasFilteredTerm(const QString& str);
private:
    static inline SettingsStore* m_instance;
    static inline std::once_flag m_onceFlag;

    void readIntoStringList(QSettings& settings, QStringList& list, const QString& prefix, const QString& key);
    void writeStringList(QSettings& settings, const QStringList& list, const QString& prefix, const QString& key);
signals:
    void appStyleChanged(const QString&);
    void condensedViewsChanged(bool);
    void darkThemeChanged(bool);
    void deArrowChanged(bool);
    void deArrowThumbsChanged(bool);
    void deArrowTitlesChanged(bool);
    void disable60FpsChanged(bool);
    void disablePlayerInfoPanelsChanged(bool);
    void filterLengthChanged(int);
    void filterLengthEnabledChanged(int);
    void filteredChannelsChanged(const QStringList&);
    void filteredTermsChanged(const QStringList&);
    void fullSubsChanged(bool);
    void h264OnlyChanged(bool);
    void hideShortsChanged(bool);
    void hideStreamsChanged(bool);
    void homeShelvesChanged(bool);
    void playbackTrackingChanged(bool);
    void preferredQualityChanged(SettingsStore::PlayerQuality);
    void preferredVolumeChanged(int);
    void restoreAnnotationsChanged(bool);
    void returnDislikesChanged(bool);
    void showSBToastsChanged(bool);
    void sponsorBlockCategoriesChanged(const QStringList&);
    void watchtimeTrackingChanged(bool);
};

#endif // SETTINGSSTORE_H
