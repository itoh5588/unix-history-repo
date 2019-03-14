/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * This software was developed by BFF Storage Systems, LLC under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

extern "C" {
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "mockfs.hh"
#include "utils.hh"

using namespace testing;

class Write: public FuseTest {

public:

void expect_lookup(const char *relpath, uint64_t ino)
{
	FuseTest::expect_lookup(relpath, ino, S_IFREG | 0644, 1);
}

void expect_release(uint64_t ino, ProcessMockerT r)
{
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_RELEASE &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke(r));
}

void require_sync_resize_0() {
	const char *sync_resize_node = "vfs.fuse.sync_resize";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(sync_resize_node, &val, &size, NULL, 0))
		<< strerror(errno);
	if (val != 0)
		FAIL() << "vfs.fuse.sync_resize must be set to 0 for this test."
			"  That sysctl will probably be removed soon.";
}

};

class AioWrite: public Write {
virtual void SetUp() {
	const char *node = "vfs.aio.enable_unsafe";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	// TODO: With GoogleTest 1.8.2, use SKIP instead
	if (!val)
		FAIL() << "vfs.aio.enable_unsafe must be set for this test";
	FuseTest::SetUp();
}
};

/* Tests for the write-through cache mode */
class WriteThrough: public Write {

virtual void SetUp() {
	const char *cache_mode_node = "vfs.fuse.data_cache_mode";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(cache_mode_node, &val, &size, NULL, 0))
		<< strerror(errno);
	// TODO: With GoogleTest 1.8.2, use SKIP instead
	if (val != 1)
		FAIL() << "vfs.fuse.data_cache_mode must be set to 1 "
			"(writethrough) for this test";

	FuseTest::SetUp();
}

};

/* Tests for the writeback cache mode */
class WriteBack: public Write {

virtual void SetUp() {
	const char *node = "vfs.fuse.data_cache_mode";
	int val = 0;
	size_t size = sizeof(val);

	ASSERT_EQ(0, sysctlbyname(node, &val, &size, NULL, 0))
		<< strerror(errno);
	// TODO: With GoogleTest 1.8.2, use SKIP instead
	if (val != 2)
		FAIL() << "vfs.fuse.data_cache_mode must be set to 2 "
			"(writeback) for this test";
	FuseTest::SetUp();
}

};

