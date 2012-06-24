//--------------------------------------------------------------------
//  SoFreeTypeFont.h
//
//  oil
//  2012-04-29
//--------------------------------------------------------------------
#ifndef _SOFREETYPEFONT_H_
#define _SOFREETYPEFONT_H_
//--------------------------------------------------------------------
#include <map>
#include <vector>
#include <ft2build.h>
#include FT_FREETYPE_H
//--------------------------------------------------------------------
namespace N_SoFont
{
//--------------------------------------------------------------------
// 存储一个字符图片相关的结构体
struct stCharTexture
{
	LPDIRECT3DTEXTURE9 pTexture;
	short AdvanceX;
	short AdvanceY;
	short LeftMargin;
	short TopMargin;
	short BitmapWidth;
	short BitmapHeight;
	wchar_t CharID;

	stCharTexture()
	{
		pTexture = 0;
		CharID = 0;
		BitmapWidth = 0;
		BitmapHeight = 0;
	}
};
//--------------------------------------------------------------------
//存储一个字符的图像纹理信息
struct stCharGlyphData
{
	//本字符的像素数据Rect处于字体纹理贴图的第几个Rect。
	//值为0表示处于第一个Rect。值为-1表示没有本字符的像素数据。
	int GlyphIndex;
	//存储本字符的字符编码。
	wchar_t Character;
	//本字符在文本排版中的步进宽度。
	short AdvanceX;
	//本字符在文本排版中的步进高度。
	short AdvanceY;
	//字符的步进宽度和步进高度构成了外围Rect，像素数据Bitmap构成了内围Rect，
	//外围Rect和内围Rect之间的空隙就是左边距，右边距，上边距，下边距。
	//这里只需要记录左边距和上边距即可。
	short LeftMargin;
	short TopMargin;
	//本字符像素数据的宽度。
	short BitmapWidth;
	//本字符像素数据的高度。
	short BitmapHeight;

	stCharGlyphData()
	:GlyphIndex(-1), Character(0), AdvanceX(0), AdvanceY(0)
	,LeftMargin(0), TopMargin(0), BitmapWidth(0), BitmapHeight(0)
	{
	}
};
//--------------------------------------------------------------------
class SoFreeTypeFont
{
public:
	SoFreeTypeFont();
	~SoFreeTypeFont();

	//载入并打开字体文件，并没有把字体文件中的字符解析加载到内存中。
	//--pFontFileName 字体文件完整路径。
	//--nFontFaceIndex 使用字体的哪个字形，一般取默认值即可，默认值为0。
	//--nFontSizeWidth 字号宽度。
	//--nFontSizeHeight 字号高度。
	//--bFontEdge 是否描边。如果描边的话，则绘制一个像素的边缘。
	//--bPreloadASCII 是否预先载入ASCII字符。
	//返回是否执行成功。如果返回false，则本对象不能使用，外界应该删除掉。
	bool InitFont(const char* pFontFileName, int nFontFaceIndex, int nFontSizeWidth, int nFontSizeHeight, bool bFontEdge, bool bPreloadASCII);
	//把一个字符图像绘制到指定贴图的指定位置上。
	//--pDestTexture 目标纹理贴图。目标纹理必须是D3DFMT_A8R8G8B8格式。
	//--nStartX 从哪个像素点开始绘制字符图像。
	//--nStartY 从哪个像素点开始绘制字符图像。
	//--theChar 绘制哪个字符。
	//--fColorR, fColorG, fColorB 颜色，取值范围[0.0f, 1.0f]。
	//--pAdvanceX [out]如果值不为NULL，则返回该字符在水平方向上占据多少个像素。
	//--pAdvanceY [out]如果值不为NULL，则返回该字符在垂直方向上占据多少个像素。
	bool DrawCharacter(IDirect3DTexture9* pDestTexture, int nStartX, int nStartY, wchar_t theChar, 
		float fColorR, float fColorG, float fColorB, int* pAdvanceX, int* pAdvanceY);
	//获取指定的字符在水平方向上和垂直方向上分别占据多少个像素。
	bool GetCharAdvance(wchar_t theChar, int& nAdvanceX, int& nAdvanceY);

protected:
	bool DrawCharacter_Edge(IDirect3DTexture9* pDestTexture, int nStartX, int nStartY, wchar_t theChar, 
		float fColorR, float fColorG, float fColorB, int* pAdvanceX, int* pAdvanceY);
	bool OpenFontFile();
	void Clear();
	const stCharGlyphData* LoadSingleChar(wchar_t theChar);
	bool DrawGlyphToTexture(const stCharGlyphData& stData, unsigned char* pPixelBuffer);
	bool DrawGlyphWithEdgeToTexture(const stCharGlyphData& stData, unsigned char* pPixelBuffer);
	void FillPixelData(IDirect3DTexture9* pTexture, const RECT& dest_rect, unsigned char* pPixelBuffer, int nWidth, int nHeight, bool bEdge);
	void GetThreeIndexByGlobalIndex(int nGlobalIndex, int& nTextureIndex, int& nSlotIndexX, int& nSlotIndexY);
	//获取纹理贴图数组中的第nTextureIndex张贴图。
	//如果该贴图尚未创建，则创建。
	IDirect3DTexture9* GetOrCreateTexture(int nTextureIndex);

private:
	typedef std::map<wchar_t, stCharGlyphData> mapChar2GlyphData;
	typedef std::vector<IDirect3DTexture9*> vecTexture;

private:
	//静态变量，存储纹理贴图的宽高。
	static int ms_nTextureWidth;
	static int ms_nTextureHeight;

private:
	//FreeType中一种外观的句柄。
	FT_Face m_FontFace;
	//使用字体的哪个字形，一般取默认值即可，默认值为0。
	int m_nFontFaceIndex;
	//本字体对象的字号。
	int m_nFontSizeWidth;
	int m_nFontSizeHeight;
	//存储字体文件的名字，用于Debug。
	//字符串的内存是new出来的。
	char* m_pFontFileName;
	//根据纹理贴图的宽高和字号，计算一张纹理贴图能容纳多少个字。
	//水平方向上能容纳多少个字。
	int m_nGlyphCountX;
	//垂直方向上能容纳多少个字。
	int m_nGlyphCountY;
	//存储字符编码到字符图像数据的映射。
	mapChar2GlyphData m_mapChar2GlyphData;
	//存储纹理贴图。
	vecTexture m_vecTexture;
	//维护一个索引位置，这个索引指向纹理贴图中待填充的空白位置。
	int m_nIndexForTextureEmptySlot;
	//记录是否描边。
	bool m_bFontEdge;
	//如果描边，记录描边占据几个像素。
	unsigned char m_byEdge;
};
} //namespace N_SoFont
//--------------------------------------------------------------------
#endif
//--------------------------------------------------------------------
