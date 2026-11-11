#include "net.hpp"
#include "resources.hpp"
#include "discord_payload.hpp"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QSslSocket>
#include <algorithm>
#include <memory>

Sender::Sender(QObject* parent) : QObject(parent) {}

void Sender::send(Channel channel, const Snapshot& s, const QString& message, Completion done)
{
    sn::CpuScope snCpuScope;
    if(!QSslSocket::supportsSsl()) {
        done({Outcome::Failed,"HTTPS unavailable: Qt TLS module missing. Reinstall the complete OZK Notify folder.",0});
        return;
    }
    tasks_.erase(std::remove_if(tasks_.begin(),tasks_.end(),[](const auto& item){ sn::CpuScope snCallbackScope; return item.expired();}),tasks_.end());
    auto task = std::make_shared<Task>();tasks_.push_back(task);
    task->channel = channel;
    task->s = s;
    task->message = message;
    task->done = std::move(done);
    task->epoch = epoch_;
    start(task);
}

void Sender::start(const std::shared_ptr<Task>& task)
{
    sn::CpuScope snCpuScope;
    if (task->epoch != epoch_) { task->done({Outcome::Cancelled, "Cancelled", task->attempts}); return; }
    ++task->attempts;
    QUrl url;
    QJsonObject payload;
    if (task->channel == Channel::Telegram) {
        // Token is only used in the official Telegram HTTPS endpoint.
        url = QUrl("https://api.telegram.org/bot" + task->s.token + "/sendMessage");
        payload["chat_id"] = task->s.chat.trimmed();
        payload["text"] = task->message;
        payload["disable_web_page_preview"] = false;
    } else {
        if (!validDiscord(task->s.webhook)) {
            task->done({Outcome::NotConfigured, "Invalid Discord webhook", task->attempts});
            return;
        }
        url = QUrl(task->s.webhook.trimmed());
        QUrlQuery q(url);
        q.removeAllQueryItems("wait");
        q.addQueryItem("wait", "true");
        url.setQuery(q);
        const auto discord=discordPayload(task->message,task->s.discordEveryone);
        if(!discord) {
            task->done({Outcome::Failed,"Discord message exceeds 2000 characters including @everyone",task->attempts});
            return;
        }
        payload=*discord;
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    req.setHeader(QNetworkRequest::UserAgentHeader, "StreamNotify/1.0 (OBS plugin)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    // A live stream can saturate the uplink. Wait longer for Discord's
    // confirmation; do not replay an ambiguous POST because it may duplicate
    // an announcement that Discord already accepted.
    req.setTransferTimeout(task->channel == Channel::Discord ? 30000 : 10000);
    task->body=QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = sn::watchReply(nam_.post(req,task->body),task->body);
    active_.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, task, reply] { sn::CpuScope snCallbackScope; 
        active_.remove(reply);
        if (task->epoch != epoch_) { reply->deleteLater(); return; }
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError net = reply->error();
        const QByteArray raw = reply->readAll();
        const QJsonDocument data = QJsonDocument::fromJson(raw);
        const QJsonObject body = data.isObject() ? data.object() : QJsonObject{};
        const QByteArray retryHeader = reply->rawHeader("Retry-After");
        // Remove secrets and URLs before exposing a transport failure reason.
        QString networkReason=reply->errorString();
        if(!task->s.token.isEmpty())networkReason.replace(task->s.token,QStringLiteral("[hidden]"));
        if(!task->s.webhook.isEmpty())networkReason.replace(task->s.webhook,QStringLiteral("[hidden]"));
        networkReason.replace(QRegularExpression("https?://[^\\s\"<>]+"),QStringLiteral("[URL hidden]"));
        networkReason=networkReason.left(240);
        const int errorCode = int(net);
        reply->deleteLater();

        if (http == 429 && task->attempts < 3) {
            // The server explicitly rejected the request; a bounded retry is safe.
            double seconds = body.value("retry_after").toDouble(0);
            if (seconds <= 0) {
                bool valid = false;
                seconds = retryHeader.toDouble(&valid);
                if (!valid || seconds <= 0) seconds = 2;
            }
            const int delay = std::clamp(int(seconds * 1000), 1000, 15000);
            QTimer::singleShot(delay, this, [this, task] { sn::CpuScope snCallbackScope;  start(task); });
            return;
        }
        if (http == 429) { task->done({Outcome::Failed, "Rate limit (429)", task->attempts}); return; }
        if (http >= 300 && http < 400) { task->done({Outcome::Failed, "HTTP redirect blocked", task->attempts}); return; }
        if (http == 400 || http == 401 || http == 403 || http == 404) {
            task->done({Outcome::Failed, QString("HTTP %1: check credentials and permissions").arg(http), task->attempts});
            return;
        }
        if (http >= 400 && http < 500) { task->done({Outcome::Failed, QString("HTTP %1").arg(http), task->attempts}); return; }
        if (http >= 500) { task->done({Outcome::Unknown, QString("HTTP %1: delivery not confirmed").arg(http), task->attempts}); return; }
        if (net != QNetworkReply::NoError) {
            // Only DNS failure / refused TCP connection have a sufficiently clear
            // pre-delivery failure signal to allow a bounded retry.
            const bool neverConnected = (http == 0) && (net == QNetworkReply::HostNotFoundError || net == QNetworkReply::ConnectionRefusedError);
            if (neverConnected && task->attempts < 3) {
                QTimer::singleShot(1500 * task->attempts, this, [this, task] { sn::CpuScope snCallbackScope;  start(task); });
                return;
            }
            task->done({neverConnected ? Outcome::Failed : Outcome::Unknown,
                        neverConnected ? QString("Connection failed") : QString("Network error %1: %2. Check connection or VPN/proxy; delivery not confirmed.").arg(errorCode).arg(networkReason), task->attempts});
            return;
        }
        bool success = false;
        if (task->channel == Channel::Telegram)
            success = (http == 200 && body.value("ok").toBool() && body.value("result").toObject().contains("message_id"));
        else success = (http == 200 && body.contains("id"));
        task->done({success ? Outcome::Sent : Outcome::Unknown,
                    success ? QString("Confirmed") : QString("No verified message id"), task->attempts});
    });
}

void Sender::abortAll()
{
    sn::CpuScope snCpuScope;
    ++epoch_;
    const auto pending = active_.values();
    for (QNetworkReply* reply : pending) if (reply) reply->abort();
}

void Sender::account(sn::BufferCensus& census) const {
    sn::CpuScope snCpuScope;
    for(const auto& item:tasks_)if(auto task=item.lock()) {
        sn::account(census,task->s);census.add(task->message);census.add(task->body);
    }
    for(auto* reply:active_)sn::accountReply(census,reply);
}
