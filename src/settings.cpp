#include "ozk_performance.hpp"
#include "settings.hpp"
#include "resources.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QRegularExpression>
#include <QDateTime>
#include <QSet>
#include <algorithm>
#ifdef Q_OS_WIN
# include <windows.h>
# include <wincrypt.h>
#endif

namespace {
QString configPath()
{
    char* raw = obs_module_config_path("settings.json");
    if (!raw) return {};
    const QString path = QString::fromUtf8(raw);
    bfree(raw);
    return path;
}

QByteArray protect(const QString& value)
{
#ifdef Q_OS_WIN
    const QByteArray input = value.toUtf8();
    if (input.isEmpty()) return {};
    DATA_BLOB src {DWORD(input.size()), reinterpret_cast<BYTE*>(const_cast<char*>(input.constData()))};
    DATA_BLOB dest {};
    if (!CryptProtectData(&src, L"OZK Notify 1.0", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dest)) return {};
    QByteArray result(reinterpret_cast<const char*>(dest.pbData), int(dest.cbData));
    LocalFree(dest.pbData);
    return result.toBase64();
#else
    Q_UNUSED(value)
    return {};
#endif
}

bool unprotect(const QByteArray& encoded, QString& value)
{
    value.clear();
    if (encoded.isEmpty()) return true;
#ifdef Q_OS_WIN
    const QByteArray input = QByteArray::fromBase64(encoded);
    if (input.isEmpty()) return false;
    DATA_BLOB src {DWORD(input.size()), reinterpret_cast<BYTE*>(const_cast<char*>(input.constData()))};
    DATA_BLOB dest {};
    if (!CryptUnprotectData(&src, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dest)) return false;
    value = QString::fromUtf8(reinterpret_cast<const char*>(dest.pbData), int(dest.cbData));
    SecureZeroMemory(dest.pbData, dest.cbData);
    LocalFree(dest.pbData);
    return true;
#else
    Q_UNUSED(encoded)
    return false;
#endif
}

bool acceptedDelay(int seconds) { return seconds == 60 || seconds == 90 || seconds == 120 || seconds == 360; }
bool acceptedResult(int seconds) { return seconds == 30 || seconds == 45 || seconds == 60; }
}

SettingsStore::SettingsStore(QObject* parent) : QObject(parent), path_(configPath())
{
    ozkperf::install();
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(900);
    writer_.setMaxThreadCount(1);writer_.setExpiryTimeout(-1);
    connect(&saveTimer_, &QTimer::timeout, this, &SettingsStore::saveBackground);
}

