#include "test.h"

#include <base/detect.h>

#if defined(CONF_FAMILY_UNIX)
#include <base/io.h>

#include <engine/shared/config.h>
#include <engine/shared/fifo.h>

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

TEST(Fifo, KeepExistingFifo)
{
	CTestInfo Info;
	ASSERT_EQ(mkfifo(Info.m_aFilename, 0600), 0);
	// Keep the existing fifo open, like a process that writes commands to it
	const int ExistingFifo = open(Info.m_aFilename, O_RDONLY | O_NONBLOCK);
	ASSERT_GE(ExistingFifo, 0);

	CFifo Fifo;
	Fifo.Init(nullptr, Info.m_aFilename, CFGFLAG_SERVER);

	struct stat Existing;
	ASSERT_EQ(fstat(ExistingFifo, &Existing), 0);
	struct stat Current;
	ASSERT_EQ(stat(Info.m_aFilename, &Current), 0);
	EXPECT_TRUE(S_ISFIFO(Current.st_mode));
	EXPECT_EQ(Existing.st_dev, Current.st_dev);
	EXPECT_EQ(Existing.st_ino, Current.st_ino);

	Fifo.Shutdown();
	close(ExistingFifo);
}

TEST(Fifo, ReplaceNonFifoFile)
{
	CTestInfo Info;
	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
	ASSERT_TRUE(File);
	EXPECT_FALSE(io_close(File));

	CFifo Fifo;
	Fifo.Init(nullptr, Info.m_aFilename, CFGFLAG_SERVER);

	struct stat Attribute;
	ASSERT_EQ(stat(Info.m_aFilename, &Attribute), 0);
	EXPECT_TRUE(S_ISFIFO(Attribute.st_mode));

	Fifo.Shutdown();
}
#endif
