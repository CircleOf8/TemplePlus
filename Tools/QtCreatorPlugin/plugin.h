
#pragma once

#include <QQmlExtensionPlugin>

class TioIntegration : public QQmlExtensionPlugin {
Q_OBJECT
Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:

    void initializeEngine(QQmlEngine *engine, const char *uri) override;

    void registerTypes(const char *uri) override;

};
