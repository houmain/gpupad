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
#ifndef TIFYTIFF_DEFS_H
#define TIFYTIFF_DEFS_H


#ifndef TINYTIFF_TRUE
/** \brief a logic value of TRUE, e.g. used by TinyTIFFWriter_wasError() and TinyTIFFReader_wasError()
  * \ingroup tinytiffwriter
  * \ingroup tinytiffreader
  */
#  define TINYTIFF_TRUE 1
#endif
#ifndef TINYTIFF_FALSE
/** \brief a logic value of FALSE, e.g. used by TinyTIFFWriter_wasError() and TinyTIFFReader_wasError()
  * \ingroup tinytiffwriter
  * \ingroup tinytiffreader
  */
#  define TINYTIFF_FALSE 0
#endif


/** \brief possible return value of TinyTIFFReader_getSampleFormat(), indicating unsigned integer data
  * \ingroup tinytiffreader
  *
  * \see TinyTIFFReader_getSampleFormat(), TINYTIFF_SAMPLEFORMAT_FLOAT, TINYTIFF_SAMPLEFORMAT_INT, TINYTIFF_SAMPLEFORMAT_UNDEFINED

  */
#define TINYTIFF_SAMPLEFORMAT_UINT 1
/** \brief possible return value of TinyTIFFReader_getSampleFormat(), indicating integer data
  * \ingroup tinytiffreader
  *
  * \see TinyTIFFReader_getSampleFormat(), TINYTIFF_SAMPLEFORMAT_FLOAT, TINYTIFF_SAMPLEFORMAT_UNDEFINED, TINYTIFF_SAMPLEFORMAT_UINT

  */
#define TINYTIFF_SAMPLEFORMAT_INT 2
/** \brief possible return value of TinyTIFFReader_getSampleFormat(), indicating floating-point data
  * \ingroup tinytiffreader
  *
  * \see TinyTIFFReader_getSampleFormat(), TINYTIFF_SAMPLEFORMAT_UNDEFINED, TINYTIFF_SAMPLEFORMAT_INT, TINYTIFF_SAMPLEFORMAT_UINT

  */
#define TINYTIFF_SAMPLEFORMAT_FLOAT 3
/** \brief possible return value of TinyTIFFReader_getSampleFormat(), indicating undefined data
  * \ingroup tinytiffreader
  *
  * \see TinyTIFFReader_getSampleFormat(), TINYTIFF_SAMPLEFORMAT_FLOAT, TINYTIFF_SAMPLEFORMAT_INT, TINYTIFF_SAMPLEFORMAT_UINT
  */
#define TINYTIFF_SAMPLEFORMAT_UNDEFINED 4



/** \brief describes the layout of multi-sample images in memory
  * \ingroup tinytiffwriter
  *
  */
enum TinyTIFFSampleLayout {
    TinyTIFF_Interleaved,                 /*!< samples are stored in interleaved/chunky order, i.e. \c R1G1B1R2G2B2R3G3B3... for 3-sample RGB-images. */
    TinyTIFF_Chunky=TinyTIFF_Interleaved, /*!< \copydoc TinyTIFFSampleLayout::TinyTIFF_Interleaved */
    TinyTIFF_Separate,                    /*!< samples are stored in separate/planar memory areas, i.e. \c R1R2R3...G1G2G3....B1B2B3... for 3-sample RGB-images. */
    TinyTIFF_Planar=TinyTIFF_Separate     /*!< \copydoc TinyTIFFSampleLayout::TinyTIFF_Separate */
};
#endif // TIFYTIFF_DEFS_H
