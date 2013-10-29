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

#include "../mozilla/fts3_tokenizer.h"
#include"MediaStore.hh"
#include"MediaFile.hh"
#include"utils.hh"
#include <sqlite3.h>
#include <cstdio>
#include <stdexcept>
#include<cassert>

using namespace std;

struct MediaStorePrivate {
    vector<MediaFile> files;
    sqlite3 *db;
};

extern "C" void sqlite3Fts3PorterTokenizerModule(
    sqlite3_tokenizer_module const**ppModule);

int register_tokenizer(sqlite3 *db) {
    int rc;
    const sqlite3_tokenizer_module *p = NULL;
    sqlite3_stmt *pStmt;
    const char *zSql = "SELECT fts3_tokenizer(?, ?)";

    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, 0);
    if( rc!=SQLITE_OK ){
        return rc;
    }

    sqlite3_bind_text(pStmt, 1, "mozporter", -1, SQLITE_STATIC);
    sqlite3Fts3PorterTokenizerModule(&p);
    sqlite3_bind_blob(pStmt, 2, &p, sizeof(p), SQLITE_STATIC);
    sqlite3_step(pStmt);

    return sqlite3_finalize(pStmt);
}

static void execute_sql(sqlite3 *db, const string &cmd) {
    char *errmsg;
    sqlite3_exec(db, cmd.c_str(), nullptr, nullptr, &errmsg);
    if(errmsg) {
        throw runtime_error(errmsg);
    }
}

void create_tables(sqlite3 *db) {
    string mediaCreate(R"(CREATE TABLE mediaroot (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    mountpoint TEXT
);

CREATE TABLE media (
    filename TEXT PRIMARY KEY NOT NULL,
    --mediaroot INTEGER REFERENCES mediaroot(id),
    title TEXT,
    artist TEXT,    -- Only relevant to audio
    album TEXT,     -- Only relevant to audio
    duration INTEGER,
    type INTEGER   -- 0=Audio, 1=Video
);

CREATE VIRTUAL TABLE media_fts USING fts4(content="media", title, artist, album, tokenize=mozporter);
)");
    string triggerCreate(R"(CREATE TRIGGER media_bu BEFORE UPDATE ON media BEGIN
  DELETE FROM media_fts WHERE docid=old.rowid;
END;

CREATE TRIGGER media_au AFTER UPDATE ON media BEGIN
  INSERT INTO media_fts(docid, title, artist, album) VALUES (new.rowid, new.title, new.artist, new.album);
END;

CREATE TRIGGER media_bd BEFORE DELETE ON media BEGIN
  DELETE FROM media_fts WHERE docid=old.rowid;
END;

CREATE TRIGGER media_ai AFTER INSERT ON media BEGIN
  INSERT INTO media_fts(docid, title, artist, album) VALUES (new.rowid, new.title, new.artist, new.album);
END;
)");
    execute_sql(db, mediaCreate);
    execute_sql(db, triggerCreate);
}

int incrementer(void* arg, int /*num_cols*/, char **/*data*/, char **/*colnames*/) {
    (*((size_t*) arg))++;
    return 0;
}

MediaStore::MediaStore(const std::string &filename_base) {
    p = new MediaStorePrivate();
    string fname = filename_base + "-mediastore.db";
    if(sqlite3_open(fname.c_str(), &p->db) != SQLITE_OK) {
        string s = sqlite3_errmsg(p->db);
        throw s;
    }
    if (register_tokenizer(p->db) != SQLITE_OK) {
        string s = sqlite3_errmsg(p->db);
        throw s;
    }
    create_tables(p->db);
}

MediaStore::~MediaStore() {
    sqlite3_close(p->db);
    delete p;
}

size_t MediaStore::size() const {
    size_t result = 0;
    char *err;
    if(sqlite3_exec(p->db, "SELECT * FROM music;", incrementer, &result, &err) != SQLITE_OK) {
        string s = err;
        throw s;
    }
    if(sqlite3_exec(p->db, "SELECT * FROM video;", incrementer, &result, &err) != SQLITE_OK) {
        string s = err;
        throw s;
    }
    return result;
}

static int yup(void* arg, int /*num_cols*/, char **/*data*/, char ** /*colnames*/) {
    bool *t = reinterpret_cast<bool *> (arg);
    *t = true;
    return 0;
}

