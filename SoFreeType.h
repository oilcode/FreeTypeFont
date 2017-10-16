//--------------------------------------------------------------------
#ifndef _SoFreeType_h_
#define _SoFreeType_h_
//--------------------------------------------------------------------
#include <ft2build.h>
#include FT_FREETYPE_H
//--------------------------------------------------------------------
#define SoFreeType_GlphyBitmap_MaxWidth 64
//--------------------------------------------------------------------
struct SoFreeType_CharGlyphData
{
	//外界不要释放内存空间。
	unsigned char* pGlphyBitmap;
	//字符。
	unsigned int Character;
    //字形位图的每一行是多少个字节。
    //BytePerPixel = BytePitch / BitMapWidth 。
	int BytePitch;
	int BitMapWidth;
	int BitMapHeight;
	int OffsetX;
	int OffsetY;
	int AdvanceX;
    int AdvanceY;

	SoFreeType_CharGlyphData() : pGlphyBitmap(0), Character(0), BytePitch(0), BitMapWidth(0), BitMapHeight(0)
	                           , OffsetX(0), OffsetY(0), AdvanceX(0), AdvanceY(0)
	{

	}
};
//--------------------------------------------------------------------
class SoFreeType
{
public:
	SoFreeType();
	~SoFreeType();
	bool InitFreeType(const char* szFontFile, int nFontSize, int nBytePerPixel);
	void ClearFreeType();
	bool GenerateCharGlyphData(unsigned int Character, SoFreeType_CharGlyphData* pData);

private:
	bool CreateGlphyBitmapData(int nBytePerPixel);
	void ReleaseGlphyBitmapData();

private:
	//a handle to the FreeType library, only one instance
	static FT_Library ms_kFTLib;
	static int ms_nFTLibRefCount;
private:
	FT_Face m_kFTFace;
	unsigned char* m_kFileData;
	unsigned char* m_kGlphyBitmapData;
	int m_nFileSize;
	int m_nFontSize;
	int m_nLineHeight;
    int m_nAscender;
    int m_nBytePerPixel;
};
//--------------------------------------------------------------------
#endif //_SoFreeType_h_
//--------------------------------------------------------------------
