# OOPL2026 Class Project

本專案為 OOPL2026 課程練習專案，基於 [PTSD Template](https://github.com/ntut-open-source-club/ptsd-template) 進行修改與開發。

專案目標是以物件導向程式設計的方式，練習專案遊戲開發。

## 快速開始

1. 複製專案：

   ```bash
   git clone https://github.com/ZoeChengYu/Infinitode-2---Tower-Defense-OOPL2026-.git --recursive
   ```

2. 使用 `Debug` 模式設定專案：

   > [!WARNING]
   > 請使用 `Debug` 模式建置專案，因為目前 `Release` 模式仍有問題。
   > 此外，請加入 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`，以避免舊版 CMake 政策造成的相容性問題。

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5
   ```

3. 建置專案：

   ```bash
   cmake --build build
   ```

4. 執行專案：

   ```bash
   ./build/REPLACE_WITH_YOUR_PROJECT_NAME
   ```