void MediaStore::insert(const MediaFile &m) {
    char *errmsg;
    p->files.push_back(m);
    // SQL injection here.
    const char *insert_templ = "INSERT INTO media VALUES(%s, %s, %s, %s, %d, %d);";
    const char *query_templ = "SELECT * FROM media WHERE filename=%s;";
    const size_t bufsize = 1024;
    char qcmd[bufsize];
    char icmd[bufsize];

    string fname = sqlQuote(m.getFileName());
    string title;
    if(m.getTitle().empty())
        title = sqlQuote(filenameToTitle(m.getFileName()));
    else
        title = sqlQuote(m.getTitle());
    string author = sqlQuote(m.getAuthor());
    string album = sqlQuote(m.getAlbum());
    int duration = m.getDuration();
    int type = (int)m.getType();
    snprintf(qcmd, bufsize, query_templ, fname.c_str());
    snprintf(icmd, bufsize, insert_templ, fname.c_str(), title.c_str(),
            author.c_str(), album.c_str(), duration, type);

    bool was_in = false;
    if(sqlite3_exec(p->db, qcmd, yup, &was_in, &errmsg ) != SQLITE_OK) {
        string s = errmsg;
        throw s;
    }
    if(was_in) {
        return;
    }
    if(sqlite3_exec(p->db, icmd, NULL, NULL, &errmsg) != SQLITE_OK) {
        string s = errmsg;
        throw s;
    }
    const char *typestr = m.getType() == AudioMedia ? "song" : "video";
    printf("Added %s to backing store: %s\n", typestr, m.getFileName().c_str());
    printf(" author   : '%s'\n", m.getAuthor().c_str());
    printf(" title    : %s\n", title.c_str());
    printf(" album    : '%s'\n", m.getAlbum().c_str());
    printf(" duration : %d\n", m.getDuration());
}

void MediaStore::remove(const string &fname) {
    const char *templ = "DELETE FROM %s WHERE filename = '%s';";
    char cmd[1024];
    sprintf(cmd, templ, "music", fname.c_str());
    char *errmsg;
    if(sqlite3_exec(p->db, cmd, NULL, NULL, &errmsg) != SQLITE_OK) {
        throw runtime_error(errmsg);
    }
    sprintf(cmd, templ, "video", fname.c_str());
    if(sqlite3_exec(p->db, cmd, NULL, NULL, &errmsg) != SQLITE_OK) {
        throw runtime_error(errmsg);
    }
}


static int media_adder(void* arg, int num_cols, char **data, char ** /*colnames*/) {
    assert(num_cols == 6);
    vector<MediaFile> *t = reinterpret_cast<vector<MediaFile> *> (arg);
    string filename(data[0]);
    string title(data[1]);
    string author(data[2]);
    string album(data[3]);
    int duration = atoi(data[4]);
    MediaType type = (MediaType)atoi(data[5]);
    t->push_back(MediaFile(filename, title, author, album, duration, type));
    return 0;
}

vector<MediaFile> MediaStore::query(const std::string &core_term, MediaType type) {
    vector<MediaFile> result;
    const char *templ = R"(SELECT * FROM media
WHERE rowid IN (SELECT docid FROM media_fts WHERE title MATCH %s)
AND type == %d;)";
    string term = sqlQuote(core_term + "*");
    char cmd[1024];
    sprintf(cmd, templ, term.c_str(), (int)type);
    if(sqlite3_exec(p->db, cmd, media_adder, &result, nullptr) != SQLITE_OK) {
        throw runtime_error(sqlite3_errmsg(p->db));
    }
    return result;
}

static int deleteChecker(void* arg, int /*num_cols*/, char **data, char ** /*colnames*/) {
    vector<string> *t = reinterpret_cast<vector<string>*> (arg);
    const char *fname = data[0];
    FILE *f = fopen(fname, "r");
    if(f) {
        fclose(f);
    } else {
        t->push_back(fname);
    }
    return 0;
}

void MediaStore::pruneDeleted() {
    vector<string> deleted;
    const char *query = "SELECT filename FROM music;";
    char *errmsg;
    if(sqlite3_exec(p->db, query, deleteChecker, &deleted, &errmsg) != SQLITE_OK) {
        string s = errmsg;
        throw s;
    }
    printf("%d files deleted from disk.\n", (int)deleted.size());
    for(const auto &i : deleted) {
        remove(i);
    }
}

