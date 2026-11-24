#include "../framework.h"
#include "../framework/vn_environment.h"

extern HWND hWnd;


//============================================================
// Shared VNM Binary Cache
// 同じVNMファイルを複数のvnModelが使用する場合
// CPU上のバイナリデータを共有する
// 共有するもの: VNMファイルから読み込んだ生のByte配列
// 共有しないもの:
//   World Matrix
//   Object Constant Buffer
//   Material Constant Buffer
//   Animation / Bone Matrix
//   その他インスタンス固有状態
// キャッシュ形式:
//   Key   = VNMの正規化済み絶対パス
//   Value = weak_ptr
// メモリの所有: vnModel側のshared_ptr
// 
// 最後のvnModelが解放されると
// VNMのCPUバイナリも自動解放される
//============================================================
//============================================================
// 共有バイナリ本体
//============================================================
struct SharedModelBinary
{
	//========================================================
	// VNMファイルの全バイトを保持
	// インスタンスごとにコピーしない
	//========================================================
	std::unique_ptr<BYTE[]> Bytes;

	//========================================================
	// ファイルサイズ
	// 読み込み範囲の検証やデバッグに使用する
	//========================================================
	size_t Size = 0;
};

namespace
{
	//============================================================
	// Preload Pin Cache
	// 通常キャッシュ: weak_ptr
	// プリロード中: shared_ptrで一時的に保持する
	// Workerが読み込んだ直後にデータが消えることを防ぐ
	// 全vnModel生成後、ReleaseVnmPreloadCache()で解除する
	//============================================================
	std::unordered_map<std::wstring, std::shared_ptr<SharedModelBinary>> gPinnedVnmBinaryCache;


	//============================================================
	// VNMをディスクから読み込むだけの関数
	// ・GPU APIは呼び出さない
	// ・グローバルキャッシュにはアクセスしない
	// ・ログ統計も変更しない
	// 複数のWorkerから同時に呼んでも
	// 各呼び出しが独立したifstreamとByte配列を持つ
	//============================================================
	std::shared_ptr<SharedModelBinary>LoadVnmBinaryFromDisk(const std::filesystem::path& filePath)
	{
		//========================================================
		// ファイルを開く
		// ate指定で末尾から開き
		// ファイルサイズを取得できるようにする
		//========================================================
		std::ifstream file(
			filePath,
			std::ios::binary | std::ios::ate
		);

		if (!file)
		{
			return nullptr;
		}

		//========================================================
		// ファイルサイズ取得
		//========================================================
		const std::streamoff fileSize = file.tellg();

		//========================================================
		// 最低限vnModelDataのヘッダーが必要
		//========================================================
		if (fileSize < static_cast<std::streamoff>(sizeof(vnModelData)))
		{
			return nullptr;
		}

		//========================================================
		// ifstream::read()で指定できる範囲か確認
		//========================================================
		if (fileSize > (std::numeric_limits<std::streamsize>::max)())
		{
			return nullptr;
		}

		const size_t byteCount = static_cast<size_t>(fileSize);

		//========================================================
		// CPUバイナリを確保
		// 読み込まれた配列はshared_ptr経由で共有する
		//========================================================
		auto binary = std::make_shared<SharedModelBinary>();

		binary->Bytes = std::make_unique<BYTE[]>(byteCount);

		binary->Size = byteCount;

		//========================================================
		// ファイル先頭へ移動
		//========================================================
		file.seekg(0, std::ios::beg);

		if (!file)
		{
			return nullptr;
		}

		//========================================================
		// 全データを読み込む
		//========================================================
		file.read(
			reinterpret_cast<char*>(binary->Bytes.get()),
			static_cast<std::streamsize>(byteCount)
		);

		if (!file)
		{
			return nullptr;
		}

		//========================================================
		// 成功
		//========================================================
		return binary;
	}



	//========================================================
	// 共有VNMキャッシュ
	// weak_ptrなので、キャッシュ自体は
	// ファイルデータの寿命を延ばさない
	//========================================================
	std::unordered_map<std::wstring, std::weak_ptr<SharedModelBinary>> gSharedVnmBinaryCache;

	//========================================================
	// キャッシュへ同時アクセスした場合の保護
	// 今回はロード中の重複生成を確実に防ぐため
	// ファイル読み込み中もMutexを保持する
	//========================================================
	std::mutex gSharedVnmBinaryMutex;

#if defined(_DEBUG)

	//========================================================
	// Debug統計
	//========================================================
	std::uint64_t gVnmCacheHitCount = 0;
	std::uint64_t gVnmCacheMissCount = 0;

	std::uint64_t gVnmAvoidedReadBytes = 0;

	//========================================================
	// Debugログは100回の取得ごとに出力
	//========================================================
	void LogVnmCacheStats()
	{
		const std::uint64_t total =gVnmCacheHitCount + gVnmCacheMissCount;

		if (total == 0 || total % 100 != 0)
		{
			return;
		}

		const double mib =
			static_cast<double>(gVnmAvoidedReadBytes) / (1024.0 * 1024.0);

		char message[256];

		sprintf_s(
			message,
			"[VNMCache] Hit=%llu Miss=%llu "
			"AvoidedRead=%.2f MiB\n",
			static_cast<unsigned long long>(gVnmCacheHitCount),
			static_cast<unsigned long long>(gVnmCacheMissCount),
			mib
		);

		OutputDebugStringA(message);
	}

#endif

	//========================================================
	// Windows向けパス正規化
	// 同じファイルを以下のように指定した場合も
	// 共通のキャッシュキーへ変換する
	// data/model/a.vnm
	// data\model\a.vnm
	// data/model/../model/a.vnm
	// 大文字・小文字の違いも吸収する
	//========================================================
	std::wstring NormalizeVnmCachePath(const std::filesystem::path& filePath)
	{
		std::error_code error;

		//====================================================
		// カレントディレクトリから絶対パスへ変換
		//====================================================
		std::filesystem::path absolutePath = std::filesystem::absolute(filePath, error);

		//====================================================
		// 絶対パス化に失敗した場合は元のPathを使う
		//====================================================
		if (error)
		{
			absolutePath = filePath;
		}

		std::wstring result = absolutePath.lexically_normal().wstring();

		//====================================================
		// Windowsファイルパスは通常大文字小文字を区別しない
		//====================================================
		std::transform(result.begin(),result.end(),result.begin(),[](wchar_t ch)
			{
				return static_cast<wchar_t>(std::towlower(ch));
			}
		);

		return result;
	}

	//========================================================
	// VNMバイナリ取得
	// HIT: 既存の共有データを返す
	// MISS: VNMファイルを読み込んで共有データを作る
	// 失敗: nullptr
	//========================================================
	std::shared_ptr<SharedModelBinary>AcquireSharedVnmBinary(const std::filesystem::path& filePath)
	{
		//========================================================
		// 共通のキャッシュキー
		//========================================================
		const std::wstring cacheKey =NormalizeVnmCachePath(filePath);

		//========================================================
		// 通常キャッシュを保護
		//========================================================
		std::lock_guard<std::mutex> lock(gSharedVnmBinaryMutex);

		//========================================================
		// 既存VNMを検索
		//========================================================
		const auto found =gSharedVnmBinaryCache.find(cacheKey);

		if (found != gSharedVnmBinaryCache.end())
		{
			auto shared = found->second.lock();

			if (shared != nullptr)
			{
#if defined(_DEBUG)
				++gVnmCacheHitCount;

				gVnmAvoidedReadBytes +=static_cast<std::uint64_t>(shared->Size);

				LogVnmCacheStats();
#endif
				return shared;
			}

			//====================================================
			// weak_ptrの参照先が解放済みなら削除
			//====================================================
			gSharedVnmBinaryCache.erase(found);
		}

		//========================================================
		// キャッシュMISS
		// Preload対象ではないVNMのため
		// 従来どおり同期ロードして互換性を維持する
		//========================================================
		auto shared = LoadVnmBinaryFromDisk(filePath);

		if (!shared)
		{
#if defined(_DEBUG)
			OutputDebugStringW(
				L"[VNMCache] VNM disk load failed.\n"
			);
#endif
			return nullptr;
		}

		//========================================================
		// weak_ptrを登録
		//========================================================
		gSharedVnmBinaryCache[cacheKey] = shared;

#if defined(_DEBUG)
		++gVnmCacheMissCount;
		LogVnmCacheStats();
#endif

		return shared;
	}
}


//============================================================
// SharedGeometryResource
// 同一VNMを使用する複数のvnModelで共有する
// GPUジオメトリリソース
// ComPtrはCOMの参照カウントを管理するため
// 最後の参照がなくなるとGPU Resourceを解放する
//============================================================
struct SharedGeometryResource
{
	//========================================================
	// GPU Vertex / Index Buffer
	//========================================================
	ComPtr<ID3D12Resource> VertexBuffer;
	ComPtr<ID3D12Resource> IndexBuffer;

	//========================================================
	// GPU Buffer View
	//
	// 描画時に使用するBufferLocation・Stride・Format等。
	//========================================================
	D3D12_VERTEX_BUFFER_VIEW VertexView = {};
	D3D12_INDEX_BUFFER_VIEW IndexView = {};
};

//============================================================
// Geometry Cache
// 現段階ではvnModelからのみ使用するため
// vn_model.cpp内の無名Namespaceに実装する
//============================================================
namespace
{
	//========================================================
	// Key   : VNMファイルパス
	// Value : Shared Geometryへの弱い参照
	// weak_ptrを使用する理由:
	// CacheだけがGPU Resourceを保持し続けないようにする
	// 最後のvnModelが破棄されると
	// 共有Geometryも破棄できる
	//========================================================
	std::unordered_map<std::wstring, std::weak_ptr<SharedGeometryResource>> gGeometryCache;

	//========================================================
	// Find Geometry
	// Cache Hit: 既存GPU Resourceを返す
	// Cache Miss: nullptrを返す
	// 現在はScene初期化スレッドからのみ呼び出す想定
	// 並列ロード対応時にはMutex等による同期が必要
	//========================================================
	std::shared_ptr<SharedGeometryResource>FindCachedGeometry(const std::wstring& key)
	{
		const auto found = gGeometryCache.find(key);

		if (found == gGeometryCache.end())
		{
			return nullptr;
		}

		//====================================================
		// weak_ptrから有効なshared_ptrを取得する
		//====================================================
		auto resource = found->second.lock();

		if (!resource)
		{
			//================================================
			// すでに全利用者が破棄されたCache Entryは削除
			//================================================
			gGeometryCache.erase(found);

			return nullptr;
		}

		return resource;
	}

	//========================================================
	// Register Geometry
	// 新規作成した共有GeometryをCacheへ登録する
	//========================================================
	void RegisterCachedGeometry(const std::wstring& key, const std::shared_ptr<SharedGeometryResource>& resource)
	{
		if (key.empty() || !resource)
		{
			return;
		}

		gGeometryCache[key] = resource;
	}

	//============================================================
	// DDS Content Identity
	// 目的:
	//   ファイル名が違っても、DDS内容が完全に同じなら
	//   同じGPU Texture Resourceを共有する
	// 重要:
	//   ・Textureの用途(BaseColor/Normal/ORM)は別途Cache Keyの先頭で区別する
	//   ・ファイル内容は初回だけ読み取る
	//   ・アセットの更新はゲーム再起動後に反映する
	//============================================================
	//========================================================
	// 同じファイルパスのDDSを何度もHashしないためのCache
	// Key   : 正規化済みファイルパス
	// Value : SHA256識別子
	//========================================================
	std::unordered_map<std::wstring, std::wstring> gTextureContentIdentityCache;

	//========================================================
	// DDSファイルの内容から識別子を作成
	// ファイル内容を変更しない
	//========================================================
	std::wstring GetTextureContentIdentity(const WCHAR* fullPath)
	{
		if (fullPath == nullptr || fullPath[0] == L'\0')
		{
			return L"INVALID";
		}

		//====================================================
		// Path正規化
		//====================================================
		const std::filesystem::path path =
			std::filesystem::path(fullPath).lexically_normal();

		const std::wstring normalizedPath = path.wstring();

		//====================================================
		// 以前計算した同一パスのHashを再利用
		//====================================================
		const auto found =
			gTextureContentIdentityCache.find(normalizedPath);

		if (found != gTextureContentIdentityCache.end())
		{
			return found->second;
		}

		//====================================================
		// DDSファイルをBinary Modeで開く
		//====================================================
		std::ifstream input(path, std::ios::binary);

		//====================================================
		// 読めない場合:
		// Hashの代わりに元のファイルパスをKeyにする
		// 既存のパスベースの共有動作を維持できる
		//====================================================
		const std::wstring fallback = L"PATH:" + normalizedPath;

		if (!input)
		{
			return fallback;
		}

		//====================================================
		// Windows CNG SHA-256
		//====================================================
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;

		const NTSTATUS openResult =
			BCryptOpenAlgorithmProvider(
				&algorithm,
				BCRYPT_SHA256_ALGORITHM,
				nullptr,
				0
			);

		if (!BCRYPT_SUCCESS(openResult))
		{
			return fallback;
		}

		//====================================================
		// Hash処理オブジェクトを生成
		//====================================================
		const NTSTATUS createResult =
			BCryptCreateHash(
				algorithm,
				&hash,
				nullptr,
				0,
				nullptr,
				0,
				0
			);

		if (!BCRYPT_SUCCESS(createResult))
		{
			BCryptCloseAlgorithmProvider(algorithm, 0);
			return fallback;
		}

		//====================================================
		// ファイルを分割してHash計算
		// DDS全体を追加の巨大vectorへ読み込まない
		// 128KBずつ処理してRAM使用量を抑える
		//====================================================
		std::array<UCHAR, 128 * 1024> buffer = {};

		bool success = true;

		while (input)
		{
			input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

			const std::streamsize readBytes = input.gcount();

			if (readBytes <= 0)
			{
				break;
			}

			const NTSTATUS updateResult =
				BCryptHashData(
					hash,
					buffer.data(),
					static_cast<ULONG>(readBytes),
					0
				);

			if (!BCRYPT_SUCCESS(updateResult))
			{
				success = false;
				break;
			}
		}

		//====================================================
		// ファイル読み取りエラーも検出
		//====================================================
		if (input.bad())
		{
			success = false;
		}

		//====================================================
		// SHA-256は32バイト
		//====================================================
		std::array<UCHAR, 32> digest = {};

		if (success)
		{
			const NTSTATUS finishResult =
				BCryptFinishHash(
					hash,
					digest.data(),
					static_cast<ULONG>(digest.size()),
					0
				);

			success = BCRYPT_SUCCESS(finishResult);
		}

		//====================================================
		// CNG Resource解放
		//====================================================
		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(algorithm, 0);

		if (!success)
		{
			return fallback;
		}

		//====================================================
		// 32バイトを64文字の16進数へ変換
		//====================================================
		constexpr wchar_t hex[] = L"0123456789abcdef";

		std::wstring identity = L"SHA256:";
		identity.reserve(7 + digest.size() * 2);

		for (const UCHAR byte : digest)
		{
			identity.push_back(hex[(byte >> 4) & 0x0F]);
			identity.push_back(hex[byte & 0x0F]);
		}

		//====================================================
		// 今回計算した識別子を保存
		//====================================================
		gTextureContentIdentityCache.emplace(normalizedPath,identity);

		return identity;
	}

}


//============================================================
// vnSharedTextureResource
// 複数のvnModel間で共有するGPU Texture
// Texture本体: 共通リソース
// SRV Descriptor: 各MaterialのDescriptor Heapに個別作成
//============================================================
struct SharedTextureResource
{
	//========================================================
	// GPU Texture
	//========================================================
	ComPtr<ID3D12Resource> Texture;

	//========================================================
	// SRV作成時に必要な情報
	//========================================================
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

	UINT MipLevels = 0;
};

//============================================================
// Texture Resource Cache
//============================================================
namespace
{
	//========================================================
	// Cache Table
	// Key: 用途 + Texture Path
	// Value: GPU Textureへの弱い参照
	//========================================================
	std::unordered_map<std::wstring, std::weak_ptr<SharedTextureResource>> gTextureCache;

	//============================================================
	// DDS CPU Preload Cache
	// GPU Texture Cacheとは別に、CPUで読み込んだDDSを
	// 一時的に保持する
	// GPU Texture Cache: SharedTextureResourceをweak_ptrで共有
	// DDS Preload Cache: ScratchImageをshared_ptrで一時保持
	// ※プリロードの登録はWorker終了後に実施する
	//============================================================
	struct PreloadedDdsData
	{
		// DDSの幅・高さ・Mip数・Formatなど
		TexMetadata Metadata = {};

		// DDS画像本体
		// MipLevelを含む全画像データを保持する
		ScratchImage Image;
	};

	//============================================================
	// Path -> Preloaded DDS
	// 同じファイルを複数モデルが使用しても
	// CPU上に1つだけ保持する
	//============================================================
	std::unordered_map<std::wstring,std::shared_ptr<PreloadedDdsData>> gPinnedDdsPreloadCache;

	//============================================================
	// DDSキャッシュアクセス用Mutex
	// Workerは直接このMapに書き込まず
	// 読み込みがすべて終わってからまとめて登録する
	//============================================================
	std::mutex gDdsPreloadMutex;

	//============================================================
	// DDS Preload Cache Key
	// VNMで使用しているパス正規化関数を再利用する
	//============================================================
	std::wstring NormalizeDdsPreloadPath(const std::filesystem::path& path)
	{
		return NormalizeVnmCachePath(path);
	}

	//============================================================
	// プリロード済みDDS取得
	// Cache Hit  : shared_ptrを返す
	// Cache Miss : nullptr
	// shared_ptrを返すことで、キャッシュが解放されても
	// 呼び出し側で使用中のScratchImageは生存する
	//============================================================
	std::shared_ptr<PreloadedDdsData> FindPreloadedDds(const WCHAR* fullPath)
	{
		if (fullPath == nullptr || fullPath[0] == L'\0')
		{
			return nullptr;
		}

		const std::wstring key =NormalizeDdsPreloadPath(std::filesystem::path(fullPath));

		std::lock_guard<std::mutex> lock(gDdsPreloadMutex);

		const auto found =gPinnedDdsPreloadCache.find(key);

		if (found == gPinnedDdsPreloadCache.end())
		{
			return nullptr;
		}

		return found->second;
	}

	//============================================================
	// Texture CPU Data取得
	// プリロード済みならScratchImageを再利用
	// 未ロードなら従来のLoadModelTextureFileを実行
	// outImageOwner: プリロード画像の生存期間を保証するshared_ptr
	// outImage: 実際にGPUアップロードへ渡すScratchImage
	// fallbackImage: キャッシュMISS時の同期ロード先
	//============================================================
	HRESULT LoadModelTextureFileWithPreload(
		const WCHAR* fullPath,
		const WCHAR* file,
		TexMetadata& outMetadata,
		ScratchImage& fallbackImage,
		std::shared_ptr<PreloadedDdsData>& outImageOwner,
		ScratchImage*& outImage)
	{
		outImageOwner.reset();
		outImage = nullptr;

		//========================================================
		// CPUプリロードキャッシュを検索
		//========================================================
		auto cached = FindPreloadedDds(fullPath);

		if (cached)
		{
			//====================================================
			// DDSのMetadataを復元
			//====================================================
			outMetadata = cached->Metadata;

			//====================================================
			// ScratchImageをコピーせず参照する
			//====================================================
			outImageOwner = std::move(cached);

			outImage = &outImageOwner->Image;

			return S_OK;
		}

		//========================================================
		// Cache Miss
		//========================================================
		const HRESULT hr =
			LoadModelTextureFile(
				fullPath,
				file,
				outMetadata,
				fallbackImage
			);

		if (SUCCEEDED(hr))
		{
			outImage = &fallbackImage;
		}

		return hr;
	}

	//========================================================
	// Acquire Shared BaseColor Texture
	// Cache Hit:	
	// Cache Miss: DDS等を読み込み、GPU Textureを生成する
	// 注意: Scene初期化中の単一スレッド実行を想定
	//========================================================
	std::shared_ptr<SharedTextureResource>AcquireSharedBaseColorTexture(const WCHAR* fullPath, const WCHAR* file)
	{
		//====================================================
		// 引数確認
		//====================================================
		if (fullPath == nullptr || file == nullptr || fullPath[0] == L'\0' || file[0] == L'\0')
		{
			return nullptr;
		}

		//====================================================
		// Cache Key
		// BaseColorとLinear Dataを混同しないよう
		// 用途をKeyに含める
		// パス表記上の . や .. を正規化する
		//====================================================
		const std::wstring normalizedPath =
			std::filesystem::path(fullPath).lexically_normal().wstring();

		const std::wstring cacheKey = L"BaseColor|" + GetTextureContentIdentity(fullPath);

		//====================================================
		// Cache Lookup
		//====================================================
		auto found = gTextureCache.find(cacheKey);

		if (found != gTextureCache.end())
		{
			//================================================
			// weak_ptrをshared_ptrへ変換
			//================================================
			auto resource = found->second.lock();

			if (resource)
			{
#if defined(_DEBUG)
				static UINT hitCount = 0;
				++hitCount;

				if (hitCount == 1 || hitCount % 25 == 0)
				{
					wchar_t buffer[256] = {};

					swprintf_s(
						buffer,
						L"[TextureCache] HIT Count=%u\n",
						hitCount
					);

					OutputDebugStringW(buffer);
				}
#endif

				return resource;
			}

			//================================================
			// 参照が失効している場合はEntry削除
			//================================================
			gTextureCache.erase(found);
		}

		//============================================================
		// CPUプリロード済みDDSの取得
		//============================================================
		TexMetadata metadata = {};

		ScratchImage scratchImage;

		// プリロード済みDDSの所有権を保持する
		std::shared_ptr<PreloadedDdsData> preloadedOwner;

		// 実際に使用するScratchImage
		ScratchImage* sourceImage = nullptr;

		//============================================================
		// プリロード済みならファイル読み込みを省略
		// 未ロードなら従来の同期ロード
		//============================================================
		const HRESULT loadResult =
			LoadModelTextureFileWithPreload(
				fullPath,
				file,
				metadata,
				scratchImage,
				preloadedOwner,
				sourceImage
			);

		//====================================================
		// S_OKでない場合は生成しない
		// 呼び出し側がWhite Textureへフォールバックする
		//====================================================
		if (loadResult != S_OK)
		{
			OutputDebugStringW(L"[TextureCache] BaseColor load failed.\n");

			return nullptr;
		}

		//====================================================
		// Texture内容確認
		//====================================================
		const Image* image = sourceImage != nullptr ? sourceImage->GetImage(0, 0, 0) : nullptr;

		if (image == nullptr || metadata.mipLevels == 0)
		{
			return nullptr;
		}

		ID3D12Device* device = Direct3D::getDevice();

		if (device == nullptr)
		{
			return nullptr;
		}

		//====================================================
		// GPU Texture Heap
		//====================================================
		D3D12_HEAP_PROPERTIES heapProperties = {};

		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		//====================================================
		// GPU Texture Description
		// DDSに含まれるMip数を維持する
		//====================================================
		D3D12_RESOURCE_DESC textureDesc = {};

		textureDesc.Dimension =static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		textureDesc.Format = metadata.format;
		textureDesc.Width = metadata.width;
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		//====================================================
		// 共有Texture Resource生成
		//====================================================
		auto resource =
			std::make_shared<SharedTextureResource>();

		const HRESULT createResult =
			device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(resource->Texture.ReleaseAndGetAddressOf())
			);

		if (FAILED(createResult) || resource->Texture == nullptr)
		{
			OutputDebugStringW(L"[TextureCache] Texture creation failed.\n");

			return nullptr;
		}

		//============================================================
		// GPUアップロード
		// プリロード画像も使用できるようsourceImageを渡す
		//============================================================
		const bool uploadSuccess =
			WriteModelTextureMipLevels(
				resource->Texture.Get(),
				metadata,
				*sourceImage
			);

		if (!uploadSuccess)
		{
			OutputDebugStringW(L"[TextureCache] BaseColor Mip upload failed.\n");

			return nullptr;
		}

		//====================================================
		// SRV作成用Metadata保存
		//====================================================
		resource->Format = metadata.format;
		resource->MipLevels = static_cast<UINT>(metadata.mipLevels);
		resource->Texture->SetName(L"SharedBaseColorTexture");

		//====================================================
		// Cacheへ登録
		// weak_ptrなのでCache単体では所有しない
		// 各vnModelがshared_ptrを保持する
		//====================================================
		gTextureCache[cacheKey] = resource;

#if defined(_DEBUG)
		OutputDebugStringW(L"[TextureCache] MISS - Created BaseColor Texture.\n");
#endif

		return resource;
	}
}


//============================================================
// Data Texture Type
// BaseColorと異なり、Normal / ORMは色データではなく
// シェーダーが数値として使用するデータ
//============================================================
enum class DataTextureType
{
	Normal,
	ORM
};

