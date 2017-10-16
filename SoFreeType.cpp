//--------------------------------------------------------------------
#include "SoFreeType.h"
#include "SoFileHelp.h"
#include "SoMessageBox.h"
//--------------------------------------------------------------------
FT_Library SoFreeType::ms_kFTLib = 0;
int SoFreeType::ms_nFTLibRefCount = 0;
//--------------------------------------------------------------------
SoFreeType::SoFreeType()
:m_kFTFace(0)
,m_kFileData(0)
,m_kGlphyBitmapData(0)
,m_nFileSize(0)
,m_nFontSize(0)
,m_nLineHeight(0)
,m_nAscender(0)
,m_nBytePerPixel(0)
{
	if (!ms_nFTLibRefCount++)
	{
		FT_Init_FreeType(&ms_kFTLib);
	}
}
//--------------------------------------------------------------------
SoFreeType::~SoFreeType()
{
	ClearFreeType();
	//
	if (!--ms_nFTLibRefCount)
	{
		FT_Done_FreeType(ms_kFTLib);
	}
}
//--------------------------------------------------------------------
bool SoFreeType::InitFreeType(const char* szFontFile, int nFontSize, int nBytePerPixel)
{
	SoFile* pFile = SoFileHelp::CreateFile(szFontFile, "rb");
	if (pFile == 0)
	{
#ifdef SoMessageBoxEnable
        SoMessageBox("Error", "SoFreeType::InitFreeType : Load file fail");
#endif
		return false;
	}

    m_kFileData = pFile->GetFileData();
    m_nFileSize = pFile->GetFileSize();
    pFile->RemoveFileDataRef();
    SoFileHelp::DeleteFile(pFile);

    bool br = false;
    do
    {
        if (m_kFileData == 0)
        {
            break;
        }
        if (m_nFileSize == 0)
        {
            break;
        }

        //0 means success.
        FT_Error nFTResult = 0;

        const FT_Long nFaceIndex = 0;
        nFTResult = FT_New_Memory_Face(ms_kFTLib, m_kFileData, m_nFileSize, nFaceIndex, &m_kFTFace);
        if (nFTResult)
        {
            break;
        }

        nFTResult = FT_Select_Charmap(m_kFTFace, FT_ENCODING_UNICODE);
        if (nFTResult)
        {
            break;
        }

        nFTResult = FT_Set_Pixel_Sizes(m_kFTFace, (FT_UInt)nFontSize, (FT_UInt)nFontSize);
        if (nFTResult)
        {
            break;
        }

        if (! CreateGlphyBitmapData(nBytePerPixel))
        {
            break;
        }

        br = true;

    } while (0);

    if (br)
    {
        m_nFontSize = nFontSize;
        m_nLineHeight = int((m_kFTFace->size->metrics.height) >> 6);
        m_nAscender = int((m_kFTFace->size->metrics.ascender) >> 6);
    }
    else
    {
        ClearFreeType();
#ifdef SoMessageBoxEnable
        SoMessageBox("Error", "SoFreeType::InitFreeType : Init FreeType fail");
#endif
    }
	return br;
}
//--------------------------------------------------------------------
void SoFreeType::ClearFreeType()
{
	if (m_kFTFace)
	{
		FT_Done_Face(m_kFTFace);
		m_kFTFace = 0;
	}
	if (m_kFileData)
	{
		free(m_kFileData);
		m_kFileData = 0;
	}
	m_nFileSize = 0;
	m_nFontSize = 0;
	m_nLineHeight = 0;
    m_nAscender = 0;
    ReleaseGlphyBitmapData();
}
//--------------------------------------------------------------------
bool SoFreeType::GenerateCharGlyphData(unsigned int Character, SoFreeType_CharGlyphData* pData)
{
	if (pData == 0)
	{
		return false;
	}
    if (m_kFTFace == 0)
    {
        return false;
    }

    //FreeType在渲染字符的时候有多种渲染模式，常用的有两种，
    //一种是FT_RENDER_MODE_NORMAL，每个像素为一个8bit的灰度值。
    //另一种是FT_RENDER_MODE_MONO，每个像素只占1bit。
    //这里使用默认值，即 FT_RENDER_MODE_NORMAL ，BytePerPixel = 1 。
	FT_Int32 nLoadFlags = FT_LOAD_TARGET_NORMAL | FT_LOAD_RENDER | FT_LOAD_NO_AUTOHINT;
	FT_Error nResult = FT_Load_Char(m_kFTFace, Character, nLoadFlags);
	if (nResult)
	{
		return false;
	}

	const FT_GlyphSlot& theGlyph = m_kFTFace->glyph;
    const int BitMapWidth = theGlyph->bitmap.width;
    const int BitMapHeight = theGlyph->bitmap.rows;
    const int BytePitch = BitMapWidth;
    //这个内存空间由FreeType维护，这里不要释放该内存。
    const unsigned char* BitMapData = theGlyph->bitmap.buffer;

    if (BitMapWidth > SoFreeType_GlphyBitmap_MaxWidth)
    {
#ifdef SoMessageBoxEnable
        SoMessageBox("Error", "SoFreeType::InitFreeType : BitMapWidth > SoFreeType_GlphyBitmap_MaxWidth");
#endif
        return false;
    }
    if (BitMapHeight > SoFreeType_GlphyBitmap_MaxWidth)
    {
#ifdef SoMessageBoxEnable
        SoMessageBox("Error", "SoFreeType::InitFreeType : BitMapHeight > SoFreeType_GlphyBitmap_MaxWidth");
#endif
        return false;
    }

    //把每个像素占1个字节的 BitMapData ，转换成每个像素占 m_nBytePerPixel 个字节，并存储在 m_kGlphyBitmapData 中。
    unsigned char* destData = m_kGlphyBitmapData;
    const int BytePerPixel = m_nBytePerPixel;
    const int destDataBytePitch = SoFreeType_GlphyBitmap_MaxWidth * BytePerPixel;

    for (int y = 0; y < BitMapHeight; ++y)
    {
        for (int x = 0; x < BitMapWidth; ++x)
        {
            for (int b = 0; b < BytePerPixel; ++b)
            {
                destData[x*BytePerPixel+b] = BitMapData[x];
            }
        }
        destData += destDataBytePitch;
        BitMapData += BytePitch;
    }

	//
    pData->pGlphyBitmap = m_kGlphyBitmapData;
	pData->Character = Character;
    pData->BytePitch = destDataBytePitch;
	pData->BitMapWidth = BitMapWidth;
	pData->BitMapHeight = BitMapHeight;
    pData->OffsetX = theGlyph->bitmap_left; //这个值也可以这样获得 (int)(theGlyph->metrics.horiBearingX >> 6);
    pData->OffsetY = m_nAscender - theGlyph->bitmap_top;
	pData->AdvanceX = (int)(theGlyph->advance.x >> 6);
    pData->AdvanceY = m_nLineHeight;
	return true;
}
//--------------------------------------------------------------------
bool SoFreeType::CreateGlphyBitmapData(int nBytePerPixel)
{
    m_nBytePerPixel = nBytePerPixel;
    const int nDataSize = SoFreeType_GlphyBitmap_MaxWidth * SoFreeType_GlphyBitmap_MaxWidth * nBytePerPixel;
    m_kGlphyBitmapData = (unsigned char*)SoMalloc(nDataSize);
    return (m_kGlphyBitmapData != 0);
}
//--------------------------------------------------------------------
void SoFreeType::ReleaseGlphyBitmapData()
{
    if (m_kGlphyBitmapData)
    {
        SoFree(m_kGlphyBitmapData);
        m_kGlphyBitmapData = 0;
    }
    m_nBytePerPixel = 0;
}
//--------------------------------------------------------------------
