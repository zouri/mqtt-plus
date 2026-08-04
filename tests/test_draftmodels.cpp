#include "models/draftfiltermodel.h"
#include "models/draftlibrarymodel.h"

#include <QtTest/QtTest>

class DraftModelsTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesStableRolesAndFiltersSearchText();
    void sortsByMostRecentlyUsed();
};

namespace {
PublishDraft draft(
    const QString &id,
    const QString &name,
    const QString &description,
    const QString &topic,
    const QString &lastUsedAt)
{
    PublishDraft result;
    result.id = id;
    result.name = name;
    result.description = description;
    result.defaultTopic = topic;
    result.payload = QStringLiteral("payload-%1").arg(id);
    result.formatId = QStringLiteral("text");
    result.lastUsedAt = lastUsedAt;
    return result;
}
}

void DraftModelsTest::exposesStableRolesAndFiltersSearchText()
{
    DraftLibraryModel source;
    source.setDrafts({
        draft(QStringLiteral("a"), QStringLiteral("Alpha"), QStringLiteral("Kitchen light"), QStringLiteral("home/light"), QString()),
        draft(QStringLiteral("b"), QStringLiteral("Beta"), QStringLiteral("Factory reset"), QStringLiteral("devices/reset"), QString()),
    });
    QCOMPARE(source.count(), 2);
    QCOMPARE(source.rowAt(0).value(QStringLiteral("formatId")).toString(), QStringLiteral("text"));
    QCOMPARE(source.rowAt(0).value(QStringLiteral("formatName")).toString(), QStringLiteral("Plaintext"));

    DraftFilterModel filter;
    filter.setSourceModel(&source);
    filter.setFilterText(QStringLiteral("RESET"));
    QCOMPARE(filter.count(), 1);
    QCOMPARE(filter.rowAt(0).value(QStringLiteral("id")).toString(), QStringLiteral("b"));
}

void DraftModelsTest::sortsByMostRecentlyUsed()
{
    DraftLibraryModel source;
    source.setDrafts({
        draft(QStringLiteral("old"), QStringLiteral("Old"), QString(), QString(), QStringLiteral("2026-08-01T00:00:00.000")),
        draft(QStringLiteral("never"), QStringLiteral("Never"), QString(), QString(), QString()),
        draft(QStringLiteral("new"), QStringLiteral("New"), QString(), QString(), QStringLiteral("2026-08-03T00:00:00.000")),
    });

    DraftFilterModel filter;
    filter.setSourceModel(&source);
    filter.setSortMode(QStringLiteral("recent"));
    QCOMPARE(filter.rowAt(0).value(QStringLiteral("id")).toString(), QStringLiteral("new"));
    QCOMPARE(filter.rowAt(1).value(QStringLiteral("id")).toString(), QStringLiteral("old"));
    QCOMPARE(filter.rowAt(2).value(QStringLiteral("id")).toString(), QStringLiteral("never"));
}

QTEST_MAIN(DraftModelsTest)

#include "test_draftmodels.moc"
