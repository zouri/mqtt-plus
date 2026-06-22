#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>

class ScriptController;
class ScriptLibraryModel;
class ScriptTestSamplesModel;
class SessionController;

class ScriptLibraryFacade : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScriptLibraryModel* scripts READ scripts CONSTANT)
    Q_PROPERTY(ScriptTestSamplesModel* scriptTestSamples READ scriptTestSamples CONSTANT)

public:
    struct Dependencies
    {
        ScriptLibraryModel *scriptsModel = nullptr;
        ScriptTestSamplesModel *scriptTestSamplesModel = nullptr;
        ScriptController *scriptController = nullptr;
        SessionController *sessionController = nullptr;
        std::function<void()> refreshScriptsModel;
        std::function<bool()> saveSessions;
        std::function<void()> notifyCurrentSessionAndSubscriptionsChanged;
        std::function<void()> notifySessionAndSubscriptionViewsChanged;
        std::function<void()> emitScriptLibraryChanged;
    };

    explicit ScriptLibraryFacade(Dependencies dependencies, QObject *parent = nullptr);

    ScriptLibraryModel *scripts();
    ScriptTestSamplesModel *scriptTestSamples();

    Q_INVOKABLE QString upsertScript(
        const QString &id,
        const QString &name,
        const QString &description,
        const QString &code);
    Q_INVOKABLE bool deleteScript(const QString &id);
    Q_INVOKABLE QVariantMap testScript(
        const QString &code,
        const QString &topic,
        const QString &payload,
        int format = 0) const;

signals:
    void scriptLibraryChanged();
    void scriptTestSamplesChanged();

private:
    Dependencies m_dependencies;
};
