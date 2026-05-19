# CARDPUTER StudyTimer

M5Stack Cardputer 向けの、落ち着いて使える低摩擦な学習タイマーです。

タイマーが主機能であり、ログは受動的な副作用です。想定する日々の流れは次のとおりです。

```text
電源オン
前回のタイマーが表示される
Enter を押す
学習開始
```

## コンセプト

CARDPUTER StudyTimer は、生産性アプリというより、100 円ショップのシンプルなタイマーに近い体験を目指しています。

意図的にスタンドアロンです。

- Cardputer 単体で完結
- 追加 UNIT モジュール不要
- MQTT 不要
- Home Assistant 連携なし
- クラウド機能なし
- ゲーミフィケーションなし
- 科目名入力なし

Wi-Fi と NTP は任意のバックグラウンド補助です。Wi-Fi、NTP、SD ログ保存のいずれかが失敗しても、タイマー自体は利用できます。

## 機能

- 前回タイマー値を即時表示する高速起動
- プリセット: 5 / 10 / 15 / 25 / 45 分
- 1 〜 99 分の手動分数入力
- 大きな残り時間表示
- 画面四辺のセグメント進捗表示
- 日本語 / 英語 UI
- サウンドモード: Quiet / Normal / Loud
- 充電中でない場合、バッテリー残量 20% 以下で視覚警告
- microSD への受動的な CSV ログ記録
- バックグラウンド Wi-Fi / NTP 時刻同期
- 起動時に Wi-Fi や NTP を待たない

## 操作

### 待機状態

- `Enter`: 前回タイマーを開始
- `1` 〜 `5`: プリセット開始
- `0`: カスタム分数入力へ
- `S`: 設定を開く

### カスタム入力

- 数字キー: 分数入力
- `Enter`: 開始
- `Del`: 1 桁削除。空なら待機状態に戻る

### 実行中 / 一時停止中

- `Enter`: 一時停止 / 再開
- `Del`: リセット確認を開く

### 設定

- `1`: 日本語
- `2`: 英語
- `3`: Quiet
- `4`: Normal
- `5`: Loud
- `Fn + ;`: 左へ移動
- `Fn + ,`: 下へ移動
- `Fn + .`: 上へ移動
- `Fn + /`: 右へ移動
- `Enter`: 選択項目を適用
- `Del`: 待機状態に戻る

## 設定

`config.example.h` を `config.h` にコピーし、編集は `config.h` のみを行ってください。

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

実際の Wi-Fi 認証情報はコミットしないでください。`config.h` は意図的に無視されています。

Wi-Fi 未設定でもタイマーは動作し、完了セッションは `unsynced` として記録されます。

## CSV ログ

完了セッションは次のファイルに追記されます。

```text
/study_log.csv
```

現在の CSV 形式:

```csv
date,time,sync_status,duration_min,completed,input_type
2026-05-19,13:32:52,synced,25,1,preset4
2026-05-19,14:10:12,synced,45,1,custom
,,unsynced,15,1,preset3
```

列の意味:

- `date`: `YYYY-MM-DD`。NTP 未同期時は空
- `time`: `HH:MM:SS`。NTP 未同期時は空
- `sync_status`: `synced` または `unsynced`
- `duration_min`: タイマー時間（分）
- `completed`: 完了セッションは `1`
- `input_type`: `preset1` 〜 `preset5`、または `custom`

Cardputer 側で将来的に結合タイムスタンプ文字列を解析せず日次集計できるように、日時列は分割されています。

## ビルド

対象ボード:

```text
m5stack:esp32:m5stack_cardputer
```

必要な Arduino ライブラリ例:

- M5Cardputer
- M5Unified
- M5GFX

コンパイル例:

```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer .
```

## リポジトリ内容

この公開向けリポジトリは、意図的に次のみを追跡します。

- `CARDPUTER-StudyTimer.ino`
- `README.md`
- `README.ja.md`
- `config.example.h`
- `.gitignore`

ローカルメモ、自動生成ドキュメント、非公開設定、エディタ関連ファイルは無視されます。
