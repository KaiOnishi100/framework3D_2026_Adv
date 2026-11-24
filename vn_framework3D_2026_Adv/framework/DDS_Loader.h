#pragma once

#include "../framework/vn_environment.h"
//============================================================
// vnModel Texture Load Helper
// DDS
// TGA
// HDR
// PNG / JPG / BMP / TIFF...
//============================================================
namespace
{
	//============================================================
	// Texture FileをDirectXTex ScratchImageへ読み込む
	//============================================================
	HRESULT LoadModelTextureFile(
		const WCHAR* path,
		const WCHAR* file,
		TexMetadata& metadata,
		ScratchImage& scratchImage
	)
	{
		if (path == nullptr || file == nullptr)
		{
			return E_INVALIDARG;
		}


		//========================================================
		// Extension取得
		//========================================================
		const WCHAR* extension = wcsrchr(file, L'.');


		if (extension == nullptr)
		{
			return E_INVALIDARG;
		}


		//========================================================
		// DDS
		// AssetConverterが生成したRuntime用Texture
		// Debug: 非圧縮DDS + MipMap
		// Release: BC圧縮DDS + MipMap
		//========================================================
		if (_wcsicmp(extension, L".dds") == 0)
		{
			return LoadFromDDSFile(
				path,
				DDS_FLAGS_NONE,
				&metadata,
				scratchImage
			);
		}


		//========================================================
		// TGA
		//========================================================
		if (_wcsicmp(extension, L".tga") == 0)
		{
			return LoadFromTGAFile(
				path,
				TGA_FLAGS_NONE,
				&metadata,
				scratchImage
			);
		}


		//========================================================
		// HDR
		//========================================================
		if (_wcsicmp(extension, L".hdr") == 0)
		{
			return DirectX::LoadFromHDRFile(
				path,
				&metadata,
				scratchImage
			);
		}


		//========================================================
		// PNG / JPG / BMP / TIFF ...
		//
		// 既存V1 Assetとの互換用
		//========================================================
		return LoadFromWICFile(
			path,
			WIC_FLAGS_NONE,
			&metadata,
			scratchImage
		);
	}


    //============================================================
    // GPU Texture Upload Batch
    // 複数TextureのCopyTextureRegionを1つのCommandListに記録し
    // GPU完了待機をまとめて実行する
    // 注意:
    // ・Scene初期化中のメインスレッド専用
    // ・通常のDirect3D描画CommandListとは独立
    // ・GPU完了までUpload Bufferと転送先Textureを保持
    //============================================================
    struct ModelTextureUploadBatch
    {
        bool Active = false;
        bool Failed = false;

        ComPtr<ID3D12CommandAllocator> Allocator;
        ComPtr<ID3D12GraphicsCommandList> CommandList;

        // GPU転送中の一時Bufferを所有する
        std::vector<ComPtr<ID3D12Resource>> UploadBuffers;

        // 転送先TextureもFence完了まで保持する
        std::vector<ComPtr<ID3D12Resource>> TargetTextures;

        UINT64 PendingBytes = 0;
        UINT PendingCount = 0;

        UINT64 TotalBytes = 0;
        UINT64 TotalTextures = 0;
        UINT64 BatchCount = 0;

        //============================================================
        // バッチ内で複数Textureが共有するUpload Buffer
        //============================================================
        // 
        // 64MiBのUPLOAD Heap Resource
        ComPtr<ID3D12Resource> SharedUploadBuffer;

        // Map済みCPU書き込み先
        BYTE* SharedMappedData = nullptr;

        // 次のTextureを書き込む位置
        UINT64 SharedNextOffset = 0;

        // 最適化効果の確認用
        UINT64 SharedTextureCount = 0;
        UINT64 SharedPageCount = 0;

        // ExecuteCommandLists後にFence完了を確認できなかった場合
        // GPUがまだ参照中の可能性があるリソースを解放しないためのフラグ
        bool SubmittedUnconfirmed = false;
    };

