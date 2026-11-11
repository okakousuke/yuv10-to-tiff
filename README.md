# yuv10-to-tiff

[![PayPalで開発を支援する](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://paypal.me/okako1124)

ITE/ARIBの仕様を題材にした、10bit YCbCr 4:2:2 RAWを8bit RGBの非圧縮TIFFへ変換する最小サンプルです。

これは過去に使用していた`ConvertYUV10.exe`や`YUV10toTIFF.dll`のソースコードではありません。記事の説明と再現テストのために、新規に書き起こした公開用コードです。実データ、元バイナリ、製品固有の仕様は含みません。

## 対応する入力

- 1920×1080（幅と高さはコマンドラインで変更可能）
- packed 10bit、4バイトに3サンプル
- YCbCr 4:2:2、成分順`Cb0, Y0, Cr0, Y1`
- 1行ごとのパディングなし
- BT.709 limited range

4バイトから取り出した10bitコード値をBT.709 limited-rangeとして正規化し、RGBへ変換します。出力TIFFは8bit/sample、RGB、非圧縮、chunkyです。

## ビルド

```sh
c++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -o yuv10_to_tiff yuv10_to_tiff.cpp
```

CMakeでもビルドできます。

```sh
cmake -S . -B build
cmake --build build --config Release
```

## 使い方

```sh
./yuv10_to_tiff input.yuv10 output.tif 1920 1080
```

入力サイズが、`width * height * 16 / 6`バイトと一致しない場合はエラーにします。この形式では、4:2:2の6画素分（Yが6個、CbとCrが3個ずつ）を、16バイトへ格納します。1920×1080なら5,529,600バイトです。

## テスト用RAWの作成

小さなグレースケール入力を生成する例です。生成したデータは説明用の架空データで、実映像ではありません。

```sh
python3 make_test_yuv10.py test.yuv10 6 2
./yuv10_to_tiff test.yuv10 test.tif 6 2
```

`make_test_yuv10.py`は、10bit値を上位ビットから3個ずつパックするだけの依存なしスクリプトです。幅×高さ×2成分が3の倍数になるサイズを指定してください。入力成分順やunpackの確認に使います。

## 自動テスト

`test_reference.py`は、既知のパッキング、黒・基準白・無彩色中間値、TIFFタグを架空データだけで確認します。

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## 公開範囲

このディレクトリのコードは、記事の再現用に新規作成したサンプルです。元の業務コード、元のGUI、DLL、実データを復元・公開したものではありません。

## ライセンス

MIT License（`LICENSE`）
