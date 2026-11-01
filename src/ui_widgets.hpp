#pragma once
#include "resources.hpp"
#include <QCheckBox>
#include <QFontMetrics>
#include <algorithm>
#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QPainterPath>
#include <QIcon>
#include <QPixmap>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>

class StreamSwitch final : public QCheckBox {
public:
    explicit StreamSwitch(const QString& title,QWidget* parent=nullptr):QCheckBox(title,parent) {
        setCursor(Qt::PointingHandCursor);
        QSizePolicy policy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        policy.setHeightForWidth(true);setSizePolicy(policy);
    }
    QSize sizeHint() const override {return QSize(180,heightForWidth(180));}
    QSize minimumSizeHint() const override {return QSize(120,24);}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {
        return std::max(24,fontMetrics().boundingRect(QRect(0,0,std::max(40,width-42),1000),
            Qt::TextWordWrap,text()).height()+4);
    }
protected:
    bool hitButton(const QPoint& point) const override {return rect().contains(point);}
    void paintEvent(QPaintEvent*) override {
        sn::CpuScope snCpuScope;
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(isEnabled()?1.0:0.45);
        const QRectF track(0,(height()-18)/2.0,32,18);
        p.setPen(QPen(isChecked()?QColor("#04de89"):QColor("#46536b"),1));
        p.setBrush(isChecked()?QColor("#00bd70"):QColor("#202a3a"));
        p.drawRoundedRect(track,9,9);
        p.setPen(Qt::NoPen);p.setBrush(QColor("#f0f5ff"));
        p.drawEllipse(QPointF(isChecked()?23:9,height()/2.0),7,7);
        p.setPen(QColor("#e3e9fb"));
        p.drawText(QRect(42,0,std::max(0,width()-42),height()),Qt::AlignVCenter|Qt::TextWordWrap,text());
        if(hasFocus()) {p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#7467ff"),1,Qt::DotLine));p.drawRoundedRect(rect().adjusted(1,1,-1,-1),4,4);}
    }
};

class ServiceBadge final : public QWidget {
public:
    explicit ServiceBadge(const QString& name,QWidget* parent=nullptr):QWidget(parent),name_(name) {
        setFixedSize(28,28);setAccessibleName(name);setToolTip(name);
        // Bundled official artwork: decode once per badge, never fetch at runtime.
        if(name_=="DonationAlerts") mark_.load(":/stream-notify/brands/donationalerts.png");
        else if(name_=="DonatePay") mark_.load(":/stream-notify/brands/donatepay.png");
    }
protected:
    void paintEvent(QPaintEvent*) override {
        sn::CpuScope snCpuScope;
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.scale(width()/32.0,height()/32.0);
        if(!mark_.isNull()) {
            // The same 28px footprint as other services. Keep the full mark,
            // its original colours and aspect ratio inside a padded square.
            p.setPen(Qt::NoPen);
            p.setBrush(name_=="DonationAlerts"?QColor("#191919"):QColor("#121212"));
            p.drawRoundedRect(QRectF(0,0,32,32),9,9);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            QSizeF fitted=mark_.size();
            fitted.scale(QSizeF(26,26),Qt::KeepAspectRatio);
            const QRectF target(QPointF((32-fitted.width())/2,(32-fitted.height())/2),fitted);
            p.drawPixmap(target,mark_,QRectF(mark_.rect()));
            return;
        }
        QColor color=name_=="Telegram"?QColor("#009aef"):name_=="Discord"?QColor("#5865f2"):
            name_=="YouTube"?QColor("#ff1538"):name_=="Twitch"?QColor("#9146ff"):name_=="Kick"?QColor("#53fc18"):
            name_=="DonationAlerts"?QColor("#f47e20"):name_=="DonatePay"?QColor("#10bb7c"):name_=="Waiting"?QColor("#3d3219"):QColor("#06382c");
        p.setPen(Qt::NoPen);p.setBrush(color);p.drawRoundedRect(QRectF(0,0,32,32),9,9);
        p.setBrush(Qt::white);
        if(name_=="Telegram") {
            QPainterPath plane;plane.moveTo(5,14);plane.lineTo(27,6);plane.lineTo(22,26);
            plane.lineTo(15,20);plane.lineTo(11,24);plane.lineTo(11,18);plane.closeSubpath();p.drawPath(plane);
            p.setPen(QPen(color,1));p.drawLine(QPointF(11,18),QPointF(23,10));
        } else if(name_=="YouTube") {
            p.drawRoundedRect(QRectF(5,8,22,16),5,5);p.setBrush(color);
            QPainterPath play;play.moveTo(13,11);play.lineTo(22,16);play.lineTo(13,21);play.closeSubpath();p.drawPath(play);
        } else if(name_=="Discord") {
            QPainterPath shape;shape.moveTo(8,9);shape.cubicTo(12,7,20,7,24,9);
            shape.lineTo(28,23);shape.lineTo(22,26);shape.lineTo(20,23);shape.lineTo(12,23);
            shape.lineTo(10,26);shape.lineTo(4,23);shape.closeSubpath();p.drawPath(shape);
            p.setBrush(color);p.drawEllipse(QPointF(12,17),2,2.5);p.drawEllipse(QPointF(20,17),2,2.5);
        } else if(name_=="Kick") {
            p.setBrush(QColor("#10200c"));const int pixels[][2]={{0,0},{0,1},{0,2},{0,3},{0,4},{1,2},{2,1},{2,2},{2,3},{3,0},{3,1},{3,3},{3,4}};
            for(const auto& cell:pixels)p.drawRect(QRectF(8+cell[0]*4,6+cell[1]*4,4,4));
        } else if(name_=="Twitch") {
            QPainterPath bubble;bubble.moveTo(7,6);bubble.lineTo(26,6);bubble.lineTo(26,21);
            bubble.lineTo(20,27);bubble.lineTo(14,27);bubble.lineTo(10,30);bubble.lineTo(10,25);bubble.lineTo(7,25);bubble.closeSubpath();
            p.drawPath(bubble);p.setPen(QPen(color,2));p.drawLine(15,11,15,19);p.drawLine(21,11,21,19);
        } else if((name_=="OZK Notify" || name_=="Waiting")) {
            const QColor broadcast=name_=="Waiting"?QColor("#ffda49"):QColor("#00df8c");
            p.setBrush(broadcast);p.drawEllipse(QPointF(16,16),3,3);
            p.setBrush(Qt::NoBrush);p.setPen(QPen(broadcast,2.5,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(QRectF(7,7,18,18),-50*16,100*16);p.drawArc(QRectF(7,7,18,18),130*16,100*16);
            p.drawArc(QRectF(3,3,26,26),-45*16,90*16);p.drawArc(QRectF(3,3,26,26),135*16,90*16);
        } else {
            QFont f=p.font();f.setPixelSize(17);f.setBold(true);p.setFont(f);p.setPen(Qt::white);
            p.drawText(QRectF(0,0,32,32),Qt::AlignCenter,name_=="DonationAlerts"?"D":"₽");
        }
    }
private: QString name_; QPixmap mark_;
};

inline QIcon streamEyeIcon() {
    QPixmap pixmap(24,24);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#bcc8ef"),1.6));p.setBrush(Qt::NoBrush);
    QPainterPath eye;eye.moveTo(3,12);eye.cubicTo(8,4,16,4,21,12);eye.cubicTo(16,20,8,20,3,12);
    p.drawPath(eye);p.drawEllipse(QPointF(12,12),3,3);return QIcon(pixmap);
}
inline void serviceSection(QVBoxLayout* layout,const QString& name) {
    auto* row=new QHBoxLayout;row->setSpacing(7);row->addWidget(new ServiceBadge(name));
    auto* title=new QLabel(name);title->setObjectName("Section");title->setWordWrap(true);row->addWidget(title,1);layout->addLayout(row);
}

inline QIcon streamNavIcon(const QString& kind,bool active=false) {
    QPixmap image(48,48);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(2,2);
    const QColor color=active?QColor("#f1f3ff"):QColor("#aebde3");
    p.setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
    if(kind=="home") {
        QPainterPath path;path.moveTo(2,11);path.lineTo(12,3);path.lineTo(22,11);
        path.lineTo(19,11);path.lineTo(19,21);path.lineTo(14,21);path.lineTo(14,15);
        path.lineTo(10,15);path.lineTo(10,21);path.lineTo(5,21);path.lineTo(5,11);path.closeSubpath();
        p.fillPath(path,color);
    } else if(kind=="settings") {
        p.save();p.translate(12,12);
        for(int i=0;i<8;++i){p.drawLine(QPointF(0,-8),QPointF(0,-10));p.rotate(45);}
        p.drawEllipse(QPointF(0,0),6.8,6.8);p.drawEllipse(QPointF(0,0),2.5,2.5);p.restore();
    } else if(kind=="connections") {
        p.save();p.translate(12,12);p.rotate(38);
        p.drawRoundedRect(QRectF(-4,-10,8,12),4,4);
        p.drawRoundedRect(QRectF(-4,-2,8,12),4,4);p.restore();
    } else if(kind=="message") {
        QPainterPath path;path.moveTo(5,4);path.lineTo(20,4);path.lineTo(20,17);
        path.lineTo(11,17);path.lineTo(5,21);path.lineTo(5,17);path.lineTo(3,17);
        path.lineTo(3,4);path.closeSubpath();p.drawPath(path);
        p.drawLine(7,9,16,9);p.drawLine(7,12,13,12);
    } else if(kind=="send") {
        QPainterPath plane;plane.moveTo(2,11);plane.lineTo(22,3);plane.lineTo(16,21);
        plane.lineTo(11,14);plane.lineTo(2,11);plane.closeSubpath();p.drawPath(plane);
        p.drawLine(11,14,22,3);
    } else if(kind=="interface") {
        p.drawRoundedRect(QRectF(2,3,20,14),2,2);p.drawLine(12,17,12,21);p.drawLine(7,21,17,21);
    } else if(kind=="donations") {
        p.drawEllipse(QPointF(12,12),9,9);
        QFont font=p.font();font.setPixelSize(15);font.setBold(true);p.setFont(font);
        p.drawText(QRectF(1,1,22,22),Qt::AlignCenter,QString::fromUtf8("₽"));
    } else if(kind=="info") {
        p.drawEllipse(QPointF(12,12),9,9);p.drawLine(12,11,12,17);p.drawPoint(12,7);
    } else if(kind=="cpu") {
        p.drawRoundedRect(QRectF(6,6,12,12),1,1);p.drawRect(QRectF(9,9,6,6));
        for(int a=8;a<=16;a+=4){p.drawLine(a,2,a,6);p.drawLine(a,18,a,22);p.drawLine(2,a,6,a);p.drawLine(18,a,22,a);}
    } else if(kind=="stop") {
        p.setPen(Qt::NoPen);p.setBrush(color);p.drawRoundedRect(QRectF(6,6,12,12),2,2);
    } else if(kind=="copy") {
        p.drawRoundedRect(QRectF(3,7,13,14),2,2);p.drawRoundedRect(QRectF(8,3,13,14),2,2);
    }
    p.end();image.setDevicePixelRatio(2);return QIcon(image);
}

class LinkCaption final : public QLabel {
public:
    explicit LinkCaption(const QString& text,QWidget* parent=nullptr):QLabel(text,parent),full_(text) {
        setTextFormat(Qt::PlainText);setToolTip(text);setAccessibleName(text);
        setMinimumWidth(0);setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        setObjectName("Hint");
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        setText(fontMetrics().elidedText(full_,Qt::ElideMiddle,std::max(1,width())));
    }
private: QString full_;
};
class ResultEmblem final : public QWidget {
public:
    explicit ResultEmblem(int state,QWidget* parent=nullptr):QWidget(parent),state_(state) {
        setFixedSize(116,116);
        setAccessibleName(state==0?"Success":state==1?"Partial delivery":"Delivery failed");
    }
protected:
    void paintEvent(QPaintEvent*) override {
        sn::CpuScope snCpuScope;
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QColor color=state_==0?QColor("#00df89"):state_==1?QColor("#ffcd57"):QColor("#ff647e");
        QColor haloColor=color;haloColor.setAlpha(55);
        QRadialGradient halo(QPointF(58,58),57);halo.setColorAt(0.4,haloColor);halo.setColorAt(1,Qt::transparent);
        p.setPen(Qt::NoPen);p.setBrush(halo);p.drawEllipse(QPointF(58,58),57,57);
        QColor edge=color;edge.setAlpha(90);p.setBrush(Qt::NoBrush);p.setPen(QPen(edge,1));
        p.drawEllipse(QPointF(58,58),49,49);
        QLinearGradient fill(20,15,95,100);fill.setColorAt(0,color);fill.setColorAt(1,color.darker(160));
        p.setBrush(fill);p.setPen(QPen(color.lighter(115),2));p.drawEllipse(QPointF(58,58),41,41);
        p.setBrush(Qt::NoBrush);p.setPen(QPen(Qt::white,6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if(state_==0) {QPainterPath mark;mark.moveTo(38,58);mark.lineTo(52,72);mark.lineTo(79,43);p.drawPath(mark);}
        else if(state_==1){p.drawLine(58,37,58,62);p.drawPoint(58,77);}
        else {p.drawLine(43,43,73,73);p.drawLine(73,43,43,73);}
    }
private:int state_;
};

class ActivityBadge final : public QWidget {
public:
    ActivityBadge(const QString& title,bool active,QWidget* parent=nullptr):QWidget(parent),title_(title),active_(active) {
        setFixedHeight(25);setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        setAccessibleName(title);
    }
    QSize sizeHint() const override {
        QFont f=font();f.setPixelSize(11);f.setWeight(QFont::DemiBold);
        return QSize(QFontMetrics(f).horizontalAdvance(title_)+31,25);
    }
    QSize minimumSizeHint() const override {return sizeHint();}
protected:
    void paintEvent(QPaintEvent*) override {
        sn::CpuScope snCpuScope;
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient fill(0,0,0,height());
        fill.setColorAt(0,active_?QColor("#10382d"):QColor("#202936"));
        fill.setColorAt(1,active_?QColor("#09221d"):QColor("#151e29"));
        p.setBrush(fill);p.setPen(QPen(active_?QColor("#237b59"):QColor("#405065"),1));
        p.drawRoundedRect(QRectF(0.5,0.5,width()-1,height()-1),12,12);
        const QColor dot=active_?QColor("#00e895"):QColor("#8b9ab1");
        QColor glow=dot;glow.setAlpha(70);
        QRadialGradient halo(QPointF(12,height()/2.0),7);
        halo.setColorAt(0,glow);halo.setColorAt(1,Qt::transparent);
        p.setPen(Qt::NoPen);p.setBrush(halo);p.drawEllipse(QPointF(12,height()/2.0),7,7);
        p.setBrush(dot);p.drawEllipse(QPointF(12,height()/2.0),3.2,3.2);
        QFont f=font();f.setPixelSize(11);f.setWeight(QFont::DemiBold);p.setFont(f);
        p.setPen(active_?QColor("#b7f4d8"):QColor("#b7c1d4"));
        p.drawText(QRect(23,0,width()-28,height()),Qt::AlignVCenter|Qt::AlignLeft,title_);
    }
private:QString title_;bool active_;
};

class ReadyStatus final : public QWidget {
public:
    explicit ReadyStatus(const QString& text,QWidget* parent=nullptr):QWidget(parent),text_(text) {
        setAccessibleName(text);setFixedHeight(18);
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        blink_.setParent(this);blink_.setObjectName("ReadyBlinkTimer");blink_.setSingleShot(true);
        connect(&blink_,&QTimer::timeout,this,[this] {
            dim_=!dim_;update();
            blink_.start(dim_?310:2770);
        });
    }
    QSize sizeHint() const override {return QSize(fontMetrics().horizontalAdvance(text_)+15,18);}
protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);dim_=false;blink_.start(2770);
    }
    void hideEvent(QHideEvent* event) override {
        blink_.stop();dim_=false;QWidget::hideEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        sn::CpuScope snCpuScope;
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        QColor green("#00df8c");green.setAlpha(dim_?100:255);
        p.setPen(Qt::NoPen);p.setBrush(green);p.drawEllipse(QPointF(4,height()/2.0),3,3);
        QColor textColor("#9baccc");textColor.setAlpha(dim_?175:255);
        p.setPen(textColor);p.setFont(font());
        p.drawText(QRect(13,0,std::max(0,width()-13),height()),Qt::AlignVCenter|Qt::AlignLeft,text_);
    }
private:
    QString text_;QTimer blink_;bool dim_=false;
};
