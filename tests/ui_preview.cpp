#include "../src/dock.hpp"
#include <QApplication>
#include <QDateTime>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QThread>
#include <QFont>
#include <QScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QLabel>
#include <QPixmap>
#include <QLineEdit>
#include <iostream>
static int previewResult(int code){std::cerr<<"Notify UI result: "<<code<<"\n";return code;}
struct DockPreview {
    static bool personalTwitchClient(StreamDock& panel){
        auto& probe=panel.platforms_;const auto previous=probe.twitchToken_;const bool fromChat=probe.twitchFromChat_;
        probe.twitchFromChat_=true;probe.twitchToken_=QJsonObject{{"client_id","personal-twitch-test"}};
        bool valid=probe.twitchApiClient()=="personal-twitch-test";
        probe.twitchFromChat_=false;valid=valid&&probe.twitchApiClient()!="personal-twitch-test";
        probe.twitchFromChat_=true;probe.twitchToken_={};valid=valid&&!probe.twitchApiClient().isEmpty();
        probe.twitchToken_=previous;probe.twitchFromChat_=fromChat;return valid;
    }
    static bool freshKickSession(StreamDock& panel){
        auto& probe=panel.platforms_;probe.kickLocal_=true;probe.kickManualId_="manual:kick:old";probe.lastKickId_="old-id";
        probe.beginOutputSession();return !probe.kickLocal_&&probe.kickManualId_.isEmpty()&&probe.lastKickId_.isEmpty()&&probe.kick().phase==sn::LivePhase::Offline;
    }
    static void journalScene(StreamDock& panel){
        const auto time=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")+"  ";
        panel.store_.current.events.prepend(time+"MAIN_TELEGRAM: SENT | Confirmed");
        panel.store_.current.events.prepend(time+"MAIN_DISCORD: FAILED | HTTP 403: check credentials and permissions");
    }
    static bool confirmedLinks(StreamDock& panel){
        const auto previous=panel.platforms_.youtube_;const bool broadcasting=panel.broadcasting_;
        auto& live=panel.platforms_.youtube_;live.phase=sn::LivePhase::Live;live.confirmed=true;live.id="preview-video";live.url="https://www.youtube.com/watch?v=preview-video";live.checkedAt=QDateTime::currentDateTimeUtc();panel.broadcasting_=true;
        QString error;const auto fresh=panel.confirmedPreview(&error);
        bool valid=error.isEmpty()&&fresh.contains(live.url)&&!fresh.contains("{youtube");
        live.checkedAt=live.checkedAt.addSecs(-100);error.clear();valid=valid&&panel.confirmedPreview(&error).isEmpty()&&!error.isEmpty();
        live.confirmed=false;live.checkedAt=QDateTime::currentDateTimeUtc();error.clear();valid=valid&&panel.confirmedPreview(&error).isEmpty();
        panel.platforms_.youtube_=previous;panel.broadcasting_=broadcasting;return valid;
    }
    static bool singleFooter(StreamDock& panel){
        return panel.findChildren<QLabel*>("SessionTrafficFooter").isEmpty()&&
               panel.findChildren<QLabel*>("Credit").size()==1;
    }
    static void rebuild(StreamDock& panel){panel.rebuild();}
    struct ServiceScene {Settings settings;QString da,dp;};
    static ServiceScene connectedServices(StreamDock& panel){
        ServiceScene previous{panel.store_.current,panel.donation_.daStatus_,panel.donation_.dpStatus_};
        // UI-only fixture. No auth calls or real connectivity are simulated as proof.
        panel.donationConfigure_.stop();panel.store_.current.donations.enabled=true;
        panel.store_.current.donations.daEnabled=true;panel.store_.current.donations.dpEnabled=true;
        panel.donation_.daStatus_="Connected";panel.donation_.dpStatus_="API connected (HTTP polling)";
        panel.page_=StreamDock::Page::Donations;panel.donationPage_=StreamDock::DonationPage::Services;panel.rebuild();return previous;
    }
    static void deliveredHome(StreamDock& panel){
        panel.store_.current.lastTime=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        panel.store_.current.lastTelegram=QString::fromUtf8("✓ Отправлено");panel.store_.current.lastDiscord=QString::fromUtf8("✓ Отправлено");
        panel.page_=StreamDock::Page::Home;panel.rebuild();
    }
    static void restoreServices(StreamDock& panel,const ServiceScene& previous){
        panel.store_.current=previous.settings;panel.donation_.daStatus_=previous.da;panel.donation_.dpStatus_=previous.dp;
        panel.page_=StreamDock::Page::Donations;panel.donationPage_=StreamDock::DonationPage::Services;panel.rebuild();
    }
    static bool ignoresSavedAnnouncementAtStartup(StreamDock& panel) {
        const Settings original=panel.store_.current;
        panel.store_.current.lastTime="2026-10-24 16:00:24";
        panel.store_.current.lastTelegram=QString::fromUtf8("✓ Отправлено");
        panel.store_.current.lastDiscord=QString::fromUtf8("✓ Отправлено");
        panel.broadcasting_=false;
        panel.announcementAttempted_=false;
        panel.page_=StreamDock::Page::Home;
        panel.rebuild();
        QApplication::processEvents();
        bool waiting=false, staleSent=false;
        for(auto* label:panel.findChildren<QLabel*>()) {
            if(!label->property("announcementStatus").toBool())continue;
            waiting|=label->text()==QString::fromUtf8("Ожидание трансляции");
            staleSent|=label->text()==QString::fromUtf8("✓ Отправлен");
        }
        panel.store_.current=original;panel.rebuild();QApplication::processEvents();
        return waiting&&!staleSent;
    }
    static QVector<sn::OutputView> outputViews(const StreamDock& panel){return panel.monitor_.views();}
    static bool live(const StreamDock& panel){return panel.broadcasting_;}
    static quint64 session(const StreamDock& panel){return panel.generation_;}
    static void monitorDemo(StreamDock& panel) {
        std::cerr<<"Entering monitor demo\n";
        panel.telemetryTimer_.stop();panel.streamStateTimer_.stop();
        panel.monitor_.clear();
        streamPreviewOutputs.clear();
        StreamPreviewOutput youtube{true,true};youtube.name="Aitum YouTube";youtube.platform="YouTube";youtube.id=1;
        StreamPreviewOutput twitch{true,true};twitch.name="Aitum Twitch";twitch.platform="Twitch";twitch.id=2;
        StreamPreviewOutput kick{true,true};kick.name="Aitum Kick";kick.platform="Kick";kick.id=3;
        streamPreviewOutputs={youtube,twitch,kick};
        for(int64_t ms=0;ms<=30000;ms+=2000) {
            if(ms) {
                streamPreviewOutputs[0].bytes+=1500000;streamPreviewOutputs[0].frames+=120;
                streamPreviewOutputs[1].bytes+=ms>=12000?650000:1500000;streamPreviewOutputs[1].frames+=120;
                streamPreviewOutputs[2].bytes+=1250000;streamPreviewOutputs[2].frames+=120;
                if(ms==20000)streamPreviewOutputs[1].dropped=12;
            }
            panel.monitor_.sample(ms);
        }
        auto& donations=panel.store_.current.donations;
        const auto originalDonations=donations;
        donations.enabled=true;donations.daEnabled=true;donations.dpEnabled=false;
        donations.toTelegram=false;donations.toDiscord=false;
        panel.donation_.setLive(true);
        Donation first;first.service="DonationAlerts";first.id="ui-summary-first";first.amount=49;first.currency="RUB";
        Donation second=first;second.id="ui-summary-second";second.amount=50;
        panel.donation_.onDonation(first);panel.donation_.onDonation(second);panel.donation_.onDonation(first);
        Donation test=first;test.id="ui-summary-test";test.isTest=true;panel.donation_.onDonation(test);
        const auto summary=panel.donation_.sessionSummary();
        std::cerr<<"Monitor donation summary: "<<summary.count<<" events, "<<summary.rub<<" RUB\n";
        if(summary.count!=2 || summary.rub!=99)qFatal("Donation summary did not deduplicate below-threshold events");
        donations=originalDonations;
        panel.broadcasting_=true;panel.page_=StreamDock::Page::Home;panel.phase_=StreamDock::Phase::Idle;
        panel.rebuild();
        std::cerr<<"Monitor demo ready\n";
    }
    static void stopMonitorDemo(StreamDock& panel) {
        streamPreviewOutputs.clear();panel.monitor_.clear();panel.broadcasting_=false;panel.donation_.setLive(false);
        panel.telemetryTimer_.start();panel.streamStateTimer_.start();panel.rebuild();
    }
    static bool retainStoppedOutputs(StreamDock& panel) {
        streamPreviewOutputs.erase(streamPreviewOutputs.begin()); // Aitum can remove an active output between samples.
        panel.monitor_.sample(34000);
        streamPreviewOutputs[0].active=false;
        panel.monitor_.sample(36000);
        streamPreviewOutputs.clear(); // Twitch output disappears later.
        const auto views=panel.monitor_.sample(38000);
        bool youtube=false,twitch=false,kick=false;
        for(const auto& view:views) {
            if(view.platform=="YouTube")youtube=!view.stats.active&&view.stats.bytes>0;
            if(view.platform=="Twitch")twitch=!view.stats.active&&view.stats.bytes>0;
            if(view.platform=="Kick")kick=!view.stats.active&&view.stats.bytes>0;
        }
        if(!youtube||!twitch||!kick)return false;
        StreamPreviewOutput restarted{true,true};restarted.id=4;restarted.name="New YouTube";restarted.platform="YouTube";
        restarted.bytes=1000;restarted.frames=1;
        streamPreviewOutputs={restarted};
        const auto restartedViews=panel.monitor_.sample(40000);
        int youtubeRows=0;bool fresh=false,oldTwitch=false;
        for(const auto& view:restartedViews){
            if(view.platform=="YouTube"){++youtubeRows;fresh=view.stats.active&&view.stats.bytes==1000;}
            if(view.platform=="Twitch")oldTwitch=!view.stats.active&&view.stats.bytes>0;
        }
        return youtubeRows==1&&fresh&&oldTwitch;
    }
    static Settings demoConnections(StreamDock& panel) {
        const Settings original=panel.store_.current;
        auto& s=panel.store_.current;
        s.telegramEnabled=true;s.discordEnabled=true;
        s.telegramToken="0:PREVIEW_ONLY_NOT_A_REAL_TOKEN";
        s.telegramChat="@preview_channel";
        s.discordWebhook="https://discord.com/api/webhooks/0/PREVIEW_ONLY_NOT_A_REAL_WEBHOOK";
        s.youtubeUrl="https://www.youtube.com/@example/live";
        s.twitchUrl="https://www.twitch.tv/example";
        panel.page_=StreamDock::Page::Home;panel.phase_=StreamDock::Phase::Idle;
        panel.rebuild();return original;
    }
    static void restoreSettings(StreamDock& panel,const Settings& settings) {
        panel.store_.current=settings;panel.rebuild();
    }
    static bool donationIdleStates(StreamDock& panel) {
        const auto original=panel.store_.current.donations;
        auto& s=panel.store_.current.donations;
        s.enabled=true;s.daEnabled=true;s.dpEnabled=true;s.onlyLive=true;s.legacyApiOptIn=true;
        s.daToken.clear();s.dpToken.clear();
        panel.donation_.setLive(false);panel.donation_.configure();
        const bool missing=panel.donation_.daStatus()=="Not configured" && panel.donation_.dpStatus()=="Not configured";
        // onlyLive blocks external service connections; these are dummy values.
        s.daToken="PREVIEW_ONLY";s.dpToken="PREVIEW_ONLY";panel.donation_.configure();
        const bool waiting=panel.donation_.daStatus()=="Waiting for stream" && panel.donation_.dpStatus()=="Waiting for stream";
        s.enabled=false;panel.donation_.configure();
        const bool disabled=panel.donation_.daStatus()=="Module disabled" && panel.donation_.dpStatus()=="Module disabled";
        panel.store_.current.donations=original;panel.donation_.configure();
        return missing && waiting && disabled;
    }
    static void countdown(StreamDock& panel,int seconds=60) {
        panel.session_.delay=seconds;panel.session_.resultSeconds=45;
        panel.session_.telegram=true;panel.session_.discord=true;
        panel.elapsed_.start();panel.phase_=StreamDock::Phase::Counting;panel.rebuild();
    }
    static void result(StreamDock& panel) {
        panel.telegram_={Outcome::Sent,QString("Confirmed"),1};
        panel.discord_={Outcome::Sent,QString("Confirmed"),1};
        panel.phase_=StreamDock::Phase::Result;
        panel.resultTimer_.start(45000);panel.rebuild();
    }
};
static void pump() {
    for(int i=0;i<25;++i){QApplication::processEvents();QThread::msleep(2);}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");app.setFont(QFont("Segoe UI",9));
    for(const char* asset : {":/stream-notify/brands/donationalerts.png",":/stream-notify/brands/donatepay.png"}) {
        if(QPixmap(QString::fromLatin1(asset)).isNull()) {
            std::cerr<<"Missing or invalid embedded service logo: "<<asset<<"\n";return previewResult(21);
        }
    }
    std::cout<<"Embedded DonationAlerts and DonatePay artwork decoded successfully\n";
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QString("ui-previews");
    QDir().mkpath(output);
    const int width=argc>2?QString::fromLocal8Bit(argv[2]).toInt():290;
    const int height=argc>3?QString::fromLocal8Bit(argv[3]).toInt():900;
    StreamDock panel;panel.resize(width,height);panel.show();pump();
    if(panel.width()!=width){std::cerr<<"Unexpected dock width: "<<panel.width()<<" instead of "<<width<<"\n";return previewResult(62);}
    auto* headerBrand=panel.findChild<QWidget*>("HeaderBrand");
    auto* headerManual=panel.findChild<QPushButton*>("ManualMessageShortcut");
    auto* headerSettings=panel.findChild<QPushButton*>("SettingsShortcut");
    const auto dimensions=[](QWidget* widget){return widget?QString::number(widget->width())+"x"+QString::number(widget->height()):"missing";};
    std::cerr<<"Header: brand="<<dimensions(headerBrand).toStdString()
             <<" y="<<(headerBrand?headerBrand->y():-1)
             <<" manual="<<dimensions(headerManual).toStdString()
             <<" y="<<(headerManual?headerManual->y():-1)
             <<" gear="<<dimensions(headerSettings).toStdString()
             <<" y="<<(headerSettings?headerSettings->y():-1)
             <<" icon="<<(headerSettings?headerSettings->iconSize().width():-1)<<"x"<<(headerSettings?headerSettings->iconSize().height():-1)
             <<" spacing="<<panel.layout()->spacing()<<'\n';
    if(!headerBrand||!headerManual||!headerSettings||panel.layout()->spacing()!=5||
       headerBrand->size()!=QSize(28,28)||headerManual->size()!=QSize(30,28)||
       headerSettings->size()!=QSize(30,28)||headerSettings->iconSize()!=QSize(19,19)||
       qAbs(headerBrand->y()-headerSettings->y())>3){std::cerr<<"Header geometry check failed\n";return previewResult(58);}
    if(!DockPreview::ignoresSavedAnnouncementAtStartup(panel)){std::cerr<<"Stale announcement check failed\n";return previewResult(43);}
    auto click=[&](const QString& text) {
        for(auto* button:panel.findChildren<QPushButton*>())
            if(button->text().contains(text) && button->isEnabled()){button->click();pump();return true;}
        std::cerr<<"Missing UI navigation control: "<<text.toStdString()<<"\n";return false;
    };
    auto save=[&](const QString& name){
        pump();
        for(auto* label:panel.findChildren<QLabel*>())
            if(label->text().contains(QString::fromUtf8("Сохранено")) || label->text().contains(QString::fromUtf8("Автосохранение"))){
                std::cerr<<"Unexpected save status in "<<name.toStdString()<<"\n";return false;
            }
        const bool saved=panel.grab().save(output+"/"+name+".png");
        if(!saved)std::cerr<<"Cannot save preview "<<name.toStdString()<<"\n";
        return saved;
    };
    if(!save("01-home"))return previewResult(1);
    if(!DockPreview::confirmedLinks(panel))return previewResult(62);
    if(!DockPreview::personalTwitchClient(panel))return previewResult(75);
    if(!DockPreview::freshKickSession(panel))return previewResult(77);
    if(sn::platformFor("Kick output","", "rtmps://example.global-contribute.live-video.net:443/app")!="Kick"||sn::platformFor("Other output","", "rtmps://example.global-contribute.live-video.net:443/app")!="RTMP")return previewResult(76);
    DockPreview::journalScene(panel);
    if(!click(QString::fromUtf8("Журнал"))||!save("22-delivery-log"))return previewResult(57);
    if(!click(QString::fromUtf8("Назад")))return previewResult(58);
    if(!click(QString::fromUtf8("Просмотр анонса"))||!save("23-announcement-preview"))return previewResult(59);
    auto* confirmedPreview=panel.findChild<QTextEdit*>("ConfirmedAnnouncementText");
    if(!confirmedPreview||confirmedPreview->toPlainText().isEmpty())return previewResult(60);
    if(!click(QString::fromUtf8("Назад")))return previewResult(61);
    auto* settingsShortcut=panel.findChild<QPushButton*>("SettingsShortcut");
    if(!settingsShortcut)return previewResult(54);
    settingsShortcut->click();pump();
    bool foundConnect=false,foundMessage=false,foundInterface=false,foundDonations=false;
    for(auto* button:panel.findChildren<QPushButton*>()) {
        foundConnect|=button->text()==QString::fromUtf8("Связь");
        foundMessage|=button->text()==QString::fromUtf8("Сообщение");
        foundInterface|=button->text()==QString::fromUtf8("Интерфейс");
        foundDonations|=button->text()==QString::fromUtf8("Донаты");
    }
    if(!foundConnect||!foundMessage||!foundInterface||!foundDonations)return previewResult(55);
    settingsShortcut=panel.findChild<QPushButton*>("SettingsShortcut");
    settingsShortcut->click();pump();
    if(panel.findChild<QPushButton*>("SettingsShortcut")==nullptr)return previewResult(56);
    const QSize homeSize=panel.size();
    auto* manualShortcut=panel.findChild<QPushButton*>("ManualMessageShortcut");
    if(!manualShortcut)return previewResult(44);
    manualShortcut->click();pump();
    auto* manualEditor=panel.findChild<QTextEdit*>("ManualMessageEditor");
    if(!manualEditor || !panel.findChild<QCheckBox*>("ManualTelegram") ||
       !panel.findChild<QCheckBox*>("ManualDiscord") ||
       !panel.findChild<QCheckBox*>("ManualEveryone") || panel.size()!=homeSize)return previewResult(45);
    if(!save("18-manual-message"))return previewResult(49);
    manualEditor->setPlainText(QString::fromUtf8("Анонс и ссылка: https://example.com"));pump();
    auto* manualBack=panel.findChild<QPushButton*>("ManualMessageBack");
    if(!manualBack)return previewResult(46);
    manualBack->click();pump();
    manualShortcut=panel.findChild<QPushButton*>("ManualMessageShortcut");
    if(!manualShortcut)return previewResult(47);
    manualShortcut->click();pump();
    manualEditor=panel.findChild<QTextEdit*>("ManualMessageEditor");
    if(!manualEditor || !manualEditor->toPlainText().contains("https://example.com"))return previewResult(48);
    manualBack=panel.findChild<QPushButton*>("ManualMessageBack");manualBack->click();pump();
    // Home offers channel setup; stream URLs remain editable in Connections.
    if(!click(QString::fromUtf8("Настроить")) || !save("02-connections"))return previewResult(31);
    if(!panel.findChild<QLineEdit*>("TelegramToken") ||
       !panel.findChild<QLineEdit*>("YouTubeUrl") || !panel.findChild<QLineEdit*>("TwitchUrl")||!panel.findChild<QLineEdit*>("KickUrl"))return previewResult(32);
    if(panel.findChild<QWidget*>("PluginResources") || panel.findChild<QWidget*>("OutputMonitor"))return previewResult(42);
    if(!click(QString::fromUtf8("Сообщение")) || !save("03-message"))return previewResult(1);
    if(!click(QString::fromUtf8("Интерфейс")) || !click(QString("English")))return previewResult(1);
    if(!click(QString::fromUtf8("Русский")) || !save("04-interface"))return previewResult(1);
    // Exercise the input-event rebuild path that previously crashed OBS.
    for(int i=0;i<2;++i) {
        bool found=false;
        for(auto* toggle:panel.findChildren<QCheckBox*>())
            if(toggle->text().contains(QString::fromUtf8("Компактный"))){toggle->click();found=true;break;}
        pump();if(!found)return previewResult(1);
    }
    if(!click(QString::fromUtf8("Донаты")) || !save("05-donations"))return previewResult(1);
    if(!click(QString::fromUtf8("Связь")) || !save("06-connections-return"))return previewResult(1);
    if(!click(QString::fromUtf8("Донаты")))return previewResult(1);
    const auto serviceScene=DockPreview::connectedServices(panel);pump();
    if(!save("20-services-connected"))return previewResult(60);
    DockPreview::deliveredHome(panel);pump();
    if(!save("21-home-delivered"))return previewResult(61);
    DockPreview::restoreServices(panel,serviceScene);pump();
    const QSize dockSize=panel.size();
    auto donationTab=[&](int section) {
        for(auto* button:panel.findChildren<QPushButton*>()) {
            const QVariant value=button->property("donationSection");
            if(value.isValid() && value.toInt()==section) {button->click();pump();return panel.size()==dockSize;}
        }
        return false;
    };
    if(!donationTab(1)||!save("10-donations-send"))return previewResult(10);
    if(!donationTab(2)||!save("11-donations-amounts"))return previewResult(11);
    const auto amounts=panel.findChildren<QDoubleSpinBox*>();
    if(amounts.size()!=3)return previewResult(12);
    amounts[0]->setValue(99);pump();
    if(!donationTab(3))return previewResult(13);
    bool correctPreview=false;
    for(auto* label:panel.findChildren<QLabel*>())
        if(label->text().contains("99 RUB"))correctPreview=true;
    if(!correctPreview)return previewResult(14);
    auto* editor=panel.findChild<QTextEdit*>();
    if(!editor)return previewResult(15);
    const QString original=editor->toPlainText();
    editor->setPlainText(original+" UI-PERSIST-CHECK");pump();
    if(!donationTab(0)||!donationTab(3))return previewResult(16);
    editor=panel.findChild<QTextEdit*>();
    if(!editor||!editor->toPlainText().endsWith(" UI-PERSIST-CHECK"))return previewResult(17);
    editor->setPlainText(original);
    if(!donationTab(2))return previewResult(18);
    const auto restoredAmounts=panel.findChildren<QDoubleSpinBox*>();
    if(restoredAmounts.isEmpty()||restoredAmounts[0]->value()!=99)return previewResult(19);
    restoredAmounts[0]->setValue(100);
    if(!donationTab(3)||!save("12-donations-text"))return previewResult(20);
    std::cout<<"Donation subtabs: stable dock size, edited amounts and template persistence passed\\n";
    for(auto* scroll:panel.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    if(!save("07-donations-bottom"))return previewResult(1);
    if(!DockPreview::donationIdleStates(panel))return previewResult(33);
    const Settings originalSettings=DockPreview::demoConnections(panel);pump();
    manualShortcut=panel.findChild<QPushButton*>("ManualMessageShortcut");
    if(!manualShortcut)return previewResult(50);
    manualShortcut->click();pump();
    auto* mention=panel.findChild<QCheckBox*>("ManualEveryone");
    if(!mention || !mention->isEnabled())return previewResult(51);
    mention->setChecked(true);pump();
    if(!save("19-manual-ready"))return previewResult(52);
    manualBack=panel.findChild<QPushButton*>("ManualMessageBack");
    if(!manualBack)return previewResult(53);
    manualBack->click();pump();
    QList<QPointer<QTimer>> readyTimers;
    for(auto* timer:panel.findChildren<QTimer*>())
        if(timer->objectName()=="ReadyBlinkTimer")readyTimers.append(timer);
    if(readyTimers.size()!=2)return previewResult(34);
    for(const auto& timer:readyTimers)if(!timer || !timer->isActive() || timer->interval()!=2770)return previewResult(35);
    if(!save("15-home-ready"))return previewResult(1);
    panel.hide();pump();for(const auto& timer:readyTimers)if(timer->isActive())return previewResult(36);
    panel.show();pump();for(const auto& timer:readyTimers)if(!timer->isActive())return previewResult(37);
    DockPreview::restoreSettings(panel,originalSettings);pump();
    for(const auto& timer:readyTimers)if(timer)return previewResult(38);
    std::cout<<"Setup links, donation idle reasons and visible-only ready indicators passed\\n";
    std::cerr<<"Starting monitor stage\n";
    const Settings monitorSettings=DockPreview::demoConnections(panel);pump();
    std::cerr<<"Monitor connections ready\n";
    DockPreview::monitorDemo(panel);pump();
    std::cerr<<"Monitor UI pumped\n";
    std::cerr<<"Monitor widgets: output="<<(panel.findChild<QWidget*>("OutputMonitor")!=nullptr)
        <<" resources="<<(panel.findChild<QWidget*>("PluginResources")!=nullptr)<<"\n";
    if(!panel.findChild<QWidget*>("OutputMonitor") || panel.findChild<QWidget*>("PluginResources"))return previewResult(40);
    const auto outputs=DockPreview::outputViews(panel);
    std::cerr<<"Monitor views: "<<outputs.size()<<"\n";
    if(outputs.size()!=3 || outputs[0].stats.recentProblem || !outputs[1].stats.recentProblem||outputs[2].platform!="Kick"||outputs[2].stats.recentProblem){
        std::cerr<<"Unexpected monitor demo outputs: "<<outputs.size()<<"\n";
        for(const auto& output:outputs)
            std::cerr<<output.platform.toStdString()<<" bytes="<<output.stats.bytes
                <<" recentProblem="<<output.stats.recentProblem<<"\n";
        return previewResult(41);
    }
    if(!save("16-monitoring-home"))return previewResult(1);
    for(auto* scroll:panel.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    if(!save("17-monitoring-bottom"))return previewResult(1);
    const bool retained=DockPreview::retainStoppedOutputs(panel);
    std::cerr<<"Stopped output histories retained="<<retained<<"\n";
    if(!retained)return previewResult(54);
    DockPreview::stopMonitorDemo(panel);pump();
    DockPreview::restoreSettings(panel,monitorSettings);pump();
    DockPreview::countdown(panel);if(!save("08-countdown"))return previewResult(1);
    QPointer<QTimer> pulse=panel.findChild<QTimer*>("CountdownPulseTimer");
    if(!pulse || pulse->isActive())return previewResult(22); // No animation outside the final ten seconds.
    DockPreview::countdown(panel,10);pump();
    pulse=panel.findChild<QTimer*>("CountdownPulseTimer");
    if(!pulse || !pulse->isActive())return previewResult(23);
    const QSize pulseSize=panel.size();
    if(!save("13-countdown-last10"))return previewResult(1);
    const QImage beforePulse=panel.grab().toImage();
    bool pulseChanged=false;
    for(int i=0;i<30 && !pulseChanged;++i){
        pump();
        pulseChanged=(beforePulse!=panel.grab().toImage());
    }
    if(!pulseChanged || panel.size()!=pulseSize)return previewResult(24);
    if(!save("14-countdown-pulse"))return previewResult(1);
    panel.hide();pump();if(pulse->isActive())return previewResult(25);
    panel.show();pump();if(!pulse->isActive())return previewResult(26);
    DockPreview::result(panel);if(!save("09-result"))return previewResult(1);
    if(pulse)return previewResult(27); // The ring and its timer must be destroyed on phase change.
    std::cout<<"Countdown pulse: final-ten-seconds only, visible frames, stable layout, hide/resume and disposal passed\\n";
    streamPreviewOutputs={{false,true}}; // Recording must not announce.
    panel.refreshStreamingState();pump();if(DockPreview::live(panel))return previewResult(2);
    const auto before=DockPreview::session(panel);
    streamPreviewOutputs.push_back({true,true}); // First Aitum destination.
    panel.refreshStreamingState();pump();
    if(!DockPreview::live(panel)||DockPreview::session(panel)!=before+1)return previewResult(3);
    streamPreviewOutputs.push_back({true,true}); // Second Aitum destination.
    panel.refreshStreamingState();pump();
    if(DockPreview::session(panel)!=before+1)return previewResult(4);
    streamPreviewOutputs[1].active=false;
    panel.refreshStreamingState();pump();if(!DockPreview::live(panel))return previewResult(5);
    streamPreviewOutputs[2].active=false;
    panel.refreshStreamingState();pump();if(DockPreview::live(panel))return previewResult(6);
    streamPreviewOutputs[1].active=true;
    panel.refreshStreamingState();pump();
    if(!DockPreview::live(panel)||DockPreview::session(panel)!=before+2)return previewResult(7);
    streamPreviewOutputs.clear();panel.refreshStreamingState();pump();
    std::cout<<"Multistream aggregate start, deduplication, last-stop and restart tests passed\\n";
    for(int i=0;i<12;++i){
        DockPreview::rebuild(panel);pump();
        if(!DockPreview::singleFooter(panel))return previewResult(44);
    }
    std::cout<<"Footer survives repeated dock rebuilds without duplicate labels\n";
    panel.shutdown();
    std::cout<<"Dock navigation, language and compact-mode smoke tests passed\n";
}

