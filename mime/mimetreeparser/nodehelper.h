/*
  Copyright (C) 2009 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.net
  Copyright (c) 2009 Andras Mantia <andras@kdab.net>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#pragma once

#include <QList>

namespace KMime
{
class Content;
}

namespace MimeTreeParser
{

class NodeHelper
{
public:
    NodeHelper() = default;
    ~NodeHelper() = default;

    void setNodeProcessed(KMime::Content *node, bool recurse);
    void setNodeUnprocessed(KMime::Content *node, bool recurse);
    bool nodeProcessed(KMime::Content *node) const;
    QList<KMime::Content *> mProcessedNodes;

private:
    Q_DISABLE_COPY(NodeHelper)
};

}
