// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include <OpenColorIO/OpenColorIO.h>

#include "ops/Lut1D/Lut1DOp.h"
#include "ops/Lut3D/Lut3DOp.h"
#include "ops/Matrix/MatrixOps.h"
#include "ParseUtils.h"
#include "MathUtils.h"
#include "Logging.h"
#include "pystring/pystring.h"
#include "transforms/FileTransform.h"

/*

Peter Chamberlain
https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=40284#p232952

Resolve LUT Info

3D Format (.cube)
The LUT file needs to have an extension of .cube
While described as a 3D LUT format the .cube file could contain ...
- 3D LUT data (only)
- 1D LUT data (only)
- Both a 3D LUT and a 1D 'shaper' LUT.

Irrespective of what data a .cube file contains (1D, 3D or both), it is always
displayed by Resolve in the 3D LUT section.

Lines beginning with # are considered comments. All comment lines need to be 
placed before the header lines.

3D LUT data (only)
There is a header of 2 lines:
LUT_3D_SIZE N (where N is the number of points along each axis of the LUT.)
LUT_3D_INPUT_RANGE MIN MAX (where MIN is the minimum input value expected to be
passed through the 3D LUT. Like wise for MAX.)

Followed by this are NxNxN rows of RGB values between 0.0 and 1.0. Each row
has 3 floating point numbers separated by a space (space delimited). The data
is ordered as red major (red fastest)

Top few lines of a sample .cube 3D LUT file which would contain 33x33x33 rows
of 3D LUT data and expects input in the range 0.0 1.0:

LUT_3D_SIZE 33
LUT_3D_INPUT_RANGE 0.0 1.0
0.000000 0.000000 0.000000
0.047059 0.000000 0.000000
0.101961 0.000000 0.000000
0.156863 0.000000 0.000000
0.207843 0.000000 0.000000
0.262745 0.000000 0.000000
0.321569 0.000000 0.000000
0.376471 0.000000 0.000000
0.427451 0.000000 0.000000
0.482353 0.000000 0.000000
0.537255 0.000000 0.000000
0.592157 0.000000 0.000000


1D LUT data (only)

The 1D LUT data requires a header with the following two fields
LUT_1D_SIZE N (where N is the number of points in each channel of the 1D LUT)
LUT_1D_INPUT_RANGE MIN MAX (where MIN is the minimum input value expected to be
passed through the 1D LUT. Like wise for MAX.)

This is followed by N data lines with 3 floating point values per line with a
space separating them (first is R, second is G, third is B)

Top few lines of a sample .cube file containing 1D LUT data only. It contains
4096 rows of 1D LUT data and expects input in the range 0.0 1.0:

LUT_1D_SIZE 4096
LUT_1D_INPUT_RANGE 0.0 1.0
0.000000 0.000000 0.000000
0.047059 0.047059 0.047059
0.101961 0.101961 0.101961
0.156863 0.156863 0.156863
0.207843 0.207843 0.207843
0.262745 0.262745 0.262745
0.321569 0.321569 0.321569
0.376471 0.376471 0.376471
0.427451 0.427451 0.427451
0.482353 0.482353 0.482353
0.537255 0.537255 0.537255
0.592157 0.592157 0.592157


1D 'shaper' LUT and 3D LUT data

When a .cube file contains both 1D and 3D LUT data the 1D LUT data is treated
as a 'shaper' LUT and is applied first with the output from the 1D 'shaper' LUT
section then being fed into the 3D LUT section.

A .cube file containing a 1D 'shaper' LUT and 3D LUT data requires a header
with the following fields:
LUT_1D_SIZE N1D (where N1D is the number of points in each channel of the
1D 'shaper' LUT)
LUT_1D_INPUT_RANGE MIN1D MAX1D (where MIN1D is the minimum input value
expected to be passed through the 'shaper' 1D LUT. Like wise for MAX1D.)
LUT_3D_SIZE N3D (where N3D is the number of points along each axis of
the LUT.)
LUT_3D_INPUT_RANGE MIN3D MAX 3D (where MIN3D is the minimum input value
expected to be passed through the 3D LUT. Like wise for MAX3D.)

This is followed the 1D 'shaper' data: N1D data lines with 3 floating point
values per line with a space separating them (first is R, second is G,
third is B).
Followed by the 3D data: N3DxN3DxN3D rows of RGB values between 0.0 and 1.0.
Each row has 3 floating point numbers separated by a space (space delimited).
The data is ordered as red major (red fastest).

Below is an example of a sample .cube file containing 1D 'shaper' LUT and
3D LUT data. It contains 6 rows of 1D LUT data with input in the range 0.0 1.0
and 3x3x3 (27) rows of 3D LUT data with input in the range 0.0 1.0

# Sample 3D cube file containing 1D shaper LUT and 3D LUT.
# 1. The size and range of both the LUTs should be specified first.
# 2. LUT_*D_INPUT_RANGE is an optional field.
# 3. The 1D shaper LUT below inverts the signal
# 4. The 3D LUT which follows inverts the signal again
LUT_1D_SIZE 6
LUT_1D_INPUT_RANGE 0.0 1.0
LUT_3D_SIZE 3
LUT_3D_INPUT_RANGE 0.0 1.0
1.0 1.0 1.0
0.8 0.8 0.8
0.6 0.6 0.6
0.4 0.4 0.4
0.2 0.2 0.2
0.0 0.0 0.0
1.0 1.0 1.0
0.5 1.0 1.0
0.0 1.0 1.0
1.0 0.5 1.0
0.5 0.5 1.0
0.0 0.5 1.0
1.0 0.0 1.0
0.5 0.0 1.0
0.0 0.0 1.0
1.0 1.0 0.5
0.5 1.0 0.5
0.0 1.0 0.5
1.0 0.5 0.5
0.5 0.5 0.5
0.0 0.5 0.5
1.0 0.0 0.5
0.5 0.0 0.5
0.0 0.0 0.5
1.0 1.0 0.0
0.5 1.0 0.0
0.0 1.0 0.0
1.0 0.5 0.0
0.5 0.5 0.0
0.0 0.5 0.0
1.0 0.0 0.0
0.5 0.0 0.0
0.0 0.0 0.0

*/


