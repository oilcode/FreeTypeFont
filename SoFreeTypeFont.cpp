//--------------------------------------------------------------------
//  SoFreeTypeFont.cpp
//
//  oil
//  2012-04-29
//--------------------------------------------------------------------
#include "StdAfx.h"
#include "SoFreeTypeFont.h"
#include "strsafe.h"
//--------------------------------------------------------------------
namespace N_SoFont
{
//--------------------------------------------------------------------
//FreeType库构造和析构的引用计数。
static int s_nFTRefCount = 0;
//A handle to the FreeType library
static FT_Library ft_lib;
//字号的最小值。
const int MinFontSize = 5;
//--------------------------------------------------------------------
#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s ) s,
#define FT_ERROR_START_LIST static const char* ft_errors[] = {
#define FT_ERROR_END_LIST 0};
#include FT_ERRORS_H
//--------------------------------------------------------------------
// 顶点数据格式
struct FontVertex
{
	float _x, _y, _z, _w;
	DWORD diffuse;
	float u,v;

	FontVertex(float x, float y, DWORD color, float tx, float ty)
	{
		_x = x; _y = y;	_z = 0.f; _w = 1.0f;
		diffuse = color;
		u = tx; v = ty;
	}
};
DWORD vertex_fvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

//--------------------------------------------------------------------
int SoFreeTypeFont::ms_nTextureWidth = 512;
int SoFreeTypeFont::ms_nTextureHeight = 512;
//--------------------------------------------------------------------
SoFreeTypeFont::SoFreeTypeFont()
:m_FontFace(0)
,m_nFontFaceIndex(0)
,m_nFontSizeWidth(0)
,m_nFontSizeHeight(0)
,m_pFontFileName(0)
,m_nGlyphCountX(0)
,m_nGlyphCountY(0)
,m_nIndexForTextureEmptySlot(0)
,m_bFontEdge(false)
,m_byEdge(1)
{
	m_vecTexture.reserve(4);
	if (!s_nFTRefCount++)
	{
		FT_Init_FreeType(&ft_lib);
	}
}
//--------------------------------------------------------------------
SoFreeTypeFont::~SoFreeTypeFont()
{
	Clear();
	if (!--s_nFTRefCount)
	{
		FT_Done_FreeType(ft_lib);
	}
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::InitFont(const char* pFontFileName, int nFontFaceIndex, int nFontSizeWidth, int nFontSizeHeight, bool bFontEdge, bool bPreloadASCII)
{
	//检查参数
	if (!pFontFileName)
	{
		return false;
	}
	size_t ValidCharCount = 0;
	if (FAILED(StringCchLengthA(pFontFileName, STRSAFE_MAX_CCH, &ValidCharCount)))
	{
		return false;
	}
	m_pFontFileName = new char[ValidCharCount+1];
	StringCchCopyA(m_pFontFileName, ValidCharCount+1, pFontFileName);
	m_nFontFaceIndex = nFontFaceIndex;
	m_nFontSizeWidth = nFontSizeWidth<MinFontSize ? MinFontSize : nFontSizeWidth;
	m_nFontSizeHeight = nFontSizeHeight<MinFontSize ? MinFontSize : nFontSizeHeight;
	m_nGlyphCountX = ms_nTextureWidth / m_nFontSizeWidth;
	m_nGlyphCountY = ms_nTextureHeight / m_nFontSizeHeight;
	m_bFontEdge = bFontEdge;

	//打开字体文件。
	if (!OpenFontFile())
	{
		return false;
	}

	//预先加载常用的所有的ASCII字符。
	if (bPreloadASCII)
	{
		for (wchar_t i=0; i<128; ++i)
		{
			LoadSingleChar(i);
		}
	}
	LoadSingleChar(L'珊');

	//=======================================
	if (false)
	{
		char szBMPName[256] = {0};
		StringCbPrintfA(szBMPName, sizeof(szBMPName), "D:\\FreeTypeAll.png");
		D3DXSaveTextureToFile(szBMPName, D3DXIFF_PNG, m_vecTexture[0], NULL);
	}

	return true;
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::DrawCharacter(IDirect3DTexture9* pDestTexture, int nStartX, int nStartY, wchar_t theChar, 
								   float fColorR, float fColorG, float fColorB, int* pAdvanceX, int* pAdvanceY)
{
	DrawCharacter_Edge(pDestTexture, nStartX, nStartY, theChar, 1.0f, 0.0f, 0.0f, 0, 0);
	if (!pDestTexture)
	{
		return false;
	}

	const stCharGlyphData* pGlyphData = 0;
	mapChar2GlyphData::iterator it = m_mapChar2GlyphData.find(theChar);
	if (it == m_mapChar2GlyphData.end())
	{
		pGlyphData = LoadSingleChar(theChar);
	}
	else
	{
		pGlyphData = &(it->second);
	}
	if (!pGlyphData)
	{
		return false;
	}
	if (pGlyphData->GlyphIndex < 0)
	{
		return false;
	}

	//Lock目标纹理贴图。
	UINT uiLevel = 0;
	D3DLOCKED_RECT locked_DestRect;
	RECT dest_rect;
	dest_rect.left = nStartX + pGlyphData->LeftMargin;
	dest_rect.right = dest_rect.left + pGlyphData->BitmapWidth;
	dest_rect.top = nStartY + pGlyphData->TopMargin;
	dest_rect.bottom = dest_rect.top + pGlyphData->BitmapHeight;
	if (pDestTexture->LockRect(uiLevel, &locked_DestRect, &dest_rect, D3DLOCK_DISCARD) != D3D_OK)
	{
		return false;
	}

	int nTextureIndex = 0;
	int nSlotIndexX = 0;
	int nSlotIndexY = 0;
	GetThreeIndexByGlobalIndex(pGlyphData->GlyphIndex, nTextureIndex, nSlotIndexX, nSlotIndexY);
	IDirect3DTexture9* pSrcTexture = m_vecTexture[nTextureIndex];
	//Lock字符纹理贴图。
	D3DLOCKED_RECT locked_SrcRect;
	RECT src_rect;
	src_rect.left = nSlotIndexX * m_nFontSizeWidth + pGlyphData->LeftMargin;
	src_rect.right = src_rect.left + pGlyphData->BitmapWidth;
	src_rect.top = nSlotIndexY * m_nFontSizeHeight + pGlyphData->TopMargin;
	src_rect.bottom = src_rect.top + pGlyphData->BitmapHeight;
	if (pSrcTexture->LockRect(uiLevel, &locked_SrcRect, &src_rect, D3DLOCK_READONLY) != D3D_OK)
	{
		pDestTexture->UnlockRect(uiLevel);
		return false;
	}

	//
	for (int y = 0; y < pGlyphData->BitmapHeight; ++y)
	{
		for (int x = 0; x < pGlyphData->BitmapWidth; ++x)
		{
			unsigned char* src_pixel = ((unsigned char*)locked_SrcRect.pBits) + locked_SrcRect.Pitch * y + x;
			if (src_pixel[0] > 0)
			{
				//本像素不是透明的，src_pixel[3]就是透明度。
				unsigned char* dest_pixel = ((unsigned char*)locked_DestRect.pBits) + locked_DestRect.Pitch * y + x * 4;
				if (src_pixel[0] == 0xFF)
				{
					dest_pixel[0] = (unsigned char)(255.0f * fColorB);
					dest_pixel[1] = (unsigned char)(255.0f * fColorG);
					dest_pixel[2] = (unsigned char)(255.0f * fColorR);
				}
				else
				{
					//float fAlpha = ((float)(src_pixel[3])) / 255.0f;
					//dest_pixel[0] = (unsigned char)(dest_pixel[0]*(1.0f-fAlpha) + 255.0f*fColorB*fAlpha);
					//上面注释的两句是原始算法，下面是精简后的代码。
					float fSrcPixel3 = (float)src_pixel[0];
					float fInverseAlpha = 1.0f - fSrcPixel3 / 255.0f;
					dest_pixel[0] = (unsigned char)(dest_pixel[0]*fInverseAlpha + fColorB*fSrcPixel3);
					dest_pixel[1] = (unsigned char)(dest_pixel[1]*fInverseAlpha + fColorG*fSrcPixel3);
					dest_pixel[2] = (unsigned char)(dest_pixel[2]*fInverseAlpha + fColorR*fSrcPixel3);
				}
			}
		}
	}

	pDestTexture->UnlockRect(uiLevel);
	pSrcTexture->UnlockRect(uiLevel);

	if (pAdvanceX)
	{
		*pAdvanceX = pGlyphData->AdvanceX;
	}
	if (pAdvanceY)
	{
		*pAdvanceY = pGlyphData->AdvanceY;
	}
	//=======================================
	if (false)
	{
		char szBMPName[256] = {0};
		StringCbPrintfA(szBMPName, sizeof(szBMPName), "D:\\FreeTypeAll.png");
		D3DXSaveTextureToFile(szBMPName, D3DXIFF_PNG, m_vecTexture[0], NULL);
	}
	return true;
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::DrawCharacter_Edge(IDirect3DTexture9* pDestTexture, int nStartX, int nStartY, wchar_t theChar, 
						float fColorR, float fColorG, float fColorB, int* pAdvanceX, int* pAdvanceY)
{
	if (!pDestTexture)
	{
		return false;
	}

	const stCharGlyphData* pGlyphData = 0;
	mapChar2GlyphData::iterator it = m_mapChar2GlyphData.find(theChar);
	if (it == m_mapChar2GlyphData.end())
	{
		pGlyphData = LoadSingleChar(theChar);
	}
	else
	{
		pGlyphData = &(it->second);
	}
	if (!pGlyphData)
	{
		return false;
	}
	if (pGlyphData->GlyphIndex < 0)
	{
		return false;
	}

	int nLeftMargin = pGlyphData->LeftMargin - m_byEdge;
	int nTopMargin = pGlyphData->TopMargin - m_byEdge;
	if (nLeftMargin < 0)
	{
		nLeftMargin = 0;
	}
	int nWidth = pGlyphData->BitmapWidth + m_byEdge * 2;
	int nHeight = pGlyphData->BitmapHeight + m_byEdge * 2;

	//Lock目标纹理贴图。
	UINT uiLevel = 0;
	D3DLOCKED_RECT locked_DestRect;
	RECT dest_rect;
	dest_rect.left = nStartX + nLeftMargin;
	dest_rect.right = dest_rect.left + nWidth;
	dest_rect.top = nStartY + nTopMargin;
	dest_rect.bottom = dest_rect.top + nHeight;
	if (pDestTexture->LockRect(uiLevel, &locked_DestRect, &dest_rect, D3DLOCK_DISCARD) != D3D_OK)
	{
		return false;
	}

	int nTextureIndex = 0;
	int nSlotIndexX = 0;
	int nSlotIndexY = 0;
	GetThreeIndexByGlobalIndex(pGlyphData->GlyphIndex+1, nTextureIndex, nSlotIndexX, nSlotIndexY);
	IDirect3DTexture9* pSrcTexture = m_vecTexture[nTextureIndex];
	//Lock字符纹理贴图。
	D3DLOCKED_RECT locked_SrcRect;
	RECT src_rect;
	src_rect.left = nSlotIndexX * m_nFontSizeWidth + nLeftMargin;
	src_rect.right = src_rect.left + nWidth;
	src_rect.top = nSlotIndexY * m_nFontSizeHeight + nTopMargin;
	src_rect.bottom = src_rect.top + nHeight;
	if (pSrcTexture->LockRect(uiLevel, &locked_SrcRect, &src_rect, D3DLOCK_READONLY) != D3D_OK)
	{
		pDestTexture->UnlockRect(uiLevel);
		return false;
	}

	//
	for (int y = 0; y < nHeight; ++y)
	{
		for (int x = 0; x < nWidth; ++x)
		{
			unsigned char* src_pixel = ((unsigned char*)locked_SrcRect.pBits) + locked_SrcRect.Pitch * y + x;
			if (src_pixel[0] > 0)
			{
				//本像素不是透明的，src_pixel[3]就是透明度。
				unsigned char* dest_pixel = ((unsigned char*)locked_DestRect.pBits) + locked_DestRect.Pitch * y + x * 4;
				if (src_pixel[0] == 0xFF)
				{
					dest_pixel[0] = (unsigned char)(255.0f * fColorB);
					dest_pixel[1] = (unsigned char)(255.0f * fColorG);
					dest_pixel[2] = (unsigned char)(255.0f * fColorR);
				}
				else
				{
					//float fAlpha = ((float)(src_pixel[3])) / 255.0f;
					//dest_pixel[0] = (unsigned char)(dest_pixel[0]*(1.0f-fAlpha) + 255.0f*fColorB*fAlpha);
					//上面注释的两句是原始算法，下面是精简后的代码。
					float fSrcPixel3 = (float)src_pixel[0];
					float fInverseAlpha = 1.0f - fSrcPixel3 / 255.0f;
					dest_pixel[0] = (unsigned char)(dest_pixel[0]*fInverseAlpha + fColorB*fSrcPixel3);
					dest_pixel[1] = (unsigned char)(dest_pixel[1]*fInverseAlpha + fColorG*fSrcPixel3);
					dest_pixel[2] = (unsigned char)(dest_pixel[2]*fInverseAlpha + fColorR*fSrcPixel3);
				}
			}
		}
	}

	pDestTexture->UnlockRect(uiLevel);
	pSrcTexture->UnlockRect(uiLevel);

	if (pAdvanceX)
	{
		*pAdvanceX = pGlyphData->AdvanceX;
	}
	if (pAdvanceY)
	{
		*pAdvanceY = pGlyphData->AdvanceY;
	}
	return true;
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::GetCharAdvance(wchar_t theChar, int& nAdvanceX, int& nAdvanceY)
{
	const stCharGlyphData* pGlyphData = 0;
	mapChar2GlyphData::iterator it = m_mapChar2GlyphData.find(theChar);
	if (it == m_mapChar2GlyphData.end())
	{
		pGlyphData = LoadSingleChar(theChar);
	}
	else
	{
		pGlyphData = &(it->second);
	}
	if (pGlyphData && pGlyphData->GlyphIndex >= 0)
	{
		nAdvanceX = pGlyphData->AdvanceX;
		nAdvanceY = pGlyphData->AdvanceY;
		return true;
	}
	else
	{
		return false;
	}
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::OpenFontFile()
{
	//FT模块的返回值。为0表示成功。
	FT_Error nFTResult = 0;

	//加载一个字体。
	nFTResult = FT_New_Face(ft_lib, m_pFontFileName, m_nFontFaceIndex, &m_FontFace);
	if (nFTResult)
	{
		::MessageBoxA(0,"加载字库文件失败",0,0);
		return false;
	}

	//FontFace加载完毕后，默认是Unicode charmap。
	//这里再主动设置一下，使用Unicode charmap。
	nFTResult = FT_Select_Charmap(m_FontFace, FT_ENCODING_UNICODE);
	if (nFTResult)
	{
		::MessageBox(0,"选择Unicode编码失败",0,0);
		return false;
	}

	//设置字符宽高，像素单位。
	nFTResult = FT_Set_Pixel_Sizes(m_FontFace, m_nFontSizeWidth, m_nFontSizeHeight);
	if (nFTResult)
	{
		::MessageBox(0,"设置大小失败",0,0);
		return false;
	}

	return true;
}
//--------------------------------------------------------------------
void SoFreeTypeFont::Clear()
{
	if (m_FontFace)
	{
		FT_Done_Face(m_FontFace);
		m_FontFace = 0;
	}
	m_nFontFaceIndex = 0;
	m_nFontSizeWidth = 0;
	m_nFontSizeHeight = 0;
	if (m_pFontFileName)
	{
		delete[] m_pFontFileName;
		m_pFontFileName = 0;
	}
	m_nGlyphCountX = 0;
	m_nGlyphCountY = 0;
	m_nIndexForTextureEmptySlot = 0;
	m_mapChar2GlyphData.clear();
	for (vecTexture::iterator it = m_vecTexture.begin();
		it != m_vecTexture.end();
		++it)
	{
		(*it)->Release();
	}
	m_vecTexture.clear();
}
//--------------------------------------------------------------------
const stCharGlyphData* SoFreeTypeFont::LoadSingleChar(wchar_t theChar)
{
	//一开始就为theChar分配内存，表示这个字符尝试过加载。
	stCharGlyphData& stData = m_mapChar2GlyphData[theChar];

	//启用抗锯齿。不使用抗锯齿的话，太难看了。
	bool bAntiAliased = true;
	FT_Int32 nLoadFlags = FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | (bAntiAliased ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
	FT_Error nResult = FT_Load_Char(m_FontFace, theChar, nLoadFlags);
	if (nResult)
	{
		//字体文件中没有这个字！
		//用一个错误字符表示，比如星号。
		//未完待续。
		::MessageBoxA(0,"SoFreeTypeFont::LoadSingleChar : 字体文件中没有这个字",0,0);
		return &stData;
	}

	//
	const FT_GlyphSlot& theGlyph = m_FontFace->glyph;
	//
	stData.Character = theChar;
	//本类只生成横向排版字形，不处理竖向排版。在非等宽字体下，X方向上的步进是各不相同的。
	stData.AdvanceX = (int)(theGlyph->advance.x >> 6);
	//本类只生成横向排版字形，所以Y方向上的步进是固定的；如果是竖向排版，则X方向上的步进是固定的。
	stData.AdvanceY = m_nFontSizeHeight;
	stData.LeftMargin = theGlyph->bitmap_left;
	stData.TopMargin = m_nFontSizeHeight - theGlyph->bitmap_top;
	stData.BitmapWidth = theGlyph->bitmap.width;
	stData.BitmapHeight = theGlyph->bitmap.rows;

	//
	const FT_Bitmap& bitmap = theGlyph->bitmap;
	//一个byte表示一个像素，存储bitmap灰度图。
	unsigned char* pPixelBuffer = 0;
	bool bShouldDeletePixelBuffer = false;
	switch (bitmap.pixel_mode)
	{
	case FT_PIXEL_MODE_GRAY:
		{
			pPixelBuffer = (unsigned char*)bitmap.buffer;
			bShouldDeletePixelBuffer = false;
		}
		break;
	case FT_PIXEL_MODE_MONO:
		{
			int width = bitmap.width;
			int height = bitmap.rows;
			pPixelBuffer = new unsigned char[width*height];
			bShouldDeletePixelBuffer = true;
			int nIndex = 0;
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (bitmap.buffer[y*width + x/8] & (0x80 >> (x & 7)))
					{
						pPixelBuffer[nIndex] = 0xFF;
					}
					else
					{
						pPixelBuffer[nIndex] = 0x00;
					}
					++nIndex;
				}
			}
		}
		break;
	default:
		::MessageBoxA(0,"SoFreeTypeFont::LoadSingleChar : 不支持的 bitmap.pixel_mode",0,0);
		break;
	}

	if (!pPixelBuffer)
	{
		//出错了。
		return &stData;
	}

	stData.GlyphIndex = m_nIndexForTextureEmptySlot;
	++m_nIndexForTextureEmptySlot;
	//
	DrawGlyphToTexture(stData, pPixelBuffer);
	//
	if (m_bFontEdge)
	{
		++m_nIndexForTextureEmptySlot;
		DrawGlyphWithEdgeToTexture(stData, pPixelBuffer);
	}
	//
	if (bShouldDeletePixelBuffer)
	{
		delete [] pPixelBuffer;
	}
	//
	return &stData;
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::DrawGlyphToTexture(const stCharGlyphData& stData, unsigned char* pPixelBuffer)
{
	//获取纹理贴图中待填充的Rect位置。
	//待填充的Rect位于哪个贴图内。
	int nTextureIndex = 0;
	int nSlotIndexX = 0;
	int nSlotIndexY = 0;
	GetThreeIndexByGlobalIndex(stData.GlyphIndex, nTextureIndex, nSlotIndexX, nSlotIndexY);
	//如果贴图尚未创建，则创建。
	IDirect3DTexture9* pTexture = GetOrCreateTexture(nTextureIndex);
	if (!pTexture)
	{
		return false;
	}

	//Lock顶层贴图。
	RECT dest_rect;
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + stData.LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + stData.TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, false);
	return true;
}
//--------------------------------------------------------------------
bool SoFreeTypeFont::DrawGlyphWithEdgeToTexture(const stCharGlyphData& stData, unsigned char* pPixelBuffer)
{
	//获取纹理贴图中待填充的Rect位置。
	//待填充的Rect位于哪个贴图内。
	int nTextureIndex = 0;
	int nSlotIndexX = 0;
	int nSlotIndexY = 0;
	GetThreeIndexByGlobalIndex(stData.GlyphIndex+1, nTextureIndex, nSlotIndexX, nSlotIndexY);
	//如果贴图尚未创建，则创建。
	IDirect3DTexture9* pTexture = GetOrCreateTexture(nTextureIndex);
	if (!pTexture)
	{
		return false;
	}

	RECT dest_rect;
	short LeftMargin = 0;
	short TopMargin = 0;
	//向左偏移
	LeftMargin = stData.LeftMargin - m_byEdge;
	TopMargin = stData.TopMargin;
	if (LeftMargin < 0)
	{
		LeftMargin = 0;
	}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向左上偏移
	LeftMargin = stData.LeftMargin - m_byEdge;
	TopMargin = stData.TopMargin - m_byEdge;
	if (LeftMargin < 0)
	{
		LeftMargin = 0;
	}
	if (TopMargin < 0)
	{
		TopMargin = 0;
	}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向上偏移
	LeftMargin = stData.LeftMargin;
	TopMargin = stData.TopMargin - m_byEdge;
	if (TopMargin < 0)
	{
		TopMargin = 0;
	}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向右上偏移
	LeftMargin = stData.LeftMargin + m_byEdge;
	TopMargin = stData.TopMargin - m_byEdge;
	//if (LeftMargin + stData.BitmapWidth > m_nFontSizeWidth)
	//{
	//	LeftMargin = m_nFontSizeWidth - stData.BitmapWidth;
	//}
	if (TopMargin < 0)
	{
		TopMargin = 0;
	}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向右偏移
	LeftMargin = stData.LeftMargin + m_byEdge;
	TopMargin = stData.TopMargin;
	//if (LeftMargin + stData.BitmapWidth > m_nFontSizeWidth)
	//{
	//	LeftMargin = m_nFontSizeWidth - stData.BitmapWidth;
	//}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向右下偏移
	LeftMargin = stData.LeftMargin + m_byEdge;
	TopMargin = stData.TopMargin + m_byEdge;
	//if (LeftMargin + stData.BitmapWidth > m_nFontSizeWidth)
	//{
	//	LeftMargin = m_nFontSizeWidth - stData.BitmapWidth;
	//}
	//if (TopMargin + stData.BitmapHeight > m_nFontSizeHeight)
	//{
	//	TopMargin = m_nFontSizeHeight - stData.BitmapHeight;
	//}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向下偏移
	LeftMargin = stData.LeftMargin;
	TopMargin = stData.TopMargin + m_byEdge;
	//if (TopMargin + stData.BitmapHeight > m_nFontSizeHeight)
	//{
	//	TopMargin = m_nFontSizeHeight - stData.BitmapHeight;
	//}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	//向左下偏移
	LeftMargin = stData.LeftMargin - m_byEdge;
	TopMargin = stData.TopMargin + m_byEdge;
	if (LeftMargin < 0)
	{
		LeftMargin = 0;
	}
	//if (TopMargin + stData.BitmapHeight > m_nFontSizeHeight)
	//{
	//	TopMargin = m_nFontSizeHeight - stData.BitmapHeight;
	//}
	dest_rect.left = nSlotIndexX * m_nFontSizeWidth + LeftMargin;
	dest_rect.right = dest_rect.left + stData.BitmapWidth;
	dest_rect.top = nSlotIndexY * m_nFontSizeHeight + TopMargin;
	dest_rect.bottom = dest_rect.top + stData.BitmapHeight;
	FillPixelData(pTexture, dest_rect, pPixelBuffer, stData.BitmapWidth, stData.BitmapHeight, true);
	return true;
}
//--------------------------------------------------------------------
void SoFreeTypeFont::FillPixelData(IDirect3DTexture9* pTexture, const RECT& dest_rect, 
								   unsigned char* pPixelBuffer, int nWidth, int nHeight, bool bEdge)
{
	if (!pTexture || !pPixelBuffer)
	{
		return;
	}

	//Lock顶层贴图。
	UINT uiLevel = 0;
	D3DLOCKED_RECT locked_rect;
	if (pTexture->LockRect(uiLevel, &locked_rect, &dest_rect, D3DLOCK_DISCARD) != D3D_OK)
	{
		return;
	}

	int nIndex = 0;
	for (int y = 0; y < nHeight; ++y)
	{
		unsigned char* pDestBuffer = ((unsigned char*)locked_rect.pBits) + locked_rect.Pitch * y;
		for (int x = 0; x < nWidth; ++x)
		{
			if (bEdge)
			{
				if (pDestBuffer[x] == 0 && pPixelBuffer[nIndex] > 0)
					pDestBuffer[x] = 0x50;
			}
			else
			{
				pDestBuffer[x] = pPixelBuffer[nIndex];
			}
			++nIndex;
		}
	}
	pTexture->UnlockRect(uiLevel);
}
//--------------------------------------------------------------------
void SoFreeTypeFont::GetThreeIndexByGlobalIndex(int nGlobalIndex, int& nTextureIndex, int& nSlotIndexX, int& nSlotIndexY)
{
	if (nGlobalIndex >= 0)
	{
		int nCountPerTexture = m_nGlyphCountX * m_nGlyphCountY;
		nTextureIndex = nGlobalIndex / nCountPerTexture;
		int nYuShu = nGlobalIndex - nTextureIndex * nCountPerTexture;
		nSlotIndexY = nYuShu / m_nGlyphCountX;
		nSlotIndexX = nYuShu - nSlotIndexY * m_nGlyphCountX;
	}
}
//--------------------------------------------------------------------
IDirect3DTexture9* SoFreeTypeFont::GetOrCreateTexture(int nTextureIndex)
{
	IDirect3DTexture9* pTexture = 0;
	if ((int)m_vecTexture.size() > nTextureIndex)
	{
		pTexture = m_vecTexture[nTextureIndex];
	}
	else
	{
		//产生几层Mipmap纹理层级，值为0表示产生完整的纹理层级链表。
		UINT uiLevels = 1;
		//取默认值。
		DWORD dwUsage = 0;
		//图片像素格式，创建灰度图，只需要存储像素点的Alpha值即可。
		D3DFORMAT eFormat = D3DFMT_A8;
		//
		if (SoD3DApp::GetD3DDevice()->CreateTexture(ms_nTextureWidth, ms_nTextureHeight, 
			uiLevels, dwUsage, eFormat, D3DPOOL_MANAGED, &pTexture, NULL) == D3D_OK)
		{
			m_vecTexture.push_back(pTexture);
		}
		else
		{
			::MessageBoxA(0,"SoFreeTypeFont::GetOrCreateTexture : 创建贴图失败",0,0);
		}
	}
	return pTexture;
}
//--------------------------------------------------------------------
} //namespace N_SoFont
//--------------------------------------------------------------------
