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
SoFreeTypeFont::SoFreeTypeFont()
:d_fontFace(0)
,m_nUserDataCharWidth(0)
,m_nUserDataCharHeight(0)
,m_nEmptyIndexForCharTextureList(0)
{
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
bool SoFreeTypeFont::Load(const char* pFontFileName)
{
	//FT模块的返回值。为0表示成功。
	FT_Error nFTResult = 0;

	//加载一个字体。
	//取默认的Face。
	int nFaceIndex = 0;
	nFTResult = FT_New_Face(ft_lib, pFontFileName, nFaceIndex, &d_fontFace);
	if (nFTResult)
	{
		::MessageBoxA(0,"加载字库文件失败",0,0);
		return false;
	}

	//FontFace加载完毕后，默认是Unicode charmap。
	//如果默认值是无效的，则尝试主动加载Unicode charmap。
	if (!d_fontFace->charmap)
	{
		nFTResult = FT_Select_Charmap(d_fontFace, FT_ENCODING_UNICODE);
		if (nFTResult)
		{
			::MessageBox(0,"选择Unicode编码失败",0,0);
			return false;
		}
	}

	//设置字符宽高，像素单位。
	m_nUserDataCharWidth = 24;
	m_nUserDataCharHeight = 24;
	nFTResult = FT_Set_Pixel_Sizes(d_fontFace, m_nUserDataCharWidth, m_nUserDataCharHeight);
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
	if (d_fontFace)
	{
		FT_Done_Face(d_fontFace);
		d_fontFace = 0;
	}
}
//--------------------------------------------------------------------
void SoFreeTypeFont::DrawText(wchar_t* strText, int x, int y, int maxW, int h)
{
	int sx = x;
	int sy = y;
	int maxH = h;
	IDirect3DVertexBuffer9* Triangle = 0;
	for(unsigned int i = 0 ; i < wcslen(strText) ; i ++)
	{
		if(strText[i] =='\n')
		{
			sx = x ; sy += maxH ;
			continue;
		}
		const stCharTexture* pCharTex = GetCharTexture(strText[i]);

		int w = pCharTex->m_Width;
		int h = pCharTex->m_Height;

		float ch_x = sx + pCharTex->m_delta_x - 0.5f;
		//float ch_y = sy - h - pCharTex->m_delta_y -0.5f;
		float ch_y = sy + (pCharTex->m_adv_y - pCharTex->m_delta_y) - 0.5f;

		if(maxH < h) maxH = h;

		if(Triangle)
			Triangle->Release();

		IDirect3DDevice9* m_pDevice = SoD3DApp::GetD3DDevice();
		m_pDevice->CreateVertexBuffer(4*sizeof(FontVertex),0,vertex_fvf,D3DPOOL_DEFAULT,&Triangle,NULL);

		FontVertex *v;
		Triangle->Lock(0,0,(void **)&v,0);

		v[0] = FontVertex(ch_x		, ch_y + h	, 0xFF000000, 0, 1 );
		v[1] = FontVertex(ch_x		, ch_y		, 0xFF000000, 0, 0 );
		v[2] = FontVertex(ch_x + w	, ch_y + h	, 0xFF000000, 1, 1 );
		v[3] = FontVertex(ch_x + w	, ch_y		, 0xFF000000, 1, 0 );

		Triangle->Unlock();

		// 开启Alphai混合与测试
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		//缩小滤波
		m_pDevice->SetSamplerState( i,D3DSAMP_MINFILTER,D3DTEXF_ANISOTROPIC) ;
		//放大滤波
		m_pDevice->SetSamplerState( i,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
		//mipmap 选择
		m_pDevice->SetSamplerState( i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);

		m_pDevice->SetStreamSource(0,Triangle,0,sizeof(FontVertex));
		m_pDevice->SetFVF(vertex_fvf);
		m_pDevice->SetTexture(0,pCharTex->m_Texture);

		// 渲染背景
		m_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);

		sx += pCharTex->m_adv_x;
		if(sx > x + maxW)
		{
			sx = x ; sy += maxH;
		}
	}
}
//--------------------------------------------------------------------
const stCharTexture* SoFreeTypeFont::GetCharTexture(wchar_t theChar)
{
	std::map<wchar_t, int>::iterator it = m_mapChar2Index.find(theChar);
	if (it != m_mapChar2Index.end())
	{
		return &(m_CharTextureList[it->second]);
	}
	else
	{
		int nIndex = LoadSingleChar(theChar);
		if (nIndex != -1)
		{
			return &(m_CharTextureList[nIndex]);
		}
		else
		{
			return NULL;
		}
	}
}
//--------------------------------------------------------------------
int SoFreeTypeFont::LoadSingleChar(wchar_t theChar)
{
	int nIndex = -1;
	//启用抗锯齿。不使用抗锯齿的话，太难看了。
	bool bAntiAliased = true;
	FT_Int32 nLoadFlags = FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | (bAntiAliased ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO);
	FT_Error nResult = FT_Load_Char(d_fontFace, theChar, nLoadFlags);
	if (nResult)
	{
		//用一个错误字符表示，比如星号。
		return nIndex;
	}

	stCharTexture& charTex = m_CharTextureList[m_nEmptyIndexForCharTextureList];
	charTex.m_chaID = theChar;

	FT_GlyphSlot theGlyph = d_fontFace->glyph;
	FT_Bitmap bitmap = theGlyph->bitmap;

	int width  =  bitmap.width;
	int height =  bitmap.rows;

	charTex.m_Width = width;
	charTex.m_Height = height;

	charTex.m_adv_x = (int)(theGlyph->advance.x >> 6); // / 64
	//charTex.m_adv_y = d_fontFace->size->metrics.y_ppem;
	charTex.m_adv_y = m_nUserDataCharHeight;
	//charTex.m_delta_x = (theGlyph->metrics.horiBearingX) >> 6;
	charTex.m_delta_x = theGlyph->bitmap_left;
	//charTex.m_delta_y = theGlyph->bitmap_top - height;
	charTex.m_delta_y = theGlyph->bitmap_top;

	LPDIRECT3DTEXTURE9 d3d9_texture = NULL;
	if (SoD3DApp::GetD3DDevice()->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &d3d9_texture, NULL) == D3D_OK)
	{
		D3DLOCKED_RECT locked_rect;
		d3d9_texture->LockRect(0, &locked_rect, NULL, 0);
		switch (d_fontFace->glyph->bitmap.pixel_mode)
		{
		case FT_PIXEL_MODE_GRAY:
			{
				for (int y = 0; y < height; ++y)
				{
					for (int x = 0; x < width; ++x)
					{
						unsigned char _vl =  (x>=bitmap.width || y>=bitmap.rows) ? 0 : bitmap.buffer[x + bitmap.width*y];
						byte* destination_pixel = ((byte*) locked_rect.pBits) + locked_rect.Pitch * y + x * 4;

						destination_pixel[0] = 0xF0; // b
						destination_pixel[1] = 0xF0; // g
						destination_pixel[2] = 0; // r
						destination_pixel[3] = _vl; // a 
					}
				}
			}
			break;
		case FT_PIXEL_MODE_MONO:
			{	
				for (int y = 0; y < height; ++y)
				{
					for (int x = 0; x < width; ++x)
					{
						unsigned char _vl = 0;
						if(bitmap.buffer[y*bitmap.pitch + x/8] & (0x80 >> (x & 7)))
							_vl = 0xFF;
						else
							_vl = 0x00;

						byte* destination_pixel = ((byte*) locked_rect.pBits) + locked_rect.Pitch * y + x * 4;
						destination_pixel[0] = 0xFF;
						destination_pixel[1] = 0xFF;
						destination_pixel[2] = 0xFF;
						destination_pixel[3] = _vl;

					}
				}
			}
			break;
		}

		d3d9_texture->UnlockRect(0);
	}
	charTex.m_Texture = d3d9_texture;

	m_mapChar2Index[theChar] = m_nEmptyIndexForCharTextureList;
	nIndex = m_nEmptyIndexForCharTextureList;
	++m_nEmptyIndexForCharTextureList;

	////=======================================
	//char szBMPName[256] = {0};
	//StringCbPrintfA(szBMPName, sizeof(szBMPName), "D:\\FreeType%d.png", m_nEmptyIndexForCharTextureList);
	//D3DXSaveTextureToFile(szBMPName, D3DXIFF_PNG, d3d9_texture, NULL);



	return nIndex;
}
//--------------------------------------------------------------------
} //namespace N_SoFont
//--------------------------------------------------------------------
