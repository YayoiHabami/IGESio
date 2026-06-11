/**
 * @file graphics/shader_registry.h
 * @brief シェーダー定義 (ソースコード+メタ情報) のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note コア改変なしに独自シェーダーを追加するための登録機構.
 *       組み込みシェーダー (ShaderIdの静的定数) も同じレジストリに保持され、
 *       照明使用・表示モードでの取捨等の性質はすべてメタ情報として参照される.
 */
#ifndef IGESIO_GRAPHICS_SHADER_REGISTRY_H_
#define IGESIO_GRAPHICS_SHADER_REGISTRY_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/graphics/core/shader_code.h"
#include "igesio/graphics/core/shader_id.h"



namespace igesio::graphics {

/// @brief 表示モード (DisplayMode) による取捨のためのシェーダー描画カテゴリ
enum class ShaderDrawCategory {
    /// @brief 表示モードに依らず常に描画する (曲線・点など)
    kAlways,
    /// @brief 面塗り (三角形描画). kWireFrameでは描画しない
    kSurfaceFill,
    /// @brief 面エッジ (境界線). kNoEdgeでは描画しない
    kSurfaceEdge,
};

/// @brief シェーダープログラム1件の定義 (ソースコード+メタ情報)
struct ShaderInfo {
    /// @brief 一意なシェーダー名
    /// @note `ToString(ShaderId)`の表示と`ShaderRegistry::Find`の検索に使用する.
    ///       組み込み名 ("GeneralCurve"等) を含め、登録済みの名前は使用できない
    std::string name;
    /// @brief GLSLソースコード一式
    ShaderCode code;
    /// @brief 描画時に光源関連のuniformを受け取るか
    /// @note trueの場合、レンダラは描画前に`viewPos_WorldSpace`/`ambientColor`/
    ///       `numLights`/`lightPositions`/`lightAttenuations`/`lightColors`を設定する
    bool uses_lighting = false;
    /// @brief 表示モードによる取捨のカテゴリ
    ShaderDrawCategory category = ShaderDrawCategory::kAlways;
};

/// @brief シェーダー定義のレジストリ
/// @note 登録は`Revision()`へ反映され、レンダラが次回の描画時に未コンパイルの
///       シェーダーを検知して追加コンパイルする (遅延コンパイル). したがって
///       レンダラの`Initialize()`後の登録も次のDrawで有効になる. ただし
///       コンパイル失敗はそのDraw内で例外 (igesio::ImplementationError) になる
///       ため、登録内容の誤りは描画時に顕在化する.
/// @note レンダラがシェーダーへ設定するuniformは`view`/`projection`
///       (+`uses_lighting`時の光源関連) のみ. `model`/`mainColor`等の
///       オブジェクト毎のuniformは各描画クラスの`Draw`実装の責務.
/// @note スレッド安全性は保証しない. 登録は描画スレッドと競合しないこと.
///       内部ストレージは関数内static (Meyersシングルトン) のため、静的初期化子
///       からの登録でも初期化順に依存せず安全.
class ShaderRegistry {
 public:
    /// @brief シェーダー定義を登録する
    /// @param info 登録する定義
    /// @return 採番されたShaderId (内部値は256以降)
    /// @throw std::invalid_argument 名前が空の場合、名前が登録済み
    ///        (組み込み名を含む) の場合、またはcodeが不完全
    ///        (`ShaderCode::IsIncomplete`) の場合
    /// @note 組み込みシェーダーの差し替えはできない (名前・IDとも予約済み)
    static ShaderId Register(ShaderInfo info);

    /// @brief 名前からShaderIdを検索する
    /// @param name シェーダー名
    /// @return 対応するShaderId. 未登録の場合はnullopt
    static std::optional<ShaderId> Find(std::string_view name);

    /// @brief 指定IDの定義を取得する
    /// @param id シェーダーの識別子
    /// @return 定義へのポインタ. 未登録IDの場合はnullptr.
    ///         返るポインタはプロセス終了まで有効 (登録で無効化されない)
    static const ShaderInfo* Get(ShaderId id);

    /// @brief レジストリのリビジョンを取得する
    /// @return 登録のたびに単調増加する値
    /// @note レンダラが遅延コンパイルの要否判定に用いる
    static std::uint64_t Revision();

    /// @brief 登録済みの全ID (組み込み+ユーザー) を取得する
    /// @return ShaderIdのリスト (順序は不定)
    /// @note レンダラのコンパイル列挙用. センチネル (kComposite/kNone) も
    ///       含まれる (コード無しのためコンパイル対象にはならない)
    static std::vector<ShaderId> AllIds();
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SHADER_REGISTRY_H_
