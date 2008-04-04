/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/**
	Information taken from Microsoft' DX SDK 9 (June 2006).
*/

#ifndef _DDS_FORMAT_H_
#define _DDS_FORMAT_H_

struct DDCOLORKEY
{
    uint32       dwColorSpaceLowValue;   // low boundary of color space that is to
                                        // be treated as Color Key, inclusive
    uint32       dwColorSpaceHighValue;  // high boundary of color space that is
                                        // to be treated as Color Key, inclusive
};

/*
 * DDPIXELFORMAT
 */
struct DDPIXELFORMAT
{
    uint32       dwSize;                 // size of structure
    uint32       dwFlags;                // pixel format flags
    uint32       dwFourCC;               // (FOURCC code)
    union
    {
        uint32   dwRGBBitCount;          // how many bits per pixel
        uint32   dwYUVBitCount;          // how many bits per pixel
        uint32   dwZBufferBitDepth;      // how many total bits/pixel in z buffer (including any stencil bits)
        uint32   dwAlphaBitDepth;        // how many bits for alpha channels
        uint32   dwLuminanceBitCount;    // how many bits per pixel
        uint32   dwBumpBitCount;         // how many bits per "buxel", total
        uint32   dwPrivateFormatBitCount;// Bits per pixel of private driver formats. Only valid in texture
                                        // format list and if DDPF_D3DFORMAT is set
    };
    union
    {
        uint32   dwRBitMask;             // mask for red bit
        uint32   dwYBitMask;             // mask for Y bits
        uint32   dwStencilBitDepth;      // how many stencil bits (note: dwZBufferBitDepth-dwStencilBitDepth is total Z-only bits)
        uint32   dwLuminanceBitMask;     // mask for luminance bits
        uint32   dwBumpDuBitMask;        // mask for bump map U delta bits
        uint32   dwOperations;           // DDPF_D3DFORMAT Operations
    };
    union
    {
        uint32   dwGBitMask;             // mask for green bits
        uint32   dwUBitMask;             // mask for U bits
        uint32   dwZBitMask;             // mask for Z bits
        uint32   dwBumpDvBitMask;        // mask for bump map V delta bits
        struct
        {
            uint16    wFlipMSTypes;       // Multisample methods supported via flip for this D3DFORMAT
            uint16    wBltMSTypes;        // Multisample methods supported via blt for this D3DFORMAT
        } MultiSampleCaps;

    };
    union
    {
        uint32   dwBBitMask;             // mask for blue bits
        uint32   dwVBitMask;             // mask for V bits
        uint32   dwStencilBitMask;       // mask for stencil bits
        uint32   dwBumpLuminanceBitMask; // mask for luminance in bump map
    };
    union
    {
        uint32   dwRGBAlphaBitMask;      // mask for alpha channel
        uint32   dwYUVAlphaBitMask;      // mask for alpha channel
        uint32   dwLuminanceAlphaBitMask;// mask for alpha channel
        uint32   dwRGBZBitMask;          // mask for Z channel
        uint32   dwYUVZBitMask;          // mask for Z channel
    };
};

struct DDSCAPS2
{
    uint32       dwCaps;         // capabilities of surface wanted
    uint32       dwCaps2;
    uint32       dwCaps3;
    union
    {
        uint32       dwCaps4;
        uint32       dwVolumeDepth;
    };
};

struct DDSURFACEDESC2
{
    uint32               dwSize;                 // size of the DDSURFACEDESC structure
    uint32               dwFlags;                // determines what fields are valid
    uint32               dwHeight;               // height of surface to be created
    uint32               dwWidth;                // width of input surface
    union
    {
        int32            lPitch;                 // distance to start of next line (return value only)
        uint32           dwLinearSize;           // Formless late-allocated optimized surface size
    };
    union
    {
        uint32           dwBackBufferCount;      // number of back buffers requested
        uint32           dwDepth;                // the depth if this is a volume texture 
    };
    union
    {
        uint32           dwMipMapCount;          // number of mip-map levels requested
                                                // dwZBufferBitDepth removed, use ddpfPixelFormat one instead
        uint32           dwRefreshRate;          // refresh rate (used when display mode is described)
        uint32           dwSrcVBHandle;          // The source used in VB::Optimize
    };
    uint32               dwAlphaBitDepth;        // depth of alpha buffer requested
    uint32               dwReserved;             // reserved
    void*                lpSurface;              // pointer to the associated surface memory
    union
    {
        DDCOLORKEY      ddckCKDestOverlay;      // color key for destination overlay use
        uint32          dwEmptyFaceColor;       // Physical color for empty cube-map faces
    };
    DDCOLORKEY          ddckCKDestBlt;          // color key for destination blt use
    DDCOLORKEY          ddckCKSrcOverlay;       // color key for source overlay use
    DDCOLORKEY          ddckCKSrcBlt;           // color key for source blt use
    union
    {
        DDPIXELFORMAT   ddpfPixelFormat;        // pixel format description of the surface
        uint32          dwFVF;                  // vertex format description of vertex buffers
    };
    DDSCAPS2            ddsCaps;                // direct draw surface capabilities
    uint32              dwTextureStage;         // stage in multi-texture cascade
};

