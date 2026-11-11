#include "dock.hpp"
#include "version.hpp"
#include "stream_activity.hpp"
#include "ui_widgets.hpp"
#include "telemetry_widgets.hpp"
#include "resources.hpp"
#include "telegram_channel.hpp"
#include "discord_payload.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QScrollArea>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QUrl>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDateTime>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QFont>
#include <QMap>
#include <QProgressBar>
#include <QTextBrowser>
#include <cmath>
#include <algorithm>

namespace {
QString announcementKey(const QString& type,const QString& platform,const QString& id,Channel channel){
    const auto digest=QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex();
    return type+"|"+platform+"|"+QString::fromLatin1(digest.constData())+"|"+
        (channel==Channel::Telegram?"telegram":"discord");
}
const char* css = R"(
QWidget { font-size: 12px; }
QWidget#PageBody, QWidget#ScrollViewport { background:#080f18; }
QPushButton:disabled { color:#6c7890;border-color:#273246; }
QWidget#StreamRoot { background: #080f18; color: #dfe8ff; }
QWidget#WaitingCard { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #211e16,stop:1 #0c151e);border:1px solid #51452b;border-radius:8px; }
QWidget#WaitingCard:hover { border-color:#695937; }
QWidget#Card { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #101925,stop:1 #0b131e); border: 1px solid #263244; border-radius: 8px; }
QWidget#Card:hover { border-color:#38516d; }
QLabel { color: #dfe8ff; background: transparent; }
QLabel#Hint { color:#8ea1c0; font-size:11px; }
QLabel#Title { color:#e7fff4; font-weight:700; font-size:17px; }
QLabel#Section { color:#edf3ff; font-size:13px; font-weight:700; }
QLabel#Waiting { color:#ffdc45; font-weight:700; }
QLabel#Good { color: #07df91; font-weight: 700; }
QLabel#Bad { color: #ff8484; font-weight: 700; }
QLabel#Count { color: #59dfff; font-weight: 750; font-size: 57px; }
QPushButton { color: #e1ebff; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b); border: 1px solid #39465d; border-radius: 8px; min-height: 20px; padding: 3px 7px; }
QPushButton#BackShortcut,QPushButton#ManualMessageShortcut,QPushButton#SettingsShortcut { border-radius:8px;padding:0;min-height:0;min-width:0; }
QPushButton:hover { background:#26344d;border-color:#6476a0; }
QPushButton:focus { border-color:#8a79ff; }
QPushButton:pressed { background:#111b30; }
QPushButton#ActiveNav { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #18203c,stop:1 #111e50); border: 1px solid #303e6c; border-bottom: 2px solid #6547ff; font-weight: 700; }
QPushButton#DonationTab {font-size:11px;min-height:18px;padding:3px 4px;background:#0d1824;border:0;border-bottom:2px solid #243246;border-radius:4px;}
QPushButton#DonationTab[selected="true"] {color:#b8ffe1;background:#102c29;border-bottom-color:#00d58a;}
QPushButton#Cancel { background: #16395b; border-color: #40b7ff; font-weight: 700; min-height: 28px; }
QPushButton#ManualMessageBack { background:transparent; border:0; color:#91a9c8; text-align:left; padding:2px 4px; }
QPushButton#ManualMessageBack:hover { color:#e7f3ff; background:#18283b; }
QLabel#ManualTitle { color:#f1f7ff; font-size:16px; font-weight:800; }
QLabel#ManualEyebrow { color:#4ce9b1; font-size:10px; font-weight:750; }
QWidget#ManualRecipient { background:#111f2d; border:1px solid #304a5d; border-radius:8px; }
QTextEdit#ManualMessageEditor { background:#091723; border:1px solid #3a6071; border-radius:8px; padding:8px; font-size:13px; }
QTextEdit#ManualMessageEditor:focus { border:1px solid #35cfad; background:#0c1b29; }
QPushButton#ManualMessageSend { color:#061c20; font-weight:800; min-height:30px; background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #51efaa,stop:1 #34cce9); border:1px solid #7fffd0; }
QPushButton#ManualMessageSend:hover { background:#8af9d0; border-color:#c7ffea; }
QPushButton#ManualMessageSend:disabled { color:#71879b; background:#1a2b39; border:1px solid #314251; }
QLineEdit,QTextEdit,QComboBox,QDoubleSpinBox { color: #e7eeff; background: #0c1420; border: 1px solid #344865; border-radius: 6px; padding: 4px; selection-background-color: #316ac4; }
QComboBox QAbstractItemView { background: #101a2b; color: #e7eeff; }
QCheckBox { color: #dce7fb; spacing: 6px; min-height: 20px; }
QCheckBox::indicator { width: 16px; height: 16px; }
QCheckBox::indicator:unchecked { border-radius: 4px; background: #344357; border: 1px solid #65748d; }
QCheckBox::indicator:checked { border-radius: 4px; background: #00d37d; border: 1px solid #2ff9a0; }
QScrollArea { border: 0; background: transparent; }
QScrollBar:vertical { border: none; background: #101929; width: 9px; }
QScrollBar::handle:vertical { background: #384d6c; border-radius: 4px;min-height:24px; }
QScrollBar::handle:vertical:hover { background:#52698d; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0px;background:transparent;border:0;}
QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:transparent;}
QLineEdit:focus,QTextEdit:focus,QComboBox:focus,QDoubleSpinBox:focus {border-color:#776af5;background:#111c2e;}

)";

QString serviceStatus(const QString& value,bool russian) {
    if(!russian)return value;
    static const QMap<QString,QString> names={
        {"Disabled",QString::fromUtf8("Сервис выключен")},
        {"Module disabled",QString::fromUtf8("Модуль выключен")},
        {"Connecting",QString::fromUtf8("Подключение…")},
        {"Connected",QString::fromUtf8("Подключён")},
        {"Reconnecting",QString::fromUtf8("Переподключение…")},
        {"Waiting for stream",QString::fromUtf8("Ожидание трансляции")},
        {"Not configured",QString::fromUtf8("Введите токен")},
        {"API verification required",QString::fromUtf8("API требует проверки")},
        {"API responded (legacy, unverified)",QString::fromUtf8("API ответил — экспериментальный режим")},
        {"API connected (HTTP polling)",QString::fromUtf8("API подключён — проверка новых донатов каждые 30 с")},
        {"Incorrect DonatePay API key",QString::fromUtf8("Неверный API-ключ DonatePay — скопируйте ключ из кабинета API")},
        {"DonatePay API error",QString::fromUtf8("DonatePay вернул ошибку API — проверьте кабинет")},
        {"DonatePay authorization rejected (401)",QString::fromUtf8("Ключ DonatePay отклонён (401)")},
        {"DonatePay authorization rejected (403)",QString::fromUtf8("Нет прав DonatePay API (403)")},
        {"Invalid or expired token",QString::fromUtf8("Токен недействителен или истёк")},
        {"OAuth token rejected (401)",QString::fromUtf8("OAuth-токен отклонён API (401)")},
        {"DonationAlerts permission denied (403)",QString::fromUtf8("Нет прав DonationAlerts API (403)")},
        {"Waiting for browser authorization",QString::fromUtf8("Подтвердите вход в открывшемся браузере")},
        {"Verifying authorization",QString::fromUtf8("Проверяем разрешение DonationAlerts…")},
        {"Authorization cancelled or expired",QString::fromUtf8("Вход отменён или время ожидания истекло")},
        {"Invalid authorization response",QString::fromUtf8("DonationAlerts вернул некорректный результат входа")},
        {"Authorization verification failed",QString::fromUtf8("Не удалось проверить вход DonationAlerts")},
        {"Cannot save authorization",QString::fromUtf8("Не удалось сохранить токен в защищённых настройках")},
        {"Local authorization port unavailable",QString::fromUtf8("Порт входа 38561 занят. Закройте вторую копию OBS")},
        {"Cannot open authorization browser",QString::fromUtf8("Не удалось открыть браузер для входа")},
        {"Invalid token format",QString::fromUtf8("Неверный формат OAuth-токена")},
        {"Unrecognized API response",QString::fromUtf8("Ответ API не распознан — проверьте ключ и метод")},
        {"Rate limited",QString::fromUtf8("Лимит запросов — подождите")},
        {"Network error",QString::fromUtf8("Ошибка соединения")},
        {"Journal error",QString::fromUtf8("Ошибка журнала")},
        {"No rates file: RUB only",QString::fromUtf8("Курсы не загружены — доступен RUB")},
        {"Loading exchange rates",QString::fromUtf8("Загрузка курсов…")}
    };
    if(value.startsWith("CBR /"))return QString(value).replace("currencies",QString::fromUtf8("валют"));
    return names.value(value,value);
}

struct ConnectionView {QString text;int tone=0;};
ConnectionView connectionView(const QString& state,bool ru) {
    auto text=[ru](const char* a,const char* b){return QString::fromUtf8(ru?a:b);};
    if(state=="Connected"||state.startsWith("API connected")||state.startsWith("API responded"))return {text("Подключён","Connected"),1};
    if(state=="Disabled"||state=="Module disabled")return {text("Выключен","Off"),0};
    if(state=="Not configured"||state=="API verification required")return {text("Не подключён","Not connected"),0};
    if(state=="Waiting for stream")return {text("Ожидает эфир","Waiting for stream"),2};
    if(state=="Waiting for browser authorization")return {text("Подтвердите вход","Confirm sign-in"),2};
    if(state=="Connecting"||state=="Verifying authorization")return {text("Проверяем…","Checking…"),2};
    if(state=="Reconnecting")return {text("Повторяем автоматически","Retrying automatically"),2};
    if(state=="Network error"||state.startsWith("HTTP ")||state=="Unrecognized API response"||state=="DonatePay API error")
        return {text("Повторяем автоматически","Retrying automatically"),2};
    if(state=="Rate limited")return {text("Подождите немного","Please wait"),2};
    if(state=="Authorization cancelled or expired")return {text("Вход не завершён","Sign-in incomplete"),2};
    if(state.contains("DonatePay")&&(state.contains("401")||state.contains("key")))return {text("Проверьте ключ","Check your key"),3};
    if(state.contains("403")||state=="Missing user/socket scope")return {text("Нет разрешения","Permission needed"),3};
    if(state.contains("401")||state=="Invalid or expired token"||state=="Invalid token format")return {text("Войдите снова","Sign in again"),3};
    return {text("Нужна проверка","Check connection"),3};
}
void stylePill(QLabel* label,int tone) {
    static const char* color[]={"#9cadc2","#45e6aa","#ffd166","#ff8d9c"};
    static const char* background[]={"#14202d","#102b27","#302a19","#321e2b"};
    const int index=std::clamp(tone,0,3);
    label->setStyleSheet(QString("QLabel{color:%1;background:%2;border:0;border-radius:5px;padding:3px 6px;font-size:11px;font-weight:600;}").arg(color[index],background[index]));
    label->setTextFormat(Qt::PlainText);label->setAlignment(Qt::AlignCenter);label->setMinimumHeight(20);
}
void updateConnectionPill(QLabel* label,const QString& raw,bool ru) {
    if(!label)return;
    if(label->property("connectionRaw").toString()==raw&&label->property("connectionRu").isValid()&&label->property("connectionRu").toBool()==ru)return;
    const bool changed=label->property("connectionRaw").isValid();
    const auto view=connectionView(raw,ru);label->setText(QString::fromUtf8("● ")+view.text);
    stylePill(label,view.tone);label->setToolTip(serviceStatus(raw,ru));
    label->setProperty("connectionRaw",raw);label->setProperty("connectionRu",ru);
    if(changed&&label->isVisible()){
        auto* effect=qobject_cast<QGraphicsOpacityEffect*>(label->graphicsEffect());
        if(!effect){effect=new QGraphicsOpacityEffect(label);label->setGraphicsEffect(effect);}
        if(auto* prior=effect->findChild<QPropertyAnimation*>()){prior->stop();prior->deleteLater();}
        auto* fade=new QPropertyAnimation(effect,"opacity",effect);fade->setDuration(170);
        fade->setStartValue(0.65);fade->setEndValue(1.0);fade->start(QAbstractAnimation::DeleteWhenStopped);
    }
}
QWidget* deliveryIcon(const QString& channel,const QString& status) {
    int tone=2;QString mark=QString::fromUtf8("?");
    if(status.startsWith(QString::fromUtf8("✓"))){tone=1;mark=QString::fromUtf8("✓");}
    else if(status.startsWith(QString::fromUtf8("✕"))){tone=3;mark="!";}
    else if(status.contains(QString::fromUtf8("Отключ"))||status=="Disabled"){tone=0;mark=QString::fromUtf8("—");}
    static const char* color[]={"#8ea1c0","#45e6aa","#ffd166","#ff8d9c"};
    auto* result=new QWidget;auto* row=new QHBoxLayout(result);row->setContentsMargins(1,0,1,0);row->setSpacing(3);
    auto* badge=new ServiceBadge(channel);badge->setFixedSize(18,18);row->addWidget(badge);
    auto* state=new QLabel(mark);state->setStyleSheet(QString("color:%1;font-size:12px;font-weight:800;").arg(color[std::clamp(tone,0,3)]));
    row->addWidget(state);result->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
    result->setToolTip(channel+"\n"+status);result->setAccessibleName(channel+": "+status);return result;
}

QVBoxLayout* column(QWidget* w, int gap = 9, int padding = 12)
{
    auto* v = new QVBoxLayout(w);
    v->setSpacing(std::min(gap,5));
    const int inset=std::min(padding,8);
    v->setContentsMargins(inset,inset,inset,inset);
    return v;
}
void clearLayoutItems(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) delete widget;
        else if (QLayout* nested = item->layout()) clearLayoutItems(nested);
        delete item;
    }
}
void rowWith(QVBoxLayout* v, QWidget* first, QWidget* last)
{
    auto* h = new QHBoxLayout();
    h->setSpacing(4);
    h->addWidget(first, 1);
    if(last) h->addWidget(last);
    v->addLayout(h);
}
QPushButton* btn(const QString& text, QWidget* parent = nullptr)
{
    auto* p = new QPushButton(text, parent);
    p->setCursor(Qt::PointingHandCursor);
    if(text==QString::fromUtf8("Копировать") || text=="Copy") {
        p->setText(QString());p->setIcon(streamNavIcon("copy"));p->setIconSize(QSize(18,18));
        p->setFixedWidth(30);p->setToolTip(text);p->setAccessibleName(text);
    }
    if(text==QString::fromUtf8("Показать") || text=="Show") {
        p->setText(QString());
        p->setIcon(streamEyeIcon());
        p->setIconSize(QSize(20,20));
        p->setFixedWidth(28);
        p->setToolTip(text);
        p->setAccessibleName(text);
    }
    return p;
}
QLabel* paragraph(const QString& text, QWidget* parent = nullptr)
{
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    return l;
}
}


class CreditFooter final : public QWidget {
public:
    explicit CreditFooter(QWidget* parent=nullptr):QWidget(parent) {
        setFixedHeight(20);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);setAttribute(Qt::WA_OpaquePaintEvent);
        setAccessibleName("developed by OTMOPOZOK — YouTube");
        setToolTip(QString("OZK Notify %1 (%2)\nhttps://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/").arg(STREAM_NOTIFY_VERSION,STREAM_NOTIFY_BUILD));
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(),QColor("#080f18"));
        QFont f=font();f.setPixelSize(11);p.setFont(f);
        const QFontMetricsF m(f);
        const QString prefix="developed by ",nick="OTMOPOZOK",version=" · v" STREAM_NOTIFY_VERSION;
        const qreal x=std::max(qreal(2),(width()-m.horizontalAdvance(prefix+nick+version))/2);
        const qreal baseline=(height()-m.height())/2+m.ascent();
        const qreal nickX=x+m.horizontalAdvance(prefix);
        nickRect_=QRectF(nickX-2,baseline-m.ascent()-2,m.horizontalAdvance(nick)+4,m.height()+4);
        auto draw=[&](qreal left,const QString& text,const QColor& fill) {
            QPainterPath path;path.addText(QPointF(left,baseline),f,text);
            p.setPen(QPen(QColor(255,255,255,70),1.0,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.setBrush(fill);p.drawPath(path);
        };
        draw(x,prefix,QColor("#9299a5"));
        draw(nickX,nick,hover_?QColor("#59dfff"):QColor("#9299a5"));
        draw(nickX+m.horizontalAdvance(nick),version,QColor("#9299a5"));
        if(hover_||hasFocus()) {
            p.setPen(QPen(QColor("#59dfff"),1));
            p.drawLine(QPointF(nickX,baseline+2),QPointF(nickX+m.horizontalAdvance(nick),baseline+2));
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        const bool next=nickRect_.contains(event->position());
        if(next!=hover_){hover_=next;setCursor(next?Qt::PointingHandCursor:Qt::ArrowCursor);update();}
    }
    void leaveEvent(QEvent*) override {hover_=false;setCursor(Qt::ArrowCursor);update();}
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==Qt::LeftButton && nickRect_.contains(event->position()))openChannel();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space)openChannel();
        else QWidget::keyPressEvent(event);
    }
    void focusInEvent(QFocusEvent* event) override {QWidget::focusInEvent(event);update();}
    void focusOutEvent(QFocusEvent* event) override {QWidget::focusOutEvent(event);update();}
private:
    void openChannel(){QDesktopServices::openUrl(QUrl("https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/"));}
    QRectF nickRect_;
    bool hover_=false;
};

StreamDock::StreamDock(QWidget* parent) : QWidget(parent), store_(this), sender_(this), testSender_(this), manualSender_(this), secondSender_(this), platforms_(this), donation_(&store_,this)
{
    setObjectName("StreamRoot");
    setAttribute(Qt::WA_StyledBackground,true);
    setMinimumWidth(275);
    setStyleSheet(css);
    loadedCleanly_ = store_.load();
    manualEveryoneSelected_=store_.current.discordEveryone;
    if (!loadedCleanly_) statusText_ = store_.error();
    connect(&store_, &SettingsStore::persisted, this, &StreamDock::updateSaved);
    countdown_.setInterval(1000);
    connect(&countdown_, &QTimer::timeout, this, &StreamDock::updateCountdown);
    resultTimer_.setSingleShot(true);
    connect(&resultTimer_, &QTimer::timeout, this, [this] { showPhase(Phase::Idle); });
    verificationTimer_.setInterval(20000);
    connect(&verificationTimer_,&QTimer::timeout,this,&StreamDock::probePlatforms);
    platforms_.setChanged([this]{if(!shuttingDown_)onPlatformsChanged();});
    root_ = column(this, 5, 8);
    donationConfigure_.setSingleShot(true);
    donationConfigure_.setInterval(1400);
    connect(&donationConfigure_, &QTimer::timeout, &donation_, &DonationEngine::configure);
    connect(&donation_,&DonationEngine::credentialsChanged,this,[this]{rebuild();});
    connect(&donation_, &DonationEngine::updated, this, [this] {
        updateConnectionPill(daStatusLabel_,donation_.daStatus(),isRu());
        updateConnectionPill(dpStatusLabel_,donation_.dpStatus(),isRu());
        updateConnectionPill(homeDaIndicator_,donation_.daStatus(),isRu());
        updateConnectionPill(homeDpIndicator_,donation_.dpStatus(),isRu());
        if(daLoginButton_)daLoginButton_->setVisible(donation_.daStatus()!="Connected");
        if(donationSummaryLabel_)donationSummaryLabel_->setText(donationSummaryText());
        if(donationJournalLabel_) {
            const auto entries=donation_.journal();
            donationJournalLabel_->setText(entries.isEmpty()?uiText("Пока нет донатов","No donations yet"):entries.mid(0,6).join("\n"));
        }
    });
    broadcasting_ = anyStreamingOutputActive();
    donation_.setLive(broadcasting_);
    donation_.configure();
    if (broadcasting_) alreadyHandled_ = true; // No retrospective notifications.
    // Plugin outputs can be created after frontend initialization.
    streamStateTimer_.setInterval(1000);
    connect(&streamStateTimer_,&QTimer::timeout,this,&StreamDock::refreshStreamingState);
    streamStateTimer_.start();
    telemetryClock_.start();usage_=usageSampler_.sample(0);
    telemetryTimer_.setInterval(2000);
    connect(&telemetryTimer_,&QTimer::timeout,this,&StreamDock::refreshTelemetry);
    telemetryTimer_.start();refreshTelemetry();
    rebuild();
}
StreamDock::~StreamDock() { shutdown(); }

bool StreamDock::isRu() const
{
    sn::CpuScope snCpuScope;
    if (store_.current.locale == "ru") return true;
    if (store_.current.locale == "en") return false;
    const char* locale = obs_get_locale();
    return locale && QString::fromUtf8(locale).startsWith("ru", Qt::CaseInsensitive);
}
QString StreamDock::uiText(const char* ru, const char* en) const
{
    sn::CpuScope snCpuScope; return QString::fromUtf8(isRu() ? ru : en); }
QLabel* StreamDock::label(const QString& text, const QString& name)
{
    sn::CpuScope snCpuScope;
    auto* w = paragraph(text);
    if(!name.isEmpty()) w->setObjectName(name);
    return w;
}
void StreamDock::section(QVBoxLayout* l, const QString& text)
{
    sn::CpuScope snCpuScope; l->addWidget(label(text, "Section")); }
QWidget* StreamDock::card(QVBoxLayout** l)
{
    sn::CpuScope snCpuScope;
    QWidget* w = new QWidget;
    w->setObjectName("Card");
    *l = column(w, store_.current.compact ? 3 : 5, store_.current.compact ? 5 : 8);
    w->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    (*l)->setAlignment(Qt::AlignTop);
    return w;
}
void StreamDock::changeSettings() {
    sn::CpuScope snCpuScope; store_.scheduleSave(); donationConfigure_.start(); }
void StreamDock::showPage(Page page) {
    sn::CpuScope snCpuScope; page_ = page; rebuild(); }
void StreamDock::openConnections(const QString& field) {
    sn::CpuScope snCpuScope;
    pendingFocus_=field;showPage(Page::Connections);
}
void StreamDock::showPhase(Phase phase) {
    sn::CpuScope snCpuScope; phase_ = phase; rebuild(); }
void StreamDock::updateSaved(bool success)
{
    sn::CpuScope snCpuScope;
    if(!success) {
        const QString error=uiText("Не удалось сохранить настройки. Проверьте доступ к папке OBS.",
            "Settings could not be saved. Check access to the OBS configuration folder.");
        if(statusText_!=error){statusText_=error;rebuild();}
    }
}

void StreamDock::rebuild()
{
    sn::CpuScope snCpuScope;
    // Never destroy a control while Qt is still delivering its input signal.
    // Coalesce changes and let the current event return before replacing widgets.
    if(shuttingDown_ || rebuildPending_)return;
    rebuildPending_=true;
    QTimer::singleShot(0,this,[this] {
        rebuildPending_=false;
        if(!shuttingDown_)rebuildNow();
    });
}

void StreamDock::rebuildNow()
{
    sn::CpuScope snCpuScope;
    // All text inputs continuously update SettingsStore::current, so rebuilding
    // for a language switch never discards uncommitted typed text.
    clearLayoutItems(root_);
    root_->setSpacing(5);
    title_ = nullptr;
    daStatusLabel_ = nullptr; dpStatusLabel_ = nullptr; donationJournalLabel_ = nullptr;
    homeDaIndicator_=nullptr;homeDpIndicator_=nullptr;daLoginButton_=nullptr;
    central_ = nullptr;
    auto* h = new QHBoxLayout;
    h->setContentsMargins(2,2,2,2);
    h->setSpacing(7);
    auto* brand = new ServiceBadge("OZK Notify");
    brand->setObjectName("HeaderBrand");
    h->addWidget(brand);
    auto* t = label("<span style='color:#57dfd1'>OZK</span> Notify", "Title");
    t->setTextFormat(Qt::RichText);
    t->setAccessibleName("OZK Notify");
    title_ = t;
    h->addWidget(t, 1);
    if(page_==Page::Connections||page_==Page::Message||page_==Page::Interface||page_==Page::Donations) {
        auto* backButton=btn(QString());
        backButton->setObjectName("BackShortcut");
        backButton->setIcon(streamNavIcon("back"));
        backButton->setIconSize(QSize(19,19));
        backButton->setFixedSize(30,28);
        backButton->setToolTip(uiText("Назад на основное", "Back to Home"));
        backButton->setAccessibleName(backButton->toolTip());
        connect(backButton,&QPushButton::clicked,this,[this]{showPage(Page::Home);});
        h->addWidget(backButton);
    }
    auto* manualButton=btn(QString());
    manualButton->setObjectName("ManualMessageShortcut");
    manualButton->setIcon(streamNavIcon("send",page_==Page::ManualMessage));
    manualButton->setIconSize(QSize(19,19));
    manualButton->setFixedSize(30,28);
    manualButton->setToolTip(uiText("Отправить сообщение", "Send a message"));
    manualButton->setAccessibleName(manualButton->toolTip());
    connect(manualButton,&QPushButton::clicked,this,[this]{showPage(Page::ManualMessage);});
    h->addWidget(manualButton);
    auto* settingsButton=btn(QString());
    settingsButton->setObjectName("SettingsShortcut");
    settingsButton->setIcon(streamNavIcon("settings",page_!=Page::Home&&page_!=Page::ManualMessage));
    settingsButton->setIconSize(QSize(19,19));
    settingsButton->setFixedSize(30,28);
    settingsButton->setToolTip(page_==Page::Home||page_==Page::ManualMessage?
        uiText("Настройки", "Settings"):uiText("На основное", "Back to Home"));
    settingsButton->setAccessibleName(settingsButton->toolTip());
    connect(settingsButton,&QPushButton::clicked,this,[this]{
        showPage(page_==Page::Home||page_==Page::ManualMessage?Page::Connections:Page::Home);
    });
    h->addWidget(settingsButton);
    root_->addLayout(h);

    if (phase_ == Phase::Idle || page_ != Page::Home) central_ = buildRegular();
    else if (phase_ == Phase::Counting) central_ = buildCountdown();
    else if (phase_ == Phase::Sending) central_ = buildSending();
    else if (phase_ == Phase::Result) central_ = buildResult();
    else {
        QVBoxLayout* v; central_ = card(&v);
        v->addStretch();
        v->addWidget(label(uiText("Отправка отменена", "Notification cancelled"), "Section"), 0, Qt::AlignHCenter);
        v->addWidget(label(uiText("Текущий эфир больше не будет объявлен", "No notification will be sent for this stream")), 0, Qt::AlignHCenter);
        v->addStretch();
    }
    if(phase_!=Phase::Idle && page_==Page::Home) {
        auto* scroll=new QScrollArea;
        scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->viewport()->setObjectName("ScrollViewport");
        if(central_->objectName().isEmpty())central_->setObjectName("PageBody");
        scroll->setWidget(central_);central_=scroll;
    }
    root_->addWidget(central_, 1);
    auto* footerWidget=new QWidget(this);
    footerWidget->setObjectName("Footer");
    auto* footer=new QVBoxLayout(footerWidget);footer->setContentsMargins(0,0,0,0);footer->setSpacing(0);
    auto* credit=new QLabel("✦ <span style=\"color:#8ea1b8;\">by</span> <a href=\"https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/\" style=\"color:#57dfd1;text-decoration:none;font-weight:700;\">OTMOPOZOK</a> <span style=\"color:#5e7790;\">·</span> <span style=\"color:#b3c5d7;\">v" STREAM_NOTIFY_VERSION "</span>",footerWidget);
    credit->setObjectName("Credit");credit->setTextFormat(Qt::RichText);
    credit->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);
    credit->setOpenExternalLinks(true);credit->setAlignment(Qt::AlignCenter);
    credit->setFixedHeight(20);credit->setStyleSheet("color:#9badc2;background:#0b1723;border-top:1px solid #263b50;font-size:10px;");
    credit->setToolTip("OZK Notify · OTMOPOZOK");
    footer->addWidget(credit);
    root_->addWidget(footerWidget);
    if(!pendingFocus_.isEmpty()) {
        QPointer<QLineEdit> field=findChild<QLineEdit*>(pendingFocus_);
        pendingFocus_.clear();
        QTimer::singleShot(0,this,[this,field] {
            if(!field)return;
            field->setFocus(Qt::OtherFocusReason);
            for(auto* scroll:findChildren<QScrollArea*>())
                if(scroll->widget() && scroll->widget()->isAncestorOf(field))
                    scroll->ensureWidgetVisible(field,0,12);
        });
    }
}

QWidget* StreamDock::buildRegular()
{
    sn::CpuScope snCpuScope;
    auto* wrap = new QWidget;
    auto* vertical = column(wrap, 6, 0);
    if(page_==Page::ManualMessage||page_==Page::Journal||page_==Page::Preview) {
        auto* scroll=new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->viewport()->setObjectName("ScrollViewport");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* body=page_==Page::Journal?buildJournal():page_==Page::Preview?buildAnnouncementPreview():buildManualMessage();body->setObjectName("PageBody");
        scroll->setWidget(body);vertical->addWidget(scroll,1);
        return wrap;
    }
    if (page_ != Page::Home) {
        auto* sub = new QWidget;
        auto* subnav = new QGridLayout(sub);
        subnav->setContentsMargins(0,0,0,0);
        subnav->setSpacing(4);
        const struct {Page page; QString name;} pages[] = {
            {Page::Connections, uiText("Связь", "Connect")},
            {Page::Message, uiText("Сообщение", "Message")},
            {Page::Interface, uiText("Интерфейс", "Interface")},
            {Page::Donations, uiText("Донаты", "Donations")}
        };
        int subIndex = 0;
        for (const auto& p : pages) {
            auto* b = btn(p.name);
            const QString icon=p.page==Page::Connections?"connections":p.page==Page::Message?"message":
                p.page==Page::Interface?"interface":"donations";
            b->setIcon(streamNavIcon(icon,p.page==page_));
            b->setIconSize(QSize(16,16));
            if(p.page == page_) b->setObjectName("ActiveNav");
            connect(b, &QPushButton::clicked, this, [this,p] {showPage(p.page);});
            subnav->addWidget(b, subIndex / 2, subIndex % 2);
            ++subIndex;
        }
        vertical->addWidget(sub);
    }
    if(page_==Page::Donations) {
        auto* group=new QWidget;
        auto* row=new QHBoxLayout(group);row->setContentsMargins(0,0,0,0);row->setSpacing(3);
        const struct {DonationPage page;QString title;QString hint;} sections[]={
            {DonationPage::Services,uiText("Сервисы","Services"),uiText("DonationAlerts и DonatePay","DonationAlerts and DonatePay")},
            {DonationPage::Delivery,uiText("Отправка","Delivery"),uiText("Получатели и правила отправки","Recipients and delivery rules")},
            {DonationPage::Amounts,uiText("Суммы","Amounts"),uiText("Рублёвые пороги и курсы валют","RUB thresholds and exchange rates")},
            {DonationPage::Templates,uiText("Текст","Text"),uiText("Шаблоны, предпросмотр и журнал","Templates, preview and delivery log")}
        };
        for(const auto& item:sections) {
            auto* choice=btn(item.title);choice->setObjectName("DonationTab");
            choice->setMinimumWidth(0);choice->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
            choice->setProperty("selected",item.page==donationPage_);
            choice->setProperty("donationSection",int(item.page));choice->setToolTip(item.hint);
            connect(choice,&QPushButton::clicked,this,[this,page=item.page]{donationPage_=page;rebuild();});
            row->addWidget(choice,1);
        }
        vertical->addWidget(group);
    }
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->viewport()->setObjectName("ScrollViewport");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* body = nullptr;
    switch(page_) {
        case Page::Home: body = buildHome(); break;
        case Page::Connections: body = buildConnections(); break;
        case Page::Message: body = buildMessage(); break;
        case Page::Interface: body = buildInterface(); break;
        case Page::Donations: body = buildDonations(); break;
        case Page::ManualMessage: body = buildManualMessage(); break;
        case Page::Journal: body=buildJournal(); break;
        case Page::Preview: body=buildAnnouncementPreview(); break;
    }
    body->setObjectName("PageBody");
    scroll->setWidget(body);
    vertical->addWidget(scroll,1);
    return wrap;
}

QWidget* StreamDock::buildHome()
{
    sn::CpuScope snCpuScope;
    auto* body=new QWidget;
    auto* outer=column(body,5,3);QVBoxLayout* v;
    auto* status=card(&v);
    if(!broadcasting_)status->setObjectName("WaitingCard");
    auto* statusRow=new QHBoxLayout;statusRow->setSpacing(7);
    statusRow->addWidget(new ServiceBadge(broadcasting_?"OZK Notify":"Waiting"));
    auto* statusText=new QVBoxLayout;statusText->setSpacing(2);
    statusText->addWidget(label(broadcasting_?uiText("В эфире","Live"):uiText("Ожидание трансляции","Waiting for stream"),broadcasting_?"Good":"Waiting"));
    statusText->addWidget(label(store_.current.enabled
        ?uiText("Автоанонс · через %1 с","Auto announcement · after %1 s").arg(store_.current.sendDelay)
        :uiText("Автоанонс выключен","Auto announcement off"),"Hint"));
    statusRow->addLayout(statusText,1);v->addLayout(statusRow);
    status->setToolTip(statusText_);outer->addWidget(status);

    auto* channels=card(&v);section(v,uiText("Отправка в мессенджеры","Notification channels"));
    auto* channelRow=new QHBoxLayout;channelRow->setSpacing(8);
    auto addChannel=[&](bool telegram) {
        auto* cell=new QWidget;auto* row=new QHBoxLayout(cell);row->setContentsMargins(0,0,0,0);row->setSpacing(6);
        row->addWidget(new ServiceBadge(telegram?"Telegram":"Discord"));
        auto* details=new QVBoxLayout;details->setSpacing(0);
        details->addWidget(label(telegram?"Telegram":"Discord","Section"));
        const bool configured=telegram?validTelegram(store_.current.telegramToken,store_.current.telegramChat):validDiscord(store_.current.discordWebhook);
        const bool enabled=telegram?store_.current.telegramEnabled:store_.current.discordEnabled;
        if(configured&&enabled)details->addWidget(new ReadyStatus(uiText("Готов","Ready")));
        else details->addWidget(label(!enabled?uiText("Выключен","Off"):uiText("Не настроен","Not set"),"Hint"));
        row->addLayout(details,1);
        cell->setToolTip(uiText("Проверка и подключение: Настройки → Связь","Test and configure under Settings → Connect"));
        channelRow->addWidget(cell,1);
    };
    addChannel(true);addChannel(false);v->addLayout(channelRow);
    if(!validTelegram(store_.current.telegramToken,store_.current.telegramChat) && !validDiscord(store_.current.discordWebhook)){
        auto* setup=btn(uiText("Настроить отправку","Set up notifications"));
        connect(setup,&QPushButton::clicked,this,[this]{openConnections("TelegramToken");});v->addWidget(setup);
    }
    outer->addWidget(channels);

    auto* delivery=card(&v);section(v,uiText("Отправка анонса","Stream announcement"));
    const bool sentNow=announcementAttempted_ &&
        (!session_.telegram || telegram_.status==Outcome::Sent) &&
        (!session_.discord || discord_.status==Outcome::Sent);
    const bool partial=announcementAttempted_ && !sentNow &&
        (telegram_.status==Outcome::Sent || discord_.status==Outcome::Sent);
    const bool cancelled=telegram_.status==Outcome::Cancelled || discord_.status==Outcome::Cancelled;
    // Persisted delivery history belongs in the tooltip, not the current session status.
    // Otherwise yesterday's successful announcement looks freshly sent on OBS startup.
    const bool sent=announcementAttempted_&&sentNow;
    QString outcome=sent?(broadcasting_?uiText("✓ Отправлен","✓ Sent"):
        uiText("Последний анонс: ✓ отправлен","Last announcement: ✓ sent")):
        partial?uiText("Анонс отправлен частично","Announcement partly delivered"):
        broadcasting_&&cancelled?uiText("Отправка отменена","Cancelled"):
        broadcasting_&&announcementAttempted_?uiText("Доставка не подтверждена","Delivery unconfirmed"):
        !broadcasting_&&announcementAttempted_?uiText("Последний анонс: доставка не подтверждена","Last announcement: delivery unconfirmed"):
        broadcasting_?uiText("Ещё не отправляли","Not sent yet"):
        uiText("Ожидание трансляции","Waiting for stream");
    auto* announcementStatus=label(outcome,sent?"Good":"Hint");
    announcementStatus->setProperty("announcementStatus",true);
    v->addWidget(announcementStatus);
    if(!store_.current.lastTime.isEmpty()){
        auto* historyRow=new QHBoxLayout;historyRow->setSpacing(7);
        const auto when=QDateTime::fromString(store_.current.lastTime,"yyyy-MM-dd HH:mm:ss");
        if(when.isValid()){
            const auto moment=when.date()==QDate::currentDate()?when.toString("HH:mm"):when.toString("dd.MM · HH:mm");
            historyRow->addWidget(label(uiText("Последняя отправка · ","Last delivery · ")+moment,"Hint"),1);
        } else {
            historyRow->addStretch(1);
        }
        historyRow->addWidget(deliveryIcon("Telegram",store_.current.lastTelegram));
        historyRow->addWidget(deliveryIcon("Discord",store_.current.lastDiscord));
        v->addLayout(historyRow);
    }
    delivery->setToolTip(store_.current.lastTime+"\nTelegram: "+store_.current.lastTelegram+"\nDiscord: "+store_.current.lastDiscord+"\n"+statusText_);
    outer->addWidget(delivery);

    auto* actions=new QHBoxLayout;actions->setSpacing(5);
    auto* preview=btn(uiText("Просмотр анонса","Preview announcement"));preview->setObjectName("AnnouncementPreview");
    connect(preview,&QPushButton::clicked,this,[this]{showPage(Page::Preview);});actions->addWidget(preview,1);
    auto* journal=btn(uiText("Журнал","Delivery log"));journal->setObjectName("DeliveryJournal");
    connect(journal,&QPushButton::clicked,this,[this]{showPage(Page::Journal);});actions->addWidget(journal,1);outer->addLayout(actions);

    auto* donations=card(&v);
    section(v,broadcasting_?uiText("Донаты за эфир","Stream donations"):uiText("Донаты","Donations"));
    auto* donationSummary=label(donationSummaryText(),"Hint");donationSummaryLabel_=donationSummary;
    donationSummary->setStyleSheet("font-size:13px;font-weight:600;color:#dce7f4;padding:2px 0;");
    donationSummary->setToolTip(uiText(
        "Полученные плагином реальные донаты за текущий/последний эфир, включая суммы ниже порога. Тесты и повторы исключены. Перезапуск OBS обнуляет эту сводку; журнал доставки сохраняется.",
        "Real donations observed during the current/last stream, including amounts below the announcement threshold. Tests and duplicates are excluded. This summary resets on OBS restart; delivery history remains saved."));
    v->addWidget(donationSummary);
    donationSummary->setVisible(broadcasting_||donation_.sessionSummary().count>0||!store_.current.donations.enabled);
    if(store_.current.donations.enabled){
        auto addConnection=[&](const QString& name,const QString& status,QPointer<QLabel>& target){
            auto* row=new QHBoxLayout;row->setSpacing(6);auto* badge=new ServiceBadge(name);badge->setFixedSize(18,18);row->addWidget(badge);
            row->addWidget(label(name,"Hint"),1);auto* indicator=new QLabel;target=indicator;
            updateConnectionPill(indicator,status,isRu());row->addWidget(indicator);v->addLayout(row);
        };
        if(store_.current.donations.daEnabled)addConnection("DonationAlerts",donation_.daStatus(),homeDaIndicator_);
        if(store_.current.donations.dpEnabled)addConnection("DonatePay",donation_.dpStatus(),homeDpIndicator_);
    }
    outer->addWidget(donations);

    auto* outputs=new sn::OutputPanel(isRu());outputs->setObjectName("OutputMonitor");
    outputs->setStyleSheet("QWidget#OutputMonitor{background:#0c1520;border:1px solid #263244;border-radius:8px;}");
    outputs->setOutputs(monitor_.views());outputPanel_=outputs;outer->addWidget(outputs);
    outer->addStretch();return body;
}

QWidget* StreamDock::buildJournal(){
    auto* body=new QWidget;auto* outer=column(body,7,3);
    auto* back=btn(uiText("← Назад","← Back"));connect(back,&QPushButton::clicked,this,[this]{showPage(Page::Home);});outer->addWidget(back);
    outer->addWidget(label(uiText("Журнал отправок","Delivery log"),"Section"));
    outer->addWidget(label(uiText("Последние отправки: получатель, время и результат. Неопределённая доставка автоматически не повторяется.","Recent deliveries: recipient, time and result. Unconfirmed deliveries are not retried blindly."),"Hint"));
    int count=0;
    for(const auto& event:store_.current.events){
        const auto code=event.mid(21).section(" | ",0,0);
        if((!code.startsWith("MAIN_")&&!code.startsWith("MANUAL_")&&!code.startsWith("TEST_")&&!code.startsWith("SECOND_"))||(!code.contains("TELEGRAM")&&!code.contains("DISCORD")))continue;
        const bool ok=code.endsWith("SENT")||code.endsWith("OK");
        const bool failed=code.endsWith("FAILED")||code.endsWith("FAIL");
        const QString recipient=event.contains("TELEGRAM")?"Telegram":"Discord";
        const QString kind=event.contains("SECOND_")?uiText("Вторая платформа","Second platform"):
            event.contains("MANUAL_")?uiText("Ручное сообщение","Manual message"):event.contains("TEST_")?uiText("Проверка","Test"):uiText("Анонс","Announcement");
        QVBoxLayout* rows;auto* item=card(&rows);
        rows->addWidget(label(recipient+" · "+kind,"Section"));rows->addWidget(label(event.left(19),"Hint"));
        rows->addWidget(label(ok?uiText("✓ Доставлено","✓ Delivered"):failed?uiText("Не отправлено","Not sent"):uiText("Доставка не подтверждена","Delivery unconfirmed"),ok?"Good":"Hint"));
        const auto reason=event.section(" | ",1);
        if(!reason.isEmpty()&&!ok){
            const QString friendly=reason.contains("401")||reason.contains("403")||reason.contains("404")?uiText("Проверьте подключение получателя и права отправки.","Check recipient settings and posting permissions."):
                reason.contains("429")?uiText("Сервис временно ограничил отправку. Попытки завершены.","The service rate-limited delivery. Attempts have ended."):
                reason.contains("Network")||reason.contains("Connection")?uiText("Проверьте интернет, VPN или прокси. Сообщение могло дойти — сначала проверьте канал.","Check internet, VPN or proxy. The message may have arrived; check the channel before trying again."):
                serviceStatus(reason,isRu());
            auto* detail=label(friendly,"Hint");detail->setToolTip(reason);rows->addWidget(detail);
        }
        outer->addWidget(item);++count;
    }
    if(!count)outer->addWidget(label(uiText("Отправок пока нет","No deliveries yet"),"Hint"));
    outer->addStretch();return body;
}

QString StreamDock::confirmedPreview(QString* error)const{
    const auto now=QDateTime::currentDateTimeUtc();
    const auto url=[&](const sn::LivePlatform& live){return broadcasting_&&live.phase==sn::LivePhase::Live&&live.confirmed&&live.checkedAt.isValid()&&live.checkedAt.secsTo(now)>=0&&live.checkedAt.secsTo(now)<90?live.url:QString();};
    const auto youtube=url(platforms_.youtube()),twitch=url(platforms_.twitch()),kick=url(platforms_.kick());
    if(youtube.isEmpty()&&twitch.isEmpty()&&kick.isEmpty()){
        if(error)*error=uiText("Нет недавно подтверждённого эфира. Запустите трансляцию и обновите проверку.","No recently confirmed live stream. Start streaming and refresh the check.");return {};
    }
    return renderAnnouncement(store_.current.message,youtube,twitch,{},error,kick);
}

QWidget* StreamDock::buildAnnouncementPreview(){
    auto* body=new QWidget;auto* outer=column(body,7,3);
    auto* back=btn(uiText("← Назад","← Back"));connect(back,&QPushButton::clicked,this,[this]{showPage(Page::Home);});outer->addWidget(back);
    outer->addWidget(label(uiText("Предварительный просмотр","Announcement preview"),"Section"));
    outer->addWidget(label(uiText("Только ссылки на недавно подтверждённые эфиры. Открытие этой страницы ничего не отправляет.","Only recently confirmed live links. Opening this page sends nothing."),"Hint"));
    QString error;const auto message=confirmedPreview(&error);
    auto* preview=new QTextEdit;preview->setObjectName("ConfirmedAnnouncementText");preview->setReadOnly(true);preview->setPlainText(error.isEmpty()?message:error);preview->setMinimumHeight(170);outer->addWidget(preview);
    auto* refresh=btn(uiText("Обновить проверку","Refresh check"));refresh->setEnabled(broadcasting_);connect(refresh,&QPushButton::clicked,this,[this]{probePlatforms();});outer->addWidget(refresh);
    auto* use=btn(uiText("Открыть для ручной отправки","Open for manual delivery"));use->setEnabled(!message.isEmpty()&&error.isEmpty());
    connect(use,&QPushButton::clicked,this,[this]{QString error;const auto message=confirmedPreview(&error);if(!error.isEmpty()||message.isEmpty()){rebuild();return;}manualDraft_=message;showPage(Page::ManualMessage);});outer->addWidget(use);
    outer->addStretch();return body;
}

QWidget* StreamDock::buildManualMessage()
{
    sn::CpuScope snCpuScope;
    auto* body=new QWidget;
    auto* outer=column(body,8,3);
    auto* back=btn(uiText("← Назад на основное", "← Back to Home"));
    back->setObjectName("ManualMessageBack");
    back->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
    connect(back,&QPushButton::clicked,this,[this]{showPage(Page::Home);});
    outer->addWidget(back);

    QVBoxLayout* v;
    auto* compose=card(&v);
    v->setSpacing(8);
    v->addWidget(label(uiText("СООБЩЕНИЕ ПОДПИСЧИКАМ", "MESSAGE TO SUBSCRIBERS"),"ManualEyebrow"));
    v->addWidget(label(uiText("Новое сообщение", "New message"),"ManualTitle"));
    v->addWidget(label(uiText("Текст и ссылки для Telegram и Discord.\nЭфир не требуется.",
        "Text and links for Telegram and Discord.\nNo stream required."),"Hint"));
    auto* editor=new QTextEdit(manualDraft_);
    editor->setObjectName("ManualMessageEditor");
    editor->setPlaceholderText(uiText("Напишите сообщение или вставьте ссылку…",
        "Write a message or paste a link…"));
    editor->setFixedHeight(142);
    v->addWidget(editor);
    const bool tgReady=validTelegram(store_.current.telegramToken,store_.current.telegramChat);
    const bool dcReady=validDiscord(store_.current.discordWebhook);
    auto* tg=new QCheckBox("Telegram");tg->setObjectName("ManualTelegram");
    auto* dc=new QCheckBox("Discord");dc->setObjectName("ManualDiscord");
    tg->setEnabled(tgReady&&!manualSending_);dc->setEnabled(dcReady&&!manualSending_);
    tg->setChecked(tgReady&&manualTelegramSelected_);
    dc->setChecked(dcReady&&manualDiscordSelected_);
    v->addWidget(label(uiText("ПОЛУЧАТЕЛИ", "RECIPIENTS"),"ManualEyebrow"));
    auto* recipients=new QHBoxLayout;recipients->setSpacing(6);
    auto addRecipient=[&](const QString& name,QCheckBox* check){
        auto* tile=new QWidget;tile->setObjectName("ManualRecipient");
        auto* row=new QHBoxLayout(tile);row->setContentsMargins(7,4,6,4);row->setSpacing(5);
        row->addWidget(new ServiceBadge(name));row->addWidget(check,1);
        recipients->addWidget(tile,1);
    };
    addRecipient("Telegram",tg);addRecipient("Discord",dc);v->addLayout(recipients);
    auto* everyone=new QCheckBox(uiText("Тегнуть @everyone в Discord", "Mention @everyone in Discord"));
    everyone->setObjectName("ManualEveryone");
    everyone->setChecked(manualEveryoneSelected_);
    everyone->setEnabled(dcReady&&dc->isChecked()&&!manualSending_);
    everyone->setToolTip(uiText("Если включено, Discord получит упоминание @everyone. Настройка действует только для этого сообщения.",
        "When enabled, Discord receives an @everyone mention. This choice applies to this message only."));
    v->addWidget(everyone);
    if(!tgReady||!dcReady)v->addWidget(label(uiText(
        "Недоступный канал можно настроить в разделе «Настройки → Связь».",
        "Set up an unavailable channel under Settings → Connect."),"Hint"));
    auto* meta=new QHBoxLayout;meta->setContentsMargins(0,0,0,0);
    meta->addWidget(label(uiText("Отправится сразу", "Sends immediately"),"Hint"),1);
    auto* count=label(QString(),"Hint");count->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    meta->addWidget(count);v->addLayout(meta);
    auto* send=btn(uiText("Отправить сообщение", "Send message"));
    send->setObjectName("ManualMessageSend");
    send->setIcon(streamNavIcon("send",true));send->setIconSize(QSize(17,17));
    v->addWidget(send);
    auto updateForm=[this,editor,tg,dc,everyone,send,count]{
        const bool chosen=tg->isChecked()||dc->isChecked();
        const int length=editor->toPlainText().size();
        const int limit=dc->isChecked()?2000:4096;
        const int outgoingLength=dc->isChecked()?discordContent(editor->toPlainText(),everyone->isChecked()).size():length;
        count->setText(QString("%1 / %2").arg(outgoingLength).arg(limit));
        count->setStyleSheet(outgoingLength>limit?"color:#ff8f8f;":"color:#8da5bb;");
        everyone->setEnabled(dc->isChecked()&&dc->isEnabled()&&!manualSending_);
        const bool fits=dc->isChecked()?discordPayload(editor->toPlainText(),everyone->isChecked()).has_value():length<=4096;
        send->setEnabled(!manualSending_&&chosen&&!editor->toPlainText().trimmed().isEmpty()&&fits);
    };
    connect(editor,&QTextEdit::textChanged,this,[this,editor,updateForm]{manualDraft_=editor->toPlainText();updateForm();});
    connect(tg,&QCheckBox::toggled,this,[this,updateForm](bool checked){manualTelegramSelected_=checked;updateForm();});
    connect(dc,&QCheckBox::toggled,this,[this,updateForm](bool checked){manualDiscordSelected_=checked;updateForm();});
    connect(everyone,&QCheckBox::toggled,this,[this,updateForm](bool checked){manualEveryoneSelected_=checked;updateForm();});
    connect(send,&QPushButton::clicked,this,&StreamDock::sendManualMessage);
    editor->setReadOnly(manualSending_);updateForm();
    outer->addWidget(compose);

    if(!manualTelegramStatus_.isEmpty()||!manualDiscordStatus_.isEmpty()) {
        auto* result=card(&v);section(v,uiText("Результат отправки", "Delivery result"));
        if(!manualTelegramStatus_.isEmpty())v->addWidget(label("Telegram: "+manualTelegramStatus_,"Hint"));
        if(!manualDiscordStatus_.isEmpty())v->addWidget(label("Discord: "+manualDiscordStatus_,"Hint"));
        outer->addWidget(result);
    }
    outer->addStretch();return body;
}

void StreamDock::sendManualMessage()
{
    sn::CpuScope snCpuScope;
    if(manualSending_)return;
    const bool tg=manualTelegramSelected_&&validTelegram(store_.current.telegramToken,store_.current.telegramChat);
    const bool dc=manualDiscordSelected_&&validDiscord(store_.current.discordWebhook);
    const QString message=manualDraft_;
    if((!tg&&!dc)||message.trimmed().isEmpty()||
       (dc?!discordPayload(message,manualEveryoneSelected_).has_value():message.size()>4096))return;
    Snapshot snap;
    snap.token=store_.current.telegramToken;snap.chat=store_.current.telegramChat;
    snap.webhook=store_.current.discordWebhook;
    snap.discordEveryone=dc&&manualEveryoneSelected_;
    manualSending_=true;manualAllSent_=true;manualPending_=int(tg)+int(dc);
    manualTelegramStatus_=tg?uiText("Отправка…","Sending…"):QString();
    manualDiscordStatus_=dc?uiText("Отправка…","Sending…"):QString();
    rebuild();
    auto submit=[this,snap,message](Channel channel){
        manualSender_.send(channel,snap,message,[this,channel](Delivery result){
            if(shuttingDown_)return;
            const bool ok=result.status==Outcome::Sent;
            if(!ok)manualAllSent_=false;
            const QString status=ok?uiText("✓ Отправлено","✓ Sent"):
                result.status==Outcome::Unknown?
                    uiText("⚠ Доставка не подтверждена. Сообщение могло дойти — проверьте сервис. Автоматического повтора нет.",
                           "⚠ Delivery unconfirmed. The message may have arrived — check the service. No automatic retry."):
                    uiText("⚠ Не отправлено: ","⚠ Not sent: ")+result.detail;
            if(channel==Channel::Telegram)manualTelegramStatus_=status;
            else manualDiscordStatus_=status;
            store_.addEvent(QString(channel==Channel::Telegram?"MANUAL_TELEGRAM ":"MANUAL_DISCORD ")+
                (ok?"SENT":"NOT_CONFIRMED")+" | "+result.detail.left(240));
            if(--manualPending_==0){
                manualSending_=false;
                if(manualAllSent_)manualDraft_.clear();
            }
            rebuild();
        });
    };
    if(tg)submit(Channel::Telegram);
    if(dc)submit(Channel::Discord);
}

QString StreamDock::donationSummaryText() const
{
    const auto summary=donation_.sessionSummary();
    if(!summary.count)return !store_.current.donations.enabled?uiText("Модуль выключен","Module off"):
        summary.started?uiText("Пока нет донатов","No donations yet"):uiText("Ожидаем эфир","Waiting for stream");
    const QLocale locale=isRu()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    QString result=uiText("Получено: %1","Received: %1").arg(qulonglong(summary.count));
    if(summary.count>summary.unpriced)result+=QString::fromUtf8(" · ")+(summary.converted?QString::fromUtf8("≈"):QString())+
        locale.toString(summary.rub,'f',std::floor(summary.rub)==summary.rub?0:2)+QString::fromUtf8(" ₽");
    if(summary.unpriced)result+=uiText(" · без курса: %1"," · unpriced: %1").arg(qulonglong(summary.unpriced));
    return result;
}

QString StreamDock::donationConnectionText() const
{
    const auto& settings=store_.current.donations;
    QStringList parts;
    if(settings.daEnabled)parts << "DonationAlerts: "+serviceStatus(donation_.daStatus(),isRu());
    if(settings.dpEnabled)parts << "DonatePay: "+serviceStatus(donation_.dpStatus(),isRu());
    return parts.join("\n");
}

QWidget* StreamDock::buildConnections()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* global = card(&v);
    auto* all = new StreamSwitch(uiText("Анонсировать начало стрима", "Announce stream start"));
    all->setChecked(store_.current.enabled);
    connect(all, &QCheckBox::toggled, this, [this](bool enabled) {store_.current.enabled=enabled;changeSettings();});
    v->addWidget(all);
    outer->addWidget(global);

    auto* telegramCard = card(&v);
    serviceSection(v,"Telegram");
    auto* tg = new StreamSwitch(uiText("Отправлять в Telegram", "Send to Telegram"));
    tg->setChecked(store_.current.telegramEnabled);
    connect(tg, &QCheckBox::toggled, this, [this](bool val){store_.current.telegramEnabled=val;changeSettings();});
    v->addWidget(tg);
    v->addWidget(label(uiText("1. Токен из @BotFather", "1. Token from @BotFather"), "Hint"));
    auto* token = new QLineEdit(store_.current.telegramToken);
    token->setObjectName("TelegramToken");
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("123456789:... (BotFather)");
    connect(token, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.telegramToken=s;changeSettings();});
    auto* showToken = btn(uiText("Показать", "Show"));
    connect(showToken, &QPushButton::clicked, this, [token] {token->setEchoMode(token->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, token, showToken);
    v->addWidget(label(uiText("2. Канал для уведомлений", "2. Notification channel"), "Hint"));
    auto* chat = new QLineEdit(store_.current.telegramChat);
    chat->setPlaceholderText(uiText("@канал или ссылка t.me/…", "@channel or t.me/… link"));
    chat->setToolTip(uiText("Публичный: @имя или ссылка. Приватный: ссылка на сообщение t.me/c/… или ID -100…",
        "Public: @name or link. Private: t.me/c/… message link or -100… ID."));
    connect(chat, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.telegramChat=telegramChannel(s);changeSettings();});
    v->addWidget(chat);
    auto* channelHint=label(QString(),"Hint");
    channelHint->setToolTip(uiText("Укажите имя канала, а не личный ник или имя бота. Бот должен быть администратором с правом публикации.",
        "Use the channel name, not a personal username or bot name. The bot must be an administrator allowed to post."));
    auto updateChannelHint=[this,channelHint](const QString& value) {
        const QString normalized=telegramChannel(value);
        const bool valid=QRegularExpression("^(?:-?[0-9]+|@[A-Za-z0-9_]{5,})$").match(normalized).hasMatch();
        channelHint->setText(value.trimmed().isEmpty()
            ? uiText("Бот должен быть администратором канала.", "The bot must be a channel administrator.")
            : valid ? uiText("Получатель: ", "Recipient: ")+normalized
            : uiText("Нужен @канал или ссылка на сообщение. Ссылка-приглашение не подходит.",
                     "Use @channel or a message link. Invite links are not supported."));
    };
    connect(chat,&QLineEdit::textChanged,channelHint,updateChannelHint);
    updateChannelHint(chat->text());
    v->addWidget(channelHint);
    auto* h1 = new QHBoxLayout;
    auto* testTg = btn(uiText("3. Отправить тест", "3. Send test"));
    testTg->setEnabled(!testingTelegram_ && validTelegram(store_.current.telegramToken,store_.current.telegramChat));
    auto updateTelegramTest=[this,testTg] {testTg->setEnabled(!testingTelegram_ && validTelegram(store_.current.telegramToken,store_.current.telegramChat));};
    connect(token,&QLineEdit::textChanged,testTg,updateTelegramTest);
    connect(chat,&QLineEdit::textChanged,testTg,updateTelegramTest);
    connect(testTg, &QPushButton::clicked, this, [this] {testChannel(Channel::Telegram);});
    auto* helpTg = btn(uiText("Как настроить?", "Setup guide"));
    connect(helpTg, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this,uiText("Подключение Telegram", "Connect Telegram"),uiText(
            "1. Откройте @BotFather в Telegram. Создайте бота командой /newbot или выберите своего через /mybots. Скопируйте токен сюда, в поле плагина.\n\n"
            "2. В настройках канала добавьте бота в администраторы. Достаточно права «Публикация сообщений».\n\n"
            "3. Публичный канал: вставьте @имя или ссылку t.me/имя.\n"
            "Приватный канал: скопируйте ссылку на любое сообщение внутри канала (t.me/c/…) и вставьте её. Плагин определит ID. Ссылка-приглашение t.me/+… не подходит.\n\n"
            "4. Включите Telegram и нажмите «Отправить тест». Трансляцию запускать не нужно.\n\n"
            "Токен вводите только здесь. Не отправляйте его в чат или GitHub.",
            "1. Open @BotFather in Telegram. Create a bot with /newbot or select yours with /mybots. Paste its token into the plugin.\n\n"
            "2. Add the bot as a channel administrator with permission to post messages.\n\n"
            "3. Public channel: paste @name or t.me/name.\n"
            "Private channel: paste a link to a message inside the channel (t.me/c/…). The plugin extracts the ID. Invite links t.me/+… are not supported.\n\n"
            "4. Enable Telegram and click Send test. No livestream is needed.\n\n"
            "Enter the token locally only; never share it in chat or GitHub."));
    });
    h1->addWidget(testTg); h1->addWidget(helpTg);
    v->addLayout(h1);
    if (!transientTelegram_.isEmpty()) v->addWidget(label(transientTelegram_,"Hint"));
    outer->addWidget(telegramCard);

    auto* discordCard = card(&v);
    serviceSection(v,"Discord");
    auto* dc = new StreamSwitch(uiText("Отправлять в Discord", "Send to Discord"));
    dc->setChecked(store_.current.discordEnabled);
    connect(dc, &QCheckBox::toggled, this, [this](bool val){store_.current.discordEnabled=val;changeSettings();});
    v->addWidget(dc);
    auto* everyone=new StreamSwitch(uiText("Упоминать @everyone","Mention @everyone"));
    everyone->setChecked(store_.current.discordEveryone);
    everyone->setToolTip(uiText("Добавлять @everyone в конец сообщений Discord: анонсы, донаты и ручные тесты.",
        "Append @everyone to Discord messages: stream announcements, donations and manual tests."));
    connect(everyone,&QCheckBox::toggled,this,[this](bool checked){
        store_.current.discordEveryone=checked;manualEveryoneSelected_=checked;changeSettings();
    });
    v->addWidget(everyone);

    v->addWidget(label("Webhook URL", "Hint"));
    auto* hook = new QLineEdit(store_.current.discordWebhook);
    hook->setObjectName("DiscordWebhook");
    hook->setEchoMode(QLineEdit::Password);
    hook->setPlaceholderText("https://discord.com/api/webhooks/...");
    connect(hook, &QLineEdit::textChanged, this, [this](const QString& s){store_.current.discordWebhook=s;changeSettings();});
    auto* showHook = btn(uiText("Показать", "Show"));
    connect(showHook, &QPushButton::clicked, this, [hook] {hook->setEchoMode(hook->echoMode()==QLineEdit::Password ? QLineEdit::Normal : QLineEdit::Password);});
    rowWith(v, hook, showHook);
    auto* h2 = new QHBoxLayout;
    auto* testDc = btn(uiText("Отправить тест", "Send test"));
    testDc->setEnabled(!testingDiscord_ && validDiscord(store_.current.discordWebhook));
    connect(hook,&QLineEdit::textChanged,testDc,[this,testDc]{testDc->setEnabled(!testingDiscord_ && validDiscord(store_.current.discordWebhook));});
    connect(testDc, &QPushButton::clicked, this, [this] {testChannel(Channel::Discord);});
    auto* helpDc = btn(uiText("Как создать?", "Setup guide"));
    connect(helpDc, &QPushButton::clicked, this, [] {QDesktopServices::openUrl(QUrl("https://support.discord.com/hc/en-us/articles/228383668-Intro-to-Webhooks"));});
    h2->addWidget(testDc); h2->addWidget(helpDc);
    v->addLayout(h2);
    if (!transientDiscord_.isEmpty()) v->addWidget(label(transientDiscord_,"Hint"));
    outer->addWidget(discordCard);

    auto* urls = card(&v);
    section(v,uiText("Ссылки на трансляцию", "Stream links"));
    v->addWidget(label("YouTube", "Hint"));
    auto* yt = new QLineEdit(store_.current.youtubeUrl);
    yt->setObjectName("YouTubeUrl");
    yt->setPlaceholderText("https://www.youtube.com/@channel/live");
    connect(yt, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.youtubeUrl=s;changeSettings();});
    v->addWidget(yt);
    v->addWidget(label("Twitch", "Hint"));
    auto* tw = new QLineEdit(store_.current.twitchUrl);
    tw->setObjectName("TwitchUrl");
    tw->setPlaceholderText("https://www.twitch.tv/channel");
    connect(tw, &QLineEdit::textChanged, this, [this](const QString& s) {store_.current.twitchUrl=s;changeSettings();});
    v->addWidget(tw);
    section(v,uiText("Подтверждение активности","Live verification"));
    v->addWidget(label(uiText("Можно войти здесь или использовать аккаунты OZK Chat. Защищённые токены OZK Chat читаются только для проверки эфира.",
                              "Sign in here or use OZK Chat accounts. Protected OZK Chat tokens are read only to verify live status."),"Hint"));
    v->addWidget(label("Kick", "Hint"));auto* ki=new QLineEdit(store_.current.kickUrl);ki->setObjectName("KickUrl");ki->setPlaceholderText("https://kick.com/channel");
    connect(ki,&QLineEdit::textChanged,this,[this](const QString& value){store_.current.kickUrl=value;changeSettings();});v->addWidget(ki);
    v->addWidget(label(uiText("Можно оставить пустым, если канал Kick настроен в OZK Chat.","Leave empty to use the Kick channel configured in OZK Chat."),"Hint"));
    if(!platforms_.kick().error.isEmpty())v->addWidget(label(platforms_.kick().error,"Hint"));
    v->addWidget(label(platforms_.youtubeAccount(),"Hint"));
    if(!platforms_.youtube().error.isEmpty())v->addWidget(label(platforms_.youtube().error,"Hint"));
    auto* ytLogin=btn(uiText("Войти в YouTube","Sign in to YouTube"));
    connect(ytLogin,&QPushButton::clicked,this,[this]{platforms_.beginYouTubeLogin();});v->addWidget(ytLogin);
    auto* ytLogout=btn(uiText("Удалить вход Notify в YouTube","Remove Notify YouTube login"));
    connect(ytLogout,&QPushButton::clicked,this,[this]{platforms_.signOutYouTube();rebuild();});v->addWidget(ytLogout);
    v->addWidget(label(platforms_.twitchAccount(),"Hint"));
    if(!platforms_.twitch().error.isEmpty())v->addWidget(label(platforms_.twitch().error,"Hint"));
    auto* twLogin=btn(uiText("Войти в Twitch","Sign in to Twitch"));
    connect(twLogin,&QPushButton::clicked,this,[this]{platforms_.beginTwitchLogin();});v->addWidget(twLogin);
    auto* twLogout=btn(uiText("Удалить вход Notify в Twitch","Remove Notify Twitch login"));
    connect(twLogout,&QPushButton::clicked,this,[this]{platforms_.signOutTwitch();rebuild();});v->addWidget(twLogout);
    if(!platforms_.twitchDeviceCode().isEmpty())v->addWidget(label(uiText("Код Twitch: ","Twitch code: ")+platforms_.twitchDeviceCode(),"Good"));
    v->addWidget(label(uiText("Ручная ссылка используется при ошибке API только для активного отдельного выхода OBS этой платформы.",
                              "A manual link is used on API error only when a separate OBS output for that platform is active."),"Hint"));
    outer->addWidget(urls);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildMessage()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* templ = card(&v);
    templ->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v, uiText("Шаблон уведомления", "Notification template"));
    v->addWidget(label(uiText("{youtube}, {twitch}, {kick}, {youtube_url}, {twitch_url}, {kick_url}, {stream_url}", "{youtube}, {twitch}, {kick}, {youtube_url}, {twitch_url}, {kick_url}, {stream_url}"), "Hint"));
    auto* edit = new QTextEdit;
    edit->setFixedHeight(store_.current.compact?92:120);
    edit->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    edit->setPlainText(store_.current.message);
    connect(edit, &QTextEdit::textChanged, this, [this,edit] {store_.current.message=edit->toPlainText();changeSettings();});
    v->addWidget(edit);
    auto* tokens = new QHBoxLayout;
    for (const QString& t : {QString("{youtube}"), QString("{twitch}"), QString("{kick}")}) {
        auto* button = btn(t);
        connect(button, &QPushButton::clicked, this, [edit,t] {edit->insertPlainText(t);});
        tokens->addWidget(button);
    }
    v->addLayout(tokens);
    outer->addWidget(templ);

    auto* second=card(&v);
    section(v,uiText("Анонс второй платформы","Second platform announcement"));
    auto* secondEnabled=new StreamSwitch(uiText("Отправлять, когда подключится вторая платформа","Send when the second platform goes live"));
    secondEnabled->setChecked(store_.current.secondAnnouncementEnabled);
    connect(secondEnabled,&QCheckBox::toggled,this,[this](bool enabled){store_.current.secondAnnouncementEnabled=enabled;changeSettings();});
    v->addWidget(secondEnabled);
    v->addWidget(label(uiText("Переменные: {platform}, {stream_url}, {youtube_url}, {twitch_url}, {kick_url}",
                             "Variables: {platform}, {stream_url}, {youtube_url}, {twitch_url}, {kick_url}"),"Hint"));
    auto* secondEdit=new QTextEdit;secondEdit->setFixedHeight(store_.current.compact?90:112);
    secondEdit->setPlainText(store_.current.secondMessage);
    connect(secondEdit,&QTextEdit::textChanged,this,[this,secondEdit]{store_.current.secondMessage=secondEdit->toPlainText();changeSettings();});
    v->addWidget(secondEdit);outer->addWidget(second);

    auto* delay = card(&v);
    delay->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v,uiText("Задержка отправки", "Notification delay"));
    v->addWidget(label(uiText("После начала эфира, включая Aitum", "After going live, including Aitum"), "Hint"));
    auto* delayRow=new QHBoxLayout;delayRow->setSpacing(4);
    for(int seconds:{60,90,120,360}) {
        auto* choice=btn(QString::number(seconds)+uiText(" сек"," sec"));
        choice->setCheckable(true);choice->setChecked(store_.current.sendDelay==seconds);
        if(choice->isChecked())choice->setObjectName("ActiveNav");
        connect(choice,&QPushButton::clicked,this,[this,seconds]{
            store_.current.sendDelay=seconds;changeSettings();rebuild();
        });
        delayRow->addWidget(choice,1);
    }
    v->addLayout(delayRow);
    outer->addWidget(delay);

    auto* prev = card(&v);
    prev->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    v->setAlignment(Qt::AlignTop);
    section(v,uiText("Предпросмотр", "Preview"));
    QString err;
    const QString preview = prepareMessage(store_.current, &err);
    auto* livePreview = label(err.isEmpty() ? preview : (uiText("Ошибка: ", "Error: ")+err),err.isEmpty()?"":"Bad");
    v->addWidget(livePreview);
    connect(edit, &QTextEdit::textChanged, this, [this, livePreview] {
        QString problem;
        QString val = prepareMessage(store_.current, &problem);
        livePreview->setText(problem.isEmpty() ? val : uiText("Ошибка: ", "Error: ") + problem);
    });
    auto* refresh = btn(uiText("Обновить предпросмотр", "Refresh preview"));
    connect(refresh, &QPushButton::clicked, this, [this]{rebuild();});
    v->addWidget(refresh);
    outer->addWidget(prev);
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildDonations()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto& s = store_.current.donations;
    auto addToggle=[&](QVBoxLayout* l,const QString& title,bool& target) {
        auto* c=new StreamSwitch(title);
        c->setChecked(target);
        connect(c,&QCheckBox::toggled,this,[this,value=&target](bool checked){*value=checked;changeSettings();});
        l->addWidget(c);
    };
    auto* master=card(&v);
    section(v,uiText("Уведомления о донатах", "Donation notifications"));
    addToggle(v,uiText("Включить модуль", "Enable donation module"),s.enabled);
    outer->addWidget(master);

    if(donationPage_==DonationPage::Services) {
    auto addService=[&](const QString& title,bool& enabled,QString& token,QLabel*& statusLabel,
                        const QString& status,std::function<void()> test){
        QVBoxLayout* l;
        auto* box=card(&l);
        serviceSection(l,title);
        addToggle(l,uiText("Получать донаты", "Receive donations"),enabled);
        statusLabel=new QLabel;
        updateConnectionPill(statusLabel,status,isRu());
        l->addWidget(statusLabel);
        auto* edit=new QLineEdit(token);edit->setEchoMode(QLineEdit::Password);
        edit->setPlaceholderText(title=="DonatePay"
            ?uiText("Вставьте ключ из кабинета DonatePay","Paste your DonatePay account key")
            :uiText("Заполнится после входа","Filled after sign-in"));
        edit->setAccessibleName(title=="DonatePay"?uiText("API-ключ DonatePay","DonatePay API key"):
            uiText("OAuth-токен DonationAlerts","DonationAlerts OAuth token"));
        connect(edit,&QLineEdit::textChanged,this,[this,value=&token](const QString& text){*value=text;changeSettings();});
        auto* reveal=btn(uiText("Показать", "Show"));
        connect(reveal,&QPushButton::clicked,this,[edit]{edit->setEchoMode(edit->echoMode()==QLineEdit::Password?QLineEdit::Normal:QLineEdit::Password);});
        rowWith(l,edit,reveal);
        if(title=="DonationAlerts"){
            auto* login=btn(uiText("Войти в DonationAlerts","Sign in to DonationAlerts"));
            login->setObjectName("DonationAlertsLogin");
            daLoginButton_=login;login->setVisible(status!="Connected");
            connect(login,&QPushButton::clicked,this,[this]{donation_.beginDonationAlertsLogin();});
            l->addWidget(login);
        }
        auto* verify=btn(uiText("Проверить", "Check"));
        connect(verify,&QPushButton::clicked,this,[this,title,test] {
            auto& settings=store_.current.donations;
            const bool donatePay=title=="DonatePay";
            QString reason;
            if(!settings.enabled)reason=uiText("Включите модуль донатов вверху этой страницы.","Enable the donation module at the top of this page.");
            else if(!(donatePay?settings.dpEnabled:settings.daEnabled))reason=uiText("Включите «Получать донаты» в этой карточке.","Turn on Receive donations in this card.");
            else if((donatePay?settings.dpToken:settings.daToken).isEmpty())reason=donatePay?
                uiText("Вставьте ключ из вашего кабинета DonatePay. Кнопка «?» откроет нужную страницу.","Paste the key from your DonatePay account. The ? button opens the right page."):
                uiText("Нажмите «Войти в DonationAlerts» и подтвердите подключение в браузере.","Click Sign in to DonationAlerts and confirm in your browser.");
            else if(settings.onlyLive && !broadcasting_)reason=uiText(
                "В разделе «Отправка» включено «Только во время стрима».\nНачните эфир в OBS/Aitum или выключите это ограничение для проверки.",
                "Only while streaming is enabled under Delivery.\nGo live with OBS/Aitum or turn this restriction off to check the connection.");
            if(!reason.isEmpty()) {QMessageBox::information(this,title,reason);return;}
            // Preserve the existing consent gate; an explicit Check action now
            // enables the officially verified adapter without an extra jargon toggle.
            if(donatePay&&!settings.legacyApiOptIn){settings.legacyApiOptIn=true;store_.scheduleSave();}
            test();
        });
        auto* help=btn("?");help->setFixedWidth(28);
        help->setAccessibleName(uiText("Помощь с подключением ","Connection help: ")+title);
        help->setToolTip(title=="DonatePay"?uiText("Скопируйте ключ из своего кабинета. Подробности подключения доступны в журнале.","Copy your account key. Connection details are available in the log."):
            uiText("Войдите через браузер. Ключ сохранится автоматически; ключ виджета здесь не нужен.","Sign in through your browser. The account key is saved automatically; a widget key is not needed."));
        connect(help,&QPushButton::clicked,this,[title] {
            QDesktopServices::openUrl(QUrl(title=="DonatePay"?"https://donatepay.ru/page/api":
                "https://github.com/DIMI0007/ozk-notify/blob/main/README_DONATIONS_RU.md#donationalerts"));
        });
        rowWith(l,verify,help);
        outer->addWidget(box);
    };
    addService("DonationAlerts",s.daEnabled,s.daToken,daStatusLabel_,donation_.daStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonationAlerts();});
    addService("DonatePay",s.dpEnabled,s.dpToken,dpStatusLabel_,donation_.dpStatus(),[this]{donationConfigure_.stop();donation_.configure();donation_.checkDonatePay();});
    }
    if(donationPage_==DonationPage::Delivery) {
    auto* rules=card(&v);
    section(v,uiText("Правила отправки","Delivery rules"));
    addToggle(v,uiText("Только во время стрима","Only while streaming"),s.onlyLive);
    addToggle(v,uiText("Публиковать сообщение донатера","Include donor message"),s.includeDonorMessage);
    v->addWidget(label(uiText("Донаты и анонсы стрима включаются независимо.","Donation and stream announcements are enabled independently."),"Hint"));
    outer->addWidget(rules);

    auto* destinations=card(&v);
    section(v,uiText("Куда отправлять", "Destinations"));
    v->addWidget(label(uiText("Используются подключения из раздела «Связь».","Uses the connections configured in Connections."),"Hint"));
    addToggle(v,QString("Telegram"),s.toTelegram);
    addToggle(v,QString("Discord"),s.toDiscord);
    outer->addWidget(destinations);

    }
    if(donationPage_==DonationPage::Amounts) {
    auto* thresholds=card(&v);
    section(v,uiText("Пороговые суммы", "Donation thresholds"));
    v->addWidget(label(uiText("Все пороги — в рублях. Курсы сохраняются один раз, без автообновлений.",
        "All thresholds are in RUB. Rates are saved once, without automatic refresh."),"Hint"));
    auto* ratesLabel=label(serviceStatus(donation_.ratesStatus(),isRu()),"Hint");
    v->addWidget(ratesLabel);
    connect(&donation_,&DonationEngine::updated,ratesLabel,[this,ratesLabel]{ratesLabel->setText(serviceStatus(donation_.ratesStatus(),isRu()));});
    auto* ratesButton=btn(uiText("Загрузить / обновить курсы", "Load / refresh rates"));
    connect(ratesButton,&QPushButton::clicked,this,[this]{donation_.fetchRates(true);});
    v->addWidget(ratesButton);
    auto* thresholdEditors=new QWidget;
    auto* editorLayout=column(thresholdEditors,5,0);
    auto populateThresholds=[this,editorLayout]() {
        while(QLayoutItem* item=editorLayout->takeAt(0)) {if(item->widget())delete item->widget();delete item;}
        const QString code="RUB";
        auto& thresholds=store_.current.donations.thresholds[code];
        if(thresholds.size()!=3)thresholds={100,500,2000};
        const QStringList names={uiText("💰 Обычный", "💰 Standard"),uiText("🔥 Крупный", "🔥 Large"),uiText("👑 Мега", "👑 Mega")};
        for(int i=0;i<3;++i){
            auto* h=new QHBoxLayout;
            h->addWidget(label(names.at(i)),1);
            auto* amount=new QDoubleSpinBox;
            amount->setRange(0.01,999999999);
            amount->setDecimals(2);
            amount->setValue(thresholds[i]);
            amount->setSuffix(" "+code);
            connect(amount,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this,code,i,amount](double val){
                auto& a=store_.current.donations.thresholds[code];
                QVector<double> next=a;next[i]=val;
                if(next[0]>0&&next[1]>next[0]&&next[2]>next[1]) {
                    a=next;amount->setStyleSheet("");changeSettings();
                } else amount->setStyleSheet("border:1px solid #e7a34d;");
            });
            h->addWidget(amount);
            editorLayout->addLayout(h);
        }
        editorLayout->addWidget(label(uiText("Пороги должны строго возрастать. Изменения применяются только к новым донатам.",
            "Thresholds must strictly increase. Changes apply to future donations only."),"Hint"));
    };
    populateThresholds();v->addWidget(thresholdEditors);
    outer->addWidget(thresholds);

    }
    if(donationPage_==DonationPage::Templates) {
    auto* templates=card(&v);
    section(v,uiText("Шаблоны трёх уровней", "Three level templates"));
    auto* select=new QComboBox;
    select->addItem(uiText("💰 Обычный", "💰 Standard"),0);
    select->addItem(uiText("🔥 Крупный", "🔥 Large"),1);
    select->addItem(uiText("👑 Мега", "👑 Mega"),2);
    v->addWidget(select);
    auto* editor=new QTextEdit;
    editor->setFixedHeight(store_.current.compact?96:132);
    editor->setPlainText(s.templates.at(0));
    v->addWidget(editor);
    v->addWidget(label("{donor} {amount} {currency} {message} {service} {stream_url}","Hint"));
    auto* preview=label("","Hint");
    preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto updatePreview=[this,select,preview]() {
        Donation test;test.service="DonationAlerts";test.id="preview";test.donor=uiText("Тестовый зритель","Test donor");
        test.message=uiText("Спасибо за стрим!","Thanks for the stream!");test.currency="RUB";
        const auto values=store_.current.donations.thresholds.value("RUB");
        test.amount=values.size()==3?values.at(select->currentIndex()):100;
        const QString url=!store_.current.youtubeUrl.isEmpty()?store_.current.youtubeUrl:!store_.current.twitchUrl.isEmpty()?store_.current.twitchUrl:store_.current.kickUrl;
        preview->setText(donationMessage(test,store_.current.donations,select->currentIndex(),url));
    };
    connect(select,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this,editor,updatePreview](int idx){
        editor->setPlainText(store_.current.donations.templates.at(idx));updatePreview();
    });
    connect(editor,&QTextEdit::textChanged,this,[this,editor,select,updatePreview]{
        store_.current.donations.templates[select->currentIndex()]=editor->toPlainText();
        changeSettings();updatePreview();
    });
    auto* templateButtons=new QHBoxLayout;
    auto* reset=btn(uiText("По умолчанию", "Reset template"));
    connect(reset,&QPushButton::clicked,this,[this,editor,select]{
        const DonationSettings defaults;
        editor->setPlainText(defaults.templates.at(select->currentIndex()));
    });
    auto* test=btn(uiText("Отправить тест", "Send test"));
    connect(test,&QPushButton::clicked,this,[this,select]{donation_.previewTest(select->currentIndex());});
    templateButtons->addWidget(reset);templateButtons->addWidget(test);
    v->addLayout(templateButtons);
    section(v,uiText("Предпросмотр", "Preview"));
    v->addWidget(preview);updatePreview();
    outer->addWidget(templates);

