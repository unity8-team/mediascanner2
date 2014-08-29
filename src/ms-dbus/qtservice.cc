/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * Authors:
 *    Jussi Pakkanen <jussi.pakkanen@canonical.com>
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

#include"qtservice.h"
#include <mediascanner/MediaFileBuilder.hh>
#include <mediascanner/Filter.hh>
#include <mediascanner/Album.hh>

QDBusArgument &operator<<(QDBusArgument &argument, const MediaFileWire &w) {
    argument.beginStructure();
    argument << w.fname
        << w.contenttype
        << w.etag
        << w.title
        << w.author
        << w.album
        << w.albumartist
        << w.date
        << w.genre
        << w.discnumber
        << w.tracknumber
        << w.duration
        << w.width
        << w.height
        << w.latitude
        << w.longitude
        << w.type;
    argument.endStructure();
    return argument;

}
const QDBusArgument &operator>>(const QDBusArgument &argument, MediaFileWire &w) {
    argument.beginStructure();
    argument >> w.fname
        >> w.contenttype
        >> w.etag
        >> w.title
        >> w.author
        >> w.album
        >> w.albumartist
        >> w.date
        >> w.genre
        >> w.discnumber
        >> w.tracknumber
        >> w.duration
        >> w.width
        >> w.height
        >> w.latitude
        >> w.longitude
        >> w.type;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const AlbumWire &a) {
    argument.beginStructure();
    argument << a.title << a.artist;
    argument.endStructure();
    return argument;

}

const QDBusArgument &operator>>(const QDBusArgument &argument, AlbumWire &a) {
    argument.beginStructure();
    argument >> a.title >> a.artist;
    argument.endStructure();
    return argument;

}

MediaFileWire::MediaFileWire(const mediascanner::MediaFile &mf) {
    fname = mf.getFileName().c_str();
    contenttype = mf.getContentType().c_str();
    etag = mf.getETag().c_str();
    title = mf.getTitle().c_str();
    author = mf.getAuthor().c_str();
    album = mf.getAlbum().c_str();
    albumartist = mf.getAlbumArtist().c_str();
    date = mf.getDate().c_str();
    genre = mf.getGenre().c_str();
    discnumber = mf.getDiscNumber();
    tracknumber = mf.getTrackNumber();
    duration = mf.getDuration();
    width = mf.getWidth();
    height = mf.getHeight();
    latitude = mf.getLatitude();
    longitude = mf.getLongitude();
    type = mf.getType();
}

mediascanner::MediaFile MediaFileWire::toMediaFile() const {
    mediascanner::MediaFileBuilder mfb(fname.toStdString());
    mfb.setETag(etag.toStdString())
             .setTitle(title.toStdString())
             .setContentType(contenttype.toStdString())
             .setAuthor(author.toStdString())
             .setAlbum(album.toStdString())
             .setAlbumArtist(albumartist.toStdString())
             .setDate(date.toStdString())
             .setGenre(genre.toStdString())
             .setDiscNumber(discnumber)
             .setTrackNumber(tracknumber)
             .setDuration(duration)
             .setWidth(width)
             .setHeight(height)
             .setLatitude(latitude)
             .setLongitude(longitude)
             .setType((mediascanner::MediaType) type);
    return mediascanner::MediaFile(mfb);
}

AlbumWire::AlbumWire(const mediascanner::Album &a) {
    title = a.getTitle().c_str();
    artist = a.getArtist().c_str();
}

mediascanner::Album AlbumWire::toAlbum() const {
    return mediascanner::Album(title.toStdString(), artist.toStdString());
}

static mediascanner::Filter vmap2filter(const QVariantMap &filter) {
    mediascanner::Filter f;
    auto a = filter.find("artist");
    if(a != filter.end()) {
        f.setArtist(a->value<QString>().toStdString());
    }
    a = filter.find("album_artist");
    if(a != filter.end()) {
        f.setAlbumArtist(a->value<QString>().toStdString());
    }
    a = filter.find("genre");
    if(a != filter.end()) {
        f.setGenre(a->value<QString>().toStdString());
    }
    a = filter.find("offset");
    if(a != filter.end()) {
        f.setOffset(a->value<int32_t>());
    }
    a = filter.find("limit");
    if(a != filter.end()) {
        f.setLimit(a->value<int32_t>());
    }
    a = filter.find("order");
    if(a != filter.end()) {
        f.setOrder((mediascanner::MediaOrder)a->value<int32_t>());
    }
    a = filter.find("reverse");
    if(a != filter.end()) {
        f.setReverse(a->value<bool>());
    }
    return f;
}

namespace mediascanner {

QtService::QtService(QObject *parent) : QDBusAbstractAdaptor(parent), store(MS_READ_ONLY) {
}

QtService::~QtService() {
}

MediaFileWire QtService::lookup(const QString &filename) const {
    return MediaFileWire(store.lookup(filename.toStdString()));
}

QList<MediaFileWire> QtService::query(const QString &q, int type, const QVariantMap &filter) const {
    Filter f = vmap2filter(filter);
    QList<MediaFileWire> resultset;
    auto res = store.query(q.toStdString(), (MediaType)type, f);
    for(const auto &r : res) {
        resultset.push_back(MediaFileWire(r));
    }
    return resultset;
}

QList<AlbumWire> QtService::queryAlbums(const QString &core_term, const QVariantMap &filter) const {
    QList<AlbumWire> result;
    for(const auto &r : store.queryAlbums(core_term.toStdString(), vmap2filter(filter))) {
        result.push_back(AlbumWire(r));
    }
    return result;
}

QStringList QtService::queryArtists(const QString &q, const QVariantMap &filter) const {
    QStringList result;
    for(const auto &a : store.queryArtists(q.toStdString(), vmap2filter(filter))) {
        result.push_back(a.c_str());
    }
    return result;
}

QList<MediaFileWire> QtService::getAlbumSongs(const AlbumWire& album) const {
    QList<MediaFileWire> result;
    for(const auto &a : store.getAlbumSongs(album.toAlbum())) {
        result.push_back(MediaFileWire(a));
    }
    return result;
}

/*
QString QtService::ping() {
    return "pong";
}
*/

}
