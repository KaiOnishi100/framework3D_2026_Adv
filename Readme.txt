Atmospheric Escape

懐中電灯を手に、追跡者のいる屋敷から脱出する3D探索ホラーゲームです。
本館では、暗い廊下を抜けて電源室を探します。明かりが戻ると探索できる場所が広がり、
中庭の先にある別館では、残された文書が設備を動かす手掛かりになります。

制作者　　大西 快
希望職種　ゲーム開発／エンジン開発
制作期間　エンジン・ツール 5か月半／ゲーム 1か月半
開発環境　Windows、C++20、DirectX 12、HLSL
動作環境　Windows（64ビット）、DirectX 12対応GPU（機能レベル11_0以上）
操作　　　キーボード・マウス、またはXInput対応ゲームパッド

【ビルド済みのゲームを遊ぶ】

1. vn_framework3D_2026_Adv.sln があるフォルダーを開きます。
2. 次にgamefileを開きます。
3. vn_framework3D_2026_Adv.exeをダブルクリックで起動します。
    ゲームは起動時の作業フォルダーから data と shader を読み込みます。
4. タイトルからニューゲームを選び、正面玄関から屋敷に入ります。
   懐中電灯で足元を照らし、マップで現在地を確かめながら電源室を探してください。

【基本操作】

※視点変更はできますが、このゲームは1人称推奨です。

キーボード・マウス
  移動　         W・A・S・D　　　   視点　          マウス
  走る　         Shiftを押し続ける　しゃがみ切替　  C
  調べる・拾う　 E　　　　          懐中電灯　      F
  視点モード切替 V　　              マップ　        M
  インベントリ　 I　　　　          メニュー　      Tab

ゲームパッド（XInput）
  移動　         左スティック　　　   視点　        右スティック
  走る　         RBを押し続ける　　   しゃがみ切替　B
  調べる・拾う　 A　　　　            懐中電灯　    X
  視点モード切替 右スティック押し込み
  マップ　       左スティック押し込み
  インベントリ　 BACK　　　           メニュー　    START

【見どころと担当】

※学校提供のC++フレームワークとはvn_○○○○というものが命名されているファイルです。

学校提供のC++フレームワークを基に、描画基盤と素材変換ツールを拡張し、
屋敷の探索進行、パズル、追跡AI、入力、UI、セーブを実装しました。

・ゲーム進行とAI
  別館では、文書に残された手順をもとに、離れた部屋の設備を操作します。
  追跡者は物音がした場所と最後にプレイヤーを見た場所を別々に記録し、
  巡回・調査・追跡・捜索・攻撃を切り替えます。
  部屋や目標はCSVで定義し、扉や操作盤は共通のインターフェースで扱います。

・材質と照明
  Forward PBR、法線マップ、ORMマップ、IBLで材質と照明を表現しました。
  IBLには拡散照明用マップ、事前フィルタ済み環境マップ、BRDF LUTを使用しています。
  SSGIで周囲の面から届く間接光を、ボリュームフォグで空間に広がる光を表現しました。
  HDRとトーンマッピング、モーションベクトルを使ったモーションブラー、Bloom、FXAAにも対応します。

・影と光源の制御
  平行光には静的・動的レイヤーを分けたカスケードシャドウを使用しました。
  点光源・スポットライトの影にも対応し、部屋、距離、優先度に応じて光源を選びます。

・素材とロード
  VNMモデルとDDSテクスチャをCPU側で先読みし、配置の生成を複数フレームに分け、
  GPUへのテクスチャ転送をまとめました。モデルデータ、GPUの頂点・インデックス、
  テクスチャは複数の配置で共有し、所有権を管理しています。

【ソースからビルドする】

1. Visual Studio 2022の「C++によるデスクトップ開発」、MSVC v143、Windows SDKを用意します。
   Gitで取得した場合は、Git LFSでDDS素材も取得します。
2. vn_framework3D_2026_Adv.sln を開き、NuGetパッケージを復元します。
   ゲームはWinPixEventRuntimeと、構成に対応するDirectXTexのx64ライブラリを使用します。
3. x64のReleaseまたはDebugを選んでビルドします。ゲームプロジェクトの
   「プロパティ」→「デバッグ」→「作業ディレクトリ」を $(ProjectDir) に設定して起動します。

【ポートフォリオと主なソース】

以下のパスは、ソリューションがあるフォルダーを基準にしています。

作品ポートフォリオ　docs/AtmosphericEscape_Portfolio_ver_1.02.pdf
探索とパズル　　　　vn_framework3D_2026_Adv/public/world/AtmosphericEscapeWorld/
目標・文書・部屋　　vn_framework3D_2026_Adv/data/csv/world/gameplay/
追跡者の行動と知覚　vn_framework3D_2026_Adv/public/object/AtmosphericEscape_Object/Enemy/pursuerMonster.cpp
材質と照明　　　　　vn_framework3D_2026_Adv/shader/ps3d/
光源の選択　　　　　vn_framework3D_2026_Adv/framework/light/AtmosphericEscape/AtmosphericEscapeLightBudget.cpp
素材変換ツール　　　tools/AssetConverter/AssetConverter/AssetConverter.cpp

【素材制作について】

音源・モデルの制作には生成AIを活用しました。キャラクターにはMixamo、
静的モデルの制作にはBlender Pythonを使用しています。