/*
 * FOURCC codes for DX compressed-texture pixel formats
 */
#ifdef WIN32
#define FOURCC_DXT1  (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2  (MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3  (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4  (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5  (MAKEFOURCC('D','X','T','5'))
#else
#define FOURCC_DXT1  ('DXT1')
#define FOURCC_DXT2  ('DXT2')
#define FOURCC_DXT3  ('DXT3')
#define FOURCC_DXT4  ('DXT4')
#define FOURCC_DXT5  ('DXT5')
#endif // WIN32

/*
 * DDSURFACEDESC2 flags.
 * Field validity.
 */
#define DDSD_CAPS               0x00000001l
#define DDSD_HEIGHT             0x00000002l
#define DDSD_WIDTH              0x00000004l
#define DDSD_PITCH              0x00000008l

#define DDSD_BACKBUFFERCOUNT    0x00000020l
#define DDSD_ZBUFFERBITDEPTH    0x00000040l
#define DDSD_ALPHABITDEPTH      0x00000080l

#define DDSD_LPSURFACE          0x00000800l

#define DDSD_PIXELFORMAT        0x00001000l
#define DDSD_CKDESTOVERLAY      0x00002000l
#define DDSD_CKDESTBLT          0x00004000l
#define DDSD_CKSRCOVERLAY       0x00008000l

#define DDSD_CKSRCBLT           0x00010000l
#define DDSD_MIPMAPCOUNT        0x00020000l
#define DDSD_REFRESHRATE        0x00040000l
#define DDSD_LINEARSIZE         0x00080000l

#define DDSD_TEXTURESTAGE       0x00100000l
#define DDSD_FVF                0x00200000l
#define DDSD_SRCVBHANDLE        0x00400000l
#define DDSD_DEPTH              0x00800000l

#define DDSD_ALL                0x00fff9eel

/*
 * The surface has alpha channel information in the pixel format.
 */
#define DDPF_ALPHAPIXELS                        0x00000001l

/*
 * The pixel format contains alpha only information
 */
#define DDPF_ALPHA                              0x00000002l

/*
 * The FourCC code is valid.
 */
#define DDPF_FOURCC                             0x00000004l

/*
 * The surface is 4-bit color indexed.
 */
#define DDPF_PALETTEINDEXED4                    0x00000008l

/*
 * The surface is indexed into a palette which stores indices
 * into the destination surface's 8-bit palette.
 */
#define DDPF_PALETTEINDEXEDTO8                  0x00000010l

/*
 * The surface is 8-bit color indexed.
 */
#define DDPF_PALETTEINDEXED8                    0x00000020l

/*
 * The RGB data in the pixel format structure is valid.
 */
#define DDPF_RGB                                0x00000040l

/*
 * The surface will accept pixel data in the format specified
 * and compress it during the write.
 */
#define DDPF_COMPRESSED                         0x00000080l

/*
 * The surface will accept RGB data and translate it during
 * the write to YUV data.  The format of the data to be written
 * will be contained in the pixel format structure.  The DDPF_RGB
 * flag will be set.
 */
#define DDPF_RGBTOYUV                           0x00000100l

/*
 * pixel format is YUV - YUV data in pixel format struct is valid
 */
#define DDPF_YUV                                0x00000200l

/*
 * pixel format is a z buffer only surface
 */
#define DDPF_ZBUFFER                            0x00000400l

/*
 * The surface is 1-bit color indexed.
 */
#define DDPF_PALETTEINDEXED1                    0x00000800l

/*
 * The surface is 2-bit color indexed.
 */
#define DDPF_PALETTEINDEXED2                    0x00001000l

/*
 * The surface contains Z information in the pixels
 */
#define DDPF_ZPIXELS                            0x00002000l

/*
 * The surface contains stencil information along with Z
 */
#define DDPF_STENCILBUFFER                      0x00004000l

/*
 * Premultiplied alpha format -- the color components have been
 * premultiplied by the alpha component.
 */
#define DDPF_ALPHAPREMULT                       0x00008000l


/*
 * Luminance data in the pixel format is valid.
 * Use this flag for luminance-only or luminance+alpha surfaces,
 * the bit depth is then ddpf.dwLuminanceBitCount.
 */
#define DDPF_LUMINANCE                          0x00020000l

/*
 * Luminance data in the pixel format is valid.
 * Use this flag when hanging luminance off bumpmap surfaces,
 * the bit mask for the luminance portion of the pixel is then
 * ddpf.dwBumpLuminanceBitMask
 */
#define DDPF_BUMPLUMINANCE                      0x00040000l

/*
 * Bump map dUdV data in the pixel format is valid.
 */
#define DDPF_BUMPDUDV                           0x00080000l


#define DDSCAPS2_CUBEMAP						0x00000200L
#define DDSCAPS2_VOLUME                         0x00200000L

#endif // _DDS_FORMAT_H_

