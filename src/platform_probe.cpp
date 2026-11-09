#include "platform_probe.hpp"
#include "settings.hpp"
#include "resources.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <algorithm>
#include <QRegularExpression>

namespace sn {
namespace {
const QString googleClient="398266429510-m38h7aklj8s3r123fqmc64h33cdbmh3j.apps.googleusercontent.com";
const QString twitchClient="6ivhu6zdlv5ojd0h8w4s037uc7rmgh";
const QString twitchScope="user:read:chat";
QByteArray form(std::initializer_list<std::pair<QString,QString>> fields){
    QUrlQuery q;for(const auto& field:fields)q.addQueryItem(field.first,field.second);
    return q.query(QUrl::FullyEncoded).toUtf8().replace("+","%2B");
}
QString randomVerifier(){
    QString value;for(int i=0;i<4;++i)value+=QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,'0');
    return value;
}
bool available(const QJsonObject& token){return !token.value("access_token").toString().isEmpty();}
bool expired(const QJsonObject& token){
    const double expiry=token.value("expires_at").toDouble(0);
    return expiry>0&&expiry<double(QDateTime::currentSecsSinceEpoch()+60);
}
bool manualPlatformUrl(const QString& raw,const QString& platform){
    const QUrl url(raw);
    if(url.scheme()!="https"||!url.userInfo().isEmpty()||!url.fragment().isEmpty())return false;
    const QString host=url.host().toLower();
    if(platform=="YouTube"){
        if(host!="youtube.com"&&host!="www.youtube.com"&&host!="youtu.be")return false;
        if(!url.hasQuery())return true;
        const QUrlQuery query(url);
        return url.path()=="/watch"&&query.queryItems().size()==1&&query.hasQueryItem("v")&&
            query.queryItemValue("v").size()<=32;
    }
    return (host=="twitch.tv"||host=="www.twitch.tv")&&!url.hasQuery();
}
}

