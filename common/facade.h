/*
 * Copyright (C) 2014 Christian Mollekopf <chrigi_1@fastmail.fm>
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

#pragma once

#include "facadeinterface.h"

#include <QByteArray>
#include <Async/Async>

#include "resourceaccess.h"
#include "commands.h"
#include "domainadaptor.h"
#include "log.h"
#include "resultset.h"
#include "storage.h"
#include "definitions.h"

/**
 * A QueryRunner runs a query and updates the corresponding result set.
 * 
 * The lifetime of the QueryRunner is defined by the resut set (otherwise it's doing useless work),
 * and by how long a result set must be updated. If the query is one off the runner dies after the execution,
 * otherwise it lives on the react to changes and updates the corresponding result set.
 * 
 * QueryRunner has to keep ResourceAccess alive in order to keep getting updates.
 */
class QueryRunner : public QObject
{
    Q_OBJECT
public:
    typedef std::function<KAsync::Job<void>()> QueryFunction;

    QueryRunner(const Akonadi2::Query &query) {};
    /**
     * Starts query
     */
    KAsync::Job<void> run(qint64 newRevision = 0)
    {
        return queryFunction();
    }

    /**
     * Set the query to run
     */
    void setQuery(const QueryFunction &query)
    {
        queryFunction = query;
    }

public slots:
    /**
     * Rerun query with new revision
     */
    void revisionChanged(qint64 newRevision)
    {
        Trace() << "New revision: " << newRevision;
        run().exec();
    }

private:
    QueryFunction queryFunction;
};

static inline ResultSet fullScan(const Akonadi2::Storage::Transaction &transaction, const QByteArray &bufferType)
{
    //TODO use a result set with an iterator, to read values on demand
    QVector<QByteArray> keys;
    transaction.openDatabase(bufferType + ".main").scan(QByteArray(), [&](const QByteArray &key, const QByteArray &value) -> bool {
        //Skip internals
        if (Akonadi2::Storage::isInternalKey(key)) {
            return true;
        }
        keys << Akonadi2::Storage::uidFromKey(key);
        return true;
    },
    [](const Akonadi2::Storage::Error &error) {
        qWarning() << "Error during query: " << error.message;
    });

    Trace() << "Full scan found " << keys.size() << " results";
    return ResultSet(keys);
}


namespace Akonadi2 {
/**
 * Default facade implementation for resources that are implemented in a separate process using the ResourceAccess class.
 * 
 * Ideally a basic resource has no implementation effort for the facades and can simply instanciate default implementations (meaning it only has to implement the factory with all supported types).
 * A resource has to implement:
 * * A facade factory registering all available facades
 * * An adaptor factory if it uses special resource buffers (default implementation can be used otherwise)
 * * A mapping between resource and buffertype if default can't be used.
 *
 * Additionally a resource only has to provide a synchronizer plugin to execute the synchronization
 */
template <typename DomainType>
class GenericFacade: public Akonadi2::StoreFacade<DomainType>
{
public:
    /**
     * Create a new GenericFacade
     * 
     * @param resourceIdentifier is the identifier of the resource instance
     * @param adaptorFactory is the adaptor factory used to generate the mappings from domain to resource types and vice versa
     */
    GenericFacade(const QByteArray &resourceIdentifier, const DomainTypeAdaptorFactoryInterface::Ptr &adaptorFactory = DomainTypeAdaptorFactoryInterface::Ptr(), const QSharedPointer<Akonadi2::ResourceAccessInterface> resourceAccess = QSharedPointer<Akonadi2::ResourceAccessInterface>())
        : Akonadi2::StoreFacade<DomainType>(),
        mResourceAccess(resourceAccess),
        mDomainTypeAdaptorFactory(adaptorFactory),
        mResourceInstanceIdentifier(resourceIdentifier)
    {
        if (!mResourceAccess) {
            mResourceAccess = QSharedPointer<Akonadi2::ResourceAccess>::create(resourceIdentifier);
        }
    }

    ~GenericFacade()
    {
    }

    static QByteArray bufferTypeForDomainType()
    {
        //We happen to have a one to one mapping
        return Akonadi2::ApplicationDomain::getTypeName<DomainType>();
    }

