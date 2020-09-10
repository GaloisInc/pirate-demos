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
#include <X11/Xlib.h>

#include "imageconvert.hpp"
#include "frameprocessor.hpp"
#include "options.hpp"
#include "orientationinput.hpp"
#include "orientationoutput.hpp"

class XWinFrameProcessor : public FrameProcessor
{
public:
    XWinFrameProcessor(const Options& options, std::shared_ptr<OrientationOutput> orientationOutput, const ImageConvert& imageConvert);
    virtual ~XWinFrameProcessor();

    virtual int init(int frameRateNum, int frameRateDiv) override;
    virtual void term() override;

protected:
    virtual int process(FrameBuffer data, size_t length) override;
    virtual unsigned char* getFrame(unsigned index, VideoType videoType) override;

private:
    std::shared_ptr<OrientationOutput> mOrientationOutput;
    const ImageConvert&                mImageConvert;
    bool                               mMonochrome;
    bool                               mImageSlidingWindow;
    Display*                           mDisplay;
    Window                             mWindow;
    XImage*                            mImage;
    unsigned char*                     mImageBuffer;
    unsigned char*                     mRGBXImageBuffer;
    unsigned char*                     mYUYVImageBuffer;
    GC                                 mContext;
    XGCValues                          mContextVals;

    int xwinDisplayInitialize();
    void xwinDisplayTerminate();
    int convertJpeg(FrameBuffer data, size_t len);
    int convertYuyv(FrameBuffer data, size_t len);
    void slidingWindow();
    void renderImage();
};

