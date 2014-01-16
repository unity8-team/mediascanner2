/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MEDIASCANNER_QML_MEDIAFILEWRAPPER_H
#define MEDIASCANNER_QML_MEDIAFILEWRAPPER_H

#include <QObject>
#include <QString>

#include <mediascanner/MediaFile.hh>

class MediaFileWrapper : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString filename READ filename CONSTANT)
    Q_PROPERTY(QString uri READ uri CONSTANT)
    Q_PROPERTY(QString contentType READ contentType CONSTANT)
    Q_PROPERTY(QString eTag READ eTag CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString author READ author CONSTANT)
    Q_PROPERTY(QString album READ album CONSTANT)
    Q_PROPERTY(QString albumArtist READ albumArtist CONSTANT)
    Q_PROPERTY(QString date READ date CONSTANT)
    Q_PROPERTY(int trackNumber READ trackNumber CONSTANT)
    Q_PROPERTY(int duration READ duration CONSTANT)
public:
    MediaFileWrapper(MediaFile media, QObject *parent=0);
    QString filename() const;
    QString uri() const;
    QString contentType() const;
    QString eTag() const;
    QString title() const;
    QString author() const;
    QString album() const;
    QString albumArtist() const;
    QString date() const;
    int trackNumber() const;
    int duration() const;
private:
    const MediaFile media;
};

#endif
