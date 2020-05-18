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

#include <cstring>
#include <errno.h>
#include <gtest/gtest.h>
#include "libpirate.h"
#include "channel_test.hpp"

// Channel-type agnostic tests
namespace GAPS {

TEST(CommonChannel, InvalidOpen)
{
    pirate_channel_param_t param;
    int rv;

    pirate_init_channel_param(PIPE, &param);

    // Invalid flags
    rv = pirate_open_param(&param, O_RDWR);
    ASSERT_EQ(EINVAL, errno);
    ASSERT_EQ(-1, rv);
    errno = 0;
}

TEST(CommonChannel, InvalidCLose)
{
    int rv;

    // Invalid channel number - negative
    rv = pirate_close(-1);
    ASSERT_EQ(EBADF, errno);
    ASSERT_EQ(-1, rv);
    errno = 0;

    // Invalid channel number - exceeds bound
    rv = pirate_close(PIRATE_NUM_CHANNELS);
    ASSERT_EQ(EBADF, errno);
    ASSERT_EQ(-1, rv);
    errno = 0;

    // Close unopened channel
    rv = pirate_close(0);
    ASSERT_EQ(EBADF, errno);
    ASSERT_EQ(-1, rv);
    errno = 0;
}

TEST(CommonChannel, InvalidReadWrite)
{
    int rv;
    uint8_t buf[16] = { 0 };

    // Read unopened channel
    rv = pirate_read(0, buf, sizeof(buf));
    ASSERT_EQ(-1, rv);
    ASSERT_EQ(EBADF, errno);
    errno = 0;

    // Write unopened channel
    rv = pirate_write(0, buf, sizeof(buf));
    ASSERT_EQ(-1, rv);
    ASSERT_EQ(EBADF, errno);
    errno = 0;
}

TEST(CommonChannel, RegisterEnclave)
{
    int rv;
    pirate_channel_param_t param;

    rv = pirate_declare_enclaves(3, "foo", "baz", "bar");
    ASSERT_EQ(0, rv);
    ASSERT_EQ(0, errno);

    rv = pirate_parse_channel_param("device,/dev/null,src=foo,dst=bar", &param);
    ASSERT_EQ(0, rv);
    ASSERT_EQ(0, errno);

    ASSERT_EQ(3u, param.src_enclave);
    ASSERT_EQ(1u, param.dst_enclave);
}

TEST(CommonChannel, UnparseChannelParam)
{
    char output[80];
    int rv;
    pirate_channel_param_t param;

    rv = pirate_parse_channel_param("device,/dev/null,iov_len=0", &param);
    ASSERT_EQ(0, rv);
    ASSERT_EQ(0, errno);

    rv = pirate_unparse_channel_param(&param, output, 80);
    ASSERT_EQ(26, rv);
    ASSERT_EQ(0, errno);
    ASSERT_STREQ("device,/dev/null,iov_len=0", output);

    rv = pirate_unparse_channel_param(&param, output, 26);
    ASSERT_EQ(26, rv);
    ASSERT_EQ(0, errno);
    ASSERT_STREQ("device,/dev/null,iov_len=", output);

    rv = pirate_unparse_channel_param(&param, output, 25);
    ASSERT_EQ(26, rv);
    ASSERT_EQ(0, errno);    
    ASSERT_STREQ("device,/dev/null,iov_len", output);
}

} // namespace
