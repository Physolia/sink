/*
 *   Copyright (C) 2016 Christian Mollekopf <chrigi_1@fastmail.fm>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */
#include "mailsynctest.h"

#include <QtTest>

#include <QString>
#include <KMime/Message>

#include "store.h"
#include "resourcecontrol.h"
#include "log.h"
#include "test.h"

using namespace Sink;
using namespace Sink::ApplicationDomain;

void MailSyncTest::initTestCase()
{
    Test::initTest();
    Log::setDebugOutputLevel(Sink::Log::Trace);
    resetTestEnvironment();
    auto resource = createResource();
    QVERIFY(!resource.identifier().isEmpty());

    VERIFYEXEC(Store::create(resource));

    mResourceInstanceIdentifier = resource.identifier();
    mCapabilities = resource.getProperty("capabilities").value<QByteArrayList>();
}

void MailSyncTest::cleanup()
{
    // VERIFYEXEC(ResourceControl::shutdown(mResourceInstanceIdentifier));
    ResourceControl::shutdown(mResourceInstanceIdentifier).exec().waitForFinished();
    removeResourceFromDisk(mResourceInstanceIdentifier);
}

void MailSyncTest::init()
{
    qDebug();
    qDebug() << "-----------------------------------------";
    qDebug();
    VERIFYEXEC(ResourceControl::start(mResourceInstanceIdentifier));
}

void MailSyncTest::testListFolders()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Folder::Name>();

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Folder>(query).then<void, QList<Folder::Ptr>>([](const QList<Folder::Ptr> &folders) {
        QCOMPARE(folders.size(), 2);
        QStringList names;
        for (const auto &folder : folders) {
            names << folder->getName();
        }
        QVERIFY(names.contains("INBOX"));
        QVERIFY(names.contains("test"));
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testListFolderHierarchy()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Folder::Name>().request<Folder::Parent>();

    createFolder(QStringList() << "test" << "sub");

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Folder>(query).then<void, QList<Folder::Ptr>>([](const QList<Folder::Ptr> &folders) {
        QCOMPARE(folders.size(), 3);
        QHash<QString, Folder::Ptr> map;
        for (const auto &folder : folders) {
            map.insert(folder->getName(), folder);
        }
        QCOMPARE(map.value("sub")->getParent(), map.value("test")->identifier());
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testListNewFolders()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Folder::Name>();

    createFolder(QStringList() << "test" << "sub1");

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Folder>(query).then<void, QList<Folder::Ptr>>([](const QList<Folder::Ptr> &folders) {
        QStringList names;
        for (const auto &folder : folders) {
            names << folder->getName();
        }
        QVERIFY(names.contains("sub1"));
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testListRemovedFolders()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Folder::Name>();

    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    removeFolder(QStringList() << "test" << "sub1");

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Folder>(query).then<void, QList<Folder::Ptr>>([](const QList<Folder::Ptr> &folders) {
        QStringList names;
        for (const auto &folder : folders) {
            names << folder->getName();
        }
        QVERIFY(!names.contains("sub1"));
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testListMails()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Mail::Subject>().request<Mail::MimeMessage>();

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Mail>(query).then<void, QList<Mail::Ptr>>([](const QList<Mail::Ptr> &mails) {
        QCOMPARE(mails.size(), 1);
        QVERIFY(mails.first()->getSubject().startsWith(QString("[Nepomuk] Jenkins build is still unstable")));
        const auto data = mails.first()->getMimeMessage();
        QVERIFY(!data.isEmpty());

        KMime::Message m;
        m.setContent(data);
        m.parse();
        QCOMPARE(mails.first()->getSubject(), m.subject(true)->asUnicodeString());
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testFetchNewMessages()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Mail::Subject>().request<Mail::MimeMessage>();

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto msg = KMime::Message::Ptr::create();
    msg->subject(true)->fromUnicodeString("Foobar", "utf8");
    msg->assemble();
    createMessage(QStringList() << "test", msg->encodedContent(true));

    Store::synchronize(query).exec().waitForFinished();
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Mail>(query).then<void, QList<Mail::Ptr>>([](const QList<Mail::Ptr> &mails) {
        QCOMPARE(mails.size(), 2);
    });
    VERIFYEXEC(job);
}

void MailSyncTest::testFetchRemovedMessages()
{
    Sink::Query query;
    query.resources << mResourceInstanceIdentifier;
    query.request<Mail::Subject>().request<Mail::MimeMessage>();

    // Ensure all local data is processed
    VERIFYEXEC(Store::synchronize(query));
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    removeMessage(QStringList() << "test", "2:*");

    Store::synchronize(query).exec().waitForFinished();
    ResourceControl::flushMessageQueue(query.resources).exec().waitForFinished();

    auto job = Store::fetchAll<Mail>(query).then<void, QList<Mail::Ptr>>([](const QList<Mail::Ptr> &mails) {
        QCOMPARE(mails.size(), 1);
    });
    VERIFYEXEC(job);
}

//TODO test flag sync

void MailSyncTest::testFailingSync()
{
    auto resource = createFaultyResource();
    QVERIFY(!resource.identifier().isEmpty());
    VERIFYEXEC(Store::create(resource));

    Sink::Query query;
    query.resources << resource.identifier();

    // Ensure sync fails if resource is misconfigured
    auto future = Store::synchronize(query).exec();
    future.waitForFinished();
    QVERIFY(future.errorCode());
}

#include "mailsynctest.moc"
