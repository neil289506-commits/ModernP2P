#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "LoopbackTransport.h"
#include "models/TransferModel.h"
#include "transfer/TransferEngine.h"

using namespace p2p;
using State = models::TransferModel::State;

namespace {
const QString kPeerA = QStringLiteral("mp2p-AAAAAAAA-aaaaaaaaaaaa");
const QString kPeerB = QStringLiteral("mp2p-BBBBBBBB-bbbbbbbbbbbb");

QByteArray randomBytes(qint64 n)
{
    QByteArray b(n, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32 *>(b.data()), n / 4);
    return b;
}

QString writeFile(const QDir &dir, const QString &rel, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(dir.filePath(rel)).absolutePath());
    QFile f(dir.filePath(rel));
    f.open(QIODevice::WriteOnly);
    f.write(data);
    return f.fileName();
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    return f.readAll();
}

QJsonObject parse(const QString &s) { return QJsonDocument::fromJson(s.toUtf8()).object(); }
QString dump(const QJsonObject &o) { return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)); }

QString rowState(const models::TransferModel &m, int row)
{
    return m.data(m.index(row), models::TransferModel::StateRole).toString();
}
QString rowError(const models::TransferModel &m, int row)
{
    return m.data(m.index(row), models::TransferModel::ErrorRole).toString();
}
} // namespace

/// Runs a sender engine and a receiver engine against each other in-process.
class TstTransferEngine : public QObject
{
    Q_OBJECT

    struct Rig
    {
        QTemporaryDir src, dst;
        LoopbackTransport ta{kPeerA}, tb{kPeerB};
        models::TransferModel ma, mb;
        transfer::TransferEngine a{&ta, &ma}, b{&tb, &mb};
        Rig() { LoopbackTransport::pair(ta, tb); b.setDownloadDir(dst.path()); }