QString PlatformProbe::accountPath(const QString& name,bool chat)const{
    char* raw=obs_module_config_path(name.toUtf8().constData());
    if(!raw)return {};
    const QString own=QString::fromUtf8(raw);bfree(raw);
    if(!chat)return own;
    return QDir(QFileInfo(own).absolutePath()).absoluteFilePath("../stream-chat-obs/"+name);
}
PlatformProbe::PlatformProbe(QObject* parent):QObject(parent),googleLogin_(this),
    ownYouTube_(accountPath("youtube-account.dat",false)),
    ownTwitch_(accountPath("twitch-account.dat",false)),
    chatYouTube_(accountPath("youtube-account.dat",true)),
    chatTwitch_(accountPath("twitch-account.dat",true)),chatKick_(accountPath("kick-account.dat",true)){
    twitchDeviceTimer_.setSingleShot(true);
    connect(&twitchDeviceTimer_,&QTimer::timeout,this,[this]{pollTwitchDevice();});
    connect(&googleLogin_,&OAuthLoopback::failed,this,[this](const QString&){failYouTube("YouTube authorization cancelled or expired");});
    connect(&googleLogin_,&OAuthLoopback::received,this,[this](const QUrlQuery& q){
        const QString code=q.queryItemValue("code",QUrl::FullyDecoded);
        const QByteArray body=form({{"client_id",googleClient},{"client_secret",googleSecret()},
            {"grant_type","authorization_code"},{"code",code},{"redirect_uri",googleLogin_.redirect().toString()},
            {"code_verifier",googleVerifier_}});
        googleVerifier_.clear();
        request(QUrl("https://oauth2.googleapis.com/token"),"POST",body,{}, {},[this](int http,QJsonObject data){
            if(http!=200||!available(data)){failYouTube(QString("YouTube authorization failed (HTTP %1)").arg(http));return;}
            data["expires_at"]=double(QDateTime::currentSecsSinceEpoch()+std::clamp(data.value("expires_in").toInt(3600),60,86400));
            saveOwnYouTube(data);checkYouTube();
        });
    });
    loadAccounts();
}
void PlatformProbe::loadAccounts(){
    const auto oldYouTube=youtubeToken_.value("access_token");
    const auto oldTwitch=twitchToken_.value("access_token");
    bool ok=false;auto own=ownYouTube_.load(&ok);auto chat=chatYouTube_.load(&ok);
    youtubeFromChat_=!available(own)&&available(chat);youtubeToken_=youtubeFromChat_?chat:own;
    own=ownTwitch_.load(&ok);chat=chatTwitch_.load(&ok);
    twitchFromChat_=!available(own)&&available(chat);twitchToken_=twitchFromChat_?chat:own;
    if(youtubeToken_.value("access_token")!=oldYouTube)youtubeRetryAt_={};
    if(twitchToken_.value("access_token")!=oldTwitch)twitchRetryAt_={};
}
QString PlatformProbe::googleSecret()const{
#ifdef STREAM_NOTIFY_UI_PREVIEW
    return {};
#else
    char* raw=obs_module_file("google-oauth.json");if(!raw)return {};
    const QString ownPath=QString::fromUtf8(raw);bfree(raw);
    QFile file(ownPath);
    if(!file.open(QIODevice::ReadOnly)){
        file.setFileName(QDir(QFileInfo(ownPath).absolutePath()).absoluteFilePath("../stream-chat-obs/google-oauth.json"));
        if(!file.open(QIODevice::ReadOnly))return {};
    }
    const auto data=QJsonDocument::fromJson(file.readAll()).object();
    return data.value("client_id").toString()==googleClient?data.value("client_secret").toString():QString();
#endif
}
QString PlatformProbe::twitchApiClient()const{
    const auto personal=twitchToken_.value("client_id").toString();
    return twitchFromChat_&&!personal.isEmpty()?personal:twitchClient;
}
QString PlatformProbe::youtubeAccount()const{
    if(!available(youtubeToken_))return "YouTube: connect account";
    return youtubeFromChat_?"YouTube: OZK Chat account":"YouTube: OZK Notify account";
}
QString PlatformProbe::twitchAccount()const{
    if(!available(twitchToken_))return "Twitch: connect account";
    return twitchFromChat_?"Twitch: OZK Chat account":"Twitch: OZK Notify account";
}
void PlatformProbe::setYouTube(LivePlatform value){
    if(value.phase==LivePhase::Live&&value.confirmed)lastYouTubeId_=value.id;
    if(value.phase==LivePhase::Offline)lastYouTubeId_.clear();
    youtube_=std::move(value);if(changed_)changed_();
}
void PlatformProbe::setTwitch(LivePlatform value){
    if(value.phase==LivePhase::Live&&value.confirmed)lastTwitchId_=value.id;
    if(value.phase==LivePhase::Offline)lastTwitchId_.clear();
    twitch_=std::move(value);if(changed_)changed_();
}
void PlatformProbe::failYouTube(const QString& reason){
    LivePlatform next=youtube_;next.error=reason;next.checkedAt=QDateTime::currentDateTimeUtc();
    if(youtube_.confirmed&&youtube_.phase==LivePhase::Live){
        next.phase=LivePhase::Reconnecting;next.confirmed=false;setYouTube(next);return;
    }
    if(youtubeLocal_&&!youtubeFallback_.isEmpty()&&manualPlatformUrl(youtubeFallback_,"YouTube")){
        next.phase=LivePhase::Live;next.confirmed=false;next.url=youtubeFallback_;
        next.id=lastYouTubeId_.isEmpty()?"manual:"+youtubeFallback_:lastYouTubeId_;
    }else{next.phase=LivePhase::Error;next.confirmed=false;next.url.clear();next.id.clear();}
    setYouTube(next);
}
void PlatformProbe::failTwitch(const QString& reason){
    LivePlatform next=twitch_;next.error=reason;next.checkedAt=QDateTime::currentDateTimeUtc();
    if(twitch_.confirmed&&twitch_.phase==LivePhase::Live){
        next.phase=LivePhase::Reconnecting;next.confirmed=false;setTwitch(next);return;
    }
    if(twitchLocal_&&!twitchFallback_.isEmpty()&&manualPlatformUrl(twitchFallback_,"Twitch")){
        next.phase=LivePhase::Live;next.confirmed=false;next.url=twitchFallback_;
        next.id=lastTwitchId_.isEmpty()?"manual:"+twitchFallback_:lastTwitchId_;
    }else{next.phase=LivePhase::Error;next.confirmed=false;next.url.clear();next.id.clear();}
    setTwitch(next);
}
void PlatformProbe::request(const QUrl& url,const QByteArray& method,const QByteArray& body,
                            const QByteArray& bearer,const QByteArray& clientId,Response done){
    if(stopping_)return;
    QNetworkRequest req(url);req.setTransferTimeout(12000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader,"StreamNotify/1.1");
    if(!bearer.isEmpty())req.setRawHeader("Authorization",bearer);
    if(!clientId.isEmpty())req.setRawHeader("Client-Id",clientId);
    if(method=="POST")req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    auto* reply=sn::watchReply(method=="GET"?http_.get(req):http_.post(req,body),body);
    replies_.insert(reply);reply->setReadBufferSize(1048576);const quint64 epoch=epoch_;
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>1048576)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,epoch,done=std::move(done)]{
        replies_.remove(reply);const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw=reply->readAll().left(1048576);
        const auto data=QJsonDocument::fromJson(raw).object();reply->deleteLater();
        if(epoch==epoch_&&!stopping_)done(http,data);
    });
}
void PlatformProbe::check(bool youtubeOutput,bool twitchOutput,const QString& youtubeFallback,const QString& twitchFallback,bool kickOutput,const QString& kickFallback){
    if(stopping_)return;
    if(kickOutput&&!kickLocal_){kickManualId_="manual:kick:"+QString::number(QDateTime::currentMSecsSinceEpoch());kickRetryAt_={};}
    kickLocal_=kickOutput;kickFallback_=kickFallback.trimmed();
    youtubeLocal_=youtubeOutput;twitchLocal_=twitchOutput;
    youtubeFallback_=youtubeFallback.trimmed();twitchFallback_=twitchFallback.trimmed();
    if(!available(youtubeToken_)||!available(twitchToken_))loadAccounts();
    if(!twitchBusy_)checkTwitch();
    if(!kickBusy_)checkKick();
    QTimer::singleShot(1700,this,[this]{if(!stopping_&&!youtubeBusy_)checkYouTube();});
}
void PlatformProbe::setKick(LivePlatform value){
    if(value.confirmed&&value.phase==LivePhase::Live)lastKickId_=value.id;
    if(value.phase==LivePhase::Offline)lastKickId_.clear();kick_=std::move(value);if(changed_)changed_();
}
void PlatformProbe::failKick(const QString& reason){
    LivePlatform next=kick_;next.error=reason;next.checkedAt=QDateTime::currentDateTimeUtc();next.confirmed=false;
    const QUrl manual(kickFallback_);
    if(kickLocal_&&manual.scheme()=="https"&&(manual.host()=="kick.com"||manual.host()=="www.kick.com")&&manual.userInfo().isEmpty()&&!manual.hasQuery()&&manual.fragment().isEmpty()&&QRegularExpression("\\A/[a-zA-Z0-9_-]{1,100}/?\\z").match(manual.path()).hasMatch()){
        next.phase=LivePhase::Live;next.url=manual.toString();next.id=lastKickId_.isEmpty()?kickManualId_:lastKickId_;
    }else{next.phase=LivePhase::Error;next.url.clear();next.id.clear();}setKick(next);
}
void PlatformProbe::checkKick(){
    if(stopping_||kickBusy_)return;if(!kickLocal_){setKick({});return;}
    if(kickRetryAt_.isValid()&&QDateTime::currentDateTimeUtc()<kickRetryAt_)return;
    QString channel;const QUrl manual(kickFallback_);
    if(!kickFallback_.isEmpty()){
        if(manual.scheme()!="https"||(manual.host()!="kick.com"&&manual.host()!="www.kick.com")||!manual.userInfo().isEmpty()||manual.hasQuery()||!manual.fragment().isEmpty()){failKick("Kick: check the channel link");return;}
        channel=manual.path();if(channel.startsWith('/'))channel.remove(0,1);if(channel.endsWith('/'))channel.chop(1);
    }else channel=chatKick_.load().value("channel").toString();
    if(!QRegularExpression("\\A[a-zA-Z0-9_-]{1,100}\\z").match(channel).hasMatch()){failKick("Kick: enter a channel link or configure OZK Chat");return;}
    kickBusy_=true;const auto expected=kickManualId_;
    request(QUrl("https://kick.com/api/v2/channels/"+channel.toLower()),"GET",{},{},{},[this,channel,expected](int http,QJsonObject data){
        kickBusy_=false;if(!kickLocal_||expected!=kickManualId_)return;
        if(http!=200){if(http==403||http==429)kickRetryAt_=QDateTime::currentDateTimeUtc().addSecs(120);failKick(QString("Kick API HTTP %1").arg(http));return;}
        kickRetryAt_={};
        LivePlatform next;next.checkedAt=QDateTime::currentDateTimeUtc();const auto live=data.value("livestream").toObject();
        if(live.value("is_live").toBool()){
            next.id=live.value("id").toVariant().toString();if(next.id.isEmpty()){failKick("Kick: invalid stream response");return;}
            next.phase=LivePhase::Live;next.confirmed=true;next.url="https://kick.com/"+channel.toLower();
        }setKick(next);
    });
}
void PlatformProbe::checkYouTube(){
    if(stopping_||youtubeBusy_)return;
    if(youtubeRetryAt_.isValid()&&QDateTime::currentDateTimeUtc()<youtubeRetryAt_)return;
    if(!available(youtubeToken_)){failYouTube("YouTube account not connected; sign in or use OZK Chat");return;}
    if(expired(youtubeToken_)){refreshYouTube();return;}
    youtubeBusy_=true;
    if(!youtube_.confirmed){LivePlatform starting=youtube_;starting.phase=LivePhase::Starting;setYouTube(starting);}
    QUrl url("https://www.googleapis.com/youtube/v3/liveBroadcasts");QUrlQuery q;
    q.addQueryItem("part","id,status,snippet");q.addQueryItem("broadcastStatus","active");
    q.addQueryItem("broadcastType","all");q.addQueryItem("maxResults","50");url.setQuery(q);
    request(url,"GET",{},("Bearer "+youtubeToken_.value("access_token").toString()).toUtf8(),{},
        [this](int http,QJsonObject data){
            youtubeBusy_=false;
            if(http==401){refreshYouTube();return;}
            if(http!=200){
                if(http==429||http==403)youtubeRetryAt_=QDateTime::currentDateTimeUtc().addSecs(http==429?120:300);
                failYouTube(http==403?"YouTube permission or quota error; sign in again":QString("YouTube API HTTP %1").arg(http));return;
            }
            youtubeRetryAt_={};
            LivePlatform next;next.checkedAt=QDateTime::currentDateTimeUtc();
            for(const auto& item:data.value("items").toArray()){
                const auto object=item.toObject(),status=object.value("status").toObject();
                if(status.value("lifeCycleStatus").toString()!="live")continue;
                const QString privacy=status.value("privacyStatus").toString();
                if(privacy!="public"&&privacy!="unlisted")continue;
                const QString id=object.value("id").toString();if(id.isEmpty())continue;
                next.phase=LivePhase::Live;next.confirmed=true;next.id=id;
                next.url="https://www.youtube.com/watch?v="+id;break;
            }
            setYouTube(next);
        });
}
void PlatformProbe::checkTwitch(){
    if(stopping_||twitchBusy_)return;
    if(twitchRetryAt_.isValid()&&QDateTime::currentDateTimeUtc()<twitchRetryAt_)return;
    if(!available(twitchToken_)){failTwitch("Twitch account not connected; sign in or use OZK Chat");return;}
    if(twitchLogin_.isEmpty()){validateTwitch();return;}
    twitchBusy_=true;
    if(!twitch_.confirmed){LivePlatform starting=twitch_;starting.phase=LivePhase::Starting;setTwitch(starting);}
    QUrl url("https://api.twitch.tv/helix/streams");QUrlQuery q;q.addQueryItem("user_login",twitchLogin_);url.setQuery(q);
    request(url,"GET",{},("Bearer "+twitchToken_.value("access_token").toString()).toUtf8(),twitchApiClient().toUtf8(),
        [this](int http,QJsonObject data){
            twitchBusy_=false;
            if(http==401){refreshTwitch();return;}
            if(http!=200){
                if(http==429||http==403)twitchRetryAt_=QDateTime::currentDateTimeUtc().addSecs(http==429?120:300);
                failTwitch(QString("Twitch API HTTP %1").arg(http));return;
            }
            twitchRetryAt_={};
            LivePlatform next;next.checkedAt=QDateTime::currentDateTimeUtc();
            for(const auto& item:data.value("data").toArray()){
                const auto object=item.toObject();if(object.value("type").toString()!="live")continue;
                const QString id=object.value("id").toString();if(id.isEmpty())continue;
                next.phase=LivePhase::Live;next.confirmed=true;next.id=id;
                next.url="https://www.twitch.tv/"+twitchLogin_;break;
            }
            setTwitch(next);
        });
}
void PlatformProbe::validateTwitch(){
    if(twitchBusy_)return;twitchBusy_=true;
    request(QUrl("https://id.twitch.tv/oauth2/validate"),"GET",{},
        ("OAuth "+twitchToken_.value("access_token").toString()).toUtf8(),{},
        [this](int http,QJsonObject data){
            twitchBusy_=false;
            if(http==401){refreshTwitch();return;}
            if(http!=200||data.value("client_id").toString()!=twitchApiClient()){
                if(http==429||http==403)twitchRetryAt_=QDateTime::currentDateTimeUtc().addSecs(http==429?120:300);
                failTwitch(QString("Twitch authorization failed (HTTP %1)").arg(http));return;
            }
            twitchRetryAt_={};
            twitchLogin_=data.value("login").toString().toLower();
            if(twitchLogin_.isEmpty()){failTwitch("Twitch account login unavailable");return;}
            checkTwitch();
        });
}
void PlatformProbe::refreshYouTube(){
    if(youtubeBusy_)return;const QString refresh=youtubeToken_.value("refresh_token").toString();
    if(youtubeFromChat_){
        const auto updated=chatYouTube_.load();
        if(available(updated)&&updated.value("access_token")!=youtubeToken_.value("access_token")){
            youtubeToken_=updated;checkYouTube();return;
        }
        failYouTube("OZK Chat YouTube token expired; reconnect OZK Chat or sign in here");return;
    }
    const QString secret=googleSecret();
    if(refresh.isEmpty()||secret.isEmpty()){failYouTube("YouTube authorization expired; sign in again");return;}
    youtubeBusy_=true;
    request(QUrl("https://oauth2.googleapis.com/token"),"POST",
        form({{"client_id",googleClient},{"client_secret",secret},{"grant_type","refresh_token"},{"refresh_token",refresh}}),{}, {},
        [this,refresh](int http,QJsonObject data){
            youtubeBusy_=false;
            if(http!=200||!available(data)){failYouTube(http==400||http==401?"YouTube authorization expired; sign in again":QString("YouTube token refresh HTTP %1").arg(http));return;}
            data["refresh_token"]=refresh;
            data["expires_at"]=double(QDateTime::currentSecsSinceEpoch()+std::clamp(data.value("expires_in").toInt(3600),60,86400));
            youtubeToken_=data;if(!youtubeFromChat_)saveOwnYouTube(data);
            checkYouTube();
        });
}
void PlatformProbe::refreshTwitch(){
    if(twitchBusy_)return;const QString refresh=twitchToken_.value("refresh_token").toString();
    if(twitchFromChat_){
        const auto updated=chatTwitch_.load();
        if(available(updated)&&updated.value("access_token")!=twitchToken_.value("access_token")){
            twitchToken_=updated;twitchLogin_.clear();validateTwitch();return;
        }
        failTwitch("OZK Chat Twitch token expired; reconnect OZK Chat or sign in here");return;
    }
    if(refresh.isEmpty()){failTwitch("Twitch authorization expired; sign in again");return;}
    twitchBusy_=true;
    request(QUrl("https://id.twitch.tv/oauth2/token"),"POST",
        form({{"client_id",twitchClient},{"grant_type","refresh_token"},{"refresh_token",refresh}}),{}, {},
        [this,refresh](int http,QJsonObject data){
            twitchBusy_=false;
            if(http!=200||!available(data)){failTwitch(http==400||http==401?"Twitch authorization expired; sign in again":QString("Twitch token refresh HTTP %1").arg(http));return;}
            if(data.value("refresh_token").toString().isEmpty())data["refresh_token"]=refresh;
            twitchToken_=data;if(!twitchFromChat_)saveOwnTwitch(data);
            twitchLogin_.clear();validateTwitch();
        });
}
void PlatformProbe::saveOwnYouTube(const QJsonObject& value){
    if(!ownYouTube_.save(value)){failYouTube("Cannot save YouTube authorization");return;}
    youtubeFromChat_=false;youtubeToken_=value;if(changed_)changed_();
}
void PlatformProbe::saveOwnTwitch(const QJsonObject& value){
    if(!ownTwitch_.save(value)){failTwitch("Cannot save Twitch authorization");return;}
    twitchFromChat_=false;twitchToken_=value;twitchLogin_.clear();if(changed_)changed_();
}
void PlatformProbe::beginYouTubeLogin(){
    if(googleSecret().isEmpty()){failYouTube("Google OAuth configuration missing; install complete plugin archive");return;}
    if(!googleLogin_.begin(0,"/google/callback",false)){failYouTube("Local Google authorization port unavailable");return;}
    googleVerifier_=randomVerifier();const auto hash=QCryptographicHash::hash(googleVerifier_.toUtf8(),QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);
    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");QUrlQuery q;
    q.addQueryItem("client_id",googleClient);q.addQueryItem("redirect_uri",googleLogin_.redirect().toString());
    q.addQueryItem("response_type","code");q.addQueryItem("scope","https://www.googleapis.com/auth/youtube.readonly");
    q.addQueryItem("state",googleLogin_.state());q.addQueryItem("code_challenge",QString::fromLatin1(hash));
    q.addQueryItem("code_challenge_method","S256");q.addQueryItem("access_type","offline");
    q.addQueryItem("prompt","consent");url.setQuery(q);
    if(!QDesktopServices::openUrl(url)){googleLogin_.stop();failYouTube("Cannot open browser for YouTube authorization");}
}
void PlatformProbe::beginTwitchLogin(){
    twitchDeviceTimer_.stop();twitchDevice_.clear();twitchCode_.clear();
    request(QUrl("https://id.twitch.tv/oauth2/device"),"POST",form({{"client_id",twitchClient},{"scopes",twitchScope}}),{}, {},
        [this](int http,QJsonObject data){
            const QUrl url(data.value("verification_uri").toString());
            if(http!=200||data.value("device_code").toString().isEmpty()||url.scheme()!="https"||url.host()!="www.twitch.tv"){
                failTwitch(QString("Twitch authorization HTTP %1").arg(http));return;
            }
            twitchDevice_=data.value("device_code").toString();twitchCode_=data.value("user_code").toString();
            twitchDeviceInterval_=std::clamp(data.value("interval").toInt(5),5,60);
            twitchDeviceExpires_=QDateTime::currentDateTimeUtc().addSecs(std::clamp(data.value("expires_in").toInt(1800),30,1800));
            if(changed_)changed_();
            QDesktopServices::openUrl(url);twitchDeviceTimer_.start(twitchDeviceInterval_*1000);
        });
}
void PlatformProbe::pollTwitchDevice(){
    if(twitchDevice_.isEmpty())return;
    if(QDateTime::currentDateTimeUtc()>=twitchDeviceExpires_){twitchDevice_.clear();twitchCode_.clear();failTwitch("Twitch authorization expired");return;}
    request(QUrl("https://id.twitch.tv/oauth2/token"),"POST",
        form({{"client_id",twitchClient},{"scopes",twitchScope},{"device_code",twitchDevice_},
              {"grant_type","urn:ietf:params:oauth:grant-type:device_code"}}),{}, {},
        [this](int http,QJsonObject data){
            if(http==200&&available(data)){
                twitchDevice_.clear();twitchCode_.clear();saveOwnTwitch(data);validateTwitch();return;
            }
            const QString error=data.value("message").toString(data.value("error").toString());
            if(error=="authorization_pending"||error=="slow_down"||http==429){
                if(error=="slow_down"||http==429)twitchDeviceInterval_=std::min(60,twitchDeviceInterval_+5);
                twitchDeviceTimer_.start(twitchDeviceInterval_*1000);return;
            }
            twitchDevice_.clear();twitchCode_.clear();failTwitch(QString("Twitch authorization failed (HTTP %1)").arg(http));
        });
}
void PlatformProbe::signOutYouTube(){googleLogin_.stop();ownYouTube_.save({});youtubeToken_={};loadAccounts();setYouTube({});}
void PlatformProbe::signOutTwitch(){twitchDeviceTimer_.stop();ownTwitch_.save({});twitchToken_={};twitchLogin_.clear();loadAccounts();setTwitch({});}
void PlatformProbe::stop(){
    stopping_=true;++epoch_;googleLogin_.stop();twitchDeviceTimer_.stop();
    const auto pending=replies_.values();replies_.clear();for(auto* reply:pending)reply->abort();
}
}
