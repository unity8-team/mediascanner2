/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * Authors:
 *    James Henstridge <james.henstridge@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of version 3 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "VolumeManager.hh"

#include <mediascanner/MediaFile.hh>
#include <mediascanner/MediaStore.hh>
#include <extractor/DetectedFile.hh>
#include <extractor/MetadataExtractor.hh>
#include "InvalidationSender.hh"
#include "Scanner.hh"
#include "SubtreeWatcher.hh"
#include "../mediascanner/internal/utils.hh"

#include <glib.h>

#include <cassert>
#include <map>
#include <vector>

using namespace std;

namespace {

enum class VolumeEventType {
    added,
    removed,
};

struct VolumeEvent {
    VolumeEventType event;
    string path;
};

}

namespace mediascanner {

struct VolumeManagerPrivate {
    MediaStore& store;
    MetadataExtractor& extractor;
    InvalidationSender& invalidator;

    map<string, unique_ptr<SubtreeWatcher>> volumes;
    vector<VolumeEvent> pending;
    unsigned int idle_id = 0;

    VolumeManagerPrivate(MediaStore& store, MetadataExtractor& extractor,
                         InvalidationSender& invalidator);
    ~VolumeManagerPrivate();

    void queueAddVolume(const string& path);
    bool queueRemoveVolume(const string& path);

    void addVolume(const string& path);
    bool removeVolume(const string& path);
    void readFiles(const string& subdir, const MediaType type);

    static gboolean processNext(void *user_data);
};

VolumeManager::VolumeManager(MediaStore& store, MetadataExtractor& extractor,
                             InvalidationSender& invalidator)
    : p(new VolumeManagerPrivate(store, extractor, invalidator)) {
}

VolumeManager::~VolumeManager() = default;

void VolumeManager::queueAddVolume(const string& path) {
    p->queueAddVolume(path);
}

bool VolumeManager::queueRemoveVolume(const string& path) {
    return p->queueRemoveVolume(path);
}

VolumeManagerPrivate::VolumeManagerPrivate(MediaStore& store,
                                           MetadataExtractor& extractor,
                                           InvalidationSender& invalidator)
    : store(store), extractor(extractor), invalidator(invalidator) {
}

VolumeManagerPrivate::~VolumeManagerPrivate()
{
    if (idle_id != 0) {
        g_source_remove(idle_id);
    }
}

void VolumeManagerPrivate::queueAddVolume(const string& path) {
    addVolume(path);
}

bool VolumeManagerPrivate::queueRemoveVolume(const string& path) {
    return removeVolume(path);
}

void VolumeManagerPrivate::addVolume(const string& path) {
    assert(path[0] == '/');
    if(volumes.find(path) != volumes.end()) {
        return;
    }
    if(is_rootlike(path)) {
        fprintf(stderr, "Directory %s looks like a top level root directory, skipping it (%s).\n",
                path.c_str(), __PRETTY_FUNCTION__);
        return;
    }
    if(is_optical_disc(path)) {
        fprintf(stderr, "Directory %s looks like an optical disc, skipping it.\n", path.c_str());
        return;
    }
    if(has_scanblock(path)) {
        fprintf(stderr, "Directory %s has a scan block file, skipping it.\n", path.c_str());
        return;
    }
    unique_ptr<SubtreeWatcher> sw(new SubtreeWatcher(store, extractor, invalidator));
    store.restoreItems(path);
    store.pruneDeleted();
    readFiles(path, AllMedia);
    sw->addDir(path);
    volumes[path] = move(sw);
}

bool VolumeManagerPrivate::removeVolume(const string& path) {
    assert(path[0] == '/');
    if(volumes.find(path) == volumes.end())
        return false;
    store.archiveItems(path);
    volumes.erase(path);
    return true;
}

void VolumeManagerPrivate::readFiles(const string &subdir, const MediaType type) {
    Scanner s(&extractor, subdir, type);
    MediaStoreTransaction txn = store.beginTransaction();
    const int update_interval = 10; // How often to send invalidations.
    struct timespec previous_update, current_time;
    clock_gettime(CLOCK_MONOTONIC, &previous_update);
    previous_update.tv_sec -= update_interval/2; // Send the first update sooner for better visual appeal.
    while(true) {
        try {
            auto d = s.next();
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            while(g_main_context_pending(g_main_context_default())) {
                g_main_context_iteration(g_main_context_default(), FALSE);
            }
            if(current_time.tv_sec - previous_update.tv_sec >= update_interval) {
                txn.commit();
                invalidator.invalidate();
                previous_update = current_time;
            }
            // If the file is broken or unchanged, use fallback.
            if (store.is_broken_file(d.filename, d.etag)) {
                fprintf(stderr, "Using fallback data for unscannable file %s.\n", d.filename.c_str());
                store.insert(extractor.fallback_extract(d));
                continue;
            }
            if(d.etag == store.getETag(d.filename))
                continue;

            try {
                store.insert_broken_file(d.filename, d.etag);
                MediaFile media;
                try {
                    media = extractor.extract(d);
                } catch (const runtime_error &e) {
                    fprintf(stderr, "Error extracting from '%s': %s\n",
                            d.filename.c_str(), e.what());
                    media = extractor.fallback_extract(d);
                }
                store.insert(std::move(media));
            } catch(const exception &e) {
                fprintf(stderr, "Error when indexing: %s\n", e.what());
            }
        } catch(const StopIteration &stop) {
            break;
        }
    }
    txn.commit();
}

}