//============================================================
// Acquire Shared Data Texture
// Normal MapとORM Mapで共通使用するTexture Cache
// Cache Hit: GPU Resourceを再利用する。
// Cache Miss: 画像ファイルを読み込み、GPU Resourceを新規生成する。
// 重要:
//     同じファイルでも用途が異なる場合は
//     別のCache Keyを使用する
//============================================================
namespace
{
	std::shared_ptr<SharedTextureResource>AcquireSharedDataTexture(
		const WCHAR* fullPath,
		const WCHAR* file,
		DataTextureType type
	)
	{
		//====================================================
		// 引数確認
		//====================================================
		if (fullPath == nullptr ||
			file == nullptr ||
			fullPath[0] == L'\0' ||
			file[0] == L'\0')
		{
			return nullptr;
		}

		//====================================================
		// Cache Key作成
		// NormalとORMを区別する
		// パス表記の「.」「..」も正規化する
		//====================================================
		const std::wstring normalizedPath =
			std::filesystem::path(fullPath).lexically_normal().wstring();

		const std::wstring cacheKey =
			(type == DataTextureType::Normal ? L"Normal|" : L"ORM|") + GetTextureContentIdentity(fullPath);

		//====================================================
		// Cache Lookup
		//====================================================
		auto found = gTextureCache.find(cacheKey);

		if (found != gTextureCache.end())
		{
			auto cachedResource = found->second.lock();

			if (cachedResource)
			{
#if defined(_DEBUG)
				//================================================
				// Cache Hitを確認
				//================================================
				static UINT normalHitCount = 0;
				static UINT ormHitCount = 0;

				UINT& hitCount =
					type == DataTextureType::Normal ? normalHitCount : ormHitCount;

				++hitCount;

				if (hitCount == 1 ||
					hitCount % 25 == 0)
				{
					wchar_t buffer[256] = {};

					swprintf_s(
						buffer,
						L"[TextureCache] %s HIT Count=%u\n",
						type == DataTextureType::Normal ? L"Normal" : L"ORM",
						hitCount
					);

					OutputDebugStringW(buffer);
				}
#endif
				return cachedResource;
			}

			//================================================
			// 参照が失効しているCache Entryを削除
			//================================================
			gTextureCache.erase(found);
		}

		//============================================================
		// CPUプリロード済みDDSの取得
		//============================================================
		TexMetadata metadata = {};

		ScratchImage scratchImage;

		// プリロード済みDDSの所有権を保持する
		std::shared_ptr<PreloadedDdsData> preloadedOwner;

		// 実際に使用するScratchImage
		ScratchImage* sourceImage = nullptr;

		//============================================================
		// プリロード済みならファイル読み込みを省略
		// 未ロードなら同期ロード
		//============================================================
		const HRESULT loadResult =
			LoadModelTextureFileWithPreload(
				fullPath,
				file,
				metadata,
				scratchImage,
				preloadedOwner,
				sourceImage
			);

		if (FAILED(loadResult))
		{
			OutputDebugStringW(L"[TextureCache] Data Texture load failed.\n");

			return nullptr;
		}

		//====================================================
		// Texture検証
		//====================================================
		if (metadata.dimension != TEX_DIMENSION_TEXTURE2D ||
			metadata.arraySize != 1 ||
			metadata.mipLevels == 0 ||
			sourceImage == nullptr ||
			sourceImage->GetImage(0, 0, 0) == nullptr)
		{
			OutputDebugStringW(L"[TextureCache] Unsupported Data Texture.\n");

			return nullptr;
		}

		//====================================================
		// Linear Format
		//====================================================
		const DXGI_FORMAT linearFormat =
			RenderFormat::ConvertToLinearFormat(metadata.format);

		if (linearFormat == DXGI_FORMAT_UNKNOWN)
		{
			OutputDebugStringW(L"[TextureCache] Unsupported Linear Format.\n");

			return nullptr;
		}

		ID3D12Device* device = Direct3D::getDevice();

		if (device == nullptr)
		{
			return nullptr;
		}

		//====================================================
		// Texture Heap
		//====================================================
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		//====================================================
		// Texture Resource Description
		//====================================================
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = metadata.width;
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		textureDesc.Format = linearFormat;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		//====================================================
		// Shared Texture Resource生成
		//====================================================
		auto resource = std::make_shared<SharedTextureResource>();

		const HRESULT createResult =
			device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(resource->Texture.ReleaseAndGetAddressOf())
			);

		if (FAILED(createResult) ||
			resource->Texture == nullptr)
		{
			OutputDebugStringW(
				L"[TextureCache] Data Texture creation failed.\n"
			);

			return nullptr;
		}

		//====================================================
		// Mip Levelの書き込み
		// DDS等に含まれる全Mipを転送する
		//====================================================
		const bool uploadSuccess =
			WriteModelTextureMipLevels(
				resource->Texture.Get(),
				metadata,
				*sourceImage
			);

		if (!uploadSuccess)
		{
			OutputDebugStringW(L"[TextureCache] Data Texture upload failed.\n");
			return nullptr;
		}

		//====================================================
		// SRV生成に必要なMetadataを保持する
		// Cache Hit時は画像ファイルを読み込まないため、
		// この情報を再利用する
		//====================================================
		resource->Format = linearFormat;
		resource->MipLevels = static_cast<UINT>(metadata.mipLevels);
		resource->Texture->SetName(
			type == DataTextureType::Normal ? L"vnSharedNormalTexture" : L"vnSharedORMTexture"
		);

		//====================================================
		// Cache登録
		//====================================================
		gTextureCache[cacheKey] = resource;

#if defined(_DEBUG)
		OutputDebugStringW(
			type == DataTextureType::Normal ? L"[TextureCache] Normal MISS\n" : L"[TextureCache] ORM MISS\n"
		);
#endif

		return resource;
	}
}


//============================================================
// Additional Texture Kind
// 今回追加する3種類のTextureを識別する
// Cache Keyに種類を含めることで、
// 同一ファイルを異なる用途で使用しても混同しない
//============================================================
enum class AdditionalTextureKind
{
	Emissive = 0,
	Thickness,
	SkinMask
};

namespace
{
	//============================================================
	// Textureの用途名
	// Cache KeyとDebug Logで共通使用する
	//============================================================
	const wchar_t* GetAdditionalTextureName(
		AdditionalTextureKind kind)
	{
		switch (kind)
		{
		case AdditionalTextureKind::Emissive:
			return L"Emissive";

		case AdditionalTextureKind::Thickness:
			return L"Thickness";

		case AdditionalTextureKind::SkinMask:
			return L"SkinMask";

		default:
			return L"Unknown";
		}
	}

	//============================================================
	// Acquire Shared Additional Texture
	// Emissive / Thickness / SkinMaskを共有する
	// Cache Hit: 既存のGPU Textureを取得する
	// Cache Miss: ファイル読み込み、Texture生成、Mip書き込みを行う
	//============================================================
	std::shared_ptr<SharedTextureResource>AcquireSharedAdditionalTexture(
		const WCHAR* fullPath,
		const WCHAR* file,
		AdditionalTextureKind kind)
	{
		//========================================================
		// 引数確認
		//========================================================
		if (fullPath == nullptr ||
			file == nullptr ||
			fullPath[0] == L'\0' ||
			file[0] == L'\0')
		{
			return nullptr;
		}

		const wchar_t* typeName =
			GetAdditionalTextureName(kind);

		//========================================================
		// Cache Key
		//========================================================
		const std::wstring normalizedPath =
			std::filesystem::path(fullPath).lexically_normal().wstring();

		const std::wstring cacheKey =
			std::wstring(typeName)+ L"|"+ GetTextureContentIdentity(fullPath);

		//========================================================
		// Cache Lookup
		//========================================================
		const auto found = gTextureCache.find(cacheKey);

		if (found != gTextureCache.end())
		{
			auto cachedTexture = found->second.lock();

			if (cachedTexture)
			{
#if defined(_DEBUG)
				//================================================
				// Texture種類ごとにHit回数を記録
				//================================================
				static UINT hitCounts[3] = {};

				UINT& hitCount =
					hitCounts[static_cast<unsigned int>(kind)];

				++hitCount;

				if (hitCount == 1 || hitCount % 25 == 0)
				{
					const std::wstring message =
						L"[TextureCache] " +
						std::wstring(typeName) +
						L" HIT Count=" +
						std::to_wstring(hitCount) +
						L"\n";

					OutputDebugStringW(message.c_str());
				}
#endif
				return cachedTexture;
			}

			//================================================
			// 参照が失効したEntryは削除する
			//================================================
			gTextureCache.erase(found);
		}

		//============================================================
		// CPUプリロード済みDDSの取得
		//============================================================
		TexMetadata metadata = {};

		ScratchImage scratchImage;

		// プリロード済みDDSの所有権を保持する
		std::shared_ptr<PreloadedDdsData> preloadedOwner;

		// 実際に使用するScratchImage
		ScratchImage* sourceImage = nullptr;

		//============================================================
		// プリロード済みならファイル読み込みを省略
		// 未ロードなら同期ロード
		//============================================================
		const HRESULT loadResult =
			LoadModelTextureFileWithPreload(
				fullPath,
				file,
				metadata,
				scratchImage,
				preloadedOwner,
				sourceImage
			);

		if (FAILED(loadResult))
		{
			OutputDebugStringW(L"[TextureCache] Additional Texture load failed.\n");
			return nullptr;
		}

		//========================================================
		// Texture検証
		//========================================================
		if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
			metadata.arraySize != 1 ||
			metadata.width == 0 ||
			metadata.height == 0 ||
			metadata.mipLevels == 0 ||
			sourceImage == nullptr ||
			sourceImage->GetImage(0, 0, 0) == nullptr)
		{
			return nullptr;
		}
		//========================================================
		// Texture Format
		// Emissive: 元の画像フォーマットを維持する
		// Thickness / SkinMask: 数値データなのでLinear Formatを使用する
		//========================================================
		DXGI_FORMAT textureFormat = metadata.format;

		if (kind != AdditionalTextureKind::Emissive)
		{
			textureFormat = RenderFormat::ConvertToLinearFormat(metadata.format);
		}

		if (textureFormat == DXGI_FORMAT_UNKNOWN)
		{
			return nullptr;
		}

		ID3D12Device* device = Direct3D::getDevice();

		if (device == nullptr)
		{
			return nullptr;
		}

		//========================================================
		// Texture Heap
		//========================================================
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		//========================================================
		// Texture Description
		//========================================================
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = metadata.width;
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		textureDesc.Format = textureFormat;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		//========================================================
		// Shared Texture生
		// ComPtrが作成したResourceを所有する
		//========================================================
		auto resource =
			std::make_shared<SharedTextureResource>();

		const HRESULT createResult =
			device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(resource->Texture.ReleaseAndGetAddressOf())
			);

		if (FAILED(createResult) || resource->Texture == nullptr)
		{
			OutputDebugStringW(L"[TextureCache] Additional Texture creation failed.\n");

			return nullptr;
		}

		//========================================================
		// DDS等の全MipをResourceへ書き込む
		// Emissiveもここで1回だけResourceを生成する
		//========================================================
		const bool uploadSuccess =
			WriteModelTextureMipLevels(
				resource->Texture.Get(),
				metadata,
				*sourceImage
			);

		if (!uploadSuccess)
		{
			OutputDebugStringW(L"[TextureCache] Additional Texture upload failed.\n");
			return nullptr;
		}

		//========================================================
		// Cache Hit時にもSRVを作れるようMetadataを保存
		//========================================================
		resource->Format = textureFormat;
		resource->MipLevels = static_cast<UINT>(metadata.mipLevels);
		resource->Texture->SetName(L"vnSharedAdditionalTexture");

		//========================================================
		// Cacheへ弱参照を登録
		//========================================================
		gTextureCache[cacheKey] = resource;

#if defined(_DEBUG)
		const std::wstring message =
			L"[TextureCache] " +
			std::wstring(typeName) +
			L" MISS\n";

		OutputDebugStringW(message.c_str());
#endif

		return resource;
	}

	//============================================================
	// Create Additional Texture SRV
	// GPU Textureは共有するが、
	// Descriptor Heapは各vnModelのMaterialが所有する
	// 引数:
	// heap            : MaterialのDescriptor Heap
	// descriptorIndex : MaterialDescriptorのSlot
	// resource        : 共有Texture
	//============================================================
	bool CreateSharedAdditionalTextureSRV(
		ID3D12DescriptorHeap* heap,
		UINT descriptorIndex,
		const std::shared_ptr<SharedTextureResource>& resource)
	{
		if (heap == nullptr || resource == nullptr || resource->Texture == nullptr)
		{
			return false;
		}

		ID3D12Device* device = Direct3D::getDevice();

		if (device == nullptr)
		{
			return false;
		}

		//========================================================
		// SRVの書き込み先Handleを計算
		//========================================================
		const UINT descriptorSize =
			device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			heap->GetCPUDescriptorHandleForHeapStart();

		handle.ptr +=
			static_cast<SIZE_T>(descriptorIndex) * static_cast<SIZE_T>(descriptorSize);

		//========================================================
		// SRV Description
		// Cacheに保存したフォーマット・Mip数を使用する
		//========================================================
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Format = resource->Format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = resource->MipLevels;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		//========================================================
		// Material固有のDescriptorへ共有Textureを設定
		//========================================================
		device->CreateShaderResourceView(
			resource->Texture.Get(),
			&srvDesc,
			handle
		);

		return true;
	}
}

//============================================================
// Static Geometry GPU Upload
// Vertex / Index BufferをDEFAULT Heapに作成する
// 入力:
//   vertexData : CPU側の頂点配列
//   vertexSize : Vertex Bufferのバイト数
//   indexData  : VNM内のIndex Data
//   indexSize  : Index Bufferのバイト数
// 出力:
//   outVertexBuffer : GPU Vertex Buffer
//   outIndexBuffer  : GPU Index Buffer
// 成功時:
//   1. DEFAULT HeapにVB / IBを確保
//   2. 一時UPLOAD Bufferに両方のデータを書き込む
//   3. CopyBufferRegion()を2回記録
//   4. VB / IBのResource Stateを遷移
//   5. GPU完了を待つ
//   6. 一時UPLOAD Bufferを解放
//============================================================
namespace
{
	bool UploadStaticGeometryBuffers(
		const void* vertexData,
		UINT64 vertexSize,
		const void* indexData,
		UINT64 indexSize,
		ID3D12Resource** outVertexBuffer,
		ID3D12Resource** outIndexBuffer
	)
	{
		using Microsoft::WRL::ComPtr;

		//====================================================
		// 引数確認
		//====================================================
		if (outVertexBuffer == nullptr ||outIndexBuffer == nullptr)
		{
			return false;
		}

		// 失敗時に未初期化ポインタを返さない
		*outVertexBuffer = nullptr;
		*outIndexBuffer = nullptr;

		if (vertexData == nullptr ||
			indexData == nullptr ||
			vertexSize == 0 ||
			indexSize == 0)
		{
			return false;
		}

		ID3D12Device* device =Direct3D::getDevice();

		ID3D12CommandQueue* queue =Direct3D::getCommandQueue();

		if (device == nullptr || queue == nullptr)
		{
			return false;
		}

		//====================================================
		// 一時Upload Buffer内での配置を計算
		// 1個のUpload Bufferへ
		// [ Vertex Data ][ Padding ][ Index Data ]の順番で配置する
		// ※管理しやすいよう256Byte境界へIndex Dataの開始位置を揃える
		//====================================================
		constexpr UINT64 alignment = 256;

		constexpr const UINT64 maxValue =(std::numeric_limits<UINT64>::max)();

		if (vertexSize > maxValue - (alignment - 1))
		{
			return false;
		}

		const UINT64 indexOffset =(vertexSize + alignment - 1) &~(alignment - 1);

		// 全体サイズのOverflowを防ぐ
		if (indexOffset > maxValue - indexSize)
		{
			return false;
		}

		const UINT64 totalUploadSize =indexOffset + indexSize;

		//====================================================
		// Buffer Resource生成用の共通ラムダ
		// DEFAULT: GPUが繰り返し読むための最終Buffer
		// UPLOAD: CPUが書き込み、GPUがコピー元として読むBuffer
		//====================================================
		const auto createBuffer =[&](
			D3D12_HEAP_TYPE heapType,
			UINT64 bufferSize,
			D3D12_RESOURCE_STATES initialState,
			ComPtr<ID3D12Resource>& output) -> bool
			{
				D3D12_HEAP_PROPERTIES heap = {};
				heap.Type = heapType;

				D3D12_RESOURCE_DESC desc = {};
				desc.Dimension =D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Alignment = 0;
				desc.Width = bufferSize;
				desc.Height = 1;
				desc.DepthOrArraySize = 1;
				desc.MipLevels = 1;
				desc.Format = DXGI_FORMAT_UNKNOWN;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				desc.Layout =D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				desc.Flags =D3D12_RESOURCE_FLAG_NONE;

				const HRESULT hr = device->CreateCommittedResource(
					&heap,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					initialState,
					nullptr,
					IID_PPV_ARGS(output.ReleaseAndGetAddressOf())
				);

				return SUCCEEDED(hr) &&output != nullptr;
			};

		//====================================================
		// DEFAULT Heap Vertex Buffer
		// CPUから直接Mapしない
		// GPUコピー先として最初はCOPY_DESTにする
		//====================================================
		ComPtr<ID3D12Resource> gpuVertexBuffer;

		if (!createBuffer(
			D3D12_HEAP_TYPE_DEFAULT,
			vertexSize,
			D3D12_RESOURCE_STATE_COPY_DEST,
			gpuVertexBuffer))
		{
			return false;
		}

		//====================================================
		// DEFAULT Heap Index Buffer
		//====================================================
		ComPtr<ID3D12Resource> gpuIndexBuffer;

		if (!createBuffer(
			D3D12_HEAP_TYPE_DEFAULT,
			indexSize,
			D3D12_RESOURCE_STATE_COPY_DEST,
			gpuIndexBuffer))
		{
			return false;
		}

		//====================================================
		// 一時UPLOAD Buffer
		// Vertex + Indexをまとめて保存する
		// UPLOAD HeapのResource StateはGENERIC_READで固定する
		//====================================================
		ComPtr<ID3D12Resource> uploadBuffer;

		if (!createBuffer(
			D3D12_HEAP_TYPE_UPLOAD,
			totalUploadSize,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			uploadBuffer))
		{
			return false;
		}

		//====================================================
		// Upload BufferへCPUデータを書き込む
		//====================================================
		void* mapped = nullptr;

		const D3D12_RANGE readRange = { 0, 0 };

		HRESULT hr = uploadBuffer->Map(
			0,
			&readRange,
			&mapped
		);

		if (FAILED(hr) || mapped == nullptr)
		{
			return false;
		}

		BYTE* destination =static_cast<BYTE*>(mapped);

		// Vertex Data
		std::memcpy(
			destination,
			vertexData,
			static_cast<size_t>(vertexSize)
		);

		// Index Data
		std::memcpy(
			destination +static_cast<size_t>(indexOffset),
			indexData,
			static_cast<size_t>(indexSize)
		);

		// CPUからの書き込み終了
		uploadBuffer->Unmap(0, nullptr);

		//====================================================
		// 転送専用CommandAllocator
		// 描画中のCommandAllocatorやCommandListを
		// Resetしないため別に作成する
		//====================================================
		ComPtr<ID3D12CommandAllocator> allocator;

		hr = device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(allocator.GetAddressOf())
		);

		if (FAILED(hr))
		{
			return false;
		}

		//====================================================
		// 転送専用CommandList
		//====================================================
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

		//====================================================
		// GPUへVertex Bufferをコピー
		// Source      : Upload Buffer Offset 0
		// Destination : DEFAULT Vertex Buffer
		//====================================================
		commandList->CopyBufferRegion(
			gpuVertexBuffer.Get(),
			0,
			uploadBuffer.Get(),
			0,
			vertexSize
		);

		//====================================================
		// GPUへIndex Bufferをコピー
		// Source      : Upload Buffer Index Offset
		// Destination : DEFAULT Index Buffer
		//====================================================
		commandList->CopyBufferRegion(
			gpuIndexBuffer.Get(),
			0,
			uploadBuffer.Get(),
			indexOffset,
			indexSize
		);

		//====================================================
		// Resource Stateを変更
		// Vertex: COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER
		// Index: COPY_DEST -> INDEX_BUFFER
		//====================================================
		D3D12_RESOURCE_BARRIER barriers[2] = {};

		// Vertex Buffer
		barriers[0].Type =D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource =gpuVertexBuffer.Get();
		barriers[0].Transition.Subresource =D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore =D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[0].Transition.StateAfter =D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		// Index Buffer
		barriers[1].Type =D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource =gpuIndexBuffer.Get();
		barriers[1].Transition.Subresource =D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Transition.StateBefore =D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[1].Transition.StateAfter =D3D12_RESOURCE_STATE_INDEX_BUFFER;

		commandList->ResourceBarrier(
			2,
			barriers
		);

		//====================================================
		// コマンド記録終了
		//====================================================
		hr = commandList->Close();

		if (FAILED(hr))
		{
			return false;
		}

		//====================================================
		// GPU完了検出用Fence
		//====================================================
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

		HANDLE fenceEvent = CreateEventW(
			nullptr,
			FALSE,
			FALSE,
			nullptr
		);

		if (fenceEvent == nullptr)
		{
			return false;
		}

		//====================================================
		// GPUコマンド実行
		//====================================================
		ID3D12CommandList* lists[] =
		{
			commandList.Get()
		};

		queue->ExecuteCommandLists(
			1,
			lists
		);

		//====================================================
		// GPUへFence Signal
		//====================================================
		constexpr UINT64 fenceValue = 1;

		hr = queue->Signal(fence.Get(),fenceValue);

		if (FAILED(hr))
		{
			CloseHandle(fenceEvent);
			return false;
		}

		//====================================================
		// GPUコピー完了まで待機
		//====================================================
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

		//====================================================
		// 成功したGPU Resourceを呼び出し元へ渡す
		// Detach()によってComPtrから所有権を切り離す
		//====================================================
		*outVertexBuffer =
			gpuVertexBuffer.Detach();

		*outIndexBuffer =
			gpuIndexBuffer.Detach();

#if defined(_DEBUG)
		OutputDebugStringA(
			"[GeometryUpload] VB/IB -> DEFAULT Heap OK\n"
		);
#endif

		//====================================================
		// 関数終了時に自動解放:
		// ・一時Upload Buffer
		// ・CommandAllocator
		// ・CommandList
		// ・Fence
		// GPUのDEFAULT Bufferは呼び出し元へ移動済み
		//====================================================
		return true;
	}
}


//============================================================
// Main Camera Frustum Cache
// カメラのViewProjectionから6枚の平面を作り
// Bounding Sphereとの交差を判定する
// ・カメラ行列が同じなら平面を再利用する
// ・World空間で判定する
// ・カメラ外に完全に出たモデルだけ除外する
// ・境界部分では安全マージンを設ける
//============================================================
namespace
{
	//========================================================
	// 平面方程式
	// Ax + By + Cz + D = 0
	// Normalは正規化して保持する
	//========================================================
	struct CameraFrustumPlane
	{
		float A = 0.0f;
		float B = 0.0f;
		float C = 0.0f;
		float D = 0.0f;

		// 無限遠平面など、法線が作れない場合は無効化
		bool Valid = false;
	};

	//========================================================
	// 前回のカメラ行列と抽出済み平面
	//========================================================
	struct CameraFrustumCache
	{
		XMFLOAT4X4 PreviousViewProjection = {};

		std::array<CameraFrustumPlane, 6> Planes = {};

		bool Initialized = false;
	};

	CameraFrustumCache gMainCameraFrustumCache;

	//========================================================
	// 平面を正規化して保存する
	// 正規化する理由:
	//   球の半径と平面までの距離を
	//   同じワールド単位で比較するため
	//========================================================
	void SetFrustumPlane(
		CameraFrustumPlane& output,
		float a,
		float b,
		float c,
		float d)
	{
		const float lengthSquared =a * a + b * b + c * c;

		//====================================================
		// 無効な平面で誤カリングしない
		//====================================================
		if (!std::isfinite(lengthSquared) ||lengthSquared <= 1.0e-12f)
		{
			output.Valid = false;
			return;
		}

		const float inverseLength =1.0f / std::sqrt(lengthSquared);

		output.A = a * inverseLength;
		output.B = b * inverseLength;
		output.C = c * inverseLength;
		output.D = d * inverseLength;

		output.Valid =
			std::isfinite(output.A) &&
			std::isfinite(output.B) &&
			std::isfinite(output.C) &&
			std::isfinite(output.D);
	}

	//========================================================
	// ViewProjectionからFrustumを抽出する
	// DirectXのRow Vector形式ではMatrixの列同士を加減算する
	// Left   : Column4 + Column1
	// Right  : Column4 - Column1
	// Bottom : Column4 + Column2
	// Top    : Column4 - Column2
	// Near   : Column3
	// Far    : Column4 - Column3
	// ※Reverse-Zでもクリップ空間の条件は共通
	//========================================================
	void BuildMainCameraFrustum(const XMFLOAT4X4& m)
	{
		auto& p = gMainCameraFrustumCache.Planes;

		// Left
		SetFrustumPlane(
			p[0],
			m._14 + m._11,
			m._24 + m._21,
			m._34 + m._31,
			m._44 + m._41
		);

		// Right
		SetFrustumPlane(
			p[1],
			m._14 - m._11,
			m._24 - m._21,
			m._34 - m._31,
			m._44 - m._41
		);

		// Bottom
		SetFrustumPlane(
			p[2],
			m._14 + m._12,
			m._24 + m._22,
			m._34 + m._32,
			m._44 + m._42
		);

		// Top
		SetFrustumPlane(
			p[3],
			m._14 - m._12,
			m._24 - m._22,
			m._34 - m._32,
			m._44 - m._42
		);

		// Near
		SetFrustumPlane(
			p[4],
			m._13,
			m._23,
			m._33,
			m._43
		);

		// Far
		SetFrustumPlane(
			p[5],
			m._14 - m._13,
			m._24 - m._23,
			m._34 - m._33,
			m._44 - m._43
		);

		gMainCameraFrustumCache.PreviousViewProjection = m;
		gMainCameraFrustumCache.Initialized = true;
	}