OCIO_NAMESPACE_ENTER
{
    namespace
    {
        class LocalCachedFile : public CachedFile
        {
        public:
            LocalCachedFile() = default;
            ~LocalCachedFile() = default;

            Lut1DOpDataRcPtr lut1D;
            float range1d_min = 0.0f;
            float range1d_max = 1.0f;

            Lut3DOpDataRcPtr lut3D;
            float range3d_min = 0.0f;
            float range3d_max = 1.0f;
        };

        typedef OCIO_SHARED_PTR<LocalCachedFile> LocalCachedFileRcPtr;



        class LocalFileFormat : public FileFormat
        {
        public:
            
            LocalFileFormat() = default;
            ~LocalFileFormat() = default;
            
            void getFormatInfo(FormatInfoVec & formatInfoVec) const override;
            
            CachedFileRcPtr read(
                std::istream & istream,
                const std::string & fileName) const override;
            
            void bake(const Baker & baker,
                      const std::string & formatName,
                      std::ostream & ostream) const override;
            
            void buildFileOps(OpRcPtrVec & ops,
                              const Config & config,
                              const ConstContextRcPtr & context,
                              CachedFileRcPtr untypedCachedFile,
                              const FileTransform & fileTransform,
                              TransformDirection dir) const override;
        private:
            static void ThrowErrorMessage(const std::string & error,
                const std::string & fileName,
                int line,
                const std::string & lineContent);
        };

        void LocalFileFormat::ThrowErrorMessage(const std::string & error,
            const std::string & fileName,
            int line,
            const std::string & lineContent)
        {
            std::ostringstream os;
            os << "Error parsing Resolve .cube file (";
            os << fileName;
            os << ").  ";
            if (-1 != line)
            {
                os << "At line (" << line << "): '";
                os << lineContent << "'.  ";
            }
            os << error;

            throw Exception(os.str().c_str());
        }

        void LocalFileFormat::getFormatInfo(FormatInfoVec & formatInfoVec) const
        {
            FormatInfo info;
            info.name = "resolve_cube";
            info.extension = "cube";
            info.capabilities = FORMAT_CAPABILITY_READ | FORMAT_CAPABILITY_BAKE;
            formatInfoVec.push_back(info);
        }
        
        CachedFileRcPtr LocalFileFormat::read(
            std::istream & istream,
            const std::string & fileName) const
        {

            // this shouldn't happen
            if(!istream)
            {
                throw Exception ("File stream empty when trying to read Resolve .cube lut");
            }

            // Parse the file
            std::vector<float> raw1d;
            std::vector<float> raw3d;

            int size3d = 0;
            int size1d = 0;

            bool has1d = false;
            bool has3d = false;

            float range1d_min = 0.0f;
            float range1d_max = 1.0f;

            float range3d_min = 0.0f;
            float range3d_max = 1.0f;

            {
                std::string line;
                StringVec parts;
                std::vector<float> tmpfloats;
                int lineNumber = 0;
                bool headerComplete = false;
                int tripletNumber = 0;

                while(nextline(istream, line))
                {
                    ++lineNumber;

                    // All lines starting with '#' are comments
                    if(pystring::startswith(line,"#"))
                    {
                        if(headerComplete)
                        {
                            ThrowErrorMessage(
                                "Comments not allowed after header.",
                                fileName,
                                lineNumber,
                                line);
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Strip, lowercase, and split the line
                    pystring::split(pystring::lower(pystring::strip(line)), parts);
                    if(parts.empty()) continue;

                    if(pystring::lower(parts[0]) == "title")
                    {
                        ThrowErrorMessage(
                            "Unsupported tag: 'TITLE'.",
                            fileName,
                            lineNumber,
                            line);
                    }
                    else if(pystring::lower(parts[0]) == "lut_1d_size")
                    {
                        if(parts.size() != 2
                            || !StringToInt( &size1d, parts[1].c_str()))
                        {
                            ThrowErrorMessage(
                                "Malformed LUT_1D_SIZE tag.",
                                fileName,
                                lineNumber,
                                line);
                        }

                        raw1d.reserve(3*size1d);
                        has1d = true;
                    }
                    else if(pystring::lower(parts[0]) == "lut_2d_size")
                    {
                        ThrowErrorMessage(
                            "Unsupported tag: 'LUT_2D_SIZE'.",
                            fileName,
                            lineNumber,
                            line);
                    }
                    else if(pystring::lower(parts[0]) == "lut_3d_size")
                    {
                        if(parts.size() != 2
                            || !StringToInt( &size3d, parts[1].c_str()))
                        {
                            ThrowErrorMessage(
                                "Malformed LUT_3D_SIZE tag.",
                                fileName,
                                lineNumber,
                                line);
                        }

                        raw3d.reserve(3*size3d*size3d*size3d);
                        has3d = true;
                    }
                    else if(pystring::lower(parts[0]) == "lut_1d_input_range")
                    {
                        if(parts.size() != 3 || 
                            !StringToFloat( &range1d_min, parts[1].c_str()) ||
                            !StringToFloat( &range1d_max, parts[2].c_str()))
                        {
                            ThrowErrorMessage(
                                "Malformed LUT_1D_INPUT_RANGE tag.",
                                fileName,
                                lineNumber,
                                line);
                        }
                    }
                    else if(pystring::lower(parts[0]) == "lut_3d_input_range")
                    {
                        if(parts.size() != 3 || 
                            !StringToFloat( &range3d_min, parts[1].c_str()) ||
                            !StringToFloat( &range3d_max, parts[2].c_str()))
                        {
                            ThrowErrorMessage(
                                "Malformed LUT_3D_INPUT_RANGE tag.",
                                fileName,
                                lineNumber,
                                line);
                        }
                    }
                    else
                    {
                        headerComplete = true;

                        // It must be a float triple!
                        if(!StringVecToFloatVec(tmpfloats, parts) || tmpfloats.size() != 3)
                        {
                            ThrowErrorMessage(
                                "Malformed color triples specified.",
                                fileName,
                                lineNumber,
                                line);
                        }

                        for(int i=0; i<3; ++i)
                        {
                            if(has1d && tripletNumber < size1d)
                            {
                                raw1d.push_back(tmpfloats[i]);
                            }
                            else
                            {
                                raw3d.push_back(tmpfloats[i]);
                            }
                        }

                        ++tripletNumber;
                    }
                }
            }

            // Interpret the parsed data, validate lut sizes

            LocalCachedFileRcPtr cachedFile = LocalCachedFileRcPtr(new LocalCachedFile());

            if(has1d)
            {
                if(size1d != static_cast<int>(raw1d.size()/3))
                {
                    std::ostringstream os;
                    os << "Incorrect number of lut1d entries. ";
                    os << "Found " << raw1d.size() / 3;
                    os << ", expected " << size1d << ".";
                    ThrowErrorMessage(
                        os.str().c_str(),
                        fileName, -1, "");
                }

                // Reformat 1D data
                if(size1d>0)
                {
                    cachedFile->lut1D = std::make_shared<Lut1DOpData>(size1d);

                    cachedFile->lut1D->setFileOutputBitDepth(BIT_DEPTH_F32);

                    cachedFile->range1d_min = range1d_min;
                    cachedFile->range1d_max = range1d_max;

                    auto & lutArray = cachedFile->lut1D->getArray();

                    for (unsigned long i = 0; i < raw1d.size(); ++i)
                    {
                        lutArray[i] = raw1d[i];
                    }
                }
            }
            if(has3d)
            {
                if(size3d*size3d*size3d 
                    != static_cast<int>(raw3d.size()/3))
                {
                    std::ostringstream os;
                    os << "Incorrect number of lut3d entries. ";
                    os << "Found " << raw3d.size() / 3 << ", expected ";
                    os << size3d * size3d * size3d << ".";
                    ThrowErrorMessage(
                        os.str().c_str(),
                        fileName, -1, "");
                }

                // Reformat 3D data
                cachedFile->range3d_min = range3d_min;
                cachedFile->range3d_max = range3d_max;

                cachedFile->lut3D = std::make_shared<Lut3DOpData>(size3d);
                cachedFile->lut3D->setFileOutputBitDepth(BIT_DEPTH_F32);
                cachedFile->lut3D->setArrayFromRedFastestOrder(raw3d);
            }
            if(!has1d && !has3d)
            {
                ThrowErrorMessage(
                    "Lut type (1D/3D) unspecified.",
                    fileName, -1, "");
            }

            return cachedFile;
        }
        
        void LocalFileFormat::bake(const Baker & baker,
                                   const std::string & formatName,
                                   std::ostream & ostream) const
        {

            const int DEFAULT_1D_SIZE = 4096;
            const int DEFAULT_SHAPER_SIZE = 4096;
            const int DEFAULT_3D_SIZE = 64;

            if(formatName != "resolve_cube")
            {
                std::ostringstream os;
                os << "Unknown cube format name, '";
                os << formatName << "'.";
                throw Exception(os.str().c_str());
            }

            //
            // Initialize config and data
            //

            ConstConfigRcPtr config = baker.getConfig();

            int onedSize = baker.getCubeSize();
            if(onedSize==-1) onedSize = DEFAULT_1D_SIZE;
            if(onedSize<2)
            {
                std::ostringstream os;
                os << "1D LUT size must be higher than 2 (was " << onedSize << ")";
                throw Exception(os.str().c_str());
            }

            int cubeSize = baker.getCubeSize();
            if(cubeSize==-1) cubeSize = DEFAULT_3D_SIZE;
            cubeSize = std::max(2, cubeSize); // smallest cube is 2x2x2

            int shaperSize = baker.getShaperSize();
            if(shaperSize<0) shaperSize = DEFAULT_SHAPER_SIZE;
            if(shaperSize<2)
            {
                std::ostringstream os;
                os << "A shaper space ('" << baker.getShaperSpace() << "') has";
                os << " been specified, so the shaper size must be 2 or larger";
                throw Exception(os.str().c_str());
            }

            // Get spaces from baker
            const std::string shaperSpace = baker.getShaperSpace();
            const std::string inputSpace = baker.getInputSpace();
            const std::string targetSpace = baker.getTargetSpace();
            const std::string looks = baker.getLooks();

            //
            // Determine required LUT type
            //

            const int CUBE_1D = 1; // 1D LUT version number
            const int CUBE_3D = 2; // 3D LUT version number
            const int CUBE_1D_3D = 3; // 3D LUT with 1D prelut

            ConstProcessorRcPtr inputToTargetProc;
            if (!looks.empty())
            {
                LookTransformRcPtr transform = LookTransform::Create();
                transform->setLooks(looks.c_str());
                transform->setSrc(inputSpace.c_str());
                transform->setDst(targetSpace.c_str());
                inputToTargetProc = config->getProcessor(transform,
                    TRANSFORM_DIR_FORWARD);
            }
            else
            {
                inputToTargetProc = config->getProcessor(
                    inputSpace.c_str(),
                    targetSpace.c_str());
            }

            int required_lut = -1;

            if(inputToTargetProc->hasChannelCrosstalk())
            {
                if(shaperSpace.empty())
                {
                    // Has crosstalk, but no shaper, so need 3D LUT
                    required_lut = CUBE_3D;
                }
                else
                {
                    // Crosstalk with shaper-space
                    required_lut = CUBE_1D_3D;
                }
            }
            else
            {
                // No crosstalk
                required_lut = CUBE_1D;
            }

            if(required_lut == -1)
            {
                // Unnecessary paranoia
                throw Exception(
                    "Internal logic error, LUT type was not determined");
            }

            //
            // Generate Shaper
            //

            std::vector<float> shaperData;

            float fromInStart = 0;
            float fromInEnd = 1;

            if(required_lut == CUBE_1D_3D)
            {
                // TODO: Later we only grab the green channel for the prelut,
                // should ensure the prelut is monochromatic somehow?
                
                ConstProcessorRcPtr inputToShaperProc = config->getProcessor(
                    inputSpace.c_str(),
                    shaperSpace.c_str());

                if(inputToShaperProc->hasChannelCrosstalk())
                {
                    // TODO: Automatically turn shaper into
                    // non-crosstalked version?
                    std::ostringstream os;
                    os << "The specified shaperSpace, '" << baker.getShaperSpace();
                    os << "' has channel crosstalk, which is not appropriate for";
                    os << " shapers. Please select an alternate shaper space or";
                    os << " omit this option.";
                    throw Exception(os.str().c_str());
                }

                // Calculate min/max value
                {
                    // Get input value of 1.0 in shaper space, as this
                    // is the higest value that is transformed by the
                    // cube (e.g for a generic lin-to-log transform,
                    // what the log value 1.0 is in linear).
                    ConstCPUProcessorRcPtr shaperToInputProc = config->getProcessor(
                        shaperSpace.c_str(),
                        inputSpace.c_str())->getDefaultCPUProcessor();

                    float minval[3] = {0.0f, 0.0f, 0.0f};
                    float maxval[3] = {1.0f, 1.0f, 1.0f};

                    shaperToInputProc->applyRGB(minval);
                    shaperToInputProc->applyRGB(maxval);
                    
                    // Grab green channel, as this is the one used later
                    fromInStart = minval[1];
                    fromInEnd = maxval[1];
                }

                // Generate the identity shaper values, then apply the transform.
                // Shaper is linearly sampled from fromInStart to fromInEnd
                shaperData.resize(shaperSize*3);

                for (int i = 0; i < shaperSize; ++i)
                {
                    const float x = (float)(double(i) / double(shaperSize - 1));
                    float cur_value = lerpf(fromInStart, fromInEnd, x);

                    shaperData[3*i+0] = cur_value;
                    shaperData[3*i+1] = cur_value;
                    shaperData[3*i+2] = cur_value;
                }

                PackedImageDesc shaperImg(&shaperData[0], shaperSize, 1, 3);
                ConstCPUProcessorRcPtr cpu = inputToShaperProc->getDefaultCPUProcessor();
                cpu->apply(shaperImg);
            }

            //
            // Generate 3DLUT
            //

            std::vector<float> cubeData;
            if(required_lut == CUBE_3D || required_lut == CUBE_1D_3D)
            {
                cubeData.resize(cubeSize*cubeSize*cubeSize*3);
                GenerateIdentityLut3D(&cubeData[0], cubeSize, 3, LUT3DORDER_FAST_RED);
                PackedImageDesc cubeImg(&cubeData[0], cubeSize*cubeSize*cubeSize, 1, 3);

                ConstProcessorRcPtr cubeProc;
                if(required_lut == CUBE_1D_3D)
                {
                    // Shaper goes from input-to-shaper, so cube goes from shaper-to-target
                    if (!looks.empty())
                    {
                        LookTransformRcPtr transform = LookTransform::Create();
                        transform->setLooks(looks.c_str());
                        transform->setSrc(shaperSpace.c_str());
                        transform->setDst(targetSpace.c_str());
                        cubeProc = config->getProcessor(transform,
                            TRANSFORM_DIR_FORWARD);
                    }
                    else
                    {
                        cubeProc = config->getProcessor(shaperSpace.c_str(),
                                                        targetSpace.c_str());
                    }
                }
                else
                {
                    // No shaper, so cube goes from input-to-target
                    cubeProc = inputToTargetProc;
                }

                ConstCPUProcessorRcPtr cpu = cubeProc->getDefaultCPUProcessor();
                cpu->apply(cubeImg);
            }

            //
            // Generate 1DLUT
            //

            std::vector<float> onedData;
            if(required_lut == CUBE_1D)
            {
                onedData.resize(onedSize * 3);
                GenerateIdentityLut1D(&onedData[0], onedSize, 3);
                PackedImageDesc onedImg(&onedData[0], onedSize, 1, 3);
                
                ConstCPUProcessorRcPtr cpu = inputToTargetProc->getDefaultCPUProcessor();
                cpu->apply(onedImg);
            }

            //
            // Write LUT
            //

            // Set to a fixed 6 decimal precision
            ostream.setf(std::ios::fixed, std::ios::floatfield);
            ostream.precision(6);

            // Comments
            const auto & metadata = baker.getFormatMetadata();
            const auto nb = metadata.getNumChildrenElements();
            for (int i = 0; i < nb; ++i)
            {
                const auto & child = metadata.getChildElement(i);
                ostream << "# " << child.getValue() << "\n";
            }
            if (nb > 0)
            {
                ostream << "\n";
            }

            // Header
            // Note about LUT_ND_INPUT_RANGE tags :
            // These tags are optional and will default to the 0..1 range,
            // not wrting them explicitly allow for wider compatibility
            // with parser based on other cube specification (eg. Iridas_Itx)
            if(required_lut == CUBE_1D)
            {
                ostream << "LUT_1D_SIZE " << onedSize << "\n";
                //ostream << "LUT_1D_INPUT_RANGE 0.0 1.0\n";
            }
            else if(required_lut == CUBE_1D_3D)
            {
                ostream << "LUT_1D_SIZE " << shaperSize << "\n";
                ostream << "LUT_1D_INPUT_RANGE " << fromInStart << " " << fromInEnd << "\n";
            }
            if(required_lut == CUBE_3D || required_lut == CUBE_1D_3D)
            {
                ostream << "LUT_3D_SIZE " << cubeSize << "\n";
                //ostream << "LUT_3D_INPUT_RANGE 0.0 1.0\n";
            }

            // Write 1D data
            if(required_lut == CUBE_1D)
            {
                for(int i=0; i<onedSize; ++i)
                {
                    ostream << onedData[3*i+0] << " "
                            << onedData[3*i+1] << " "
                            << onedData[3*i+2] << "\n";
                }
            }
            else if(required_lut == CUBE_1D_3D)
            {
                for(int i=0; i<shaperSize; ++i)
                {
                    ostream << shaperData[3*i+0] << " "
                            << shaperData[3*i+1] << " "
                            << shaperData[3*i+2] << "\n";
                }
            }

            // Write 3D data
            if(required_lut == CUBE_3D || required_lut == CUBE_1D_3D)
            {
                for(int i=0; i<cubeSize*cubeSize*cubeSize; ++i)
                {
                    ostream << cubeData[3*i+0] << " "
                            << cubeData[3*i+1] << " "
                            << cubeData[3*i+2] << "\n";
                }
            }
        }

        void
        LocalFileFormat::buildFileOps(OpRcPtrVec & ops,
                                      const Config & /*config*/,
                                      const ConstContextRcPtr & /*context*/,
                                      CachedFileRcPtr untypedCachedFile,
                                      const FileTransform & fileTransform,
                                      TransformDirection dir) const
        {
            LocalCachedFileRcPtr cachedFile = DynamicPtrCast<LocalCachedFile>(untypedCachedFile);

            // This should never happen.
            if(!cachedFile)
            {
                std::ostringstream os;
                os << "Cannot build Resolve .cube Op. Invalid cache type.";
                throw Exception(os.str().c_str());
            }

            TransformDirection newDir = CombineTransformDirections(dir,
                fileTransform.getDirection());
            if(newDir == TRANSFORM_DIR_UNKNOWN)
            {
                std::ostringstream os;
                os << "Cannot build file format transform,";
                os << " unspecified transform direction.";
                throw Exception(os.str().c_str());
            }

            if (cachedFile->lut3D)
            {
                cachedFile->lut3D->setInterpolation(fileTransform.getInterpolation());
            }
            else if (cachedFile->lut1D)
            {
                cachedFile->lut1D->setInterpolation(fileTransform.getInterpolation());
            }

            if(newDir == TRANSFORM_DIR_FORWARD)
            {
                if(cachedFile->lut1D)
                {
                    CreateMinMaxOp(ops,
                                   cachedFile->range1d_min,
                                   cachedFile->range1d_max,
                                   newDir);
                    CreateLut1DOp(ops, cachedFile->lut1D, newDir);
                }
                if(cachedFile->lut3D)
                {
                    CreateMinMaxOp(ops,
                                   cachedFile->range3d_min,
                                   cachedFile->range3d_max,
                                   newDir);
                    CreateLut3DOp(ops, cachedFile->lut3D, newDir);
                }
            }
            else if(newDir == TRANSFORM_DIR_INVERSE)
            {
                if(cachedFile->lut3D)
                {
                    CreateLut3DOp(ops, cachedFile->lut3D, newDir);
                    CreateMinMaxOp(ops,
                                   cachedFile->range3d_min,
                                   cachedFile->range3d_max,
                                   newDir);
                }
                if(cachedFile->lut1D)
                {
                    CreateLut1DOp(ops, cachedFile->lut1D, newDir);
                    CreateMinMaxOp(ops,
                                   cachedFile->range1d_min,
                                   cachedFile->range1d_max,
                                   newDir);
                }
            }
        }
    }

    FileFormat * CreateFileFormatResolveCube()
    {
        return new LocalFileFormat();
    }
}
OCIO_NAMESPACE_EXIT

///////////////////////////////////////////////////////////////////////////////

#ifdef OCIO_UNIT_TEST

#include "UnitTest.h"
#include "UnitTestUtils.h"
namespace OCIO = OCIO_NAMESPACE;

namespace
{
OCIO::LocalCachedFileRcPtr ReadResolveCube(const std::string & fileContent)
{
    std::istringstream is;
    is.str(fileContent);

    // Read file
    OCIO::LocalFileFormat tester;
    const std::string SAMPLE_NAME("Memory File");
    OCIO::CachedFileRcPtr cachedFile = tester.read(is, SAMPLE_NAME);

    return OCIO::DynamicPtrCast<OCIO::LocalCachedFile>(cachedFile);
}
}

OCIO_ADD_TEST(FileFormatResolveCube, format_info)
{
    OCIO::FormatInfoVec formatInfoVec;
    OCIO::LocalFileFormat tester;
    tester.getFormatInfo(formatInfoVec);

    OCIO_CHECK_EQUAL(1, formatInfoVec.size());
    OCIO_CHECK_EQUAL("resolve_cube", formatInfoVec[0].name);
    OCIO_CHECK_EQUAL("cube", formatInfoVec[0].extension);
    OCIO_CHECK_EQUAL(OCIO::FORMAT_CAPABILITY_READ | OCIO::FORMAT_CAPABILITY_BAKE,
                     formatInfoVec[0].capabilities);
}

OCIO_ADD_TEST(FileFormatResolveCube, read_1d)
{
    const std::string SAMPLE =
        "LUT_1D_SIZE 2\n"
        "LUT_1D_INPUT_RANGE 0.0 1.0\n"

        "0.0 0.0 0.0\n"
        "1.0 0.0 0.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE));
}

OCIO_ADD_TEST(FileFormatResolveCube, read_3d)
{
    const std::string SAMPLE =
        "LUT_3D_SIZE 2\n"
        "LUT_3D_INPUT_RANGE 0.0 1.0\n"

        "0.0 0.0 0.0\n"
        "1.0 0.0 0.0\n"
        "0.0 1.0 0.0\n"
        "1.0 1.0 0.0\n"
        "0.0 0.0 1.0\n"
        "1.0 0.0 1.0\n"
        "0.0 1.0 1.0\n"
        "1.0 1.0 1.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE));
}

OCIO_ADD_TEST(FileFormatResolveCube, read_1d_3d)
{
    const std::string SAMPLE =
        "LUT_1D_SIZE 6\n"
        "LUT_1D_INPUT_RANGE 0.0 1.0\n"
        "LUT_3D_SIZE 3\n"
        "LUT_3D_INPUT_RANGE 0.0 1.0\n"
        
        "1.0 1.0 1.0\n"
        "0.8 0.8 0.8\n"
        "0.6 0.6 0.6\n"
        "0.4 0.4 0.4\n"
        "0.2 0.2 0.2\n"
        "0.0 0.0 0.0\n"
        "1.0 1.0 1.0\n"
        "0.5 1.0 1.0\n"
        "0.0 1.0 1.0\n"
        "1.0 0.5 1.0\n"
        "0.5 0.5 1.0\n"
        "0.0 0.5 1.0\n"
        "1.0 0.0 1.0\n"
        "0.5 0.0 1.0\n"
        "0.0 0.0 1.0\n"
        "1.0 1.0 0.5\n"
        "0.5 1.0 0.5\n"
        "0.0 1.0 0.5\n"
        "1.0 0.5 0.5\n"
        "0.5 0.5 0.5\n"
        "0.0 0.5 0.5\n"
        "1.0 0.0 0.5\n"
        "0.5 0.0 0.5\n"
        "0.0 0.0 0.5\n"
        "1.0 1.0 0.0\n"
        "0.5 1.0 0.0\n"
        "0.0 1.0 0.0\n"
        "1.0 0.5 0.0\n"
        "0.5 0.5 0.0\n"
        "0.0 0.5 0.0\n"
        "1.0 0.0 0.0\n"
        "0.5 0.0 0.0\n"
        "0.0 0.0 0.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE));
}

OCIO_ADD_TEST(FileFormatResolveCube, read_default_range)
{
    const std::string SAMPLE_1D =
        "LUT_1D_SIZE 2\n"

        "0.0 0.0 0.0\n"
        "1.0 0.0 0.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE_1D));
    
    const std::string SAMPLE_3D =
        "LUT_3D_SIZE 2\n"

        "0.0 0.0 0.0\n"
        "1.0 0.0 0.0\n"
        "0.0 1.0 0.0\n"
        "1.0 1.0 0.0\n"
        "0.0 0.0 1.0\n"
        "1.0 0.0 1.0\n"
        "0.0 1.0 1.0\n"
        "1.0 1.0 1.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE_3D));
    
    const std::string SAMPLE_1D3D =
        "LUT_1D_SIZE 2\n"
        "LUT_3D_SIZE 2\n"
        
        "0.0 0.0 0.0\n"
        "1.0 1.0 1.0\n"
        "0.0 0.0 0.0\n"
        "1.0 0.0 0.0\n"
        "0.0 1.0 0.0\n"
        "1.0 1.0 0.0\n"
        "0.0 0.0 1.0\n"
        "1.0 0.0 1.0\n"
        "0.0 1.0 1.0\n"
        "1.0 1.0 1.0\n";

    OCIO_CHECK_NO_THROW(ReadResolveCube(SAMPLE_1D3D));
}


OCIO_ADD_TEST(FileFormatResolveCube, read_failure)
{
    {
        // Wrong LUT_3D_SIZE tag
        const std::string SAMPLE_ERROR =
            "LUT_3D_SIZE 2 2\n"
            "LUT_3D_INPUT_RANGE 0.0 1.0\n"

            "0.0 0.0 0.0\n"
            "1.0 0.0 0.0\n"
            "0.0 1.0 0.0\n"
            "1.0 1.0 0.0\n"
            "0.0 0.0 1.0\n"
            "1.0 0.0 1.0\n"
            "0.0 1.0 1.0\n"
            "1.0 1.0 1.0\n";

        OCIO_CHECK_THROW(ReadResolveCube(SAMPLE_ERROR), OCIO::Exception);
    }
    {
        // Wrong LUT_3D_INPUT_RANGE tag
        const std::string SAMPLE_ERROR =
            "LUT_3D_SIZE 2\n"
            "LUT_3D_INPUT_RANGE 0.0 1.0 2.0\n"

            "0.0 0.0 0.0\n"
            "1.0 0.0 0.0\n"
            "0.0 1.0 0.0\n"
            "1.0 1.0 0.0\n"
            "0.0 0.0 1.0\n"
            "1.0 0.0 1.0\n"
            "0.0 1.0 1.0\n"
            "1.0 1.0 1.0\n";

        OCIO_CHECK_THROW(ReadResolveCube(SAMPLE_ERROR), OCIO::Exception);
    }
    {
        // Comment after header
        const std::string SAMPLE_ERROR =
            "LUT_3D_SIZE 2\n"
            "LUT_3D_INPUT_RANGE 0.0 1.0 2.0\n"
            "# Malformed comment\n"
            "0.0 0.0 0.0\n"
            "1.0 0.0 0.0\n"
            "0.0 1.0 0.0\n"
            "1.0 1.0 0.0\n"
            "0.0 0.0 1.0\n"
            "1.0 0.0 1.0\n"
            "0.0 1.0 1.0\n"
            "1.0 1.0 1.0\n";

        OCIO_CHECK_THROW(ReadResolveCube(SAMPLE_ERROR), OCIO::Exception);
    }
    {
        // Unexpected tag
        const std::string SAMPLE_ERROR =
            "LUT_3D_SIZE 2\n"
            "LUT_3D_INPUT_RANGE 0.0 1.0 2.0\n"
            "WRONG_TAG\n"
            "0.0 0.0 0.0\n"
            "1.0 0.0 0.0\n"
            "0.0 1.0 0.0\n"
            "1.0 1.0 0.0\n"
            "0.0 0.0 1.0\n"
            "1.0 0.0 1.0\n"
            "0.0 1.0 1.0\n"
            "1.0 1.0 1.0\n";

        OCIO_CHECK_THROW(ReadResolveCube(SAMPLE_ERROR), OCIO::Exception);
    }
    {
        // Wrong number of entries
        const std::string SAMPLE_ERROR =
            "LUT_3D_SIZE 2\n"
            "LUT_3D_INPUT_RANGE 0.0 1.0 2.0\n"

            "0.0 0.0 0.0\n"
            "1.0 0.0 0.0\n"
            "0.0 1.0 0.0\n"
            "1.0 1.0 0.0\n"
            "0.0 0.0 1.0\n"
            "1.0 0.0 1.0\n"
            "0.0 1.0 1.0\n"
            "0.0 1.0 1.0\n"
            "0.0 1.0 1.0\n"
            "1.0 1.0 1.0\n";

        OCIO_CHECK_THROW(ReadResolveCube(SAMPLE_ERROR), OCIO::Exception);
    }
}

OCIO_ADD_TEST(FileFormatResolveCube, bake_1d)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create();
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("input");
        cs->setFamily("input");
        config->addColorSpace(cs);
        config->setRole(OCIO::ROLE_REFERENCE, cs->getName());
    }
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("target");
        cs->setFamily("target");
        config->addColorSpace(cs);
    }
    
    std::ostringstream bout;
    bout << "LUT_1D_SIZE 2"                                         << "\n";
    bout << "0.000000 0.000000 0.000000"                            << "\n";
    bout << "1.000000 1.000000 1.000000"                            << "\n";
    
    OCIO::BakerRcPtr baker = OCIO::Baker::Create();
    baker->setConfig(config);
    baker->setFormat("resolve_cube");
    baker->setInputSpace("input");
    baker->setTargetSpace("target");
    baker->setCubeSize(2);
    std::ostringstream output;
    baker->bake(output);
    
    //
    std::vector<std::string> osvec;
    pystring::splitlines(output.str(), osvec);
    std::vector<std::string> resvec;
    pystring::splitlines(bout.str(), resvec);
    OCIO_CHECK_EQUAL(osvec.size(), resvec.size());
    for(unsigned int i = 0; i < resvec.size(); ++i)
    {
        OCIO_CHECK_EQUAL(osvec[i], resvec[i]);
    }
}

