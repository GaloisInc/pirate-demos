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

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <time.h>

#include "frameprocessor.hpp"
#include "imageconvert.hpp"
#include "options.hpp"

class VideoSource
{
public:
    VideoSource(const Options& options,
        const std::vector<std::shared_ptr<FrameProcessor>>& frameProcessors);
    virtual ~VideoSource();

    virtual int init();
    virtual void term();

protected:
    ImageConvert mImageConvert;
    const std::vector<std::shared_ptr<FrameProcessor>>& mFrameProcessors;
    const bool mVerbose;
    const VideoType mVideoOutputType;
    const unsigned mOutputWidth;
    const unsigned mOutputHeight;
    unsigned mIndex, mSnapshotIndex;
    time_t mSnapshotTime;

    int process(FrameBuffer data, size_t length, DataStreamType dataStream);
};
