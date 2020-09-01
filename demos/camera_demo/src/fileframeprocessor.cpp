#include <cerrno>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "fileframeprocessor.hpp"

FileFrameProcessor::FileFrameProcessor(const Options& options) :
    FrameProcessor(options.mVideoType, options.mImageWidth, options.mImageHeight),
    mOutputDirectory(options.mImageOutputDirectory),
    mVerbose(options.mVerbose)
{

}

FileFrameProcessor::~FileFrameProcessor()
{
    term();
}

int FileFrameProcessor::init()
{
    return 0;
}

void FileFrameProcessor::term()
{

}

unsigned char* FileFrameProcessor::getFrame(unsigned index, VideoType videoType) {
    (void) index;
    (void) videoType;
    return nullptr;
}

int FileFrameProcessor::processFrame(FrameBuffer data, size_t length)
{
    // Save the image file
    std::stringstream ss;

    ss << mOutputDirectory << "/capture_" 
       << std::setfill('0') << std::setw(4) << mIndex;
    switch (mVideoType) {
        case JPEG:
            ss << ".jpg";
            break;
        case YUYV:
            ss << ".raw";
            break;
        default:
            std::cout << "Unknown video type " << mVideoType << std::endl;
            return -1;
    }
    
    std::ofstream out(ss.str(), std::ios::out | std::ios::binary);
    if (!out)
    {
        std::perror("Failed to open image file for writing");
        return -1;   
    }

    out.write((const char*) data, length);
    out.close();

    if (!out.good())
    {
        std::perror("Failed to write image content");
        return -1;
    }

    if (mVerbose)
    {
        std::cout << ss.str() << std::endl;
    }

    return 0;
}

