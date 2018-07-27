/*
 * Copyright (C) 2014 Christian Mollekopf <chrigi_1@fastmail.fm>
 * Copyright (C) 2018 Rémi Nicole <minijackson@riseup.net>
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

#include "key.h"
#include "utils.h"

using Sink::Storage::Identifier;
using Sink::Storage::Key;
using Sink::Storage::Revision;

QDebug &operator<<(QDebug &dbg, const Identifier &id)
{
    dbg << id.toDisplayString();
    return dbg;
}

QDebug &operator<<(QDebug &dbg, const Revision &rev)
{
    dbg << rev.toDisplayString();
    return dbg;
}

QDebug &operator<<(QDebug &dbg, const Key &key)
{
    dbg << key.toDisplayString();
    return dbg;
}

// Identifier

QByteArray Identifier::toInternalByteArray() const
{
    return uid.toRfc4122();
}

Identifier Identifier::fromInternalByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == INTERNAL_REPR_SIZE);
    return Identifier(QUuid::fromRfc4122(bytes));
}

QString Identifier::toDisplayString() const
{
    return uid.toString();
}

QByteArray Identifier::toDisplayByteArray() const
{
    return uid.toByteArray();
}

Identifier Identifier::fromDisplayByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == DISPLAY_REPR_SIZE);
    return Identifier(QUuid(bytes));
}

// Revision

QByteArray Revision::toInternalByteArray() const
{
    return padNumber(rev);
}

Revision Revision::fromInternalByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == INTERNAL_REPR_SIZE);
    return Revision(bytes.toLongLong());
}

QString Revision::toDisplayString() const
{
    return QString::fromUtf8(toInternalByteArray());
}

QByteArray Revision::toDisplayByteArray() const
{
    return toInternalByteArray();
}

Revision Revision::fromDisplayByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == DISPLAY_REPR_SIZE);
    return fromInternalByteArray(bytes);
}

qint64 Revision::toQint64() const
{
    return rev;
}

// Key

QByteArray Key::toInternalByteArray() const
{
    return id.toInternalByteArray() + rev.toInternalByteArray();
}

Key Key::fromInternalByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == INTERNAL_REPR_SIZE);
    auto idBytes = bytes.mid(0, Identifier::INTERNAL_REPR_SIZE);
    auto revBytes = bytes.mid(Identifier::INTERNAL_REPR_SIZE);
    return Key(Identifier::fromInternalByteArray(idBytes), Revision::fromInternalByteArray(revBytes));
}

QString Key::toDisplayString() const
{
    return id.toDisplayString() + rev.toDisplayString();
}

QByteArray Key::toDisplayByteArray() const
{
    return id.toDisplayByteArray() + rev.toDisplayByteArray();
}

Key Key::fromDisplayByteArray(const QByteArray &bytes)
{
    Q_ASSERT(bytes.size() == DISPLAY_REPR_SIZE);
    auto idBytes = bytes.mid(0, Identifier::DISPLAY_REPR_SIZE);
    auto revBytes = bytes.mid(Identifier::DISPLAY_REPR_SIZE);
    return Key(Identifier::fromDisplayByteArray(idBytes), Revision::fromDisplayByteArray(revBytes));
}

const Identifier &Key::identifier() const
{
    return id;
}

const Revision &Key::revision() const
{
    return rev;
}

void Key::setRevision(const Revision &newRev)
{
    rev = newRev;
}