	//========================================================
	// Bounding SphereがFrustum内に存在するか
	// 球がどれか1つの平面の完全に外側なら描画不要
	// 球が平面に少しでも接している場合は描画する
	//========================================================
	bool IsSphereVisibleToMainCamera(FXMVECTOR worldCenter,float worldRadius)
	{
		const XMMATRIX* viewProjection =vnCamera::getScreen();

		//====================================================
		// カメラがない場合は安全側で描画する
		//====================================================
		if (viewProjection == nullptr)
		{
			return true;
		}

		//====================================================
		// 現在の行列を取得する
		//====================================================
		XMFLOAT4X4 matrix = {};

		XMStoreFloat4x4(&matrix,*viewProjection);

		//====================================================
		// 行列が変化した場合だけ6平面を再構築する
		// カメラが動いていない場合は前回結果を利用
		//====================================================
		if (!gMainCameraFrustumCache.Initialized ||
			std::memcmp(
				&matrix,
				&gMainCameraFrustumCache.PreviousViewProjection,
				sizeof(XMFLOAT4X4)
			) != 0)
		{
			BuildMainCameraFrustum(matrix);
		}

		//====================================================
		// 中心座標をワールド空間のfloat3へ展開
		//====================================================
		XMFLOAT3 center = {};

		XMStoreFloat3(&center,worldCenter);

		//====================================================
		// Bounding Sphereの半径が不正なら除外しない
		//====================================================
		if (!std::isfinite(worldRadius) ||worldRadius <= 0.0f)
		{
			return true;
		}

		//====================================================
		// 数値誤差・境界での表示のちらつきを防止
		//====================================================
		constexpr float margin = 0.05f;

		const float radius = worldRadius + margin;

		//====================================================
		// 6枚の平面で判定
		// 内側の符号: distance >= 0
		// distance < -radius: 球が完全に外側
		//====================================================
		for (const CameraFrustumPlane& plane :gMainCameraFrustumCache.Planes)
		{
			if (!plane.Valid)
			{
				continue;
			}

			const float distance =
				plane.A * center.x +
				plane.B * center.y +
				plane.C * center.z +
				plane.D;

			//================================================
			// カメラの外側へ完全に出ている
			//================================================
			if (distance < -radius)
			{
				return false;
			}
		}

		//====================================================
		// 少なくともFrustumに接している
		//====================================================
		return true;
	}
}


ModelBase::ModelBase(const WCHAR* folder, const WCHAR* file) : ObjectBase()
{
	HRESULT hr;

	pModelData = NULL;
	pMaterials = NULL;
	pVertexBuffer = NULL;
	pIndexBuffer = NULL;
	bones = NULL;
	bonesBuff = NULL;

	//vnmファイルのロード
	WCHAR path[256];
	swprintf_s(path, L"%s%s", folder, file);

	const std::wstring geometryCacheKey(path);

	//============================================================
	// VNMの共有データを取得
	//============================================================
	mSharedModelBinary = AcquireSharedVnmBinary(std::filesystem::path(path));

	//============================================================
	// 読み込み失敗確認
	//============================================================
	if (mSharedModelBinary == nullptr ||mSharedModelBinary->Bytes == nullptr)
	{
#if defined(_DEBUG)
		OutputDebugStringW(L"[vnModel] Shared VNM acquisition failed.\n");
#endif

		pModelData = nullptr;
		return;
	}

	//============================================================
	// 既存のpModelDataへ共有データの先頭を設定
	// ※このポインタは所有権を持たない
	//============================================================
	pModelData =reinterpret_cast<vnModelData*>(mSharedModelBinary->Bytes.get());

	//============================================================
	// ファイルサイズ
	// 既存コンストラクタの後続処理でsizeを使用する場合
	// 共有データから取得する
	//============================================================
	const size_t size = mSharedModelBinary->Size;

	//バージョンチェック
	if (pModelData->Version != vnMODEL_DATA_V1_VER &&pModelData->Version != vnMODEL_DATA_V2_VER &&pModelData->Version != vnMODEL_DATA_V3_VER)
	{
		WCHAR text[256] = {};

		swprintf_s(
			text,
			L"unsupported VNM version : \"%s\"\nVersion = 0x%08X",
			path,
			pModelData->Version
		);

		vnFont::output(text);

		MessageBox(
			hWnd,
			text,
			L"vnModel Construct Error",
			MB_OK
		);

		assert(false);

		return;
	}

	//============================================================
	// VNM Format判定
	//============================================================

	const bool isV1 =
		pModelData->Version == vnMODEL_DATA_V1_VER;

	const bool isV2 =
		pModelData->Version == vnMODEL_DATA_V2_VER;

	const bool isV3 =
		pModelData->Version == vnMODEL_DATA_V3_VER;

	//============================================================
	// Material Binary Data
	// ※V1 / V2でMaterialの1要素サイズが違うため、
	// ※両方のPointerを用意してVersionによって使い分ける
	//============================================================

	const BYTE* materialBase =
		reinterpret_cast<const BYTE*>(pModelData) + pModelData->MaterialAccess;


	//============================================================
	// V1 Material
	// Version == V1の場合のみ使用する
	//============================================================
	const vnModel_MaterialData* pMaterialDataV1 =
		reinterpret_cast<const vnModel_MaterialData*>(materialBase);


	//============================================================
	// V2 Material
	// Version == V2の場合のみ使用する
	//============================================================
	const vnModel_MaterialDataV2* pMaterialDataV2 =
		reinterpret_cast<const vnModel_MaterialDataV2*>(materialBase);


	//============================================================
	// V3 Material
	// Version == V3の場合のみ使用する
	//============================================================
	const vnModel_MaterialDataV3* pMaterialDataV3 =
		reinterpret_cast<const vnModel_MaterialDataV3*>(materialBase);

	//============================================================
	// Runtime Material
	//============================================================
	pMaterials = new stMaterial[pModelData->MaterialNum];

	mSharedBaseColorTextures.resize(pModelData->MaterialNum);
	mSharedNormalTextures.resize(pModelData->MaterialNum);
	mSharedORMTextures.resize(pModelData->MaterialNum);
	mSharedEmissiveTextures.resize(pModelData->MaterialNum);
	mSharedThicknessTextures.resize(pModelData->MaterialNum);
	mSharedSkinMaskTextures.resize(pModelData->MaterialNum);

	for (int i = 0; i < (int)pModelData->MaterialNum; i++)
	{
		//============================================================
		// V1 / V2共通で使うTexture Name
		//============================================================

		const WCHAR* baseColorTexture =nullptr;
		const WCHAR* normalTexture =nullptr;
		const WCHAR* ormTexture =nullptr;
		const WCHAR* thicknessTexture =nullptr;
		const WCHAR* emissiveTexture =nullptr;
		const WCHAR* skinMaskTexture = nullptr;
		//============================================================
		// V3 Material
		//============================================================
		if (isV3)
		{
			const vnModel_MaterialDataV3& src = pMaterialDataV3[i];
			baseColorTexture = src.BaseColorTexture;
			normalTexture = src.NormalTexture;
			ormTexture = src.ORMTexture;
			thicknessTexture = src.ThicknessTexture;
			emissiveTexture = src.EmissiveTexture;
			skinMaskTexture = src.SkinMaskTexture;
		}

		//============================================================
		// V2 Material
		//============================================================
		else if (isV2)
		{
			const vnModel_MaterialDataV2& src = pMaterialDataV2[i];
			baseColorTexture = src.BaseColorTexture;
			normalTexture = src.NormalTexture;
			ormTexture = src.ORMTexture;
			thicknessTexture = src.ThicknessTexture;
			emissiveTexture = src.EmissiveTexture;
		}

		//============================================================
		// Legacy V1
		//============================================================
		else
		{
			const vnModel_MaterialData& src = pMaterialDataV1[i];
			baseColorTexture = src.Texture;
		}
		TexMetadata metadata = {};
		ScratchImage scratchImg = {};

		pMaterials[i].texbuff = NULL;
		pMaterials[i].normalTexBuff = NULL;
		pMaterials[i].ormTexBuff = NULL;
		pMaterials[i].emissiveTexBuff.Reset();
		pMaterials[i].thicknessTexBuff.Reset();
		pMaterials[i].basicDescHeap = NULL;
		pMaterials[i].skinMaskTexBuff.Reset();
		// x = 0.0 : 無効
		// y = 1.0 : 強度
		// z = 1.0 : Y反転なし
		pMaterials[i].NormalParam = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);// Normal Map初期値

		//============================================================
		// PBR初期値
		// 既存モデルを壊さないため、
		// PBR使用フラグは最初は0にする
		//============================================================
		pMaterials[i].PBRParam = XMVectorSet(
			0.0f, // Metallic
			0.5f, // Roughness
			1.0f, // AO
			0.0f  // Use PBR
		);

		//============================================================
		// ORM初期値
		// x = 0: ORM Texture未使用
		// yzw = 1: 各Channelをそのまま使用する倍率
		//============================================================
		pMaterials[i].ORMParam =
			XMVectorSet(
				0.0f, // Use ORM
				1.0f, // AO Scale
				1.0f, // Roughness Scale
				1.0f  // Metallic Scale
			);

		//============================================================
		// Environment Reflection初期値
		// x = 0: 最初は無効
		// y = 1: Reflection強度
		// z: Load後にCubeMapのMip数から設定する
		//============================================================
		pMaterials[i].EnvironmentParam =
			XMVectorSet(
				0.0f, // Use Environment Reflection
				1.0f, // Reflection Intensity
				0.0f, // Maximum Mip Level
				0.0f
			);

		//============================================================
		// Emissive初期値
		// 発光なし
		//============================================================
		pMaterials[i].EmissiveParam = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

		//============================================================
		// Subsurface Scattering初期値
		//============================================================
		pMaterials[i].SSSParam =
			XMVectorSet(
				0.0f,	// x : Enable
				0.5f,	// y : Strength
				0.5f,	// z : Thickness Mask
				0.25f	// w : Distortion
			);


		//============================================================
		// Subsurface Color
		//============================================================
		pMaterials[i].SSSColor =
			XMVectorSet(
				1.00f,	// R
				0.25f,	// G
				0.12f,	// B
				4.00f	// Transmission Power
			);

		//============================================================
		// Thickness Map Parameter
		//============================================================
		pMaterials[i].SSSMapParam =
			XMVectorSet(
				0.0f,// x = 0 : Map無効
				1.0f,// y = 1 : Scale
				0.0f,// z = 0 : Invertなし
				0.0f// w = 0 : unused
			);


		//============================================================
		// Screen-Space SSS Material Profile
		// 全成分0: Global Profile / Radiusを使う
		//============================================================
		pMaterials[i].SSSDiffusionParam =
			XMVectorSet(
				0.0f,
				0.0f,
				0.0f,
				0.0f
			);
		//============================================================
		// V3 Material Parameters
		//============================================================
		if (isV3)
		{
			const vnModel_MaterialDataV3& src =
				pMaterialDataV3[i];


			//========================================================
			// Legacy Material
			//========================================================
			pMaterials[i].Diffuse =
				XMVectorSet(
					src.Diffuse[0],
					src.Diffuse[1],
					src.Diffuse[2],
					src.Diffuse[3]
				);


			pMaterials[i].Ambient =
				XMVectorSet(
					src.Ambient[0],
					src.Ambient[1],
					src.Ambient[2],
					src.Ambient[3]
				);


			pMaterials[i].Specular =
				XMVectorSet(
					src.Specular[0],
					src.Specular[1],
					src.Specular[2],
					src.Specular[3]
				);


			pMaterials[i].TilingOffset =
				XMVectorSet(
					src.TilingOffset[0],
					src.TilingOffset[1],
					src.TilingOffset[2],
					src.TilingOffset[3]
				);


			//========================================================
			// Normal
			//========================================================
			pMaterials[i].NormalParam =
				XMVectorSet(
					src.NormalParam[0],
					src.NormalParam[1],
					src.NormalParam[2],
					src.NormalParam[3]
				);


			//========================================================
			// PBR
			//========================================================
			pMaterials[i].PBRParam =
				XMVectorSet(
					src.PBRParam[0],
					src.PBRParam[1],
					src.PBRParam[2],
					src.PBRParam[3]
				);


			//========================================================
			// ORM
			//========================================================
			pMaterials[i].ORMParam =
				XMVectorSet(
					src.ORMParam[0],
					src.ORMParam[1],
					src.ORMParam[2],
					src.ORMParam[3]
				);


			//========================================================
			// Emissive
			//========================================================
			pMaterials[i].EmissiveParam =
				XMVectorSet(
					src.EmissiveParam[0],
					src.EmissiveParam[1],
					src.EmissiveParam[2],
					src.EmissiveParam[3]
				);


			//========================================================
			// Environment
			//========================================================
			pMaterials[i].EnvironmentParam =
				XMVectorSet(
					src.EnvironmentParam[0],
					src.EnvironmentParam[1],
					src.EnvironmentParam[2],
					src.EnvironmentParam[3]
				);


			//========================================================
			// SSS
			//========================================================
			pMaterials[i].SSSParam =
				XMVectorSet(
					src.SSSParam[0],
					src.SSSParam[1],
					src.SSSParam[2],
					src.SSSParam[3]
				);


			pMaterials[i].SSSColor =
				XMVectorSet(
					src.SSSColor[0],
					src.SSSColor[1],
					src.SSSColor[2],
					src.SSSColor[3]
				);


			//========================================================
			// SSS Maps
			//
			// x = Thickness Map
			// y = Thickness Scale
			// z = Thickness Invert
			// w = SkinMask Enable
			//========================================================
			pMaterials[i].SSSMapParam =
				XMVectorSet(
					src.SSSMapParam[0],
					src.SSSMapParam[1],
					src.SSSMapParam[2],
					src.SSSMapParam[3]
				);


			//========================================================
			// Screen-Space Diffusion
			//========================================================
			pMaterials[i].SSSDiffusionParam =
				XMVectorSet(
					src.SSSDiffusionParam[0],
					src.SSSDiffusionParam[1],
					src.SSSDiffusionParam[2],
					src.SSSDiffusionParam[3]
				);
		}
		else if (isV2)
		{
			//========================================================
			// V2
			//========================================================
			const vnModel_MaterialDataV2& src = pMaterialDataV2[i];


			//========================================================
			// Legacy Material Parameter
			//========================================================
			pMaterials[i].Diffuse =
				XMVectorSet(
					src.Diffuse[0],
					src.Diffuse[1],
					src.Diffuse[2],
					src.Diffuse[3]
				);


			pMaterials[i].Ambient =
				XMVectorSet(
					src.Ambient[0],
					src.Ambient[1],
					src.Ambient[2],
					src.Ambient[3]
				);


			pMaterials[i].Specular =
				XMVectorSet(
					src.Specular[0],
					src.Specular[1],
					src.Specular[2],
					src.Specular[3]
				);


			pMaterials[i].TilingOffset =
				XMVectorSet(
					src.TilingOffset[0],
					src.TilingOffset[1],
					src.TilingOffset[2],
					src.TilingOffset[3]
				);


			//========================================================
			// Normal Map
			//
			// x = Enabled
			// y = Strength
			// z = Y Sign
			// w = unused
			//========================================================
			pMaterials[i].NormalParam =
				XMVectorSet(
					src.NormalParam[0],
					src.NormalParam[1],
					src.NormalParam[2],
					src.NormalParam[3]
				);


			//========================================================
			// PBR
			//
			// x = Metallic
			// y = Roughness
			// z = AO
			// w = Enabled
			//========================================================
			pMaterials[i].PBRParam =
				XMVectorSet(
					src.PBRParam[0],
					src.PBRParam[1],
					src.PBRParam[2],
					src.PBRParam[3]
				);


			//========================================================
			// ORM
			//========================================================
			pMaterials[i].ORMParam =
				XMVectorSet(
					src.ORMParam[0],
					src.ORMParam[1],
					src.ORMParam[2],
					src.ORMParam[3]
				);


			//========================================================
			// Emissive
			//========================================================
			pMaterials[i].EmissiveParam =
				XMVectorSet(
					src.EmissiveParam[0],
					src.EmissiveParam[1],
					src.EmissiveParam[2],
					src.EmissiveParam[3]
				);


			//========================================================
			// Environment
			//========================================================
			pMaterials[i].EnvironmentParam =
				XMVectorSet(
					src.EnvironmentParam[0],
					src.EnvironmentParam[1],
					src.EnvironmentParam[2],
					src.EnvironmentParam[3]
				);


			//========================================================
			// SSS
			//========================================================
			pMaterials[i].SSSParam =
				XMVectorSet(
					src.SSSParam[0],
					src.SSSParam[1],
					src.SSSParam[2],
					src.SSSParam[3]
				);


			pMaterials[i].SSSColor =
				XMVectorSet(
					src.SSSColor[0],
					src.SSSColor[1],
					src.SSSColor[2],
					src.SSSColor[3]
				);


			//========================================================
			// Thickness Map
			//========================================================
			pMaterials[i].SSSMapParam =
				XMVectorSet(
					src.SSSMapParam[0],
					src.SSSMapParam[1],
					src.SSSMapParam[2],
					src.SSSMapParam[3]
				);


			//========================================================
			// SSS Diffusion Profile
			//========================================================
			pMaterials[i].SSSDiffusionParam =
				XMVectorSet(
					src.SSSDiffusionParam[0],
					src.SSSDiffusionParam[1],
					src.SSSDiffusionParam[2],
					src.SSSDiffusionParam[3]
				);
		}
		else
		{
			//========================================================
			// V1
			// Unity VNMのMaterial Parameterをそのまま使用する。
			// PBR / Normal / ORM / SSSなどは先ほど設定した
			// Runtime Defaultを維持する
			//========================================================
			const vnModel_MaterialData& src =
				pMaterialDataV1[i];


			pMaterials[i].Diffuse =
				XMVectorSet(
					src.Diffuse[0],
					src.Diffuse[1],
					src.Diffuse[2],
					src.Diffuse[3]
				);


			pMaterials[i].Ambient =
				XMVectorSet(
					src.Ambient[0],
					src.Ambient[1],
					src.Ambient[2],
					src.Ambient[3]
				);


			pMaterials[i].Specular =
				XMVectorSet(
					src.Specular[0],
					src.Specular[1],
					src.Specular[2],
					src.Specular[3]
				);


			pMaterials[i].TilingOffset =
				XMVectorSet(
					src.TilingOffset[0],
					src.TilingOffset[1],
					src.TilingOffset[2],
					src.TilingOffset[3]
				);
		}


		//============================================================
		// BaseColor Texture
		//============================================================
		if (baseColorTexture != nullptr &&baseColorTexture[0] != L'\0')
		{
			//========================================================
			// Texture Path
			//========================================================
			swprintf_s(
				path,
				L"%s%s",
				folder,
				baseColorTexture
			);

			//========================================================
			// Cacheから共有Textureを取得
			// Cache Hit: 既存のTextureを返す
			// Cache Miss: Textureの読み込み・GPU生成を実行
			//========================================================
			auto sharedTexture =
				AcquireSharedBaseColorTexture(
					path,
					baseColorTexture
				);

			if (sharedTexture)
			{
				//====================================================
				// このMaterialがTextureを使用し続けるため
				// shared_ptrを保持する
				// ※weak_ptrを使用しているCacheが途中で失効しないために必要
				//====================================================
				mSharedBaseColorTextures[i] =
					sharedTexture;

				//====================================================
				// 既存のstMaterialへResourceを渡す
				// texbuffはID3D12Resource*なので、
				// 共有Resourceの生ポインタを設定する
				//====================================================
				pMaterials[i].texbuff =sharedTexture->Texture.Get();

				//====================================================
				// COM参照を1つ取得
				// 既存のデストラクタはtexbuffを
				// SAFE_RELEASEするため、
				// Materialが所有するCOM参照を追加する
				//====================================================
				pMaterials[i].texbuff->AddRef();

				//====================================================
				// 元のDescriptor生成処理で使用する
				// Texture FormatとMip数を復元する
				// Cache Hit時にはDDSを再読み込みしないため、
				// この情報を共有Resourceから取得する
				//====================================================
				metadata.format =sharedTexture->Format;
				metadata.mipLevels =sharedTexture->MipLevels;
			}
			else
			{
				//====================================================
				// 読み込み失敗
				// texbuffはnullptrのまま
				//====================================================
				OutputDebugStringW(L"[vnModel] BaseColor Cache Acquire Failed.\n");
			}
		}


		{
			D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			descHeapDesc.NodeMask = 0;
			descHeapDesc.NumDescriptors = MaterialDescriptor::Count;
			descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			hr = Direct3D::getDevice()->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&pMaterials[i].basicDescHeap));
			assert(SUCCEEDED(hr));
			//定数バッファ
			D3D12_HEAP_PROPERTIES constHeapProp = {};
			constHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
			constHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			constHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			constHeapProp.CreationNodeMask = 1;
			constHeapProp.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC constDesc = {};
			constDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			constDesc.Width = ((sizeof(stMaterialBuffer) + 0xff) & ~0xff);
			constDesc.Height = 1;
			constDesc.DepthOrArraySize = 1;
			constDesc.MipLevels = 1;
			constDesc.Format = DXGI_FORMAT_UNKNOWN;
			constDesc.SampleDesc.Count = 1;
			constDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			constDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			hr = Direct3D::getDevice()->CreateCommittedResource(
				&constHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&constDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pMaterials[i].materialBuff)
			);

			if (hr != S_OK)
			{
				assert(hr == S_OK);
			}
			pMaterials[i].materialBuff->SetName(L"vnModel::materialBuff");

			//============================================================
			// DescriptorHeapに入れるDescriptorのサイズを取得
			//============================================================
			ID3D12Device* device = Direct3D::getDevice();
			assert(device != nullptr);
			const UINT descriptorSize =
				device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			//============================================================
			// 指定番号のCPU Descriptor Handleを取得する補助関数
			//============================================================
			auto GetCPUHandle = [&, descriptorSize](UINT descriptorIndex)-> D3D12_CPU_DESCRIPTOR_HANDLE
				{
					D3D12_CPU_DESCRIPTOR_HANDLE handle =pMaterials[i].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

					handle.ptr +=
						static_cast<SIZE_T>(descriptorIndex) *
						static_cast<SIZE_T>(descriptorSize);

					return handle;
				};

			//============================================================
			// [0] Diffuse Texture SRV
			// HLSL: Texture2D txDiffuse : register(t0);
			//============================================================
			D3D12_CPU_DESCRIPTOR_HANDLE diffuseHandle = GetCPUHandle(MaterialDescriptor::Diffuse);

			if (pMaterials[i].texbuff != nullptr)
			{
				//========================================================
				// モデルにテクスチャがある場合は、そのテクスチャをSRV化
				//========================================================
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format = metadata.format;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

				Direct3D::getDevice()->CreateShaderResourceView(
						pMaterials[i].texbuff,
						&srvDesc,
						diffuseHandle
					);
			}
			else
			{
				//========================================================
				// テクスチャがない場合は白テクスチャを入れる
				//========================================================
				Direct3D::getDevice()->CreateShaderResourceView(
					Direct3D::getWhiteTexture(),
					Direct3D::getWhiteTextueViewDesc(),
					diffuseHandle
				);
			}



			//============================================================
			// ShadowMap用Descriptorの初期化
			// ShadowMapが登録されていないSceneでも、
			// 未初期化DescriptorをShaderが参照しないようにする
			//============================================================
			const UINT shadowDescriptorIndices[] =
			{
				MaterialDescriptor::StaticShadow0,
				MaterialDescriptor::StaticShadow1,
				MaterialDescriptor::StaticShadow2,

				MaterialDescriptor::DynamicShadow0,
				MaterialDescriptor::DynamicShadow1,
				MaterialDescriptor::DynamicShadow2
			};

			for (UINT shadowIndex = 0; shadowIndex < _countof(shadowDescriptorIndices); ++shadowIndex)
			{
				const UINT descriptorIndex = shadowDescriptorIndices[shadowIndex];
				const D3D12_CPU_DESCRIPTOR_HANDLE shadowHandle = GetCPUHandle(descriptorIndex);
				device->CreateShaderResourceView(
					Direct3D::getWhiteTexture(),
					Direct3D::getWhiteTextueViewDesc(),
					shadowHandle
				);
			}

			//============================================================
			// [8] t7 : Normal Map
			// Normal Mapが設定されていない段階では
			// 白Textureをダミーとして登録する
			// Shader側ではNormalParam.xが0の場合にSample結果を使用しない
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE normalHandle =
				GetCPUHandle(MaterialDescriptor::Normal);

			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				normalHandle
			);

			//============================================================
			// [9] t8 : ORM Map
			// ORM Mapが未設定でも、Shaderから安全にSampleできるように
			// 白Textureをダミーとして登録する
			// 白Texture:
			//   R = 1.0 AO
			//   G = 1.0 Roughness
			//   B = 1.0 Metallic
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE ormHandle =
				GetCPUHandle(MaterialDescriptor::ORM);

			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				ormHandle
			);

			//============================================================
			// [10] t9 : Environment CubeMap
			// EnvironmentMapManagerが保持しているCubeMapから
			// このMaterial HeapへTextureCube SRVを直接作成する
			// Environment未ロード時はNull TextureCube SRVになる
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE environmentHandle =
				GetCPUHandle(MaterialDescriptor::Environment);

			EnvironmentMapManager::WriteEnvironmentSRV(environmentHandle);

			//============================================================
			// [11] t10 : Prefiltered Environment CubeMap
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE prefilteredEnvironmentHandle =
				GetCPUHandle(MaterialDescriptor::PrefilteredEnvironment);

			EnvironmentMapManager::WritePrefilteredEnvironmentSRV(prefilteredEnvironmentHandle);

			//============================================================
			// [12] t11 : Irradiance Environment CubeMap
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE irradianceHandle =
				GetCPUHandle(MaterialDescriptor::IrradianceEnvironment);

			EnvironmentMapManager::WriteIrradianceEnvironmentSRV(irradianceHandle);

			//============================================================
			// [13] t12 : BRDF LUT
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE brdfLUTHandle =
				GetCPUHandle(MaterialDescriptor::BRDFLUT);

			EnvironmentMapManager::WriteBRDFLUTSRV(brdfLUTHandle);

			//============================================================
			// [14] t13 : Local Light StructuredBuffer
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE localLightHandle =
				GetCPUHandle(MaterialDescriptor::LocalLightBuffer);

			LocalLightManager::WriteSRV(localLightHandle);

			//============================================================
			// [15] t14 : Spot Light Shadow Map
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE spotShadowHandle =
				GetCPUHandle(MaterialDescriptor::SpotLightShadow);

			SpotLightShadowManager::WriteSRV(spotShadowHandle);

			//============================================================
			// [16] t15 : Point Light Shadow Map
			//============================================================

			const D3D12_CPU_DESCRIPTOR_HANDLE pointShadowHandle =
				GetCPUHandle(MaterialDescriptor::PointLightShadow);

			PointLightShadowManager::WriteSRV(pointShadowHandle);

			//============================================================
			// [17] t16 : SSS Thickness Map
			// Thickness Map未設定時でもDescriptorを有効にしておく
			// Map使用フラグはSSSMapParam.x = 0なので
			// 通常はこのWhite Textureの値を使用しない
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE thicknessHandle =
				GetCPUHandle(MaterialDescriptor::Thickness);

			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				thicknessHandle
			);

			//============================================================
			// [18] t17 : Emissive Map
			// 実際にTextureを使用するかどうかは
			// EmissiveParam.wで判定する
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE emissiveHandle =
				GetCPUHandle(MaterialDescriptor::Emissive);


			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				emissiveHandle
			);

			//============================================================
			// [19] t18 : SSS Skin Mask
			//============================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE skinMaskHandle =
				GetCPUHandle(MaterialDescriptor::SkinMask);

			device->CreateShaderResourceView(
				Direct3D::getWhiteTexture(),
				Direct3D::getWhiteTextueViewDesc(),
				skinMaskHandle
			);

			//============================================================
			// [4] MaterialBuffer CBV
			// HLSL: cbuffer MaterialBuffer : register(b1)
			//============================================================
			D3D12_CPU_DESCRIPTOR_HANDLE materialHandle =
				GetCPUHandle(MaterialDescriptor::Material);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

			cbvDesc.BufferLocation = pMaterials[i].materialBuff->GetGPUVirtualAddress();

			cbvDesc.SizeInBytes =
				static_cast<UINT>(pMaterials[i].materialBuff->GetDesc().Width);

			Direct3D::getDevice()->CreateConstantBufferView(
				&cbvDesc,
				materialHandle
			);

			hr = pMaterials[i].materialBuff->Map(0, NULL, (void**)&pMaterials[i].pMaterialBuffer);
		}

		//============================================================
		// VNM V2 / V3 Additional Texture Auto Load		
		//============================================================
		if (isV2 || isV3)
		{
			//========================================================
			// Normal Map
			//========================================================
			if (normalTexture != nullptr &&normalTexture[0] != L'\0')
			{
				const bool loaded =
					LoadNormalMap(
						folder,
						normalTexture,
						i
					);


				if (!loaded)
				{
					OutputDebugStringW(L"[vnModel] Normal Map load failed.\n");
				}
			}


			//========================================================
			// ORM
			//========================================================
			if (ormTexture != nullptr &&ormTexture[0] != L'\0')
			{
				const bool loaded =LoadORMMap(
						folder,
						ormTexture,
						i
					);


				if (!loaded)
				{
					OutputDebugStringW(L"[vnModel] ORM Map load failed.\n");
				}
			}


			//========================================================
			// Thickness
			//========================================================
			if (thicknessTexture != nullptr &&thicknessTexture[0] != L'\0')
			{
				const bool loaded = LoadSubsurfaceThicknessMap(
					folder,
					thicknessTexture,
					i
				);


				if (!loaded)
				{
					OutputDebugStringW(L"[vnModel] Thickness Map load failed.\n");
				}
			}

			//============================================================
			// Emissive Map
			// VNM V2にEmissive Texture名が存在する場合
			// Descriptor Heap作成後に自動ロードする
			//============================================================
			if (emissiveTexture != nullptr &&emissiveTexture[0] != L'\0')
			{
				const bool loaded =
					LoadEmissiveMap(
						folder,
						emissiveTexture,
						i
					);


				if (!loaded)
				{
					OutputDebugStringW(L"[vnModel] Emissive Map load failed.\n");

					//====================================================
					// Texture使用だけOFFにする
					// xyzのEmissive Colorは残せるので、
					// wのみ0へ戻す
					//====================================================
					pMaterials[i].EmissiveParam =
						XMVectorSetW(
							pMaterials[i].EmissiveParam,
							0.0f
						);
				}
			}

			if (isV3 &&skinMaskTexture != nullptr &&skinMaskTexture[0] != L'\0')
			{
				const bool loaded =
					LoadSubsurfaceSkinMask(
						folder,
						skinMaskTexture,
						i
					);


				if (!loaded)
				{
					OutputDebugStringW(L"[vnModel] V3 SkinMask load failed.\n");

					//================================================
					// Load失敗時:
					// SkinMask使用をOFFに戻す
					// DescriptorにはWhite Textureが入っているので
					// 未初期化Descriptorにはならない
					//================================================
					pMaterials[i].SSSMapParam =
						XMVectorSetW(
							pMaterials[i].SSSMapParam,
							0.0f
						);
				}
			}

		}


	}


	//============================================================
	// Geometry Cache Lookup
	//============================================================
	const bool canShareGeometry =pModelData->BoneNum == 0;

	//============================================================
	// すでに同じVNMからGeometryが生成されているか検索
	//============================================================
	if (canShareGeometry)
	{
		mSharedGeometry = FindCachedGeometry(geometryCacheKey);
	}

	//============================================================
	// Cache Hit
	// 既存のVertex / Index Bufferを共有する
	//============================================================
	if (mSharedGeometry)
	{
		//========================================================
		// 共有GPU Resourceを取得する
		//========================================================
		pVertexBuffer = mSharedGeometry->VertexBuffer.Get();
		pIndexBuffer = mSharedGeometry->IndexBuffer.Get();

		assert(pVertexBuffer != nullptr);
		assert(pIndexBuffer != nullptr);

		//========================================================
		// COM参照カウントを追加
		// 重要: 既存のvnModelデストラクタではSAFE_RELEASE()を行う
		//========================================================
		pVertexBuffer->AddRef();
		pIndexBuffer->AddRef();
		vertexBufferView = mSharedGeometry->VertexView;
		indexBufferView = mSharedGeometry->IndexView;

#if defined(_DEBUG)
		//========================================================
		// Cache Hit確認用
		// 大量モデル生成時にログを出しすぎないよう、
		// 最初と50個ごとに表示する
		//========================================================
		static unsigned int cacheHitCount = 0;
		++cacheHitCount;

		if (cacheHitCount == 1 || cacheHitCount % 50 == 0)
		{
			wchar_t buffer[256] = {};

			swprintf_s(
				buffer,
				L"[GeometryCache] HIT Count=%u, Users=%lu\n",
				cacheHitCount,
				mSharedGeometry.use_count()
			);

			OutputDebugStringW(buffer);
		}
#endif
	}

	else
	{
		{
			//========================================================
			// スキニングモデルか判定
			//========================================================
			const bool isSkinnedModel =pModelData->BoneNum > 0;

			const void* uploadVertexData = nullptr;

			UINT vertexStride = 0;
			UINT vertexBufferSize = 0;

			//========================================================
			// CPU上で頂点データを準備
			//========================================================
			if (!isSkinnedModel)
			{
				//====================================================
				// 通常モデル
				//====================================================
				if (!BuildGpuVerticesWithTangents())
				{
					return;
				}

				if (mGpuVertices.empty())
				{
					return;
				}

				vertexStride =static_cast<UINT>(sizeof(vnVertex3D));

				vertexBufferSize =vertexStride *static_cast<UINT>(mGpuVertices.size());

				uploadVertexData =mGpuVertices.data();
			}
			else
			{
				//====================================================
				// スキニングモデル
				// SkinIndexとSkinWeightを保持する
				//====================================================
				if (!BuildGpuSkinnedVerticesWithTangents())
				{
					return;
				}

				if (mGpuSkinVertices.empty())
				{
					return;
				}

				vertexStride =static_cast<UINT>(sizeof(vnVertex3DSkin));

				vertexBufferSize =vertexStride *static_cast<UINT>(mGpuSkinVertices.size());

				uploadVertexData =mGpuSkinVertices.data();
			}

			//========================================================
			// Index Format
			// VertexNum <= 65535 : R16_UINT
			// VertexNum >  65535 : R32_UINT
			//========================================================
			const bool use16BitIndex = pModelData->VertexNum <= 0xffff;

			const UINT indexStride = use16BitIndex ? sizeof(WORD) : sizeof(DWORD);

			const UINT64 indexBufferSize64 =
				static_cast<UINT64>(indexStride) *static_cast<UINT64>(pModelData->IndexNum);

			//========================================================
			// VNM内のIndex Dataへのポインタ
			// VNMファイル全体はpModelDataに展開済み
			// IndexAccessが先頭からのByte Offset
			//========================================================
			const BYTE* indexData = reinterpret_cast<const BYTE*>(pModelData) + pModelData->IndexAccess;

			//========================================================
			// バッファサイズ検証
			//========================================================
			if (uploadVertexData == nullptr ||
				indexData == nullptr ||
				vertexBufferSize == 0 ||
				indexBufferSize64 == 0 ||
				indexBufferSize64 >
				(std::numeric_limits<UINT>::max)())
			{
				OutputDebugStringA("[vnModel] Invalid geometry buffer size.\n");

				return;
			}

			const UINT indexBufferSize =static_cast<UINT>(indexBufferSize64);

			//========================================================
			// DEFAULT Heapへ転送
			// この関数内で
			// ・GPU Resource作成
			// ・一時Upload Buffer作成
			// ・GPUコピー
			// ・Barrier
			// ・Fence待機
			// まで実行する。
			//========================================================
			const bool uploadSuccess =
				UploadStaticGeometryBuffers(
					uploadVertexData,
					vertexBufferSize,
					indexData,
					indexBufferSize,
					&pVertexBuffer,
					&pIndexBuffer
				);

			if (!uploadSuccess)
			{
				OutputDebugStringA("[vnModel] Geometry GPU Upload failed.\n");

				return;
			}

			//========================================================
			// 7. Resource名
			//========================================================
			if (isSkinnedModel)
			{
				pVertexBuffer->SetName(L"vnModel::SkinVertexBuffer_DEFAULT");
			}
			else
			{
				pVertexBuffer->SetName(L"vnModel::TangentVertexBuffer_DEFAULT");
			}

			pIndexBuffer->SetName(L"vnModel::IndexBuffer_DEFAULT");

			//========================================================
			// Vertex Buffer View
			// GPU Virtual AddressはDEFAULT HeapのBufferを参照
			//========================================================
			vertexBufferView = {};
			vertexBufferView.BufferLocation =pVertexBuffer->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes =vertexStride;
			vertexBufferView.SizeInBytes =vertexBufferSize;

			//========================================================
			// Index Buffer View
			//========================================================
			indexBufferView = {};
			indexBufferView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
			indexBufferView.Format =use16BitIndex? DXGI_FORMAT_R16_UINT: DXGI_FORMAT_R32_UINT;
			indexBufferView.SizeInBytes =indexBufferSize;
		}
		//============================================================
		// Geometry Cache Register
		// 既存のVertex / Index Buffer生成が成功した場合
		// そのGPU Resourceを共有キャッシュへ登録する
		//============================================================
		if (canShareGeometry &&pVertexBuffer != nullptr &&pIndexBuffer != nullptr)
		{
			//========================================================
			// 共有Geometry Resource生成
			//========================================================
			auto resource =
				std::make_shared<SharedGeometryResource>();

			//========================================================
			// ComPtrへGPU Resourceを保持させる
			// raw pointerからComPtrへの代入はAddRefを行う
			// したがって所有関係は次のようになる
			// 1. vnModelのraw pointerがCOM参照を1つ所有
			// 2. 共有ResourceのComPtrがCOM参照を1つ所有
			//========================================================
			resource->VertexBuffer = pVertexBuffer;
			resource->IndexBuffer = pIndexBuffer;

			//========================================================
			// Buffer View保存
			//========================================================
			resource->VertexView = vertexBufferView;
			resource->IndexView = indexBufferView;

			//========================================================
			// このvnModelが共有Resourceを所有する
			//========================================================
			mSharedGeometry = resource;

			//========================================================
			// Cache登録
			// 次回、同じVNMからモデルが生成される場合は
			// このGeometryを再利用する
			//========================================================
			RegisterCachedGeometry(
				geometryCacheKey,
				resource
			);

#if defined(_DEBUG)
			OutputDebugStringW(L"[GeometryCache] MISS - Created shared geometry.\n");
#endif
		}
	}

	{	//コンスタントバッファ(マトリクス)

		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 1;
		heapProp.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = ((sizeof(stConstantBuffer) + 0xff) & ~0xff);
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.Format = DXGI_FORMAT_UNKNOWN;
		resDesc.SampleDesc.Count = 1;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		hr = Direct3D::getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constBuff)
		);
		assert(hr == S_OK);
		constBuff->SetName(L"vnModel::constBuff");

		hr = constBuff->Map(0, NULL, (void**)&pConstBuffer);
		assert(hr == S_OK);
	}

	pSkinMatrix = nullptr;

	mPreviousSkinMatrix = nullptr;


	if (pModelData->BoneNum > 0)
	{
		setRenderFlag(eRenderFlag::Skin, true);


		//============================================================
		// Current / Previous Skin MatrixをCPU側に確保
		//============================================================
		pSkinMatrix = new XMMATRIX[pModelData->BoneNum];

		mPreviousSkinMatrix = new XMMATRIX[pModelData->BoneNum];


		//============================================================
		// 初期状態
		//============================================================
		for (UINT boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
		{
			pSkinMatrix[boneIndex] = XMMatrixIdentity();
			mPreviousSkinMatrix[boneIndex] = XMMatrixIdentity();
		}


		mSkinMotionHistoryValid = false;


		//============================================================
		// Bone Constant Buffer 1個分のサイズ
		//
		// Current / Previousとも同じサイズ。
		//============================================================
		const UINT boneBufferSize = (sizeof(XMFLOAT4X4) * pModelData->BoneNum + 0xff) & ~0xff;


		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProp.CreationNodeMask = 1;
		heapProp.VisibleNodeMask = 1;


		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = boneBufferSize;
		resDesc.Height = 1;
		resDesc.DepthOrArraySize = 1;
		resDesc.MipLevels = 1;
		resDesc.SampleDesc.Count = 1;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


		//============================================================
		// Current Bone Buffer
		//============================================================
		HRESULT hr = Direct3D::getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&bonesBuff)
		);

		assert(SUCCEEDED(hr));

		bonesBuff->SetName(L"vnModel::CurrentBones");

		hr = bonesBuff->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&bones));

		assert(SUCCEEDED(hr));


		//============================================================
		// Previous Bone Buffer
		//============================================================
		hr = Direct3D::getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mPreviousBonesBuff));

		assert(SUCCEEDED(hr));

		mPreviousBonesBuff->SetName(L"vnModel::PreviousBones");

		hr = mPreviousBonesBuff->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mPreviousBones)
		);

		assert(SUCCEEDED(hr));
	}

	//============================================================
	// Shadow Casterカリング用Bounding Sphereを作成
	// pModelDataと頂点データの読み込みが完了した後に
	// 一度だけ実行する
	//============================================================
	BuildShadowBoundingSphere();

	//============================================================
	// CPU頂点配列のメモリ解放
	// DEFAULT Heapへの転送はFence待機済み
	// GPUはこのCPU配列を直接参照しない
	// Shadow Bounding Sphereの生成も完了している
	// ※CPU頂点を使うRaycastやMesh編集を追加するなら
	// ※この解放処理との依存関係を確認する
	//============================================================

	std::vector<vnVertex3D>().swap(mGpuVertices);

	std::vector<vnVertex3DSkin>().swap(mGpuSkinVertices);
}

