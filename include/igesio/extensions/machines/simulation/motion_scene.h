/**
 * @file extensions/machines/simulation/motion_scene.h
 * @brief 動作のサンプル列から作成された表示オブジェクト (工具軌跡、動作軌跡、現在レコード)
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note `MotionTrack`を`MachineScene`の配置先 (`trajectory:`/`trace:machine`/
 *       `trace:work`) の中身にする非メンバ関数. 配置先自体は`MachineScene::Build`で
 *       作り、本ヘッダの関数は中身だけを作り直す.
 *       ```text
 *       trajectory:                  (work_mount座標. ワークビューのみ表示)
 *       └─ run<k>                    (EnumeratePathsの区間k. サンプルの無い区間は作らない)
 *          └─ tool<n>                (工具nのコピー. 区間内で工具が変わればその数だけ)
 *             └─ holder              (ホルダ部のコピー)
 *       trace:machine / trace:work   (動作軌跡)
 *       ├─ rapid / cut               (早送り/それ以外の連続区間ごとに1本の折れ線)
 *       └─ current                   (現在レコードの強調. 1本の折れ線)
 *       ```
 * @note 工具軌跡の同次変換は、テンプレート (`tool:<n>`) のルート座標系→`work_mount`
 *       コンポーネント座標系の同次変換 P_i = F_wm(q_i)⁻¹·F_tm(q_i)·H_tm·T(0,0,-ゲージ長)
 *       である (F_tm/F_wmは`tool_mount`/`work_mount`コンポーネントの運動F_c(q)).
 *       `work_mount`の子として置いたときの累積変換が同時刻の`tool:<n>`の累積変換と
 *       一致する.
 * @note 動作軌跡は工具の指令点の軌跡で、補間点、機械座標の移動、軸の指令のみの移動,
 *       到達不能で保持した姿勢を含む実際の動きを表す. 指令点は工具座標の
 *       (0, 0, command_point_z) とし、G43と制御点の設定は反映しない. 工具が無ければ
 *       取り付けフレームの原点とする. `trace:machine`には p_i = F_tm(q_i)·c を,
 *       `trace:work`には F_wm(q_i)⁻¹·p_i を置く (cは指令点のゼロポーズ機械座標).
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_SCENE_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_SCENE_H_

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "igesio/common/color.h"
#include "igesio/common/id_generator.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 工具軌跡の区間のアセンブリの名前の接頭辞 (`"run<k>"`)
constexpr std::string_view kTrajectoryRunPrefix = "run";
/// @brief 工具軌跡の区間内の工具のアセンブリの名前の接頭辞 (`"tool<n>"`)
constexpr std::string_view kTrajectoryToolPrefix = "tool";
/// @brief 現在レコードの強調のアセンブリの名前 (`trace:*`の子)
constexpr std::string_view kCurrentRecordName = "current";
/// @brief 現在レコードの強調の色
constexpr Color kCurrentRecordColor = Color::FromRGB255(255, 140, 0);

/// @brief 間引きの方式
enum class ThinningMode {
    /// @brief 間引かない
    kNone,
    /// @brief 割合で間引く (`rate`の割合を等間隔に残す)
    kRate,
    /// @brief 上限数で間引く (`max_count`個を等間隔に残す)
    kMaxCount,
};

/// @brief 間引きの指定
/// @note いずれの方式でも両端は必ず残す
struct Thinning {
    /// @brief 方式
    ThinningMode mode = ThinningMode::kNone;
    /// @brief 残す割合
    /// @note 0.0〜1.0. `kRate`で用いる
    double rate = 1.0;
    /// @brief 残す上限数 (`kMaxCount`で用いる)
    int max_count = 50;
};

/// @brief 工具軌跡の生成設定
struct ToolTrajectoryOptions {
    /// @brief レコードの終点のサンプルだけを用いるか (`false`なら補間点も含める)
    bool command_points_only = true;
    /// @brief 区間内のサンプルの間引き
    Thinning thinning;
    /// @brief ホルダ部を表示するか
    bool holder_visible = true;
    /// @brief 不透明度のオーバーライド (省略時は工具の見た目のまま)
    std::optional<float> opacity;
    /// @brief 除外するサンプルのインデックス (表示中の工具と重なるサンプルを除く用途)
    std::optional<std::size_t> skip_sample;
};

/// @brief 工具軌跡の区間単位の可視性
struct TrajectoryVisibility {
    /// @brief 工具軌跡全体を表示するか
    bool show = true;
    /// @brief この区間だけを表示する (省略時は全区間が対象)
    std::optional<std::size_t> only_run;
    /// @brief 区間の間引き (存在する区間の並びに対して適用する)
    Thinning thinning;
    /// @brief 間引きの対象でも表示する区間 (通常は現在の区間)
    std::optional<std::size_t> always_visible_run;
};

/// @brief 動作軌跡の生成設定
struct MotionTraceOptions {
    /// @brief 機械座標の動作軌跡 (`trace:machine`) を作るか
    bool machine_frame = true;
    /// @brief `work_mount`座標の動作軌跡 (`trace:work`) を作るか
    bool work_frame = true;
};

/// @brief 間引き後に残すインデックスを計算する
/// @param count 要素数
/// @param thinning 間引きの指定
/// @return 残すインデックス (昇順)
/// @throw std::invalid_argument `kRate`で`rate`が正でない、または`kMaxCount`で
///        `max_count`が正でない場合
/// @note 2つ以上あれば両端を含む
std::vector<std::size_t> ThinIndices(std::size_t count, const Thinning& thinning);

/// @brief 工具軌跡 (`trajectory:`の中身) を作り直す
/// @param[in,out] scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param program `track`の元のCLプログラム (`EnumeratePaths`で区間を分ける)
/// @param options 設定
/// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
/// @throw std::invalid_argument `scene`が未構築、または`thinning`が不正な場合
///        (`ThinIndices`から伝播)
/// @note 区間kのサンプルは`record_index`が区間内のもの. 工具番号が区間内で変わる
///       場合は工具ごとに`tool<n>`を分ける. 工具表に無い番号と工具なしのサンプルは
///       省き、番号ごとに1回警告する
void RebuildToolTrajectory(MachineScene& scene, const MotionTrack& track,
                           const ClProgram& program,
                           const ToolTrajectoryOptions& options = {},
                           std::vector<Diagnostic>* warnings = nullptr);

/// @brief 工具軌跡の区間単位の可視性を設定する
/// @param[in,out] scene 構築済みのシーン
/// @param visibility 可視性
/// @throw std::invalid_argument `thinning`が不正な場合 (`ThinIndices`から伝播)
/// @note 未構築、または工具軌跡が無ければ何もしない
void SetToolTrajectoryVisible(MachineScene& scene,
                              const TrajectoryVisibility& visibility);

/// @brief 工具軌跡の全区間のホルダ部の可視性を設定する
/// @param[in,out] scene 構築済みのシーン
/// @param visible 表示するなら`true`
/// @note 未構築、または工具軌跡が無ければ何もしない
void SetToolTrajectoryHolderVisible(MachineScene& scene, bool visible);

/// @brief 金属材質を設定する工具軌跡のエンティティ (各区間の切れ刃部+シャンク部の
///        コピー) のIDを集める
/// @param scene 構築済みのシーン
/// @return IDの一覧 (ホルダ部は含まない. 工具軌跡が無ければ空)
std::vector<ObjectID> ToolTrajectoryMetallicIds(const MachineScene& scene);

/// @brief 動作軌跡 (`trace:machine`/`trace:work`の中身) を作り直す
/// @param[in,out] scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param options 設定
/// @throw std::invalid_argument `scene`が未構築の場合
/// @note 早送りか否かと工具番号の変化で区間を分け、区間ごとに1本の3次元折れ線
///       (Type 106 Form 12) を`rapid`/`cut`の子に置く (色は経路線と同じ).
///       `current`は空の非表示で作る. 設定で作らない側の配置先は空にする
void RebuildMotionTrace(MachineScene& scene, const MotionTrack& track,
                        const MotionTraceOptions& options = {});

/// @brief 現在レコードの強調 (`trace:*`の子`current`) を設定し直す
/// @param[in,out] scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param record_index 強調するレコードのインデックス (`ClProgram::records`)
/// @throw std::invalid_argument `scene`が未構築の場合
/// @note レコードのサンプル列 (直前のサンプルを始点に含める) の折れ線に置き換える.
///       サンプルが2点未満 (動作でないレコード等) なら非表示にする.
///       `RebuildMotionTrace`で`current`を作っていない配置先では何もしない.
///       再生中は`"record"`イベントの値が変わったときだけ呼ぶ
void UpdateCurrentRecord(MachineScene& scene, const MotionTrack& track,
                         std::size_t record_index);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_SCENE_H_