/* AIO writes need to set the header's pid field correctly */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236379 */
TEST_F(AioWrite, DISABLED_aio_write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	uint64_t offset = 4096;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	struct aiocb iocb, *piocb;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, offset, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	iocb.aio_nbytes = bufsize;
	iocb.aio_fildes = fd;
	iocb.aio_buf = (void *)CONTENTS;
	iocb.aio_offset = offset;
	iocb.aio_sigevent.sigev_notify = SIGEV_NONE;
	ASSERT_EQ(0, aio_write(&iocb)) << strerror(errno);
	ASSERT_EQ(bufsize, aio_waitcomplete(&piocb, NULL)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * When a file is opened with O_APPEND, we should forward that flag to
 * FUSE_OPEN (tested by Open.o_append) but still attempt to calculate the
 * offset internally.  That way we'll work both with filesystems that
 * understand O_APPEND (and ignore the offset) and filesystems that don't (and
 * simply use the offset).
 *
 * Note that verifying the O_APPEND flag in FUSE_OPEN is done in the
 * Open.o_append test.
 */
TEST_F(Write, append)
{
	const ssize_t BUFSIZE = 9;
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS[BUFSIZE] = "abcdefgh";
	uint64_t ino = 42;
	/* 
	 * Set offset to a maxbcachebuf boundary so we don't need to RMW when
	 * using writeback caching
	 */
	uint64_t initial_offset = m_maxbcachebuf;
	int fd;

	require_sync_resize_0();

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, initial_offset);
	expect_write(ino, initial_offset, BUFSIZE, BUFSIZE, 0, CONTENTS);

	/* Must open O_RDWR or fuse(4) implicitly sets direct_io */
	fd = open(FULLPATH, O_RDWR | O_APPEND);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(BUFSIZE, write(fd, CONTENTS, BUFSIZE)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Write, append_direct_io)
{
	const ssize_t BUFSIZE = 9;
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS[BUFSIZE] = "abcdefgh";
	uint64_t ino = 42;
	uint64_t initial_offset = 4096;
	int fd;

	expect_lookup(RELPATH, ino);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_getattr(ino, initial_offset);
	expect_write(ino, initial_offset, BUFSIZE, BUFSIZE, 0, CONTENTS);

	fd = open(FULLPATH, O_WRONLY | O_APPEND);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(BUFSIZE, write(fd, CONTENTS, BUFSIZE)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* A direct write should evict any overlapping cached data */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235774 */
TEST_F(Write, DISABLED_direct_io_evicts_cache)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char CONTENTS0[] = "abcdefgh";
	const char CONTENTS1[] = "ijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS0) + 1;
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, bufsize);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS1);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	// Prime cache
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);

	// Write directly, evicting cache
	ASSERT_EQ(0, fcntl(fd, F_SETFL, O_DIRECT)) << strerror(errno);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, write(fd, CONTENTS1, bufsize)) << strerror(errno);

	// Read again.  Cache should be bypassed
	expect_read(ino, 0, bufsize, bufsize, CONTENTS1);
	ASSERT_EQ(0, fcntl(fd, F_SETFL, 0)) << strerror(errno);
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	ASSERT_STREQ(readbuf, CONTENTS1);

	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* 
 * When the direct_io option is used, filesystems are allowed to write less
 * data than requested
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236381 */
TEST_F(Write, DISABLED_direct_io_short_write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefghijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	ssize_t halfbufsize = bufsize / 2;
	const char *halfcontents = CONTENTS + halfbufsize;

	expect_lookup(RELPATH, ino);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, halfbufsize, 0, CONTENTS);
	expect_write(ino, halfbufsize, halfbufsize, halfbufsize, 0,
		halfcontents);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * An insidious edge case: the filesystem returns a short write, and the
 * difference between what we requested and what it actually wrote crosses an
 * iov element boundary
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236381 */
TEST_F(Write, DISABLED_direct_io_short_write_iov)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS0 = "abcdefgh";
	const char *CONTENTS1 = "ijklmnop";
	const char *EXPECTED0 = "abcdefghijklmnop";
	const char *EXPECTED1 = "hijklmnop";
	uint64_t ino = 42;
	int fd;
	ssize_t size0 = strlen(CONTENTS0) - 1;
	ssize_t size1 = strlen(CONTENTS1) + 1;
	ssize_t totalsize = size0 + size1;
	struct iovec iov[2];

	expect_lookup(RELPATH, ino);
	expect_open(ino, FOPEN_DIRECT_IO, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, totalsize, size0, 0, EXPECTED0);
	expect_write(ino, size0, size1, size1, 0, EXPECTED1);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	iov[0].iov_base = (void*)CONTENTS0;
	iov[0].iov_len = strlen(CONTENTS0);
	iov[1].iov_base = (void*)CONTENTS1;
	iov[1].iov_len = strlen(CONTENTS1);
	ASSERT_EQ(totalsize, writev(fd, iov, 2)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * If the kernel cannot be sure which uid, gid, or pid was responsible for a
 * write, then it must set the FUSE_WRITE_CACHE bit
 */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=236378 */
// TODO: check vfs.fuse.mmap_enable
TEST_F(Write, DISABLED_mmap)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	void *p;
	uint64_t offset = 10;
	size_t len;
	void *zeros, *expected;

	len = getpagesize();

	zeros = calloc(1, len);
	ASSERT_NE(NULL, zeros);
	expected = calloc(1, len);
	ASSERT_NE(NULL, expected);
	memmove((uint8_t*)expected + offset, CONTENTS, bufsize);

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, len);
	expect_read(ino, 0, len, len, zeros);
	/* 
	 * Writes from the pager may or may not be associated with the correct
	 * pid, so they must set FUSE_WRITE_CACHE
	 */
	expect_write(ino, 0, len, len, FUSE_WRITE_CACHE, expected);
	expect_release(ino, ReturnErrno(0));

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ASSERT_NE(MAP_FAILED, p) << strerror(errno);

	memmove((uint8_t*)p + offset, CONTENTS, bufsize);

	ASSERT_EQ(0, munmap(p, len)) << strerror(errno);
	close(fd);	// Write mmap'd data on close

	free(expected);
	free(zeros);
}

TEST_F(Write, pwrite)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	uint64_t offset = 4096;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, offset, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, pwrite(fd, CONTENTS, bufsize, offset))
		<< strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

