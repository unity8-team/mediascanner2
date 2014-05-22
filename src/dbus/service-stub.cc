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

#include <stdexcept>

#include <mediascanner/Album.hh>
#include <mediascanner/MediaFile.hh>
#include "service-stub.hh"
#include "dbus-interface.hh"
#include "dbus-codec.hh"

using std::string;

namespace mediascanner {
namespace dbus {

struct ServiceStub::Private
{
    core::dbus::Object::Ptr object;
};

ServiceStub::ServiceStub(core::dbus::Bus::Ptr bus)
    : core::dbus::Stub<ScannerService>(bus),
      p(new Private{access_service()->object_for_path(
                  core::dbus::types::ObjectPath(core::dbus::traits::Service<ScannerService>::object_path()))}) {
}

ServiceStub::~ServiceStub() {
}

MediaFile ServiceStub::lookup(const string &filename) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::Lookup, MediaFile>(filename);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

std::vector<MediaFile> ServiceStub::query(const string &q, MediaType type, int limit) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::Query, std::vector<MediaFile>>(q, (int32_t)type, (int32_t)limit);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

std::vector<Album> ServiceStub::queryAlbums(const string &core_term, int limit) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::QueryAlbums, std::vector<Album>>(core_term, (int32_t)limit);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

std::vector<MediaFile> ServiceStub::getAlbumSongs(const Album& album) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::GetAlbumSongs, std::vector<MediaFile>>(album);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

string ServiceStub::getETag(const string &filename) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::GetETag, string>(filename);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

std::vector<MediaFile> ServiceStub::listSongs(const string &artist, const string &album, const string &album_artist, int limit) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::ListSongs, std::vector<MediaFile>>(artist, album, album_artist, (int32_t)limit);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

std::vector<Album> ServiceStub::listAlbums(const string &artist, const string &album_artist, int limit) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::ListAlbums, std::vector<Album>>(artist, album_artist, (int32_t)limit);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}
std::vector<string> ServiceStub::listArtists(bool album_artists, int limit) const {
    auto result = p->object->invoke_method_synchronously<MediaStoreInterface::ListArtists, std::vector<string>>(album_artists, (int32_t)limit);
    if (result.is_error())
        throw std::runtime_error(result.error().print());
    return result.value();
}

}
}
