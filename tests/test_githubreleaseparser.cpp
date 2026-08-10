#include "services/update/githubreleaseparser.h"

#include <QtTest/QtTest>

class GitHubReleaseParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesMatchingMacAsset();
    void fallsBackToReleasePageWhenAssetIsMissing();
    void rejectsInvalidPayloads();
};

void GitHubReleaseParserTest::parsesMatchingMacAsset()
{
    const QByteArray payload = QByteArrayLiteral(
        "{\"tag_name\":\"v0.2.0\","
        "\"name\":\"MQTT Plus 0.2.0\","
        "\"body\":\"Changes\","
        "\"html_url\":\"https://github.com/zouri/mqtt-plus/releases/tag/v0.2.0\","
        "\"assets\":["
        "{\"name\":\"mqtt-plus-0.2.0-macos-x86_64.dmg\","
        "\"browser_download_url\":\"https://example.com/intel.dmg\"},"
        "{\"name\":\"mqtt-plus-0.2.0-macos-arm64.dmg\","
        "\"browser_download_url\":\"https://example.com/arm.dmg\"}]}"
    );

    const auto result = parseGitHubLatestRelease(payload, QStringLiteral("-macos-arm64.dmg"));
    const QUrl expectedDownloadUrl(QStringLiteral("https://example.com/arm.dmg"));

    QVERIFY2(result.release.has_value(), qPrintable(result.error));
    QCOMPARE(result.release->version, QStringLiteral("0.2.0"));
    QCOMPARE(result.release->downloadUrl, expectedDownloadUrl);
}

void GitHubReleaseParserTest::fallsBackToReleasePageWhenAssetIsMissing()
{
    const QByteArray payload = QByteArrayLiteral(
        "{\"tag_name\":\"1.0.0\","
        "\"html_url\":\"https://github.com/zouri/mqtt-plus/releases/tag/v1.0.0\","
        "\"assets\":[]}"
    );

    const auto result = parseGitHubLatestRelease(payload, QStringLiteral("-macos-arm64.dmg"));
    const QUrl expectedReleaseUrl(
        QStringLiteral("https://github.com/zouri/mqtt-plus/releases/tag/v1.0.0"));

    QVERIFY2(result.release.has_value(), qPrintable(result.error));
    QVERIFY(result.release->downloadUrl.isEmpty());
    QCOMPARE(result.release->releasePageUrl, expectedReleaseUrl);
}

void GitHubReleaseParserTest::rejectsInvalidPayloads()
{
    QVERIFY(!parseGitHubLatestRelease(QByteArrayLiteral("[]"), {}).release);
    QVERIFY(!parseGitHubLatestRelease(
        QByteArrayLiteral("{\"tag_name\":\"beta\",\"html_url\":\"https://example.com\"}"),
        {}).release);
    QVERIFY(!parseGitHubLatestRelease(
        QByteArrayLiteral("{\"tag_name\":\"1.0.0\",\"html_url\":\"http://example.com\"}"),
        {}).release);
}

QTEST_MAIN(GitHubReleaseParserTest)
#include "test_githubreleaseparser.moc"
