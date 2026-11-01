#include "donations.hpp"
#include "resources.hpp"
#include "donation_logic.hpp"
#include "donatepay_reply.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
QString ledgerPath() {
    char* path = obs_module_config_path("donation-ledger.json");
    if(!path) return {};
    QString result = QString::fromUtf8(path); bfree(path); return result;
}
QString ratesPath() {
    char* raw=obs_module_config_path("exchange-rates.json");
    if(!raw)return {};
    const QString path=QString::fromUtf8(raw);bfree(raw);return path;
}
QJsonObject savedRates() {
    QFile file(ratesPath());
    if(!file.open(QIODevice::ReadOnly))return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QString jsonString(const QJsonValue& value) {
    return value.isString() ? value.toString() : value.isDouble() ? QString::number(value.toDouble(),'f',0) : QString();
}
QString first(const QJsonObject& j, std::initializer_list<const char*> keys) {
    for(const auto* k : keys) {QString v=jsonString(j.value(QLatin1String(k))); if(!v.isEmpty()) return v;}
    return {};
}
QJsonObject normalize(const QJsonObject& j) {
    QJsonObject r=j;
    for(int i=0;i<4;i++) {
        const QJsonValue data=r.value("data");
        if (!data.isObject()) break;
        r=data.toObject();
    }
    return r;
}
QString rawKey(const QString& service, const QString& id) {return service + ":" + id;}
QString failureStatus(int status) {
    if(status==401 || status==403) return "Invalid or expired token";
    if(status==429) return "Rate limited";
    return status>0 ? QString("HTTP %1").arg(status) : "Network error";
}
bool validBearerToken(const QString& token) {
    static const QRegularExpression tokenRE("^[A-Za-z0-9._~+/-]+=*$");
    return tokenRE.match(token).hasMatch();
}
bool trueFlag(const QJsonValue& value) {
    return value.toBool(false)||(value.isDouble()&&value.toDouble()==1)||
        (value.isString()&&(value.toString()=="1"||value.toString().compare("true",Qt::CaseInsensitive)==0));
}
QString streamUrl(const Settings& s) {
    if(!s.youtubeUrl.trimmed().isEmpty()) return s.youtubeUrl.trimmed();
    return s.twitchUrl.trimmed();
}
QString cleanUserText(QString v) {
    // Prevent hidden controls, invite spam and accidental mass-mentions.
    v.remove(QRegularExpression("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]"));
    v.replace(QRegularExpression("@(?=(?:everyone|here)(?:\\b|$))",QRegularExpression::CaseInsensitiveOption),(QString("@")+QChar(0x200B)));
    v.replace(QRegularExpression("<@(?:&|!|)[0-9]+>"), "[mention]");
    if(v.size()>650) v=v.left(650)+"…";
    return v.trimmed();
}
}

int donationLevel(const Donation& d,const DonationSettings& s) {
    if(d.isTest||d.id.isEmpty())return -1;
    const QString cur=d.currency.trimmed().toUpper();
    const auto explicitThresholds=s.thresholds.value(cur);
    if(explicitThresholds.size()==3)return donation_logic::levelForAmount(d.amount,
        {explicitThresholds[0],explicitThresholds[1],explicitThresholds[2]});
    const auto thresholds=s.thresholds.value("RUB");
    if(thresholds.size()!=3)return -1;
    const double rate=cur=="RUB"?1.0:savedRates().value("rub_per_unit").toObject().value(cur).toDouble(0);
    return donation_logic::levelForConvertedAmount(d.amount,rate,
        {thresholds[0],thresholds[1],thresholds[2]});
}

QString donationMessage(const Donation& d, const DonationSettings& s,int level,const QString& url) {
    if(level<0 || level>2 || s.templates.size()<3) return {};
    QString text=s.templates[level];
    QStringList lines;
    for(QString line : text.split('\n')) {
        if((!s.includeDonorMessage || d.message.trimmed().isEmpty()) && line.contains("{message}")) continue;
        line.replace("{donor}", cleanUserText(d.donor.isEmpty()?QString::fromUtf8("Аноним"):d.donor));
        line.replace("{amount}", QString::number(d.amount, 'f', (std::floor(d.amount)==d.amount ? 0:2)));
        line.replace("{currency}", d.currency.toUpper());
        line.replace("{message}", cleanUserText(d.message));
        line.replace("{service}", d.service);
        line.replace("{stream_url}", url);
        lines.append(line);
    }
    text=lines.join('\n').trimmed();
    // Discord content hard limit; prefer truncation to rejecting an actual donation.
    return text.left(1900);
}

