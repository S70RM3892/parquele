# Parquele

UE 5.8 / Android 向け 3人称パルクールゲーム。仕様書「パルクールゲーム仕様書（UE5 / Android）」に基づく。

このリポジトリは **Phase 0（環境構築）の設定と Phase 1（移動プロトタイプ）の C++ 実装**を含む。
仕様書の方針どおり、受験期間中は Phase 1 までに留める。

> **重要**：このコードは UE が入っていない環境で書いたため、**UE でのコンパイルはまだ確認していない**。
> UE に依存しない判定ロジック（`ParkourRules.h`）だけは、ネイティブのテストで動作を確認している。
> 初回ビルドでエラーが出たら、エラー全文を持ってきてくれれば直す。

## 実装済み（Phase 1）

| 仕様 | 実装場所 |
| --- | --- |
| 2ボタン文脈式操作（上アクション／下アクション） | `ParkourMovementComponent::UpActionPressed` / `DownActionPressed` |
| 基本移動（歩き 350／走り 650／最高 900 cm/s、0.25 s で走行速度、重力 1.6／2.2、空中制御 0.35） | `GetMaxSpeed`、`ApplySettings`、`UpdateCharacterStateBeforeMovement` |
| ヴォルト・マントル・ウォールクライム（高さ・奥行きで自動選択） | `ParkourRules::ChooseWallAction`、`StartVault` / `StartMantle` / `StartWallClimb` |
| ウォールラン（15〜60°、500 cm/s 以上、最大 1.2 s、重力 25%）とウォールジャンプ（45°、同じ壁で連続不可） | `TryStartWallRun`、`PhysWallRun`、`WallJump` |
| スライド（0.8 s、+150 cm/s、下り坂で加速継続） | `StartSlide`、`UpdateSlide` |
| 受け身（400 cm 以上、着地 0.25 s 前〜着地時）、失敗時 0.4 s 硬直 | `ProcessLanded`、`ParkourRules::EvaluateLanding` |
| レッジ（左右移動、上で登る、下で離す） | `PhysLedgeHang`、`TryClimbUpFromLedge`、`DropFromLedge` |
| 判定：前方カプセルトレース＋下向きトレース、0.05 s 間隔の事前スキャン | `ScanForObstacle`、`TickComponent` |
| コヨーテタイム 0.12 s、ジャンプ先行入力 0.15 s、パルクール先行入力 0.20 s | `ParkourRules::FTimedWindow` |
| 縁吸着アシスト 40 cm、進行方向補正 最大 10° | `TryLedgeGrabAssist`、`RotateTowards2D` |
| FOV 90°→105°（速度連動） | `AParkourCharacter::Tick` |
| デッドゾーン 0.15、走り移行 0.6 | `ParkourRules::EvaluateStick` |
| 落下時の即時復帰、Back 長押しでリスタート | `AParkourCharacter::Respawn`、`AParkourCheckpoint` |
| グレーボックスコース（レベル設計メトリクスの標準寸法） | `AGrayboxCourse` |
| 数値は全て Data Asset | `UParkourMovementSettings` |

全アクションの開始は `OnParkourAction` で通知する。Phase 2 のアニメーション、Phase 3 の音・振動・カメラ揺れはここに繋ぐ。

## セットアップ（Phase 0）

1. Epic Games Launcher で **UE 5.8** をインストールする（Android のターゲットプラットフォームも含める）。
2. C++ のビルド環境を用意する（Windows なら Visual Studio 2022 の「C++ によるゲーム開発」）。
3. `Parquele.uproject` を右クリック →「Generate Visual Studio project files」→ 開いてビルド、またはそのまま `.uproject` を開いてビルドさせる。
4. エディタで **File > New Level > Empty Level** を作って保存し、**Project Settings > Maps & Modes** の Editor Startup Map と Game Default Map に設定する。
   - 空のレベルでもゲームモードがコース・スタート地点・太陽光を自動で置く。
   - Basic テンプレートを使う場合は、コースの床と重なるので `Floor` を削除する。