OCIO_ADD_TEST(FileFormatResolveCube, bake_3d)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create();
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("input");
        cs->setFamily("input");
        config->addColorSpace(cs);
        config->setRole(OCIO::ROLE_REFERENCE, cs->getName());
    }
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("target");
        cs->setFamily("target");
        
        // Set saturation to cause channel crosstalk, making a 3D LUT
        OCIO::CDLTransformRcPtr transform1 = OCIO::CDLTransform::Create();
        transform1->setSat(0.5f);
        cs->setTransform(transform1, OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        
        config->addColorSpace(cs);
    }
    
    std::ostringstream bout;
    bout << "# OpenColorIO Test Line 1"                             << "\n";
    bout << "# OpenColorIO Test Line 2"                             << "\n";
    bout << ""                                                      << "\n";
    bout << "LUT_3D_SIZE 2"                                         << "\n";
    bout << "0.000000 0.000000 0.000000"                            << "\n";
    bout << "0.606300 0.106300 0.106300"                            << "\n";
    bout << "0.357600 0.857600 0.357600"                            << "\n";
    bout << "0.963900 0.963900 0.463900"                            << "\n";
    bout << "0.036100 0.036100 0.536100"                            << "\n";
    bout << "0.642400 0.142400 0.642400"                            << "\n";
    bout << "0.393700 0.893700 0.893700"                            << "\n";
    bout << "1.000000 1.000000 1.000000"                            << "\n";
    
    OCIO::BakerRcPtr baker = OCIO::Baker::Create();
    baker->setConfig(config);
    baker->getFormatMetadata().addChildElement(OCIO::METADATA_DESCRIPTION,
                                               "OpenColorIO Test Line 1");
    baker->getFormatMetadata().addChildElement(OCIO::METADATA_DESCRIPTION,
                                               "OpenColorIO Test Line 2");
    baker->setFormat("resolve_cube");
    baker->setInputSpace("input");
    baker->setTargetSpace("target");
    baker->setCubeSize(2);
    std::ostringstream output;
    baker->bake(output);
    
    //
    std::vector<std::string> osvec;
    pystring::splitlines(output.str(), osvec);
    std::vector<std::string> resvec;
    pystring::splitlines(bout.str(), resvec);
    OCIO_CHECK_EQUAL(osvec.size(), resvec.size());
    for(unsigned int i = 0; i < resvec.size(); ++i)
    {
        OCIO_CHECK_EQUAL(osvec[i], resvec[i]);
    }
}