    auto* journal=card(&v);
    section(v,uiText("Журнал донатов и доставки", "Donation and delivery log"));
    const auto recentDonations=donation_.journal();
    donationJournalLabel_=label(recentDonations.isEmpty()?uiText("Пока нет донатов","No donations yet"):recentDonations.mid(0,6).join("\n"),"Hint");
    donationJournalLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(donationJournalLabel_);
    auto* fullLog=btn(uiText("Весь журнал","Full log"));
    connect(fullLog,&QPushButton::clicked,this,[this] {
        QMessageBox box(this);box.setWindowTitle(uiText("Журнал донатов","Donation log"));
        const auto entries=donation_.journal();
        box.setText(entries.isEmpty()?uiText("Пока нет донатов","No donations yet"):
            uiText("Последние записи доставки. Нажмите «Показать подробности».","Recent delivery records. Open Show Details."));
        if(!entries.isEmpty())box.setDetailedText(entries.join("\n"));
        box.exec();
    });
    v->addWidget(fullLog);
    outer->addWidget(journal);
    }
    outer->addStretch();
    return body;
}

QWidget* StreamDock::buildInterface()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* outer = column(body, 11, 3);
    QVBoxLayout* v;
    auto* locale = card(&v);
    section(v,uiText("Язык интерфейса", "Interface language"));
    auto* languageRow=new QHBoxLayout;languageRow->setSpacing(4);
    const struct {const char* code;QString title;} languages[]={
        {"auto",uiText("Авто","Auto")},{"ru",QString::fromUtf8("Русский")},{"en",QString("English")}
    };
    for(const auto& item:languages) {
        auto* choice=btn(item.title);choice->setCheckable(true);
        choice->setChecked(store_.current.locale==item.code);
        if(choice->isChecked())choice->setObjectName("ActiveNav");
        const QString code=QString::fromLatin1(item.code);
        connect(choice,&QPushButton::clicked,this,[this,code] {
            store_.current.locale=code;changeSettings();rebuild();
        });
        languageRow->addWidget(choice,1);
    }
    v->addLayout(languageRow);
    outer->addWidget(locale);

    auto* appearance = card(&v);
    section(v,uiText("Вид панели", "Appearance"));
    auto* compact = new StreamSwitch(uiText("Компактный режим", "Compact mode"));
    compact->setChecked(store_.current.compact);
    connect(compact, &QCheckBox::toggled, this, [this](bool b){store_.current.compact=b;changeSettings();rebuild();});
    v->addWidget(compact);
    section(v,uiText("Длительность экрана результата", "Result screen duration"));
    auto* result = new QComboBox;
    result->addItem(uiText("30 секунд", "30 seconds"),30);
    result->addItem(uiText("45 секунд — по умолчанию", "45 seconds — default"),45);
    result->addItem(uiText("60 секунд", "60 seconds"),60);
    result->setCurrentIndex(result->findData(store_.current.resultSeconds));
    connect(result, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this,result](int){store_.current.resultSeconds=result->currentData().toInt();changeSettings();});
    v->addWidget(result);
    outer->addWidget(appearance);

    auto* history = card(&v);
    section(v,uiText("Журнал событий (последние 100)", "Event log (last 100)"));
    for (const QString& msg : store_.current.events.mid(0,12)) v->addWidget(label(msg,"Hint"));
    if(store_.current.events.isEmpty()) v->addWidget(label(uiText("Пока пусто", "No events yet"),"Hint"));
    auto* clear = btn(uiText("Очистить журнал", "Clear log"));
    connect(clear, &QPushButton::clicked, this, [this] {store_.current.events.clear();changeSettings();rebuild();});
    v->addWidget(clear);
    outer->addWidget(history);

    auto* about = card(&v);
    section(v,uiText("О плагине", "About"));
    auto* versionLabel=label(QString("OZK Notify %1\n%2").arg(STREAM_NOTIFY_VERSION,STREAM_NOTIFY_BUILD));
    versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(versionLabel);
    v->addWidget(label(uiText("Анонсы стримов и донатов.\nНастройки хранятся на этом ПК.", "Stream and donation announcements.\nSettings are stored on this PC."), "Hint"));
    outer->addWidget(about);
    outer->addStretch();
    return body;
}