ModelBase::~ModelBase()
{
	if (pSkinMatrix)delete[] pSkinMatrix;
	// bonesBuff が Map されている場合は Unmap してから解放
	if (bonesBuff)
	{
		if (bones != nullptr)
		{
			bonesBuff->Unmap(0, nullptr);
			bones = nullptr;
		}
		SAFE_RELEASE(bonesBuff);
	}

	if (mPreviousSkinMatrix != nullptr)
	{
		delete[] mPreviousSkinMatrix;
		mPreviousSkinMatrix =nullptr;
	}

	if (mPreviousBonesBuff != nullptr)
	{
		if (mPreviousBones != nullptr)
		{
			mPreviousBonesBuff->Unmap(
				0,
				nullptr
			);

			mPreviousBones = nullptr;
		}
		SAFE_RELEASE(mPreviousBonesBuff);
	}
	// コンスタントバッファ(constBuff)がMapされている場合はUnmap
	if (constBuff)
	{
		if (pConstBuffer != nullptr)
		{
			constBuff->Unmap(0, nullptr);
			pConstBuffer = nullptr;
		}
		SAFE_RELEASE(constBuff);
	}
	SAFE_RELEASE(pIndexBuffer);
	SAFE_RELEASE(pVertexBuffer);
	if (pMaterials != nullptr && pModelData != nullptr)
	{
		for (int i = 0; i < static_cast<int>(pModelData->MaterialNum); ++i)
		{
			SAFE_RELEASE(pMaterials[i].materialBuff);
			SAFE_RELEASE(pMaterials[i].texbuff);
			SAFE_RELEASE(pMaterials[i].normalTexBuff);
			SAFE_RELEASE(pMaterials[i].ormTexBuff);
			SAFE_RELEASE(pMaterials[i].basicDescHeap);
		}
	}
	delete[] pMaterials;
	pMaterials = nullptr;
	pModelData = nullptr;
	mSharedModelBinary.reset();
}

void ModelBase::execute()
{
}

void ModelBase::render()
{
	if (pModelData == nullptr ||
		pVertexBuffer == nullptr ||
		pIndexBuffer == nullptr ||
		pMaterials == nullptr ||
		constBuff == nullptr ||
		pConstBuffer == nullptr)
	{
		return;
	}

	if (!mVisibleInMainCamera)
	{
		return;
	}

	//============================================================
	// 現在のメインカメラから見えないモデルは描画しない
	//============================================================
	if (!IsInsideMainCameraFrustum())
	{
		return;
	}

	//============================================================
	// Skinned VNMが本当にSceneから描画されているか確認
	//============================================================
	if (pModelData != nullptr && pModelData->BoneNum > 0)
	{
		static bool outputOnce = false;

		if (!outputOnce)
		{
			wchar_t debugText[256] = {};


			swprintf_s(
				debugText,
				L"[vnModel::render] "
				L"Skinned Model Render Called. "
				L"Vertex=%u Index=%u Bone=%u Mesh=%u\n",
				pModelData->VertexNum,
				pModelData->IndexNum,
				pModelData->BoneNum,
				pModelData->MeshNum
			);

			OutputDebugStringW(debugText);

			outputOnce = true;
		}
	}

	ObjectBase::render();

	//ビューポート変換マトリクスの計算
	XMMATRIX WVP = pModelData->BoneNum ? (*vnCamera::getScreen()) : XMMatrixMultiply(World, *vnCamera::getScreen());
	XMMATRIX WV = pModelData->BoneNum ? (*vnCamera::getView()) : XMMatrixMultiply(World, *vnCamera::getView());

	//コンスタントバッファに情報を設定
	XMStoreFloat4x4(
		&pConstBuffer->WVP,
		XMMatrixTranspose(WVP)
	);

	XMStoreFloat4x4(
		&pConstBuffer->World,
		XMMatrixTranspose(World)
	);

	XMStoreFloat4x4(
		&pConstBuffer->WorldView,
		XMMatrixTranspose(WV)
	);

	//============================================================
	// transpose(inverse(World))をShaderへ渡す
	//============================================================
	const XMMATRIX worldInvTranspose =
		XMMatrixTranspose(XMMatrixInverse(nullptr, World));

	XMStoreFloat4x4(
		&pConstBuffer->WorldInvTranspose,
		XMMatrixTranspose(worldInvTranspose)
	);

	XMMATRIX previousWVP = XMMatrixIdentity();

	if (!HasMotionHistory())
	{
		previousWVP = WVP;
	}
	else
	{
		if (pModelData->BoneNum > 0)
		{
			previousWVP = PostProcessManager::GetPreviousViewProjection();
		}
		else
		{
			//====================================================
			// Rigid Model
			//====================================================
			previousWVP =
				XMMatrixMultiply(
					PreviousWorld,
					PostProcessManager::GetPreviousViewProjection()
				);
		}
	}

	//============================================================
	// GPUへ転送
	// Shaderではmul(vector, matrix)を使用するためTranspose
	//============================================================
	XMStoreFloat4x4(
		&pConstBuffer->PreviousWVP,
		XMMatrixTranspose(previousWVP)
	);

	if (ShadowMapManager::IsValid())
	{
		//========================================================
		// Boneを持っているかどうか
		//========================================================
		const bool isSkinned = pModelData->BoneNum > 0;

		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			//====================================================
			// CascadeごとのLightViewProjectionを取得
			//====================================================
			const XMMATRIX lightViewProjection =
				ShadowMapManager::GetLightViewProj(cascadeIndex);

			XMMATRIX lightWVP = XMMatrixIdentity();

			if (isSkinned)
			{
				//================================================
				// Skin Shader側のwposはBone変換後のワールド座標
				// HLSL:
				//   float4 wpos = mul(mtx, IN.pos);
				//   mul(wpos, LightWVP[cascade])
				//================================================
				lightWVP = lightViewProjection;
			}

			//====================================================
			// HLSL側でmul(vector, matrix)を使用しているため
			// CPU側で転置して格納する
			//====================================================
			XMStoreFloat4x4(
				&pConstBuffer->LightWVP[cascadeIndex],
				XMMatrixTranspose(ShadowMapManager::GetLightViewProj(cascadeIndex))
			);
		}
	}
	else
	{
		//========================================================
		// ShadowMapが存在しない場合
		// 未初期化行列がShaderへ送られないように
		// 全CascadeをIdentityで初期化する
		//========================================================
		for (UINT cascadeIndex = 0; cascadeIndex < SHADOW_CASCADE_COUNT; ++cascadeIndex)
		{
			XMStoreFloat4x4(
				&pConstBuffer->LightWVP[cascadeIndex],
				XMMatrixIdentity()
			);
		}
	}

	//============================================================
	// b1 : オブジェクト用定数バッファ
	//============================================================
	Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(
		1,
		constBuff->GetGPUVirtualAddress()
	);

	if (pSkinMatrix)
	{
		for (DWORD boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
		{
			//========================================================
			// Current Bone
			//========================================================
			XMStoreFloat4x4(&bones[boneIndex], pSkinMatrix[boneIndex]);
			const XMMATRIX previousBone =
				mSkinMotionHistoryValid ? mPreviousSkinMatrix[boneIndex] : pSkinMatrix[boneIndex];

			XMStoreFloat4x4(&mPreviousBones[boneIndex], previousBone);
		}

		Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(3, bonesBuff->GetGPUVirtualAddress());
		Direct3D::getCommandList()->SetGraphicsRootConstantBufferView(4u, mPreviousBonesBuff->GetGPUVirtualAddress());

	}


	Direct3D::getCommandList()->SetPipelineState(pPipelineState[renderFlag]);

	Direct3D::getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Direct3D::getCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	Direct3D::getCommandList()->IASetIndexBuffer(&indexBufferView);

	//メッシュ単位の描画
	vnModel_MeshData* pMesh = reinterpret_cast<vnModel_MeshData*>(reinterpret_cast<__int64>(pModelData) + pModelData->MeshAccess);
	for (int i = 0; i < (int)pModelData->MeshNum; i++)
	{
		int mid = pMesh[i].MaterialID;

		//============================================================
		// MaterialBuffer 更新
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->Diffuse,
			*getDiffuse(mid)
		);

		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->Ambient,
			*getAmbient(mid)
		);

		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->Specular,
			*getSpecular(mid)
		);

		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->TilingOffset,
			*getTilingOffset(mid)
		);

		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->ShadowParam,
			XMVectorSet(
				isReceiveShadow() ? 1.0f : 0.0f,
				0.0f,
				0.0f,
				0.0f
			)
		);

		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->NormalParam,
			pMaterials[mid].NormalParam
		);

		//============================================================
		// PBR Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->PBRParam,
			pMaterials[mid].PBRParam
		);

		//============================================================
		// Emissive Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->EmissiveParam,
			pMaterials[mid].EmissiveParam
		);

		//============================================================
		// ORM Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->ORMParam,
			pMaterials[mid].ORMParam
		);

		//============================================================
		// Environment Reflection Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->EnvironmentParam,
			pMaterials[mid].EnvironmentParam
		);

		//============================================================
		// Subsurface Scattering Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->SSSParam,
			pMaterials[mid].SSSParam
		);


		//============================================================
		// Subsurface Scattering Color
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->SSSColor,
			pMaterials[mid].SSSColor
		);


		//============================================================
		// SSS Thickness Map Parameter
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->SSSMapParam,
			pMaterials[mid].SSSMapParam
		);


		//============================================================
		// Screen-Space SSS Material Profile
		//============================================================
		XMStoreFloat4(
			&pMaterials[mid].pMaterialBuffer->SSSDiffusionParam,
			pMaterials[mid].SSSDiffusionParam
		);

		//============================================================
		// Static / Dynamic ShadowMap SRVをMaterial Heapへコピー
		//============================================================
		if (ShadowMapManager::IsValid())
		{
			ID3D12Device* device = Direct3D::getDevice();

			assert(device != nullptr);

			const UINT descriptorSize =
				device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			//========================================================
			// Heap先頭Handle
			//========================================================
			const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
				pMaterials[mid].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

			//========================================================
			// Static ShadowMap 3枚
			//
			// コピー先:
			//   [1] Static Cascade 0
			//   [2] Static Cascade 1
			//   [3] Static Cascade 2
			//
			// コピー元:
			//   ShadowMap側のStatic Cascade 0～2
			//========================================================
			{
				D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle = heapStart;

				destinationHandle.ptr +=
					static_cast<SIZE_T>(MaterialDescriptor::StaticShadow0) *
					static_cast<SIZE_T>(descriptorSize);

				const D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle =
					ShadowMapManager::GetSRVCopyCPUHandle(
						0,
						ShadowMapLayer::Static
					);

				device->CopyDescriptorsSimple(
					SHADOW_CASCADE_COUNT,
					destinationHandle,
					sourceHandle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}

			//========================================================
			// Dynamic ShadowMap 3枚
			//
			// コピー先:
			//   [5] Dynamic Cascade 0
			//   [6] Dynamic Cascade 1
			//   [7] Dynamic Cascade 2
			//
			// コピー元:
			//   ShadowMap側のDynamic Cascade 0～2
			//========================================================
			{
				D3D12_CPU_DESCRIPTOR_HANDLE destinationHandle = heapStart;

				destinationHandle.ptr +=
					static_cast<SIZE_T>(MaterialDescriptor::DynamicShadow0) *
					static_cast<SIZE_T>(descriptorSize);

				const D3D12_CPU_DESCRIPTOR_HANDLE sourceHandle =
					ShadowMapManager::GetSRVCopyCPUHandle(
						0,
						ShadowMapLayer::Dynamic
					);

				device->CopyDescriptorsSimple(
					SHADOW_CASCADE_COUNT,
					destinationHandle,
					sourceHandle,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
				);
			}
		}

		//============================================================
		// DescriptorHeap セット
		//============================================================
		Direct3D::getCommandList()->SetDescriptorHeaps(
			1,
			&pMaterials[mid].basicDescHeap
		);

		//============================================================
		// RootParameter[2] に DescriptorTable の先頭を渡す
		//
		// この先頭から、
		//   t0 : Diffuse
		//   t1 : ShadowMap
		//   b1 : MaterialBuffer
		// がRootSignatureに従って参照される。
		//============================================================
		D3D12_GPU_DESCRIPTOR_HANDLE basicHeapHandle =
			pMaterials[mid].basicDescHeap->GetGPUDescriptorHandleForHeapStart();

		Direct3D::getCommandList()->SetGraphicsRootDescriptorTable(
			2,
			basicHeapHandle
		);

		//============================================================
		// Draw
		//============================================================
		Direct3D::getCommandList()->DrawIndexedInstanced(
			pMesh[i].IndexNum,
			1,
			pMesh[i].StartIndex,
			pMesh[i].StartVertex,
			0
		);
	}
}