OCIO_ADD_TEST(FileFormatResolveCube, bake_1d_3d)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create();
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("input");
        cs->setFamily("input");
        config->addColorSpace(cs);
        config->setRole(OCIO::ROLE_REFERENCE, cs->getName());
    }
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("shaper");
        cs->setFamily("shaper");
        OCIO::ExponentTransformRcPtr transform1 = OCIO::ExponentTransform::Create();
        double test[4] = {2.2, 2.2, 2.2, 1.0};
        transform1->setValue(test);
        cs->setTransform(transform1, OCIO::COLORSPACE_DIR_TO_REFERENCE);
        config->addColorSpace(cs);
    }
    {
        OCIO::ColorSpaceRcPtr cs = OCIO::ColorSpace::Create();
        cs->setName("target");
        cs->setFamily("target");
        
        // Set saturation to cause channel crosstalk, making a 3D LUT
        OCIO::CDLTransformRcPtr transform1 = OCIO::CDLTransform::Create();
        transform1->setSat(0.5f);
        cs->setTransform(transform1, OCIO::COLORSPACE_DIR_FROM_REFERENCE);
        
        config->addColorSpace(cs);
    }
    
    std::ostringstream bout;
    bout << "LUT_1D_SIZE 10"                                        << "\n";
    bout << "LUT_1D_INPUT_RANGE 0.000000 1.000000"                  << "\n";
    bout << "LUT_3D_SIZE 2"                                         << "\n";
    bout << "0.000000 0.000000 0.000000"                            << "\n";
    bout << "0.368344 0.368344 0.368344"                            << "\n";
    bout << "0.504760 0.504760 0.504760"                            << "\n";
    bout << "0.606913 0.606913 0.606913"                            << "\n";
    bout << "0.691699 0.691699 0.691699"                            << "\n";
    bout << "0.765539 0.765539 0.765539"                            << "\n";
    bout << "0.831684 0.831684 0.831684"                            << "\n";
    bout << "0.892049 0.892049 0.892049"                            << "\n";
    bout << "0.947870 0.947870 0.947870"                            << "\n";
    bout << "1.000000 1.000000 1.000000"                            << "\n";
    bout << "0.000000 0.000000 0.000000"                            << "\n";
    bout << "0.606300 0.106300 0.106300"                            << "\n";
    bout << "0.357600 0.857600 0.357600"                            << "\n";
    bout << "0.963900 0.963900 0.463900"                            << "\n";
    bout << "0.036100 0.036100 0.536100"                            << "\n";
    bout << "0.642400 0.142400 0.642400"                            << "\n";
    bout << "0.393700 0.893700 0.893700"                            << "\n";
    bout << "1.000000 1.000000 1.000000"                            << "\n";
    
    OCIO::BakerRcPtr baker = OCIO::Baker::Create();
    baker->setConfig(config);
    baker->setFormat("resolve_cube");
    baker->setInputSpace("input");
    baker->setShaperSpace("shaper");
    baker->setTargetSpace("target");
    baker->setShaperSize(10);
    baker->setCubeSize(2);
    std::ostringstream output;
    baker->bake(output);
    
    //
    std::vector<std::string> osvec;
    pystring::splitlines(output.str(), osvec);
    std::vector<std::string> resvec;
    pystring::splitlines(bout.str(), resvec);
    OCIO_CHECK_EQUAL(osvec.size(), resvec.size());
    for(unsigned int i = 0; i < resvec.size(); ++i)
    {
        OCIO_CHECK_EQUAL(osvec[i], resvec[i]);
    }
}

