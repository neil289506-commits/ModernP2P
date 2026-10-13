#include <QTemporaryDir>
#include <QtTest>

#include "core/FileNames.h"

using namespace p2p::core;

class TstFileNames : public QObject
{
    Q_OBJECT
private slots:
    void stripsDirectories()
    {
        QCOMPARE(sanitizeFileName("..\\..\\Windows\\evil.dll"), QString("evil.dll"));
        QCOMPARE(sanitizeFileName("../../etc/passwd"), QString("passwd"));
        QCOMPARE(sanitizeFileName("C:\\x\\y.txt"), QString("y.txt"));
    }

    void replacesIllegalCharacters()
    {
        QCOMPARE(sanitizeFileName("a<b>c:d\"e|f?g*h.txt"), QString("a_b_c_d_e_f_g_h.txt"));
    }

    void avoidsReservedNamesAndTrailingDots()
    {
        QCOMPARE(sanitizeFileName("CON"), QString("_CON"));
        QCOMPARE(sanitizeFileName("nul.txt"), QString("_nul.txt"));
        QCOMPARE(sanitizeFileName("name. . "), QString("name"));
        QCOMPARE(sanitizeFileName(".."), QString("file"));
        QCOMPARE(sanitizeFileName(""), QString("file"));
    }

    void limitsLengthKeepingExtension()
    {
        const QString out = sanitizeFileName(QString(300, 'a') + ".png");
        QCOMPARE(out.size(), 200);
        QVERIFY(out.endsWith(".png"));
    }

    void uniquePathAvoidsCollisions()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
        QCOMPARE(uniqueFilePath(dir, "a.txt"), dir.filePath("a.txt"));

        QFile(dir.filePath("a.txt")).open(QIODevice::WriteOnly);
        QCOMPARE(uniqueFilePath(dir, "a.txt"), dir.filePath("a (1).txt"));

        QFile(dir.filePath("a (1).txt.part")).open(QIODevice::WriteOnly);
        QFile(dir.filePath("a (1).txt")).open(QIODevice::WriteOnly);
        QCOMPARE(uniqueFilePath(dir, "a.txt"), dir.filePath("a (2).txt"));
    }
};

QTEST_APPLESS_MAIN(TstFileNames)
#include "tst_filenames.moc"
