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


#ifndef TINYTIFF_TOOLS_HXX
#define TINYTIFF_TOOLS_HXX

extern "C" {
    #include "tinytiffwriter.h"
    #include "tinytiffreader.h"
}
#include <string>

/*! \defgroup tinytiffwriter_CXX TinyTIFFWriter: Optional C++-Interface (include-only!)
    \ingroup tinytiffwriter

    All functions and tenplates in this group are extensions to the basic TinyTIFFWriter libarary. They
    are all header-only (or templates), so no link library other than libTinyTIFF is required.

*/

/*! \defgroup tinytiffreader_CXX TinyTIFFReader: Optional C++-Interface (include-only!)
    \ingroup tinytiffreader

    All functions in this group are extensions to the basic TinyTIFFReader libarary. They
    are all header-only (or templates), so no link library other than libTinyTIFF is required.

*/


/** \brief this template allows to determine a value for TinyTIFFWriterSampleFormat from a given type \a T
 *  \ingroup tinytiffwriter_CXX
 *
 *  The format can be determined from the type \c TDATA by :
 *  \code
 *    TinyTIFFWriterFile* tiff = TinyTIFFWriter_open(filename, sizeof(TDATA)*8, TinyTIFF_SampleFormatFromType<TDATA>().format, 1, width, height, TinyTIFFWriter_Greyscale);
 *  \endcode
 */
template<class T>
struct TinyTIFF_SampleFormatFromType {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_UInt;
};

template<>
struct TinyTIFF_SampleFormatFromType<float> {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_Float;
};
template<>
struct TinyTIFF_SampleFormatFromType<double> {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_Float;
};


template<>
struct TinyTIFF_SampleFormatFromType<int8_t> {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_Int;
};
template<>
struct TinyTIFF_SampleFormatFromType<int16_t> {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_Int;
};
template<>
struct TinyTIFF_SampleFormatFromType<int32_t> {
    const enum TinyTIFFWriterSampleFormat format=TinyTIFFWriter_Int;
};


#endif // TINYTIFF_TOOLS_HXX