// Ring updates once per countdown tick; the local pulse runs only in the final ten visible seconds.
class CountdownRing final : public QWidget {
public:
    explicit CountdownRing(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(160,160);setMaximumSize(280,280);
        QSizePolicy policy(QSizePolicy::Preferred,QSizePolicy::Preferred);policy.setHeightForWidth(true);setSizePolicy(policy);
        // Local repaint only, at 20 fps for the last ten visible seconds.
        // Sending still follows StreamDock's elapsed timer, independently.
        pulseTimer_.setParent(this);pulseTimer_.setObjectName("CountdownPulseTimer");
        pulseTimer_.setInterval(50);
        connect(&pulseTimer_,&QTimer::timeout,this,[this]{update();});
    }
    QSize sizeHint() const override {return QSize(236,236);}
    QSize minimumSizeHint() const override {return QSize(160,160);}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {return std::clamp(width,160,280);}
    void setValues(int remaining, int total) {
        const int next=std::max(0,remaining);
        if(next!=remaining_ || !beat_.isValid()) beat_.start();
        remaining_=next;total_=std::max(1,total);
        refreshAnimation();update();
    }
protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);refreshAnimation();
    }
    void hideEvent(QHideEvent* event) override {
        pulseTimer_.stop();QWidget::hideEvent(event);
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int side = std::min(width(), height()) - 17;
        const QRectF r((width()-side)/2.0, (height()-side)/2.0, side, side);
        QRadialGradient halo(r.center(),side/2.0);halo.setColorAt(0,QColor("#0d2635"));halo.setColorAt(1,QColor("#080f18"));
        p.setPen(Qt::NoPen);p.setBrush(halo);p.drawEllipse(r);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#26364d"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawEllipse(r);
        p.setPen(QPen(QColor("#24dcf8"), 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r,90*16, -int(360.0*16*remaining_/total_));
        const double angle=(90.0-360.0*remaining_/total_)*3.14159265358979323846/180.0;
        const QPointF tip(r.center().x()+r.width()/2*std::cos(angle),r.center().y()-r.height()/2*std::sin(angle));
        p.setPen(Qt::NoPen);p.setBrush(QColor("#6ef2ff"));p.drawEllipse(tip,5,5);
        // One soft heartbeat per second. No layout changes, flashing or effects.
        const double phase=beat_.isValid()?(beat_.elapsed()%1000)/1000.0:0.0;
        const double wave=lastSeconds()?std::pow(std::sin(phase*3.14159265358979323846),2):0.0;
        const qreal scale=1.0+0.06*wave;
        const QColor accent=QColor("#24dcf8").lighter(100+int(18*wave));
        p.save();p.translate(r.center());p.scale(scale,scale);p.translate(-r.center());
        p.setPen(QColor("#e4f3ff"));
        QFont f=font(); f.setPixelSize(std::clamp(int(side*0.22),32,60)); f.setBold(true); p.setFont(f);
        const QString minutes=QString("%1:").arg(remaining_/60,2,10,QChar('0'));
        const QString seconds=QString("%1").arg(remaining_%60,2,10,QChar('0'));
        const QFontMetricsF metrics(f);
        const qreal start=(width()-metrics.horizontalAdvance(minutes+seconds))/2;
        const qreal baseline=height()/2.0+metrics.ascent()/2;
        p.drawText(QPointF(start,baseline),minutes);
        p.setPen(accent);
        p.drawText(QPointF(start+metrics.horizontalAdvance(minutes),baseline),seconds);
        p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const qreal cx=width()/2.0,cy=height()/2.0-side*0.27;
        p.drawEllipse(QPointF(cx,cy),11,11);
        p.drawLine(QPointF(cx,cy),QPointF(cx,cy-6));
        p.drawLine(QPointF(cx,cy),QPointF(cx+5,cy+3));
        p.restore();
    }
private:
    bool lastSeconds() const {return remaining_>0 && remaining_<=10;}
    void refreshAnimation() {
        if(lastSeconds() && isVisible()) {if(!pulseTimer_.isActive())pulseTimer_.start();}
        else pulseTimer_.stop();
    }
    int remaining_ = 60;
    int total_ = 60;
    QTimer pulseTimer_;
    QElapsedTimer beat_;
};

