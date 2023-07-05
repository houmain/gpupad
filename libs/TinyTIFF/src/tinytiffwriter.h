/*
    Copyright (c) 2008-220 Jan W. Krieger (<jan@jkrieger.de>), German Cancer Research Center (DKFZ) & IWR, University of Heidelberg

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





#ifndef TINYTIFFWRITER_H
#define TINYTIFFWRITER_H

#include "tinytiff_export.h"
#include "tinytiff_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*! \defgroup tinytiffwriter Tiny TIFF writer library
    \ingroup tinytiff_maingroup

   The methods in this file allow to write TIFF files with limited capabilites,
   but very fast. 
   
   \see \ref mainpagetinytiff_writer for details on how this library works
 */


/*! \defgroup tinytiffwriter_C TinyTIFFWriter: C-Interface
   \ingroup tinytiffwriter

*/


/** \brief struct used to describe a TIFF file
  * \ingroup tinytiffwriter_C
  */
typedef struct TinyTIFFWriterFile TinyTIFFWriterFile; // forward



#ifdef __cplusplus
extern "C" {
#endif

    /** \brief returns the version number of TinyTIFFReader
      * \ingroup tinytiffreader_C
      */
    TINYTIFF_EXPORT const char* TinyTIFFWriter_getVersion();

    /** \brief maximum size of the imageDescription field in the first frame (including trailing \c 0, which has to be present!)
      * \ingroup tinytiffwriter_C
      */
    TINYTIFF_EXPORT int TinyTIFFWriter_getMaxDescriptionTextSize();

    /*! \brief returns a pointer to the last error message
        \ingroup tinytiffwriter_C

        \param tiff TIFF file

        \note the pointer is accessible as long as the TIFF file has not been closed using TINYTIFFWRITER_close()
    */
    TINYTIFF_EXPORT const char* TinyTIFFWriter_getLastError(TinyTIFFWriterFile* tiff);

    /*! \brief returns TRUE (non-zero) when there was an error in the last function call, or FALSE (zero) if there was no error
        \ingroup tinytiffwriter_C

        \param tiff TIFF file

    */
    TINYTIFF_EXPORT int TinyTIFFWriter_wasError(TinyTIFFWriterFile* tiff);

    /*! \brief returns TINYTIFF_TRUE (non-zero) when there was no error in the last function call, or TINYTIFF_FALSE if there was an error
        \ingroup tinytiffwriter_C

        \param tiff TIFF file

        */
    TINYTIFF_EXPORT int TinyTIFFWriter_success(TinyTIFFWriterFile* tiff);

    /** \brief allows to specify in TinyTIFFWriter_open() how to interpret the image channels
     *  \ingroup tinytiffwriter_C
     *
     *  \see TinyTIFFWriter_open()
     */
    enum TinyTIFFWriterSampleInterpretation {
        TinyTIFFWriter_AutodetectSampleInterpetation, /*!< tells TinyTIFFWriter_open() to try and determine the sample interpretation automatically from the sumber of samples! */
        TinyTIFFWriter_Greyscale, /*!< Greyscale image with one channel (samples need to be at least 1) */
        TinyTIFFWriter_GreyscaleAndAlpha, /*!< Greyscale image with one channel and one additional ALPHA channel (samples need to be at least 2) */
        TinyTIFFWriter_RGB, /*!< RGB image with three channel (samples need to be at least 3) */
        TinyTIFFWriter_RGBA /*!< RGBA image with four channel (samples need to be at least 4) */
    };

    /** \brief allows to specify in TinyTIFFWriter_open() the type of the data
     *  \ingroup tinytiffwriter_C
     *
     *  \see TinyTIFFWriter_open()
     */
    enum TinyTIFFWriterSampleFormat {
        TinyTIFFWriter_UInt, /*!< unsigned integer images (the default) */
        TinyTIFFWriter_Int, /*!< signed integer images */
        TinyTIFFWriter_Float /*!< floating point images */
    };

    /*! \brief create a new TIFF file
        \ingroup tinytiffwriter_C

        \param filename name of the new TIFF file
        \param number of samples per pixel (e.g. 3 for RGB images)
        \param bitsPerSample bits used to save each sample of the images
        \param samples number of samples in each frame, See documentation of TinyTIFFWriterSampleInterpretation for limitations that apply, if set to 0, the
                       number of samples is automatically determined from the value of \a sampleInterpretation
        \param width width of the images in pixels
        \param height height of the images in pixels
        \param sampleFormat data type of data in image pixels
        \param sampleInterpretation how to interpret the samples in each frame. Note that you may specify more samples than required by the value from TinyTIFFWriterSampleInterpretation
                                    these are then marked as unspecified samples. If you want to store alpha information or else, choose the correct value from TinyTIFFWriterSampleInterpretation!
                                    if TinyTIFFWriter_AutodetectSampleInterpetation is specified, the sample interpretation is choosen from the number of channels (as specified in TinyTIFFWriterSampleInterpretation,
                                    i.e. 1 channel=greyscale, 2 channels=greyscale+alpha, 3 channels=RGB, 4 channels= RGBA)
        \return a new TinyTIFFWriterFile pointer on success, or NULL on errors

        \see TinyTIFFWriterSampleInterpretation

      */
    TINYTIFF_EXPORT TinyTIFFWriterFile* TinyTIFFWriter_open(const char* filename, uint16_t bitsPerSample, enum TinyTIFFWriterSampleFormat sampleFormat, uint16_t samples, uint32_t width, uint32_t height, enum TinyTIFFWriterSampleInterpretation sampleInterpretation);



    /** \brief write a new image to the give TIFF file. the image ist stored in separate planes or planar configuration, dependeing on \a outputOrganization and
     *         the data is possibly reorganized
     *  \ingroup tinytiffwriter_C
     *
     *  \note if \a outputOrganization does not match \a inputOrganisation. Note that such a reorganization requires additional
     *        time and memory! This can be seen in the following image comparing the performance of non-reordered writing (left)
     *        and writing with reordering
     *        \image html tinytiffwriter_performance_rgb.png
     *
     *  \param tiff TIFF file to write to
     *  \param data points to the image in row-major ordering with the right bit-depth,
     *              multi-sample data has to be provided in the format defined by \a inputOrganisation
     *  \param inputOrganisation data format of the multi-channel data in \a data
     *  \param outputOrganization data format of the image data in the generated TIFF file
     *  \return TINYTIFF_TRUE on success and TINYTIFF_FALSE on failure.
     *           An error description can be obtained by calling TinyTIFFWriter_getLastError().
     */
    TINYTIFF_EXPORT int TinyTIFFWriter_writeImageMultiSample(TinyTIFFWriterFile* tiff, const void* data, enum TinyTIFFSampleLayout inputOrganisation, enum TinyTIFFSampleLayout outputOrganization);


    /*! \brief write a new image to the give TIFF file, in planar configuration, i.e. the image data is reorganized from RGBRGBRGB to RRR...GGG...BBB... before writing.
               This operation requires additional memory and time! Use TinyTIFFWriter_writeImage() for speed
        \ingroup tinytiffwriter_C

        This is equivalent to calling
        \code
          TinyTIFFWriter_writeImageMultiSample(tiff, data, TinyTIFF_Interleaved, TinyTIFF_Separate);
        \endcode

        \param tiff TIFF file to write to
        \param data points to the image in row-major ordering with the right bit-depth,
                    multi-sample data has to be provided in the "chunky" format, e.g. if
                    you have 3 samples ("R", "G" and "B"), the the data in this field has to
                    be \c R1G1B1|R2G2B2|R3G3B3|R4G4B4|...
        \return TINYTIFF_TRUE on success and TINYTIFF_FALSE on failure.
                An error description can be obtained by calling TinyTIFFWriter_getLastError().


       \note Note that the reorganization from chunky to planar requires additional
             time and memory! This can be seen in the following image comparing the performance of non-reordered writing (left)
             and writing with reordering
             \image html tinytiffwriter_performance_rgb.png
    */
    TINYTIFF_EXPORT int TinyTIFFWriter_writeImagePlanarReorder(TinyTIFFWriterFile* tiff, const void* data);


    /*! \brief write a new image to the give TIFF file, in planar configuration, i.e. the image data is reorganized from RGBRGBRGB to RRR...GGG...BBB... before writing.
               This operation requires additional memory and time! Use TinyTIFFWriter_writeImage() for speed
        \ingroup tinytiffwriter_C

        This is equivalent to calling
        \code
          TinyTIFFWriter_writeImageMultiSample(tiff, data, TinyTIFF_Separate, TinyTIFF_Interleaved);
        \endcode

        \param tiff TIFF file to write to
        \param data points to the image in row-major ordering with the right bit-depth,
                multi-sample data has to be provided in the "planat" format, e.g. if
                you have 3 samples ("R", "G" and "B"), the the data in this field has to
                be \c R1R2R3R4...G1G2G3G4...B1B2B3B4...
        \return TINYTIFF_TRUE on success and TINYTIFF_FALSE on failure.
                An error description can be obtained by calling TinyTIFFWriter_getLastError().

        \note Note that the reorganization from planar to chunky requires additional
              time and memory! This can be seen in the following image comparing the performance of non-reordered writing (left)
              and writing with reordering
              \image html tinytiffwriter_performance_rgb.png
    */
    TINYTIFF_EXPORT int TinyTIFFWriter_writeImageChunkyReorder(TinyTIFFWriterFile* tiff, const void* data);


    /*! \brief Write a new image to the give TIFF file, in chunky configuration, expects the data to be chunky too.
               This method is here for compatibility with older version on TinyTIFFWriter an for simple semantics for
               1-sample data, where the organization does not play any role!
        \ingroup tinytiffwriter_C

        This is equivalent to calling
        \code
          TinyTIFFWriter_writeImageMultiSample(tiff, data, TinyTIFF_Interleaved, TinyTIFF_Interleaved);
        \endcode

        \param tiff TIFF file to write to
        \param data points to the image in row-major ordering with the right bit-depth,
                multi-sample data has to be provided in the "chunky" format, e.g. if
                you have 3 samples ("R", "G" and "B"), the the data in this field has to
                be \c R1G1B1|R2G2B2|R3G3B3|R4G4B4|...
        \return TINYTIFF_TRUE on success and TINYTIFF_FALSE on failure.
                An error description can be obtained by calling TinyTIFFWriter_getLastError().
    */
    TINYTIFF_EXPORT int TinyTIFFWriter_writeImage(TinyTIFFWriterFile* tiff, const void* data);

    /*! \brief close a given TIFF file
        \ingroup tinytiffwriter_C

        \param tiff TIFF file to close
        \param pixel_width pixel width in nanometers
        \param pixel_height pixel width in nanometers
        \param deltaz in a multi-frame-TIFF distance between image planes in nanometers
        \param frametime in a multi-frame-TIFF frametime in seconds

        This functions writes some additional data into the ImageDescription field of the first frame, if it is proved (!=0!!!).
        It also writes the image count there. The ImageDescription finally has the form:
    \verbatim
        TinyTIFFWriter_version=1.1
        images=1000
        pixel_width=100
        pixel_height=100
        deltaz=100
        frametime=1e-4
    \endverbatim

    This function also releases memory allocated in TinyTIFFWriter_open() in \a tiff.
    */
    TINYTIFF_EXPORT void TinyTIFFWriter_close_withmetadatadescription(TinyTIFFWriterFile* tiff, double pixel_width, double pixel_height, double frametime, double deltaz);

    /*! \brief close a given TIFF file
        \ingroup tinytiffwriter_C

        \param tiff TIFF file to close


        This function also releases memory allocated in TinyTIFFWriter_open() in \a tiff.
    */
    TINYTIFF_EXPORT void TinyTIFFWriter_close(TinyTIFFWriterFile* tiff);


    /*! \brief close a given TIFF file and write the given string into the IMageDescription tag of the first frame in the file.
        \ingroup tinytiffwriter_C

        \param tiff TIFF file to close
        \param imageDescription ImageDescription tag contents (max. size: TINYTIFFWRITER_DESCRIPTION_SIZE, including trailing 0!!!)


        This function also releases memory allocated in TinyTIFFWriter_open() in \a tiff.
     */
    TINYTIFF_EXPORT void TinyTIFFWriter_close_withdescription(TinyTIFFWriterFile* tiff, const char* imageDescription);
#ifdef __cplusplus
}
#endif

#endif // TINYTIFFWRITER_H

