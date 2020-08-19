
#include <argp.h>
#include <chrono>
#include <thread>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>
#include "orientationinputcreator.hpp"
#include "orientationoutputcreator.hpp"
#include "frameprocessorcreator.hpp"
#include "videosensor.hpp"
#include "options.hpp"

static struct argp_option options[] =
{
    { "video_device", 'd', "device",      0, "video device",                      0 },
    { "video_type",   't', "jpeg|yuyv",   0, "video type",                        0 },
    { "width",        'W', "pixels",      0, "image width",                       0 },
    { "height",       'H', "pixels",      0, "image height",                      0 },
    { "flip",         'f', "v|h",         0, "horizontal or vertical image flip", 0 },
    { "monochrome",   'm', NULL,          0, "monochrome image filter",           0 },
    { "framerate",    'r', "num/den",     0, "frame rate fraction",               0 },
    { "out_dir",      'O', "path",        0, "image output directory",            0 },
    { "pos_our",      'o', "servo|print", 0, "angular position output",           0 },
    { "pos_in",       'i', "acc|kbd",     0, "position input",                    0 },
    { "pos_lim",      'l', "val",         0, "angular position bound",            0 },
    { "processor",    'p', "fs|xwin",     0, "frame processor",                   0 },
    { "verbose",      'v', NULL,          0, "verbose output",                    0 },
    { NULL,           0,   NULL,          0, NULL,                                0 },
};

static error_t parseOpt(int key, char * arg, struct argp_state * state)
{
    Options * opt = static_cast<Options *>(state->input);
    std::istringstream ss(arg != NULL ? arg : "");
    char delim;

    switch (key)
    {
        case 'd':
            ss >> opt->mVideoDevice;
            break;

        case 't':
            if (ss.str() == "jpeg")
            {
                opt->mVideoType = JPEG;
            }
            else if (ss.str() == "yuyv")
            {
                opt->mVideoType = YUYV;
            }
            else
            {
                argp_usage(state);
                argp_error(state, "invalid -t argument '%s'", arg);
            }
            break;

        case 'W':
            ss >> opt->mImageWidth;
            break;

        case 'H':
            ss >> opt->mImageHeight;
            break;

        case 'f':
            if (ss.str() == "v")
            {
                opt->mImageVerticalFlip = true;
            }
            else if (ss.str() == "h")
            {
                opt->mImageHorizontalFlip = true;
            }
            else
            {
                argp_error(state, "invalid -f argument '%s'", arg);
            }

            break;

        case 'r':
            ss >> opt->mFrameRateNumerator;
            ss >> delim;
            ss >> opt->mFrameRateDenominator;
            if ((opt->mFrameRateNumerator == 0) || (opt->mFrameRateDenominator == 0))
            {
                argp_error(state, "invalid -r argument '%s'", arg);
            }
            break;

        case 'O':
            ss >> opt->mImageOutputDirectory;
            break;

        case 'o':
            if (ss.str() == "servo")
            {
                opt->mOutputType = PiServo;
            }
            else if (ss.str() == "print")
            {
                opt->mOutputType = Print;
            }
            else
            {
                argp_usage(state);
                argp_error(state, "invalid -o argument '%s'", arg);
            }
            break;

        case 'i':
            if (ss.str() == "acc")
            {
                opt->mInputType = Freespace;
            }
            else if (ss.str() == "kbd")
            {
                opt->mInputType = Keyboard;
            }
            else
            {
                argp_usage(state);
                argp_error(state, "invalid -i argument '%s'", arg);
            }
            break;

        case 'p':
            if (ss.str() == "fs")
            {
                opt->mProcessorType = Filesystem;
            }
            else if (ss.str() == "xwin")
            {
                opt->mProcessorType = XWindows;
            }
            else
            {
                argp_usage(state);
                argp_error(state, "invalid -p argument '%s'", arg);
            }
            break;

        case 'l':
            ss >> opt->mAngularPositionLimit;
            break;

        case 'm':
            opt->mImageMonochrome = true;
            break;

        case 'v':
            opt->mVerbose = true;
            break;
    }

    return 0;
}

static void parseArgs(int argc, char * argv[], Options * opt)
{
    struct argp argp;
    argp.options = options;
    argp.parser = parseOpt;
    argp.args_doc = "test";
    argp.doc = "Embedded application based on camera, position input and position driver";
    argp.children = NULL;
    argp.help_filter = NULL;
    argp.argp_domain = NULL;

    argp_parse(&argp, argc, argv, 0, 0, opt);
}

int main(int argc, char *argv[])
{
    int rv;
    Options options;
    parseArgs(argc, argv, &options);

    OrientationOutput * orientationOutput = OrientationOutputCreator::get(
        options.mOutputType, options.mAngularPositionLimit, options.mVerbose);

    OrientationInput * orientationInput = OrientationInputCreator::get(
        options.mInputType, orientationOutput->getUpdateCallback(),
        -options.mAngularPositionLimit, options.mAngularPositionLimit);

    FrameProcessor * frameProcessor = FrameProcessorCreator::get(
        options.mProcessorType, options.mVideoType,
        options.mImageWidth, options.mImageHeight,
        options.mImageMonochrome, options.mImageOutputDirectory, options.mVerbose);

    VideoSensor * videoSensor = new VideoSensor(
            frameProcessor->getProcessFrameCallback(),
            options.mVideoDevice,
            options.mVideoType,
            options.mImageHorizontalFlip, options.mImageVerticalFlip,
            options.mImageWidth, options.mImageHeight,
            options.mFrameRateNumerator, options.mFrameRateDenominator);

    rv = orientationOutput->init();
    if (rv != 0)
    {
        return -1;
    }

    rv = orientationInput->init();
    if (rv != 0)
    {
        return -1;
    }

    rv = frameProcessor->init();
    if (rv != 0)
    {
        return -1;
    }

    rv = videoSensor->init();
    if (rv != 0)
    {
        videoSensor->term();
        return -1;
    }

    rv = videoSensor->captureEnable(true);
    if (rv != 0)
    {
        return -1;
    }

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1  ));
    }

    rv = videoSensor->captureEnable(false);
    if (rv != 0)
    {
        return -1;
    }

    delete orientationOutput;
    delete orientationInput;
    delete frameProcessor;
    delete videoSensor;

    return 0;
}