QWidget* StreamDock::buildCountdown()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    body->setObjectName("PageBody");
    auto* v = column(body, 12, 7);
    v->addSpacing(12);
    auto* heading = label(uiText("До отправки уведомлений", "Until notifications are sent"),"Section");
    heading->setAlignment(Qt::AlignCenter);

    auto* ring = new CountdownRing;
    ring_ = ring;
    ring->setValues(std::max(0, session_.delay - int(elapsed_.elapsed()/1000)),session_.delay);
    v->addWidget(ring, 0, Qt::AlignHCenter);
    v->addWidget(heading);
    auto* help = label(uiText("Уведомления будут отправлены после таймера", "Messages will be sent when the timer expires"),"Hint");
    help->setAlignment(Qt::AlignCenter);
    v->addWidget(help);

    QVBoxLayout* channels;
    auto* box = card(&channels);
    section(channels,uiText("Активные каналы", "Active channels"));
    auto addChannel=[this,channels](const QString& name,bool enabled) {
        auto* row=new QHBoxLayout;row->setSpacing(7);
        row->addWidget(new ServiceBadge(name));
        row->addWidget(label(name,"Section"),1);
        row->addWidget(label(enabled?uiText("● Вкл","● On"):uiText("Выключен","Off"),enabled?"Good":"Hint"));
        channels->addLayout(row);
    };
    addChannel("Telegram",session_.telegram);addChannel("Discord",session_.discord);
    channels->addWidget(label(uiText("Задержка: %1 сек.", "Delay: %1 sec.").arg(session_.delay),"Hint"));
    v->addWidget(box);
    auto* cancel = btn(uiText("Отменить отправку", "Cancel notification"));
    cancel->setIcon(streamNavIcon("stop",true));cancel->setIconSize(QSize(18,18));
    cancel->setObjectName("Cancel");
    connect(cancel, &QPushButton::clicked, this, [this] {cancelCurrent("User cancelled");});
    v->addWidget(cancel);
    auto* sub = label(uiText("Отмена действует только для текущего стрима", "Cancellation only affects this stream"),"Hint");
    sub->setAlignment(Qt::AlignCenter);
    v->addWidget(sub);
    v->addStretch(1);
    return body;
}

