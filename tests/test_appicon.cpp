#include "app/appicon.h"

#include <QtTest/QtTest>

#include <QImage>

class AppIconTest : public QObject
{
    Q_OBJECT

private slots:
    void usesThemeSwatchColors();
    void fallsBackToMint();
    void recolorsOnlyThemeChannels();
};

void AppIconTest::usesThemeSwatchColors()
{
    QCOMPARE(AppIcon::themeColor(QStringLiteral("mint")), QColor(QStringLiteral("#35d0aa")));
    QCOMPARE(AppIcon::themeColor(QStringLiteral("blue")), QColor(QStringLiteral("#6aa3ff")));
    QCOMPARE(AppIcon::themeColor(QStringLiteral("violet")), QColor(QStringLiteral("#ad8cff")));
    QCOMPARE(AppIcon::themeColor(QStringLiteral("amber")), QColor(QStringLiteral("#f1b86a")));
    QCOMPARE(AppIcon::themeColor(QStringLiteral("rose")), QColor(QStringLiteral("#ff879d")));
}

void AppIconTest::fallsBackToMint()
{
    QCOMPARE(AppIcon::themeColor(QStringLiteral("unknown")), QColor(QStringLiteral("#35d0aa")));
}

void AppIconTest::recolorsOnlyThemeChannels()
{
    const QImage base(QStringLiteral(":/assets/icons/app-icon.png"));
    const QImage mask(QStringLiteral(":/assets/icons/app-icon-theme-mask.png"));
    const QImage themed = AppIcon::themed(QStringLiteral("rose"))
                              .pixmap(base.size())
                              .toImage()
                              .convertToFormat(QImage::Format_RGBA8888);
    QVERIFY(!base.isNull());
    QVERIFY(!mask.isNull());
    QCOMPARE(themed.size(), base.size());

    const QRgb themePixel = QColor(QStringLiteral("#ff879d")).rgba();
    qsizetype themedPixelCount = 0;
    qsizetype unchangedWhitePixelCount = 0;
    for (int y = 0; y < base.height(); ++y) {
        for (int x = 0; x < base.width(); ++x) {
            const QColor basePixel = base.pixelColor(x, y);
            if (mask.pixelColor(x, y).alpha() == 255 && basePixel == QColorConstants::White) {
                QCOMPARE(themed.pixel(x, y), themePixel);
                ++themedPixelCount;
            } else if (mask.pixelColor(x, y).alpha() == 0
                       && basePixel == QColorConstants::White) {
                QCOMPARE(themed.pixelColor(x, y), QColorConstants::White);
                ++unchangedWhitePixelCount;
            }
        }
    }
    QVERIFY(themedPixelCount > 1000);
    QVERIFY(unchangedWhitePixelCount > 1000);
}

QTEST_MAIN(AppIconTest)

#include "test_appicon.moc"
