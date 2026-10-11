#include <QSet>
#include <QtTest>

#include "core/CodeGenerator.h"

using p2p::core::CodeGenerator;

class TstCodeGenerator : public QObject
{
    Q_OBJECT
private slots:
    void generatesValidCodes()
    {
        for (int i = 0; i < 1000; ++i) {
            const QString code = CodeGenerator::generate();
            QCOMPARE(code.size(), 8);
            QVERIFY2(CodeGenerator::isValid(code), qPrintable(code));
        }
    }

    void codesVary()
    {
        QSet<QString> seen;
        for (int i = 0; i < 200; ++i)
            seen.insert(CodeGenerator::generate());
        QVERIFY(seen.size() > 190);
    }

    void rejectsInvalid()
    {
        QVERIFY(!CodeGenerator::isValid(""));
        QVERIFY(!CodeGenerator::isValid("ABC123"));
        QVERIFY(!CodeGenerator::isValid("ABCDEFG!"));
        QVERIFY(!CodeGenerator::isValid("abcdefgh"));
        QVERIFY(!CodeGenerator::isValid("ABCDEFGHI"));
    }

    void normalizesInput() { QCOMPARE(CodeGenerator::normalize("  ab12cd34 "), QString("AB12CD34")); }

    void peerIdMatchesWorkerFormat()
    {
        const QString id = CodeGenerator::makePeerId("AB12CD34");
        QVERIFY(QRegularExpression("^mp2p-AB12CD34-[a-z0-9]{12}$").match(id).hasMatch());
    }
};

QTEST_APPLESS_MAIN(TstCodeGenerator)
#include "tst_codegenerator.moc"