void ModelBase::renderShadow()
{
	ID3D12GraphicsCommandList* cmdList = Direct3D::getCommandList();

	assert(cmdList != nullptr);

	//============================================================
	// 頂点・インデックスがない場合は描画しない
	//============================================================
	if (pModelData == nullptr)
	{
		return;
	}

	if (pVertexBuffer == nullptr || pIndexBuffer == nullptr)
	{
		return;
	}

	//============================================================
	// スキニングモデルか判定
	// ※BoneNumだけでなく
	// ※実際に必要なBufferが存在するかも確認する
	//============================================================
	const bool isSkinModel =
		pModelData->BoneNum > 0 &&
		pSkinMatrix != nullptr &&
		bonesBuff != nullptr &&
		bones != nullptr;

	//============================================================
	// モデルの種類に応じてShadow用PSOを切り替える
	//============================================================
	ID3D12PipelineState* shadowPSO = ShadowMapManager::GetPipelineState(isSkinModel);

	assert(shadowPSO != nullptr);

	cmdList->SetPipelineState(shadowPSO);

	//============================================================
	// スキニングモデルの場合※前フレームのボーンの位置を使わないように再計算を行う
	//============================================================
	if (isSkinModel)
	{
		for (DWORD boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
		{
			//====================================================
			// 通常のVS_3D_Skinと同じ形式で格納する
			//====================================================
			XMStoreFloat4x4(
				&bones[boneIndex],
				pSkinMatrix[boneIndex]
			);
		}

		//========================================================
		// RootParameter[1]へBoneBufferを設定
		// RootParameter[1]はShaderRegister b3として定義済み
		//========================================================
		cmdList->SetGraphicsRootConstantBufferView(
			1,
			bonesBuff->GetGPUVirtualAddress()
		);
	}


	//============================================================
	// 頂点構造
	//============================================================
	cmdList->IASetPrimitiveTopology(
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	);

	cmdList->IASetVertexBuffers(
		0,
		1,
		&vertexBufferView
	);

	cmdList->IASetIndexBuffer(&indexBufferView);

	//============================================================
	// メッシュ単位で描画
	// 各メッシュのIndex範囲だけ使ってDrawする
	//============================================================
	vnModel_MeshData* pMesh =
		reinterpret_cast<vnModel_MeshData*>(reinterpret_cast<__int64>(pModelData) + pModelData->MeshAccess);

	for (int i = 0; i < (int)pModelData->MeshNum; i++)
	{
		cmdList->DrawIndexedInstanced(
			pMesh[i].IndexNum,
			1,
			pMesh[i].StartIndex,
			pMesh[i].StartVertex,
			0
		);

		//============================================================
		// Directional Shadow PassのときだけCascade統計へ加算
		// ※Spot Shadow Pass中に加算すると
		// ※最後に描画したCascadeの統計へ混ざってしまう
		//============================================================
		if (!SpotLightShadowManager::IsRendering())
		{
			//============================================================
			// DrawIndexedInstancedを1回実行したため加算
			// 1オブジェクトが複数メッシュを持つ場合、
			// メッシュ数だけDraw Call数が増える
			//============================================================
			ShadowMapManager::AddShadowDrawCall();
		}

	}

}

void ModelBase::renderMotionVector()
{
	if (pModelData == nullptr || pVertexBuffer == nullptr || pIndexBuffer == nullptr)
	{
		return;
	}

	// 一人称カメラに自分の頭や胸元のモデルが映り込むのを防ぐ
	if (!mVisibleInMainCamera)
	{
		return;
	}

	//============================================================
	// Transparentは今は対象外
	//============================================================
	if (getRenderFlag(eRenderFlag::Transparent))
	{
		return;
	}


	const bool isSkinned = pModelData->BoneNum > 0;


	//============================================================
	// Rigid Model
	//============================================================
	if (!isSkinned)
	{
		if (!HasRigidMotion())
		{
			return;
		}


		PostProcessManager::GetMotionVectorPass().SetRigidObjectMotionPipeline();
	}
	else
	{
		//========================================================
		// Skinned Model
		//========================================================
		if (pSkinMatrix == nullptr || bonesBuff == nullptr || mPreviousBonesBuff == nullptr)
		{
			return;
		}


		PostProcessManager::GetMotionVectorPass().SetSkinnedObjectMotionPipeline();
	}


	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	//============================================================
	// b2 Matrix
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		constBuff->GetGPUVirtualAddress()
	);


	//============================================================
	// Skinの場合
	//============================================================
	if (isSkinned)
	{
		//========================================================
		// b3 Current Bones
		//========================================================
		commandList->SetGraphicsRootConstantBufferView(
			3u,
			bonesBuff->GetGPUVirtualAddress()
		);


		//========================================================
		// b4 Previous Bones
		//========================================================
		commandList->SetGraphicsRootConstantBufferView(
			4u,
			mPreviousBonesBuff->GetGPUVirtualAddress()
		);
	}


	//============================================================
	// Geometry
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(
		0u,
		1u,
		&vertexBufferView
	);

	commandList->IASetIndexBuffer(&indexBufferView);


	//============================================================
	// Mesh描画
	//============================================================
	vnModel_MeshData* meshes = getMesh();

	if (meshes == nullptr)
	{
		return;
	}


	for (UINT meshIndex = 0; meshIndex < pModelData->MeshNum; ++meshIndex)
	{
		commandList->DrawIndexedInstanced(
			meshes[meshIndex].IndexNum,
			1u,
			meshes[meshIndex].StartIndex,
			meshes[meshIndex].StartVertex,
			0
		);
	}
}

void ModelBase::CommitMotionHistory()
{
	//============================================================
	// 基底クラスのWorld履歴を保存
	//============================================================
	ObjectBase::CommitMotionHistory();


	//============================================================
	// Skinなしならここまで
	//============================================================
	if (pModelData == nullptr ||
		pModelData->BoneNum == 0 ||
		pSkinMatrix == nullptr ||
		mPreviousSkinMatrix == nullptr)
	{
		return;
	}

	for (UINT boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
	{
		mPreviousSkinMatrix[boneIndex] = pSkinMatrix[boneIndex];
	}

	mSkinMotionHistoryValid = true;
}

void ModelBase::ResetMotionHistory()
{
	//============================================================
	// World履歴Reset
	//============================================================
	ObjectBase::ResetMotionHistory();


	if (pModelData == nullptr ||
		pModelData->BoneNum == 0 ||
		pSkinMatrix == nullptr ||
		mPreviousSkinMatrix == nullptr)
	{
		return;
	}


	//============================================================
	// Previous Pose = Current Pose
	// Teleport / Animation強制切替時の巨大Velocityを防ぐ
	//============================================================
	for (UINT boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
	{
		mPreviousSkinMatrix[boneIndex] = pSkinMatrix[boneIndex];
	}

	mSkinMotionHistoryValid = true;
}

void ModelBase::renderSubsurfaceMask(ID3D12PipelineState* pipelineState)
{
	//============================================================
	// 基本チェック
	//============================================================
	if (pipelineState == nullptr ||
		pModelData == nullptr ||
		pMaterials == nullptr ||
		pVertexBuffer == nullptr ||
		pIndexBuffer == nullptr)
	{
		return;
	}

	// 一人称カメラに自分の頭や胸元のモデルが映り込むのを防ぐ
	if (!mVisibleInMainCamera)
	{
		return;
	}

	//========================================================
	// SSS Maskは画面上のモデルだけを描画する
	//========================================================
	if (!IsInsideMainCameraFrustum())
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();

	if (commandList == nullptr)
	{
		return;
	}


	//============================================================
	// Mask用PSO
	//============================================================
	commandList->SetPipelineState(pipelineState);

	//============================================================
	// Geometry
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		&vertexBufferView
	);

	commandList->IASetIndexBuffer(&indexBufferView);

	//============================================================
	// 現在のWVP
	// 通常render()と同じ計算にしておく
	//============================================================
	const bool skinned = pModelData->BoneNum > 0;


	const XMMATRIX WVP =
		skinned ? (*vnCamera::getScreen()) : XMMatrixMultiply(World, *vnCamera::getScreen());


	const XMMATRIX WV =
		skinned ? (*vnCamera::getView()) : XMMatrixMultiply(World, *vnCamera::getView());


	XMStoreFloat4x4(
		&pConstBuffer->WVP,
		XMMatrixTranspose(WVP)
	);


	XMStoreFloat4x4(
		&pConstBuffer->World,
		XMMatrixTranspose(World)
	);


	XMStoreFloat4x4(
		&pConstBuffer->WorldView,
		XMMatrixTranspose(WV)
	);


	//============================================================
	// Root[1]
	//
	// Object Matrix Buffer
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		constBuff->GetGPUVirtualAddress()
	);


	//============================================================
	// Skin Matrix
	//============================================================
	if (skinned && pSkinMatrix != nullptr && bonesBuff != nullptr)
	{
		//========================================================
		// 現在Bone姿勢をUpload
		//========================================================
		for (DWORD boneIndex = 0; boneIndex < pModelData->BoneNum; ++boneIndex)
		{
			XMStoreFloat4x4(
				&bones[boneIndex],
				pSkinMatrix[boneIndex]
			);
		}


		//========================================================
		// Root[3]
		//
		// Current Bone Buffer
		//========================================================
		commandList->SetGraphicsRootConstantBufferView(
			3u,
			bonesBuff->GetGPUVirtualAddress()
		);
	}


	//============================================================
	// Mesh Data
	//============================================================
	vnModel_MeshData* meshes = reinterpret_cast<vnModel_MeshData*>(reinterpret_cast<BYTE*>(pModelData) + pModelData->MeshAccess);


	//============================================================
	// Mesh単位
	//============================================================
	for (int meshIndex = 0; meshIndex < getMeshNum(); ++meshIndex)
	{
		const int materialId = meshes[meshIndex].MaterialID;


		if (materialId < 0 || materialId >= getMaterialNum())
		{
			continue;
		}


		//========================================================
		// CPU Material側でSSSがOFFなら
		// Draw Callそのものを発行しない
		//========================================================
		const bool sssEnabled =
			XMVectorGetX(pMaterials[materialId].SSSParam) >= 0.5f;


		const bool pbrEnabled =
			XMVectorGetW(pMaterials[materialId].PBRParam) >= 0.5f;


		const float sssStrength =
			XMVectorGetY(pMaterials[materialId].SSSParam);


		const float globalThickness =
			XMVectorGetZ(pMaterials[materialId].SSSParam);


		if (!sssEnabled ||
			!pbrEnabled ||
			sssStrength <= 0.0001f ||
			globalThickness <= 0.0001f)
		{
			continue;
		}


		//========================================================
		// Mask Shaderが使用するMaterial情報だけ
		// 念のため最新値をGPU Bufferへ書き込む
		//========================================================
		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->Diffuse,
			pMaterials[materialId].Diffuse
		);


		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->TilingOffset,
			pMaterials[materialId].TilingOffset
		);


		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->PBRParam,
			pMaterials[materialId].PBRParam
		);


		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->SSSParam,
			pMaterials[materialId].SSSParam
		);


		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->SSSMapParam,
			pMaterials[materialId].SSSMapParam
		);


		XMStoreFloat4(
			&pMaterials[materialId].pMaterialBuffer->SSSDiffusionParam,
			pMaterials[materialId].SSSDiffusionParam
		);

		//========================================================
		// Material Descriptor Heap
		//
		// t0  = Diffuse
		// b1  = Material
		// t16 = Thickness
		//========================================================
		ID3D12DescriptorHeap* heaps[] =
		{
			pMaterials[materialId].basicDescHeap
		};


		commandList->SetDescriptorHeaps(
			1u,
			heaps
		);


		commandList->SetGraphicsRootDescriptorTable(
			2u,
			pMaterials[materialId].basicDescHeap->GetGPUDescriptorHandleForHeapStart()
		);


		//========================================================
		// Draw
		//========================================================
		commandList->DrawIndexedInstanced(
			meshes[meshIndex].IndexNum,
			1u,
			meshes[meshIndex].StartIndex,
			meshes[meshIndex].StartVertex,
			0u
		);
	}
}

void ModelBase::renderGISurface(ID3D12PipelineState* rigidPSO, ID3D12PipelineState* skinPSO)
{
	//============================================================
	// 基本チェック
	//============================================================
	if (pModelData == nullptr ||
		pMaterials == nullptr ||
		pVertexBuffer == nullptr ||
		pIndexBuffer == nullptr ||
		constBuff == nullptr ||
		pConstBuffer == nullptr)
	{
		return;
	}

	// 一人称カメラに自分の頭や胸元のモデルが映り込むのを防ぐ
	if (!mVisibleInMainCamera)
	{
		return;
	}

	//============================================================
	// 通常描画されていないObjectはGI Surfaceにも入れない
	//============================================================
	if (!isRenderEnable())
	{
		return;
	}


	//============================================================
	// Screen-Space GI SurfaceはOpaque Geometryを基準にする
	//============================================================
	if (getRenderFlag(eRenderFlag::Transparent))
	{
		return;
	}


	//============================================================
	// Main Depthへ書いていないObjectは
	// Depth EQUALを使うGI Surface Passとは一致しない
	//============================================================
	if (!getRenderFlag(eRenderFlag::ZWrite))
	{
		return;
	}

	if (!IsInsideMainCameraFrustum())
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = Direct3D::getCommandList();


	if (commandList == nullptr)
	{
		return;
	}


	//============================================================
	// Skinning Model判定
	//============================================================
	const bool skinned = pModelData->BoneNum > 0;


	//============================================================
	// 使用するGI Surface PSOを決定
	//============================================================
	ID3D12PipelineState* pipelineState = skinned ? skinPSO : rigidPSO;


	if (pipelineState == nullptr)
	{
		return;
	}


	//============================================================
	// GI Surface専用PSO
	//============================================================
	commandList->SetPipelineState(pipelineState);


	//============================================================
	// 現在の行列
	// DepthFunc = EQUALなので、
	// Main Scene描画時と同じClip Positionを生成する必要がある
	//============================================================
	const XMMATRIX WVP =
		skinned ? (*vnCamera::getScreen()) : XMMatrixMultiply(World, *vnCamera::getScreen());


	const XMMATRIX WV =
		skinned ? (*vnCamera::getView()) : XMMatrixMultiply(World, *vnCamera::getView());


	//============================================================
	// WVP
	//============================================================
	XMStoreFloat4x4(
		&pConstBuffer->WVP,
		XMMatrixTranspose(WVP)
	);


	//============================================================
	// World
	//============================================================
	XMStoreFloat4x4(
		&pConstBuffer->World,
		XMMatrixTranspose(World)
	);


	//============================================================
	// WorldView
	//============================================================
	XMStoreFloat4x4(
		&pConstBuffer->WorldView,
		XMMatrixTranspose(WV)
	);


	//============================================================
	// Normal変換用 World Inverse Transpose
	//============================================================
	const XMMATRIX worldInvTranspose =
		XMMatrixTranspose(XMMatrixInverse(nullptr, World));


	XMStoreFloat4x4(
		&pConstBuffer->WorldInvTranspose,
		XMMatrixTranspose(worldInvTranspose)
	);


	//============================================================
	// Root[1]
	// MatrixBuffer b2
	//============================================================
	commandList->SetGraphicsRootConstantBufferView(
		1u,
		constBuff->GetGPUVirtualAddress()
	);


	//============================================================
	// Skinning Model
	//============================================================
	if (skinned)
	{
		//========================================================
		// Bone情報がない場合は描画できない
		//========================================================
		if (pSkinMatrix == nullptr || bonesBuff == nullptr || bones == nullptr)
		{
			return;
		}


		//========================================================
		// Current Bone MatrixをGPU Bufferへ更新
		//========================================================
		for (UINT boneIndex = 0u; boneIndex < pModelData->BoneNum; ++boneIndex)
		{
			XMStoreFloat4x4(
				&bones[boneIndex],
				pSkinMatrix[boneIndex]
			);
		}


		//========================================================
		// Root[3]
		// BoneBuffer b3
		//========================================================
		commandList->SetGraphicsRootConstantBufferView(
			3u,
			bonesBuff->GetGPUVirtualAddress()
		);


		//========================================================
		// Root[4]
		//========================================================
		if (mPreviousBonesBuff != nullptr)
		{
			commandList->SetGraphicsRootConstantBufferView(
				4u,
				mPreviousBonesBuff->GetGPUVirtualAddress()
			);
		}
	}


	//============================================================
	// Geometry
	//============================================================
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	commandList->IASetVertexBuffers(
		0u,
		1u,
		&vertexBufferView
	);


	commandList->IASetIndexBuffer(&indexBufferView);


	//============================================================
	// Mesh Data
	//============================================================
	vnModel_MeshData* meshes =
		reinterpret_cast<vnModel_MeshData*>(reinterpret_cast<BYTE*>(pModelData) + pModelData->MeshAccess);


	if (meshes == nullptr)
	{
		return;
	}


	//============================================================
	// Mesh単位でMaterialを設定して描画
	//============================================================
	for (int meshIndex = 0; meshIndex < getMeshNum(); ++meshIndex)
	{
		const int materialId = meshes[meshIndex].MaterialID;


		if (materialId < 0 || materialId >= getMaterialNum())
		{
			continue;
		}


		stMaterial& material = pMaterials[materialId];


		if (material.pMaterialBuffer == nullptr || material.basicDescHeap == nullptr)
		{
			continue;
		}

		//========================================================
		// BaseColor
		//========================================================
		XMStoreFloat4(
			&material.pMaterialBuffer->Diffuse,
			material.Diffuse
		);


		//========================================================
		// UV Tiling / Offset
		//========================================================
		XMStoreFloat4(
			&material.pMaterialBuffer->TilingOffset,
			material.TilingOffset
		);


		//========================================================
		// Normal Map
		// TEST 3-Bに関係する
		//========================================================
		XMStoreFloat4(
			&material.pMaterialBuffer->NormalParam,
			material.NormalParam
		);


		//========================================================
		// Metallic / Roughness / AO / PBR Enabled
		//========================================================
		XMStoreFloat4(
			&material.pMaterialBuffer->PBRParam,
			material.PBRParam
		);


		//========================================================
		// ORM Texture設定
		//========================================================
		XMStoreFloat4(
			&material.pMaterialBuffer->ORMParam,
			material.ORMParam
		);


		//========================================================
		// Material Descriptor Heap
		// t0  Diffuse
		// b1  MaterialBuffer
		// t7  Normal
		// t8  ORM
		// ...
		//========================================================
		ID3D12DescriptorHeap* heaps[] =
		{
			material.basicDescHeap
		};


		commandList->SetDescriptorHeaps(
			1u,
			heaps
		);


		//========================================================
		// Root[2]
		//
		// Main 3D Material Descriptor Table
		//========================================================
		commandList->SetGraphicsRootDescriptorTable(
			2u,
			material.basicDescHeap->GetGPUDescriptorHandleForHeapStart()
		);


		//========================================================
		// GI Surface MRTへ描画
		//========================================================
		commandList->DrawIndexedInstanced(
			meshes[meshIndex].IndexNum,
			1u,
			meshes[meshIndex].StartIndex,
			meshes[meshIndex].StartVertex,
			0u
		);
	}
}

//スキンマトリクスの設定
void ModelBase::setSkinMatrix(XMMATRIX* m)
{
	if (pSkinMatrix == NULL)return;

	WORD* table = reinterpret_cast<WORD*>(reinterpret_cast<__int64>(pModelData) + pModelData->BoneTableAccess);

	XMMATRIX* iBind = reinterpret_cast<XMMATRIX*>(reinterpret_cast<__int64>(pModelData) + pModelData->iBindAccess);

	for (DWORD i = 0; i < pModelData->BoneNum; i++)
	{
		pSkinMatrix[i] = iBind[i] * m[table[i]];
	}
}


int ModelBase::getVertexNum()
{
	return pModelData ? pModelData->VertexNum : 0;
}

int ModelBase::getIndexNum()
{
	return pModelData ? pModelData->IndexNum : 0;
}

int ModelBase::getFaceNum()
{
	return pModelData ? pModelData->IndexNum / 3 : 0;
}

int ModelBase::getMaterialNum()
{
	return pModelData ? pModelData->MaterialNum : 0;
}

int ModelBase::getMeshNum()
{
	return pModelData ? pModelData->MeshNum : 0;
}

int ModelBase::getBoneNum()
{
	return pModelData ? pModelData->BoneNum : 0;
}

