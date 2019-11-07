#include "TextureLoader.h"
#include <vector>
#include <atlstr.h>
#include "Framework/Utility/Debug.h"

namespace Framework::Utility {
    TextureLoader::TextureLoader() {
        CoInitialize(NULL);
        //�C���[�W�t�@�N�g���쐬
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
            nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mFactory));
        MY_THROW_IF_FAILED(hr);

        //�t�H�[�}�b�g�ϊ���쐬
        hr = mFactory->CreateFormatConverter(&mConverter);
        MY_THROW_IF_FAILED(hr);
    }

    TextureLoader::~TextureLoader() { }

    std::vector<BYTE> TextureLoader::load(const std::wstring& filepath, UINT* width, UINT* height) {
        //�p�X����f�R�[�_�[���쐬
        HRESULT hr = mFactory->CreateDecoderFromFilename(
            filepath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &mDecoder);
        MY_THROW_IF_FAILED_LOG(hr, "�e�N�X�`���̓ǂݍ��݂Ɏ��s���܂����B\n%ls�����݂��܂���B\n", filepath);

        //�t���[���̎擾
        hr = mDecoder->GetFrame(0, &mFrame);
        MY_THROW_IF_FAILED(hr);

        UINT w, h;
        mFrame->GetSize(&w, &h);
        //�s�N�Z���`�����擾
        WICPixelFormatGUID pixelFormat;
        hr = mFrame->GetPixelFormat(&pixelFormat);
        MY_THROW_IF_FAILED(hr);

        int stride = w * 4;
        int bufferSize = stride * h;
        std::vector<BYTE> buffer(bufferSize);
        //�s�N�Z���`����32bitRGBA�łȂ�������ϊ�����
        if (pixelFormat != GUID_WICPixelFormat32bppRGBA) {
            hr = mFactory->CreateFormatConverter(&mConverter);
            MY_THROW_IF_FAILED(hr);

            hr = mConverter->Initialize(mFrame, GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeCustom);
            MY_THROW_IF_FAILED(hr);

            hr = mConverter->CopyPixels(0, stride, bufferSize, buffer.data());
            MY_THROW_IF_FAILED(hr);
        }
        else {
            hr = mFrame->CopyPixels(nullptr, stride, bufferSize, buffer.data());
            MY_THROW_IF_FAILED(hr);
        }

        *width = w;
        *height = h;
        return buffer;
    }
} //Framework::Utility