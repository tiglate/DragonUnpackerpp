// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/DesktopScheme.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>

#include <spdlog/spdlog.h>

namespace Dup::Ui
{
namespace
{
const char *PORTAL_SERVICE = "org.freedesktop.portal.Desktop";
const char *PORTAL_PATH = "/org/freedesktop/portal/desktop";
const char *PORTAL_INTERFACE = "org.freedesktop.portal.Settings";

// color-scheme: 1 prefer dark, 2 prefer light, 0 no preference
auto DarkOfScheme(QVariant value) -> std::optional<bool>
{
    if (value.canConvert<QDBusVariant>())
    {
        value = value.value<QDBusVariant>().variant(); // Read wraps the value in one more variant
    }
    bool ok = false;
    const uint scheme = value.toUInt(&ok);
    if (!ok || (scheme != 1 && scheme != 2))
    {
        return std::nullopt;
    }
    return scheme == 1;
}
} // namespace

/// Receives the portal's SettingChanged signal (QDBusConnection::connect takes a slot, not a lambda)
class PortalListener : public QObject
{
    Q_OBJECT
  public:
    PortalListener(std::function<void(std::optional<bool>)> onChange, QObject *parent)
        : QObject(parent), _onChange(std::move(onChange))
    {
    }

  public slots:
    void OnSettingChanged(const QString &group, const QString &key, const QDBusVariant &value)
    {
        if (group == "org.freedesktop.appearance" && key == "color-scheme")
        {
            _onChange(DarkOfScheme(value.variant()));
        }
    }

  private:
    std::function<void(std::optional<bool>)> _onChange;
};

auto WatchDesktopScheme(std::function<void(std::optional<bool>)> onChange) -> std::optional<bool>
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
    {
        return std::nullopt;
    }
    auto message = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INTERFACE, "Read");
    message << QString("org.freedesktop.appearance") << QString("color-scheme");
    // a short timeout: a session without the portal must not hold the start of the program
    const QDBusReply<QDBusVariant> reply = bus.call(message, QDBus::Block, 500);
    if (!reply.isValid())
    {
        spdlog::debug("no desktop colour scheme from the settings portal: {}", reply.error().message().toStdString());
        return std::nullopt;
    }
    auto *listener = new PortalListener(std::move(onChange), QCoreApplication::instance());
    if (!bus.connect(PORTAL_SERVICE, PORTAL_PATH, PORTAL_INTERFACE, "SettingChanged", listener,
                     SLOT(OnSettingChanged(QString, QString, QDBusVariant))))
    {
        spdlog::warn("the theme will not follow the changes of the desktop: {}", bus.lastError().message().toStdString());
    }
    return DarkOfScheme(reply.value().variant());
}
} // namespace Dup::Ui

#include "DesktopScheme.moc"