    KAsync::Job<void> create(const DomainType &domainObject) Q_DECL_OVERRIDE
    {
        if (!mDomainTypeAdaptorFactory) {
            Warning() << "No domain type adaptor factory available";
            return KAsync::error<void>();
        }
        flatbuffers::FlatBufferBuilder entityFbb;
        mDomainTypeAdaptorFactory->createBuffer(domainObject, entityFbb);
        return mResourceAccess->sendCreateCommand(bufferTypeForDomainType(), QByteArray::fromRawData(reinterpret_cast<const char*>(entityFbb.GetBufferPointer()), entityFbb.GetSize()));
    }

    KAsync::Job<void> modify(const DomainType &domainObject) Q_DECL_OVERRIDE
    {
        if (!mDomainTypeAdaptorFactory) {
            Warning() << "No domain type adaptor factory available";
            return KAsync::error<void>();
        }
        flatbuffers::FlatBufferBuilder entityFbb;
        mDomainTypeAdaptorFactory->createBuffer(domainObject, entityFbb);
        return mResourceAccess->sendModifyCommand(domainObject.identifier(), domainObject.revision(), bufferTypeForDomainType(), QByteArrayList(), QByteArray::fromRawData(reinterpret_cast<const char*>(entityFbb.GetBufferPointer()), entityFbb.GetSize()));
    }

    KAsync::Job<void> remove(const DomainType &domainObject) Q_DECL_OVERRIDE
    {
        return mResourceAccess->sendDeleteCommand(domainObject.identifier(), domainObject.revision(), bufferTypeForDomainType());
    }

    KAsync::Job<void> load(const Akonadi2::Query &query, Akonadi2::ResultProviderInterface<typename DomainType::Ptr> &resultProvider) Q_DECL_OVERRIDE
    {
        //We delegate loading of initial data to the result provider, os it can decide for itself what it needs to load.
        resultProvider.setFetcher([this, query, &resultProvider](const QByteArray &parent) {
            const qint64 newRevision = executeInitialQuery(query, parent, resultProvider);
            mResourceAccess->sendRevisionReplayedCommand(newRevision);
        });


        //In case of a live query we keep the runner for as long alive as the result provider exists
        if (query.liveQuery) {
            auto runner = QSharedPointer<QueryRunner>::create(query);
            //Incremental updates are always loaded directly, leaving it up to the result to discard the changes if they are not interesting
            runner->setQuery([this, query, &resultProvider] () -> KAsync::Job<void> {
                return KAsync::start<void>([this, query, &resultProvider](KAsync::Future<void> &future) {
                    Trace() << "Executing query ";
                    const qint64 newRevision = executeIncrementalQuery(query, resultProvider);
                    mResourceAccess->sendRevisionReplayedCommand(newRevision);
                    future.setFinished();
                });
            });
            resultProvider.setQueryRunner(runner);
            //Ensure the connection is open, if it wasn't already opened
            //TODO If we are not connected already, we have to check for the latest revision once connected, otherwise we could miss some updates
            mResourceAccess->open();
            QObject::connect(mResourceAccess.data(), &Akonadi2::ResourceAccess::revisionChanged, runner.data(), &QueryRunner::revisionChanged);
        }
        return KAsync::null<void>();
    }

private:

    //TODO move into result provider?
    static void replaySet(ResultSet &resultSet, Akonadi2::ResultProviderInterface<typename DomainType::Ptr> &resultProvider)
    {
        while (resultSet.next([&resultProvider](const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &value, Akonadi2::Operation operation) -> bool {
            switch (operation) {
            case Akonadi2::Operation_Creation:
                Trace() << "Got creation";
                resultProvider.add(Akonadi2::ApplicationDomain::ApplicationDomainType::getInMemoryRepresentation<DomainType>(*value).template staticCast<DomainType>());
                break;
            case Akonadi2::Operation_Modification:
                Trace() << "Got modification";
                resultProvider.modify(Akonadi2::ApplicationDomain::ApplicationDomainType::getInMemoryRepresentation<DomainType>(*value).template staticCast<DomainType>());
                break;
            case Akonadi2::Operation_Removal:
                Trace() << "Got removal";
                resultProvider.remove(Akonadi2::ApplicationDomain::ApplicationDomainType::getInMemoryRepresentation<DomainType>(*value).template staticCast<DomainType>());
                break;
            }
            return true;
        })){};
    }

