# CSVマップの作り方

## ステージエディタで編集する

CSVを手打ちしなくても、ゲームを起動したまま絵として編集できます。
`Window > Application > Stage` を選ぶと Inspector に「Stage」タブが出ます。
ImGuiが入るDebug・Developmentビルドだけの機能です。Releaseには含まれません。

タブは3枚あります。

- **区画を編集**: 開くCSVを選び、マス目を塗ります。
  左ドラッグで選択中のチップ、右ドラッグで空白に戻します。
  ホイールクリックでその場のチップを吸い取り、マップの上では数字キー `0`〜`4` でも切り替わります。
  `Ctrl+Z`（マップの上にカーソルがあるとき）とボタンでドラッグ1回ぶん戻せます。
  保存先はカレントディレクトリ基準なので、デバッグ実行なら `Project/Application/Assets/Maps/` の
  ファイルがそのまま書き換わります。
- **実行中へ反映**: 編集中の区画を、いま動いているマップのどこへ置くかを決めて書き込みます。
  シーンを読み直さずに地形が変わります。「生成済みの先端へ」を押すと、
  走っている列車のすぐ前に当たる区画番号が入ります。
  **書き換えるのは実行中のマップだけです。CSVは「区画を編集」タブで別に保存してください。**
- **エリア構成**: エリアの追加・削除と、エリアに属する区画CSVの登録を編集し、
  `stage_project.json` へ保存します。

`stage_project.json` はエディタが扱う構成表で、**ゲーム本体は読みません**。
エリアの増減をゲームへ効かせるときは、このタブの下にあるコードをコピーして
`GameScene::OnInitialize` の `mapSettings` と差し替えてください。

## 生成方式の切り替え

`Project/Application/Src/Scenes/GameScene/GameScene.cpp` の `mapSettings` を設定します。

- `RandomCsvPool`（現在の設定）: 選択中の名前付きプール内のCSVから均等にランダム選択し、X正方向へ区画をつなぎます。同じCSVが連続することもあります。
- `FixedCsv`: `fixedCsvPath` のCSVを原点から1回だけ使用します。CSVの終端以降はVoidです。
- `Procedural`: 従来のチップ単位のランダム生成です。

```cpp
mapSettings.mode = GameComponents::MapGenerationMode::RandomCsvPool;
mapSettings.csvChunkSizeX = 10;
mapSettings.csvPools = {
    { "Area1", {
        "Application/Assets/Maps/Areas/Area1/chunk_01.csv",
        "Application/Assets/Maps/Areas/Area1/chunk_02.csv",
        "Application/Assets/Maps/Areas/Area1/chunk_03.csv",
    } },
    { "Area2", {
        "Application/Assets/Maps/Areas/Area2/chunk_01.csv",
        "Application/Assets/Maps/Areas/Area2/chunk_02.csv",
        "Application/Assets/Maps/Areas/Area2/chunk_03.csv",
    } },
};
mapSettings.initialCsvPoolName = "Area1";
mapSettings.fixedCsvPath = "Application/Assets/Maps/fixed.csv";
// mapSettings.randomSeed = 123; // 配列の選択順を再現したい場合
```

固定マップを使う場合は `mode` を `MapGenerationMode::FixedCsv` に変えてください。
CSVはシーン生成時に一度だけ読み込みます。編集後はリトライまたはシーン再読み込みで反映されます。
パスは実行時のカレントディレクトリ基準です（デバッグ時はProject、配布時は実行ファイルのディレクトリ）。
Assetsは既存のビルド後処理で実行ファイル側へコピーされます。

## エリアプールの構成と切り替え

**1エリア用の複数の区画CSVをまとめたものが1プール**です。そのプールをエリア数分用意します。
地面用・水用・資源用といったチップ種別の分類ではありません。どの区画CSVにも全種類のチップを配置できます。

```text
Areas/
├─ Area1/                 ← エリア1用プール
│  ├─ chunk_01.csv
│  ├─ chunk_02.csv
│  └─ chunk_03.csv
└─ Area2/                 ← エリア2用プール
   ├─ chunk_01.csv
   ├─ chunk_02.csv
   └─ chunk_03.csv
```

