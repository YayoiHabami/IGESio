/**
 * @file examples/gui/surface_algorithm_verifier_gui.h
 * @brief サーフェスのアルゴリズム検証用GUI (IgesViewerGUIの派生)
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note 基底クラス (IgesViewerGUI) の挙動は変更しない.
 */
#ifndef EXAMPLES_GUI_SURFACE_ALGORITHM_VERIFIER_GUI_H_
#define EXAMPLES_GUI_SURFACE_ALGORITHM_VERIFIER_GUI_H_

#ifdef IGESIO_INSPECTION_EXTENSION_ENABLED

#include <optional>
#include <string>
#include <vector>

#include "./iges_viewer_gui.h"

#include <igesio/extensions/inspection.h>



namespace igesio::graphics {

/// @brief サーフェスのアルゴリズム検証用GUI
class SurfaceAlgorithmVerifierGUI : public IgesViewerGUI {
 public:
    /// @brief コンストラクタ
    /// @param width ウィンドウ幅の初期値 [px]
    /// @param height ウィンドウ高さの初期値 [px]
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @param initial_file 起動時に読み込むファイル (空で無し)
    /// @throw std::runtime_error ウィンドウの初期化に失敗した場合
    explicit SurfaceAlgorithmVerifierGUI(int width = kDefaultDisplayWidth,
                                         int height = kDefaultDisplayHeight,
                                         int msaa_samples = 0,
                                         const std::string& initial_file = "");

 protected:
    /// @brief 「Verify」メニューを追加する
    void RenderExtraMenus() override;
    /// @brief 検証ウィンドウを描画する (平時非表示)
    void RenderExtraWindows() override;
    /// @brief ピックモード中は面上のクリックを横取りして点を収集する
    bool OnViewportClick(double x, double y, int mods) override;



 private:
    /**
     * 複製表示 (InstancedEntity) 検証関連
     */

    /// @brief 複製表示の検証ウィンドウ
    void RenderDuplicationWindow();
    /// @brief 選択中エンティティ (または所有Assembly) を複製したInstancedEntityを
    ///        生成して描画へ投入する
    /// @note UIの複製単位 (選択エンティティ/所有Assembly)・複製個数・スパン (直線配列)・
    ///       角度変化から各複製の配置行列を組み立て、共有メッシュで描く複製表示
    ///       エンティティを生成する. Assembly単位では`MakeInstancedAssembly`で
    ///       サブツリーを平坦化する.
    void RunDuplication();
    /// @brief 生成済みの複製 (子Assembly) を除去する
    void ClearDuplicates();

    /// @brief 複製検証ウィンドウを表示中か
    bool dup_window_open_ = false;
    /// @brief 複製の単位 (0=選択エンティティ, 1=所有Assembly)
    int dup_unit_ = 0;
    /// @brief 複製個数 N (>=1)
    int dup_count_ = 5;
    /// @brief 隣接複製間の距離 [モデル長]
    double dup_span_ = 20.0;
    /// @brief 直線配列の方向 (ワールド軸; 0=X, 1=Y, 2=Z)
    int dup_translate_axis_ = 0;
    /// @brief 1複製あたりの角度変化 [deg]
    double dup_angle_deg_ = 15.0;
    /// @brief 回転軸 (ワールド軸; 0=X, 1=Y, 2=Z)
    int dup_rotate_axis_ = 2;
    /// @brief copy0を原寸位置 (元エンティティと同一) に重ねるか
    bool dup_include_origin_ = true;
    /// @brief 生成した複製を保持する子AssemblyのID
    std::optional<ObjectID> duplicates_assembly_id_;
    /// @brief 複製操作の結果メッセージ
    std::string dup_status_;
};

}  // namespace igesio::graphics

#endif  // IGESIO_INSPECTION_EXTENSION_ENABLED

#endif  // EXAMPLES_GUI_SURFACE_ALGORITHM_VERIFIER_GUI_H_