bool SettingsStore::load()
{
    sn::CpuScope snCpuScope;
    if (path_.isEmpty()) { error_ = "OBS plugin config path unavailable"; return false; }
    QFile f(path_);
    if (!f.exists()) return true;
    if (!f.open(QFile::ReadOnly)) { error_ = "Cannot read settings"; return false; }
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) { error_ = "Invalid settings JSON"; return false; }
    const QJsonObject j = d.object();
    if (j.value("schema_version").toInt(1) != 1) { error_ = "Unsupported settings version"; return false; }
    auto str = [&](const char* key, const QString& def) { return j.value(QLatin1String(key)).toString(def); };
    auto flag = [&](const char* key, bool def) { return j.value(QLatin1String(key)).toBool(def); };
    current.enabled = flag("enabled", true);
    current.telegramEnabled = flag("telegram_enabled", false);
    current.discordEnabled = flag("discord_enabled", false);
    current.discordEveryone = flag("discord_everyone", false);
    current.telegramChat = str("telegram_chat", {});
    current.youtubeUrl = str("youtube_url", {});
    current.twitchUrl = str("twitch_url", {});
    current.kickUrl = str("kick_url", {});
    current.message = str("message", current.message);
    const QString oldDefault=QString::fromUtf8("🔴 НАЧАЛСЯ СТРИМ!\n\nСмотрите трансляцию:\n▶️ YouTube: {youtube}\n🎮 Twitch: {twitch}");
    if(current.message==oldDefault)current.message+=QString::fromUtf8("\n🟢 Kick: {kick}");
    current.secondAnnouncementEnabled=flag("second_announcement_enabled",true);
    current.secondMessage=str("second_message",current.secondMessage);
    const QJsonObject journal=j.value("delivery_journal").toObject();
    for(auto it=journal.begin();it!=journal.end();++it)
        if(it.value().isString()&&it.key().size()<400)current.deliveryJournal.insert(it.key(),it.value().toString());
    current.sendDelay = j.value("send_delay").toInt(120);
    if (!acceptedDelay(current.sendDelay)) current.sendDelay = 120;
    current.resultSeconds = j.value("result_seconds").toInt(45);
    if (!acceptedResult(current.resultSeconds)) current.resultSeconds = 45;
    current.locale = str("locale", "auto");
    if (!QSet<QString>{"ru", "en", "auto"}.contains(current.locale)) current.locale = "auto";
    current.compact = flag("compact", true);
    current.showStatus = flag("show_status", true);
    current.lastTelegram = str("last_telegram", {});
    current.lastDiscord = str("last_discord", {});
    current.lastTime = str("last_time", {});
    for (const QJsonValue& v : j.value("events").toArray()) if (v.isString()) current.events.append(v.toString());
    while (current.events.size() > 100) current.events.removeFirst();
    bool a = unprotect(str("telegram_secret", {}).toUtf8(), current.telegramToken);
    bool b = unprotect(str("discord_secret", {}).toUtf8(), current.discordWebhook);
    if (!a || !b) {
        error_ = "Could not decrypt one or more secrets; please enter them again";
        if (!a) current.telegramToken.clear();
        if (!b) current.discordWebhook.clear();
        return false;
    }
    const QJsonObject donations=j.value("donations").toObject();
    auto& ds=current.donations;
    ds.enabled=donations.value("enabled").toBool(false);
    ds.daEnabled=donations.value("donation_alerts_enabled").toBool(false);
    ds.dpEnabled=donations.value("donate_pay_enabled").toBool(false);
    ds.onlyLive=donations.value("only_live").toBool(true);
    ds.includeDonorMessage=donations.value("include_message").toBool(true);
    ds.toTelegram=donations.value("telegram").toBool(true);
    ds.toDiscord=donations.value("discord").toBool(true);
    ds.legacyApiOptIn=donations.value("legacy_api_opt_in").toBool(false);
    const QJsonObject thresholdJson=donations.value("thresholds").toObject();
    for(auto it=thresholdJson.begin();it!=thresholdJson.end();++it) {
        QJsonArray a=it.value().toArray();
        if(a.size()!=3)continue;
        double a0=a[0].toDouble(),a1=a[1].toDouble(),a2=a[2].toDouble();
        if(a0>0 && a1>a0 && a2>a1 && a2<1e12)
            ds.thresholds[it.key().trimmed().toUpper()]={a0,a1,a2};
    }
    const QJsonArray templates=donations.value("templates").toArray();
    if(templates.size()==3)for(int i=0;i<3;++i)if(templates[i].isString())ds.templates[i]=templates[i].toString();
    bool c=unprotect(donations.value("da_secret").toString().toUtf8(),ds.daToken);
    bool dpDecrypted=unprotect(donations.value("dp_secret").toString().toUtf8(),ds.dpToken);
    if(!c || !dpDecrypted) {
        if(!c)ds.daToken.clear(); if(!dpDecrypted)ds.dpToken.clear();
        error_="Could not decrypt donation tokens";return false;
    }
    return true;
}

