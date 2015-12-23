/*
 * Copyright (C) 2014 Aaron Seigo <aseigo@kde.org>
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

#include "module.h"

#include <QCoreApplication>
#include <QDebug>

// TODO: needs a proper registry; making "core" modules plugins is
//       almost certainly overkill, but this is not the way either
#include "modules/core_syntax.h"

Module *Module::s_module = 0;

Module::Syntax::Syntax()
{
}

Module::Syntax::Syntax(const QString &k, const QString &helpText, std::function<bool(const QStringList &, State &)> l, Interactivity inter)
    : keyword(k),
      help(helpText),
      interactivity(inter),
      lambda(l)
{
}

Module::Module()
{
    QVector<std::function<SyntaxList()> > syntaxModules;
    syntaxModules << &CoreSyntax::syntax;
    for (auto syntaxModule: syntaxModules) {
        m_syntax += syntaxModule();
    }
}

Module *Module::self()
{
    if (!s_module) {
        s_module = new Module;
    }

    return s_module;
}

Module::SyntaxList Module::syntax() const
{
    return m_syntax;
}

bool Module::run(const QStringList &commands)
{
    Command command = match(commands);
    if (command.first && command.first->lambda) {
        command.first->lambda(command.second, m_state);
        if (command.first->interactivity == Syntax::EventDriven) {
            return QCoreApplication::instance()->exec();
        }

        return true;
    }

    return false;
}

Module::Command Module::match(const QStringList &commandLine) const
{
    if (commandLine.isEmpty()) {
        return Command();
    }

    QStringListIterator commandLineIt(commandLine);

    QVectorIterator<Syntax> syntaxIt(m_syntax);
    const Syntax *lastFullSyntax = 0;
    QStringList tailCommands;
    while (commandLineIt.hasNext() && syntaxIt.hasNext()) {
        const QString word = commandLineIt.next();
        while (syntaxIt.hasNext()) {
            const Syntax &syntax = syntaxIt.next();
            if (word == syntax.keyword) {
                lastFullSyntax = &syntax;
                syntaxIt = syntax.children;
            }
        }

        if (!syntaxIt.hasNext()) {
            break;
        }
    }

    if (lastFullSyntax && lastFullSyntax->lambda) {
        while (commandLineIt.hasNext()) {
            tailCommands << commandLineIt.next();
        }

        return std::make_pair(lastFullSyntax, tailCommands);
    }

    return Command();
}

Module::SyntaxList Module::nearestSyntax(const QStringList &words, const QString &fragment) const
{
    SyntaxList matches;

    //qDebug() << "words are" << words;
    if (words.isEmpty()) {
        for (const Syntax &syntax: m_syntax) {
            if (syntax.keyword.startsWith(fragment)) {
                matches.push_back(syntax);
            }
        }
    } else {
        QStringListIterator wordIt(words);
        QVectorIterator<Syntax> syntaxIt(m_syntax);
        Syntax lastFullSyntax;

        while (wordIt.hasNext()) {
            QString word = wordIt.next();
            while (syntaxIt.hasNext()) {
                const Syntax &syntax = syntaxIt.next();
                if (word == syntax.keyword) {
                    lastFullSyntax = syntax;
                    syntaxIt = syntax.children;
                }
            }
        }

        //qDebug() << "exiting with" << lastFullSyntax.keyword << words.last();
        if (lastFullSyntax.keyword == words.last()) {
            syntaxIt = lastFullSyntax.children;
            while (syntaxIt.hasNext()) {
                Syntax syntax = syntaxIt.next();
                if (fragment.isEmpty() || syntax.keyword.startsWith(fragment)) {
                    matches.push_back(syntax);
                }
            }
        }
    }

    return matches;
}

QStringList Module::tokenize(const QString &text)
{
    //TODO: properly tokenize (e.g. "foo bar" should not become ['"foo', 'bar"']
    return text.split(" ");
}

