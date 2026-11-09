#pragma once
#include "settings.hpp"
#include "net.hpp"
#include "donation_summary.hpp"
#include "oauth_loopback.hpp"
#include <QObject>
#include <QMap>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QTimer>
#include <QSet>
#include <functional>

struct Donation {
    QString service, id, donor, message, currency;
    double amount = 0;
    bool isTest = false;
};

// Strict amount categorization; malformed values and unsupported currencies are ignored.
int donationLevel(const Donation& d, const DonationSettings& s);
QString donationMessage(const Donation& d, const DonationSettings& s, int level,
                        const QString& streamUrl);
Donation parseDonationAlerts(const QJsonObject& packet);
Donation parseDonatePay(const QJsonObject& packet);

// Crash-safe journal; each inbound event is claimed before outbound POST.
// An interrupted in-flight delivery stays 'unknown', never silently resent.
class DonationLedger {
public:
    explicit DonationLedger(const QString& path);
    bool load();
    bool contains(const QString& key) const;
    bool claim(const QString& key, const Donation& d, int level, bool telegram, bool discord);
    bool setStatus(const QString& key, const QString& channel, const QString& status);
    QStringList recent() const;
    QString lastError() const {return error_;}
private:
    bool persist();
    QString path_, error_;
    QMap<QString, QJsonObject> items_;
};

class DonationEngine : public QObject {
    Q_OBJECT
#ifdef STREAM_NOTIFY_UI_PREVIEW
    friend struct DockPreview;
#endif
public:
    explicit DonationEngine(SettingsStore* settings, QObject* parent = nullptr);
    void configure();
    void setLive(bool active);
    void stop();
    sn::DonationSummary sessionSummary() const{return sessionStats_.value();}
    QString daStatus() const { return daStatus_; }
    QString dpStatus() const { return dpStatus_; }
    QStringList journal() const { return ledger_.recent(); }
    void fetchRates(bool manual = false);
    QString ratesStatus() const;
    void checkDonationAlerts();
    void beginDonationAlertsLogin();
    void checkDonatePay();
    void account(sn::BufferCensus& census) const;
    void previewTest(int level); // Explicit test; no fake event is inserted into ledger.
Q_SIGNALS:
    void updated();
    void credentialsChanged();
private:
    void sendFrame(const QString& data);
    void daConnect();
    void daSubscribe(const QString& clientId);
    void daReceive(const QString& data);
    void daReconnect();
    void dpPoll();
    void dpReconnect();
    void onDonation(const Donation& d);
    void deliver(const QString& key, const Donation& d, int level);
    void state(QString& target, const QString& text);
    void clearConnections();
    void verifyDAReply(int status, const QByteArray& data);
    void verifyDPReply(int status, const QByteArray& data);
    SettingsStore* store_;
    Sender sender_;
    QNetworkAccessManager http_;
    QWebSocket websocket_;
    OAuthLoopback daLogin_;
    QPointer<QNetworkReply> daLoginReply_;
    quint64 daLoginEpoch_=0;
    QTimer daRetry_, dpTimer_, dpBridgeHeartbeat_, daBridgeHeartbeat_;
    DonationLedger ledger_;
    sn::DonationSession sessionStats_;
    bool live_ = false, closing_ = false, dpPrimed_ = false;
    int daBackoff_ = 0, dpBackoff_ = 0, dpRateLimitCount_ = 0;
    quint64 daEpoch_ = 0, dpEpoch_ = 0;
    QString daStatus_ = "Disabled", dpStatus_ = "Disabled";
    QString daSignature_, dpSignature_;
    QString daSocketToken_, daChannel_;
    QSet<QString> dpBaseline_;
    QSet<QString> openJobs_;
    bool resettingSocket_ = false;
    bool ratesAttempted_ = false, ratesLoading_ = false;
};