QWidget* StreamDock::buildSending()
{
    sn::CpuScope snCpuScope;
    auto* body = new QWidget;
    auto* v = column(body, 15, 20);
    v->addStretch();
    auto* heading = label(uiText("Отправляем уведомления…", "Sending notifications…"), "Section");
    heading->setAlignment(Qt::AlignCenter);
    v->addWidget(heading);
    auto* sub = label(uiText("Telegram и Discord обрабатываются независимо", "Telegram and Discord are handled independently"),"Hint");
    sub->setAlignment(Qt::AlignCenter);
    v->addWidget(sub);
    auto add = [&](const QString& title, const Delivery& d) {
        QVBoxLayout* c;
        auto* b = card(&c);
        section(c,title);
        c->addWidget(label(statusName(d),"Hint"));
        v->addWidget(b);
    };
    add("Telegram",telegram_);
    add("Discord",discord_);
    v->addStretch();
    return body;
}

QString StreamDock::statusName(const Delivery& d) const
{
    sn::CpuScope snCpuScope;
    switch(d.status) {
        case Outcome::Disabled: return uiText("Отключён", "Disabled");
        case Outcome::NotConfigured: return uiText("Не настроен", "Not configured");
        case Outcome::Waiting: return uiText("Ожидает отправки", "Queued");
        case Outcome::Sending: return uiText("Отправляется…", "Sending…");
        case Outcome::Sent: return uiText("✓ Отправлено", "✓ Sent");
        case Outcome::Failed: return uiText("✕ Ошибка", "✕ Error");
        case Outcome::Unknown: return uiText("? Результат неизвестен", "? Unconfirmed");
        case Outcome::Cancelled: return uiText("Отменено", "Cancelled");
    }
    return {};
}
QWidget* StreamDock::buildResult()
{
    sn::CpuScope snCpuScope;
    const bool tgSent = telegram_.status == Outcome::Sent;
    const bool dcSent = discord_.status == Outcome::Sent;
    const bool allOk = (!session_.telegram || tgSent) && (!session_.discord || dcSent);
    const bool partial = (tgSent || dcSent) && !allOk;
    const bool unconfirmed = telegram_.status == Outcome::Unknown || discord_.status == Outcome::Unknown;
    auto* body = new QWidget;
    body->setObjectName("PageBody");
    auto* v = column(body, 16, 13);
    v->addSpacing(16);
    auto* symbol=new ResultEmblem(allOk?0:partial?1:2);
    v->addWidget(symbol,0,Qt::AlignHCenter);
    auto* title = label(allOk ? uiText("Уведомления отправлены", "Notifications sent") :
       partial ? uiText("Отправлено частично", "Partially delivered") :
       unconfirmed ? uiText("Доставка не подтверждена", "Delivery unconfirmed") :
       uiText("Не удалось отправить", "Delivery failed"));
    title->setAlignment(Qt::AlignHCenter);
    QFont font = title->font(); font.setPointSize(15); font.setBold(true); title->setFont(font);
    title->setStyleSheet("color:#e7fff4;font-size:17px;font-weight:700;");
    v->addWidget(title);
    auto* summary=label(allOk?uiText("Рассылка успешно завершена","Notifications delivered"):
        uiText("Подробности по каждому каналу ниже","Details for each channel below"),"Hint");
    summary->setAlignment(Qt::AlignCenter);v->addWidget(summary);
    QVBoxLayout* results;
    auto* cardWidget = card(&results);
    auto detail = [this,results](const QString& name, const Delivery& d) {
        auto* row=new QHBoxLayout;row->setSpacing(7);
        row->addWidget(new ServiceBadge(name));row->addWidget(label(name,"Section"),1);
        row->addWidget(label(statusName(d),d.status==Outcome::Sent?"Good":"Hint"));
        results->addLayout(row);
        if (d.status == Outcome::Failed || d.status == Outcome::Unknown || d.status == Outcome::NotConfigured) {
            results->addWidget(label(d.detail,"Hint"));
        }
    };
    detail("Telegram",telegram_); detail("Discord",discord_);
    v->addWidget(cardWidget);
    auto* hint = label(uiText("Панель автоматически вернётся через %1 секунд", "The normal panel will return in %1 seconds")
                       .arg(session_.resultSeconds),"Hint");
    hint->setAlignment(Qt::AlignCenter);
    QVBoxLayout* returnLayout;
    auto* returnCard=card(&returnLayout);
    returnLayout->addWidget(hint);
    auto* progress=new QProgressBar;
    progress->setRange(0,session_.resultSeconds);progress->setValue(session_.resultSeconds);progress->setTextVisible(false);
    progress->setFixedHeight(6);
    progress->setStyleSheet("QProgressBar {background:#1b293b;border:0;border-radius:3px;} QProgressBar::chunk {background:#00db89;border-radius:3px;}");
    returnLayout->addWidget(progress);
    auto* back=btn(uiText("На основную","Back to Home"));back->setIcon(streamNavIcon("home"));
    connect(back,&QPushButton::clicked,this,[this]{resultTimer_.stop();page_=Page::Home;showPhase(Phase::Idle);});
    returnLayout->addWidget(back);
    v->addWidget(returnCard);
    auto* tick=new QTimer(progress);tick->setInterval(1000);
    connect(tick,&QTimer::timeout,progress,[this,progress,hint] {
        const int remaining=std::max(0,(resultTimer_.remainingTime()+999)/1000);
        progress->setValue(remaining);
        hint->setText(uiText("Возврат к панели через %1 сек.","Back to panel in %1 sec.").arg(remaining));
    });
    tick->start();
    v->addStretch();
    return body;
}

