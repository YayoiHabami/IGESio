/**
 * @file examples/gui/input_config.h
 * @brief ビューアのマウス操作のキー割り当て設定を定義する
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * マウス操作 (回転/移動/ズーム/選択) を「ボタン + 修飾キー」の組で表現し、
 * IgesViewerGUIへ注入することで操作の割り当てを変更可能にする。
 * ウィンドウシステム (GLFW) に依存しないよう抽象的な列挙体で表現し、
 * GLFW定数との変換はGUI実装側 (.cpp) で行う。
 */
#ifndef EXAMPLES_GUI_INPUT_CONFIG_H_
#define EXAMPLES_GUI_INPUT_CONFIG_H_

namespace igesio::graphics {

/// @brief マウスボタン (ウィンドウシステム非依存)
enum class MouseButton {
    /// @brief いずれのボタンでもない (未対応ボタン用)
    kNone,
    /// @brief 左ボタン
    kLeft,
    /// @brief 中ボタン
    kMiddle,
    /// @brief 右ボタン
    kRight,
};

/// @brief 修飾キー (ビットフラグ; ORで組み合わせ可能)
enum class ModifierKey : int {
    /// @brief 修飾なし
    kNone = 0,
    /// @brief Ctrl
    kCtrl = 1 << 0,
    /// @brief Shift
    kShift = 1 << 1,
    /// @brief Alt
    kAlt = 1 << 2,
    /// @brief Super (Windows/Commandキー)
    kSuper = 1 << 3,
};

/// @brief ModifierKeyのビット和
constexpr ModifierKey operator|(const ModifierKey a, const ModifierKey b) {
    return static_cast<ModifierKey>(
            static_cast<int>(a) | static_cast<int>(b));
}

/// @brief ModifierKeyのビット積
constexpr ModifierKey operator&(const ModifierKey a, const ModifierKey b) {
    return static_cast<ModifierKey>(
            static_cast<int>(a) & static_cast<int>(b));
}

/// @brief マウス操作の割り当て (ボタン + 修飾キー)
struct InputBinding {
    /// @brief 操作に使用するマウスボタン
    MouseButton button = MouseButton::kNone;
    /// @brief 同時に押下が必要な修飾キー (kNoneで修飾なし)
    ModifierKey mods = ModifierKey::kNone;
};

/// @brief ビューア操作のキー割り当て設定
/// @note 各メンバを書き換えてSetInputConfigで注入することで操作を変更できる
struct InputConfig {
    /// @brief カメラ回転
    InputBinding rotate = {MouseButton::kMiddle, ModifierKey::kNone};
    /// @brief カメラ移動 (パン)
    InputBinding pan = {MouseButton::kMiddle, ModifierKey::kCtrl};
    /// @brief ドラッグによるズーム (スクロールによるズームとは別に有効)
    InputBinding zoom_drag = {MouseButton::kMiddle, ModifierKey::kShift};
    /// @brief 単一選択 (クリック)
    InputBinding select = {MouseButton::kLeft, ModifierKey::kNone};
    /// @brief 複数選択 (トグル) 時にselectのボタンへ追加で要求する修飾キー
    ModifierKey multi_select_mod = ModifierKey::kCtrl;
};

/// @brief 押下されたボタン+修飾キーが割り当てに一致するか
/// @param binding 判定対象の割り当て
/// @param button 押下されたボタン
/// @param mods 押下中の修飾キー
/// @return ボタンと修飾キーがともに一致する場合はtrue
/// @note modsはMouseButton/ModifierKeyへ正規化済み (関係する修飾ビットのみ)
///       であることを前提に、完全一致で判定する
inline bool Matches(const InputBinding& binding,
                    const MouseButton button, const ModifierKey mods) {
    return binding.button == button && binding.mods == mods;
}

}  // namespace igesio::graphics

#endif  // EXAMPLES_GUI_INPUT_CONFIG_H_
