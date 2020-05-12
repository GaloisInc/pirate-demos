/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2020 Two Six Labs, LLC.  All rights reserved.
 */

#include <stdlib.h>
#include "libpirate.h"
#include "libpirate_internal.h"
#include "channel_test.hpp"

namespace GAPS
{

ChannelTest::ChannelTest() : testing::Test() ,
    len({DEFAULT_START_LEN, DEFAULT_STOP_LEN, DEFAULT_STEP_LEN}),
    gapsTag(GAPS_TAG_NONE)
{
}

void ChannelTest::SetUp()
{
    int rv;
    errno = 0;

    Writer.buf = (uint8_t *) malloc(len.stop);
    ASSERT_NE(nullptr, Writer.buf);

    Reader.buf = (uint8_t *) malloc(len.stop);
    ASSERT_NE(nullptr, Reader.buf);

    rv = pthread_barrier_init(&barrier, NULL, 2);
    ASSERT_EQ(0, rv);

    pirate_reset_gd();
}

void ChannelTest::TearDown()
{
    if (Writer.buf != NULL)
    {
        free(Writer.buf);
        Writer.buf  = NULL;
    }

    if (Reader.buf != NULL)
    {
        free(Reader.buf);
        Reader.buf = NULL;
    }

    pthread_barrier_destroy(&barrier);
    errno = 0;
}

void ChannelTest::WriteDataInit(ssize_t len)
{
    for (ssize_t i = 0; i < len; ++i)
    {
        Writer.buf[i] = (i + len) & 0xFF; 
    }
}

void ChannelTest::WriterChannelOpen()
{
    int rv;
    char desc[256];

    Writer.gd = pirate_open_param(&Writer.param, O_WRONLY);
    ASSERT_EQ(0, errno);
    ASSERT_GE(Writer.gd, 0);

    rv = pirate_get_channel_description(Writer.gd, desc, sizeof(desc));
    ASSERT_EQ((int)Writer.desc.length(), rv);
    ASSERT_EQ(0, errno);
    ASSERT_STREQ(Writer.desc.c_str(), desc);

    WriterChannelPostOpen();

    rv = pthread_barrier_wait(&barrier);
    ASSERT_TRUE(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
}

void ChannelTest::ReaderChannelOpen()
{
    int rv;
    char desc[256];

    Reader.gd = pirate_open_param(&Reader.param, O_RDONLY);
    ASSERT_EQ(0, errno);
    ASSERT_GE(Reader.gd, 0);

    rv = pirate_get_channel_description(Reader.gd, desc, sizeof(desc));
    ASSERT_EQ((int)Reader.desc.length(), rv);
    ASSERT_EQ(0, errno);
    ASSERT_STREQ(Reader.desc.c_str(), desc);

    ReaderChannelPostOpen();

    rv = pthread_barrier_wait(&barrier);
    ASSERT_TRUE(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
}

void ChannelTest::WriterChannelClose()
{
    int rv;
    
    WriterChannelPreClose();

    rv = pirate_close(Writer.gd);
    ASSERT_EQ(0, errno);
    ASSERT_EQ(0, rv);
}

void ChannelTest::ReaderChannelClose()
{
    int rv;

    ReaderChannelPreClose();
    
    rv = pirate_close(Reader.gd);
    ASSERT_EQ(0, errno);
    ASSERT_EQ(0, rv);
}

void ChannelTest::Run()
{
    RunChildOpen(true);
    if (pirate_pipe_channel_type(Writer.param.channel_type)) {
        RunChildOpen(false);
    }
}

void ChannelTest::RunChildOpen(bool child)
{
    int rv;
    pthread_t WriterId, ReaderId;
    void *WriterStatus, *ReaderStatus;

    childOpen = child;

    ChannelInit();

    if (!childOpen)
    {
        int rv, gd[2] = {-1, -1};
        rv = pirate_pipe_param(gd, &Writer.param, O_RDWR);
        ASSERT_EQ(0, errno);
        ASSERT_EQ(0, rv);
        ASSERT_GE(gd[0], 0);
        ASSERT_GE(gd[1], 0);
        Reader.gd = gd[0];
        Writer.gd = gd[1];
    }

    rv = pthread_create(&ReaderId, NULL, ChannelTest::ReaderThreadS, this);
    ASSERT_EQ(0, rv);

    rv = pthread_create(&WriterId, NULL, ChannelTest::WriterThreadS, this);
    ASSERT_EQ(0, rv);

    rv = pthread_join(ReaderId, &ReaderStatus);
    ASSERT_EQ(0, rv);

    rv = pthread_join(WriterId, &WriterStatus);
    ASSERT_EQ(0, rv);
}

void *ChannelTest::WriterThreadS(void *param)
{
    ChannelTest *inst = static_cast<ChannelTest*>(param);
    inst->WriterTest();
    return NULL;
}

void *ChannelTest::ReaderThreadS(void *param)
{
    ChannelTest *inst = static_cast<ChannelTest*>(param);
    inst->ReaderTest();
    return NULL;
}

void ChannelTest::WriterTest()
{
    if (childOpen)
    {
        WriterChannelOpen();
    }

    memset(&statsWr, 0, sizeof(statsWr));

    for (ssize_t l = len.start; l < len.stop; l += len.step)
    {
        int sts;
        ssize_t rv;

        WriteDataInit(l);

        rv = pirate_write(Writer.gd, gapsTag, Writer.buf, l);
        ASSERT_EQ(l, rv);
        ASSERT_EQ(0, errno);

        statsWr.packets++;
        statsWr.bytes += l;

        sts = pthread_barrier_wait(&barrier);
        ASSERT_TRUE(sts == 0 || sts == PTHREAD_BARRIER_SERIAL_THREAD);
    }

    WriterChannelClose();
}

void ChannelTest::ReaderTest()
{
    gaps_tag_t rdTag = GAPS_TAG_NONE;
    gaps_tag_t *rdTagPtr = gapsTag == GAPS_TAG_NONE ? NULL : &rdTag;

    if (childOpen)
    {
        ReaderChannelOpen();
    }

    memset(&statsRd, 0, sizeof(statsRd));

    for (ssize_t l = len.start; l < len.stop; l += len.step)
    {
        int sts;
        ssize_t rv;

        memset(Reader.buf, 0xFA, l);

        ssize_t remain = l;
        uint8_t *buf = Reader.buf;
        do {
            rv = pirate_read(Reader.gd, rdTagPtr, buf, remain);
            ASSERT_EQ(0, errno);
            ASSERT_GT(rv, 0);
            remain -= rv;
            buf += rv;

        } while (remain > 0);

        if (rdTagPtr != NULL) {
            ASSERT_EQ(rdTag, gapsTag);
        }
        EXPECT_TRUE(0 == std::memcmp(Writer.buf, Reader.buf, l));

        statsRd.packets++;
        statsRd.bytes += l;

        sts = pthread_barrier_wait(&barrier);
        ASSERT_TRUE(sts == 0 || sts == PTHREAD_BARRIER_SERIAL_THREAD);
    }

    ReaderChannelClose();
}

} // namespace