    void readEntity(const Akonadi2::Storage::Transaction &transaction, const QByteArray &key, const std::function<void(const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &, Akonadi2::Operation)> &resultCallback)
    {
        const auto bufferType = bufferTypeForDomainType();
        //This only works for a 1:1 mapping of resource to domain types.
        //Not i.e. for tags that are stored as flags in each entity of an imap store.
        //additional properties that don't have a 1:1 mapping (such as separately stored tags),
        //could be added to the adaptor.
        //
        // Akonadi2::Storage::getLatest(transaction, bufferTye, key);
        transaction.openDatabase(bufferType + ".main").findLatest(key, [=](const QByteArray &key, const QByteArray &value) -> bool {
            Akonadi2::EntityBuffer buffer(value.data(), value.size());
            const Akonadi2::Entity &entity = buffer.entity();
            const auto metadataBuffer = Akonadi2::EntityBuffer::readBuffer<Akonadi2::Metadata>(entity.metadata());
            Q_ASSERT(metadataBuffer);
            const qint64 revision = metadataBuffer ? metadataBuffer->revision() : -1;
            resultCallback(DomainType::Ptr::create(mResourceInstanceIdentifier, Akonadi2::Storage::uidFromKey(key), revision, mDomainTypeAdaptorFactory->createAdaptor(entity)), metadataBuffer->operation());
            return false;
        },
        [](const Akonadi2::Storage::Error &error) {
            qWarning() << "Error during query: " << error.message;
        });
    }

    ResultSet loadInitialResultSet(const Akonadi2::Query &query, Akonadi2::Storage::Transaction &transaction, QSet<QByteArray> &remainingFilters)
    {
        QSet<QByteArray> appliedFilters;
        auto resultSet = Akonadi2::ApplicationDomain::TypeImplementation<DomainType>::queryIndexes(query, mResourceInstanceIdentifier, appliedFilters, transaction);
        remainingFilters = query.propertyFilter.keys().toSet() - appliedFilters;

        //We do a full scan if there were no indexes available to create the initial set.
        if (appliedFilters.isEmpty()) {
            //TODO this should be replaced by an index lookup as well
            resultSet = fullScan(transaction, bufferTypeForDomainType());
        }
        return resultSet;
    }

    ResultSet loadIncrementalResultSet(qint64 baseRevision, const Akonadi2::Query &query, Akonadi2::Storage::Transaction &transaction, QSet<QByteArray> &remainingFilters)
    {
        const auto bufferType = bufferTypeForDomainType();
        auto revisionCounter = QSharedPointer<qint64>::create(baseRevision);
        remainingFilters = query.propertyFilter.keys().toSet();
        return ResultSet([bufferType, revisionCounter, &transaction]() -> QByteArray {
            const qint64 topRevision = Akonadi2::Storage::maxRevision(transaction);
            //Spit out the revision keys one by one.
            while (*revisionCounter <= topRevision) {
                const auto uid = Akonadi2::Storage::getUidFromRevision(transaction, *revisionCounter);
                const auto type = Akonadi2::Storage::getTypeFromRevision(transaction, *revisionCounter);
                Trace() << "Revision" << *revisionCounter << type << uid;
                if (type != bufferType) {
                    //Skip revision
                    *revisionCounter += 1;
                    continue;
                }
                const auto key = Akonadi2::Storage::assembleKey(uid, *revisionCounter);
                *revisionCounter += 1;
                return key;
            }
            //We're done
            return QByteArray();
        });
    }