namespace {
QByteArray cachedProtect(const QString& key,const QString& value){
    thread_local QMap<QString,QPair<QString,QByteArray>> cache;
    if(cache.contains(key)&&cache.value(key).first==value)return cache.value(key).second;
    const auto encoded=protect(value);if(value.isEmpty()||!encoded.isEmpty())cache[key]=qMakePair(value,encoded);
    return encoded;
}
QString writeSnapshot(const QString& path,const Settings& snapshot){ozkperf::Scope perf(Q_FUNC_INFO);    QString error_;

    sn::CpuScope snCpuScope;
    if (path.isEmpty()) { error_ = "OBS plugin config path unavailable"; return error_; }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error_ = "Cannot create settings folder"; return error_; }
    const QByteArray t = cachedProtect(path+"/telegram",snapshot.telegramToken);
    const QByteArray d = cachedProtect(path+"/discord",snapshot.discordWebhook);
    if ((!snapshot.telegramToken.isEmpty() && t.isEmpty()) || (!snapshot.discordWebhook.isEmpty() && d.isEmpty())) {
        error_ = "Windows failed to encrypt secret"; return error_;
    }
    const QByteArray da=cachedProtect(path+"/da",snapshot.donations.daToken),dp=cachedProtect(path+"/dp",snapshot.donations.dpToken);
    if((!snapshot.donations.daToken.isEmpty()&&da.isEmpty()) ||
       (!snapshot.donations.dpToken.isEmpty()&&dp.isEmpty())) {
        error_="Cannot encrypt donation tokens"; return error_;
    }
    QJsonObject donationObj;
    const auto& ds=snapshot.donations;
    donationObj["enabled"]=ds.enabled;
    donationObj["donation_alerts_enabled"]=ds.daEnabled;
    donationObj["donate_pay_enabled"]=ds.dpEnabled;
    donationObj["only_live"]=ds.onlyLive;
    donationObj["include_message"]=ds.includeDonorMessage;
    donationObj["telegram"]=ds.toTelegram;
    donationObj["discord"]=ds.toDiscord;
    donationObj["legacy_api_opt_in"]=ds.legacyApiOptIn;
    donationObj["da_secret"]=QString::fromLatin1(da);
    donationObj["dp_secret"]=QString::fromLatin1(dp);
    QJsonObject thresholdJson;
    for(auto it=ds.thresholds.cbegin();it!=ds.thresholds.cend();++it) {
        if(it.value().size()==3)thresholdJson[it.key()]=QJsonArray{it.value()[0],it.value()[1],it.value()[2]};
    }
    donationObj["thresholds"]=thresholdJson;
    QJsonArray templates;
    for(const auto& t:ds.templates)templates.append(t);
    donationObj["templates"]=templates;
    QJsonObject j;
    j["donations"]=donationObj;
    j["schema_version"] = 1;
    j["enabled"] = snapshot.enabled;
    j["telegram_enabled"] = snapshot.telegramEnabled;
    j["discord_enabled"] = snapshot.discordEnabled;
    j["discord_everyone"] = snapshot.discordEveryone;
    j["telegram_chat"] = snapshot.telegramChat;
    j["telegram_secret"] = QString::fromLatin1(t);
    j["discord_secret"] = QString::fromLatin1(d);
    j["youtube_url"] = snapshot.youtubeUrl;
    j["twitch_url"] = snapshot.twitchUrl;
    j["kick_url"] = snapshot.kickUrl;
    j["message"] = snapshot.message;
    j["second_announcement_enabled"]=snapshot.secondAnnouncementEnabled;
    j["second_message"]=snapshot.secondMessage;
    QJsonObject journal;
    for(auto it=snapshot.deliveryJournal.cbegin();it!=snapshot.deliveryJournal.cend();++it)journal[it.key()]=it.value();
    j["delivery_journal"]=journal;
    j["send_delay"] = snapshot.sendDelay;
    j["result_seconds"] = snapshot.resultSeconds;
    j["locale"] = snapshot.locale;
    j["compact"] = snapshot.compact;
    j["show_status"] = snapshot.showStatus;
    j["last_telegram"] = snapshot.lastTelegram;
    j["last_discord"] = snapshot.lastDiscord;
    j["last_time"] = snapshot.lastTime;
    QJsonArray events;
    for (const QString& e : snapshot.events) events.append(e);
    j["events"] = events;
    QSaveFile f(path);
    if (!f.open(QFile::WriteOnly)) { error_ = "Cannot open settings for writing"; return error_; }
    if (f.write(QJsonDocument(j).toJson()) < 0 || !f.commit()) {
        error_ = "Cannot commit settings"; return error_;
    }
    return {};
}
}
SettingsStore::~SettingsStore(){flush();writer_.waitForDone();}
bool SettingsStore::save(){
    writer_.waitForDone();error_=writeSnapshot(path_,current);Q_EMIT persisted(error_.isEmpty());return error_.isEmpty();
}
void SettingsStore::saveBackground(){
    const auto snapshot=current;const auto path=path_;
    writer_.start([this,path,snapshot]{const auto error=writeSnapshot(path,snapshot);
        QMetaObject::invokeMethod(this,[this,error]{error_=error;Q_EMIT persisted(error.isEmpty());},Qt::QueuedConnection);
    });
}
void SettingsStore::scheduleSave() {
    sn::CpuScope snCpuScope; saveTimer_.start(); }
void SettingsStore::flush() {
    sn::CpuScope snCpuScope; if (saveTimer_.isActive()) { saveTimer_.stop(); save(); } writer_.waitForDone(); }
void SettingsStore::addEvent(const QString& msg)
{
    sn::CpuScope snCpuScope;
    current.events.prepend(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "  " + msg);
    while (current.events.size() > 100) current.events.removeLast();
    scheduleSave();
}

