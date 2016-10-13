#pragma once

#include "sink_export.h"

#include <string>
#include <functional>
#include <QString>
#include <memory>
#include "storage.h"
#include "log.h"

namespace Xapian {
    class WritableDatabase;
};

/**
 * An index for value pairs.
 */
class SINK_EXPORT FulltextIndex
{
public:
    enum ErrorCodes
    {
        IndexNotAvailable = -1
    };

    class Error
    {
    public:
        Error(const QByteArray &s, int c, const QByteArray &m) : store(s), message(m), code(c)
        {
        }
        QByteArray store;
        QByteArray message;
        int code;
    };

    /* FulltextIndex(const QString &storageRoot, const QString &name, Sink::Storage::AccessMode mode = Sink::Storage::ReadOnly); */
    /* FulltextIndex(const QByteArray &name); */
    FulltextIndex(const QByteArray &resourceInstanceIdentifier, const QByteArray &name);

    void add(const QByteArray &key, const QString &value);
    void remove(const QByteArray &key);

    /* void startTransaction(); */
    /* void commit(qint64 revision); */

    QByteArrayList lookup(const QString &key);

private:
    Q_DISABLE_COPY(FulltextIndex);
    std::shared_ptr<Xapian::WritableDatabase> mDb;
    QString mName;
    SINK_DEBUG_COMPONENT(mName.toLatin1())
};
