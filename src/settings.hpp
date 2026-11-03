#pragma once
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>

struct DonationSettings {
    bool enabled = false;
    bool daEnabled = false;
    bool dpEnabled = false;
    bool onlyLive = true;
    bool includeDonorMessage = true;
    bool toTelegram = true;
    bool toDiscord = true;
    bool legacyApiOptIn = false; // Requires confirmation against actual DonatePay account API.
    QString daToken;
    QString dpToken;
    QMap<QString,QVector<double>> thresholds {{"RUB",{100.0,500.0,2000.0}}};
    QStringList templates {
        QString::fromUtf8("💰 НОВЫЙ ДОНАТ!\n\n{donor} поддержал стрим на {amount} {currency}!\n\n💬 {message}\n\n🎮 Присоединяйся к трансляции!\n{stream_url}"),
        QString::fromUtf8("🔥 КРУПНЫЙ ДОНАТ!\n\n💸 {donor} отправил {amount} {currency}!\n\n💬 {message}\n\n🔥 На стриме жарко! Заходи смотреть!\n{stream_url}"),
        QString::fromUtf8("👑 МЕГА-ДОНАТ НА СТРИМЕ!\n\n🏆 {donor} задонатил {amount} {currency}!\n\n💬 {message}\n\n🚀 Присоединяйся к трансляции!\n{stream_url}")
    };
};

struct Settings {
    DonationSettings donations;
    bool enabled = true;
    bool telegramEnabled = false;
    bool discordEnabled = false;
    bool discordEveryone = false;
    QString telegramToken;
    QString telegramChat;
    QString discordWebhook;
    QString youtubeUrl;
    QString twitchUrl;
    QString kickUrl;
    QString message = QString::fromUtf8("🔴 НАЧАЛСЯ СТРИМ!\n\nСмотрите трансляцию:\n▶️ YouTube: {youtube}\n🎮 Twitch: {twitch}\n🟢 Kick: {kick}");
    bool secondAnnouncementEnabled = true;
    QString secondMessage = QString::fromUtf8("🔴 Мы теперь и на {platform}!\n\n🎬 Подключайтесь: {stream_url}");
    QMap<QString,QString> deliveryJournal;
    int sendDelay = 120;
    int resultSeconds = 45;
    QString locale = "auto";
    bool compact = true;
    bool showStatus = true;
    QString lastTelegram = "";
    QString lastDiscord = "";
    QString lastTime = "";
    QStringList events;
};

struct Snapshot {
    bool telegram = false;
    bool discord = false;
    bool discordEveryone = false;
    QString token;
    QString chat;
    QString webhook;
    QString message;
    int delay = 120;
    int resultSeconds = 45;
};

class SettingsStore : public QObject {
    Q_OBJECT
public:
    explicit SettingsStore(QObject* parent = nullptr);
    ~SettingsStore() override;
    Settings current;
    bool load();
    bool save();
    void scheduleSave();
    void flush();
    QString error() const { return error_; }
    void addEvent(const QString& event);
Q_SIGNALS:
    void persisted(bool success);
private:
    QString path_;
    QString error_;
    QTimer saveTimer_;
    QThreadPool writer_;
    void saveBackground();
};

bool validTelegram(const QString& token, const QString& chat);
bool validDiscord(const QString& url);
bool validStreamUrl(const QString& url);
QString prepareMessage(const Settings& s, QString* validationError = nullptr);
QString renderAnnouncement(const QString& message,const QString& youtubeUrl,const QString& twitchUrl,
                           const QString& platform=QString(),QString* validationError=nullptr,const QString& kickUrl=QString());
Snapshot makeSnapshot(const Settings& s, QString* error = nullptr);