const WCHAR* ModelBase::getTextureName(int id)
{
	if (pModelData == nullptr || id < 0 || id >= getMaterialNum())
	{
		return nullptr;
	}


	const BYTE* materialBase =
		reinterpret_cast<const BYTE*>(pModelData) + pModelData->MaterialAccess;


	//========================================================
	// V3
	//========================================================
	if (pModelData->Version == vnMODEL_DATA_V3_VER)
	{
		const vnModel_MaterialDataV3* materials =
			reinterpret_cast<const vnModel_MaterialDataV3*>(materialBase);

		return materials[id].BaseColorTexture;
	}


	//========================================================
	// V2
	//========================================================
	if (pModelData->Version == vnMODEL_DATA_V2_VER)
	{
		const vnModel_MaterialDataV2* materials =
			reinterpret_cast<const vnModel_MaterialDataV2*>(materialBase);

		return materials[id].BaseColorTexture;
	}

	//========================================================
	// V1
	//========================================================
	const vnModel_MaterialData* materials =
		reinterpret_cast<const vnModel_MaterialData*>(materialBase);

	return materials[id].Texture;
}

//メッシュデータ(先頭アドレス)の取得
vnModel_MeshData *ModelBase::getMesh()
{
	if (!pModelData)return NULL;
	vnModel_MeshData *ret = reinterpret_cast<vnModel_MeshData*>(reinterpret_cast<__int64>(pModelData) + pModelData->MeshAccess);
	return ret;
}

vnVertex3DFile* ModelBase::getFileVertex()
{
	if (pModelData == nullptr)
	{
		return nullptr;
	}

	return reinterpret_cast<vnVertex3DFile*>(reinterpret_cast<BYTE*>(pModelData) + pModelData->VertexAccess);
}

//インデックスデータ(先頭アドレス)の取得
LPWORD ModelBase::getIndex()
{
	if (!pModelData)return NULL;
	LPWORD ret = reinterpret_cast<LPWORD>(reinterpret_cast<__int64>(pModelData) + pModelData->IndexAccess);
	return ret;
}

//拡散色の設定
void ModelBase::setDiffuse(float r, float g, float b, float a, int id)
{
	if(pMaterials==NULL || getMaterialNum()<=0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorSet(r, g, b, a);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorSet(r, g, b, a);
	}
}

void ModelBase::setDiffuse(const XMVECTOR *v, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = *v;
		}
	}
	else if(id>=0 && id< getMaterialNum())
	{
		pMaterials[id].Diffuse = *v;
	}
}

void ModelBase::setDiffuseR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i< getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorSetX(pMaterials[i].Diffuse, value);
		}
	}
	else if(id>=0 && id< getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorSetX(pMaterials[id].Diffuse, value);
	}
}

void ModelBase::setDiffuseG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorSetY(pMaterials[i].Diffuse, value);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorSetY(pMaterials[id].Diffuse, value);
	}
}

void ModelBase::setDiffuseB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorSetZ(pMaterials[i].Diffuse, value);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorSetZ(pMaterials[id].Diffuse, value);
	}
}

void ModelBase::setDiffuseA(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorSetW(pMaterials[i].Diffuse, value);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorSetW(pMaterials[id].Diffuse, value);
	}
}

void ModelBase::addDiffuseR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorAdd(pMaterials[i].Diffuse, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorAdd(pMaterials[id].Diffuse, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
	}
}

void ModelBase::addDiffuseG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorAdd(pMaterials[i].Diffuse, XMVectorSet(0.0f, value, 0.0f, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorAdd(pMaterials[id].Diffuse, XMVectorSet(0.0f, value, 0.0f, 0.0f));
	}
}

void ModelBase::addDiffuseB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorAdd(pMaterials[i].Diffuse, XMVectorSet(0.0f, 0.0f, value, 0.0f));
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorAdd(pMaterials[id].Diffuse, XMVectorSet(0.0f, 0.0f, value, 0.0f));
	}
}

void ModelBase::addDiffuseA(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Diffuse = XMVectorAdd(pMaterials[i].Diffuse, XMVectorSet(0.0f, 0.0f, 0.0f, value));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Diffuse = XMVectorAdd(pMaterials[id].Diffuse, XMVectorSet(0.0f, 0.0f, 0.0f, value));
	}
}

//拡散色の取得
XMVECTOR *ModelBase::getDiffuse(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return NULL;
	return &pMaterials[id].Diffuse;
}

float ModelBase::getDiffuseR(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 1.0f;
	return XMVectorGetX(pMaterials[id].Diffuse);
}

float ModelBase::getDiffuseG(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 1.0f;
	return XMVectorGetY(pMaterials[id].Diffuse);
}

float ModelBase::getDiffuseB(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 1.0f;
	return XMVectorGetZ(pMaterials[id].Diffuse);
}

float ModelBase::getDiffuseA(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 1.0f;
	return XMVectorGetW(pMaterials[id].Diffuse);
}


//環境色の設定
void ModelBase::setAmbient(float r, float g, float b, float a, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorSet(r, g, b, a);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorSet(r, g, b, a);
	}
}

void ModelBase::setAmbient(const XMVECTOR *v, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = *v;
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Ambient = *v;
	}
}

void ModelBase::setAmbientR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if(id==-1)
	{
		for(int i=0; i<getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorSetX(pMaterials[i].Ambient, value);
		}
	}
	else if(id>=0 && id<getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorSetX(pMaterials[id].Ambient, value);
	}
}

void ModelBase::setAmbientG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorSetY(pMaterials[i].Ambient, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorSetY(pMaterials[id].Ambient, value);
	}
}

void ModelBase::setAmbientB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorSetZ(pMaterials[i].Ambient, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorSetZ(pMaterials[id].Ambient, value);
	}
}

void ModelBase::setAmbientA(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorSetW(pMaterials[i].Ambient, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorSetW(pMaterials[id].Ambient, value);
	}
}

void ModelBase::addAmbientR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorAdd(pMaterials[i].Ambient, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorAdd(pMaterials[id].Ambient, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
	}
}

void ModelBase::addAmbientG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorAdd(pMaterials[i].Ambient, XMVectorSet(0.0f, value, 0.0f, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorAdd(pMaterials[id].Ambient, XMVectorSet(0.0f, value, 0.0f, 0.0f));
	}
}

void ModelBase::addAmbientB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorAdd(pMaterials[i].Ambient, XMVectorSet(0.0f, 0.0f, value, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorAdd(pMaterials[id].Ambient, XMVectorSet(0.0f, 0.0f, value, 0.0f));
	}
}

void ModelBase::addAmbientA(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Ambient = XMVectorAdd(pMaterials[i].Ambient, XMVectorSet(0.0f, 0.0f, 0.0f, value));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Ambient = XMVectorAdd(pMaterials[id].Ambient, XMVectorSet(0.0f, 0.0f, 0.0f, value));
	}
}

//環境色の取得
XMVECTOR *ModelBase::getAmbient(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return NULL;
	return &pMaterials[id].Ambient;
}

float ModelBase::getAmbientR(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetX(pMaterials[id].Ambient);
}

float ModelBase::getAmbientG(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetY(pMaterials[id].Ambient);
}

float ModelBase::getAmbientB(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetZ(pMaterials[id].Ambient);
}

float ModelBase::getAmbientA(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetW(pMaterials[id].Ambient);
}


//鏡面反射色の設定
void ModelBase::setSpecular(float r, float g, float b, float p, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorSet(r, g, b, p);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorSet(r, g, b, p);
	}
}

void ModelBase::setSpecular(const XMVECTOR *v, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = *v;
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = *v;
	}
}

void ModelBase::setSpecularR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorSetX(pMaterials[i].Specular, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorSetX(pMaterials[id].Specular, value);
	}
}

void ModelBase::setSpecularG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorSetY(pMaterials[i].Specular, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorSetY(pMaterials[id].Specular, value);
	}
}

void ModelBase::setSpecularB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorSetZ(pMaterials[i].Specular, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorSetZ(pMaterials[id].Specular, value);
	}
}

void ModelBase::setSpecularPower(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorSetW(pMaterials[i].Specular, value);
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorSetW(pMaterials[id].Specular, value);
	}
}

void ModelBase::addSpecularR(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorAdd(pMaterials[i].Specular, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorAdd(pMaterials[id].Specular, XMVectorSet(value, 0.0f, 0.0f, 0.0f));
	}
}

void ModelBase::addSpecularG(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorAdd(pMaterials[i].Specular, XMVectorSet(0.0f, value, 0.0f, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorAdd(pMaterials[id].Specular, XMVectorSet(0.0f, value, 0.0f, 0.0f));
	}
}

void ModelBase::addSpecularB(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorAdd(pMaterials[i].Specular, XMVectorSet(0.0f, 0.0f, value, 0.0f));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorAdd(pMaterials[id].Specular, XMVectorSet(0.0f, 0.0f, value, 0.0f));
	}
}

void ModelBase::addSpecularPower(float value, int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return;
	if (id == -1)
	{
		for (int i = 0; i < getMaterialNum(); i++)
		{
			pMaterials[i].Specular = XMVectorAdd(pMaterials[i].Specular, XMVectorSet(0.0f, 0.0f, 0.0f, value));
		}
	}
	else if (id >= 0 && id < getMaterialNum())
	{
		pMaterials[id].Specular = XMVectorAdd(pMaterials[id].Specular, XMVectorSet(0.0f, 0.0f, 0.0f, value));
	}
}

//鏡面反射色の取得
XMVECTOR *ModelBase::getSpecular(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return NULL;
	return &pMaterials[id].Specular;
}

float ModelBase::getSpecularR(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetX(pMaterials[id].Specular);
}

float ModelBase::getSpecularG(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetY(pMaterials[id].Specular);
}

float ModelBase::getSpecularB(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 0.0f;
	return XMVectorGetZ(pMaterials[id].Specular);
}

float ModelBase::getSpecularPower(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return 1.0f;
	return XMVectorGetW(pMaterials[id].Specular);
}

void ModelBase::SetPBREnabled(bool enabled, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].PBRParam =
		XMVectorSetW(pMaterials[materialId].PBRParam, enabled ? 1.0f : 0.0f);
}

void ModelBase::SetMetallic(float metallic, int materialId)
{
	if(pMaterials == nullptr || materialId< 0 || materialId >= getMaterialNum())
	{
		return;
	}

	const float clampedMetallic = (std::clamp)(metallic, 0.0f, 1.0f);

	pMaterials[materialId].PBRParam =
		XMVectorSetX(pMaterials[materialId].PBRParam, clampedMetallic);

}

void ModelBase::SetRoughness(float roughness, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	//========================================================
	// ※0にするとGGX計算が極端になりやすいため
	// ※最低値を0.04にする
	//========================================================
	const float clampedRoughness = (std::clamp)(roughness, 0.04f, 1.0f);

	pMaterials[materialId].PBRParam =
		XMVectorSetY(pMaterials[materialId].PBRParam, clampedRoughness);
}

void ModelBase::SetMaterialAO(float ao, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}
	const float clampedAO = (std::clamp)(ao, 0.0f, 1.0f);

	pMaterials[materialId].PBRParam = XMVectorSetZ(pMaterials[materialId].PBRParam, clampedAO);
}

void ModelBase::SetEmissive(float r, float g, float b, float intensity, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].EmissiveParam =
		XMVectorSet(
			(std::max)(r, 0.0f),
			(std::max)(g, 0.0f),
			(std::max)(b, 0.0f),
			(std::max)(intensity, 0.0f)
		);
}

//タイリングとオフセットの取得
const XMVECTOR* ModelBase::getTilingOffset(int id)
{
	if (pMaterials == NULL || getMaterialNum() <= 0)return NULL;
	return &pMaterials[id].TilingOffset;
}

bool ModelBase::BeginTextureUploadBatch()
{
	return BeginModelTextureUploadBatch();
}

bool ModelBase::EndTextureUploadBatch()
{
	return EndModelTextureUploadBatch();
}

bool ModelBase::PreloadVnmFilesParallel(const std::vector<std::wstring>& modelPaths, unsigned int workerCount)
{
	using Clock = std::chrono::steady_clock;

	const auto startTime = Clock::now();

	//========================================================
	// Workerに渡す読み込み対象
	//========================================================
	struct PreloadJob
	{
		std::wstring Key;
		std::filesystem::path FilePath;
	};

	std::vector<PreloadJob> jobs;

	jobs.reserve(modelPaths.size());

	//========================================================
	// 入力に同じVNMが何回も現れることがある
	// 例えば同じ本棚を何十個配置しても
	// 読み込み対象は1ファイルだけでよい
	//========================================================
	std::unordered_set<std::wstring> registeredKeys;

	//========================================================
	// 重複排除・既存キャッシュの確認
	//========================================================
	for (const std::wstring& pathString : modelPaths)
	{
		if (pathString.empty())
		{
			continue;
		}

		const std::filesystem::path filePath(pathString);

		const std::wstring key =NormalizeVnmCachePath(filePath);

		//====================================================
		// 今回のPreloadリスト内で重複していればスキップ
		//====================================================
		if (!registeredKeys.insert(key).second)
		{
			continue;
		}

		//====================================================
		// 既存のvnModelが使用中のVNMであれば
		// ディスクから読み直す必要はない
		//====================================================
		bool alreadyLoaded = false;

		{
			std::lock_guard<std::mutex> lock(gSharedVnmBinaryMutex);

			auto found = gSharedVnmBinaryCache.find(key);

			if (found != gSharedVnmBinaryCache.end())
			{
				alreadyLoaded =static_cast<bool>(found->second.lock());
			}
		}

		if (alreadyLoaded)
		{
			continue;
		}

		jobs.push_back({ key, filePath });
	}

	//========================================================
	// 対象がなければ成功
	//========================================================
	if (jobs.empty())
	{
		OutputDebugStringA("[VNMPreload] No new files to preload.\n");

		return true;
	}

	//========================================================
	// Worker数決定
	// デフォルトはCPU数を参考にしつつ最大4
	// ファイル数よりWorker数を増やさない
	//========================================================
	if (workerCount == 0)
	{
		workerCount = std::thread::hardware_concurrency();

		if (workerCount == 0)
		{
			workerCount = 2;
		}

		workerCount = (std::min)(workerCount, 4u);
	}

	workerCount = (std::max)(workerCount, 1u);

	workerCount = (std::min)(workerCount,static_cast<unsigned int>(jobs.size()));

	//========================================================
	// 読み込み結果を保持
	// 各Workerは別々のindexに結果を書き込む
	// vectorのサイズはここで固定するため
	// 実行中の再配置は発生しない
	//========================================================
	std::vector<std::shared_ptr<SharedModelBinary>> loadedBinaries(jobs.size());

	//========================================================
	// 次に処理するJob番号
	// atomicにすることで複数Workerが同じファイルを担当しない
	//========================================================
	std::atomic<size_t> nextJobIndex{ 0 };

	//========================================================
	// Worker処理
	// GPU Resource・Scene・vnModelには触れない
	// ディスク読み込みだけを並列実行する
	//========================================================
	const auto worker = [&]()
		{
			while (true)
			{
				const size_t index = nextJobIndex.fetch_add(1, std::memory_order_relaxed);

				if (index >= jobs.size())
				{
					break;
				}

				//================================================
				// ファイル単位の失敗は個別に処理する
				// 他のファイルの読み込みは継続する
				//================================================
				try
				{
					loadedBinaries[index] =LoadVnmBinaryFromDisk(jobs[index].FilePath);
				}
				catch (...)
				{
					loadedBinaries[index].reset();
				}
			}
		};

	//========================================================
	// Worker起動
	// ※asyncをファイル数分生成するのではなくWorker数だけ起動する
	// ※WorkerはatomicのJob番号を取得して処理を続ける
	//========================================================
	std::vector<std::future<void>> futures;

	futures.reserve(workerCount);

	for (unsigned int i = 0; i < workerCount; ++i)
	{
		futures.emplace_back(std::async(std::launch::async,worker));
	}

	//========================================================
	// 全Workerの終了を待つ
	// ※この待機中にGPU CommandListを操作しない
	//========================================================
	for (auto& future : futures)
	{
		future.get();
	}

	//========================================================
	// 読み込み済みデータを共有キャッシュへ登録
	// ここから先はメインスレッドだけで実行する
	//========================================================
	size_t successCount = 0;
	size_t failureCount = 0;

	std::uint64_t totalBytes = 0;

	{
		std::lock_guard<std::mutex> lock(gSharedVnmBinaryMutex);

		for (size_t i = 0; i < jobs.size(); ++i)
		{
			auto& binary = loadedBinaries[i];

			if (!binary)
			{
				++failureCount;
				continue;
			}

			//================================================
			// 読み込み中に同じキーが登録されていた場合は
			// 既存のResourceを優先する
			//================================================
			auto found =gSharedVnmBinaryCache.find(jobs[i].Key);

			if (found != gSharedVnmBinaryCache.end())
			{
				if (found->second.lock())
				{
					continue;
				}
			}

			//================================================
			// 通常キャッシュには弱参照を登録
			//================================================
			gSharedVnmBinaryCache[jobs[i].Key] = binary;

			//================================================
			// プリロード期間中だけ強参照を保持
			// ※これがないとWorkerの一時配列が消えた後に
			// ※weak_ptrが失効してしまう
			//================================================
			gPinnedVnmBinaryCache[jobs[i].Key] = binary;

			++successCount;

			totalBytes +=static_cast<std::uint64_t>(binary->Size);
		}
	}

	//========================================================
	// 統計出力
	//========================================================
	const auto endTime = Clock::now();

	const double elapsedMs =std::chrono::duration<double, std::milli>(endTime - startTime).count();

	const double totalMiB =static_cast<double>(totalBytes) /(1024.0 * 1024.0);

	char message[512] = {};

	sprintf_s(
		message,
		"[VNMPreload] Workers=%u Files=%zu "
		"Loaded=%zu Failed=%zu "
		"Size=%.2f MiB Elapsed=%.2f ms\n",
		workerCount,
		jobs.size(),
		successCount,
		failureCount,
		totalMiB,
		elapsedMs
	);

	OutputDebugStringA(message);

	//========================================================
	// 一部失敗しても、成功したファイルは使用可能
	// 失敗ファイルはvnModelの生成時に従来の同期ロードへ
	// フォールバックする
	//========================================================
	return failureCount == 0;
}

void ModelBase::ReleaseVnmPreloadCache()
{
	std::lock_guard<std::mutex> lock(gSharedVnmBinaryMutex);

	//========================================================
	// プリロード用の強参照をすべて解放
	// 通常キャッシュのweak_ptrは残す
	// 失効したエントリは次回アクセス時に削除される
	//========================================================
	gPinnedVnmBinaryCache.clear();

#if defined(_DEBUG)
	OutputDebugStringA(
		"[VNMPreload] Temporary cache released.\n"
	);
#endif
}

bool ModelBase::PreloadDdsFromVnmFilesParallel(
	const std::vector<std::wstring>& modelPaths,
	unsigned int workerCount)
{
	using Clock = std::chrono::steady_clock;

	const auto startTime = Clock::now();

	//========================================================
	// DDSロードJob
	//========================================================
	struct DdsJob
	{
		std::wstring Key;
		std::filesystem::path FilePath;
	};

	std::vector<DdsJob> jobs;

	//========================================================
	// 重複したDDSファイルを除外する
	// 同じ画像を複数のMaterialやModelが使用しても
	// ディスクからのロードは1回だけにする
	//========================================================
	std::unordered_set<std::wstring> registeredKeys;

	size_t invalidModelCount = 0;

	//========================================================
	// Texture登録用Lambda
	// VNMに記録されたTexture名は
	// モデルファイルのフォルダからの相対パス
	// BaseColor / Normal / ORMなどの用途に関係なく
	// 同じDDSファイルならCPU画像を共有する
	//========================================================
	auto AddDdsJob =[&](const std::filesystem::path& modelPath,const WCHAR* textureName)
		{
			if (textureName == nullptr ||
				textureName[0] == L'\0')
			{
				return;
			}

			//====================================================
			// モデルと同じフォルダを基準にTexture Pathを作る
			//====================================================
			const std::filesystem::path texturePath =
				modelPath.parent_path() /std::filesystem::path(textureName);

			//====================================================
			// 今回の並列ロード対象はDDSのみ
			// PNG/TGA/HDRなどは同期ロードへ渡す
			//====================================================
			std::wstring extension =texturePath.extension().wstring();

			std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t ch)
				{
					return static_cast<wchar_t>(std::towlower(ch));
				}
			);

			if (extension != L".dds")
			{
				return;
			}

			//====================================================
			// パスを正規化して重複を検出
			//====================================================
			const std::wstring key =NormalizeDdsPreloadPath(texturePath);

			if (!registeredKeys.insert(key).second)
			{
				return;
			}

			//====================================================
			// 既にプリロード済みなら再ロードしない
			//====================================================
			{
				std::lock_guard<std::mutex> lock(gDdsPreloadMutex);

				if (gPinnedDdsPreloadCache.find(key) !=gPinnedDdsPreloadCache.end())
				{
					return;
				}
			}

			jobs.push_back({ key, texturePath });
		};

	//========================================================
	// VNMからDDSファイルのパスを収集
	// この処理はメインスレッドで実行する
	// VNMはSTEP 50-Fのキャッシュから取得できる
	//========================================================
	for (const std::wstring& pathString : modelPaths)
	{
		if (pathString.empty())
		{
			continue;
		}

		const std::filesystem::path modelPath(pathString);

		auto binary = AcquireSharedVnmBinary(modelPath);

		if (!binary || !binary->Bytes)
		{
			++invalidModelCount;
			continue;
		}

		//====================================================
		// ヘッダーの最低サイズを確認
		//====================================================
		if (binary->Size < sizeof(vnModelData))
		{
			++invalidModelCount;
			continue;
		}

		const BYTE* bytes = binary->Bytes.get();

		const vnModelData* header =
			reinterpret_cast<const vnModelData*>(bytes);

		//====================================================
		// VNMバージョンごとのMaterialサイズ
		//====================================================
		size_t materialStride = 0;

		switch (header->Version)
		{
		case vnMODEL_DATA_V1_VER:
			materialStride = sizeof(vnModel_MaterialData);
			break;

		case vnMODEL_DATA_V2_VER:
			materialStride = sizeof(vnModel_MaterialDataV2);
			break;

		case vnMODEL_DATA_V3_VER:
			materialStride = sizeof(vnModel_MaterialDataV3);
			break;

		default:
			++invalidModelCount;
			continue;
		}

		//====================================================
		// 不正なMaterial Accessからの読み出しを防ぐ
		// MaterialNum * materialStrideで先に計算すると
		// オーバーフローする可能性があるため
		// 除算を使って範囲確認する
		//====================================================
		const size_t materialOffset =
			static_cast<size_t>(header->MaterialAccess);

		const size_t materialCount =
			static_cast<size_t>(header->MaterialNum);

		if (materialOffset > binary->Size ||materialCount >(binary->Size - materialOffset) / materialStride)
		{
			++invalidModelCount;
			continue;
		}

		const BYTE* materialBase =bytes + materialOffset;

		//====================================================
		// VNM V1
		// BaseColorのみ
		//====================================================
		if (header->Version == vnMODEL_DATA_V1_VER)
		{
			const auto* materials =reinterpret_cast<const vnModel_MaterialData*>(materialBase);

			for (size_t i = 0; i < materialCount; ++i)
			{
				AddDdsJob(modelPath,materials[i].Texture);
			}
		}
		//====================================================
		// VNM V2
		// BaseColor / Normal / ORM / Thickness / Emissive
		//====================================================
		else if (header->Version == vnMODEL_DATA_V2_VER)
		{
			const auto* materials =reinterpret_cast<const vnModel_MaterialDataV2*>(materialBase);

			for (size_t i = 0; i < materialCount; ++i)
			{
				const auto& mat = materials[i];

				AddDdsJob(modelPath, mat.BaseColorTexture);
				AddDdsJob(modelPath, mat.NormalTexture);
				AddDdsJob(modelPath, mat.ORMTexture);
				AddDdsJob(modelPath, mat.ThicknessTexture);
				AddDdsJob(modelPath, mat.EmissiveTexture);
			}
		}
		//====================================================
		// VNM V3
		// V2 + SkinMask
		//====================================================
		else
		{
			const auto* materials = reinterpret_cast<const vnModel_MaterialDataV3*>(materialBase);

			for (size_t i = 0; i < materialCount; ++i)
			{
				const auto& mat = materials[i];

				AddDdsJob(modelPath, mat.BaseColorTexture);
				AddDdsJob(modelPath, mat.NormalTexture);
				AddDdsJob(modelPath, mat.ORMTexture);
				AddDdsJob(modelPath, mat.ThicknessTexture);
				AddDdsJob(modelPath, mat.EmissiveTexture);
				AddDdsJob(modelPath, mat.SkinMaskTexture);
			}
		}
	}

	//========================================================
	// ロード対象なし
	//========================================================
	if (jobs.empty())
	{
		OutputDebugStringA("[DDSPreload] No new DDS files.\n");

		return invalidModelCount == 0;
	}

	//========================================================
	// Worker数決定
	// 自動設定では最大4スレッド
	// Job数よりWorkerを増やさない
	//========================================================
	if (workerCount == 0)
	{
		workerCount = std::thread::hardware_concurrency();

		if (workerCount == 0)
		{
			workerCount = 2;
		}

		workerCount = (std::min)(workerCount, 4u);
	}

	workerCount = (std::max)(workerCount, 1u);

	workerCount = (std::min)(workerCount, static_cast<unsigned int>(jobs.size()));

	//========================================================
	// 読み込み結果
	// Workerごとに独立したScratchImageを使用する
	// 複数Threadから同一ScratchImageを変更しない
	//========================================================
	std::vector<std::shared_ptr<PreloadedDdsData>> results(jobs.size());

	std::atomic<size_t> nextJobIndex{ 0 };

	//========================================================
	// Worker Thread
	// DDSをCPUへロードするだけ
	// GPU Resource・CommandList・SRVは触らない
	//========================================================
	auto worker = [&]()
		{
			while (true)
			{
				const size_t index = nextJobIndex.fetch_add(1, std::memory_order_relaxed);

				if (index >= jobs.size())
				{
					break;
				}

				try
				{
					auto data =std::make_shared<PreloadedDdsData>();

					//============================================
					// DirectXTex DDS Loader
					// 全MipをScratchImageへ読み込む
					//============================================
					const HRESULT hr =
						LoadFromDDSFile(
							jobs[index].FilePath.c_str(),
							DirectX::DDS_FLAGS_NONE,
							&data->Metadata,
							data->Image
						);

					if (FAILED(hr) ||
						data->Metadata.mipLevels == 0 ||
						data->Image.GetImage(0, 0, 0) == nullptr)
					{
						continue;
					}

					//============================================
					// Worker専用Indexに結果を保存
					// vectorサイズは起動前に固定済み
					//============================================
					results[index] = std::move(data);
				}
				catch (...)
				{
					//============================================
					// 失敗したDDSのみ従来ロードへ
					// フォールバックできるようnullptrを残す
					//============================================
					results[index].reset();
				}
			}
		};

	//========================================================
	// Worker起動
	//========================================================
	std::vector<std::future<void>> futures;

	futures.reserve(workerCount);

	for (unsigned int i = 0; i < workerCount; ++i)
	{
		futures.emplace_back(std::async(std::launch::async,worker));
	}

	//========================================================
	// 全Worker完了待機
	// 読み込み結果のキャッシュ登録は
	// すべてのWorkerが終了してから実行する
	//========================================================
	for (auto& future : futures)
	{
		future.get();
	}

	//========================================================
	// 統計
	//========================================================
	size_t loadedCount = 0;
	size_t failedCount = 0;
	std::uint64_t totalBytes = 0;

	//========================================================
	// CPUキャッシュ登録
	// Worker終了後にまとめて登録する
	//========================================================
	{
		std::lock_guard<std::mutex> lock(gDdsPreloadMutex);

		for (size_t i = 0; i < jobs.size(); ++i)
		{
			if (!results[i])
			{
				++failedCount;
				continue;
			}

			//============================================
			// 既存エントリがあればそれを優先する
			//============================================
			const auto found =gPinnedDdsPreloadCache.find(jobs[i].Key);

			if (found != gPinnedDdsPreloadCache.end())
			{
				continue;
			}

			totalBytes += static_cast<std::uint64_t>(results[i]->Image.GetPixelsSize());

			gPinnedDdsPreloadCache.emplace(jobs[i].Key,std::move(results[i]));

			++loadedCount;
		}
	}

	//========================================================
	// デバッグログ
	//========================================================
	const auto endTime = Clock::now();

	const double elapsedMs =std::chrono::duration<double, std::milli>(endTime - startTime).count();

	const double totalMiB =
		static_cast<double>(totalBytes) / (1024.0 * 1024.0);

	char message[512] = {};

	sprintf_s(
		message,
		"[DDSPreload] Workers=%u Files=%zu "
		"Loaded=%zu Failed=%zu InvalidVNM=%zu "
		"CPUData=%.2f MiB Elapsed=%.2f ms\n",
		workerCount,
		jobs.size(),
		loadedCount,
		failedCount,
		invalidModelCount,
		totalMiB,
		elapsedMs
	);

	OutputDebugStringA(message);

	//========================================================
	// 一部失敗しても正常なDDSは利用可能
	// 失敗したものは同期ロードに任せる
	//========================================================
	return failedCount == 0 && invalidModelCount == 0;
}

