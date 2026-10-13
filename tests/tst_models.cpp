#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "models/RequestsModel.h"
#include "models/StagingModel.h"
#include "models/TransferModel.h"

using namespace p2p::models;

class TstModels : public QObject
{
    Q_OBJECT
private slots:
    void stagingAddsFilesOnly()
    {
        QTemporaryDir tmp;
        QFile f(tmp.filePath("a.bin"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(100, 'x'));
        f.close();

        StagingModel m;
        QCOMPARE(m.addPaths({f.fileName(), tmp.path(), tmp.filePath("missing")}), 2); // folder + missing
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.totalSize(), 100);
        QCOMPARE(m.addPaths({f.fileName()}), 1); // duplicate
        QCOMPARE(m.toJson().first().toObject().value("name").toString(), QString("a.bin"));
        m.removeAt(0);
        QCOMPARE(m.count(), 0);
    }

    void requestsDeduplicateAndTake()
    {
        RequestsModel m;
        const QJsonObject r{{"id", "abc"}, {"from", "AB12CD34"},
                            {"files", QJsonArray{QJsonObject{{"name", "x"}, {"size", 5}},
                                                 QJsonObject{{"name", "y"}, {"size", 7}}}}};
        QVERIFY(m.add(r));
        QVERIFY(!m.add(r));
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.data(m.index(0), RequestsModel::TotalSizeRole).toLongLong(), 12);
        QCOMPARE(m.data(m.index(0), RequestsModel::FileCountRole).toInt(), 2);
        QCOMPARE(m.take("abc").value("from").toString(), QString("AB12CD34"));
        QVERIFY(m.take("abc").isEmpty());
        QCOMPARE(m.count(), 0);
    }

    void transfersTrackActiveCount()
    {
        TransferModel m;
        QSignalSpy spy(&m, &TransferModel::activeCountChanged);
        const QString id = m.add("f.txt", 10, true, "AB12CD34");
        QCOMPARE(m.activeCount(), 1);
        m.update(id, 5, TransferModel::State::Transferring, 1.0);
        QVERIFY(m.isActive(id));
        m.update(id, 10, TransferModel::State::Done);
        QCOMPARE(m.activeCount(), 0);
        QVERIFY(!m.isActive(id));
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_APPLESS_MAIN(TstModels)
#include "tst_models.moc"
