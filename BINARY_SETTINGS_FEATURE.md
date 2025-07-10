# 二值化設定管理功能

## 新增功能概覽

我已經為您的影像處理程式新增了完整的二值化設定管理功能，包括：

### 1. 保存二值化設定
- 在 OpenCV Image Viewer & Processor 視窗中新增了 "Save Binary Settings" 按鈕
- 只有在啟用二值化模式時才能保存設定
- 可以為每個設定命名，方便識別
- 設定會自動保存到 JSON 檔案

### 2. 載入二值化設定管理視窗
- 在主視窗新增了 "Binary Settings Manager" 選項
- 可以查看所有已保存的二值化設定
- 顯示設定的詳細資訊（色彩空間、閾值等）
- 支援載入、刪除和清除所有設定

### 3. JSON 檔案儲存位置
- **Windows**: `%USERPROFILE%\Documents\imgBinHistory.json`
- **Linux**: `~/imgBinHistory.json`

## 使用方法

### 保存設定
1. 在 OpenCV 視窗中調整二值化參數
2. 確保勾選了 "Enable Binary Threshold"
3. 點擊 "Save Binary Settings" 按鈕
4. 輸入設定名稱並確認保存

### 載入設定
1. 在主視窗勾選 "Binary Settings Manager"
2. 在設定列表中選擇要載入的設定
3. 點擊 "Load Selected Setting" 按鈕
4. 影像會自動套用新的二值化設定

### 管理設定
- **刪除單一設定**: 選擇設定後點擊 "Delete Selected Setting"
- **清除所有設定**: 點擊 "Clear All Settings"
- **重新載入**: 點擊 "Reload Settings from File"

## 技術實現

### 新增的結構體
```cpp
struct BinaryThresholdSetting {
    string name;
    int color_space;
    float rgb_threshold[3];
    float hsl_threshold[3];
    float hsv_threshold[3];
    bool enable_binary;
};
```

### 主要函數
- `saveBinaryThresholdSettings()`: 保存設定到 JSON 檔案
- `loadBinaryThresholdSettings()`: 從 JSON 檔案載入設定
- `getDocumentPath()`: 取得平台相應的文檔目錄路徑

### 依賴套件
- 新增了 jsoncpp 庫用於 JSON 檔案處理
- 已在 CMakeLists.txt 中配置相關依賴

## 檔案結構範例

imgBinHistory.json 檔案結構：
```json
[
  {
    "name": "手寫字二值化",
    "color_space": 1,
    "enable_binary": true,
    "rgb_threshold": [128.0, 128.0, 128.0],
    "hsl_threshold": [0.0, 0.0, 68.0],
    "hsv_threshold": [180.0, 50.0, 50.0]
  }
]
```

## 編譯需求

確保已安裝 jsoncpp 套件：
```bash
vcpkg install jsoncpp:x64-windows
```

然後按照原有的編譯指令進行編譯即可。
