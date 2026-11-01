#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include <QPointer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include <QAction>
#include <QSslSocket>
#include <util/bmem.h>
#include "dock.hpp"
#include "resources.hpp"
#include "version.hpp"

extern "C" __declspec(dllexport) bool stream_notify_telemetry_v1(uint64_t* sent,uint64_t* received){
    if(!sent||!received)return false;
    const auto totals=sn::Meter::instance().totals();
    *sent=totals.tx;
    *received=totals.rx;
    return true;
}
extern "C" __declspec(dllexport) bool stream_notify_telemetry_v2(uint64_t* sent,uint64_t* received,uint64_t* cpuNs,uint64_t* dataBytes){
    if(!sent||!received||!cpuNs||!dataBytes)return false;
    const auto totals=sn::Meter::instance().totals();
    *sent=totals.tx;*received=totals.rx;*cpuNs=totals.cpuNs;
    *dataBytes=sn::measuredDataBytes.load(std::memory_order_relaxed);
    return true;
}

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("DMITRII")

namespace {
QPointer<StreamDock> g_panel;
bool g_registered = false;
bool g_exiting = false;
QString g_qtPluginPath;
QString dockVisibilityPath(){char* path=obs_module_config_path("dock-visibility.ini");if(!path)return {};const QString value=QString::fromUtf8(path);bfree(path);return value;}
void saveDockVisibility(){
    auto* window=static_cast<QMainWindow*>(obs_frontend_get_main_window());
    auto* dock=window?window->findChild<QDockWidget*>("stream-notify-dock"):nullptr;
    const auto path=dockVisibilityPath();
    if(dock&&!path.isEmpty()){QSettings saved(path,QSettings::IniFormat);saved.setValue("visible",dock->toggleViewAction()->isChecked());saved.sync();}
}

void installDock()
{
    if (g_exiting || g_panel) return;
    auto* window = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if(!window) return;
    auto* panel = new StreamDock;
    if (!obs_frontend_add_dock_by_id("stream-notify-dock", "OZK Notify", panel)) {
        delete panel;
        blog(LOG_ERROR,"[OZK Notify] Could not register dock");
        return;
    }
    g_panel = panel;
    // OBS stores geometry. Its dock registration starts hidden, so persist the
    // user's visibility preference separately and restore it after registration.
    auto* dock = window->findChild<QDockWidget*>("stream-notify-dock");
    if(dock) {
        const auto path=dockVisibilityPath();
        QTimer::singleShot(0,dock,[window,dock,path]{
            if(path.isEmpty())return;
            QSettings saved(path,QSettings::IniFormat);
            const bool first=!saved.contains("visible");
            if(first){dock->setFloating(false);window->addDockWidget(Qt::RightDockWidgetArea,dock);}
            dock->setVisible(saved.value("visible",true).toBool());
            QObject::connect(dock->toggleViewAction(),&QAction::toggled,dock,[path,window](bool visible){
                if(path.isEmpty()||!window->isVisible()||window->isMinimized())return;
                QSettings state(path,QSettings::IniFormat);state.setValue("visible",visible);state.sync();
            });
        });
    }
}
void onFrontend(enum obs_frontend_event event, void*)
{
    if(g_exiting) return;
    switch(event) {
        case OBS_FRONTEND_EVENT_FINISHED_LOADING: installDock(); break;
        case OBS_FRONTEND_EVENT_STREAMING_STARTED:
            if(g_panel) g_panel->refreshStreamingState();
            break;
        case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
        case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
            if(g_panel) g_panel->refreshStreamingState();
            break;
        case OBS_FRONTEND_EVENT_EXIT:
            saveDockVisibility();
            g_exiting = true;
            if(g_panel) g_panel->shutdown();
            break;
        default: break;
    }
}
}

MODULE_EXPORT const char* obs_module_name(void) {return "OZK Notify";}
MODULE_EXPORT const char* obs_module_description(void)
{return "Telegram and Discord stream start notifications with a native OBS dock";}

bool obs_module_load(void)
{
    g_exiting = false;
    char* tlsDirectory=obs_module_file("qt-plugins");
    if(tlsDirectory) {
        g_qtPluginPath=QString::fromUtf8(tlsDirectory);
        bfree(tlsDirectory);
        QCoreApplication::addLibraryPath(g_qtPluginPath);
    }
    if(!QSslSocket::supportsSsl() && QSslSocket::availableBackends().contains("schannel"))
        QSslSocket::setActiveBackend("schannel");
    blog(LOG_INFO,"[OZK Notify] HTTPS support: %s; TLS backend: %s",
        QSslSocket::supportsSsl()?"available":"unavailable",
        QSslSocket::activeBackend().toUtf8().constData());
    obs_frontend_add_event_callback(onFrontend, nullptr);
    g_registered = true;
    blog(LOG_INFO, "[OZK Notify] Version %s (%s) loaded; waiting for frontend initialization",STREAM_NOTIFY_VERSION,STREAM_NOTIFY_BUILD);
    return true;
}
void obs_module_unload(void)
{
    g_exiting = true;
    if(g_registered) {
        obs_frontend_remove_event_callback(onFrontend, nullptr);
        g_registered = false;
    }
    if(g_panel) {
        g_panel->shutdown();
        obs_frontend_remove_dock("stream-notify-dock");
        g_panel = nullptr;
    }
    if(!g_qtPluginPath.isEmpty()) {
        QCoreApplication::removeLibraryPath(g_qtPluginPath);
        g_qtPluginPath.clear();
    }
}