    //============================================================
    // 同じ翻訳単位でバッチ状態を共有する
    //============================================================
    ModelTextureUploadBatch& GetModelTextureUploadBatch()
    {
        static ModelTextureUploadBatch batch;
        return batch;
    }

    //============================================================
    // 現在のバッチをGPUへ送信する
    //============================================================
    bool FlushModelTextureUploadBatch()
    {
        auto& batch = GetModelTextureUploadBatch();

        if (!batch.Active || batch.Failed)
        {
            return false;
        }

        // 転送対象がなければ何もしない
        if (batch.PendingCount == 0)
        {
            return true;
        }

        ID3D12Device* device = Direct3D::getDevice();
        ID3D12CommandQueue* queue = Direct3D::getCommandQueue();

        if (device == nullptr ||
            queue == nullptr ||
            batch.CommandList == nullptr)
        {
            batch.Failed = true;
            return false;
        }

        //========================================================
        // GPU完了通知用Fence
        // CommandList送信前に必要なリソースを準備する
        //========================================================
        ComPtr<ID3D12Fence> fence;

        HRESULT hr = device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.GetAddressOf())
        );

        if (FAILED(hr))
        {
            batch.Failed = true;
            return false;
        }

        HANDLE fenceEvent =
            CreateEventW(nullptr, FALSE, FALSE, nullptr);

        if (fenceEvent == nullptr)
        {
            batch.Failed = true;
            return false;
        }

        //============================================================
        // 共有Upload BufferへのCPU書き込み終了
        // GPU送信前にUnmapし、今回書き込んだ範囲を通知する
        //============================================================
        if (batch.SharedUploadBuffer && batch.SharedMappedData != nullptr)
        {
            const D3D12_RANGE writtenRange =
            {
                0,
                static_cast<SIZE_T>(batch.SharedNextOffset)
            };

            batch.SharedUploadBuffer->Unmap(
                0,
                &writtenRange
            );

            batch.SharedMappedData = nullptr;
        }

        //========================================================
        // 複数Textureのコピーを記録済みのCommandListを閉じる
        //========================================================
        hr = batch.CommandList->Close();

        if (FAILED(hr))
        {
            CloseHandle(fenceEvent);
            batch.Failed = true;
            return false;
        }

        //========================================================
        // GPUへまとめて送信
        //========================================================
        ID3D12CommandList* lists[] =
        {
            batch.CommandList.Get()
        };

        queue->ExecuteCommandLists(1, lists);

        //============================================================
        // GPUが転送リソースを参照する可能性がある
        // ※Fence完了確認まではリソースを解放してはいけない
        //============================================================
        batch.SubmittedUnconfirmed = true;

        //========================================================
        // このバッチ全体に対するFenceを1回発行する
        //========================================================
        constexpr UINT64 fenceValue = 1;

        hr = queue->Signal(fence.Get(), fenceValue);

        if (FAILED(hr))
        {
            CloseHandle(fenceEvent);
            batch.Failed = true;
            return false;
        }

        //========================================================
        // GPUコピー完了まで待機
        //========================================================
        if (fence->GetCompletedValue() < fenceValue)
        {
            hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);

            if (FAILED(hr))
            {
                CloseHandle(fenceEvent);
                batch.Failed = true;
                return false;
            }

            const DWORD waitResult =
                WaitForSingleObject(fenceEvent, INFINITE);

            if (waitResult != WAIT_OBJECT_0)
            {
                CloseHandle(fenceEvent);
                batch.Failed = true;
                return false;
            }
        }

        CloseHandle(fenceEvent);

        //============================================================
        // GPU完了を確認できたのでリソース解放可能
        //============================================================
        batch.SubmittedUnconfirmed = false;

        //========================================================
        // この時点でGPUコピーはすべて完了している
        // Upload Bufferを安全に解放できる
        //========================================================
        batch.UploadBuffers.clear();
        batch.TargetTextures.clear();

        batch.SharedUploadBuffer.Reset();

        //========================================================
        // 次のバッチで新しいAllocator / CommandListを作る
        //========================================================
        batch.CommandList.Reset();
        batch.Allocator.Reset();

        batch.PendingBytes = 0;
        batch.PendingCount = 0;
        ++batch.BatchCount;

        return true;
    }

    //============================================================
    // バッチモード開始
    //============================================================
    bool BeginModelTextureUploadBatch()
    {
        auto& batch = GetModelTextureUploadBatch();

        // 二重開始は禁止
        if (batch.Active || batch.SubmittedUnconfirmed)
        {
            return false;
        }

        batch.Failed = false;
        batch.PendingBytes = 0;
        batch.PendingCount = 0;
        batch.TotalBytes = 0;
        batch.TotalTextures = 0;
        batch.BatchCount = 0;

        batch.UploadBuffers.clear();
        batch.TargetTextures.clear();
        batch.CommandList.Reset();
        batch.Allocator.Reset();

        batch.SharedTextureCount = 0;
        batch.SharedPageCount = 0;
        batch.SharedNextOffset = 0;
        batch.SharedMappedData = nullptr;
        batch.SharedUploadBuffer.Reset();

        batch.Active = true;

        return true;
    }

    //============================================================
    // 1枚のTextureコピーをバッチへ追加
    // ※この関数ではGPU完了を待たない
    // ※実際のCopyTextureRegionはすべて記録する
    //============================================================
    bool QueueModelTextureUpload(
        ID3D12Resource* texture,
        ComPtr<ID3D12Resource>& uploadBuffer,
        const std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>& layouts,
        UINT mipCount,
        UINT64 requiredBytes)
    {
        auto& batch = GetModelTextureUploadBatch();

        if (!batch.Active ||
            batch.Failed ||
            texture == nullptr ||
            uploadBuffer == nullptr ||
            mipCount == 0 ||
            layouts.size() < mipCount)
        {
            return false;
        }

        //========================================================
        // 一度に保持するUpload Bufferの上限
        // 32枚または64MiBを目安にGPUへ送信する
        // 大きなDDS1枚については単独バッチを許可する
        //========================================================
        constexpr UINT kMaxTextures = 32;
        constexpr UINT64 kMaxBytes = 64ull * 1024ull * 1024ull;

        const bool memoryLimitExceeded =
            requiredBytes > kMaxBytes || batch.PendingBytes > kMaxBytes - requiredBytes;

        if (batch.PendingCount > 0 &&
            (batch.PendingCount >= kMaxTextures ||
                memoryLimitExceeded))
        {
            if (!FlushModelTextureUploadBatch())
            {
                return false;
            }
        }

        ID3D12Device* device = Direct3D::getDevice();

        if (device == nullptr)
        {
            batch.Failed = true;
            return false;
        }

        //========================================================
        // このバッチの最初のTextureならCommandList生成
        //========================================================
        if (batch.CommandList == nullptr)
        {
            HRESULT hr = device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(batch.Allocator.ReleaseAndGetAddressOf())
            );

            if (FAILED(hr))
            {
                batch.Failed = true;
                return false;
            }

            hr = device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                batch.Allocator.Get(),
                nullptr,
                IID_PPV_ARGS(batch.CommandList.ReleaseAndGetAddressOf())
            );

            if (FAILED(hr))
            {
                batch.Failed = true;
                return false;
            }
        }

        //========================================================
        // GPU転送に必要なResourceの参照を保持
        // ※WriteModelTextureMipLevels()から戻っても
        // ※Upload Bufferが破棄されないようにする
        //========================================================
        batch.UploadBuffers.push_back(uploadBuffer);
        batch.TargetTextures.emplace_back(texture);

        //========================================================
        // 全MipのコピーをCommandListへ記録
        //========================================================
        for (UINT mip = 0; mip < mipCount; ++mip)
        {
            D3D12_TEXTURE_COPY_LOCATION destination = {};
            destination.pResource = texture;
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = mip;

            D3D12_TEXTURE_COPY_LOCATION source = {};
            source.pResource = uploadBuffer.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = layouts[mip];

            batch.CommandList->CopyTextureRegion(
                &destination,
                0, 0, 0,
                &source,
                nullptr
            );
        }

        //========================================================
        // このTextureのコピー完了後にPixel Shaderから
        // 読み込めるようResource Stateを変更する
        //========================================================
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        batch.CommandList->ResourceBarrier(1, &barrier);

        //========================================================
        // 統計
        //========================================================
        batch.PendingBytes += requiredBytes;
        ++batch.PendingCount;

        batch.TotalBytes += requiredBytes;
        ++batch.TotalTextures;

        return true;
    }

    //============================================================
    // Shared Upload Buffer Texture Upload
    // 複数TextureのMip Dataを1つのUPLOAD Heapへ格納する
    // ・64MiB以下のTextureを対象にする
    // ・512Byte境界に配置する
    // ・必要に応じて既存バッチをFlushする
    // ・GPUへのコピーは既存QueueModelTextureUploadを利用
    //============================================================
    bool QueueModelTextureUploadShared(
        ID3D12Resource* texture,
        const ScratchImage& scratchImage,
        const std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>& layouts,
        const std::vector<UINT>& numRows,
        const std::vector<UINT64>& rowSizes,
        UINT mipCount,
        UINT64 requiredBytes)
    {
        auto& batch = GetModelTextureUploadBatch();

        constexpr UINT64 kPageBytes = 64ull * 1024ull * 1024ull;

        constexpr UINT kMaxTextures = 32;

        //========================================================
        // 引数・バッチ状態チェック
        //========================================================
        if (!batch.Active ||
            batch.Failed ||
            texture == nullptr ||
            mipCount == 0 ||
            requiredBytes == 0 ||
            requiredBytes > kPageBytes ||
            layouts.size() < mipCount ||
            numRows.size() < mipCount ||
            rowSizes.size() < mipCount)
        {
            return false;
        }

        //========================================================
        // D3D12 Texture配置には512Byteアライメントが必要
        //========================================================
        constexpr UINT64 kAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

        auto AlignUp = [](UINT64 value) -> UINT64
            {
                constexpr UINT64 alignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

                return (value + alignment - 1) & ~(alignment - 1);
            };

        UINT64 baseOffset = AlignUp(batch.SharedNextOffset);

        //========================================================
        // 次のTextureを格納できるか判定
        // PendingBytesは従来の専用Upload Bufferも含む
        //========================================================
        const bool tooManyTextures = batch.PendingCount >= kMaxTextures;
        const bool tooManyBytes = batch.PendingBytes > kPageBytes - requiredBytes;
        const bool pageOverflow = baseOffset > kPageBytes - requiredBytes;

        //========================================================
        // 入り切らない場合は現在のバッチをGPUへ送信
        //========================================================
        if (batch.PendingCount > 0 && (tooManyTextures || tooManyBytes || pageOverflow))
        {
            if (!FlushModelTextureUploadBatch())
            {
                return false;
            }

            // Flush後は新しいページの先頭から使う
            baseOffset = 0;
        }

        //========================================================
        // Shared Upload Bufferがなければ新規生成
        //========================================================
        if (batch.SharedUploadBuffer == nullptr)
        {
            ID3D12Device* device = Direct3D::getDevice();

            if (device == nullptr)
            {
                batch.Failed = true;
                return false;
            }

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = kPageBytes;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            const HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(batch.SharedUploadBuffer.ReleaseAndGetAddressOf())
            );

            if (FAILED(hr))
            {
                batch.Failed = true;
                return false;
            }

            batch.SharedUploadBuffer->SetName(L"vnModel::SharedTextureUpload");

            //====================================================
            // CPUから書き込むためMapする
            //====================================================
            const D3D12_RANGE readRange = { 0, 0 };

            const HRESULT mapHr =
                batch.SharedUploadBuffer->Map(
                    0,
                    &readRange,
                    reinterpret_cast<void**>(&batch.SharedMappedData)
                );

            if (FAILED(mapHr) ||
                batch.SharedMappedData == nullptr)
            {
                batch.SharedUploadBuffer.Reset();
                batch.Failed = true;
                return false;
            }

            ++batch.SharedPageCount;
        }

        //========================================================
        // 格納範囲を再検証
        //========================================================
        if (baseOffset > kPageBytes - requiredBytes)
        {
            batch.Failed = true;
            return false;
        }

        //========================================================
        // 全Mipを共有Upload BufferへCPUコピー
        // layouts[mip].OffsetはTexture単体の相対Offset
        //========================================================
        for (UINT mip = 0; mip < mipCount; ++mip)
        {
            const Image* image = scratchImage.GetImage(mip, 0, 0);

            if (image == nullptr || image->pixels == nullptr)
            {
                return false;
            }

            const size_t sourcePitch = image->rowPitch;

            const size_t destinationPitch = layouts[mip].Footprint.RowPitch;

            const UINT64 rowSize = rowSizes[mip];
            const UINT rowCount = numRows[mip];

            //====================================================
            // 不正なPitchや画像サイズを検出
            //====================================================
            if (sourcePitch == 0 ||
                rowCount == 0 ||
                rowSize > sourcePitch ||
                rowSize > destinationPitch ||
                rowCount > image->slicePitch / sourcePitch)
            {
                return false;
            }

            //====================================================
            // ページ外への書き込みを防ぐ
            //====================================================
            const UINT64 mipEnd =
                layouts[mip].Offset +
                static_cast<UINT64>(rowCount - 1) *
                destinationPitch +
                rowSize;

            if (mipEnd > requiredBytes)
            {
                return false;
            }

            BYTE* destination =
                batch.SharedMappedData + static_cast<size_t>(baseOffset + layouts[mip].Offset);

            const BYTE* source = image->pixels;

            //====================================================
            // Mip Dataを行単位でコピー
            // BC圧縮の場合はブロック行単位になる
            // RowPitchのPadding部分はコピーしない
            //====================================================
            for (UINT row = 0; row < rowCount; ++row)
            {
                std::memcpy(
                    destination + static_cast<size_t>(row) * destinationPitch,
                    source + static_cast<size_t>(row) * sourcePitch,
                    static_cast<size_t>(rowSize)
                );
            }
        }

        //========================================================
        // GPUコピー用Footprintを作成
        // 共有Buffer内での実際の位置を反映する
        //========================================================
        auto sharedLayouts = layouts;

        for (auto& layout : sharedLayouts)
        {
            layout.Offset += baseOffset;
        }

        //========================================================
        // 既存のGPUコピー登録処理を再利用
        //========================================================
        if (!QueueModelTextureUpload(
            texture,
            batch.SharedUploadBuffer,
            sharedLayouts,
            mipCount,
            requiredBytes))
        {
            batch.Failed = true;
            return false;
        }

        //========================================================
        // 次に書き込む位置を更新
        //========================================================
        batch.SharedNextOffset = baseOffset + requiredBytes;

        ++batch.SharedTextureCount;

        return true;
    }

    //============================================================
    // バッチモード終了
    // 未送信のTextureを転送し、GPU完了まで待機する
    //============================================================
    bool EndModelTextureUploadBatch()
    {
        auto& batch = GetModelTextureUploadBatch();

        if (!batch.Active)
        {
            return false;
        }

        const bool success = !batch.Failed && FlushModelTextureUploadBatch();

#if defined(_DEBUG)
        char message[256] = {};

        sprintf_s(
            message,
            "[TextureBatch] Textures=%llu "
            "Shared=%llu Pages=%llu "
            "Batches=%llu Upload=%.2f MiB Result=%s\n",

            static_cast<unsigned long long>(batch.TotalTextures),
            static_cast<unsigned long long>(batch.SharedTextureCount),
            static_cast<unsigned long long>(batch.SharedPageCount),
            static_cast<unsigned long long>(batch.BatchCount),
            static_cast<double>(batch.TotalBytes) / (1024.0 * 1024.0),
            success ? "OK" : "FAILED"
        );

        OutputDebugStringA(message);

#endif

        //============================================================
        // バッチ終了
        //============================================================
        batch.Active = false;

        //============================================================
        // 失敗時など、まだMapされている場合は解除
        //============================================================
        if (batch.SharedUploadBuffer && batch.SharedMappedData != nullptr)
        {
            batch.SharedUploadBuffer->Unmap(0, nullptr);
            batch.SharedMappedData = nullptr;
        }

        //============================================================
        // GPU完了を確認できた場合だけResourceを解放する
        // Fence Signal/Wait失敗などでGPUの実行状況が不明な場合は参照を保持する
        // ※Scene初期化を失敗させGPUリソースを再利用しない
        //============================================================
        if (!batch.SubmittedUnconfirmed)
        {
            batch.UploadBuffers.clear();
            batch.TargetTextures.clear();
            batch.SharedUploadBuffer.Reset();

            batch.CommandList.Reset();
            batch.Allocator.Reset();

            batch.SharedNextOffset = 0;
        }
        else
        {
            batch.Failed = true;

            OutputDebugStringA(
                "[TextureBatch] GPU completion unknown; "
                "upload resources retained.\n"
            );
        }

        return success;
    }



	//============================================================
	// ScratchImageが持つ全MipをD3D12 Textureへ書き込む
	//============================================================
	bool WriteModelTextureMipLevels(
		ID3D12Resource* texture,
		const TexMetadata& metadata,
		const ScratchImage& scratchImage
	)
	{
#if defined(_DEBUG)
        {
            static unsigned long long uploadCallCount = 0;
            ++uploadCallCount;

            if (uploadCallCount <= 20 ||
                uploadCallCount % 100 == 0)
            {
                auto& batch = GetModelTextureUploadBatch();

                char message[256] = {};

                sprintf_s(
                    message,
                    "[TextureUpload] Call=%llu "
                    "BatchActive=%d State=%p\n",
                    uploadCallCount,
                    batch.Active ? 1 : 0,
                    static_cast<void*>(&batch)
                );

                OutputDebugStringA(message);
            }
        }
#endif

        //========================================================
        // 基本チェック
        //========================================================
        if (texture == nullptr)
        {
            return false;
        }

        if (metadata.dimension != TEX_DIMENSION_TEXTURE2D ||
            metadata.arraySize != 1 ||
            metadata.mipLevels == 0 ||
            metadata.mipLevels > D3D12_REQ_MIP_LEVELS)
        {
            OutputDebugStringW(L"[TextureUpload] Unsupported texture metadata.\n");

            return false;
        }

        ID3D12Device* device = Direct3D::getDevice();

        ID3D12CommandQueue* queue = Direct3D::getCommandQueue();

        if (device == nullptr || queue == nullptr)
        {
            return false;
        }

        //========================================================
        // Textureは呼び出し側でDEFAULT Heapとして生成済み
        // 初期状態はCOPY_DESTであること
        //========================================================
        const D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();

        const UINT mipCount = static_cast<UINT>(metadata.mipLevels);

        if (textureDesc.Dimension !=
            D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            textureDesc.DepthOrArraySize != 1 ||
            textureDesc.MipLevels != mipCount)
        {
            OutputDebugStringW(L"[TextureUpload] Texture description mismatch.\n");
            return false;
        }

        //========================================================
        // 全Mipのコピー配置を取得
        // ※DDSのrowPitchとD3D12 Upload BufferのrowPitchは
        // 一致するとは限らない
        // ※D3D12ではRowPitchが通常256Byte境界に揃う
        // ※GetCopyableFootprints()で正しい配置を取得する
        //========================================================
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(mipCount);

        std::vector<UINT> numRows(mipCount);

        std::vector<UINT64> rowSizes(mipCount);

        UINT64 requiredBytes = 0;

        device->GetCopyableFootprints(
            &textureDesc,
            0,                  // FirstSubresource
            mipCount,           // NumSubresources
            0,                  // BaseOffset
            layouts.data(),
            numRows.data(),
            rowSizes.data(),
            &requiredBytes
        );

        if (requiredBytes == 0 || requiredBytes == UINT64_MAX)
        {
            OutputDebugStringW(L"[TextureUpload] Invalid upload buffer size.\n");

            return false;
        }

        //========================================================
        // 一時Upload Bufferを生成
        // ※Upload HeapはCPUから書き込むためのBuffer
        // ※このBufferは描画用Textureとして保持しない
        //========================================================
        D3D12_HEAP_PROPERTIES uploadHeap = {};

        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};

        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = requiredBytes;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> uploadBuffer;

        HRESULT hr = device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())
        );

        if (FAILED(hr))
        {
            OutputDebugStringW(L"[TextureUpload] Could not create Upload Buffer.\n");

            return false;
        }

        uploadBuffer->SetName(L"vnModel::TemporaryTextureUpload");

        //============================================================
        // GPU Texture Batch中のShared Upload Buffer経路
        // 64MiB以下: 共有Upload BufferにCPUコピーして転送登録
        // 64MiB超: この分岐を通らず専用Upload Bufferへ進む
        //============================================================
        if (GetModelTextureUploadBatch().Active &&
            requiredBytes <= 64ull * 1024ull * 1024ull)
        {
            return QueueModelTextureUploadShared(
                texture,
                scratchImage,
                layouts,
                numRows,
                rowSizes,
                mipCount,
                requiredBytes
            );
        }

        //========================================================
        // Upload BufferをCPUから書き込み可能にする
        //========================================================
        BYTE* mappedData = nullptr;

        //========================================================
        // CPUはGPUのデータを読み込まないため
        // 読み取り範囲は0Byteとする
        //========================================================
        const D3D12_RANGE readRange = { 0, 0 };

        hr = uploadBuffer->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&mappedData)
        );

        if (FAILED(hr) || mappedData == nullptr)
        {
            OutputDebugStringW(L"[TextureUpload] Upload Buffer Map failed.\n");

            return false;
        }

        //========================================================
        // 全MipをUpload Bufferへコピー
        // 非圧縮DDS: 1行のPixel Dataをコピーする
        // BC圧縮DDS: 圧縮ブロック行単位でコピーする
        //========================================================
        bool copyValid = true;

        for (UINT mip = 0; mip < mipCount; ++mip)
        {
            const DirectX::Image* image = scratchImage.GetImage(mip, 0, 0);

            if (image == nullptr || image->pixels == nullptr)
            {
                copyValid = false;
                break;
            }

            //====================================================
            // コピー元のDDS RowPitch
            //====================================================
            const size_t sourceRowPitch = image->rowPitch;

            //====================================================
            // コピー先のD3D12 RowPitch
            //====================================================
            const size_t destinationRowPitch = layouts[mip].Footprint.RowPitch;

            //====================================================
            // 1行に含まれる実際のデータサイズ
            // ※D3D12 RowPitchのPadding領域はコピーしない
            //====================================================
            const UINT64 rowSize = rowSizes[mip];

            const UINT rowCount = numRows[mip];

            //====================================================
            // ScratchImageのデータ領域が十分あるか検証
            // 除算を利用して掛け算のOverflowを回避する
            //====================================================
            if (sourceRowPitch == 0 ||
                rowSize > sourceRowPitch ||
                rowCount == 0 ||
                rowCount > image->slicePitch / sourceRowPitch ||
                rowSize > destinationRowPitch)
            {
                copyValid = false;
                break;
            }

            //====================================================
            // このMipの配置先先頭
            //====================================================
            BYTE* destination = mappedData + static_cast<size_t>(layouts[mip].Offset);

            const BYTE* source = image->pixels;

            //====================================================
            // 行単位でコピー
            // ※元DDSとUpload BufferのRowPitchが異なるため
            // ※単純にslicePitch全体をmemcpyしないこと
            //====================================================
            for (UINT row = 0; row < rowCount; ++row)
            {
                std::memcpy(
                    destination + static_cast<size_t>(row) * destinationRowPitch,
                    source + static_cast<size_t>(row) * sourceRowPitch,
                    static_cast<size_t>(rowSize)
                );
            }
        }

        //========================================================
        // CPU書き込み終了
        //========================================================
        uploadBuffer->Unmap(0, nullptr);
        mappedData = nullptr;

        if (!copyValid)
        {
            OutputDebugStringW(L"[TextureUpload] Mip copy validation failed.\n");

            return false;
        }

        if (GetModelTextureUploadBatch().Active)
        {
            return QueueModelTextureUpload(
                texture,
                uploadBuffer,
                layouts,
                mipCount,
                requiredBytes
            );
        }

        //========================================================
        // 専用CommandAllocatorを作成
        //========================================================
        ComPtr<ID3D12CommandAllocator> allocator;

        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.GetAddressOf())
        );

        if (FAILED(hr))
        {
            return false;
        }

        //========================================================
        // Textureコピー専用CommandList
        //========================================================
        ComPtr<ID3D12GraphicsCommandList> commandList;

        hr = device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(commandList.GetAddressOf())
        );

        if (FAILED(hr))
        {
            return false;
        }

        //========================================================
        // 各MipをGPUコピー
        // Source: Upload BufferのPlaced Footprint
        // Destination: DEFAULT Heap TextureのMip
        //========================================================
        for (UINT mip = 0; mip < mipCount; ++mip)
        {
            D3D12_TEXTURE_COPY_LOCATION destination = {};
            destination.pResource = texture;
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = mip;

            D3D12_TEXTURE_COPY_LOCATION source = {};
            source.pResource = uploadBuffer.Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint = layouts[mip];

            commandList->CopyTextureRegion(
                &destination,
                0,
                0,
                0,
                &source,
                nullptr
            );
        }

        //========================================================
        // コピー完了後のResource State
        // COPY_DEST: コピー先Textureとして書き込み可能
        // PIXEL_SHADER_RESOURCE: PBR Pixel ShaderからTextureを読める
        //========================================================
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        commandList->ResourceBarrier(
            1,
            &barrier
        );

        //========================================================
        // CommandList確定
        //========================================================
        hr = commandList->Close();

        if (FAILED(hr))
        {
            return false;
        }

        //========================================================
        // GPU完了待ち用Fence
        //========================================================
        ComPtr<ID3D12Fence> fence;

        hr = device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.GetAddressOf())
        );

        if (FAILED(hr))
        {
            return false;
        }

        HANDLE fenceEvent =
            CreateEventW(
                nullptr,
                FALSE,
                FALSE,
                nullptr
            );

        if (fenceEvent == nullptr)
        {
            return false;
        }

        //========================================================
        // GPUコマンドを実行
        // Direct3Dと同じCommandQueueを使用
        // 先行するGPUコマンドとの実行順序が維持される
        //========================================================
        ID3D12CommandList* commandLists[] =
        {
            commandList.Get()
        };

        queue->ExecuteCommandLists(1, commandLists);

        //========================================================
        // 1Fence Signal
        //========================================================
        constexpr UINT64 fenceValue = 1;

        hr = queue->Signal(fence.Get(), fenceValue);

        if (FAILED(hr))
        {
            CloseHandle(fenceEvent);
            return false;
        }

        //========================================================
        // 1GPUがコピーを完了するまで待機
        // 完了前にUpload Bufferを解放すると
        // GPUがコピー元を参照できなくなる
        //========================================================
        if (fence->GetCompletedValue() < fenceValue)
        {
            hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);

            if (FAILED(hr))
            {
                CloseHandle(fenceEvent);
                return false;
            }

            const DWORD waitResult = WaitForSingleObject(fenceEvent, INFINITE);

            if (waitResult != WAIT_OBJECT_0)
            {
                CloseHandle(fenceEvent);
                return false;
            }
        }

        CloseHandle(fenceEvent);

        //========================================================
        // 正常完了
        // 関数を抜けるとComPtrにより自動解放:
        //   uploadBuffer
        //   allocator
        //   commandList
        //   fence
        // DEFAULT Heap Texture自体は解放されない
        // 共有キャッシュが引き続き所有する
        //========================================================
        return true;
	}
}