TEST_F(Write, write)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* fuse(4) should not issue writes of greater size than the daemon requests */
TEST_F(Write, write_large)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	int *contents;
	uint64_t ino = 42;
	int fd;
	ssize_t halfbufsize, bufsize;

	halfbufsize = m_mock->m_max_write;
	bufsize = halfbufsize * 2;
	contents = (int*)malloc(bufsize);
	ASSERT_NE(NULL, contents);
	for (int i = 0; i < (int)bufsize / (int)sizeof(i); i++) {
		contents[i] = i;
	}

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, halfbufsize, halfbufsize, 0, contents);
	expect_write(ino, halfbufsize, halfbufsize, halfbufsize, 0,
		&contents[halfbufsize / sizeof(int)]);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, contents, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */

	free(contents);
}

TEST_F(Write, write_nothing)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = 0;

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);

	fd = open(FULLPATH, O_WRONLY);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* In writeback mode, dirty data should be written on close */
TEST_F(WriteBack, close)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_SETATTR);
		}, Eq(true)),
		_)
	).WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
	}));
	expect_release(ino, ReturnErrno(0));

	fd = open(FULLPATH, O_RDWR);
	ASSERT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	close(fd);
}

/*
 * Without direct_io, writes should be committed to cache
 */
TEST_F(WriteBack, writeback)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* 
	 * A subsequent read should be serviced by cache, without querying the
	 * filesystem daemon
	 */
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * With O_DIRECT, writes should be not committed to cache.  Admittedly this is
 * an odd test, because it would be unusual to use O_DIRECT for writes but not
 * reads.
 */
TEST_F(WriteBack, o_direct)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);
	expect_read(ino, 0, bufsize, bufsize, CONTENTS);

	fd = open(FULLPATH, O_RDWR | O_DIRECT);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* A subsequent read must query the daemon because cache is empty */
	ASSERT_EQ(0, lseek(fd, 0, SEEK_SET)) << strerror(errno);
	ASSERT_EQ(0, fcntl(fd, F_SETFL, 0)) << strerror(errno);
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/*
 * Without direct_io, writes should be committed to cache
 */
/* 
 * Disabled because we don't yet implement write-through caching.  No bugzilla
 * entry, because that's a feature request, not a bug.
 */
TEST_F(WriteThrough, DISABLED_writethrough)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);
	char readbuf[bufsize];

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	expect_getattr(ino, 0);
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* 
	 * A subsequent read should be serviced by cache, without querying the
	 * filesystem daemon
	 */
	ASSERT_EQ(bufsize, read(fd, readbuf, bufsize)) << strerror(errno);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}

/* With writethrough caching, writes update the cached file size */
/* https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235775 */
TEST_F(WriteThrough, DISABLED_update_file_size)
{
	const char FULLPATH[] = "mountpoint/some_file.txt";
	const char RELPATH[] = "some_file.txt";
	const char *CONTENTS = "abcdefgh";
	struct stat sb;
	uint64_t ino = 42;
	int fd;
	ssize_t bufsize = strlen(CONTENTS);

	expect_lookup(RELPATH, ino);
	expect_open(ino, 0, 1);
	EXPECT_CALL(*m_mock, process(
		ResultOf([=](auto in) {
			return (in->header.opcode == FUSE_GETATTR &&
				in->header.nodeid == ino);
		}, Eq(true)),
		_)
	).Times(2)
	.WillRepeatedly(Invoke([=](auto in, auto out) {
		out->header.unique = in->header.unique;
		SET_OUT_HEADER_LEN(out, attr);
		out->body.attr.attr.ino = ino;	// Must match nodeid
		out->body.attr.attr.mode = S_IFREG | 0644;
		out->body.attr.attr.size = 0;
		out->body.attr.attr_valid = UINT64_MAX;
	}));
	expect_write(ino, 0, bufsize, bufsize, 0, CONTENTS);

	fd = open(FULLPATH, O_RDWR);
	EXPECT_LE(0, fd) << strerror(errno);

	ASSERT_EQ(bufsize, write(fd, CONTENTS, bufsize)) << strerror(errno);
	/* Get cached attributes */
	ASSERT_EQ(0, fstat(fd, &sb)) << strerror(errno);
	ASSERT_EQ(bufsize, sb.st_size);
	/* Deliberately leak fd.  close(2) will be tested in release.cc */
}
