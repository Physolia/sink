/*
 * Copyright (C) 2016 Christian Mollekopf <mollekopf@kolabsys.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "sink_export.h"
#include <QObject>
#include <QStack>
#include <KAsync/Async>
#include <domainadaptor.h>
#include <query.h>
#include <messagequeue.h>
#include <storage.h>
#include <storage/entitystore.h>
#include "changereplay.h"
#include "synchronizerstore.h"

namespace Sink {
class SynchronizerStore;

/**
 * Synchronize and add what we don't already have to local queue
 */
class SINK_EXPORT Synchronizer : public ChangeReplay
{
    Q_OBJECT
public:
    Synchronizer(const Sink::ResourceContext &resourceContext);
    virtual ~Synchronizer() Q_DECL_OVERRIDE;

    void setup(const std::function<void(int commandId, const QByteArray &data)> &enqueueCommandCallback, MessageQueue &messageQueue);
    void synchronize(const Sink::QueryBase &query);
    void flush(int commandId, const QByteArray &flushId);

    //Read only access to main storage
    Storage::EntityStore &store();

    //Read/Write access to sync storage
    SynchronizerStore &syncStore();

    void commit();
    Sink::Storage::DataStore::Transaction &syncTransaction();

    bool allChangesReplayed() Q_DECL_OVERRIDE;
    void flushComplete(const QByteArray &flushId);

    void setSecret(const QString &s);

signals:
    void notify(Notification);

public slots:
    virtual void revisionChanged() Q_DECL_OVERRIDE;

protected:
    ///Base implementation calls the replay$Type calls
    KAsync::Job<void> replay(const QByteArray &type, const QByteArray &key, const QByteArray &value) Q_DECL_OVERRIDE;
    virtual bool canReplay(const QByteArray &type, const QByteArray &key, const QByteArray &value) Q_DECL_OVERRIDE;

protected:
    ///Implement to write back changes to the server
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Contact &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Addressbook &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Mail &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Folder &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Event &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Todo &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
    virtual KAsync::Job<QByteArray> replay(const Sink::ApplicationDomain::Calendar &, Sink::Operation, const QByteArray &oldRemoteId, const QList<QByteArray> &);
protected:
    QString secret() const;

    ///Calls the callback to enqueue the command
    void enqueueCommand(int commandId, const QByteArray &data);

    void createEntity(const QByteArray &localId, const QByteArray &bufferType, const Sink::ApplicationDomain::ApplicationDomainType &domainObject);
    void modifyEntity(const QByteArray &localId, qint64 revision, const QByteArray &bufferType, const Sink::ApplicationDomain::ApplicationDomainType &domainObject, const QByteArray &newResource = QByteArray(), bool remove = false);
    void deleteEntity(const QByteArray &localId, qint64 revision, const QByteArray &bufferType);

    /**
    * A synchronous algorithm to remove entities that are no longer existing.
    *
    * A list of entities is generated by @param entryGenerator.
    * The entiry Generator typically iterates over an index to produce all existing entries.
    * This algorithm calls @param exists for every entity of type @param type, with its remoteId. For every entity where @param exists returns false,
    * an entity delete command is enqueued.
    *
    * All functions are called synchronously, and both @param entryGenerator and @param exists need to be synchronous.
    */
    void scanForRemovals(const QByteArray &bufferType,
        const std::function<void(const std::function<void(const QByteArray &sinkId)> &callback)> &entryGenerator, std::function<bool(const QByteArray &remoteId)> exists);
    void scanForRemovals(const QByteArray &bufferType, std::function<bool(const QByteArray &remoteId)> exists);

    /**
     * An algorithm to create or modify the entity.
     *
     * Depending on whether the entity is locally available, or has changed.
     */
    void createOrModify(const QByteArray &bufferType, const QByteArray &remoteId, const Sink::ApplicationDomain::ApplicationDomainType &entity);
    template <typename DomainType>
    void SINK_EXPORT createOrModify(const QByteArray &bufferType, const QByteArray &remoteId, const DomainType &entity, const QHash<QByteArray, Sink::Query::Comparator> &mergeCriteria);
    void modify(const QByteArray &bufferType, const QByteArray &remoteId, const Sink::ApplicationDomain::ApplicationDomainType &entity);

    // template <typename DomainType>
    // void create(const DomainType &entity);
    template <typename DomainType>
    void SINK_EXPORT modify(const DomainType &entity, const QByteArray &newResource = QByteArray(), bool remove = false);
    // template <typename DomainType>
    // void remove(const DomainType &entity);
 
