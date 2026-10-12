# us2jis

Waveshare RP2350-USB-A向けのUSBキーボード変換ファームウェアです。
USB-C側ではPCにUSB HIDキーボードとして接続し、USB-A側ではPIO USB HIDホストとしてキーボードを受け付けます。
ブートプロトコルのキー入力をPCへ転送し、Windowsの日本語106/109キーボード配列に合わせてUS配列の記号キーを変換します。
左右Altの単独押しではIME操作キーを送信し、CDC経由の設定更新にも対応します。

## ビルド

必要なものはPico SDK 2.2.0、ARM用ツールチェーン、CMake、Ninjaです。Pico-PIO-USB 0.7.2は `third_party/Pico-PIO-USB` に固定しています（上流コミット `3c1eec341a5232640e4c00628b889b641af34b28`）。
TinyUSBはPico SDKに含まれるものを使用します。

通常は次のコマンドでビルドします。

```sh
make build
```

初回はPython仮想環境 `.venv` を作成し、`scripts/requirements.txt` の依存パッケージをインストールします。
ビルド成果物は `build/us2jis.uf2` です。
このUF2にはファームウェアと `config/keymap.yml` から生成した設定の両方が含まれます。
BOOTSELドライブへこのファイルをコピーすると、両方をまとめて書き込めます。

GNU Makeの主なコマンドは次のとおりです。

| コマンド | 内容 |
| --- | --- |
| `make` / `make build` | ファームウェアとYAML設定を含む `build/us2jis.uf2` を生成 |
| `make firmware` | `make build` と同じ |
| `make keymap-uf2` | ファームウェアを書き換えず設定だけ更新する `build/keymap.uf2` を生成 |
| `make deps` | `.venv` を作成し、Python依存パッケージを準備 |
| `make clean` | `build/` を削除 |

Makefileは `~/.pico-sdk` 配下のARM GCC、Ninja、picotoolを自動検出します。
別の場所にインストールしている場合は、`PICO_TOOLCHAIN_PATH`、`NINJA`、`PICOTOOL_FETCH_FROM_GIT_PATH` で指定できます。

フラッシュ容量2 MiBのうち、末尾8 KiBを4 KiBずつ2つの設定スロットとして予約しています。ファームウェアのリンク領域はこの領域を避けます。
CMakeを直接実行する場合は、先に `make deps` を実行してPython環境を準備してください。
直接のCMakeビルドはファームウェアUF2のみを生成するため、設定とまとめた書き込み用イメージは `make build` で作成してください。

## ハードウェアの準備

Waveshareの回路図では、USB-AのD+がGP12、D−がGP13に接続されています。
メーカーの説明では、USB-A側でホットプラグを行うには抵抗 **R13を取り外す必要があります**。
取り外すとUSB-Aをデバイスとして使えなくなります。

- [メーカー回路図（PDF）](https://files.waveshare.com/wiki/RP2350-USB-A/RP2350-USB-A.pdf)
- [メーカーFAQ](https://www.waveshare.com/wiki/RP2350-USB-A)

RP2350-USB-AのUSB-C端子をPCへ、USB-A端子をHHKBへ接続します。
書き込み時はBOOTボタンを押しながらUSB-Cを接続し、マウントされたRP2350ドライブへ `build/us2jis.uf2` をコピーします。

## キー設定

キー変換とキーボード動作の設定元は `config/keymap.yml` です。
`make build` はこのYAMLからファームウェアの初期設定と設定スロットの内容を生成し、単一の `build/us2jis.uf2` にまとめます。
YAMLを変更したら再度 `make build` を実行し、生成されたUF2を書き込んでください。

ファームウェアを書き換えずに設定だけ更新したい場合は、次のコマンドでデータ専用UF2を作成します。

```sh
make keymap-uf2
```

生成される `build/keymap.uf2` をBOOTSELドライブへコピーすると、予約済みの設定領域だけが更新されます。
YAMLファイルそのものはBOOTSELドライブへ書き込めません。

各マッピングは入力HID Usageをキーにし、`plain` と `shifted` それぞれに出力Usageと出力Shift状態を指定します。
Alt単独押しの動作には `ime_on`、`ime_off`、`none` を指定でき、初期タイムアウトは200 msです。
左右Altの割り当て、IMEキーのUsage、記号を含むキー変換はすべてYAMLで管理します。

YAMLの `schema_version` は1、デバイス設定データの形式はv2です。
設定領域に有効な保存済み設定がある場合は、ファームウェアの初期設定より保存済み設定が優先されます。
実行中のデバイスへCDCで設定を送るCLIとして `scripts/config_tool.py` も利用できます。

## テストと制約

Python依存パッケージを準備してから、ホスト側の設定チェックを実行します。

```sh
make deps
./scripts/run_host_tests.sh
```

使用するWindows環境で記号変換表とIME動作を確認してください。
Microsoftの日本語キーボード経路では `LANG1`（Usage `0x90`）が `VK_IME_ON`、`LANG2`（Usage `0x91`）が `VK_IME_OFF` に対応します。