        void prepare(const QStringList &paths, const QStringList &names, const QList<qint64> &sizes,
                     const QList<QString> &announced)
        {
            QList<transfer::FileSpec> specs;
            for (int i = 0; i < announced.size(); ++i)
                specs.append({announced.at(i), sizes.at(i)});
            b.expectIncoming(QStringLiteral("req1"), QStringLiteral("AAAAAAAA"), kPeerA, specs);
            a.startSend(QStringLiteral("req1"), kPeerB, QStringLiteral("BBBBBBBB"), paths, names);
        }
        bool wait(int ms = 15000)
        {
            QSignalSpy done(&a, &transfer::TransferEngine::sessionFinished);
            return done.wait(ms) || done.count() > 0;
        }
    };

private slots:
    void singleFileRoundTrip()
    {
        Rig r;
        const QByteArray data = randomBytes(300'001);   // not a multiple of the chunk size
        const QString path = writeFile(r.src.path(), "a.bin", data);
        r.prepare({path}, {"a.bin"}, {data.size()}, {"a.bin"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Done"));
        QCOMPARE(rowState(r.ma, 0), QString("Done"));
        QCOMPARE(readFile(QDir(r.dst.path()).filePath("a.bin")), data);
        QVERIFY(!QFileInfo::exists(QDir(r.dst.path()).filePath("a.bin.part")));
        QCOMPARE(r.mb.data(r.mb.index(0), models::TransferModel::PathRole).toString(),
                 QDir(r.dst.path()).filePath("a.bin"));
    }

    void emptyFile()
    {
        Rig r;
        const QString path = writeFile(r.src.path(), "empty.txt", {});
        r.prepare({path}, {"empty.txt"}, {0}, {"empty.txt"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Done"));
        QVERIFY(QFileInfo::exists(QDir(r.dst.path()).filePath("empty.txt")));
    }

    void folderKeepsStructureAndAvoidsMerging()
    {
        // Two sequential transfers of "Album" into one download folder must not merge.
        QTemporaryDir src, dst;
        const QByteArray d1 = randomBytes(1000), d2 = randomBytes(70'000);
        const QString p1 = writeFile(src.path(), "Album/a.txt", d1);
        const QString p2 = writeFile(src.path(), "Album/sub/b.bin", d2);
        for (int i = 0; i < 2; ++i) {
            LoopbackTransport ta{kPeerA}, tb{kPeerB};
            LoopbackTransport::pair(ta, tb);
            models::TransferModel ma, mb;
            transfer::TransferEngine a{&ta, &ma}, b{&tb, &mb};
            b.setDownloadDir(dst.path());
            b.expectIncoming("req", "AAAAAAAA", kPeerA, {{"Album/a.txt", d1.size()}, {"Album/sub/b.bin", d2.size()}});
            QSignalSpy done(&a, &transfer::TransferEngine::sessionFinished);
            a.startSend("req", kPeerB, "BBBBBBBB", {p1, p2}, {"Album/a.txt", "Album/sub/b.bin"});
            QVERIFY(done.wait(15000));
            QTRY_COMPARE(mb.activeCount(), 0);
        }
        const QDir d(dst.path());
        QCOMPARE(readFile(d.filePath("Album/a.txt")), d1);
        QCOMPARE(readFile(d.filePath("Album/sub/b.bin")), d2);
        QCOMPARE(readFile(d.filePath("Album (1)/a.txt")), d1);
        QCOMPARE(readFile(d.filePath("Album (1)/sub/b.bin")), d2);
    }

    void corruptedChunkIsRejected()
    {
        Rig r;
        const QByteArray data = randomBytes(200'000);
        const QString path = writeFile(r.src.path(), "a.bin", data);
        int chunks = 0;
        r.ta.tamper = [&](const QString &msg) {
            QJsonObject o = parse(msg);
            if (o.value("t").toString() == "chunk" && ++chunks == 2) {
                QByteArray raw = QByteArray::fromBase64(o.value("d").toString().toLatin1());
                raw[0] = raw[0] ^ 0xFF;
                o["d"] = QString::fromLatin1(raw.toBase64());
                return dump(o);
            }
            return msg;
        };
        r.prepare({path}, {"a.bin"}, {data.size()}, {"a.bin"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Failed"));
        QCOMPARE(rowError(r.mb, 0), QString("Checksum mismatch"));
        QTRY_COMPARE(rowState(r.ma, 0), QString("Failed"));
        QVERIFY(!QFileInfo::exists(QDir(r.dst.path()).filePath("a.bin")));
        QVERIFY(!QFileInfo::exists(QDir(r.dst.path()).filePath("a.bin.part")));
    }

    void sizeMismatchIsRejected()
    {
        Rig r;
        const QByteArray data = randomBytes(5000);
        const QString path = writeFile(r.src.path(), "a.bin", data);
        r.prepare({path}, {"a.bin"}, {data.size() + 1}, {"a.bin"});   // receiver was told a different size
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Failed"));
        QVERIFY(rowError(r.mb, 0).contains("size"));
        QVERIFY(QDir(r.dst.path()).entryList(QDir::Files).isEmpty());
    }

    void moreDataThanAnnouncedIsRejected()
    {
        Rig r;
        const QByteArray data = randomBytes(100'000);
        const QString path = writeFile(r.src.path(), "a.bin", data);
        // Sender lies in the header (claims the size the receiver expects) but sends more.
        r.ta.tamper = [&](const QString &msg) {
            QJsonObject o = parse(msg);
            if (o.value("t").toString() == "file") { o["size"] = 1000; return dump(o); }
            return msg;
        };
        r.prepare({path}, {"a.bin"}, {1000}, {"a.bin"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Failed"));
        QVERIFY(QDir(r.dst.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());
    }

    void announcedNamesCannotEscapeTheDownloadFolder()
    {
        Rig r;
        const QByteArray data = randomBytes(1000);
        const QString path = writeFile(r.src.path(), "x.txt", data);
        const QString evil = "..\\..\\Windows/../../evil.txt";
        r.prepare({path}, {evil}, {data.size()}, {evil});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.mb, 0), QString("Done"));
        const QString saved = r.mb.data(r.mb.index(0), models::TransferModel::PathRole).toString();
        QVERIFY2(QFileInfo(saved).absoluteFilePath().startsWith(QDir(r.dst.path()).absolutePath() + "/"),
                 qPrintable(saved));
        QCOMPARE(readFile(saved), data);
    }

    void strangersAreRejected()
    {
        Rig r;
        const QString path = writeFile(r.src.path(), "a.bin", randomBytes(100));
        // The receiver expects a different peer id than the one connecting.
        r.b.expectIncoming("req1", "AAAAAAAA", "mp2p-ZZZZZZZZ-zzzzzzzzzzzz", {{"a.bin", 100}});
        r.a.startSend("req1", kPeerB, "BBBBBBBB", {path}, {"a.bin"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.ma, 0), QString("Failed"));
        QVERIFY(r.mb.rowCount() == 0);
    }

    void unknownRequestIdIsRejected()
    {
        Rig r;
        const QString path = writeFile(r.src.path(), "a.bin", randomBytes(100));
        r.a.startSend("not-accepted", kPeerB, "BBBBBBBB", {path}, {"a.bin"});
        QVERIFY(r.wait());
        QTRY_COMPARE(rowState(r.ma, 0), QString("Failed"));
        QCOMPARE(r.mb.rowCount(), 0);
    }

    void cancellingStopsBothSidesAndCleansUp()
    {
        Rig r;
        const QByteArray data = randomBytes(8 * 1024 * 1024);
        const QString path = writeFile(r.src.path(), "big.bin", data);
        bool cancelled = false;
        r.ta.tamper = [&](const QString &msg) {
            if (!cancelled && r.ta.messagesSent > 20) {
                cancelled = true;
                QTimer::singleShot(0, &r.a, [&] {
                    r.a.cancel(r.ma.data(r.ma.index(0), models::TransferModel::IdRole).toString());
                });
            }
            return msg;
        };
        r.prepare({path}, {"big.bin"}, {data.size()}, {"big.bin"});
        QVERIFY(r.wait());
        QCOMPARE(rowState(r.ma, 0), QString("Cancelled"));
        QTRY_COMPARE(rowState(r.mb, 0), QString("Failed"));
        QTRY_VERIFY(QDir(r.dst.path()).entryList(QDir::Files | QDir::Hidden).isEmpty());
    }

    void finishedSignalFiresOncePerFile()
    {
        Rig r;
        const QString p1 = writeFile(r.src.path(), "1.txt", randomBytes(10));
        const QString p2 = writeFile(r.src.path(), "2.txt", randomBytes(20));
        QSignalSpy spyB(&r.mb, &models::TransferModel::finished);
        r.prepare({p1, p2}, {"1.txt", "2.txt"}, {10, 20}, {"1.txt", "2.txt"});
        QVERIFY(r.wait());
        QTRY_COMPARE(spyB.count(), 2);
        QCOMPARE(spyB.at(0).at(4).toString(), QString("Done"));
    }
};

QTEST_MAIN(TstTransferEngine)
#include "tst_transfer_engine.moc"
