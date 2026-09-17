/**
 * @file extensions/machines/simulation/animation_bridge.cpp
 * @brief 動作のサンプル列からのアニメーションクリップの生成
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/simulation/animation_bridge.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"

namespace igesio::extensions::machines {

namespace {

namespace anim = igesio::extensions::animation;

/// @brief 2つの変換が同じか (成分の差の最大が許容誤差以下)
/// @param lhs 比較する変換
/// @param rhs 比較する変換
/// @param tolerance 許容誤差
bool SameState(const igesio::Matrix4d& lhs, const igesio::Matrix4d& rhs,
               const double tolerance) {
    return (lhs - rhs).cwiseAbs().maxCoeff() <= tolerance;
}

/// @brief 2つの可視性が同じか
/// @param lhs 比較する可視性
/// @param rhs 比較する可視性
/// @note 第3引数 (許容誤差) は用いない
bool SameState(const bool lhs, const bool rhs, double) { return lhs == rhs; }

/// @brief 2つのイベント値が同じか
/// @param lhs 比較する値
/// @param rhs 比較する値
/// @note 第3引数 (許容誤差) は用いない
bool SameState(const std::int64_t lhs, const std::int64_t rhs, double) {
    return lhs == rhs;
}

/// @brief 1ターゲット (または1イベント名) 分のキー発行の管理用クラス
/// @tparam State キーの値の型 (変換は`Matrix4d`、可視性は`bool`、イベントは`int64_t`)
/// @note 同時刻に複数の値が来た場合は最後の値だけをキーにするため、値を保留して
///       時刻が進んだときに発行する. `skip`なら直前に発行した値と同じキーを省く.
///       クリップへの追加は種別ごとに異なるため、発行先は呼び出し側が渡す
template <class State>
class KeySlot {
 public:
    /// @brief コンストラクタ
    /// @param skip 直前に発行した値と同じキーを省くか
    /// @param tolerance 同一判定の許容誤差 (変換のみ用いる)
    /// @param base 基準状態
    /// @note 基準状態と同じ最初のキーも`skip`の対象にする. 基準状態が無ければ
    ///       最初のキーは必ず発行する
    KeySlot(const bool skip, const double tolerance,
            const std::optional<State>& base)
        : skip_(skip), tolerance_(tolerance), last_(base) {}

    /// @brief 値を保留する
    /// @param time 時刻 [s]
    /// @param value 値
    /// @param emit 保留中の値を発行する関数 (時刻と値を受け取る)
    /// @note 保留中の値と同時刻なら置き換え、時刻が進んでいれば先に発行する
    template <class Emit>
    void Push(const double time, const State& value, Emit emit) {
        if (pending_.has_value() && pending_->first == time) {
            pending_->second = value;
            return;
        }
        Flush(emit);
        pending_ = std::make_pair(time, value);
    }

    /// @brief 保留中の値を発行する
    /// @param emit 発行する関数 (時刻と値を受け取る)
    template <class Emit>
    void Flush(Emit emit) {
        if (!pending_.has_value()) return;

        const bool same = last_.has_value()
                          && SameState(*last_, pending_->second, tolerance_);
        if (!skip_ || !same) {
            emit(pending_->first, pending_->second);
            last_ = pending_->second;
        }
        pending_.reset();
    }

 private:
    /// @brief 直前に発行した値と同じキーを省くか
    bool skip_ = true;
    /// @brief 同一判定の許容誤差
    double tolerance_ = 0.0;
    /// @brief 直前に発行した値 (未発行なら基準状態)
    std::optional<State> last_;
    /// @brief 保留中のキー (時刻と値)
    std::optional<std::pair<double, State>> pending_;
};

/// @brief 工具1つ分の可視性キーの対象と管理用構造体
struct ToolSlot {
    /// @brief 工具のアセンブリのID
    ObjectID id;
    /// @brief 可視性キーの発行管理
    KeySlot<bool> visibility;
};

/// @brief クリップの組み立て
/// @note クリップを所有し、総時間を構築時に固定する. 完成したクリップの取り出しは
///       `Finish`だけが行い、保留中のキーの発行漏れが起きないようにする
class ClipBuilder {
 public:
    /// @brief 組み立てを準備する
    /// @param scene 構築済みのシーン
    /// @param track 動作のサンプル列 (空でない)
    /// @param program 元のCLプログラム
    /// @param options 設定
    /// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
    ClipBuilder(const MachineScene& scene, const MotionTrack& track,
                const ClProgram& program, const ClipBuildOptions& options,
                std::vector<Diagnostic>* warnings)
        : scene_(scene), track_(track), program_(program), options_(options),
          warnings_(warnings), clip_(track.stats.duration_sec),
          tool_event_(true, 0.0, std::nullopt),
          record_event_(true, 0.0, std::nullopt),
          program_event_(true, 0.0, std::nullopt) {
        const MachineModel& model = scene.Model();
        for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
            transforms_.emplace_back(options.skip_unchanged, options.tolerance,
                                     igesio::Matrix4d::Identity());
            component_ids_.push_back(
                    scene.ComponentAssembly(model.Component(i).name)->GetID());
        }
        for (const auto& [number, id] : scene.ToolAssemblyIds()) {
            tools_.emplace(number, ToolSlot{
                    id, KeySlot<bool>(options.skip_unchanged, 0.0, std::nullopt)});
        }
    }

    /// @brief 全サンプルをキーにし、完成したクリップを取り出す
    /// @return クリップ (総時間は`track.stats.duration_sec`)
    /// @throw std::invalid_argument サンプルの軸変位量の長さが軸数と異なる場合
    anim::AnimationClip Finish() {
        for (const MotionSample& sample : track_.samples) {
            PushTransforms(sample);
            PushVisibility(sample);
            PushEvents(sample);
        }
        FlushAll();
        return std::move(clip_);
    }

 private:
    /// @brief 変換キーを追加する関数を作る
    /// @param id 対象のアセンブリのID (呼び出し側で寿命を保つこと)
    auto TransformEmitter(const ObjectID& id) {
        return [this, &id](const double time, const igesio::Matrix4d& transform) {
            clip_.AddKey(id, time, transform);
        };
    }

    /// @brief 可視性キーを追加する関数を作る
    /// @param id 対象のアセンブリのID (呼び出し側で寿命を保つこと)
    auto VisibilityEmitter(const ObjectID& id) {
        return [this, &id](const double time, const bool visible) {
            clip_.AddVisibilityKey(id, time, visible);
        };
    }

    /// @brief イベントキーを追加する関数を作る
    /// @param name イベントトラックの名前
    auto EventEmitter(const std::string_view name) {
        return [this, name](const double time, const std::int64_t value) {
            clip_.AddEvent(std::string(name), time, value);
        };
    }

    /// @brief サンプルの姿勢を各コンポーネントの変換キーにする
    /// @param sample サンプル
    void PushTransforms(const MotionSample& sample) {
        Forward(scene_.Model(), sample.q, &forward_buffer_);
        for (std::size_t i = 0; i < transforms_.size(); ++i) {
            transforms_[i].Push(sample.time, forward_buffer_[i],
                                TransformEmitter(component_ids_[i]));
        }
    }

    /// @brief サンプルの工具番号を各工具の可視性キーにする
    /// @param sample サンプル
    /// @note 工具表に無い番号は番号ごとに1回警告する (全工具が非表示になる)
    void PushVisibility(const MotionSample& sample) {
        const int number = sample.tool_number;
        if (number != kNoTool && tools_.count(number) == 0
            && unknown_tools_.insert(number).second && warnings_ != nullptr) {
            warnings_->push_back(Diagnostic{
                    Severity::kWarning,
                    "record " + std::to_string(sample.record_index),
                    "tool is not in the tool table; no tool is shown: T"
                    + std::to_string(number),
                    LineOf(sample.record_index)});
        }
        for (auto& [tool_number, slot] : tools_) {
            slot.visibility.Push(sample.time, tool_number == number,
                                 VisibilityEmitter(slot.id));
        }
    }

    /// @brief サンプルの工具番号、レコード、プログラムをイベントキーにする
    /// @param sample サンプル
    void PushEvents(const MotionSample& sample) {
        tool_event_.Push(sample.time, sample.tool_number,
                         EventEmitter(kToolEventTrack));
        if (options_.emit_record_events) {
            record_event_.Push(sample.time,
                               static_cast<std::int64_t>(sample.record_index),
                               EventEmitter(kRecordEventTrack));
        }
        if (program_.HasSources()) {
            program_event_.Push(sample.time,
                                program_.sources[sample.record_index].program_index,
                                EventEmitter(kProgramEventTrack));
        }
    }

    /// @brief 全てのKeySlotの保留中のキーを発行する
    void FlushAll() {
        for (std::size_t i = 0; i < transforms_.size(); ++i) {
            transforms_[i].Flush(TransformEmitter(component_ids_[i]));
        }
        for (auto& [tool_number, slot] : tools_) {
            slot.visibility.Flush(VisibilityEmitter(slot.id));
        }
        tool_event_.Flush(EventEmitter(kToolEventTrack));
        record_event_.Flush(EventEmitter(kRecordEventTrack));
        program_event_.Flush(EventEmitter(kProgramEventTrack));
    }

    /// @brief レコードの元ファイルの行番号を取得する
    /// @param record_index レコードのインデックス
    /// @return 行番号. 不明なら0
    int LineOf(const std::size_t record_index) const {
        return program_.HasSources() ? program_.sources[record_index].line : 0;
    }

    /// @brief 構築済みのシーン
    const MachineScene& scene_;
    /// @brief 動作のサンプル列
    const MotionTrack& track_;
    /// @brief 元のCLプログラム
    const ClProgram& program_;
    /// @brief 設定
    ClipBuildOptions options_;
    /// @brief 警告の追加先 (`nullptr`なら追加しない)
    std::vector<Diagnostic>* warnings_ = nullptr;
    /// @brief 組み立て中のクリップ (総時間は構築時に固定)
    anim::AnimationClip clip_;
    /// @brief コンポーネントごとの変換キーのKeySlot (`Component()`の順)
    std::vector<KeySlot<igesio::Matrix4d>> transforms_;
    /// @brief コンポーネントのアセンブリのID (`Component()`の順)
    std::vector<ObjectID> component_ids_;
    /// @brief 工具ごとの可視性キーの対象とKeySlot (工具番号→`ToolSlot`)
    std::map<int, ToolSlot> tools_;
    /// @brief `"tool"`イベントのKeySlot
    KeySlot<std::int64_t> tool_event_;
    /// @brief `"record"`イベントのKeySlot
    KeySlot<std::int64_t> record_event_;
    /// @brief `"program"`イベントのKeySlot
    KeySlot<std::int64_t> program_event_;
    /// @brief 警告済みの工具表に無い工具番号
    std::set<int> unknown_tools_;
    /// @brief 順運動学の出力バッファ
    std::vector<igesio::Matrix4d> forward_buffer_;
};

}  // namespace



anim::AnimationClip MakeMachineClip(const MachineScene& scene,
                                    const MotionTrack& track,
                                    const ClProgram& program,
                                    const ClipBuildOptions& options,
                                    std::vector<Diagnostic>* warnings) {
    if (!scene.IsBuilt()) {
        throw std::invalid_argument("MakeMachineClip: MachineScene is not built");
    }
    if (track.samples.empty()) {
        throw std::invalid_argument("MakeMachineClip: motion track has no samples");
    }
    return ClipBuilder(scene, track, program, options, warnings).Finish();
}

}  // namespace igesio::extensions::machines