void StreamDock::updateCountdown()
{
    sn::CpuScope snCpuScope;
    if(phase_ != Phase::Counting) {countdown_.stop(); return;}
    const qint64 msRemaining = qint64(session_.delay)*1000 - elapsed_.elapsed();
    const int remain = int(std::max<qint64>(0, (msRemaining + 999) / 1000));
    if(ring_) ring_->setValues(remain,session_.delay);
    if (msRemaining <= 0) {
        countdown_.stop();
        dispatch();
    }
}

void StreamDock::refreshStreamingState()
{
    sn::CpuScope snCpuScope;
    if(shuttingDown_)return;
    const bool live=anyStreamingOutputActive();
    if(live==broadcasting_)return;
    if(live)streamStarted();else streamStopped();
}

void StreamDock::streamStarted()
{
    sn::CpuScope snCpuScope;
    if (shuttingDown_) return;
    broadcasting_ = true;
    donation_.setLive(true);
    // One notification session spans all concurrently active streaming outputs.
    if(alreadyHandled_) {rebuild();return;}
    platforms_.beginOutputSession();
    alreadyHandled_ = true;
    announcementAttempted_=false;telegram_={};discord_={};
    mainDelivered_=false;mainIds_.clear();secondPending_.clear();mainTemplate_=store_.current.message;
    resultTimer_.stop();
    ++generation_;
    if(!store_.current.enabled) {statusText_=uiText("Автоуведомления выключены", "Automatic notifications disabled");rebuild();return;}
    QString error;
    session_ = makeSnapshot(store_.current,&error);
    telegram_ = {session_.telegram ? Outcome::Waiting : Outcome::Disabled, {}, 0};
    discord_ = {session_.discord ? Outcome::Waiting : Outcome::Disabled, {}, 0};
    if(!session_.telegram && !session_.discord) {
        statusText_=uiText("Оба мессенджера отключены", "Both channels are disabled");rebuild();return;
    }
    const bool configured = (session_.telegram && validTelegram(session_.token,session_.chat)) ||
                            (session_.discord && validDiscord(session_.webhook));
    if(!configured) {statusText_=uiText("Сначала настройте Telegram или Discord", "Configure Telegram or Discord first");rebuild();return;}
    // URLs are resolved after the platform APIs respond; validate the template at dispatch.
    elapsed_.start();
    countdown_.start();
    verificationTimer_.setInterval(20000);
    verificationTimer_.start();
    QTimer::singleShot(2500,this,[this]{if(broadcasting_&&!shuttingDown_)probePlatforms();});
    store_.addEvent("STREAM_STARTED");
    showPhase(Phase::Counting);
}