Donation parseDonationAlerts(const QJsonObject& packet) {
    Donation d; d.service="DonationAlerts";
    const QJsonObject j=normalize(packet);
    d.id=first(j,{"id"});
    d.donor=first(j,{"username","name"});
    d.message=first(j,{"message"});
    d.currency=first(j,{"currency"}).toUpper();
    d.amount=j.value("amount").isString()?j.value("amount").toString().toDouble():j.value("amount").toDouble();
    d.isTest=trueFlag(j.value("is_test"))||trueFlag(j.value("test"))||trueFlag(j.value("is_fake"))||j.value("type").toString()=="test";
    return d;
}

Donation parseDonatePay(const QJsonObject& packet) {
    Donation d; d.service="DonatePay";
    QJsonObject j=normalize(packet);
    d.id=first(j,{"id","transaction_id"});
    d.donor=first(j,{"what","name","username","donor_name"});
    d.message=first(j,{"comment","message"});
    d.currency=first(j,{"currency"}).toUpper();
    // Missing currency is not inferred from account balance or locale.
    // Unknown currencies are rejected by donationLevel until the API schema is verified.
    const QJsonValue a = j.contains("sum")?j.value("sum"):j.value("amount");
    d.amount=a.isString()?a.toString().toDouble():a.toDouble();
    const QString type=first(j,{"type"}).toLower();
    const QString status=first(j,{"status"}).toLower();
    // Do not treat unknown status/types as confirmed donations.
    d.isTest=trueFlag(j.value("is_test")) || trueFlag(j.value("test")) ||
             type!="donation" || status!="success";
    return d;
}

DonationLedger::DonationLedger(const QString& path):path_(path){}
bool DonationLedger::load() {
    sn::CpuScope snCpuScope;
    QFile f(path_); if(!f.exists()) return true;
    if(!f.open(QIODevice::ReadOnly)){error_="Cannot read donation journal";return false;}
    const QByteArray bytes=f.readAll();f.close();
    QJsonParseError e; auto j=QJsonDocument::fromJson(bytes,&e);
    if(e.error!=QJsonParseError::NoError || !j.isObject()){error_="Donation journal corrupted: no events will be sent";return false;}
    if(j.object().value("version").toInt()!=1||!j.object().value("items").isObject()){
        error_="Donation journal schema invalid: no events will be sent";return false;
    }
    const QJsonObject items=j.object().value("items").toObject();
    bool repaired=false;
    for(auto it=items.begin();it!=items.end();++it) {
        if(!it.value().isObject()){error_="Donation journal record invalid: no events will be sent";return false;}
        QJsonObject record=it.value().toObject();
        for(const QString& dest:{QString("telegram"),QString("discord")}) {
            if(record.value(dest).toString()=="inflight") {record[dest]="unknown";repaired=true;}
            if(record.value(dest).toString()=="pending") {record[dest]="not_sent_after_restart";repaired=true;}
        }
        items_[it.key()]=record;
    }
    return !repaired || persist();
}
bool DonationLedger::contains(const QString& key) const {
    sn::CpuScope snCpuScope;return items_.contains(key);}
