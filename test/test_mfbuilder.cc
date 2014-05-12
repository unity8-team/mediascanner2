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

#include<gtest/gtest.h>
#include"mediascanner/MediaFile.hh"
#include"mediascanner/MediaFileBuilder.hh"

using namespace mediascanner;

class MFBTest : public ::testing::Test {
 protected:
  MFBTest() {
  }

  virtual ~MFBTest() {
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(MFBTest, basic) {
    MediaType type(AudioMedia);
    std::string fname("abc");
    std::string title("def");
    std::string date("ghi");
    std::string author("jkl");
    std::string album("mno");
    std::string album_artist("pqr");
    std::string etag("stu");
    std::string content_type("vwx");
    int track_number = 13;
    int duration = 99;

    MediaFileBuilder b(fname);

    b.setType(type);
    b.setTitle(title);
    b.setDate(date);
    b.setAuthor(author);
    b.setAlbum(album);
    b.setAlbumArtist(album_artist);
    b.setTrackNumber(track_number);
    b.setDuration(duration);
    b.setETag(etag);
    b.setContentType(content_type);

    // Now see if data survives a round trip.
    MediaFile mf = b.build();
    ASSERT_EQ(mf.getType(), type);
    ASSERT_EQ(mf.getFileName(), fname);
    ASSERT_EQ(mf.getTitle(), title);
    ASSERT_EQ(mf.getDate(), date);
    ASSERT_EQ(mf.getAuthor(), author);
    ASSERT_EQ(mf.getAlbum(), album);
    ASSERT_EQ(mf.getAlbumArtist(), album_artist);
    ASSERT_EQ(mf.getTrackNumber(), track_number);
    ASSERT_EQ(mf.getDuration(), duration);
    ASSERT_EQ(mf.getETag(), etag);
    ASSERT_EQ(mf.getContentType(), content_type);

    MediaFileBuilder mfb2(mf);
    MediaFile mf2 = mfb2.build();
    ASSERT_EQ(mf, mf2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
