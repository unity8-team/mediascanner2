/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#ifndef MEDIASTORE_HH_
#define MEDIASTORE_HH_

#include "MediaStoreBase.hh"
#include<vector>
#include<string>

namespace mediascanner {

enum OpenType {
    MS_READ_ONLY,
    MS_READ_WRITE
};

class MediaStore final : public virtual MediaStoreBase {
private:
    MediaStorePrivate *p;

public:
    MediaStore(OpenType access, const std::string &retireprefix="");
    MediaStore(const std::string &filename, OpenType access, const std::string &retireprefix="");
    MediaStore(const MediaStore &other) = delete;
    MediaStore& operator=(const MediaStore &other) = delete;
    virtual ~MediaStore();

    void insert(const MediaFile &m) const;
    void remove(const std::string &fname) const;
    virtual MediaFile lookup(const std::string &filename) const override;
    virtual std::vector<MediaFile> query(const std::string &q, MediaType type, int limit=-1) const override;
    virtual std::vector<Album> queryAlbums(const std::string &core_term, int limit=-1) const override;
    virtual std::vector<MediaFile> getAlbumSongs(const Album& album) const override;
    virtual std::string getETag(const std::string &filename) const override;
    virtual std::vector<MediaFile> listSongs(const Filter &filter) const override;
    virtual std::vector<Album> listAlbums(const Filter &filter) const override;
    virtual std::vector<std::string> listArtists(const Filter &filter) const override;
    virtual std::vector<std::string>listAlbumArtists(const Filter &filter) const override;
    virtual std::vector<std::string>listGenres(const Filter &filter) const override;

    size_t size() const;
    void pruneDeleted();
    void archiveItems(const std::string &prefix);
    void restoreItems(const std::string &prefix);
};

}

#endif
