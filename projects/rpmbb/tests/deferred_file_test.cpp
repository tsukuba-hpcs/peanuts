#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "rpmbb/deferred_file.hpp"
#include <doctest/doctest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

using namespace rpmbb;

TEST_CASE("Testing deferred_file class") {
  const std::string test_file = "test_file.txt";
  const std::string test_content = "Hello, World!";

  SUBCASE("Create, open, and close file") {
    deferred_file df_create(test_file, O_CREAT | O_RDWR, 0666);
    df_create.open();
    CHECK(df_create.is_open());

    deferred_file df_open(test_file, O_RDWR, 0);
    df_open.open();
    CHECK(df_open.is_open());

    df_create.close();
    df_open.close();
    CHECK_FALSE(df_create.is_open());
    CHECK_FALSE(df_open.is_open());

    unlink(test_file.c_str());
  }

  SUBCASE("Write and read from file") {
    deferred_file df_write(test_file, O_CREAT | O_RDWR, 0666);
    df_write.open();
    auto written_bytes = df_write.pwrite(test_content, 0);
    CHECK(written_bytes == static_cast<ssize_t>(test_content.size()));

    deferred_file df_read(test_file, O_RDWR, 0);
    std::vector<char> read_buffer(test_content.size());
    auto read_bytes = df_read.pread(read_buffer, 0);
    CHECK(read_bytes == static_cast<ssize_t>(test_content.size()));
    CHECK(std::equal(test_content.begin(), test_content.end(),
                     read_buffer.begin(), read_buffer.end()));
    unlink(test_file.c_str());
  }

  SUBCASE("Check file size") {
    deferred_file df_size(test_file, O_CREAT | O_RDWR, 0666);
    df_size.pwrite(test_content, 0);
    CHECK(df_size.size() == test_content.size());

    unlink(test_file.c_str());
  }

  SUBCASE("Check inode number") {
    deferred_file df1(test_file, O_CREAT | O_RDWR, 0666);
    auto inode1 = df1.ino();

    deferred_file df2(test_file, O_RDWR, 0);
    auto inode2 = df2.ino();

    CHECK(inode1 == inode2);

    unlink(test_file.c_str());
  }
}
