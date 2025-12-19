#pragma once

#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

template <typename T>
class UploadBuffer {
 public:
  UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
      : mElementCount(elementCount), mIsConstantBuffer(isConstantBuffer) {
    mElementByteSize = sizeof(T);

    // Для константных буферов требуется выравнивание до 256 байт
    if (isConstantBuffer) {
      mElementByteSize = (sizeof(T) + 255) & ~255;
    }

    D3D12_HEAP_PROPERTIES heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mUploadBuffer)));

    ThrowIfFailed(
        mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
  }

  ~UploadBuffer() {
    if (mUploadBuffer != nullptr) {
      mUploadBuffer->Unmap(0, nullptr);
    }
    mMappedData = nullptr;
  }

  UploadBuffer(const UploadBuffer&) = delete;
  UploadBuffer& operator=(const UploadBuffer&) = delete;

  void CopyData(int elementIndex, const T& data) {
    memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
  }

  ID3D12Resource* Resource() const { return mUploadBuffer.Get(); }

 private:
  ComPtr<ID3D12Resource> mUploadBuffer;
  BYTE* mMappedData = nullptr;
  UINT mElementCount;
  UINT mElementByteSize = 0;
  bool mIsConstantBuffer;
};