初期設定は `Area1` です。エリア1中はArea1の3枚からランダムに区画をつなぎ、
`Area2` へ切り替えるとArea2の3枚からランダムに区画をつなぎます。別エリアのCSVを混ぜて抽選しません。
サンプルでは各エリアに異なる配置の3区画を用意しています。
CSVを追加するときは、そのエリアのフォルダーへ置き、`csvPools` の該当エリアのリストへ登録してください（フォルダーの自動走査はしません）。
エリアを追加する場合は、`Area3` など新しいプール定義を追加します。
固定CSVモードではこれらのプールを使用しません。

- 初期選択: `initialCsvPoolName` を変更します。空文字なら最初のプールです。
- 実行中のコード: `mapGeneratorComponent->SelectCsvPool("Area2");` と呼びます。
- 実行中のUI: ImGuiが有効なビルドで `MapGenerator` オブジェクトの「マップ生成」インスペクターを開き、「エリアプール」で選択します。

距離や駅到達などによるエリア切替の条件はまだ設定していません。ゲーム進行側から切替APIを呼ぶ構成です。

`SelectCsvPool` は成功時に `true`、未登録名や固定/従来生成モードでは `false` を返します。
無効な切替要求は現在の選択を変えません。初期設定の名前が不明な場合は、勝手に別プールを選ばずVoidで開始します。
`GetCsvPoolNames()` で一覧、`GetSelectedCsvPoolName()` で次区画用の選択、`GetActiveCsvPoolName()` で直前または生成途中の区画に使用したプールを取得できます。

**既に生成した地形は変更しません。** 区画の途中で切り替えると残りは元のCSVから生成し、次の区画境界から新しいプールを使います。
描画用に先読み生成済みの区画も保持されるため、カメラ直前の地形がすぐ変わるとは限りません。
インスペクターには切替が反映される次の区画開始Xを表示します。
次の区画が生成されるまでに何度も選択した場合は最後の選択が有効です。同じプールの再選択では区画や乱数をリセットしません。
CSVが0枚の名前付きプールは有効な選択として扱い、区画全体をVoidにします。
実行中の選択は保存されず、リトライ時は `initialCsvPoolName` に戻ります。

プール名は大文字・小文字を区別します。名前なし定義・重複名の2件目以降は警告して無視します。
従来の `csvPoolPaths` だけを使う設定も `Default` プールとして動作します。
`csvPools` が指定されている場合はそちらが優先され、従来のパスは混ぜません。

## CSVの配置とチップ

ヘッダーなし、UTF-8（BOMあり・なし両対応）、カンマ区切りです。
**列がX、行がZ**で、左上が区画内の `(x=0,z=0)` です。右・下に行くほど座標が増えます。
各セルは次の数字または英語名を使います。英語名は大文字・小文字を区別せず、前後の空白は無視します。

| 数字 | 名前 | 内容 |
| --- | --- | --- |
| 0 | Void | 空白・レール設置不可 |
| 1 | Water | 水場・レール設置コスト2 |
| 2 | Ground | 地面・レール設置コスト1 |
| 3 | Station | 駅 |
| 4 | Resource | 資源 |

例:
```csv
Ground,,Water
2,Station
```

この例では `(1,0)` と `(2,1)` がVoidです。設定されたZ方向サイズまで、残りの行もVoidで埋まります。
セルを引用符で囲むこともできます。セル内改行は非対応です。

## データ不足の扱い

- 空セル、末尾の空セル、短い行の不足列、空行、不足行はすべてVoidです。空行を詰めて読み込みません。
- 不明な文字列・数値は警告を出してVoidになります。
- ファイルが存在しない・読めない・空の場合もVoidになります。
- ランダムプールが空の場合はワールド全体をVoidで生成します。
- ランダム生成で読めないCSVが選ばれても、その区画を飛ばさずVoidで埋めます。
- CSV方式では駅・資源・水場などを後から自動配置しません。CSVの配置をそのまま使用します。

ランダム区画は `csvChunkSizeX × mapSizeZ` マスです（現在は10×9）。
小さいCSVはVoidで埋め、大きいCSVのはみ出した部分は切り捨てます。
`csvChunkSizeX = 0` は1に補正します。
固定CSVのX幅は有効な行の最大列数、Z方向は `mapSizeZ` までです。
描画範囲などからCSVの外を要求されてもVoidで埋めます。固定CSVを繰り返すことはありません。

サンプルは初期列車位置 `(3,4)` を地面にしています。
独自CSVでも初期位置と通行可能な経路を用意してください。開始位置を自動でGroundに置き換える処理は入れていません。
固定CSVの終端によるゲームクリア判定は追加しておらず、既存の列車終端処理が使われます。
