/*
 * Copyright (C) 2015 Christian Mollekopf <chrigi_1@fastmail.fm>
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

#include <akonadi2common_export.h>
#include <resource.h>
#include <messagequeue.h>
#include <flatbuffers/flatbuffers.h>
#include <domainadaptor.h>
#include <QTimer>

class CommandProcessor;
class ChangeReplay;

namespace Akonadi2
{
class Pipeline;
class Preprocessor;

/**
 * Generic Resource implementation.
 */
class AKONADI2COMMON_EXPORT GenericResource : public Resource
{
public:
    GenericResource(const QByteArray &resourceInstanceIdentifier, const QSharedPointer<Pipeline> &pipeline = QSharedPointer<Pipeline>());
    virtual ~GenericResource();

    virtual void processCommand(int commandId, const QByteArray &data) Q_DECL_OVERRIDE;
    virtual KAsync::Job<void> synchronizeWithSource() Q_DECL_OVERRIDE;
    virtual KAsync::Job<void> synchronizeWithSource(Akonadi2::Storage &mainStore, Akonadi2::Storage &synchronizationStore);
    virtual KAsync::Job<void> processAllMessages() Q_DECL_OVERRIDE;
    virtual void setLowerBoundRevision(qint64 revision) Q_DECL_OVERRIDE;

    int error() const;

    static void removeFromDisk(const QByteArray &instanceIdentifier);
    static qint64 diskUsage(const QByteArray &instanceIdentifier);

private Q_SLOTS:
    void updateLowerBoundRevision();

protected:
    void enableChangeReplay(bool);
    void addType(const QByteArray &type, DomainTypeAdaptorFactoryInterface::Ptr factory, const QVector<Akonadi2::Preprocessor*> &preprocessors);
    virtual KAsync::Job<void> replay(Akonadi2::Storage &synchronizationStore, const QByteArray &type, const QByteArray &key, const QByteArray &value);
    void onProcessorError(int errorCode, const QString &errorMessage);
    void enqueueCommand(MessageQueue &mq, int commandId, const QByteArray &data);

    static void createEntity(const QByteArray &localId, const QByteArray &bufferType, const Akonadi2::ApplicationDomain::ApplicationDomainType &domainObject, DomainTypeAdaptorFactoryInterface &adaptorFactory, std::function<void(const QByteArray &)> callback);
    static void modifyEntity(const QByteArray &localId, qint64 revision, const QByteArray &bufferType, const Akonadi2::ApplicationDomain::ApplicationDomainType &domainObject, DomainTypeAdaptorFactoryInterface &adaptorFactory, std::function<void(const QByteArray &)> callback);
    static void deleteEntity(const QByteArray &localId, qint64 revision, const QByteArray &bufferType, std::function<void(const QByteArray &)> callback);

    /**
     * Records a localId to remoteId mapping
     */
    void recordRemoteId(const QByteArray &bufferType, const QByteArray &localId, const QByteArray &remoteId, Akonadi2::Storage::Transaction &transaction);
    void removeRemoteId(const QByteArray &bufferType, const QByteArray &localId, const QByteArray &remoteId, Akonadi2::Storage::Transaction &transaction);

    /**
     * Tries to find a local id for the remote id, and creates a new local id otherwise.
     * 
     * The new local id is recorded in the local to remote id mapping.
     */
    QByteArray resolveRemoteId(const QByteArray &type, const QByteArray &remoteId, Akonadi2::Storage::Transaction &transaction);

    /**
     * Tries to find a remote id for a local id.
     * 
     * This can fail if the entity hasn't been written back to the server yet.
     */
    QByteArray resolveLocalId(const QByteArray &bufferType, const QByteArray &localId, Akonadi2::Storage::Transaction &transaction);

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
    void scanForRemovals(Akonadi2::Storage::Transaction &transaction, Akonadi2::Storage::Transaction &synchronizationTransaction, const QByteArray &bufferType, const std::function<void(const std::function<void(const QByteArray &key)> &callback)> &entryGenerator, std::function<bool(const QByteArray &remoteId)> exists);

    /**
     * An algorithm to create or modify the entity.
     *
     * Depending on whether the entity is locally available, or has changed.
     */
    void createOrModify(Akonadi2::Storage::Transaction &transaction, Akonadi2::Storage::Transaction &synchronizationTransaction, DomainTypeAdaptorFactoryInterface &adaptorFactory, const QByteArray &bufferType, const QByteArray &remoteId, const Akonadi2::ApplicationDomain::ApplicationDomainType &entity);

    MessageQueue mUserQueue;
    MessageQueue mSynchronizerQueue;
    QByteArray mResourceInstanceIdentifier;
    QSharedPointer<Pipeline> mPipeline;

private:
    CommandProcessor *mProcessor;
    ChangeReplay *mSourceChangeReplay;
    int mError;
    QTimer mCommitQueueTimer;
    qint64 mClientLowerBoundRevision;
};

}