void StreamDock::cancelCurrent(const QString& reason)
{
    sn::CpuScope snCpuScope;
    if(phase_ != Phase::Counting) return;
    countdown_.stop();
    telegram_.status = (telegram_.status == Outcome::Waiting) ? Outcome::Cancelled : telegram_.status;
    discord_.status = (discord_.status == Outcome::Waiting) ? Outcome::Cancelled : discord_.status;
    store_.addEvent("CANCELLED: " + reason);
    statusText_ = uiText("Уведомление отменено для текущего эфира", "Notification cancelled for this stream");
    showPhase(Phase::Cancelled);
    resultTimer_.start(3000);
}

void StreamDock::streamStopped()
{
    sn::CpuScope snCpuScope;
    if (shuttingDown_) return;
    broadcasting_ = false;
    verificationTimer_.stop();
    donation_.setLive(false);
    alreadyHandled_ = false;
    if (phase_ == Phase::Counting) {cancelCurrent("Stream stopped"); return;}
    if (phase_ == Phase::Sending) {
        // Requests already sent must finish independently of the stream output.
        // Aborting here loses a Discord confirmation when the last output stops.
        rebuild();
        return;
    }
    rebuild();
}

void StreamDock::probePlatforms()
{
    if(shuttingDown_||!broadcasting_)return;
    const auto outputs=monitor_.sample(telemetryClock_.elapsed());
    bool youtubeOutput=false,twitchOutput=false,kickOutput=false;
    for(const auto& output:outputs){
        if(!output.stats.active)continue;
        if(output.platform=="YouTube")youtubeOutput=true;
        if(output.platform=="Twitch")twitchOutput=true;
        if(output.platform=="Kick")kickOutput=true;
    }
    platforms_.check(youtubeOutput,twitchOutput,store_.current.youtubeUrl,store_.current.twitchUrl,kickOutput,store_.current.kickUrl);
}

