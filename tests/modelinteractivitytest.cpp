#include <QtTest>

#include <QString>
#include <iostream>

#include "dummyresource/resourcefactory.h"
#include "store.h"
#include "resourcecontrol.h"
#include "commands.h"
#include "resourceconfig.h"
#include "log.h"
#include "modelresult.h"
#include "test.h"

static int blockingTime;

class TimeMeasuringApplication : public QCoreApplication
{
    QElapsedTimer t;

public:
    TimeMeasuringApplication(int &argc, char **argv) : QCoreApplication(argc, argv)
    {
    }
    virtual ~TimeMeasuringApplication()
    {
    }

    virtual bool notify(QObject *receiver, QEvent *event)
    {
        t.start();
        const bool ret = QCoreApplication::notify(receiver, event);
        if (t.elapsed() > 1) {
            std::cout
                << QString("processing event type %1 for object %2 took %3ms").arg((int)event->type()).arg(receiver->metaObject()->className()).arg((int)t.elapsed()).toStdString()
                << std::endl;
        }
        blockingTime += t.elapsed();
        return ret;
    }
};

/**
 * Ensure that queries don't block the system for an extended period of time.
 *
 * This is done by ensuring that the event loop is never blocked.
 */
class ModelinteractivityTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        Sink::Test::initTest();
        ResourceConfig::addResource("sink.dummy.instance1", "sink.dummy");
        Sink::Store::removeDataFromDisk(QByteArray("sink.dummy.instance1")).exec().waitForFinished();
    }

    void cleanup()
    {
        Sink::Store::removeDataFromDisk(QByteArray("sink.dummy.instance1")).exec().waitForFinished();
    }

    void init()
    {
    }

    void testSingle()
    {
        // Setup
        {
            Sink::ApplicationDomain::Mail mail("sink.dummy.instance1");
            for (int i = 0; i < 1000; i++) {
                Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
            }
        }

        Sink::Query query;
        query.resources << "sink.dummy.instance1";
        query.liveQuery = true;

        Sink::ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

        // Test
        QTime time;
        time.start();
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        blockingTime += time.elapsed();
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        // Never block longer than 10 ms
        QVERIFY2(blockingTime < 10, QString("Total blocking time: %1").arg(blockingTime).toLatin1().data());
    }
};

int main(int argc, char *argv[])
{
    blockingTime = 0;
    TimeMeasuringApplication app(argc, argv);
    ModelinteractivityTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "modelinteractivitytest.moc"