OCIO_ADD_TEST(FileFormatResolveCube, load_ops)
{
    const std::string fileName("resolve_1d3d.cube");
    OCIO::OpRcPtrVec ops;
    OCIO::ContextRcPtr context = OCIO::Context::Create();
    OCIO_CHECK_NO_THROW(BuildOpsTest(ops, fileName, context,
        OCIO::TRANSFORM_DIR_FORWARD));

    OCIO_REQUIRE_EQUAL(ops.size(), 5);
    OCIO_CHECK_EQUAL("<FileNoOp>", ops[0]->getInfo());
    OCIO_CHECK_EQUAL("<MatrixOffsetOp>", ops[1]->getInfo());
    OCIO_CHECK_EQUAL("<Lut1DOp>", ops[2]->getInfo());
    OCIO_CHECK_EQUAL("<MatrixOffsetOp>", ops[3]->getInfo());
    OCIO_CHECK_EQUAL("<Lut3DOp>", ops[4]->getInfo());

    auto op1 = std::const_pointer_cast<const OCIO::Op>(ops[1]);
    auto opData1 = op1->data();
    auto mat = std::dynamic_pointer_cast<const OCIO::MatrixOpData>(opData1);
    OCIO_REQUIRE_ASSERT(mat);
    auto & matArray = mat->getArray();
    OCIO_CHECK_EQUAL(matArray[0], 0.25f);
    OCIO_CHECK_EQUAL(matArray[1], 0.0f);
    OCIO_CHECK_EQUAL(matArray[2], 0.0f);
    OCIO_CHECK_EQUAL(matArray[3], 0.0f);
    OCIO_CHECK_EQUAL(matArray[4], 0.0f);
    OCIO_CHECK_EQUAL(matArray[5], 0.25f);
    OCIO_CHECK_EQUAL(matArray[6], 0.0f);
    OCIO_CHECK_EQUAL(matArray[7], 0.0f);
    OCIO_CHECK_EQUAL(matArray[8], 0.0f);
    OCIO_CHECK_EQUAL(matArray[9], 0.0f);
    OCIO_CHECK_EQUAL(matArray[10], 0.25f);
    OCIO_CHECK_EQUAL(matArray[11], 0.0f);
    OCIO_CHECK_EQUAL(matArray[12], 0.0f);
    OCIO_CHECK_EQUAL(matArray[13], 0.0f);
    OCIO_CHECK_EQUAL(matArray[14], 0.0f);
    OCIO_CHECK_EQUAL(matArray[15], 1.0f);

    auto & matOffsets = mat->getOffsets();
    OCIO_CHECK_EQUAL(matOffsets[0], 0.25f);
    OCIO_CHECK_EQUAL(matOffsets[1], 0.25f);
    OCIO_CHECK_EQUAL(matOffsets[2], 0.25f);
    OCIO_CHECK_EQUAL(matOffsets[3], 0.0f);

    auto op2 = std::const_pointer_cast<const OCIO::Op>(ops[2]);
    auto opData2 = op2->data();
    auto lut = std::dynamic_pointer_cast<const OCIO::Lut1DOpData>(opData2);
    OCIO_REQUIRE_ASSERT(lut);
    OCIO_CHECK_EQUAL(lut->getFileOutputBitDepth(), OCIO::BIT_DEPTH_F32);

    auto & lutArray = lut->getArray();
    OCIO_REQUIRE_EQUAL(lutArray.getNumValues(), 18);

    OCIO_CHECK_EQUAL(lutArray[0], 3.3f);
    OCIO_CHECK_EQUAL(lutArray[1], 3.4f);
    OCIO_CHECK_EQUAL(lutArray[2], 3.5f);
    OCIO_CHECK_EQUAL(lutArray[3], 3.0f);
    OCIO_CHECK_EQUAL(lutArray[4], 3.1f);
    OCIO_CHECK_EQUAL(lutArray[5], 3.2f);
    OCIO_CHECK_EQUAL(lutArray[6], 2.2f);
    OCIO_CHECK_EQUAL(lutArray[7], 2.3f);
    OCIO_CHECK_EQUAL(lutArray[8], 2.4f);
    OCIO_CHECK_EQUAL(lutArray[9], 2.1f);
    OCIO_CHECK_EQUAL(lutArray[10], 2.0f);
    OCIO_CHECK_EQUAL(lutArray[11], 2.0f);
    OCIO_CHECK_EQUAL(lutArray[12], 1.0f);
    OCIO_CHECK_EQUAL(lutArray[13], 1.0f);
    OCIO_CHECK_EQUAL(lutArray[14], 1.0f);
    OCIO_CHECK_EQUAL(lutArray[15], 0.0f);
    OCIO_CHECK_EQUAL(lutArray[16], 0.0f);
    OCIO_CHECK_EQUAL(lutArray[17], 0.0f);

    auto op3 = std::const_pointer_cast<const OCIO::Op>(ops[3]);
    auto opData3 = op3->data();
    auto mat3 = std::dynamic_pointer_cast<const OCIO::MatrixOpData>(opData3);
    OCIO_REQUIRE_ASSERT(mat3);
    auto & mat3Array = mat3->getArray();
    OCIO_CHECK_EQUAL(mat3Array[0], 0.25f);
    OCIO_CHECK_EQUAL(mat3Array[1], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[2], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[3], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[4], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[5], 0.25f);
    OCIO_CHECK_EQUAL(mat3Array[6], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[7], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[8], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[9], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[10], 0.25f);
    OCIO_CHECK_EQUAL(mat3Array[11], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[12], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[13], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[14], 0.0f);
    OCIO_CHECK_EQUAL(mat3Array[15], 1.0f);

    auto & mat3Offsets = mat->getOffsets();
    OCIO_CHECK_EQUAL(mat3Offsets[0], 0.25f);
    OCIO_CHECK_EQUAL(mat3Offsets[1], 0.25f);
    OCIO_CHECK_EQUAL(mat3Offsets[2], 0.25f);
    OCIO_CHECK_EQUAL(mat3Offsets[3], 0.0f);

    auto op4 = std::const_pointer_cast<const OCIO::Op>(ops[4]);
    auto opData4 = op4->data();
    auto lut4 = std::dynamic_pointer_cast<const OCIO::Lut3DOpData>(opData4);
    OCIO_REQUIRE_ASSERT(lut4);
    OCIO_CHECK_EQUAL(lut4->getFileOutputBitDepth(), OCIO::BIT_DEPTH_F32);

    auto & lut4Array = lut4->getArray();
    OCIO_REQUIRE_EQUAL(lut4Array.getNumValues(), 81);

    // File line 11 - R:0 - G:0 - B:0
    OCIO_CHECK_EQUAL(lut4Array[0], 1.1f);
    OCIO_CHECK_EQUAL(lut4Array[1], 1.1f);
    OCIO_CHECK_EQUAL(lut4Array[2], 1.1f);

    // File line 23 - R:0 - G:1 - B:1
    OCIO_CHECK_EQUAL(lut4Array[12], 1.0f);
    OCIO_CHECK_EQUAL(lut4Array[13], 0.5f);
    OCIO_CHECK_EQUAL(lut4Array[14], 0.5f);

    // File line 31 - R:2 - G:0 - B:2
    OCIO_CHECK_EQUAL(lut4Array[60], 0.0f);
    OCIO_CHECK_EQUAL(lut4Array[61], 1.0f);
    OCIO_CHECK_EQUAL(lut4Array[62], 0.0f);

}

#endif // OCIO_UNIT_TEST