bool validTelegram(const QString& token, const QString& chat)
{
    static const QRegularExpression tokenRE("^[0-9]+:[A-Za-z0-9_-]{20,}$");
    static const QRegularExpression idRE("^(?:-?[0-9]+|@[A-Za-z0-9_]{5,})$");
    return tokenRE.match(token.trimmed()).hasMatch() && idRE.match(chat.trimmed()).hasMatch();
}
bool validDiscord(const QString& raw)
{
    QUrl u(raw.trimmed());
    if (u.scheme() != "https" || (u.port(-1) != -1 && u.port(-1) != 443) || !u.userInfo().isEmpty() || !u.fragment().isEmpty()) return false;
    if (u.host().compare("discord.com", Qt::CaseInsensitive) != 0 && u.host().compare("discordapp.com", Qt::CaseInsensitive) != 0) return false;
    static const QRegularExpression re("^/api(?:/v[0-9]+)?/webhooks/[0-9]+/[A-Za-z0-9._-]+/?$");
    return re.match(u.path()).hasMatch();
}
bool validStreamUrl(const QString& raw)
{
    if (raw.trimmed().isEmpty()) return true;
    const QUrl u(raw.trimmed());
    return (u.scheme() == "https" && !u.host().isEmpty() && u.userInfo().isEmpty());
}
QString prepareMessage(const Settings& s, QString* error)
{
    return renderAnnouncement(s.message,s.youtubeUrl,s.twitchUrl,{},error,s.kickUrl);
}
QString renderAnnouncement(const QString& message,const QString& youtubeUrl,const QString& twitchUrl,
                           const QString& platform,QString* error,const QString& kickUrl)
{
    if(error)error->clear();
    if (!validStreamUrl(youtubeUrl) || !validStreamUrl(twitchUrl) || !validStreamUrl(kickUrl)) {
        if (error) *error = "Invalid YouTube, Twitch or Kick URL";
        return {};
    }
    QString body=message;
    if (youtubeUrl.trimmed().isEmpty()) {
        QStringList lines;
        for (const QString& line : body.split('\n')) if (!line.contains("{youtube}")&&!line.contains("{youtube_url}")) lines << line;
        body = lines.join('\n');
    } else body.replace("{youtube}",youtubeUrl.trimmed()).replace("{youtube_url}",youtubeUrl.trimmed());
    if (twitchUrl.trimmed().isEmpty()) {
        QStringList lines;
        for (const QString& line : body.split('\n')) if (!line.contains("{twitch}")&&!line.contains("{twitch_url}")) lines << line;
        body = lines.join('\n');
    } else body.replace("{twitch}",twitchUrl.trimmed()).replace("{twitch_url}",twitchUrl.trimmed());
    if(kickUrl.trimmed().isEmpty()){
        QStringList lines;for(const auto& line:body.split('\n'))if(!line.contains("{kick}")&&!line.contains("{kick_url}"))lines<<line;body=lines.join('\n');
    }else body.replace("{kick}",kickUrl.trimmed()).replace("{kick_url}",kickUrl.trimmed());
    QStringList urls;if(!youtubeUrl.trimmed().isEmpty())urls<<youtubeUrl.trimmed();if(!twitchUrl.trimmed().isEmpty())urls<<twitchUrl.trimmed();if(!kickUrl.trimmed().isEmpty())urls<<kickUrl.trimmed();
    body.replace("{stream_url}",urls.join("\n")).replace("{platform}",platform);
    static const QRegularExpression unknown("\\{[A-Za-z_][A-Za-z0-9_]*\\}");
    if (unknown.match(body).hasMatch()) { if (error) *error = "Unsupported template variable"; return {}; }
    body.replace(QRegularExpression("\\n{3,}"),"\n\n");
    body = body.trimmed();
    if (body.isEmpty()) { if (error) *error = "Message is empty"; return {}; }
    if (body.size() > 2000) { if (error) *error = "Message exceeds Discord 2000 character limit"; return {}; }
    return body;
}
Snapshot makeSnapshot(const Settings& s, QString* error)
{
    Snapshot out;
    out.telegram = s.telegramEnabled;
    out.discord = s.discordEnabled;
    out.discordEveryone = s.discordEveryone;
    out.token = s.telegramToken;
    out.chat = s.telegramChat;
    out.webhook = s.discordWebhook;
    out.delay = s.sendDelay;
    out.resultSeconds = s.resultSeconds;
    out.message = prepareMessage(s, error);
    return out;
}
