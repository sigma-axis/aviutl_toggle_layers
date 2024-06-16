# レイヤー一括切り替え AviUtl プラグイン

複数レイヤーの表示 / 非表示状態をドラッグ操作で一括に切り替えられるようになるなど，レイヤーに対するマウス操作を拡充するプラグイン．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_toggle_layers/releases)

![一括切り替えのデモ](https://github.com/sigma-axis/aviutl_toggle_layers/assets/132639613/dc4cd6d3-e295-47d8-be84-5ae6653b494f)

https://github.com/sigma-axis/aviutl_toggle_layers/assets/132639613/63deb25e-f931-49f6-92c0-a3dbc9b4ea90

https://github.com/sigma-axis/aviutl_toggle_layers/assets/132639613/0fc36ad1-d997-4882-9916-8b49dd24791c


## 動作要件

- AviUtl 1.10 + 拡張編集 0.92

  http://spring-fragrance.mints.ne.jp/aviutl

  - 拡張編集 0.93rc1 等の他バージョンでは動作しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

- **(推奨)** patch.aul r43 謎さうなフォーク版60 以降

  https://github.com/nazonoSAUNA/patch.aul

  - レイヤーのドラッグ移動操作とレイヤー名変更を繰り返した後「元に戻す」を行うとレイヤー名がおかしくなる不具合が修正されます．

## 導入方法

以下のフォルダのいずれかに `toggle_layers.auf` と `toggle_layers.ini` をコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ
1. (2) のフォルダにある任意のフォルダ


## 使い方

初期設定で以下の操作ができます (`Ctrl`, `Shift`, `Alt` の修飾キーは指定されているもの以外を離している必要があります):

- タイムライン左側のレイヤーボタンを左クリックドラッグで操作すると，カーソルの通った範囲が全て表示 / 非表示状態に切り替わります．

- `Shift` + 左クリックドラッグで，カーソルが通った範囲のロック / ロック解除が切り替わります．

- `Ctrl` + 左クリックドラッグで，カーソルが通った範囲にあるレイヤー上のオブジェクトを選択 / 選択解除します．

- `Alt` + 左クリックで，カーソル位置のレイヤー名を変更するダイアログが表示されます．

- 左ダブルクリックで，他のレイヤーを全表示したり非表示にしたりできます（右クリックメニューの「他のレイヤーを全表示/非表示」を実行）．

[設定ファイル](#設定ファイル)を編集すると以下の操作が使えます．修飾キーとドラッグ / ダブルクリックドラッグの組み合わせに割り当ててください:

- カーソルが通った範囲の「座標のリンク」を切り替えられます．

- カーソルが通った範囲の「上のオブジェクトでクリッピング」を切り替えられます．

- レイヤーの内容を丸ごと上下にドラッグ移動できます．


## 設定ファイル

`toggle_layers.ini` ファイルをテキストエディタで編集することで，キーの組み合わせとマウス操作に対する動作を割り当てるなどの設定ができます．詳しくはファイル内のコメントに設定方法の記述があるのでそちらをご参照ください．

- ドラッグでオブジェクト選択する機能は拡張編集の仕様上の理由から，`Ctrl` キーを含む組み合わせでのみ有効です．


### TIPS

- ドラッグ状態でタイムラインウィンドウの上端・下端をマウスカーソルが超えるとスクロールします．

- `Ctrl`, `Shift`, `Alt` はドラッグ開始時に押していれば，ドラッグの最中に離しても OK です．ただし `Ctrl` を離すとオブジェクト選択は一旦解除されます．

- ダブルクリックに操作を割り当てている場合でもシングルクリックによる動作が実行されます．シングルクリックによる動作を無効化したい場合は，設定ファイルを編集して当該項目に「本当に何もしない」を割り当ててください．


### 既知の不具合

- レイヤーのドラッグ移動とレイヤー名変更を繰り返した後「元に戻す」をすると，レイヤー名がきちんと元に戻らなかったり全く別の名前になったりすることがあります．[patch.aul r43 謎さうなフォーク版60 以降](https://github.com/nazonoSAUNA/patch.aul) の導入で修正されます．

## 改版履歴

- **v1.61** (2024-06-08)

  - ルート以外のシーンでレイヤーを丸ごとドラッグした場合，レイヤー名などの設定が移動せず，代わりにルートシーンで移動が起こっていたバグを修正．

- **v1.60** (2024-06-07)

  - ダブルクリックやダブルクリックドラッグにも操作を割り当てられるように拡張．

    設定ファイルに `[double_click]` のセクションを追加しました．割り当ての変更はこの項目でできます．設定を引き継ぐ場合は同梱の `.ini` ファイルを参考にして追記してください．

    - 初期状態で「他のレイヤーを全表示/非表示」が割り当てられています．

  - マウスを表示範囲外までドラッグ移動した場合の自動スクロールの挙動を改善．マウスを動かさなくてもスクロールが継続するように．

  - 自動スクロールの有効/無効やスピードを設定できるように．

    設定ファイルに `[scroll]` のセクションを追加しました．設定の変更はこの項目でできます．設定を引き継ぐ場合は同梱の `.ini` ファイルを参考にして追記してください．

- **v1.50** (2024-06-06)

  - レイヤーの内容を丸ごと上下にドラッグ移動する機能を追加．

  - 設定ファイルの仕様を微変更．ファイル冒頭の `[key_combination]` を `[drag]` に変更しました．

    **v1.40 から設定を引き継ぐ場合，この冒頭部分を書き変えてください．**

  - ドラッグ中にショートカットキーコマンドなどで編集状態が変わってしまうと「元に戻す」が正しく機能しなくなっていたのを修正．編集状態の変更を見つけた場合ドラッグ操作を中止するよう変更．

  - 「他のレイヤーを全表示/非表示」で編集画面が更新していなかったのを修正．

- **v1.40** (2024-06-04)

  - 各種修飾キーの組み合わせとレイヤー操作との対応を `.ini` ファイルで設定できるように．

  - レイヤーに対して行える操作を追加:

    1.  「座標のリンク」の切り替え
    1.  「上のオブジェクトでクリッピング」の切り替え
    1.  他のレイヤーを全表示・非表示

- **v1.30** (2024-06-04)

  - `Alt` + クリックでレイヤー名変更のダイアログを表示する機能を追加．

- **v1.20** (2024-05-31)

  - `Ctrl` + ドラッグでレイヤー上のオブジェクトを選択 / 選択解除する機能を追加．

- **v1.12** (2024-05-30)

  - レイヤー番号の範囲チェック修正．

- **v1.11** (2024-05-30)

  - レイヤーをロック / ロック解除するごとに編集画像が不必要に再描画されていたのを修正．

- **v1.10** (2024-05-28)

  - `Shift` + ドラッグでレイヤーをロック / ロック解除する機能を追加．

- **v1.02** (2024-04-26)

  - コード整理，動作も少し改善．

- **v1.01** (2024-04-26)

  - ドラッグ中にカーソルをウィンドウの外に大きく外しても，スクロール範囲外のレイヤーは影響を受けないように変更．

- **v1.00** (2024-04-26)

  - 初版．


## ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (C) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org/


#  Credits

##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#  連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://twitter.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