void StreamDock::onPlatformsChanged()
{
    if(shuttingDown_)return;
    if(page_==Page::Connections||page_==Page::Preview)rebuild();
    if(!broadcasting_)return;
    for(const QString platform:{QString("YouTube"),QString("Twitch"),QString("Kick")}){
        const auto live=platform=="YouTube"?platforms_.youtube():platform=="Twitch"?platforms_.twitch():platforms_.kick();
        const QString oldId=mainIds_.value(platform);
        if(!oldId.startsWith("manual:")||!live.confirmed||live.phase!=sn::LivePhase::Live||live.id.isEmpty())continue;
        for(const auto channel:{Channel::Telegram,Channel::Discord}){
            const QString oldKey=announcementKey("main",platform,oldId,channel);
            const QString newKey=announcementKey("main",platform,live.id,channel);
            if(store_.current.deliveryJournal.contains(oldKey)&&!store_.current.deliveryJournal.contains(newKey))
                store_.current.deliveryJournal[newKey]=store_.current.deliveryJournal.value(oldKey);
        }
        mainIds_[platform]=live.id;store_.save();
    }
    if(!mainDelivered_||!store_.current.secondAnnouncementEnabled)return;
    if(mainIds_.isEmpty())return;
    for(const QString other:{QString("YouTube"),QString("Twitch"),QString("Kick")}){
        const auto live=other=="YouTube"?platforms_.youtube():other=="Twitch"?platforms_.twitch():platforms_.kick();
        if(live.phase!=sn::LivePhase::Live||!live.confirmed||live.id.isEmpty()||mainIds_.contains(other))continue;
        sendSecondPlatform(other,live);
    }
}

void StreamDock::sendSecondPlatform(const QString& platform,const sn::LivePlatform& live)
{
    const QString pending=platform+"|"+live.id;
    if(secondPending_.contains(pending))return;
    secondPending_.insert(pending);
    const QString youtube=platform=="YouTube"?live.url:QString();
    const QString twitch=platform=="Twitch"?live.url:QString();
    QString error;
    const QString message=renderAnnouncement(store_.current.secondMessage,youtube,twitch,platform,&error,platform=="Kick"?live.url:QString());
    if(!error.isEmpty()){
        store_.addEvent("SECOND_TEMPLATE_ERROR "+error);secondPending_.remove(pending);return;
    }
    const Snapshot snapshot=makeSnapshot(store_.current);
    auto send=[this,platform,live,pending,message,snapshot](Channel channel){
        if(shuttingDown_||!broadcasting_){secondPending_.remove(pending);return;}
        if(channel==Channel::Telegram&&!snapshot.telegram)return;
        if(channel==Channel::Discord&&!snapshot.discord)return;
        const QString journalKey=announcementKey("second",platform,live.id,channel);
        if(store_.current.deliveryJournal.contains(journalKey))return;
        if(channel==Channel::Telegram&&!validTelegram(snapshot.token,snapshot.chat))return;
        if(channel==Channel::Discord&&!validDiscord(snapshot.webhook))return;
        store_.current.deliveryJournal[journalKey]="reserved";
        if(!store_.save()){store_.current.deliveryJournal.remove(journalKey);return;}
        secondSender_.send(channel,snapshot,message,[this,journalKey,channel,platform](Delivery result){
            if(shuttingDown_)return;
            store_.current.deliveryJournal[journalKey]=result.status==Outcome::Sent?"sent":
                result.status==Outcome::Unknown?"unknown":"failed";
            store_.save();
            store_.addEvent(QString("SECOND_%1_%2 ").arg(platform,channel==Channel::Telegram?"TELEGRAM":"DISCORD")+
                (result.status==Outcome::Sent?"SENT":result.status==Outcome::Unknown?"UNKNOWN":"FAILED")+" | "+result.detail.left(240));
        });
    };
    send(Channel::Telegram);
    QTimer::singleShot(1100,this,[this,send,pending]{send(Channel::Discord);secondPending_.remove(pending);});
    mainIds_[platform]=live.id;
}

void StreamDock::dispatch()
{
    sn::CpuScope snCpuScope;
    if (phase_ != Phase::Counting) return;
    if (!anyStreamingOutputActive() || !broadcasting_) {cancelCurrent("Stream inactive");return;}
    const auto youtube=platforms_.youtube(),twitch=platforms_.twitch(),kick=platforms_.kick();
    const bool ytLive=youtube.phase==sn::LivePhase::Live&&!youtube.id.isEmpty()&&validStreamUrl(youtube.url);
    const bool twLive=twitch.phase==sn::LivePhase::Live&&!twitch.id.isEmpty()&&validStreamUrl(twitch.url);
    const bool kiLive=kick.phase==sn::LivePhase::Live&&!kick.id.isEmpty()&&validStreamUrl(kick.url);
    if(!ytLive&&!twLive&&!kiLive){
        cancelCurrent("No active platform");
        statusText_=uiText("Активные трансляции не подтверждены: анонс не отправлен",
                           "No active platform verified: announcement not sent");
        rebuild();return;
    }
    if(ytLive)mainIds_["YouTube"]=youtube.id;
    if(twLive)mainIds_["Twitch"]=twitch.id;
    if(kiLive)mainIds_["Kick"]=kick.id;
    announcementAttempted_=true;
    activePending_ = 0;
    telegramMainKeys_.clear();discordMainKeys_.clear();
    const quint64 key = generation_;
    auto prepare = [this,ytLive,twLive,kiLive,&youtube,&twitch,&kick](Channel channel,Delivery& outcome,QStringList& keys)->QString {
        if(outcome.status!=Outcome::Waiting)return {};
        if(channel==Channel::Telegram&&!validTelegram(session_.token,session_.chat)){
            outcome={Outcome::NotConfigured,"Check BotFather token and Chat ID",0};return {};
        }
        if(channel==Channel::Discord&&!validDiscord(session_.webhook)){
            outcome={Outcome::NotConfigured,"Check Discord webhook URL",0};return {};
        }
        QString ytUrl,twUrl,kiUrl;
        if(ytLive){const QString k=announcementKey("main","YouTube",youtube.id,channel);
            if(!store_.current.deliveryJournal.contains(k)){ytUrl=youtube.url;keys<<k;}
            else if(store_.current.deliveryJournal.value(k)=="sent")mainDelivered_=true;}
        if(twLive){const QString k=announcementKey("main","Twitch",twitch.id,channel);
            if(!store_.current.deliveryJournal.contains(k)){twUrl=twitch.url;keys<<k;}
            else if(store_.current.deliveryJournal.value(k)=="sent")mainDelivered_=true;}
        if(kiLive){const QString k=announcementKey("main","Kick",kick.id,channel);
            if(!store_.current.deliveryJournal.contains(k)){kiUrl=kick.url;keys<<k;}
            else if(store_.current.deliveryJournal.value(k)=="sent")mainDelivered_=true;}
        if(keys.isEmpty()){outcome={Outcome::Disabled,"Already announced",0};return {};}
        QString error;const QString message=renderAnnouncement(mainTemplate_,ytUrl,twUrl,{},&error,kiUrl);
        if(!error.isEmpty()){outcome={Outcome::Failed,error,0};keys.clear();return {};}
        for(const auto& k:keys)store_.current.deliveryJournal[k]="reserved";
        if(!store_.save()){
            for(const auto& k:keys)store_.current.deliveryJournal.remove(k);
            outcome={Outcome::Failed,"Cannot reserve delivery journal",0};keys.clear();return {};
        }
        outcome.status=Outcome::Sending;++activePending_;return message;
    };
    const QString telegramMessage=prepare(Channel::Telegram,telegram_,telegramMainKeys_);
    const QString discordMessage=prepare(Channel::Discord,discord_,discordMainKeys_);
    showPhase(Phase::Sending);
    auto submit=[this,key](Channel channel,const QString& message){
        sender_.send(channel,session_,message,[this,key,channel](Delivery delivery){
            if(shuttingDown_||key!=generation_||phase_!=Phase::Sending)return;
            recordOutcome(channel,delivery);--activePending_;
            if(activePending_<=0)complete();else rebuild();
        });
    };
    if(telegram_.status==Outcome::Sending)submit(Channel::Telegram,telegramMessage);
    if(discord_.status==Outcome::Sending)QTimer::singleShot(1100,this,[this,key,submit,discordMessage]{
        if(!shuttingDown_&&key==generation_)submit(Channel::Discord,discordMessage);
    });
    if(activePending_ == 0) complete();
}

void StreamDock::recordOutcome(Channel channel, const Delivery& delivery)
{
    sn::CpuScope snCpuScope;
    if (channel == Channel::Telegram) telegram_ = delivery;
    else discord_ = delivery;
    const auto& keys=channel==Channel::Telegram?telegramMainKeys_:discordMainKeys_;
    const QString journalStatus=delivery.status==Outcome::Sent?"sent":delivery.status==Outcome::Unknown?"unknown":"failed";
    for(const auto& key:keys)store_.current.deliveryJournal[key]=journalStatus;
    store_.save();
    if(delivery.status==Outcome::Sent)mainDelivered_=true;
    const QString name = channel == Channel::Telegram ? "MAIN_TELEGRAM" : "MAIN_DISCORD";
    store_.addEvent(name + ": " + (delivery.status == Outcome::Sent ? "SENT" :
                              delivery.status == Outcome::Unknown ? "UNKNOWN" : "FAILED")+" | "+delivery.detail.left(240));
}
void StreamDock::complete()
{
    sn::CpuScope snCpuScope;
    countdown_.stop();
    store_.current.lastTelegram = statusName(telegram_)+
        (telegram_.detail.isEmpty()?QString():" — "+telegram_.detail);
    store_.current.lastDiscord = statusName(discord_)+
        (discord_.detail.isEmpty()?QString():" — "+discord_.detail);
    store_.current.lastTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    store_.addEvent("SESSION_FINISHED");
    verificationTimer_.setInterval(60000);
    statusText_=uiText("Последняя рассылка завершена", "Last delivery completed");
    showPhase(Phase::Result);
    resultTimer_.start(session_.resultSeconds*1000);
}

void StreamDock::testChannel(Channel channel)
{
    sn::CpuScope snCpuScope;
    const bool tg = channel == Channel::Telegram;
    if(tg ? testingTelegram_ : testingDiscord_) return;
    Snapshot snap = makeSnapshot(store_.current);
    if(tg ? !validTelegram(snap.token,snap.chat) : !validDiscord(snap.webhook)) {
        if(tg) transientTelegram_ = uiText("Неверный токен или ID чата", "Invalid token or chat ID");
        else transientDiscord_ = uiText("Неверный Webhook URL", "Invalid webhook URL");
        rebuild();return;
    }
    if(tg) {testingTelegram_=true;transientTelegram_=uiText("Проверяем…", "Testing…");}
    else {testingDiscord_=true;transientDiscord_=uiText("Проверяем…", "Testing…");}
    rebuild();
    const QString message = uiText("🧪 OZK Notify — тестовое сообщение", "🧪 OZK Notify — test message");
    testSender_.send(channel,snap,message,[this,channel](Delivery d) {
        if(shuttingDown_) return;
        const QString text = d.status == Outcome::Sent ? uiText("✓ Тест отправлен", "✓ Test sent") :
         d.status == Outcome::Unknown ? uiText("⚠ Доставка теста не подтверждена. Проверьте сервис перед повтором: ",
                                              "⚠ Test delivery unconfirmed. Check the service before retrying: ") + d.detail :
         uiText("⚠ Ошибка теста: ", "⚠ Test error: ") + d.detail;
        if(channel == Channel::Telegram) {testingTelegram_=false;transientTelegram_=text;}
        else {testingDiscord_=false;transientDiscord_=text;}
        store_.addEvent(QString(channel == Channel::Telegram ? "TEST_TELEGRAM " : "TEST_DISCORD ")+
                        (d.status == Outcome::Sent ? "OK" : "FAIL")+" | "+d.detail.left(240));
        rebuild();
    });
}
void StreamDock::shutdown()
{
    sn::CpuScope snCpuScope;
    if(shuttingDown_) return;
    shuttingDown_ = true;
    countdown_.stop();
    streamStateTimer_.stop();
    telemetryTimer_.stop();monitor_.clear();
    resultTimer_.stop();
    verificationTimer_.stop();platforms_.stop();secondSender_.abortAll();
    ++generation_;
    sender_.abortAll();
    testSender_.abortAll();
    manualSender_.abortAll();
    donation_.stop();
    store_.flush();
}

void StreamDock::refreshTelemetry() {
    sn::CpuScope snCpuScope;
    sn::CpuScope scope;
    if(shuttingDown_)return;
    monitor_.sample(telemetryClock_.elapsed());
    uint64_t buffers=usage_.buffers;
    const bool show=phase_==Phase::Idle && page_==Page::Home && isVisible();
    if(show) {
        sn::BufferCensus census;
        sn::account(census,store_.current);sn::account(census,session_);
        for(const auto* value:{&statusText_,&transientTelegram_,&transientDiscord_,
            &telegram_.detail,&discord_.detail})census.add(*value);
        sender_.account(census);testSender_.account(census);manualSender_.account(census);donation_.account(census);monitor_.account(census);
        for(auto* label:findChildren<QLabel*>())census.add(label->text());
        for(auto* edit:findChildren<QLineEdit*>())census.add(edit->text());
        buffers=census.bytes();
    }
    usage_=usageSampler_.sample(buffers);
    sn::measuredDataBytes.store(usage_.buffers,std::memory_order_relaxed);
    if(show&&outputPanel_)outputPanel_->setOutputs(monitor_.views());
}
