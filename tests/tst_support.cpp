#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "core/Logger.h"
#include "core/Version.h"
#include "models/BlockListModel.h"
#include "models/HistoryModel.h"
#include "models/StagingModel.h"

using namespace p2p;

class TstSupport : public QObject
{
    Q_OBJECT
private slots:
    void versionComparison()
    {
        QVERIFY(core::isNewerVersion("v1.2.0", "1.1.9"));
        QVERIFY(core::isNewerVersion("2.0", "1.9.9"));
        QVERIFY(core::isNewerVersion("v0.10.0", "0.9.0"));      // numeric, not lexicographic
        QVERIFY(!core::isNewerVersion("1.2.0", "1.2.0"));
        QVERIFY(!core::isNewerVersion("1.2", "1.2.0"));
        QVERIFY(!core::isNewerVersion("1.1.0", "1.2.0"));
        QVERIFY(core::isNewerVersion("v1.3.0-beta", "1.2.0"));  // suffix ignored
        QVERIFY(!core::isNewerVersion("garbage", "1.0.0"));
        QVERIFY(!core::isNewerVersion("1.0.0", ""));
    }

    void historyPersistsAndCaps()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("history.json");
        {
            models::HistoryModel h(file);
            models::HistoryModel::Entry e;
            e.name = "a.txt"; e.peer = "AB12CD34"; e.result = "Done"; e.size = 42; e.sending = true;
            e.when = QDateTime::currentDateTime();
            h.add(e);
            e.name = "b.txt"; e.sending = false; e.path = "C:/x/b.txt";
            h.add(e);
            QCOMPARE(h.count(), 2);
            QCOMPARE(h.data(h.index(0), models::HistoryModel::NameRole).toString(), QString("b.txt"));  // newest first
        }   // destructor flushes
        models::HistoryModel again(file);
        QCOMPARE(again.count(), 2);
        QCOMPARE(again.data(again.index(1), models::HistoryModel::SizeRole).toLongLong(), 42);
        QCOMPARE(again.data(again.index(0), models::HistoryModel::PathRole).toString(), QString("C:/x/b.txt"));

        for (int i = 0; i < models::HistoryModel::kMaxEntries + 20; ++i) {
            models::HistoryModel::Entry e;
            e.name = QString::number(i); e.when = QDateTime::currentDateTime();
            again.add(e);
        }
        QCOMPARE(again.count(), models::HistoryModel::kMaxEntries);
        again.clear();
        again.flush();
        QCOMPARE(models::HistoryModel(file).count(), 0);
    }

    void historyToleratesCorruptFile()
    {
        QTemporaryDir dir;
        QFile f(dir.filePath("history.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{not json");
        f.close();
        models::HistoryModel h(f.fileName());
        QCOMPARE(h.count(), 0);
    }

    void blockListPersists()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("blocked.json");
        {
            models::BlockListModel b(file);
            QVERIFY(b.add("AB12CD34"));
            QVERIFY(!b.add("AB12CD34"));    // no duplicates
            QVERIFY(b.add("ZZ99YY88"));
            QVERIFY(b.contains("AB12CD34"));
        }
        models::BlockListModel again(file);
        QCOMPARE(again.count(), 2);
        QVERIFY(again.contains("ZZ99YY88"));
        again.remove("AB12CD34");
        QVERIFY(!again.contains("AB12CD34"));
        QCOMPARE(models::BlockListModel(file).count(), 1);
    }

    void loggerWritesAndRotates()
    {
        QTemporaryDir dir;
        core::Logger::install(dir.path());
        qWarning() << "hello from the test";
        const QString big(2000, 'x');
        for (int i = 0; i < 1500; ++i)          // ~3 MB in total
            qInfo().noquote() << big;
        core::Logger::uninstall();

        const QDir d(dir.path());
        QVERIFY(d.exists("app.log"));
        QVERIFY(d.exists("app.1.log"));
        QVERIFY(!d.exists("app.4.log"));
        QVERIFY(QFileInfo(d.filePath("app.log")).size() <= core::Logger::kMaxBytes + 4096);
    }

    void stagingExpandsFolders()
    {
        QTemporaryDir dir;
        QDir root(dir.path());
        root.mkpath("Album/sub");
        for (const char *rel : {"Album/a.txt", "Album/sub/b.txt"}) {
            QFile f(root.filePath(rel));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("data");
        }
        models::StagingModel m;
        QCOMPARE(m.addPaths({root.filePath("Album")}), 0);
        QCOMPARE(m.count(), 2);
        QStringList names = m.names();
        names.sort();
        QCOMPARE(names, (QStringList{"Album/a.txt", "Album/sub/b.txt"}));
        QCOMPARE(m.addPaths({root.filePath("Album")}), 2);      // duplicates skipped
        QCOMPARE(m.count(), 2);
    }
};

QTEST_MAIN(TstSupport)
#include "tst_support.moc"