    QByteArrayList resolveQuery(const QueryBase &query);
    QByteArrayList resolveFilter(const QueryBase::Comparator &filter);

    virtual KAsync::Job<void> synchronizeWithSource(const Sink::QueryBase &query) = 0;

public:
    struct SyncRequest {
        enum RequestType {
            Synchronization,
            ChangeReplay,
            Flush
        };

        enum RequestOptions {
            NoOptions,
            RequestFlush
        };

        SyncRequest() = default;

        SyncRequest(const Sink::QueryBase &q, const QByteArray &requestId_ = QByteArray(), RequestOptions o = NoOptions)
            : requestId(requestId_),
            requestType(Synchronization),
            options(o),
            query(q),
            applicableEntities(q.ids())
        {
        }

        SyncRequest(RequestType type)
            : requestType(type)
        {
        }

        SyncRequest(RequestType type, const QByteArray &requestId_)
            : requestId(requestId_),
            requestType(type)
        {
        }

        SyncRequest(RequestType type, int flushType_, const QByteArray &requestId_)
            : flushType(flushType_),
            requestId(requestId_),
            requestType(type)
        {
        }

        int flushType = 0;
        QByteArray requestId;
        RequestType requestType;
        RequestOptions options = NoOptions;
        Sink::QueryBase query;
        QByteArrayList applicableEntities;
    };

protected:
    /**
     * This allows the synchronizer to turn a single query into multiple synchronization requests.
     *
     * The idea is the following;
     * The input query is a specification by the application of what data needs to be made available.
     * Requests could be:
     * * Give me everything (signified by the default constructed/empty query)
     * * Give me all mails of folder X
     * * Give me all mails of folders matching some constraints
     *
     * getSyncRequests allows the resource implementation to apply it's own defaults to that request;
     * * While a maildir resource might give you always all emails of a folder, an IMAP resource might have a date limit, to i.e. only retrieve the last 14 days worth of data.
     * * A resource get's to define what "give me everything" means. For email that may be turned into first a requests for folders, and then a request for all emails in those folders.
     *
     * This will allow synchronizeWithSource to focus on just getting to the content.
     */
    virtual QList<Synchronizer::SyncRequest> getSyncRequests(const Sink::QueryBase &query);

    /**
     * This allows the synchronizer to merge new requests with existing requests in the queue.
     */
    virtual void mergeIntoQueue(const Synchronizer::SyncRequest &request, QList<Synchronizer::SyncRequest> &queue);

    void emitNotification(Notification::NoticationType type, int code, const QString &message, const QByteArray &id = QByteArray{}, const QByteArrayList &entiteis = QByteArrayList{});
    void emitProgressNotification(Notification::NoticationType type, int progress, int total, const QByteArray &id, const QByteArrayList &entities);

    /**
     * Report progress for current task
     */
    virtual void reportProgress(int progress, int total, const QByteArrayList &entities = {}) Q_DECL_OVERRIDE;

    Sink::Log::Context mLogCtx;

private:
    QStack<ApplicationDomain::Status> mCurrentState;
    void setStatusFromResult(const KAsync::Error &error, const QString &s, const QByteArray &requestId);
    void setStatus(ApplicationDomain::Status busy, const QString &reason, const QByteArray requestId);
    void resetStatus(const QByteArray requestId);
    void setBusy(bool busy, const QString &reason, const QByteArray requestId);

    void modifyIfChanged(Storage::EntityStore &store, const QByteArray &bufferType, const QByteArray &sinkId, const Sink::ApplicationDomain::ApplicationDomainType &entity);
    KAsync::Job<void> processRequest(const SyncRequest &request);
    KAsync::Job<void> processSyncQueue();

    Sink::ResourceContext mResourceContext;
    Sink::Storage::EntityStore::Ptr mEntityStore;
    QSharedPointer<SynchronizerStore> mSyncStore;
    Sink::Storage::DataStore mSyncStorage;
    Sink::Storage::DataStore::Transaction mSyncTransaction;
    std::function<void(int commandId, const QByteArray &data)> mEnqueue;
    QList<SyncRequest> mSyncRequestQueue;
    SyncRequest mCurrentRequest;
    MessageQueue *mMessageQueue;
    bool mSyncInProgress;
    QMultiHash<QByteArray, SyncRequest> mPendingSyncRequests;
    QString mSecret;
};

}

