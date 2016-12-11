//
// Created by Lectem on 23/10/2016.
//

#include "dc6.h"
#include <fmt/format.h>
#include "palette.h"
#include "utils.h"

namespace WorldStone
{
void DC6::Decode(const char* filename)
{
    assert(!stream);
    stream = std::make_unique<FileStream>(filename);
    if (stream && stream->good()) {
        extractHeaders();
    }
    }

    void DC6::Decode(StreamPtr&& streamPtr)
    {
        assert(!stream);
        stream = std::move(streamPtr);
        if (stream && stream->good()) {
            extractHeaders();
        }
    }

    bool DC6::extractHeaders()
    {
        if (stream->read(&header, sizeof(header), 1) != 1) return false;
        size_t framesNumber = header.directions * header.frames_per_dir;
        frameHeaders.resize(framesNumber);

        framePointers.resize(framesNumber);
        if (static_cast<size_t>(
                stream->read(framePointers.data(), sizeof(uint32_t), framesNumber)) != framesNumber)
            return false;

        for (size_t i = 0; i < framesNumber; ++i)
        {
            FrameHeader& frameHeader = frameHeaders[i];
            stream->seek(framePointers[i], Stream::beg);
            if (stream->read(&frameHeader, sizeof(frameHeader), 1) != 1) return false;
            fmt::print("\nframe {}\n", i);
            fmt::print("flip {}\n", frameHeader.flip);
            fmt::print("width {}\n", frameHeader.width);
            fmt::print("height {}\n", frameHeader.height);
            fmt::print("offset_x {}\n", frameHeader.offset_x);
            fmt::print("offset_y {}\n", frameHeader.offset_y);
            fmt::print("zeros {}\n", frameHeader.zeros);
            fmt::print("next_block {}\n", frameHeader.next_block);
            fmt::print("length {}\n", frameHeader.length);
        }
        return true;
    }

    std::vector<uint8_t> DC6::decompressFrame(size_t frameNumber) const
    {
        assert(stream != nullptr);
        stream->seek(framePointers[frameNumber] + sizeof(FrameHeader), Stream::beg);
        const FrameHeader& fHeader = frameHeaders[frameNumber];

        // The block can't be bigger than 256 x 256 because of the compression
        assert(fHeader.width > 0 && fHeader.width <= 256 && fHeader.height > 0 &&
               fHeader.height <= 256);
        // Allocate memory for the decoded data, init to 0
        std::vector<uint8_t> data(static_cast<size_t>(fHeader.width * fHeader.height));

        assert(!fHeader.flip);
        // We're reading it bottom to top, but save data with the y axis from top to
        // bottom
        int x = 0, y = fHeader.height - 1;
        int rawIndex = 0;
        while (rawIndex < fHeader.length)
        {
            int val = stream->getc();
            rawIndex++;
            if (val == EOF) throw;
            uint8_t chunkSize = static_cast<uint8_t>(val);
            if (chunkSize == 0x80) // end of line
            {
                x = 0;
                y--;
            }
            else if (chunkSize & 0x80) // chunkSize & 0x80 is the number of transparent pixels
            {
                x += chunkSize & 0x7F;
            }
            else // chunkSize is the number of colors to read
            {
                assert(chunkSize + x <= fHeader.width);
                for (int i = 0; i < chunkSize; i++, x++)
                {
                    int color = stream->getc();
                    rawIndex++;
                    if (color == EOF) throw;
                    assert(x >= 0 && y >= 0 &&
                           x + fHeader.width * y < fHeader.width * fHeader.height);
                    data[static_cast<size_t>(x + fHeader.width * y)] = static_cast<uint8_t>(color);
                }
            }
        }
        assert(fHeader.length == rawIndex);

        return data;
    }

    void DC6::exportToPPM(const char* ppmFilenameBase, const Palette& palette) const
    {
        for (uint32_t dir = 0; dir < header.directions; ++dir)
        {
            for (size_t frameInDir = 0; frameInDir < header.frames_per_dir; ++frameInDir)
            {
                size_t frame = dir * header.frames_per_dir + frameInDir;
                auto   data  = decompressFrame(frame);
                Utils::exportToPPM(
                    fmt::format("{}{}-{}.ppm", ppmFilenameBase, dir, frameInDir).c_str(),
                    data.data(), frameHeaders[frame].width, frameHeaders[frame].height, palette);
            }
        }
    }
}