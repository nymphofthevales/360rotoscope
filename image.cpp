#pragma once

#include <atlbase.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <cassert>
#include "WICImagingFactory.h"

#pragma comment(lib, "windowscodecs.lib")
#define IfFailedThrowHR(expr) {HRESULT hr = (expr); if (FAILED(hr)) throw hr;}
#define DIB_WIDTHBYTES(bits) ((((bits) + 31)>>5)<<2)

class Image
{
public:
    Image()
        : m_pDecoder(nullptr)
        , m_pFrame(nullptr)
        , m_pConvertedFrame(nullptr)
        , m_nWidth(0)
        , m_nHeight(0)
    {
        //Initialize COM
        CoInitialize(nullptr);
    }
    Image(const wchar_t* filename, UINT cx, UINT cy, UINT frame = 0)
        : m_pDecoder(nullptr)
        , m_pFrame(nullptr)
        , m_pConvertedFrame(nullptr)
        , m_nWidth(0)
        , m_nHeight(0)
    {
        CoInitialize(nullptr);
        this->Open(filename, cx, cy, frame);
    }

    virtual ~Image()
    {
        Cleanup();

        //Uninitialize COM
        CoUninitialize();
    }

    // Returns true if an image is loaded successfully, false otherwise
    virtual bool IsLoaded() const
    {
        return m_pConvertedFrame != nullptr;
    }

    // Returns the width of the loaded image.
    virtual UINT GetWidth() const
    {
        return m_nWidth;
    }

    // Returns the height of the loaded image.
    virtual UINT GetHeight() const
    {
        return m_nHeight;
    }

