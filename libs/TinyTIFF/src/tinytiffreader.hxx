/*
    Copyright (c) 2008-2020 Jan W. Krieger (<jan@jkrieger.de>), German Cancer Research Center (DKFZ) & IWR, University of Heidelberg

    This software is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License (LGPL) as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


*/


#ifndef TINYTIFFREADER_HXX
#define TINYTIFFREADER_HXX

extern "C" {
    #include "tinytiffreader.h"
}
#include <string>



/*! \brief template function that internally calls TinyTIFFReader_getSampleData() and copies the data into the specified output buffer
    \ingroup tinytiffreader_CXX

\tparam Tin datatype of the sample in the TIFF file
    \tparam Tout datatype of \a buffer
    \param tif the TIFF file to read from
    \param buffer the buffer to write into

    This function may be used in reader code like this:
\code
    bool intReadFrameFloat(float *data) {
    if (!tif) return false;

    uint32_t wwidth=TinyTIFFReader_getWidth(tif);
    uint32_t hheight=TinyTIFFReader_getHeight(tif);
    if (!(wwidth>0 && hheight>0)) tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("error in file '%1': frame %2 is too small\n").arg(filename).arg(frame));
    else {
        uint16_t sformat=TinyTIFFReader_getSampleFormat(tif);
        uint16_t bits=TinyTIFFReader_getBitsPerSample(tif);

        if (sformat==TINYTIFF_SAMPLEFORMAT_UINT) {
            if (bits==8) TinyTIFFReader_readFrame<uint8_t, float>(tif, data);
            else if (bits==16) TinyTIFFReader_readFrame<uint16_t, float>(tif, data);
            else if (bits==32) TinyTIFFReader_readFrame<uint32_t, float>(tif, data);
            else {
                tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("frame %1 has a datatype not convertible to float (type=%2, bitspersample=%3)\n").arg(frame).arg(sformat).arg(bits));
                return false;
            }
        } else if (sformat==TINYTIFF_SAMPLEFORMAT_INT) {
            if (bits==8) TinyTIFFReader_readFrame<int8_t, float>(tif, data);
            else if (bits==16) TinyTIFFReader_readFrame<int16_t, float>(tif, data);
            else if (bits==32) TinyTIFFReader_readFrame<int32_t, float>(tif, data);
            else {
                tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("frame %1 has a datatype not convertible to float (type=%2, bitspersample=%3)\n").arg(frame).arg(sformat).arg(bits));
                return false;
            }
        } else if (sformat==TINYTIFF_SAMPLEFORMAT_FLOAT) {
            if (bits==32) TinyTIFFReader_readFrame<float, float>(tif, data);
            else {
                tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("frame %1 has a datatype not convertible to float (type=%2, bitspersample=%3)\n").arg(frame).arg(sformat).arg(bits));
                return false;
            }
        } else {
            tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("frame %1 has a datatype not convertible to float (type=%2, bitspersample=%3)\n").arg(frame).arg(sformat).arg(bits));
            return false;
        }
    }
    if (TinyTIFFReader_wasError(tif)) {
        tinyTIFFErrorHandler("QFImageReaderTinyTIFF", QObject::tr("error reading frame %1: %2\n").arg(frame).arg(TinyTIFFReader_getLastError(tif)));
        return false;
    }

    return true;
}
\endcode
    */
template <class Tin, class Tout>
inline void TinyTIFFReader_readFrame(TinyTIFFReaderFile* tif, Tout* buffer, uint16_t sample=0) {
    uint32_t wwidth=TinyTIFFReader_getWidth(tif);
    uint32_t hheight=TinyTIFFReader_getHeight(tif);
    Tin* tmp=(Tin*)calloc(wwidth*hheight, sizeof(Tin));
    TinyTIFFReader_getSampleData(tif, tmp, sample);
    for (uint32_t i=0; i<wwidth*hheight; i++) {
        buffer[i]=tmp[i];
    }
    free(tmp);
}

#endif // TINYTIFFREADER_HXX