    ResultSet filterSet(const ResultSet &resultSet, const std::function<bool(const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &domainObject)> &filter, const Akonadi2::Storage::Transaction &transaction, bool initialQuery)
    {
        auto resultSetPtr = QSharedPointer<ResultSet>::create(resultSet);

        //Read through the source values and return whatever matches the filter
        std::function<bool(std::function<void(const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &, Akonadi2::Operation)>)> generator = [this, resultSetPtr, &transaction, filter, initialQuery](std::function<void(const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &, Akonadi2::Operation)> callback) -> bool {
            while (resultSetPtr->next()) {
                //readEntity is only necessary if we actually want to filter or know the operation type (but not a big deal if we do it always I guess)
                readEntity(transaction, resultSetPtr->id(), [this, filter, callback, initialQuery](const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &domainObject, Akonadi2::Operation operation) {
                    //Always remove removals, they probably don't match due to non-available properties
                    if (filter(domainObject) || operation == Akonadi2::Operation_Removal) {
                        if (initialQuery) {
                            //We're not interested in removals during the initial query
                            if (operation != Akonadi2::Operation_Removal) {
                                callback(domainObject, Akonadi2::Operation_Creation);
                            }
                        } else {
                            callback(domainObject, operation);
                        }
                    }
                });
            }
            return false;
        };
        return ResultSet(generator);
    }


    std::function<bool(const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &domainObject)> getFilter(const QSet<QByteArray> remainingFilters, const Akonadi2::Query &query)
    {
        return [remainingFilters, query](const Akonadi2::ApplicationDomain::ApplicationDomainType::Ptr &domainObject) -> bool {
            for (const auto &filterProperty : remainingFilters) {
                const auto property = domainObject->getProperty(filterProperty);
                if (property.isValid()) {
                    //TODO implement other comparison operators than equality
                    if (property != query.propertyFilter.value(filterProperty)) {
                        Trace() << "Filtering entity due to property mismatch: " << domainObject->getProperty(filterProperty);
                        return false;
                    }
                } else {
                    Warning() << "Ignored property filter because value is invalid: " << filterProperty;
                }
            }
            return true;
        };
    }

    qint64 load(const Akonadi2::Query &query, const std::function<ResultSet(Akonadi2::Storage::Transaction &, QSet<QByteArray> &)> &baseSetRetriever, Akonadi2::ResultProviderInterface<typename DomainType::Ptr> &resultProvider)
    {
        Akonadi2::Storage storage(Akonadi2::storageLocation(), mResourceInstanceIdentifier);
        storage.setDefaultErrorHandler([](const Akonadi2::Storage::Error &error) {
            Warning() << "Error during query: " << error.store << error.message;
        });
        auto transaction = storage.createTransaction(Akonadi2::Storage::ReadOnly);

        QSet<QByteArray> remainingFilters;
        auto resultSet = baseSetRetriever(transaction, remainingFilters);
        auto filteredSet = filterSet(resultSet, getFilter(remainingFilters, query), transaction, false);
        replaySet(filteredSet, resultProvider);
        resultProvider.setRevision(Akonadi2::Storage::maxRevision(transaction));
        return Akonadi2::Storage::maxRevision(transaction);
    }


    qint64 executeIncrementalQuery(const Akonadi2::Query &query, Akonadi2::ResultProviderInterface<typename DomainType::Ptr> &resultProvider)
    {
        const qint64 baseRevision = resultProvider.revision() + 1;
        Trace() << "Running incremental query " << baseRevision;
        return load(query, [&](Akonadi2::Storage::Transaction &transaction, QSet<QByteArray> &remainingFilters) -> ResultSet {
            return loadIncrementalResultSet(baseRevision, query, transaction, remainingFilters);
        }, resultProvider);
    }

    qint64 executeInitialQuery(const Akonadi2::Query &query, const QByteArray &parent, Akonadi2::ResultProviderInterface<typename DomainType::Ptr> &resultProvider)
    {
        Trace() << "Running initial query for parent:" << parent;
        auto modifiedQuery = query;
        modifiedQuery.propertyFilter.insert("parent", parent);
        return load(modifiedQuery, [&](Akonadi2::Storage::Transaction &transaction, QSet<QByteArray> &remainingFilters) -> ResultSet {
            return loadInitialResultSet(modifiedQuery, transaction, remainingFilters);
        }, resultProvider);
    }

protected:
    //TODO use one resource access instance per application & per resource
    QSharedPointer<Akonadi2::ResourceAccessInterface> mResourceAccess;
    DomainTypeAdaptorFactoryInterface::Ptr mDomainTypeAdaptorFactory;
    QByteArray mResourceInstanceIdentifier;
};

}
