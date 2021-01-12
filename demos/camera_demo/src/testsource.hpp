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

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <stdint.h>
#include <linux/videodev2.h>

#include "imageconvert.hpp"
#include "frameprocessor.hpp"
#include "options.hpp"
#include "videosource.hpp"

class TestSource : public VideoSource
{
public:
    TestSource(const Options& options,
        const std::vector<std::shared_ptr<FrameProcessor>>& frameProcessors);
    virtual ~TestSource();

    virtual int init() override;
    virtual void term() override;

private:
    uint8_t *mBuffer;
    std::thread *mPollThread;
    bool mPoll;
    void pollThread();
    void perturb();
    uint8_t perturbComponent(uint8_t val);
};