5. **Platforms > Android > SDK Management（Turnkey）** で SDK と NDK r27c を入れる。
6. Android 端末を USB デバッグで接続し、**Platforms > Android > Launch** で実機起動する。
7. 実機で `stat unit` を入力し、空ステージで 60 fps 出るかを確認する（Phase 0 の完了条件）。60 fps に張り付かない場合は端末側のフレームレート上限設定も確認する。

Android 設定（`Config/DefaultEngine.ini`）：minSdk 26、targetSdk 35（サイドロード APK 用）、arm64、Vulkan＋OpenGL ES フォールバック、横画面。Play 公開時の targetSdk 36 は仕様書どおり要検証。

## 操作

| 操作 | ゲームパッド | キーボード／マウス（PC での確認用） |
| --- | --- | --- |
| 移動 | 左スティック | WASD |
| カメラ | 右スティック | マウス |
| 上アクション | A／LB | Space |
| 下アクション | B／RB | Ctrl／C |
| 自動加速の切り替え | 左スティック押し込み | Shift |
| リスタート | Back 長押し（1 s） | R 長押し |

タッチ操作は Phase 4 で作る。それまで Android 実機ではゲームパッド（Bluetooth）で確認する。

## コース（`AGrayboxCourse`）

X 方向に進む1本道。各区間で1つずつアクションを試せる。

1. ヴォルト（高さ 100 と 70）→ マントル（高さ 200）
2. ウォールクライム（高さ 300）→ 走り幅跳び（隙間 500）
3. 500 cm の落下（受け身）→ スライドでバーをくぐる（床から 110 cm）
4. 穴の上のウォールラン（壁 A に 15〜60° で入り、壁蹴りで壁 B へ）
5. ゴール（画面左上にタイム表示）

落ちたら直前のチェックポイントに戻る。

## 調整

**Content Browser > 右クリック > Miscellaneous > Data Asset > ParkourMovementSettings** で作成し、キャラクターの `CharacterMovement > Parkour > Settings` に設定する。
未設定なら仕様書の初期値がそのまま使われる。実行中に値を変えたら `ApplySettings` を呼ぶ。

`UParkourMovementSettings` のコメントに「仮値」とある項目は、仕様書に数値がないため実装上置いた値。主なもの：

- 最高速度に達するまでの時間 2.0 s
- マントル 0.45 s（高速時 0.28 s、600 cm/s 以上）、ウォールクライム 0.55 s
- ウォールジャンプの速さ 750 cm/s、ウォールラン開始の上昇速度 100 cm/s
- スライド開始の最低速度 550 cm/s、平地の減速 300 cm/s²、上限 1100 cm/s
- ぶら下がりの左右移動 150 cm/s

## テスト

UE に依存しない判定ロジックは UE なしでテストできる（GitHub Actions でも自動実行）。

```sh
mkdir -p build
g++ -std=c++20 -Wall -Wextra -I Source/Parquele/Public Tests/ParkourRulesTests.cpp -o build/rules_tests
./build/rules_tests
```

## 構成

```
Parquele.uproject             UE 5.8、Enhanced Input・Motion Warping を有効化
Config/                       描画・Android・入力の設定
Source/Parquele/Public/
  ParkourRules.h              UE 非依存の判定ロジック（テスト対象）
  ParkourMovementSettings.h   全ての調整値（Data Asset）
  ParkourMovementComponent.h  パルクール移動（CMC＋カスタム Movement Mode）
  ParkourCharacter.h          入力・カメラ・復帰
  ParkourGameMode.h           既定のゲームモード、テスト環境の自動配置
  GrayboxCourse.h             グレーボックスコース
  ParkourCheckpoint.h         チェックポイント・ゴール
Tests/ParkourRulesTests.cpp   ネイティブテスト
```

## 今後（受験後）

- **Phase 2**：Game Animation Sample 導入、`OnParkourAction` からモンタージュ＋Motion Warping を再生、Motion Matching、足 IK。
- **Phase 3**：アート、ライティングのベイク、速度連動の放射状ブラー・スピードライン、風切り音、振動、フローゲージ（最高速 +15%）、パーフェクト判定。
- **Phase 4**：タッチ UI（浮動スティック・ボタン配置変更・パッド接続で自動非表示）、ゴースト、ランク。

ネットワーク対戦は想定していないため、移動の予測同期（SavedMove）は実装していない。
