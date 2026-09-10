/**
 * @file extensions/machines/machine/axis_values.h
 * @brief 機械の軸の値の表現 (軸変位量ベクトル・NC指令値)
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 * @note 各軸の値は2つの形で表現する. `JointVector`は軸配列 (`MachineModel::Axes()`)
 *       のインデックス順に並べた軸変位量 (σを掛けた後の値)、`NcValues`は各軸のNC指令値
 *       (σを掛ける前の値). どちらも直進軸の単位はmm、回転軸の単位はrad.
 *       ここで、σはNC指令値をコンポーネントの動きに対応する軸変位量に変換する際の係数で,
 *       ワーク側チェーンの軸では-1、工具側チェーンの軸では+1となる. これは回転の式を
 *       機械の軸構成に依らず統一形式で書くための規約である (ワークを+θ回転させるのは,
 *       ワークから見て工具を-θ回転させるのと同じであるため).
 * @note 2つの型の相互変換は`forward_kinematics.h`の`JointsFromNc`/`NcFromJoints`
 *       のみで行う. σの符号を個別に掛ける処理を利用側で書かないこと.
 * @note NCプログラムの解釈等、機械モデルを持たない段階でも`NcValues`を扱えるように,
 *       本ヘッダは`MachineModel`に依存しない形で記述している.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_AXIS_VALUES_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_AXIS_VALUES_H_

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace igesio::extensions::machines {

/// @brief 各軸の変位量q [mm] or [rad] (一般化座標のようなもの)
/// @note `MachineModel::Axes()`のインデックス順に並べた軸変位量であり,
///       σを掛けた後の値 (コンポーネント自身の移動量・回転角).
/// @note 長さは構築時に決まり、以後は変えられない. `InitialJoints(model)`等の
///       生成側で軸数と一致させるものとし、運動学関連の関数は冒頭で長さを確認すること.
///       ノットや重み等の`std::vector<double>`と取り違えないように,
///       生の配列からの構築は明示的なかたちでのみ可能にしている.
class JointVector {
 public:
    /// @brief 長さ0のベクトル
    JointVector() = default;
    /// @brief 全軸を同じ値で初期化する
    /// @param axis_count 軸数
    /// @param value 各軸の値
    explicit JointVector(std::size_t axis_count, double value = 0.0);
    /// @brief 生の配列から構築する
    /// @param values 軸変位量
    ///        (`MachineModel::Axes()`の順. 順序は呼び出し側が保証すること)
    explicit JointVector(std::vector<double> values);

    /// @brief 軸数
    std::size_t Size() const { return values_.size(); }
    /// @brief 軸数が0か
    bool Empty() const { return values_.empty(); }
    /// @brief 指定軸の変位量を取得する (非const参照)
    /// @param axis_index `MachineModel::Axes()`におけるインデックス
    double& operator[](const std::size_t axis_index) {
        return values_[axis_index];
    }
    /// @brief 指定軸の変位量を取得する
    /// @param axis_index `MachineModel::Axes()`におけるインデックス
    double operator[](const std::size_t axis_index) const {
        return values_[axis_index];
    }
    /// @brief 全軸の変位量
    const std::vector<double>& Values() const { return values_; }

 private:
    /// @brief 軸変位量 (`MachineModel::Axes()`の順)
    std::vector<double> values_;
};

/// @brief 全要素の完全一致 (許容誤差なし)
/// @return 長さと全要素が一致すれば`true`
inline bool operator==(const JointVector& lhs, const JointVector& rhs) {
    return lhs.Values() == rhs.Values();
}

/// @brief `operator==`の否定
/// @return 長さまたはいずれかの要素が異なれば`true`
inline bool operator!=(const JointVector& lhs, const JointVector& rhs) {
    return !(lhs == rhs);
}

/// @brief NC指令値の1項目 (軸名と値)
struct NcEntry {
    /// @brief 軸名 (NCのレジスタ名; "X","A"等)
    std::string register_name;
    /// @brief NC指令値 [mm] or [rad]
    /// @note 直進軸の場合は並進[mm]、回転軸の場合は回転角[rad]
    double value = 0.0;
};

/// @brief 各軸のNC指令値 [mm] or [rad]
/// @note 軸名 ("X","A"等) をキーとする辞書. σを掛ける前の値であり,
///       NCプログラムに書かれる値に等しい.
/// @note 直進軸や回転軸だけなど、指定された軸だけを持つ部分指定 (ワークオフセットや
///       IKで解いた軸など) と、全軸の値を持つケース (`NcFromJoints`の戻り値や
///       表示用の値など) の両方に用いる.
/// @note 読込のつもりでの値の挿入を防ぐため、添字演算子は提供しない.
class NcValues {
 public:
    /// @brief 空の指令値
    NcValues() = default;
    /// @brief `{{"A", a}, {"C", c}}`の形で構築する
    /// @param entries 軸名と値の組
    /// @note 同じ軸名が複数あれば後の要素がを優先する
    NcValues(std::initializer_list<NcEntry> entries);

    /// @brief 指定された軸のNC指令値を取得する
    /// @param register_name 軸名
    /// @return 指令値. 未指定なら`std::nullopt`
    std::optional<double> Get(std::string_view register_name) const;
    /// @brief 指定された軸のNC指令値を取得する (未指定なら既定値)
    /// @param register_name 軸名
    /// @param fallback 未指定の場合に返す値
    double GetOr(std::string_view register_name, double fallback) const;
    /// @brief 指定された軸のNC指令値を取得する (指定済みであること)
    /// @param register_name 軸名
    /// @throw std::out_of_range 未指定の場合
    double At(std::string_view register_name) const;
    /// @brief 指定された軸が設定されているか
    /// @param register_name 軸名
    /// @return 設定されていれば`true`
    bool Contains(std::string_view register_name) const;
    /// @brief 指定された軸のNC指令値を設定する
    /// @param register_name 軸名
    /// @param value 指令値
    /// @note 既存の軸であればそのまま上書きし、無ければ末尾に追加する
    void Set(std::string register_name, double value);
    /// @brief 他のNC指令値を統合する
    /// @param overlay 統合する指令値
    /// @note `overlay`の全項目を挿入順に設定する. 同一の軸については`overlay`
    ///       の値で上書きされ、`overlay`のみに存在する軸は末尾に追加される
    void Merge(const NcValues& overlay);

    /// @brief 設定されている軸の数
    std::size_t Size() const { return entries_.size(); }
    /// @brief 空か (設定されている軸の数が0か)
    bool Empty() const { return entries_.empty(); }
    /// @brief 全項目
    /// @return 軸名とNC指令値の組の配列 (挿入順)
    const std::vector<NcEntry>& Entries() const { return entries_; }

 private:
    /// @brief 指定された軸のインデックスを取得する
    /// @param register_name 軸名
    /// @return `entries_`におけるインデックス. 無ければ`std::nullopt`
    std::optional<std::size_t> IndexOf(std::string_view register_name) const;

    /// @brief 項目 (挿入順. 軸名は重複しない)
    std::vector<NcEntry> entries_;
};

/// @brief 軸の集合と各値の完全一致 (順序は無視. 許容誤差なし)
/// @return 同じ軸の集合を持ち、各軸の値が一致すれば`true`
bool operator==(const NcValues& lhs, const NcValues& rhs);

/// @brief `operator==`の否定
/// @return 軸の集合またはいずれかの値が異なれば`true`
inline bool operator!=(const NcValues& lhs, const NcValues& rhs) {
    return !(lhs == rhs);
}

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_AXIS_VALUES_H_
