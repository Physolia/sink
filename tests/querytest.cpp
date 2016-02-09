#include <QtTest>

#include <QString>

#include "dummyresource/resourcefactory.h"
#include "clientapi.h"
#include "commands.h"
#include "resourceconfig.h"
#include "log.h"
#include "modelresult.h"

/**
 * Test of the query system using the dummy resource.
 *
 * This test requires the dummy resource installed.
 */
class QueryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        Sink::Log::setDebugOutputLevel(Sink::Log::Trace);
        auto factory = Sink::ResourceFactory::load("org.kde.dummy");
        QVERIFY(factory);
        ResourceConfig::addResource("org.kde.dummy.instance1", "org.kde.dummy");
        Sink::Store::removeDataFromDisk(QByteArray("org.kde.dummy.instance1")).exec().waitForFinished();
    }

    void cleanup()
    {
        Sink::Store::removeDataFromDisk(QByteArray("org.kde.dummy.instance1")).exec().waitForFinished();
    }

    void init()
    {
        qDebug();
        qDebug() << "-----------------------------------------";
        qDebug();
    }

    void testNoResources()
    {
        //Test
        Sink::Query query;
        query.resources << "foobar";
        query.liveQuery = true;

        //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 0);
    }


    void testSingle()
    {
        //Setup
        {
            Sink::ApplicationDomain::Mail mail("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.liveQuery = true;

        //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        model->fetchMore(QModelIndex());
        QTRY_COMPARE(model->rowCount(), 1);
    }

    void testSingleWithDelay()
    {
        //Setup
        {
            Sink::ApplicationDomain::Mail mail("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.liveQuery = false;

        //Ensure all local data is processed
        Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

        //We fetch after the data is available and don't rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);

        model->fetchMore(QModelIndex());
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 1);
    }

    void testById()
    {
        QByteArray id;
        //Setup
        {
            Sink::ApplicationDomain::Mail mail("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();

            Sink::Query query;
            query.resources << "org.kde.dummy.instance1";

            //Ensure all local data is processed
            Sink::Store::synchronize(query).exec().waitForFinished();

            //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
            auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
            model->fetchMore(QModelIndex());
            QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
            QVERIFY(model->rowCount() >= 1);
            id = model->index(0, 0).data(Sink::Store::DomainObjectRole).value<Sink::ApplicationDomain::Mail::Ptr>()->identifier();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.ids << id;
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        model->fetchMore(QModelIndex());
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 1);
    }

    void testFolder()
    {
        //Setup
        {
            Sink::ApplicationDomain::Folder folder("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Folder>(folder).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.liveQuery = true;

        //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Folder>(query);
        model->fetchMore(QModelIndex());
        QTRY_COMPARE(model->rowCount(), 1);
        auto folderEntity = model->index(0, 0).data(Sink::Store::DomainObjectRole).value<Sink::ApplicationDomain::Folder::Ptr>();
        QVERIFY(!folderEntity->identifier().isEmpty());
    }

    void testFolderTree()
    {
        //Setup
        {
            Sink::ApplicationDomain::Folder folder("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Folder>(folder).exec().waitForFinished();

            Sink::Query query;
            query.resources << "org.kde.dummy.instance1";

            //Ensure all local data is processed
            Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

            auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Folder>(query);
            QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
            QCOMPARE(model->rowCount(), 1);

            auto folderEntity = model->index(0, 0).data(Sink::Store::DomainObjectRole).value<Sink::ApplicationDomain::Folder::Ptr>();
            QVERIFY(!folderEntity->identifier().isEmpty());

            Sink::ApplicationDomain::Folder subfolder("org.kde.dummy.instance1");
            subfolder.setProperty("parent", folderEntity->identifier());
            Sink::Store::create<Sink::ApplicationDomain::Folder>(subfolder).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.parentProperty = "parent";

        //Ensure all local data is processed
        Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

        //We fetch after the data is available and don't rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Folder>(query);
        model->fetchMore(QModelIndex());
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 1);
        model->fetchMore(model->index(0, 0));
        QTRY_VERIFY(model->data(model->index(0, 0), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(model->index(0, 0)), 1);
    }

    void testMailByUid()
    {
        //Setup
        {
            Sink::ApplicationDomain::Mail mail("org.kde.dummy.instance1");
            mail.setProperty("uid", "test1");
            mail.setProperty("sender", "doe@example.org");
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.liveQuery = false;
        query.propertyFilter.insert("uid", "test1");

        //Ensure all local data is processed
        Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

        //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 1);
    }

    void testMailByFolder()
    {
        //Setup
        Sink::ApplicationDomain::Folder::Ptr folderEntity;
        {
            Sink::ApplicationDomain::Folder folder("org.kde.dummy.instance1");
            Sink::Store::create<Sink::ApplicationDomain::Folder>(folder).exec().waitForFinished();

            Sink::Query query;
            query.resources << "org.kde.dummy.instance1";

            //Ensure all local data is processed
            Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

            auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Folder>(query);
            QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
            QCOMPARE(model->rowCount(), 1);

            folderEntity = model->index(0, 0).data(Sink::Store::DomainObjectRole).value<Sink::ApplicationDomain::Folder::Ptr>();
            QVERIFY(!folderEntity->identifier().isEmpty());

            Sink::ApplicationDomain::Mail mail("org.kde.dummy.instance1");
            mail.setProperty("uid", "test1");
            mail.setProperty("folder", folderEntity->identifier());
            Sink::Store::create<Sink::ApplicationDomain::Mail>(mail).exec().waitForFinished();
        }

        //Test
        Sink::Query query;
        query.resources << "org.kde.dummy.instance1";
        query.propertyFilter.insert("folder", folderEntity->identifier());

        //Ensure all local data is processed
        Sink::Resources::flushMessageQueue(query.resources).exec().waitForFinished();

        //We fetch before the data is available and rely on the live query mechanism to deliver the actual data
        auto model = Sink::Store::loadModel<Sink::ApplicationDomain::Mail>(query);
        QTRY_VERIFY(model->data(QModelIndex(), Sink::Store::ChildrenFetchedRole).toBool());
        QCOMPARE(model->rowCount(), 1);
    }
};

QTEST_MAIN(QueryTest)
#include "querytest.moc"