void ModelBase::ReleaseDdsPreloadCache()
{
	//========================================================
	// プリロードで保持していたScratchImageを解放
	// Acquire関数側でshared_ptrを取得している間は
	// キャッシュをclearしても画像本体は生存する
	//========================================================
	std::lock_guard<std::mutex> lock(gDdsPreloadMutex);

	gPinnedDdsPreloadCache.clear();

#if defined(_DEBUG)
	OutputDebugStringA("[DDSPreload] Temporary cache released.\n");
#endif
}

void ModelBase::BuildShadowBoundingSphere()
{
	mHasShadowBounds = false;

	mShadowBoundsCenterLocal =
	{
		0.0f,
		0.0f,
		0.0f
	};

	mShadowBoundsRadiusLocal = 0.0f;

	//============================================================
	// モデルデータ確認
	//============================================================
	if (pModelData == nullptr || pModelData->VertexNum == 0)
	{
		return;
	}

	//============================================================
	// 頂点データの先頭
	//============================================================
	const BYTE* vertexData =
		reinterpret_cast<const BYTE*>(reinterpret_cast<const BYTE*>(pModelData) + pModelData->VertexAccess);

	//============================================================
	// 実際の頂点Stride
	// 通常モデル: sizeof(vnVertex3D)
	// スキニングモデル: vnVertex3D + SkinIndex + SkinWeight
	//============================================================
	const UINT vertexStride =
		(pModelData->VertexStride > 0) ?
		pModelData->VertexStride : static_cast<UINT>(sizeof(vnVertex3DFile));

	if (vertexData == nullptr)
	{
		return;
	}

	//============================================================
	// AABBの最小・最大座標
	//============================================================
	XMFLOAT3 minimum =
	{
		FLT_MAX,
		FLT_MAX,
		FLT_MAX
	};

	XMFLOAT3 maximum =
	{
		-FLT_MAX,
		-FLT_MAX,
		-FLT_MAX
	};

	for (DWORD vertexIndex = 0; vertexIndex < pModelData->VertexNum; ++vertexIndex)
	{
		//========================================================
		// 実際のStrideを使用して頂点先頭へ移動
		//========================================================
		const vnVertex3DFile* vertex =
			reinterpret_cast<const vnVertex3DFile*>(
				vertexData +
				static_cast<size_t>(vertexIndex) *
				static_cast<size_t>(vertexStride));

		minimum.x = (std::min)(minimum.x, vertex->x);
		minimum.y = (std::min)(minimum.y, vertex->y);
		minimum.z = (std::min)(minimum.z, vertex->z);
		maximum.x = (std::max)(maximum.x, vertex->x);
		maximum.y = (std::max)(maximum.y, vertex->y);
		maximum.z = (std::max)(maximum.z, vertex->z);
	}

	//============================================================
	// AABBの中央
	//============================================================
	mShadowBoundsCenterLocal =
	{
		(minimum.x + maximum.x) * 0.5f,
		(minimum.y + maximum.y) * 0.5f,
		(minimum.z + maximum.z) * 0.5f
	};

	const XMVECTOR localCenter =
		XMLoadFloat3(&mShadowBoundsCenterLocal);

	float maximumDistanceSquared = 0.0f;

	//============================================================
	// 中心から最も遠い頂点を探す
	//============================================================
	for (DWORD vertexIndex = 0; vertexIndex < pModelData->VertexNum; ++vertexIndex)
	{
		const vnVertex3DFile* vertex =
			reinterpret_cast<const vnVertex3DFile*>(
				vertexData +
				static_cast<size_t>(vertexIndex) *
				static_cast<size_t>(vertexStride));

		const XMVECTOR position =
			XMVectorSet(
				vertex->x,
				vertex->y,
				vertex->z,
				1.0f
			);

		const XMVECTOR difference =
			XMVectorSubtract(
				position,
				localCenter
			);

		const float distanceSquared =
			XMVectorGetX(XMVector3LengthSq(difference));

		maximumDistanceSquared = (std::max)(maximumDistanceSquared, distanceSquared);

	}

	mShadowBoundsRadiusLocal = sqrtf(maximumDistanceSquared);

	//============================================================
	// ※Skinningモデルはアニメーションで頂点が広がるため
	// ※通常モデルより余裕を大きくする
	//============================================================
	mShadowBoundsPadding = (pModelData->BoneNum > 0) ? 1.50f : 1.10f;

	mHasShadowBounds = mShadowBoundsRadiusLocal > 0.0f;
}

bool ModelBase::GetShadowBoundingSphere(XMVECTOR& outWorldCenter, float& outWorldRadius) const
{
	if (!mHasShadowBounds)
	{
		outWorldCenter =XMVectorZero();
		outWorldRadius =0.0f;

		return false;
	}

	//============================================================
	// Bounds変換に使用するWorld Matrix
	// Rigid: vnModel::World
	// Skinned: SkeletalModelから渡されたRoot World
	//============================================================
	const XMMATRIX boundsWorld =
		mUseShadowBoundsWorld ? mShadowBoundsWorld : World;


	//============================================================
	// Local Bounding Sphere Center
	//============================================================
	const XMVECTOR localCenter =
		XMLoadFloat3(&mShadowBoundsCenterLocal);


	//============================================================
	// World Center
	//============================================================
	outWorldCenter =
		XMVector3TransformCoord(
			localCenter,
			boundsWorld
		);


	//============================================================
	// 非均一Scale対応
	// Sphereなので最大Scaleを使用する
	//============================================================
	const float scaleX =
		XMVectorGetX(XMVector3Length(boundsWorld.r[0]));

	const float scaleY =
		XMVectorGetX(XMVector3Length(boundsWorld.r[1]));

	const float scaleZ =
		XMVectorGetX(XMVector3Length(boundsWorld.r[2]));

	const float maximumScale =
		(std::max)(scaleX, (std::max)(scaleY, scaleZ));


	//============================================================
	// World Radius
	//============================================================
	outWorldRadius = mShadowBoundsRadiusLocal * maximumScale * mShadowBoundsPadding;

	return true;
}

bool ModelBase::LoadNormalMap(const WCHAR* folder, const WCHAR* file, int materialId)
{
	//============================================================
	// 引数確認
	//============================================================
	if (folder == nullptr ||
		file == nullptr ||
		pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return false;
	}

	//============================================================
	// File Path生成
	//============================================================
	WCHAR path[512] = {};

	swprintf_s(
		path,
		L"%s%s",
		folder,
		file
	);

	//============================================================
	// Shared Normal Texture取得
	// Cache Hit: 既存Textureを再利用
	// Cache Miss: Textureをロードして新規生成
	//============================================================
	auto sharedTexture =
		AcquireSharedDataTexture(
			path,
			file,
			DataTextureType::Normal
		);

	if (!sharedTexture)
	{
		return false;
	}

	//============================================================
	// Materialの既存Textureを解放
	//============================================================
	SAFE_RELEASE(pMaterials[materialId].normalTexBuff);
	//============================================================
	// 共有TextureをMaterialに登録
	//============================================================
	pMaterials[materialId].normalTexBuff = sharedTexture->Texture.Get();
	pMaterials[materialId].normalTexBuff->AddRef();

	//============================================================
	// shared_ptrによって共有Textureの寿命を保持
	//============================================================
	mSharedNormalTextures[materialId] = sharedTexture;

	ID3D12Device* device = Direct3D::getDevice();

	//============================================================
	// Normal Map用Descriptor位置
	//============================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE normalHandle =
		pMaterials[materialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	normalHandle.ptr +=
		static_cast<SIZE_T>(MaterialDescriptor::Normal) * static_cast<SIZE_T>(descriptorSize);

	//============================================================
	// Normal Map SRV生成
	//============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = sharedTexture->Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = sharedTexture->MipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		pMaterials[materialId].normalTexBuff,
		&srvDesc,
		normalHandle
	);

	//============================================================
	// Normal Mapping有効化
	//============================================================
	pMaterials[materialId].NormalParam = XMVectorSetX(pMaterials[materialId].NormalParam, 1.0f);

	return true;
}

void ModelBase::SetNormalMapStrength(float strength, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].NormalParam =
		XMVectorSetY(pMaterials[materialId].NormalParam, (std::max)(strength, 0.0f));
}

void ModelBase::SetNormalMapFlipY(bool flipY, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].NormalParam =
		XMVectorSetZ(pMaterials[materialId].NormalParam, flipY ? -1.0f : 1.0f);
}

bool ModelBase::LoadORMMap(const WCHAR* folder, const WCHAR* file, int materialId)
{
	//============================================================
	// 引数確認
	//============================================================
	if (folder == nullptr ||
		file == nullptr ||
		pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return false;
	}

	//============================================================
	// Texture Path
	//============================================================
	WCHAR path[512] = {};

	swprintf_s(
		path,
		L"%s%s",
		folder,
		file
	);

	//============================================================
	// ORM TextureをCacheから取得
	// 同じORM Textureが読み込まれている場合、
	// GPU Resourceは新たに生成されない
	//============================================================
	auto sharedTexture =
		AcquireSharedDataTexture(
			path,
			file,
			DataTextureType::ORM
		);

	if (!sharedTexture)
	{
		return false;
	}

	//============================================================
	// 既存ORM Textureを解放
	//============================================================
	SAFE_RELEASE(pMaterials[materialId].ormTexBuff);

	//============================================================
	// Shared TextureをMaterialへ登録
	//============================================================
	pMaterials[materialId].ormTexBuff = sharedTexture->Texture.Get();

	//============================================================
	// 既存のvnModelデストラクタに対応するCOM参照
	//============================================================
	pMaterials[materialId].ormTexBuff->AddRef();

	//============================================================
	// 共有TextureのLifetimeを保持する
	//============================================================
	mSharedORMTextures[materialId] = sharedTexture;

	ID3D12Device* device = Direct3D::getDevice();

	//============================================================
	// ORM Descriptor Handle
	//============================================================
	const UINT descriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE ormHandle =
		pMaterials[materialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	ormHandle.ptr +=
		static_cast<SIZE_T>(MaterialDescriptor::ORM) * static_cast<SIZE_T>(descriptorSize);

	//============================================================
	// ORM SRV生成
	// ORMはLinear Formatを使用する
	//============================================================
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = sharedTexture->Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = sharedTexture->MipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(
		pMaterials[materialId].ormTexBuff,
		&srvDesc,
		ormHandle
	);

	//============================================================
	// ORM Mappingを有効化
	//============================================================
	pMaterials[materialId].ORMParam =
		XMVectorSetX(pMaterials[materialId].ORMParam, 1.0f);

	return true;
}

void ModelBase::SetORMMapEnabled(bool enabled, int materialId)
{
	if (pMaterials == nullptr ||materialId < 0 ||materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].ORMParam =
		XMVectorSetX(
			pMaterials[materialId].ORMParam,
			enabled ? 1.0f : 0.0f
		);
}

void ModelBase::SetORMScale(float aoScale, float roughnessScale, float metallicScale, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	const float useORM = XMVectorGetX(pMaterials[materialId].ORMParam);

	pMaterials[materialId].ORMParam = XMVectorSet(
		useORM,
		(std::max)(aoScale, 0.0f),
		(std::max)(roughnessScale, 0.0f),
		(std::max)(metallicScale, 0.0f)
	);
}

void ModelBase::RefreshEnvironmentMapDescriptor(int materialId)
{
	if (pMaterials == nullptr || getMaterialNum() <= 0)
	{
		return;
	}

	ID3D12Device* device = Direct3D::getDevice();
	assert(device != nullptr);
	if (device == nullptr)
	{
		return;
	}

	const UINT descriptorSize = 
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//============================================================
	// 1マテリアル分のDescriptorを更新する補助処理
	//============================================================
	auto RefreshOneMaterial = [this, descriptorSize](int targetMaterialId)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE environmentHandle =
				pMaterials[targetMaterialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

			environmentHandle.ptr +=
				static_cast<SIZE_T>(MaterialDescriptor::Environment) *
				static_cast<SIZE_T>(descriptorSize);

			//========================================================
			// Environment ResourceからSRVを再作成
			//========================================================
			EnvironmentMapManager::WriteEnvironmentSRV(environmentHandle);

			//========================================================
			// Roughness参照用の最大Mip Level
			// MipCountが1なら最大Mipは0
			//========================================================
			const UINT mipCount = EnvironmentMapManager::GetPrefilteredMipCount();

			const float maximumMipLevel =
				static_cast<float>((mipCount > 0) ? mipCount - 1 : 0);

			pMaterials[targetMaterialId].EnvironmentParam =
				XMVectorSetZ(pMaterials[targetMaterialId].EnvironmentParam, maximumMipLevel);

			D3D12_CPU_DESCRIPTOR_HANDLE prefilteredHandle =
				pMaterials[targetMaterialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

			prefilteredHandle.ptr +=
				static_cast<SIZE_T>(MaterialDescriptor::PrefilteredEnvironment) *
				static_cast<SIZE_T>(descriptorSize);

			EnvironmentMapManager::WritePrefilteredEnvironmentSRV(prefilteredHandle);

			D3D12_CPU_DESCRIPTOR_HANDLE irradianceHandle =pMaterials[targetMaterialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

			irradianceHandle.ptr +=
				static_cast<SIZE_T>(MaterialDescriptor::IrradianceEnvironment) *
				static_cast<SIZE_T>(descriptorSize);

			EnvironmentMapManager::WriteIrradianceEnvironmentSRV(irradianceHandle);

			D3D12_CPU_DESCRIPTOR_HANDLE brdfLUTHandle =pMaterials[targetMaterialId].basicDescHeap->GetCPUDescriptorHandleForHeapStart();

			brdfLUTHandle.ptr +=
				static_cast<SIZE_T>(MaterialDescriptor::BRDFLUT) *
				static_cast<SIZE_T>(descriptorSize);

			EnvironmentMapManager::WriteBRDFLUTSRV(brdfLUTHandle);
		};

	//============================================================
	// 全マテリアル
	//============================================================
	if (materialId == -1)
	{
		for (int currentMaterialId = 0; currentMaterialId < getMaterialNum(); ++currentMaterialId)
		{
			RefreshOneMaterial(currentMaterialId);
		}

		return;
	}

	//============================================================
	// 指定マテリアル
	//============================================================
	if (materialId < 0 || materialId >= getMaterialNum())
	{
		assert(false);
		return;
	}

	RefreshOneMaterial(materialId);
}

void ModelBase::SetEnvironmentReflection(bool enabled, float diffuseIntensity, float specularIntensity, int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	const UINT mipCount = EnvironmentMapManager::GetPrefilteredMipCount();

	const float maximumMipLevel =
		static_cast<float>((mipCount > 0) ? mipCount - 1 : 0);

	pMaterials[materialId].EnvironmentParam =
		XMVectorSet(
			enabled ? 1.0f : 0.0f,
			(std::max)(diffuseIntensity, 0.0f),
			maximumMipLevel,
			(std::max)(specularIntensity, 0.0f)
		);
}

bool ModelBase::LoadSubsurfaceThicknessMap(const WCHAR* folder, const WCHAR* file, int materialId)
{
	//============================================================
	// 引数・Material確認
	//============================================================
	if (folder == nullptr ||
		file == nullptr ||
		file[0] == L'\0' ||
		pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return false;
	}

	//============================================================
	// Texture Path
	//============================================================
	WCHAR path[512] = {};

	swprintf_s(path, L"%s%s", folder, file);

	//============================================================
	// Shared Thickness Texture取得
	//============================================================
	auto sharedTexture =
		AcquireSharedAdditionalTexture(
			path,
			file,
			AdditionalTextureKind::Thickness
		);

	if (!sharedTexture)
	{
		return false;
	}

	//============================================================
	// Thickness SRVを現在のMaterialへ登録
	//============================================================
	if (!CreateSharedAdditionalTextureSRV(
		pMaterials[materialId].basicDescHeap,
		static_cast<UINT>(MaterialDescriptor::Thickness),
		sharedTexture))
	{
		return false;
	}

	//============================================================
	// COM参照を保持
	//============================================================
	pMaterials[materialId].thicknessTexBuff =sharedTexture->Texture;

	//============================================================
	// 共有ResourceをMaterial単位で保持
	//============================================================
	mSharedThicknessTextures[materialId] =std::move(sharedTexture);

	//============================================================
	// 既存のSSS設定を維持
	// SSSMapParam.x = Thickness Map有効
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetX(pMaterials[materialId].SSSMapParam,1.0f);

	return true;

}

void ModelBase::SetSubsurfaceThicknessMapEnabled(bool enabled, int materialId)
{
	//============================================================
	// Material確認
	//============================================================
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//============================================================
	// x = Use Thickness Map
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetX(
			pMaterials[materialId].SSSMapParam,
			enabled ? 1.0f : 0.0f
		);
}

void ModelBase::SetSubsurfaceThicknessMapScale(float scale, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}


	//============================================================
	// 負値は使用しない
	//============================================================
	const float safeScale = (std::max)(scale, 0.0f);


	//============================================================
	// y = Map Scale
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetY(
			pMaterials[materialId].SSSMapParam,
			safeScale
		);
}

void ModelBase::SetSubsurfaceThicknessMapInvert(bool invert, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//============================================================
	// z = Invert
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetZ(
			pMaterials[materialId].SSSMapParam,
			invert ? 1.0f : 0.0f
		);
}

void ModelBase::SetSubsurfaceEnabled(bool enabled, int materialId)
{
	//========================================================
	// Material確認
	//========================================================
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//========================================================
	// x = Enable
	//========================================================
	pMaterials[materialId].SSSParam =
		XMVectorSetX(
			pMaterials[materialId].SSSParam,
			enabled ? 1.0f : 0.0f
		);
}

void ModelBase::SetSubsurfaceStrength(float strength, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//========================================================
	// 負値は使用しない
	//========================================================
	const float safeStrength = (std::max)(strength, 0.0f);

	//========================================================
	// y = Strength
	//========================================================
	pMaterials[materialId].SSSParam =
		XMVectorSetY(
			pMaterials[materialId].SSSParam,
			safeStrength
		);
}

void ModelBase::SetSubsurfaceThickness(float thickness, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}


	//========================================================
	// 0～1のMaskとして扱う
	//========================================================
	const float safeThickness =(std::clamp)(thickness,0.0f,1.0f);

	//========================================================
	// z = Thickness Mask
	//========================================================
	pMaterials[materialId].SSSParam =
		XMVectorSetZ(
			pMaterials[materialId].SSSParam,
			safeThickness
		);
}

void ModelBase::SetSubsurfaceDistortion(float distortion, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	const float safeDistortion = (std::clamp)(distortion, 0.0f, 1.0f);

	//========================================================
	// w = Distortion
	//========================================================
	pMaterials[materialId].SSSParam =
		XMVectorSetW(
			pMaterials[materialId].SSSParam,
			safeDistortion
		);
}

void ModelBase::SetSubsurfaceColor(float r, float g, float b, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//========================================================
	// 現在のPowerを保持する
	//========================================================
	const float currentPower = XMVectorGetW(pMaterials[materialId].SSSColor);

	//========================================================
	// RGBだけ更新
	// 物理的な散乱色として0～1へ制限
	//========================================================
	pMaterials[materialId].SSSColor =
		XMVectorSet(
			(std::clamp)(r, 0.0f, 1.0f),
			(std::clamp)(g, 0.0f, 1.0f),
			(std::clamp)(b, 0.0f, 1.0f),
			currentPower
		);
}

void ModelBase::SetSubsurfacePower(float power, int materialId)
{
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//========================================================
	// 0に近すぎるとpow()の形が極端になるためClamp
	//========================================================
	const float safePower =(std::clamp)(power,0.1f,32.0f);

	pMaterials[materialId].SSSColor =
		XMVectorSetW(
			pMaterials[materialId].SSSColor,
			safePower
		);
}

void ModelBase::SetSubsurfaceScatteringProfile(float red, float green, float blue, int materialId)
{
	//============================================================
   // Material確認
   //============================================================
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}

	//============================================================
	// 現在設定されているWorld Radiusは維持する
	//============================================================
	const float radius = XMVectorGetW(pMaterials[materialId].SSSDiffusionParam);

	//============================================================
	// 負数は使用しない
	// RGBすべて0ならGlobal ProfileへFallbackする
	//============================================================
	pMaterials[materialId].SSSDiffusionParam =
		XMVectorSet(
			(std::max)(red, 0.0f),
			(std::max)(green, 0.0f),
			(std::max)(blue, 0.0f),
			radius
		);
}

void ModelBase::SetSubsurfaceWorldSpaceRadius(float radius, int materialId)
{
	//============================================================
	// Material確認
	//============================================================
	if (pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return;
	}


	//============================================================
	// RGB Profileを維持したままWだけ変更
	//============================================================
	pMaterials[materialId].SSSDiffusionParam =
		XMVectorSetW(
			pMaterials[materialId].SSSDiffusionParam,
			(std::max)(radius,0.0f)
		);
}

bool ModelBase::LoadSubsurfaceSkinMask(const WCHAR* folder, const WCHAR* file, int materialId)
{
	//============================================================
	// 引数・Material確認
	//============================================================
	if (folder == nullptr ||
		file == nullptr ||
		file[0] == L'\0' ||
		pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return false;
	}

	//============================================================
	// Texture Path
	//============================================================
	WCHAR path[512] = {};

	swprintf_s(path, L"%s%s", folder, file);

	//============================================================
	// Shared SkinMask Texture取得
	//============================================================
	auto sharedTexture =
		AcquireSharedAdditionalTexture(
			path,
			file,
			AdditionalTextureKind::SkinMask
		);

	if (!sharedTexture)
	{
		return false;
	}

	//============================================================
	// SkinMask SRVをMaterialへ登録
	//============================================================
	if (!CreateSharedAdditionalTextureSRV(
		pMaterials[materialId].basicDescHeap,
		static_cast<UINT>(MaterialDescriptor::SkinMask),
		sharedTexture))
	{
		return false;
	}

	//============================================================
	// Material側のComPtrへ共有Resourceを設定
	//============================================================
	pMaterials[materialId].skinMaskTexBuff = sharedTexture->Texture;

	//============================================================
	// 共有Resourceへの参照を保持
	//============================================================
	mSharedSkinMaskTextures[materialId] = std::move(sharedTexture);

	//============================================================
	// SkinMask有効化
	// SSSMapParam.w = 1.0f
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetW(pMaterials[materialId].SSSMapParam, 1.0f);

	return true;
}