    void Open(const wchar_t* pszFile, UINT cx, UINT cy, UINT nFrame = 0)
    {
        //try
        //{
            // Cleanup a previously loaded image
        Cleanup();

        // Get the WIC factory from the singleton wrapper class
        IWICImagingFactory* pFactory = CWICImagingFactory::GetInstance().GetFactory();
        assert(pFactory);
        if (!pFactory)
            throw WINCODEC_ERR_NOTINITIALIZED;

        // Create a decoder for the given image file
        IfFailedThrowHR(pFactory->CreateDecoderFromFilename(
            pszFile, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &m_pDecoder));

        // Validate the given frame index nFrame
        UINT nCount = 0;
        // Get the number of frames in this image
        if (SUCCEEDED(m_pDecoder->GetFrameCount(&nCount)))
        {
            if (nFrame >= nCount)
                nFrame = nCount - 1; // If the requested frame number is too big, default to the last frame
        }
        // Retrieve the requested frame of the image from the decoder
        IfFailedThrowHR(m_pDecoder->GetFrame(nFrame, &m_pFrame));

        // Retrieve the image dimensions
        //IfFailedThrowHR(m_pFrame->GetSize(&m_nWidth, &m_nHeight));

        // Convert the format of the image frame to 32bppBGR
        IfFailedThrowHR(pFactory->CreateFormatConverter(&m_pConvertedFrame));
        IfFailedThrowHR(m_pConvertedFrame->Initialize(
            m_pFrame,                        // Source frame to convert
            GUID_WICPixelFormat32bppBGR,     // The desired pixel format
            WICBitmapDitherTypeNone,         // The desired dither pattern
            NULL,                            // The desired palette 
            0.f,                             // The desired alpha threshold
            WICBitmapPaletteTypeCustom       // Palette translation type
        ));

        if (!IsLoaded())
            throw WINCODEC_ERR_WRONGSTATE;


        // Create a WIC image scaler to scale the image to the requested size
        CComPtr<IWICBitmapScaler> pScaler = nullptr;
        IfFailedThrowHR(pFactory->CreateBitmapScaler(&pScaler));
        IfFailedThrowHR(pScaler->Initialize(m_pConvertedFrame, cx, cy, WICBitmapInterpolationModeFant));

        // Render the image to a GDI device context
        HBITMAP hDIBBitmap = NULL;

        // Get a DC for the full screen
        HDC hdcScreen = GetDC(NULL);
        if (!hdcScreen)
            throw 1;

        BITMAPINFO bminfo;
        ZeroMemory(&bminfo, sizeof(bminfo));
        bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bminfo.bmiHeader.biWidth = cx;
        bminfo.bmiHeader.biHeight = -(LONG)cy;
        bminfo.bmiHeader.biPlanes = 1;
        bminfo.bmiHeader.biBitCount = 32;
        bminfo.bmiHeader.biCompression = BI_RGB;

        void* pvImageBits = nullptr;    // Freed with DeleteObject(hDIBBitmap)
        hDIBBitmap = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
        if (!hDIBBitmap)
            throw 2;

        ReleaseDC(NULL, hdcScreen);

        // Calculate the number of bytes in 1 scanline
        UINT nStride = DIB_WIDTHBYTES(cx * 32);
        // Calculate the total size of the image
        UINT nImage = nStride * cy;
        // Copy the pixels to the DIB section
        IfFailedThrowHR(pScaler->CopyPixels(nullptr, nStride, nImage, reinterpret_cast<BYTE*>(pvImageBits)));

        m_bitmapFromMemory = VirtualAlloc(0, cx * cy * sizeof(unsigned int), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); //freed in destructor

        unsigned int* pixel = (unsigned int*)m_bitmapFromMemory;
        unsigned int* og = (unsigned int*)pvImageBits;

        for (UINT i = 0; i < cy; i++) {
            for (UINT k = 0; k < cx; k++)
            {
                *pixel++ = *og++;
            }
        }

        m_nHeight = cy;
        m_nWidth = cx;

        DeleteObject(hDIBBitmap);
        /*}
        catch (...)
        {
            // Cleanup after something went wrong
            Cleanup();
            if (hDIBBitmap)
                DeleteObject(hDIBBitmap);
            // Rethrow the exception, so the client code can handle it
            throw;
        }*/

    }
    void passToBuffer(int x, int y)
    {
        // this needs to be able to handle cases where the image is bigger than the client's screen, and won't need the half-off render lmao


        if (x > renderState.width || y > renderState.height || x + m_nWidth < 0 || y + m_nHeight < 0 || x + m_nWidth > renderState.width)  //if the whole img is off-screen, forget it.
            return;

        unsigned int* pixel = (unsigned int*)renderState.memory;
        unsigned int* og = (unsigned int*)m_bitmapFromMemory;

        if (x < 0 && y < 0)
        {
            int xdist{ 0 - x };
            int ydist{ 0 - y };
            og += (ydist * m_nWidth) + xdist;
            for (int i = 0; i < m_nHeight - ydist; i++) {
                for (int k = 0; k < m_nWidth - xdist; k++)
                {
                    *pixel++ = *og++;
                }
                pixel += renderState.width - (m_nWidth - xdist);
                og += xdist;
            }
            return;
        }
        else if (x < 0)
        {
            int dist{ 0 - x };
            pixel += (renderState.width * y);
            og += dist;
            for (int i = 0; i < m_nHeight; i++) {
                for (int k = 0; k < m_nWidth - dist; k++)
                {
                    *pixel++ = *og++;
                }
                pixel += renderState.width - (m_nWidth - dist);
                og += dist;
            }
            return;
        }
        else if (y < 0)
        {
            int dist{ 0 - y };
            pixel += x;
            og += dist * m_nWidth;
            for (int i = 0; i < m_nHeight - dist; i++) {
                for (int k = 0; k < m_nWidth; k++)
                {
                    *pixel++ = *og++;
                }
                pixel += renderState.width - m_nWidth;
            }
            return;
        }
        else if (y + m_nHeight > renderState.height)
        {
            pixel += (renderState.width * y) + x;
            for (int i = 0; i < m_nHeight - ((y + m_nHeight) - renderState.height); i++) {
                for (int k = 0; k < m_nWidth; k++)
                {
                    *pixel++ = *og++;
                }
                pixel += renderState.width - m_nWidth;
            }
            return;
        }


        pixel += (renderState.width * y) + x;
        for (int i = 0; i < m_nHeight; i++) {
            for (int k = 0; k < m_nWidth; k++)
            {
                *pixel++ = *og++;
            }
            pixel += renderState.width - m_nWidth;
        }
    }

protected:
    virtual void Cleanup()
    {
        m_nWidth = m_nHeight = 0;

        if (m_pConvertedFrame)
        {
            m_pConvertedFrame.Release();
            m_pConvertedFrame = nullptr;
        }
        if (m_pFrame)
        {
            m_pFrame.Release();
            m_pFrame = nullptr;
        }
        if (m_pDecoder)
        {
            m_pDecoder.Release();
            m_pDecoder = nullptr;
        }
        if (m_bitmapFromMemory)
        {
            VirtualFree(m_bitmapFromMemory, 0, MEM_RELEASE);
            m_bitmapFromMemory = nullptr;
        }
    }

    CComPtr<IWICBitmapDecoder> m_pDecoder;
    CComPtr<IWICBitmapFrameDecode> m_pFrame;
    CComPtr<IWICFormatConverter> m_pConvertedFrame;
    int m_nWidth;
    int m_nHeight;
    void* m_bitmapFromMemory;
};