bool DonationLedger::persist() {
    sn::CpuScope snCpuScope;
    if(path_.isEmpty() || !QDir().mkpath(QFileInfo(path_).absolutePath())){error_="Cannot create donation journal folder";return false;}
    QJsonObject items; for(auto it=items_.cbegin();it!=items_.cend();++it)items[it.key()]=it.value();
    QSaveFile f(path_);
    if(!f.open(QIODevice::WriteOnly)){error_="Cannot write donation journal";return false;}
    if(f.write(QJsonDocument(QJsonObject{{"version",1},{"items",items}}).toJson(QJsonDocument::Compact))<0 || !f.commit()){
       error_="Cannot commit donation journal";return false;
    }
    error_.clear();return true;
}
bool DonationLedger::claim(const QString& key,const Donation& d,int level,bool tg,bool dc) {
    sn::CpuScope snCpuScope;
    if(contains(key))return false;
    QJsonObject item{{"timestamp",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
      {"service",d.service},{"amount",d.amount},{"currency",d.currency},{"level",level+1},
      {"telegram",tg?"pending":"disabled"},{"discord",dc?"pending":"disabled"}};
    items_[key]=item;
    if(!persist()){items_.remove(key);return false;}
    return true;
}
bool DonationLedger::setStatus(const QString& key,const QString& channel,const QString& status) {
    sn::CpuScope snCpuScope;
    if(!items_.contains(key))return false;
    const QJsonObject old=items_[key]; QJsonObject item=old; item[channel]=status;items_[key]=item;
    if(!persist()){items_[key]=old;return false;}
    return true;
}
QStringList DonationLedger::recent()const{
    sn::CpuScope snCpuScope;
    QStringList lines;
    for(auto it=items_.cend();it!=items_.cbegin() && lines.size()<100;){
        --it;
        const QJsonObject j=it.value();
        lines << j.value("timestamp").toString()+"  ["+j.value("service").toString()+"] "+
        QString::number(j.value("amount").toDouble(),'f',2)+" "+j.value("currency").toString()+
        "  L"+QString::number(j.value("level").toInt())+
        "  TG:"+j.value("telegram").toString()+"  DC:"+j.value("discord").toString();
    }
    return lines;
}

DonationEngine::DonationEngine(SettingsStore* settings,QObject* parent):QObject(parent),store_(settings),sender_(this),daLogin_(this),ledger_(ledgerPath()) {
    if(!ledger_.load()){ closing_=true; state(daStatus_,"Journal error");state(dpStatus_,"Journal error"); }
    daRetry_.setSingleShot(true);
    connect(&daRetry_,&QTimer::timeout,this,&DonationEngine::daConnect);
    dpTimer_.setSingleShot(true);
    connect(&dpTimer_,&QTimer::timeout,this,&DonationEngine::dpPoll);
    connect(&websocket_,&QWebSocket::connected,this,[this] { sn::CpuScope snCallbackScope; 
        if(closing_)return;
        QJsonObject hello{{"id",1},{"params",QJsonObject{{"token",daSocketToken_}}}};
        sendFrame(QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
    });
    connect(&websocket_,&QWebSocket::textMessageReceived,this,&DonationEngine::daReceive);
    connect(&websocket_,&QWebSocket::disconnected,this,[this]{ sn::CpuScope snCallbackScope; if(!closing_&&!resettingSocket_)daReconnect();});
    connect(&websocket_,&QWebSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){ sn::CpuScope snCallbackScope; if(!closing_&&!resettingSocket_)daReconnect();});
    connect(&daLogin_,&OAuthLoopback::failed,this,[this](const QString&){
        store_->addEvent("DONATIONALERTS AUTH_CANCELLED_OR_EXPIRED");state(daStatus_,"Authorization cancelled or expired");
    });
    connect(&daLogin_,&OAuthLoopback::received,this,[this](const QUrlQuery& query){
        const QString token=query.queryItemValue("access_token",QUrl::FullyDecoded);
        if(!validBearerToken(token)||query.queryItemValue("token_type").compare("bearer",Qt::CaseInsensitive)!=0){
            state(daStatus_,"Invalid authorization response");return;
        }
        const quint64 epoch=daLoginEpoch_;
        QNetworkRequest req(QUrl("https://www.donationalerts.com/api/v1/user/oauth"));
        req.setRawHeader("Authorization",("Bearer "+token).toUtf8());req.setTransferTimeout(10000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto* reply=sn::watchReply(http_.get(req));daLoginReply_=reply;
        state(daStatus_,"Verifying authorization");
        connect(reply,&QNetworkReply::finished,this,[this,reply,epoch,token]{
            const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto data=QJsonDocument::fromJson(reply->readAll()).object().value("data").toObject();
            reply->deleteLater();if(epoch!=daLoginEpoch_||closing_)return;
            daLoginReply_=nullptr;
            if(http!=200||first(data,{"id"}).isEmpty()||data.value("socket_connection_token").toString().isEmpty()){
                store_->addEvent(QString("DONATIONALERTS AUTH_FAILED HTTP_%1").arg(http));
                state(daStatus_,http==401?"OAuth token rejected (401)":http==403?"DonationAlerts permission denied (403)":"Authorization verification failed");return;
            }
            const QString previous=store_->current.donations.daToken;
            store_->current.donations.daToken=token;
            if(!store_->save()){
                store_->current.donations.daToken=previous;state(daStatus_,"Cannot save authorization");return;
            }
            store_->addEvent("DONATIONALERTS AUTHORIZED");
            configure();Q_EMIT updated();Q_EMIT credentialsChanged();
        });
    });
}
void DonationEngine::beginDonationAlertsLogin() {
    if(closing_)return;
    ++daLoginEpoch_;if(daLoginReply_)daLoginReply_->abort();daLoginReply_=nullptr;
    if(!daLogin_.begin(38561,"/donationalerts",true)){
        state(daStatus_,"Local authorization port unavailable");return;
    }
    QUrl url("https://www.donationalerts.com/oauth/authorize");QUrlQuery q;
    q.addQueryItem("client_id","22338");q.addQueryItem("redirect_uri",daLogin_.redirect().toString());
    q.addQueryItem("response_type","token");q.addQueryItem("scope","oauth-user-show oauth-donation-subscribe");
    q.addQueryItem("state",daLogin_.state());url.setQuery(q);
    state(daStatus_,"Waiting for browser authorization");
    store_->addEvent("DONATIONALERTS AUTH_STARTED");
    if(!QDesktopServices::openUrl(url)){daLogin_.stop();state(daStatus_,"Cannot open authorization browser");}
}
void DonationEngine::state(QString& dest,const QString& value) {
    sn::CpuScope snCpuScope;if(dest!=value){dest=value;
        store_->addEvent(QString(&dest==&daStatus_?"DONATIONALERTS ":"DONATEPAY ")+value);Q_EMIT updated();}}
void DonationEngine::clearConnections() {
    sn::CpuScope snCpuScope;
    ++daEpoch_;++dpEpoch_;daRetry_.stop();dpTimer_.stop();
    resettingSocket_=true;websocket_.abort();resettingSocket_=false;
    sender_.abortAll();
}
void DonationEngine::stop(){
    sn::CpuScope snCpuScope;closing_=true;++daLoginEpoch_;daLogin_.stop();
    if(daLoginReply_)daLoginReply_->abort();daLoginReply_=nullptr;clearConnections();}
void DonationEngine::setLive(bool active) {
    sn::CpuScope snCpuScope;
    const bool changed=(live_!=active);
    if(changed){if(active)sessionStats_.begin();else sessionStats_.end();}
    live_=active;
    if(!active && store_->current.donations.onlyLive) {
        // Inflight network delivery is indeterminate; do not retry after stop.
        sender_.abortAll();
        for(const QString& pending:openJobs_) {
            const int split=pending.lastIndexOf('|');
            if(split>0)ledger_.setStatus(pending.left(split),pending.mid(split+1),"unknown");
        }
        openJobs_.clear();Q_EMIT updated();
        // Journal entries claimed before POST remain safely protected from duplicate replay.
    }
    if(changed && store_->current.donations.onlyLive)configure();
}
void DonationEngine::configure() {
    sn::CpuScope snCpuScope;
    if(closing_)return;
    const auto& s=store_->current.donations;
    if(s.enabled)fetchRates();
    const bool monitoringPermitted=s.enabled && (!s.onlyLive || live_);
    const QString newDA=monitoringPermitted && s.daEnabled && validBearerToken(s.daToken)?s.daToken:QString();
    const QString newDP=monitoringPermitted && s.dpEnabled && !s.dpToken.isEmpty() && s.legacyApiOptIn?s.dpToken:QString();
    if(newDA!=daSignature_) {
        daSignature_=newDA;daRetry_.stop();
        resettingSocket_=true;websocket_.abort();resettingSocket_=false;
        ++daEpoch_;daBackoff_=0;
        if(!newDA.isEmpty())daConnect();
    }
    if(newDP!=dpSignature_) {
        dpSignature_=newDP;dpTimer_.stop();++dpEpoch_;dpBackoff_=0;dpPrimed_=false;dpBaseline_.clear();
        if(!newDP.isEmpty())dpPoll();
    }
    // Inactive signatures may stay empty across setting changes. Refresh their
    // reason every time, even when no connection must be restarted.
    if(newDA.isEmpty())state(daStatus_,!s.enabled?"Module disabled":!s.daEnabled?"Disabled":
        s.daToken.isEmpty()?"Not configured":!validBearerToken(s.daToken)?"Invalid token format":"Waiting for stream");
    if(newDP.isEmpty())state(dpStatus_,!s.enabled?"Module disabled":!s.dpEnabled?"Disabled":
        s.dpToken.isEmpty()?"Not configured":!s.legacyApiOptIn?"API verification required":"Waiting for stream");
}
void DonationEngine::daConnect() {
    sn::CpuScope snCpuScope;
    if(daSignature_.isEmpty()||closing_)return;
    ++daEpoch_;
    state(daStatus_,"Connecting");
    QNetworkRequest request(QUrl("https://www.donationalerts.com/api/v1/user/oauth"));
    request.setRawHeader("Authorization",("Bearer "+daSignature_).toUtf8());
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    const quint64 epoch=daEpoch_;
    QNetworkReply* r=sn::watchReply(http_.get(request));
    connect(r,&QNetworkReply::finished,this,[this,r,epoch]{ sn::CpuScope snCallbackScope; 
        int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data=r->readAll();r->deleteLater();
        if(epoch!=daEpoch_||closing_||daSignature_.isEmpty())return;
        if(status!=200) {
            state(daStatus_,status==401?"OAuth token rejected (401)":
                status==403?"DonationAlerts permission denied (403)":failureStatus(status));
            if(status!=401&&status!=403)daReconnect();return;
        }
        auto result=QJsonDocument::fromJson(data).object().value("data").toObject();
        const QString userId=first(result,{"id"});daSocketToken_=result.value("socket_connection_token").toString();
        if(userId.isEmpty()||daSocketToken_.isEmpty()){state(daStatus_,"Missing user/socket scope");return;}
        daChannel_="$alerts:donation_"+userId;
        websocket_.open(QUrl("wss://centrifugo.donationalerts.com/connection/websocket"));
    });
}
void DonationEngine::daSubscribe(const QString& clientId) {
    sn::CpuScope snCpuScope;
    QNetworkRequest request(QUrl("https://www.donationalerts.com/api/v1/centrifuge/subscribe"));
    request.setRawHeader("Authorization",("Bearer "+daSignature_).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    const auto data=QJsonDocument(QJsonObject{{"channels",QJsonArray{daChannel_}},{"client",clientId}}).toJson();
    const quint64 epoch=daEpoch_;
    QNetworkReply* r=sn::watchReply(http_.post(request,data),data);
    connect(r,&QNetworkReply::finished,this,[this,r,epoch]{ sn::CpuScope snCallbackScope; 
        int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument json=QJsonDocument::fromJson(r->readAll());r->deleteLater();
        if(epoch!=daEpoch_||closing_)return;
        if(status!=200){
            state(daStatus_,status==401?"OAuth token rejected (401)":
                status==403?"DonationAlerts permission denied (403)":failureStatus(status));
            if(status!=401&&status!=403)daReconnect();return;
        }
        QJsonArray channels=json.object().value("channels").toArray();
        if(channels.isEmpty()){state(daStatus_,"Channel subscribe failed");daReconnect();return;}
        const QString token=channels.first().toObject().value("token").toString();
        if(token.isEmpty()){state(daStatus_,"Channel token missing");return;}
        QJsonObject sub{{"id",2},{"method",1},{"params",QJsonObject{{"channel",daChannel_},{"token",token}}}};
        sendFrame(QString::fromUtf8(QJsonDocument(sub).toJson(QJsonDocument::Compact)));
    });
}
void DonationEngine::daReceive(const QString& raw) {
    sn::CpuScope snCpuScope;
    const QByteArray payloadBytes=raw.toUtf8();sn::Meter::instance().received(uint64_t(payloadBytes.size()));
    if(!store_->current.donations.enabled || !store_->current.donations.daEnabled)return;
    const QJsonObject packet=QJsonDocument::fromJson(payloadBytes).object();
    const int id=packet.value("id").toInt();
    const QJsonObject result=packet.value("result").toObject();
    if(id==1 && result.contains("client")) {daSubscribe(result.value("client").toString());return;}
    if(id==2 && !packet.contains("error")){daBackoff_=0;state(daStatus_,"Connected");return;}
    if(packet.contains("error")){
        const int code=packet.value("error").toObject().value("code").toInt();
        state(daStatus_,code==401||code==403?"Invalid or expired token":"Centrifugo error");
        if(code!=401&&code!=403)daReconnect();
        return;
    }
    if(result.value("channel").toString()!=daChannel_)return;
    if(result.value("type").toInt()==1)return; // subscription acknowledgement, not donation
    const QJsonObject payload=result.value("data").toObject();
    const QString reason=payload.value("reason").toString();
    if(!reason.isEmpty() && reason!="create" && reason!="new")return;
    Donation d=parseDonationAlerts(payload);
    if(!d.id.isEmpty())onDonation(d);
}
void DonationEngine::daReconnect() {
    sn::CpuScope snCpuScope;
    if(daSignature_.isEmpty()||closing_)return;
    ++daEpoch_;
    resettingSocket_=true;websocket_.abort();resettingSocket_=false;daRetry_.stop();
    daBackoff_=std::min(daBackoff_+1,7);
    const int ms=std::min(120000,2000*(1<<daBackoff_)) + QRandomGenerator::global()->bounded(1000);
    state(daStatus_,"Reconnecting");daRetry_.start(ms);
}
void DonationEngine::dpPoll() {
    sn::CpuScope snCpuScope;
    if(dpSignature_.isEmpty()||closing_)return;
    // EXPERIMENTAL / account-verification gated. DonatePay official API is login-gated.
    // A legacy GET endpoint and >=30s interval are used only with explicit opt-in.
    state(dpStatus_,"Connecting");
    QUrl u("https://donatepay.ru/api/v1/transactions");
    QUrlQuery q;q.addQueryItem("access_token",dpSignature_);q.addQueryItem("limit","100");
    q.addQueryItem("type","donation");q.addQueryItem("status","success");
    q.addQueryItem("order","DESC");
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setTransferTimeout(10000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    const quint64 epoch=dpEpoch_;
    QNetworkReply* r=sn::watchReply(http_.get(req));
    connect(r,&QNetworkReply::finished,this,[this,r,epoch]{ sn::CpuScope snCallbackScope; 
        int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes=r->readAll();const QByteArray retryAfter=r->rawHeader("Retry-After");r->deleteLater();
        if(epoch!=dpEpoch_||closing_||dpSignature_.isEmpty())return;
        const auto response=sn::parseDonatePayReply(status,bytes);
        if(!response.ok){
            state(dpStatus_,response.status);
            if(response.permanent){dpTimer_.stop();return;}
            if(response.rateLimited){
                bool valid=false;const double seconds=retryAfter.toDouble(&valid);
                const int delay=valid&&std::isfinite(seconds)&&seconds>0?int(std::min(seconds,3600.0)*1000):60000;
                dpTimer_.start(std::max(30000,delay));return;
            }
            dpReconnect();return;
        }
        const auto rows=response.rows;
        dpBackoff_=0;
        if(!dpPrimed_){
            for(const auto& row:rows) if(row.isObject())dpBaseline_.insert(first(row.toObject(),{"id","transaction_id"}));
            dpPrimed_=true;state(dpStatus_,response.status);dpTimer_.start(30000);return;
        }
        // Old-first; donations already present before monitoring are never sent.
        for(int i=rows.size()-1;i>=0;--i){
            if(!rows.at(i).isObject())continue;
            Donation d=parseDonatePay(rows.at(i).toObject());
            if(d.id.isEmpty()||dpBaseline_.contains(d.id))continue;
            dpBaseline_.insert(d.id);
            onDonation(d);
        }
        dpBaseline_.clear();for(const auto& row:rows)if(row.isObject())dpBaseline_.insert(first(row.toObject(),{"id","transaction_id"}));
        state(dpStatus_,response.status);dpTimer_.start(30000);
    });
}
void DonationEngine::dpReconnect(){
    sn::CpuScope snCpuScope;
    if(dpSignature_.isEmpty()||closing_)return;
    dpBackoff_=std::min(dpBackoff_+1,7);
    const int ms=std::max(30000,std::min(120000,30000*(1<<std::min(dpBackoff_,2))));
    dpTimer_.start(ms);
}
void DonationEngine::onDonation(const Donation& d) {
    sn::CpuScope snCpuScope;
    if(closing_)return;
    const auto& cfg=store_->current.donations;
    if(!cfg.enabled || d.isTest)return;
    if(d.id.isEmpty()||d.id.size()>256||!std::isfinite(d.amount)||d.amount<=0||
       !QRegularExpression("^[A-Z]{3}$").match(d.currency.trimmed().toUpper()).hasMatch())return;
    const QString statsKey=rawKey(d.service,d.id);
    if(ledger_.contains(statsKey))return;
    store_->addEvent(d.service+" DONATION_RECEIVED");
    const QString currency=d.currency.trimmed().toUpper();
    const double rate=currency=="RUB"?1.0:savedRates().value("rub_per_unit").toObject().value(currency).toDouble(0);
    if(!d.id.isEmpty() && !ledger_.contains(statsKey) &&
       sessionStats_.observe(statsKey,d.amount,rate,d.isTest,currency!="RUB"))Q_EMIT updated();
    const int level=donationLevel(d,cfg);
    const QString key=rawKey(d.service,d.id);
    if(ledger_.contains(key))return;
    const bool tg=level>=0&&cfg.toTelegram && store_->current.telegramEnabled;
    const bool dc=level>=0&&cfg.toDiscord && store_->current.discordEnabled;
    if(!ledger_.claim(key,d,level,tg,dc)) {state(daStatus_,"Journal write error");return;}
    if(level<0){store_->addEvent(d.service+" DONATION_BELOW_THRESHOLD_OR_UNPRICED");Q_EMIT updated();return;}
    if(!tg&&!dc){store_->addEvent(d.service+" DONATION_NO_RECIPIENTS");Q_EMIT updated();return;}
    if(cfg.onlyLive && !live_) {
        if(tg)ledger_.setStatus(key,"telegram","not_sent_stream_stopped");
        if(dc)ledger_.setStatus(key,"discord","not_sent_stream_stopped");
        Q_EMIT updated();return;
    }
    Q_EMIT updated();
    deliver(key,d,level);
}
void DonationEngine::deliver(const QString& key,const Donation& d,int level) {
    sn::CpuScope snCpuScope;
    const auto& s=store_->current;
    const QString body=donationMessage(d,s.donations,level,streamUrl(s));
    Snapshot snap=makeSnapshot(s);
    auto route=[this,&snap,&body,&key](Channel channel,bool active,const QString& name){ sn::CpuScope snCallbackScope; 
        if(!active)return;
        if(store_->current.donations.onlyLive && !live_){ledger_.setStatus(key,name,"cancelled");return;}
        const bool valid=channel==Channel::Telegram?validTelegram(snap.token,snap.chat):validDiscord(snap.webhook);
        if(!valid){ledger_.setStatus(key,name,"not_configured");return;}
        if(!ledger_.setStatus(key,name,"inflight"))return; // persist before POST
        openJobs_.insert(key+"|"+name);
        sender_.send(channel,snap,body,[this,key,name](Delivery result){ sn::CpuScope snCallbackScope; 
            openJobs_.remove(key+"|"+name);
            if(closing_)return;
            const QString status=result.status==Outcome::Sent?"sent":result.status==Outcome::Failed?"failed":"unknown";
            ledger_.setStatus(key,name,status);
            store_->addEvent("DONATION "+name.toUpper()+" "+status.toUpper());
            Q_EMIT updated();
        });
    };
    route(Channel::Telegram,s.donations.toTelegram&&s.telegramEnabled,"telegram");
    route(Channel::Discord,s.donations.toDiscord&&s.discordEnabled,"discord");
    Q_EMIT updated();
}
void DonationEngine::checkDonationAlerts() {
    sn::CpuScope snCpuScope;if(!daSignature_.isEmpty()){daRetry_.stop();websocket_.abort();daConnect();}}
void DonationEngine::checkDonatePay() {
    sn::CpuScope snCpuScope;if(!dpSignature_.isEmpty()){dpTimer_.stop();dpPrimed_=false;dpPoll();}}
void DonationEngine::previewTest(int level) {
    sn::CpuScope snCpuScope;
    const auto& s=store_->current;
    Donation d; d.service="OZK Notify TEST";d.id="test";d.currency="RUB";const auto thresholds=s.donations.thresholds.value("RUB");
    d.amount=thresholds.size()==3 && level>=0 && level<3?thresholds.at(level):100;
    d.donor="Test donor";d.message="Test donation";
    QString body=donationMessage(d,s.donations,level,streamUrl(s));
    Snapshot snap=makeSnapshot(s);
    // Manual test intentionally avoids ledger, never claims a real donation ID.
    if(s.donations.toTelegram&&s.telegramEnabled&&validTelegram(snap.token,snap.chat))
        sender_.send(Channel::Telegram,snap,"🧪 TEST / ТЕСТ\n"+body,[this](Delivery){ sn::CpuScope snCallbackScope; Q_EMIT updated();});
    if(s.donations.toDiscord&&s.discordEnabled&&validDiscord(snap.webhook))
        sender_.send(Channel::Discord,snap,"🧪 TEST / ТЕСТ\n"+body,[this](Delivery){ sn::CpuScope snCallbackScope; Q_EMIT updated();});
}

QString DonationEngine::ratesStatus() const {
    sn::CpuScope snCpuScope;
    const auto data=savedRates();
    if(!data.value("rub_per_unit").toObject().isEmpty())
        return "CBR / "+data.value("date").toString()+" / "+QString::number(data.value("rub_per_unit").toObject().size())+" currencies";
    return ratesLoading_?"Loading exchange rates":"No rates file: RUB only";
}
void DonationEngine::fetchRates(bool manual) {
    sn::CpuScope snCpuScope;
    if(closing_ || ratesLoading_)return;
    if(!manual && (ratesAttempted_ || !savedRates().value("rub_per_unit").toObject().isEmpty()))return;
    ratesAttempted_=true;ratesLoading_=true;
    QNetworkRequest req(QUrl("https://www.cbr-xml-daily.ru/daily_json.js"));
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto* reply=sn::watchReply(http_.get(req));
    connect(reply,&QNetworkReply::finished,this,[this,reply] { sn::CpuScope snCallbackScope; 
        ratesLoading_=false;
        const bool ok=reply->error()==QNetworkReply::NoError &&
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200;
        const auto input=QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        if(closing_)return;
        if(ok) {
            QJsonObject rates{{"RUB",1.0}};
            const auto currencies=input.value("Valute").toObject();
            for(auto it=currencies.constBegin();it!=currencies.constEnd();++it) {
                const auto row=it.value().toObject();
                const double nominal=row.value("Nominal").toDouble(0);
                const double value=row.value("Value").toDouble(0);
                const QString code=row.value("CharCode").toString().toUpper();
                if(QRegularExpression("^[A-Z]{3}$").match(code).hasMatch() &&
                    std::isfinite(nominal)&&nominal>0&&std::isfinite(value)&&value>0)
                    rates[code]=value/nominal;
            }
            const QString path=ratesPath();
            if(rates.size()>1 && !input.value("Date").toString().isEmpty() &&
                !path.isEmpty() && QDir().mkpath(QFileInfo(path).absolutePath())) {
                QSaveFile file(path);
                const QByteArray bytes=QJsonDocument(QJsonObject{
                    {"source","https://www.cbr-xml-daily.ru/daily_json.js"},
                    {"date",input.value("Date")},{"rub_per_unit",rates}}).toJson();
                if(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size())file.commit();
            }
        }
        Q_EMIT updated();
    });
    Q_EMIT updated();
}

void DonationEngine::sendFrame(const QString& data) {
    sn::CpuScope snCpuScope;
    sn::CpuScope scope;
    const auto sent=websocket_.sendTextMessage(data);
    if(sent>0)sn::Meter::instance().sent(uint64_t(sent));
}
void DonationEngine::account(sn::BufferCensus& census) const {
    sn::CpuScope snCpuScope;
    for(const auto* value:{&daStatus_,&dpStatus_,&daSignature_,&dpSignature_,&daSocketToken_,&daChannel_})census.add(*value);
    for(const auto& value:dpBaseline_)census.add(value);
    for(const auto& value:openJobs_)census.add(value);
    sender_.account(census);sessionStats_.account(census);
    for(auto* reply:http_.findChildren<QNetworkReply*>())sn::accountReply(census,reply);
}