void ModelBase::SetSubsurfaceSkinMaskEnabled(bool enabled, int materialId)
{
	//============================================================
	// Material確認
	//============================================================
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}


	//============================================================
	// SSSMapParam.w
	//
	// 0 = SkinMaskを使用しない
	// 1 = SkinMaskを使用する
	//============================================================
	pMaterials[materialId].SSSMapParam =
		XMVectorSetW(
			pMaterials[materialId].SSSMapParam,
			enabled ? 1.0f : 0.0f
		);
}

void ModelBase::ResetSubsurfaceDiffusionProfile(int materialId)
{
	if (pMaterials == nullptr || materialId < 0 || materialId >= getMaterialNum())
	{
		return;
	}

	pMaterials[materialId].SSSDiffusionParam = XMVectorZero();
}

bool ModelBase::LoadEmissiveMap(const WCHAR* folder, const WCHAR* file, int materialId)
{
	//============================================================
	// 引数・Material確認
	//============================================================
	if (folder == nullptr ||
		file == nullptr ||
		file[0] == L'\0' ||
		pMaterials == nullptr ||
		materialId < 0 ||
		materialId >= getMaterialNum())
	{
		return false;
	}

	//============================================================
	// Texture Path
	//============================================================
	WCHAR path[512] = {};

	swprintf_s(path, L"%s%s", folder, file);

	//============================================================
	// Emissive Texture取得
	// Cache Miss時のみGPU Resourceを生成する
	//============================================================
	auto sharedTexture =
		AcquireSharedAdditionalTexture(
			path,
			file,
			AdditionalTextureKind::Emissive
		);

	if (!sharedTexture)
	{
		return false;
	}

	//============================================================
	// MaterialのEmissive SRVへ共有Resourceを設定
	//============================================================
	if (!CreateSharedAdditionalTextureSRV(
		pMaterials[materialId].basicDescHeap,
		static_cast<UINT>(MaterialDescriptor::Emissive),
		sharedTexture))
	{
		return false;
	}

	//============================================================
	// ComPtrへコピー
	// コピーによってCOM参照が追加される
	// ※生ポインタのAddRef()は不要
	//============================================================
	pMaterials[materialId].emissiveTexBuff = sharedTexture->Texture;

	//============================================================
	// 共有ResourceのLifetimeを維持
	//============================================================
	mSharedEmissiveTextures[materialId] = std::move(sharedTexture);

	return true;
}

bool ModelBase::BuildGpuVerticesWithTangents()
{
	//============================================================
	// モデルデータ確認
	//============================================================
	if (pModelData == nullptr || pModelData->VertexNum == 0)
	{
		return false;
	}

	//============================================================
	// VNM頂点データの先頭
	//============================================================
	const BYTE* sourceVertexData =
		reinterpret_cast<const BYTE*>(pModelData) + pModelData->VertexAccess;

	//============================================================
	// VNMファイル内の実際のStride
	// 通常モデル: 48 bytes
	// スキニングモデル: 48 bytes + SkinIndex + SkinWeight
	//============================================================
	const UINT sourceVertexStride =
		(pModelData->VertexStride > 0) ? pModelData->VertexStride : static_cast<UINT>(sizeof(vnVertex3DFile));

	const UINT vertexCount = static_cast<UINT>(pModelData->VertexNum);

	//============================================================
	// GPU用頂点配列を確保
	//============================================================
	mGpuVertices.clear();

	mGpuVertices.resize(vertexCount);

	//============================================================
	// VNM頂点をGPU頂点へ変換
	//============================================================
	for (UINT vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		//========================================================
		// VertexStrideを使って対象頂点へ移動
		//========================================================
		const vnVertex3DFile* source =
			reinterpret_cast<const vnVertex3DFile*>(
				sourceVertexData +
				static_cast<size_t>(vertexIndex) *
				static_cast<size_t>(sourceVertexStride));

		assert(source != nullptr);

		vnVertex3D& destination =mGpuVertices[vertexIndex];


		//========================================================
		// Position
		//========================================================
		destination.x = source->x;
		destination.y = source->y;
		destination.z = source->z;

		//========================================================
		// Normal
		//========================================================
		destination.nx = source->nx;
		destination.ny = source->ny;
		destination.nz = source->nz;

		//========================================================
		// Vertex Color
		//========================================================
		destination.r = source->r;
		destination.g = source->g;
		destination.b = source->b;
		destination.a = source->a;

		//========================================================
		// UV
		//========================================================
		destination.u = source->u;
		destination.v = source->v;

		//========================================================
		// Tangentは後で三角形から計算する
		//========================================================
		destination.tx = 0.0f;
		destination.ty = 0.0f;
		destination.tz = 0.0f;
		destination.tw = 1.0f;
	}

	//============================================================
	// Index Bufferの先頭アドレス
	//============================================================
	const BYTE* indexData =
		reinterpret_cast<const BYTE*>(pModelData) +pModelData->IndexAccess;

	//============================================================
	// 16bit Indexか32bit Indexか
	//============================================================
	const bool uses16BitIndices =
		pModelData->VertexNum <=0xffff;

	//============================================================
	// Index番号を取得する補助関数
	//============================================================
	auto GetIndex = [indexData, uses16BitIndices](UINT indexPosition)-> UINT
		{
			if (uses16BitIndices)
			{
				const WORD* indices = reinterpret_cast<const WORD*>(indexData);

				return static_cast<UINT>(indices[indexPosition]);
			}

			const DWORD* indices = reinterpret_cast<const DWORD*>(indexData);

			return static_cast<UINT>(indices[indexPosition]);
		};

	//============================================================
	// XMFLOAT3なら特別な16バイトAlignmentを要求しない
	//============================================================
	std::vector<XMFLOAT3> tangentSums(vertexCount, XMFLOAT3(0.0f, 0.0f, 0.0f));
	std::vector<XMFLOAT3> bitangentSums(vertexCount, XMFLOAT3(0.0f, 0.0f, 0.0f));

	vnModel_MeshData* meshes = getMesh();

	//============================================================
	// 三角形単位でTangentを計算
	//============================================================
	for (UINT meshIndex = 0; meshIndex < pModelData->MeshNum; ++meshIndex)
	{
		//========================================================
		// 三角形を構成する頂点番号
		//========================================================
		const vnModel_MeshData& mesh = meshes[meshIndex];

		const UINT meshIndexEnd = mesh.StartIndex + mesh.IndexNum;

		for (UINT indexPosition = mesh.StartIndex; indexPosition + 2 < meshIndexEnd; indexPosition += 3)
		{
			const UINT index0 =
				GetIndex(indexPosition + 0) +mesh.StartVertex;

			const UINT index1 =
				GetIndex(indexPosition + 1) +mesh.StartVertex;

			const UINT index2 =
				GetIndex(indexPosition + 2) +mesh.StartVertex;

			//========================================================
			// 不正なIndexを保護
			//========================================================
			if (index0 >= vertexCount ||index1 >= vertexCount ||index2 >= vertexCount)
			{
				assert(false);

				continue;
			}

			const vnVertex3D& vertex0 =
				mGpuVertices[index0];

			const vnVertex3D& vertex1 =
				mGpuVertices[index1];

			const vnVertex3D& vertex2 =
				mGpuVertices[index2];

			//========================================================
			// 頂点位置
			//========================================================
			const XMVECTOR position0 =
				XMVectorSet(
					vertex0.x,
					vertex0.y,
					vertex0.z,
					0.0f
				);

			const XMVECTOR position1 =
				XMVectorSet(
					vertex1.x,
					vertex1.y,
					vertex1.z,
					0.0f
				);

			const XMVECTOR position2 =
				XMVectorSet(
					vertex2.x,
					vertex2.y,
					vertex2.z,
					0.0f
				);

			//========================================================
			// 三角形の辺ベクトル
			//========================================================
			const XMVECTOR edge1 =
				XMVectorSubtract(
					position1,
					position0
				);

			const XMVECTOR edge2 =
				XMVectorSubtract(
					position2,
					position0
				);

			//========================================================
			// UV差
			//========================================================
			const float deltaU1 =
				vertex1.u - vertex0.u;

			const float deltaV1 =
				vertex1.v - vertex0.v;

			const float deltaU2 =
				vertex2.u - vertex0.u;

			const float deltaV2 =
				vertex2.v - vertex0.v;

			//========================================================
			// UV行列の行列式
			//
			// 0に近い場合:
			//   UVが潰れている
			//   3頂点が同じUV
			//   Tangentを一意に求められない
			//========================================================
			const float determinant =
				deltaU1 * deltaV2 -
				deltaU2 * deltaV1;

			const float determinantEpsilon = 0.000001f;

			if (fabsf(determinant) < determinantEpsilon)
			{
				//====================================================
				// UVからTangentを計算できない三角形はスキップ
				// 最終処理で法線からFallback Tangentを作る
				//====================================================
				continue;
			}

			const float inverseDeterminant = 1.0f / determinant;

			//========================================================
			// Tangent
			// edge1 * deltaV2 - edge2 * deltaV1
			//========================================================
			const XMVECTOR tangent =
				XMVectorScale(
					XMVectorSubtract(
						XMVectorScale(edge1, deltaV2),
						XMVectorScale(edge2, deltaV1)
					),
					inverseDeterminant
				);

			//========================================================
			// Bitangent
			// edge2 * deltaU1 - edge1 * deltaU2
			//========================================================
			const XMVECTOR bitangent =
				XMVectorScale(
					XMVectorSubtract(
						XMVectorScale(edge2, deltaU1),
						XMVectorScale(edge1, deltaU2)
					),
					inverseDeterminant
				);

			//========================================================
			// 3頂点へ同じ三角形Tangentを加算
			//========================================================
			const UINT triangleIndices[3] =
			{
				index0,
				index1,
				index2
			};

			for (UINT cornerIndex = 0; cornerIndex < 3; ++cornerIndex)
			{
				const UINT vertexIndex =
					triangleIndices[cornerIndex];

				XMVECTOR tangentSum = XMLoadFloat3(&tangentSums[vertexIndex]);

				tangentSum = XMVectorAdd(tangentSum, tangent);

				XMStoreFloat3(
					&tangentSums[vertexIndex],
					tangentSum
				);

				XMVECTOR bitangentSum =
					XMLoadFloat3(&bitangentSums[vertexIndex]);

				bitangentSum =
					XMVectorAdd(bitangentSum, bitangent);

				XMStoreFloat3(
					&bitangentSums[vertexIndex],
					bitangentSum
				);
			}
		}
	}
	//============================================================
	// 頂点ごとのTangentを正規化・直交化
	//============================================================
	for (UINT vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		vnVertex3D& vertex = mGpuVertices[vertexIndex];

		//========================================================
		// Normal
		//========================================================
		XMVECTOR normal =
			XMVectorSet(
				vertex.nx,
				vertex.ny,
				vertex.nz,
				0.0f
			);

		const float normalLengthSquared =
			XMVectorGetX(XMVector3LengthSq(normal));

		if (normalLengthSquared > 0.000001f)
		{
			normal = XMVector3Normalize(normal);
		}
		else
		{
			//====================================================
			// 法線が不正な場合のFallback
			//====================================================
			normal =
				XMVectorSet(
					0.0f,
					1.0f,
					0.0f,
					0.0f
				);
		}

		//========================================================
		// 三角形から加算したTangent
		//========================================================
		XMVECTOR tangent = XMLoadFloat3(&tangentSums[vertexIndex]);

		const float tangentLengthSquared = XMVectorGetX(XMVector3LengthSq(tangent));

		if (tangentLengthSquared <= 0.000001f)
		{
			//====================================================
			// UVが潰れてTangentを計算できなかった場合
			// Normalと平行でない軸を選び、
			// crossからFallback Tangentを作る
			//====================================================
			const float normalY = fabsf(XMVectorGetY(normal));

			const XMVECTOR referenceAxis =
				(normalY < 0.999f) ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			tangent = XMVector3Normalize(
				XMVector3Cross(
					referenceAxis,
					normal
				)
			);
		}
		else
		{
			//====================================================
			// Gram-Schmidt直交化
			// TangentからNormal方向成分を除去する
			//====================================================
			const float normalDotTangent =
				XMVectorGetX(
					XMVector3Dot(
						normal,
						tangent
					)
				);

			tangent = XMVectorSubtract(
				tangent,
				XMVectorScale(
					normal,
					normalDotTangent
				)
			);

			const float orthogonalLengthSquared =
				XMVectorGetX(XMVector3LengthSq(tangent));

			if (orthogonalLengthSquared > 0.000001f)
			{
				tangent = XMVector3Normalize(tangent);
			}
			else
			{
				//================================================
				// 直交化後に0になった場合のFallback
				//================================================
				const XMVECTOR referenceAxis =
					(fabsf(XMVectorGetY(normal)) < 0.999f) ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

				tangent = XMVector3Normalize(
					XMVector3Cross(
						referenceAxis,
						normal
					)
				);
			}
		}

		//========================================================
		// Bitangent符号を求める
		// calculatedBitangent: NormalとTangentから作ったBitangent
		// accumulatedBitangent: UVから計算したBitangent
		// 向きが同じなら+1、逆なら-1
		//========================================================
		const XMVECTOR calculatedBitangent =
			XMVector3Cross(
				normal,
				tangent
			);

		const XMVECTOR accumulatedBitangent = XMLoadFloat3(&bitangentSums[vertexIndex]);

		const float handednessDot =
			XMVectorGetX(
				XMVector3Dot(
					calculatedBitangent,
					accumulatedBitangent
				)
			);

		const float handedness =
			(handednessDot < 0.0f) ? -1.0f : 1.0f;

		//========================================================
		// GPU頂点へ保存
		//========================================================
		vertex.nx = XMVectorGetX(normal);

		vertex.ny = XMVectorGetY(normal);

		vertex.nz = XMVectorGetZ(normal);

		vertex.tx = XMVectorGetX(tangent);

		vertex.ty = XMVectorGetY(tangent);

		vertex.tz = XMVectorGetZ(tangent);

		vertex.tw = handedness;
	}

	return true;
}

bool ModelBase::BuildGpuSkinnedVerticesWithTangents()
{
	//========================================================
	// 基本確認
	//========================================================
	if (pModelData == nullptr || pModelData->BoneNum == 0 || pModelData->VertexNum == 0)
	{
		return false;
	}


	//========================================================
	// File側Skin Vertex Stride
	//========================================================
	const UINT fileVertexStride = pModelData->VertexStride;


	//========================================================
	// 少なくとも72byte必要
	//========================================================
	if (fileVertexStride < sizeof(vnVertex3DFileSkin))
	{
		OutputDebugStringW(L"[vnModel] Invalid Skin VertexStride.\n");

		return false;
	}


	//========================================================
	// File Vertex先頭
	//========================================================
	const BYTE* sourceVertexBase =
		reinterpret_cast<const BYTE*>(pModelData) + pModelData->VertexAccess;


	//========================================================
	// GPU Vertex
	//========================================================
	mGpuSkinVertices.clear();

	mGpuSkinVertices.resize(pModelData->VertexNum);


	//========================================================
	// Tangent / Bitangent累積値
	// Triangle単位で計算し
	// 共有頂点へ加算して最後に正規化する
	//========================================================
	std::vector<XMFLOAT3> tangentAccum(
		pModelData->VertexNum,
		XMFLOAT3(
			0.0f,
			0.0f,
			0.0f
		)
	);


	std::vector<XMFLOAT3> bitangentAccum(
		pModelData->VertexNum,
		XMFLOAT3(
			0.0f,
			0.0f,
			0.0f
		)
	);


	//========================================================
	// File Vertex → GPU Vertex
	//========================================================
	for (UINT vertexIndex = 0; vertexIndex < pModelData->VertexNum; ++vertexIndex)
	{
		const vnVertex3DFileSkin* source =
			reinterpret_cast<const vnVertex3DFileSkin*>(sourceVertexBase +
				static_cast<size_t>(vertexIndex) *
				static_cast<size_t>(fileVertexStride));


		vnVertex3DSkin& destination =
			mGpuSkinVertices[vertexIndex];


		//====================================================
		// Position
		//====================================================
		destination.x = source->x;
		destination.y = source->y;
		destination.z = source->z;


		//====================================================
		// Normal
		//====================================================
		destination.nx = source->nx;
		destination.ny = source->ny;
		destination.nz = source->nz;


		//====================================================
		// Color
		//====================================================
		destination.r = source->r;
		destination.g = source->g;
		destination.b = source->b;
		destination.a = source->a;


		//====================================================
		// UV
		//====================================================
		destination.u = source->u;
		destination.v = source->v;


		//====================================================
		// Tangentは後で計算
		//====================================================
		destination.tx = 0.0f;
		destination.ty = 0.0f;
		destination.tz = 0.0f;
		destination.tw = 1.0f;


		//====================================================
		// Skinning情報をそのまま維持
		//====================================================
		for (int influence = 0; influence < 4; ++influence)
		{
			destination.skinIndex[influence] = source->skinIndex[influence];
			destination.skinWeight[influence] = source->skinWeight[influence];
		}
	}


	//========================================================
	// Index取得補助関数
	// 16bit / 32bit両対応
	//========================================================
	const BYTE* indexBase =
		reinterpret_cast<const BYTE*>(pModelData) + pModelData->IndexAccess;


	const bool use16BitIndex =
		pModelData->VertexNum <= 0xffff;


	auto GetIndex = [indexBase, use16BitIndex](UINT indexPosition) -> UINT
		{
			if (use16BitIndex)
			{
				const WORD* indices = reinterpret_cast<const WORD*>(indexBase);

				return static_cast<UINT>(indices[indexPosition]);
			}

			const DWORD* indices = reinterpret_cast<const DWORD*>(indexBase);

			return static_cast<UINT>(indices[indexPosition]);
		};


	//========================================================
	// Mesh単位でTriangleを処理
	//========================================================
	const vnModel_MeshData* meshes =
		reinterpret_cast<const vnModel_MeshData*>(reinterpret_cast<const BYTE*>(pModelData) + pModelData->MeshAccess);


	for (UINT meshIndex = 0; meshIndex < pModelData->MeshNum; ++meshIndex)
	{
		const vnModel_MeshData& mesh =
			meshes[meshIndex];


		//====================================================
		// Triangleごと
		//====================================================
		for (UINT localIndex = 0; localIndex + 2 < mesh.IndexNum; localIndex += 3)
		{
			const UINT indexPosition0 = mesh.StartIndex + localIndex;
			const UINT indexPosition1 = mesh.StartIndex + localIndex + 1;
			const UINT indexPosition2 = mesh.StartIndex + localIndex + 2;

			//================================================
			// 実際のVertex番号
			//================================================
			const UINT i0 = GetIndex(indexPosition0) + mesh.StartVertex;
			const UINT i1 = GetIndex(indexPosition1) + mesh.StartVertex;
			const UINT i2 = GetIndex(indexPosition2) + mesh.StartVertex;


			//================================================
			// Safety
			//================================================
			if (i0 >= pModelData->VertexNum || i1 >= pModelData->VertexNum || i2 >= pModelData->VertexNum)
			{
				continue;
			}


			const vnVertex3DSkin& v0 = mGpuSkinVertices[i0];
			const vnVertex3DSkin& v1 = mGpuSkinVertices[i1];
			const vnVertex3DSkin& v2 = mGpuSkinVertices[i2];


			//================================================
			// Triangle Edge
			//================================================
			const XMFLOAT3 edge1 =
			{
				v1.x - v0.x,
				v1.y - v0.y,
				v1.z - v0.z
			};


			const XMFLOAT3 edge2 =
			{
				v2.x - v0.x,
				v2.y - v0.y,
				v2.z - v0.z
			};


			//================================================
			// UV Edge
			//================================================
			const float du1 = v1.u - v0.u;
			const float dv1 = v1.v - v0.v;
			const float du2 = v2.u - v0.u;
			const float dv2 = v2.v - v0.v;

			//================================================
			// UV Matrix determinant
			//================================================
			const float denominator = du1 * dv2 - du2 * dv1;

			//================================================
			// UVが潰れているTriangle
			//================================================
			if (fabsf(denominator) < 0.000001f)
			{
				continue;
			}

			const float inverseDenominator = 1.0f / denominator;

			//================================================
			// Tangent
			//
			// dP / dU
			//================================================
			const XMFLOAT3 tangent =
			{
				(edge1.x * dv2 - edge2.x * dv1) * inverseDenominator,
				(edge1.y * dv2 - edge2.y * dv1) * inverseDenominator,
				(edge1.z * dv2 - edge2.z * dv1) * inverseDenominator
			};


			//================================================
			// Bitangent
			// dP / dV
			//================================================
			const XMFLOAT3 bitangent =
			{
				(edge2.x * du1 - edge1.x * du2) * inverseDenominator,
				(edge2.y * du1 - edge1.y * du2) * inverseDenominator,
				(edge2.z * du1 - edge1.z * du2) * inverseDenominator
			};


			//================================================
			// 3頂点へ累積
			//================================================
			const UINT triangleIndices[3] =
			{
				i0,
				i1,
				i2
			};


			for (int corner = 0; corner < 3; ++corner)
			{
				XMFLOAT3& tangentSum = tangentAccum[triangleIndices[corner]];

				tangentSum.x += tangent.x;
				tangentSum.y += tangent.y;
				tangentSum.z += tangent.z;

				XMFLOAT3& bitangentSum = bitangentAccum[triangleIndices[corner]];

				bitangentSum.x += bitangent.x;
				bitangentSum.y += bitangent.y;
				bitangentSum.z += bitangent.z;
			}
		}
	}


	//========================================================
	// 頂点ごとの最終Tangent作成
	//========================================================
	for (UINT vertexIndex = 0; vertexIndex < pModelData->VertexNum; ++vertexIndex)
	{
		vnVertex3DSkin& vertex = mGpuSkinVertices[vertexIndex];

		//====================================================
		// Normal
		//====================================================
		XMVECTOR normal =
			XMVectorSet(
				vertex.nx,
				vertex.ny,
				vertex.nz,
				0.0f
			);


		normal = XMVector3Normalize(normal);


		//====================================================
		// Tangent
		//====================================================
		XMVECTOR tangent = XMLoadFloat3(&tangentAccum[vertexIndex]);


		//====================================================
		// Gram-Schmidt
		// T = T - N * dot(N,T)
		//====================================================
		tangent =
			XMVectorSubtract(tangent, XMVectorScale(normal, XMVectorGetX(XMVector3Dot(normal, tangent))));


		//====================================================
		// Tangentが作れなかった場合のFallback
		//====================================================
		const float tangentLengthSq =
			XMVectorGetX(XMVector3LengthSq(tangent));


		if (tangentLengthSq < 0.000001f)
		{
			//================================================
			// Normalと平行になりにくい軸を選ぶ
			//================================================
			const float normalY = fabsf(XMVectorGetY(normal));


			const XMVECTOR helperAxis =
				normalY < 0.999f ?
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);


			tangent = XMVector3Cross(helperAxis, normal);
		}

		tangent = XMVector3Normalize(tangent);

		//====================================================
		// Bitangent累積値
		//====================================================
		XMVECTOR bitangent = XMLoadFloat3(&bitangentAccum[vertexIndex]);


		//====================================================
		// Handedness
		// B ≒ cross(N,T) * w
		//====================================================
		const XMVECTOR calculatedBitangent = XMVector3Cross(normal, tangent);

		const float handedness =
			XMVectorGetX(XMVector3Dot(calculatedBitangent, bitangent)) < 0.0f ? -1.0f : 1.0f;

		//====================================================
		// GPU Vertexへ格納
		//====================================================
		XMFLOAT3 tangentFloat = {};

		XMStoreFloat3(&tangentFloat, tangent);

		vertex.tx = tangentFloat.x;
		vertex.ty = tangentFloat.y;
		vertex.tz = tangentFloat.z;
		vertex.tw = handedness;
	}


	return !mGpuSkinVertices.empty();
}

bool ModelBase::IsInsideMainCameraFrustum() const
{
	//========================================================
    // モデルデータがない場合
    // render()側でもチェックする
    //========================================================
	if (pModelData == nullptr)
	{
		return false;
	}

	if (pModelData->BoneNum > 0)
	{
		return true;
	}

	//========================================================
	// ワールド空間Bounding Sphere
	//========================================================
	XMVECTOR worldCenter = XMVectorZero();

	float worldRadius = 0.0f;

	//========================================================
	// Bounds取得に失敗した場合は描画する
	//========================================================
	if (!GetShadowBoundingSphere(worldCenter,worldRadius))
	{
		return true;
	}

	//========================================================
	// カメラのFrustumとBounding Sphereの交差判定
	//========================================================
	return IsSphereVisibleToMainCamera(worldCenter,worldRadius);
}
