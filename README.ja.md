# CARDPUTER StudyTimer

M5Stack Cardputer 向けの、落ち着いて使えるシンプルな学習タイマーです。

![CARDPUTER StudyTimer 全体像](images/overview.jpeg)

このアプリの主役はタイマーで、ログは自動で残る補助機能です。想定している日常の流れは次のとおりです。

```text
電源オン
前回のタイマーが表示される
Enter を押す
学習開始
```

## コンセプト

CARDPUTER StudyTimer は、いわゆる生産性アプリではなく、100 円ショップのキッチンタイマーのように気軽に使える体験を目指しています。

あえて単体完結の設計にしています。

- Cardputer 単体で完結
- 追加 UNIT モジュール不要
- MQTT 不要
- Home Assistant 連携なし
- クラウド機能なし
- ゲーミフィケーションなし
- 科目名入力なし

Wi-Fi と NTP は、必要な場合だけ裏で動く補助機能です。Wi-Fi 接続、NTP 同期、SD へのログ保存のいずれかに失敗しても、タイマー自体は問題なく使えます。

## 機能

- 前回設定した時間をすぐ表示する高速起動
- プリセット: 5 / 10 / 15 / 25 / 45 分
- 1 〜 99 分の手動入力
- 大きな残り時間表示
- 画面四辺のセグメントによる進捗表示
- 日本語 / 英語 UI
- サウンドモード: 静音 / 普通の音 / うるさい
- 非充電時にバッテリー残量 20% 以下で視覚警告
- 完了セッションを microSD の CSV に自動記録
- バックグラウンドでの Wi-Fi / NTP 時刻同期
- 起動時に Wi-Fi や NTP を待たない

## 操作

### 待機状態

- `Enter`: 前回設定のタイマーを開始
- `1` 〜 `5`: プリセット開始
- `0`: カスタム時間入力へ
- `S`: 設定を開く

### カスタム入力

- 数字キー: 分を入力
- `Enter`: 開始
- `Del`: 1 桁削除（空の状態で押すと待機状態に戻る）

### 実行中 / 一時停止中

- `Enter`: 一時停止 / 再開
- `Del`: リセット確認画面を開く

### 設定

- `1`: 日本語
- `2`: 英語
- `3`: 静音
- `4`: 普通の音
- `5`: うるさい
- `Fn + ;`: 左へ移動
- `Fn + ,`: 下へ移動
- `Fn + .`: 上へ移動
- `Fn + /`: 右へ移動
- `Enter`: 選択項目を適用
- `Del`: 待機状態に戻る

![設定画面](images/settings.jpeg)

## 設定ファイル

`config.example.h` を `config.h` としてコピーし、編集は `config.h` だけに行ってください。

```cpp
#define WIFI_ENABLED true
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define NTP_SERVER_3 "time.cloudflare.com"
#define GMT_OFFSET_SECONDS 32400
#define DAYLIGHT_OFFSET_SECONDS 0
```

実際の Wi-Fi 認証情報はコミットしないでください。`config.h` は .gitignore で除外されています。

Wi-Fi を設定していなくてもタイマーは動作し、完了セッションは `unsynced` として記録されます。

## CSV ログ

完了したセッションは次のファイルに追記されます。

```text
/study_log.csv
```

現在の CSV 形式は次のとおりです。

```csv
date,time,sync_status,duration_min,completed,input_type
2026-05-19,13:32:52,synced,25,1,preset4
2026-05-19,14:10:12,synced,45,1,custom
,,unsynced,15,1,preset3
```

各列の意味:

- `date`: `YYYY-MM-DD`（NTP 未同期時は空）
- `time`: `HH:MM:SS`（NTP 未同期時は空）
- `sync_status`: `synced` または `unsynced`
- `duration_min`: タイマー時間（分単位）
- `completed`: 完了セッションは `1`
- `input_type`: `preset1` 〜 `preset5`、または `custom`

日時を 1 つのタイムスタンプ文字列にまとめず分割しているのは、将来的に Cardputer 上で日次集計をしやすくするためです。

## ビルド

対象ボード:

```text
m5stack:esp32:m5stack_cardputer
```

必要な Arduino ライブラリ:

- M5Cardputer
- M5Unified
- M5GFX

コンパイルコマンド例:

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer .
```

## リポジトリ内容

この公開向けリポジトリでは、意図的に次のファイルだけを追跡しています。

- `CARDPUTER-StudyTimer.ino`
- `README.md`
- `README.ja.md`
- `config.example.h`
- `images/`
- `.gitignore`

ローカルメモ、自動生成ドキュメント、個人用設定、エディタ関連ファイルは追跡対象外です。
