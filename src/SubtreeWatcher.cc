/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * Authors:
 *    Jussi Pakkanen <jussi.pakkanen@canonical.com>
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

#include"SubtreeWatcher.hh"
#include"MediaStore.hh"
#include"MediaFile.hh"

#include<sys/select.h>
#include<stdexcept>
#include<sys/inotify.h>
#include<dirent.h>
#include<sys/stat.h>
#include<unistd.h>
#include<cstring>
#include<cerrno>
#include<string>
#include<memory>

using namespace std;

SubtreeWatcher::SubtreeWatcher(MediaStore &store) : store(store) {
    inotifyid = inotify_init();
    keep_going = true;
    if(inotifyid == -1) {
        string msg("Could not init inotify: ");
        msg += strerror(errno);
        throw runtime_error(msg);
    }
}

SubtreeWatcher::~SubtreeWatcher() {
    for(auto &i : wd2str) {
        inotify_rm_watch(inotifyid, i.first);
    }
    close(inotifyid);
}

void SubtreeWatcher::addDir(const string &root) {
    if(root[0] != '/')
        throw runtime_error("Path must be absolute.");
    if(str2wd.find(root) != str2wd.end())
        return;
    unique_ptr<DIR, int(*)(DIR*)> dir(opendir(root.c_str()), closedir);
    if(!dir) {
        return;
    }
    int wd = inotify_add_watch(inotifyid, root.c_str(),
            IN_CREATE | IN_DELETE_SELF | IN_DELETE | IN_CLOSE_WRITE |
            IN_MOVED_FROM | IN_MOVED_TO | IN_ONLYDIR);
    if(wd == -1) {
        string msg("Could not create inotify watch object: ");
        msg += strerror(errno);
        throw runtime_error(msg);
    }
    wd2str[wd] = root;
    str2wd[root] = wd;
    printf("Watching subdirectory %s, %ld watches in total.\n", root.c_str(), (long)wd2str.size());
    unique_ptr<struct dirent, void(*)(void*)> entry((dirent*)malloc(sizeof(dirent) + NAME_MAX),
                    free);
    struct dirent *de;
        while(readdir_r(dir.get(), entry.get(), &de) == 0 && de ) {
        struct stat statbuf;
        string fname = entry.get()->d_name;
        if(fname == "." || fname == "..") // Maybe ignore all entries starting with a period?
            continue;
        string fullpath = root + "/" + fname;
        stat(fullpath.c_str(), &statbuf);
        if(S_ISDIR(statbuf.st_mode)) {
            addDir(fullpath);
        }
    }
}

bool SubtreeWatcher::removeDir(const string &abspath) {
    if(str2wd.find(abspath) == str2wd.end())
        return false;
    int wd = str2wd[abspath];
    inotify_rm_watch(inotifyid, wd);
    wd2str.erase(wd);
    str2wd.erase(abspath);
    printf("Stopped watching %s, %ld directories remain.\n", abspath.c_str(), (long)wd2str.size());
    if(wd2str.empty())
        keep_going = false;
    return true;
}

void SubtreeWatcher::fileAdded(const string &abspath) {
    printf("New file was created: %s.\n", abspath.c_str());
    try {
        MediaFile m(abspath);
        store.insert(m);
    } catch(const exception &e) {
        fprintf(stderr, "Error when adding new file: %s\n", e.what());
    }
}

void SubtreeWatcher::fileDeleted(const string &abspath) {
    printf("File was deleted: %s\n", abspath.c_str());
    store.remove(abspath);
}

void SubtreeWatcher::dirAdded(const string &abspath) {
    printf("New directory was created: %s.\n", abspath.c_str());
    addDir(abspath);
}

void SubtreeWatcher::dirRemoved(const string &abspath) {
    printf("Subdirectory was deleted: %s.\n", abspath.c_str());
    removeDir(abspath);
}


void SubtreeWatcher::pumpEvents() {
    char buf[BUFSIZE];
    if(wd2str.empty())
        return;
    while(true) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(inotifyid, &reads);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        if(select(inotifyid+1, &reads, nullptr, nullptr, &timeout) <= 0) {
            return;
        }
        ssize_t num_read;
        num_read = read(inotifyid, buf, BUFSIZE);
        if(num_read == 0) {
            printf("Inotify returned 0.\n");
            break;
        }
        if(num_read == -1) {
            printf("Read error.\n");
            break;
        }
        for(char *p = buf; p < buf + num_read;) {
            struct inotify_event *event = (struct inotify_event *) p;
            string directory = wd2str[event->wd];
            string filename(event->name);
            string abspath = directory + '/' + filename;
            bool is_dir = false;
            bool is_file = false;
            struct stat statbuf;
            stat(abspath.c_str(), &statbuf);
            // Remember: these are not valid in case of delete event.
            if(S_ISDIR(statbuf.st_mode))
                is_dir = true;
            if(S_ISREG(statbuf.st_mode))
                is_file = true;

            if(event->mask & IN_CREATE) {
                if(is_dir)
                    dirAdded(abspath);
                // Do not add files upon creation because we can't parse
                // their metadata until it is fully written.
            } else if((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)) {
                if(is_dir)
                    dirAdded(abspath);
                if(is_file)
                    fileAdded(abspath);
            } else if((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
                if(str2wd.find(abspath) != str2wd.end())
                    dirRemoved(abspath);
                else
                    fileDeleted(abspath);
            } else if((event->mask & IN_IGNORED) || (event->mask & IN_UNMOUNT) || (event->mask & IN_DELETE_SELF)) {
                removeDir(abspath);
            }
            p += sizeof(struct inotify_event) + event->len;
        }
    }
}