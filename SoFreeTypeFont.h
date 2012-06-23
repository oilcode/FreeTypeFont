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
#include <ft2build.h>
#include FT_FREETYPE_H
//--------------------------------------------------------------------
namespace N_SoFont
{
//--------------------------------------------------------------------
// 存储一个字符图片相关的结构体
struct stCharTexture
{
	LPDIRECT3DTEXTURE9 m_Texture;
	wchar_t m_chaID;
	int m_Width;
	int m_Height;
	int m_adv_x;
	int m_adv_y;
	int m_delta_x;
	int m_delta_y;

	stCharTexture()
	{
		m_Texture = 0;
		m_chaID = 0;
		m_Width = 0;
		m_Height = 0;
	}
};
//--------------------------------------------------------------------
class SoFreeTypeFont
{
public:
	SoFreeTypeFont();
	virtual ~SoFreeTypeFont();

	bool Load(const char* pFontFileName);
	void Clear();

	//用FreeType渲染字符串。
	void DrawText(wchar_t* strText, int x, int y, int maxW, int h);
	const stCharTexture* GetCharTexture(wchar_t theChar);

protected:
	int LoadSingleChar(wchar_t theChar);

protected:
	//FreeType中一种外观的句柄。
    FT_Face d_fontFace;
	int m_nUserDataCharWidth;
	int m_nUserDataCharHeight;

	stCharTexture m_CharTextureList[256*256];
	std::map<wchar_t, int> m_mapChar2Index;
	int m_nEmptyIndexForCharTextureList;

};
} //namespace N_SoFont
//--------------------------------------------------------------------
#endif
//--------------------------------------------------------------------
