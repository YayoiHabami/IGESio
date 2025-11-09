/**
 * @file numerics/bounding_box.h
 * @brief ジオメトリエンティティ用のバウンディングボックス計算
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_BOUNDING_BOX_H_
#define IGESIO_NUMERICS_BOUNDING_BOX_H_

#include <array>
#include <limits>
#include <vector>

#include "igesio/numerics/matrix.h"



namespace igesio::numerics {

/// @brief バウンディングボックスを表すクラス
class BoundingBox {
 public:
    /// @brief 延伸方向D1~D3の種類
    enum class DirectionType {
        /// @brief 線分 (辺とベクトルの長さが同じ)
        kSegment,
        /// @brief 半直線 (辺がベクトル方向に無限に伸びる)
        kRay,
        /// @brief 無限直線 (辺が両方向に無限に伸びる)
        kLine
    };

 private:
    /// @brief 基準点 P0
    Vector3d control_;
    /// @brief 基準点から延びる3方向 D0, D1, D2
    /// @note D0, D1, D2は単位ベクトルであり、D0×D1=D2を満たす.
    std::array<Vector3d, 3> directions_;
    /// @brief 各方向の長さ s0, s1, s2
    /// @note DiがkSegmentの場合は正の有限値、DiがkRayの場合は+∞、DiがkLineの場合は-∞となる.
    ///       また、sizes_[0], sizes_[1] > 0, sizes_[2] >= 0 を満たす
    ///       (2次元の場合はsizes_[2] = 0).
    std::array<double, 3> sizes_;

    /// @brief デフォルトのdirection_types_
    /// @note すべての要素が`DirectionType::kSegment`となる
    static constexpr std::array<DirectionType, 3> kDefaultDirectionTypes3D = {
        DirectionType::kSegment, DirectionType::kSegment, DirectionType::kSegment
    };
    /// @brief 2次元用のデフォルトのdirection_types_
    static constexpr std::array<DirectionType, 2> kDefaultDirectionTypes2D = {
        DirectionType::kSegment, DirectionType::kSegment
    };

 public:
    /// @brief デフォルトコンストラクタ
    /// @note 基準点が原点、大きさが０のバウンディングボックスを作成する.
    BoundingBox();

    /// @brief コンストラクタ (3次元用)
    /// @param control 基準点 P0
    /// @param directions 延伸方向 D0, D1, D2 (単位ベクトル)
    /// @param sizes 各方向のサイズ s0, s1, s2
    /// @param is_line 各方向がkLineかどうかを示す配列. is_line[i]がfalseの場合は、
    ///        sizes[i]が有限値であればkSegment、+∞であればkRayとして扱う.
    /// @throw std::invalid_argument controlに無限大の値が含まれる場合、sizes[2]以外の
    ///        要素がゼロの場合、D0, D1, D2が単位ベクトルでない場合、または
    ///        D0・D1!=0 または D0×D1!=D2 の場合 (各軸が直交座標系を形成しない場合)
    BoundingBox(const Vector3d&, const std::array<Vector3d, 3>&,
                const std::array<double, 3>&,
                const std::array<bool, 3>& = {false, false, false});

    /// @brief コンストラクタ (3次元用; xyz軸方向に平行な場合)
    /// @param control 基準点 P0 (左下前の点)
    /// @param sizes 各方向のサイズ (x軸正方向, y軸正方向, z軸正方向)
    /// @param is_line 各方向がkLineかどうかを示す配列. is_line[i]がfalseの場合は、
    ///        sizes[i]が有限値であればkSegment、+∞であればkRayとして扱う.
    /// @throw std::invalid_argument sizesのいずれかの要素が負値の場合、
    ///        またはsizes[2]以外が0の場合
    BoundingBox(const Vector3d&, const std::array<double, 3>&,
                const std::array<bool, 3>& = {false, false, false});

    /// @brief コンストラクタ (2次元用)
    /// @param control 基準点 P0
    /// @param directions 延伸方向 D0, D1 (単位ベクトル)
    /// @param sizes 各方向のサイズ
    /// @param is_line 各方向がkLineかどうかを示す配列. is_line[i]がfalseの場合は、
    ///        sizes[i]が有限値であればkSegment、+∞であればkRayとして扱う.
    /// @throw std::invalid_argument controlに無限大の値が含まれる場合、sizesの
    ///        いずれかの要素が0以下の場合、D0, D1が単位ベクトルでない場合、または
    ///        D0・D1!=0の場合 (各軸が直交座標系を形成しない場合)
    BoundingBox(const Vector3d&, const std::array<Vector3d, 2>&,
                const std::array<double, 2>&,
                const std::array<bool, 2>& = {false, false});

    /// @brief コンストラクタ (2次元用; xy軸方向に平行な場合)
    /// @param control 基準点 P0 (左下の点)
    /// @param sizes 各方向のサイズ (x軸正方向, y軸正方向)
    /// @param is_line 各方向がkLineかどうかを示す配列. is_line[i]がfalseの場合は、
    ///        sizes[i]が有限値であればkSegment、+∞であればkRayとして扱う.
    /// @throw std::invalid_argument sizesのいずれかの要素が0以下の値の場合
    BoundingBox(const Vector3d&, const std::array<double, 2>&,
                const std::array<bool, 2>& = {false, false});

    /// @brief コンストラクタ (2/3次元共通; 2点指定)
    /// @param point1 バウンディングボックスに含まれる点1
    /// @param point2 バウンディングボックスに含まれる点2
    /// @throw std::invalid_argument point1またはpoint2に無限大の成分が含まれる場合、
    ///        point1とpoint2の2つ以上の座標値が等しい ((1,2,3)と(1,2,4)など) 場合、
    ///        またはpoint1とpoint2が同じ点の場合
    /// @note point1とpoint2が同じ座標値を持つ軸が1つの場合は2次元のバウンディングボックスを、
    ///       2つの場合は1次元のバウンディングボックスを作成する. 基本的にはD1,D2,D3を
    ///       x,y,z軸方向の単位ベクトルとするが、2次元の場合はサイズが0の方向をD2として扱い、
    ///       D0×D1=D2となるようにD0,D1を設定する (x1=x2の場合はD0=y軸,D1=z軸,D2=x軸、
    ///       y1=y2の場合はD0=z軸,D1=x軸,D2=y軸).
    BoundingBox(const Vector3d&, const Vector3d&);



    /**
     * パラメータの取得・設定
     */

    /// @brief 基準点 P0 を取得する
    const Vector3d& GetControl() const { return control_; }
    /// @brief 基準点 P0 を設定する
    /// @param control 新しい基準点
    /// @throw std::invalid_argument controlが無限大の成分を持つ場合
    void SetControl(const Vector3d&);

    /// @brief 延伸方向 D0, D1, D2 を取得する
    /// @note D2 = D0 × D1 を満たす単位ベクトル
    const std::array<Vector3d, 3>& GetDirections() const { return directions_; }
    /// @brief 延伸方向 D0, D1, D2 を設定する
    /// @param directions 新しい延伸方向 D0, D1, D2
    /// @throw std::invalid_argument directions[i]が単位ベクトルではない場合、
    ///        D0・D1!=0 または D0×D1!=D2 の場合 (各軸が直交座標系を形成しない場合)
    void SetDirections(const std::array<Vector3d, 3>&);
    /// @brief 延伸方向 D0, D1 を設定する (2次元用)
    /// @param directions 新しい延伸方向 D0, D1
    /// @throw std::invalid_argument directions[i]が単位ベクトルではない場合、
    ///        D0・D1!=0 の場合
    /// @note D2=D0×D1に設定され、sizes_[2]はゼロに設定される
    void SetDirections2D(const std::array<Vector3d, 2>&);

    /// @brief 各方向のサイズ s0, s1, s2 を取得する
    /// @note GetDirectionType(i)がkRayまたはkLineの場合は+∞を、kSegmentの場合は
    ///       正の有限値を返す. s2のみ0も許容する.
    /// @note privateメンバのsizes_とは異なる (kLineの場合も+∞を返す)
    std::array<double, 3> GetSizes() const;
    /// @brief 各方向がkLineかどうかを示す配列を取得する
    std::array<bool, 3> GetIsLines() const;
    /// @brief 各方向のサイズ s0, s1, s2 を設定する
    /// @param sizes 新しい各方向のサイズ s0, s1, s2
    /// @param is_line 各方向がkLineかどうかを示す配列
    /// @throw std::invalid_argument sizesのいずれかが負の値の場合、
    ///        またはsizes[2]以外が0の場合
    /// @note privateメンバのsizes_とは異なる (kLineの場合も+∞を与える)
    void SetSizes(const std::array<double, 3>&,
                  const std::array<bool, 3>& = {false, false, false});
    /// @brief 各方向のサイズ s0, s1 を設定する (2次元用)
    /// @param sizes 新しい各方向のサイズ s0, s1
    /// @param is_line 各方向がkLineかどうかを示す配列
    /// @throw std::invalid_argument sizesのいずれかが0以下の値の場合
    /// @note privateメンバのsizes_とは異なる (kLineの場合も+∞を与える)
    /// @note sizes_[2]は0に設定される
    void SetSizes2D(const std::array<double, 2>&,
                    const std::array<bool, 2>& = {false, false});
    /// @brief i番目の方向のサイズを設定する
    /// @param i 方向のインデックス (0~2)
    /// @param size 新しいサイズ
    /// @param is_line その方向がkLineかどうか
    /// @throw std::invalid_argument sizeが負の値の場合、
    ///        またはiが0または1の場合にsizeが0の場合
    /// @throw std::out_of_range iが0,1,2以外の場合
    void SetSize(const size_t, const double, const bool);

    /// @brief 各延伸方向の種類を取得する
    /// @return 各延伸方向の種類
    std::array<DirectionType, 3> GetDirectionTypes() const;

    /// @brief 平行移動
    /// @param vec 移動量
    /// @throw std::invalid_argument vecが無限大の成分を持つ場合
    void Translate(const Vector3d&);
    /// @brief 回転
    /// @param rot 回転行列 (3x3の直交行列)
    /// @throw std::invalid_argument rotが3x3の直交行列でない場合
    void Rotate(const Matrix3d&);
    /// @brief 回転
    /// @param rot 回転行列 (3x3の直交行列)
    /// @param center 回転の中心点
    /// @throw std::invalid_argument rotが3x3の直交行列でない場合、
    ///        またはcenterが無限大の成分を持つ場合
    void Rotate(const Matrix3d&, const Vector3d&);
    /// @brief 平行移動と回転
    /// @param rot 回転行列 (3x3の直交行列)
    /// @param vec 移動量
    /// @throw std::invalid_argument rotが3x3の直交行列でない場合、
    ///        またはvecが無限大の成分を持つ場合
    void Transform(const Matrix3d&, const Vector3d&);

    /// @brief 与えられたバウンディングボックスを包含するように拡大する
    /// @param other 拡大対象のバウンディングボックス
    /// @note 延伸方向D0~D2は変更されず、基点P0とサイズs0~s2のみが変更される
    /// @throw std::invalid_argument 延伸方向を変更せずに、otherを包含可能な
    ///        バウンディングボックスを作成できない場合 (例えば自身のD0~D2がx,y,z軸
    ///        に平行であるときに、-y方向のkRayを持つotherを与えた場合; この場合、
    ///        自身のD1方向を-y方向に変更しないと包含できない)
    void ExpandToInclude(const BoundingBox&);



    /**
     * 状態の取得
     */

    /// @brief 空であるか判定する
    /// @note デフォルトコンストラクタで作成され、サイズが変更されていない場合にtrueを返す
    bool IsEmpty() const;

    /// @brief 囲む領域が2次元であるか
    bool Is2D() const;
    /// @brief 囲む領域が3次元であるか
    bool Is3D() const { return !Is2D(); }

    /// @brief Z=0平面上にあるか
    /// @note Is2D()==trueかつd2//z軸かつp0.z()==0の場合にtrueを返す
    bool IsOnZPlane() const;

    /// @brief 囲む領域が有限であるか判定する
    /// @note いずれかのサイズが+∞の場合にfalseを返す
    bool IsFinite() const;

    /// @brief すべての頂点を取得する
    /// @return 頂点の配列 (2Dの場合は4頂点、3Dの場合は8頂点)
    /// @note 頂点の順序は保証されない. また、無限に伸びる方向がある場合は
    ///       その頂点の当該成分が±∞となる
    std::vector<Vector3d> GetVertices() const;
    /// @brief 有限な頂点を取得する
    /// @return 頂点の配列 (2Dの場合は4頂点、3Dの場合は8頂点)
    /// @note IsFinite()がfalseの場合、空の配列を返す
    std::vector<Vector3d> GetFiniteVertices() const;



    /**
     * 包含・交差判定
     */

    /// @brief 点を包含するか判定する
    /// @param point 判定する点
    /// @return pointがバウンディングボックス内にある場合にtrueを返す
    bool Contains(const Vector3d&) const;

    /// @brief 他のバウンディングボックスを完全に包含するか判定する
    /// @param other 判定するバウンディングボックス
    /// @return boxがこのバウンディングボックス内に完全に含まれる場合にtrueを返す
    bool Contains(const BoundingBox&) const;

    /// @brief 直線(半直線・線分を含む)と交差するか判定する
    /// @param start 直線の始点 (直線の場合は通過点)
    /// @param end 直線の終点 (直線・半直線の場合は通過点)
    /// @param type 直線の種類
    /// @return 線がバウンディングボックスと交差する場合にtrueを返す.
    ///         線分/半直線が完全に内包される場合にもtrueを返す.
    /// @throw std::invalid_argument startとendが同じ点の場合、
    ///        start/endに無限大やNaNの成分が含まれる場合
    bool Intersects(const Vector3d&, const Vector3d&, const DirectionType) const;

    /// @brief 点との最短距離を計算する
    /// @param point 判定する点
    /// @return pointとバウンディングボックスの最短距離
    /// @note pointがバウンディングボックス内にある場合は0を返す
    double DistanceTo(const Vector3d&) const;



 private:
    /// @brief 基底D0,D1,D2がワールド座標系と同じか
    /// @note (D0,D1,D2) = (1,0,0),(0,1,0),(0,0,1)の場合にtrueを返す
    bool IsAxisAligned() const;

    /// @brief ローカル座標系からワールド座標系への変換行列を取得する
    /// @return 変換行列 (3x3行列)
    /// @note [D0 D1 D2] を返す
    Matrix3d GetLocalToWorldRotation() const;

    /// @brief ワールド座標系からローカル座標系への変換行列を取得する
    /// @return 変換行列 (3x3行列)
    /// @note [D0 D1 D2]^-1 を返す
    Matrix3d GetWorldToLocalRotation() const;

    /// @brief ローカル座標系において点が領域内に含まれるかを判定する
    /// @param local_point ローカル座標系における点
    /// @return local_pointがバウンディングボックス内にある場合にtrueを返す
    bool ContainsLocalPoint(const Vector3d&) const;
};


}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_BOUNDING_BOX_H_
