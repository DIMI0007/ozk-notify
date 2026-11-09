#include "../src/donations.hpp"
#include "../src/donatepay_reply.hpp"
#include "../src/donation_bridge.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <cstdlib>
#include <iostream>

static void require(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
struct DockPreview {static void receive(DonationEngine& engine,const Donation& donation){engine.onDonation(donation);}};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    ozk_bridge::publishDPProvider("notify-test-account");
    require(ozk_bridge::hasDPProvider("notify-test-account"),"DonatePay provider lease missing");
    require(!ozk_bridge::hasDPProvider("different-account"),"Provider lease crossed accounts");
    ozk_bridge::clearDPProvider("notify-test-account");
    require(!ozk_bridge::hasDPProvider("notify-test-account"),"Provider lease survived shutdown");
    const auto rejected=sn::parseDonatePayReply(200,R"({"status":"error","message":"Incorrect token"})");
    require(!rejected.ok&&rejected.permanent&&rejected.status=="Incorrect DonatePay API key","HTTP-200 token rejection misclassified");
    require(sn::parseDonatePayReply(429,"{}").rateLimited,"HTTP 429 not rate limited");
    require(sn::parseDonatePayReply(200,R"({"status":429})").rateLimited,"Envelope 429 not rate limited");
    for(int code:{401,403})require(sn::parseDonatePayReply(code,"{}").permanent,"Unauthorized request retried");
    const auto html=sn::parseDonatePayReply(200,"<html>not API</html>");
    require(!html.ok&&!html.permanent,"Temporary proxy response must recover");
    const auto temporary=sn::parseDonatePayReply(200,R"({"status":"error","message":"Temporary error"})");
    require(!temporary.ok&&!temporary.permanent,"Temporary API error must recover");
    const auto valid=sn::parseDonatePayReply(200,R"({"status":"success","data":[{"id":123,"what":"Fixture","sum":"112.21","currency":"RUB","type":"donation","status":"success","comment":"hello"}]})");
    require(valid.ok&&valid.rows.size()==1,"Official DonatePay transaction envelope rejected");
    const auto donation=parseDonatePay(valid.rows[0].toObject());
    require(donation.id=="123"&&donation.amount==112.21&&donation.currency=="RUB"&&!donation.isTest,"Transaction fields not preserved");
    auto test=valid.rows[0].toObject();test["status"]="user";require(parseDonatePay(test).isTest,"DonatePay manual event counted as real");
    test=valid.rows[0].toObject();test.remove("currency");require(parseDonatePay(test).currency.isEmpty(),"Currency inferred without evidence");
    QJsonObject da{{"id",9},{"amount","150"},{"currency","RUB"},{"username","Fixture"},{"is_test","1"}};
    require(parseDonationAlerts(da).isTest,"String test flag counted as real");da["is_test"]=false;
    require(!parseDonationAlerts(da).isTest,"Real DonationAlerts event rejected");
    DonationSettings thresholds;thresholds.thresholds["USD"]={2,10,50};Donation usd;usd.service="Fixture";usd.id="usd";usd.currency="USD";usd.amount=10;
    require(donationLevel(usd,thresholds)==1,"Explicit foreign currency thresholds ignored");
    QTemporaryDir directory;require(directory.isValid(),"Temp directory unavailable");
    qputenv("STREAM_NOTIFY_PREVIEW_DIR",directory.path().toUtf8());SettingsStore store;
    const QString mainTemplate="Live!\nYouTube: {youtube_url}\nTwitch: {twitch_url}\n{stream_url}";
    QString problem;
    const QString twitchOnly=renderAnnouncement(mainTemplate,{},"https://www.twitch.tv/test",{},&problem);
    require(problem.isEmpty()&&twitchOnly.contains("https://www.twitch.tv/test")&&
            !twitchOnly.contains("YouTube:")&&!twitchOnly.contains("{stream_url}"),"Twitch-only link rendering failed");
    const QString youtubeOnly=renderAnnouncement(mainTemplate,"https://www.youtube.com/watch?v=abc",{}, {},&problem);
    require(problem.isEmpty()&&youtubeOnly.contains("https://www.youtube.com/watch?v=abc")&&
            !youtubeOnly.contains("Twitch:"),"YouTube-only link rendering failed");
    const QString both=renderAnnouncement(mainTemplate,"https://www.youtube.com/watch?v=abc","https://www.twitch.tv/test",{},&problem);
    require(problem.isEmpty()&&both.contains("YouTube:")&&both.contains("Twitch:"),"Two-platform link rendering failed");
    const auto kickOnly=renderAnnouncement("Live!\nYouTube: {youtube}\nTwitch: {twitch}\nKick: {kick}",{},{},"Kick",&problem,"https://kick.com/example");
    require(problem.isEmpty()&&kickOnly.contains("Kick: https://kick.com/example")&&!kickOnly.contains("YouTube:")&&!kickOnly.contains("Twitch:"),"Kick-only announcement failed");
    const auto allThree=renderAnnouncement("{stream_url}","https://youtube.com/watch?v=abc","https://twitch.tv/example",{},&problem,"https://kick.com/example");
    require(problem.isEmpty()&&allThree.split('\n').size()==3&&allThree.contains("kick.com/example"),"Three-platform announcement lost a link");
    store.current.deliveryJournal["main|youtube|id|telegram"]="sent";
    require(store.save(),"Announcement delivery journal not saved");
    SettingsStore restored;require(restored.load()&&restored.current.deliveryJournal.value("main|youtube|id|telegram")=="sent",
                                   "Announcement delivery journal lost on restart");
    store.current.message="background-save-fixture";store.scheduleSave();
    QEventLoop wait;QTimer::singleShot(1200,&wait,&QEventLoop::quit);wait.exec();store.flush();
    SettingsStore backgroundRestored;require(backgroundRestored.load()&&backgroundRestored.current.message=="background-save-fixture",
        "Background settings snapshot lost or shutdown flush incomplete");
    store.current.donations.enabled=true;store.current.donations.onlyLive=false;
    store.current.donations.toTelegram=false;store.current.donations.toDiscord=false;
    Donation below=donation;below.id="below-threshold";below.amount=30;
    {DonationEngine engine(&store);engine.setLive(true);DockPreview::receive(engine,below);DockPreview::receive(engine,below);
     require(engine.sessionSummary().count==1,"Below-threshold deduplication failed");}
    {DonationEngine engine(&store);engine.setLive(true);DockPreview::receive(engine,below);
     require(engine.sessionSummary().count==0,"Event replayed after engine restart");}
    const auto path=directory.filePath("ledger.json");
    {DonationLedger ledger(path);require(ledger.load()&&ledger.claim("DonatePay:123",donation,0,true,true),"Ledger claim failed");
     require(ledger.setStatus("DonatePay:123","telegram","inflight"),"Inflight state not saved");}
    {DonationLedger ledger(path);require(ledger.load()&&ledger.contains("DonatePay:123"),"Persisted ID lost on restart");
     require(!ledger.claim("DonatePay:123",donation,0,true,true),"Duplicate donation claimed after restart");
     require(ledger.recent().join('\n').contains("unknown")&&ledger.recent().join('\n').contains("not_sent_after_restart"),"Interrupted delivery states not recovered safely");}
    std::cout<<"Donation protocols: official envelope, errors, real/test flags, thresholds and persistent replay protection passed\n";
}

