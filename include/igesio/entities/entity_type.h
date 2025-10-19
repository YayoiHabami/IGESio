/**
 * @file entities/entity_type.h
 * @brief IGESで定義されるエンティティタイプの列挙型
 * @author Yayoi Habami
 * @date 2025-04-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_ENTITY_TYPE_H_
#define IGESIO_ENTITIES_ENTITY_TYPE_H_

#include <cstdint>
#include <optional>
#include <string>

/// @brief IGESのエンティティ型やその操作を定義する
namespace igesio::entities {

/// @brief IGESで定義されるエンティティタイプの列挙型
/// @note 重要性の低そうなものについてはbriefで名前を提示するのみにとどめる
/// @note 数字はUS PROによるIGES 5.3の仕様書 (ANS US PRO/IPO-100-1996) に従う
/// @note 全てのエンティティは、次の5つのエンティティクラスのいずれかに分類される:
///       (1) 曲線・面形状エンティティ (curve and surface geometry entities)
///       (2) 構成的ソリッド幾何エンティティ (constructive solid geometry entities)
///       (3) 境界表現ソリッドエンティティ (boundary representation solid entities)
///       (4) 注釈エンティティ (annotation entities)
///       (5) 構造エンティティ (structure entities)
/// @note 「‡」マークがついているものは、IGES 5.3でUNTESTEDとされているもの.
///       これらは、実装の際に特別な配慮をすることを推奨されている
enum class EntityType : uint16_t {
    /// @brief Nullエンティティ (構造クラス)
    /// @note プロセッサによって無視されることを意図したデータ
    kNull = 0,
    /// @brief 円弧 (曲線・面形状クラス)
    /// @note 一定半径の単純な円弧
    kCircularArc = 100,
    /// @brief 複合曲線 (曲線・面形状クラス)
    /// @note 他の曲線をグループ化して形成される曲線、
    ///       Line, Circle, Spline, Conicなどを組み合わせて形成される
    kCompositeCurve = 102,
    /// @brief 円錐曲線 (曲線・面形状クラス)
    /// @note 楕円、放物線、双曲線などを定義可能な曲線
    kConicArc = 104,
    /// @brief 点群 (曲線・面形状クラス(FORMS 1-13, 63); 注釈クラス(FORMS 20-40))
    /// @note (x,y)、(x,y,z)、または(x,y,z,i,j,k)で定義される点を
    ///       複数定義する.
    ///       1つ目の数字が点のタイプ（1:(x,y)、2:(x,y,z)、3:(x,y,z,i,j,k)）
    ///       2つ目の数字が点の数を表す.
    kCopiousData = 106,
    /// @brief 平面 (曲線・面形状クラス)
    /// @note Ax + By + Cz = Dの形式で定義される平面.
    ///       指定された頂点に指定されたサイズの表示シンボルも提供する
    kPlane = 108,
    /// @brief 線分
    /// @note 始点と終点を指定して定義される線分
    kLine = 110,
    /// @brief パラメトリックスプライン曲線 (曲線・面形状クラス)
    kParametricSplineCurve = 112,
    /// @brief パラメトリックスプライン曲面 (曲線・面形状クラス)
    kParametricSplineSurface = 114,
    /// @brief 点 (曲線・面形状クラス)
    /// @note 3次元空間内の点を定義する
    kPoint = 116,
    /// @brief ルールド曲面 (曲線・面形状クラス)
    /// @note 定義された曲線間の領域をスイープして形成される曲面
    kRuledSurface = 118,
    /// @brief 回転曲面 (曲線・面形状クラス)
    /// @note 境界面を指定された軸を中心に回転させて形成される曲面
    kSurfaceOfRevolution = 120,
    /// @brief Tabulated Cylinder (曲線・面形状クラス)
    /// @note 準線と呼ばれる曲線に沿って、線分を自身と平行に移動させることで形成される曲面.
    ///       曲線は、直線、円弧、円錐曲線、パラメトリックスプライン曲線、
    ///       有理Bスプライン曲線のいずれか
    kTabulatedCylinder = 122,
    /// @brief 方向‡ (境界表現ソリッドクラス)
    /// @note 3次元における x^2 + y^2 + z^2 > 0 の形式で表現される方向
    kDirection = 123,
    /// @brief 変換行列 (曲線・面形状クラス)
    /// @note R11~R33, T1~T3の形式で表現される、回転行列と平行移動ベクトル
    kTransformationMatrix = 124,
    /// @brief flash (曲線・面形状クラス)
    kFlash = 125,
    /// @brief 有理Bスプライン曲線 (実質的にはNURBS曲線) (曲線・面形状クラス)
    kRationalBSplineCurve = 126,
    /// @brief 有理Bスプライン曲面 (実質的にはNURBS曲面) (曲線・面形状クラス)
    kRationalBSplineSurface = 128,
    /// @brief オフセット曲線 (曲線・面形状クラス)
    /// @note 曲線のオフセットを決定するためのデータ
    /// @note 他の曲線を指定し、そのオフセットを定義する
    kOffsetCurve = 130,
    /// @brief connect point (構造クラス)
    kConnectPoint = 132,
    /// @brief node (構造クラス)
    kNode = 134,
    /// @brief finite element‡ (構造クラス)
    kFiniteElement = 136,
    /// @brief nodal displacement and rotation (構造クラス)
    kNodalDisplacementAndRotation = 138,
    /// @brief オフセット曲面 (曲線・面形状クラス)
    /// @note 特定のサーフェスのオフセットサーフェスを計算するためのデータ
    kOffsetSurface = 140,
    /// @brief 境界‡ (曲線・面形状クラス)
    /// @note 曲面上にある曲線で構成される曲面境界
    kBoundary = 141,
    /// @brief 曲面上の曲線 (曲線・面形状クラス)
    /// @note 与えられた曲線が与えられたサーフェス上にあることを識別する
    kCurveOnAParametricSurface = 142,
    /// @brief 境界付き曲面‡ (曲線・面形状クラス)
    /// @note 境界エンティティにより囲まれた曲面
    kBoundedSurface = 143,
    /// @brief トリム面 (曲線・面形状クラス)
    /// @note 境界エンティティによりトリムされた曲面
    kTrimmedSurface = 144,
    /// @brief nodal results‡ (構造クラス)
    kNodalResults = 146,
    /// @brief element results‡ (構造クラス)
    kElementResults = 148,
    /// @brief 直方体 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note (X1,Y1,Z1)を頂点とし、ローカルのX軸とZ軸により定義される、
    ///       LX,LY,LZのサイズを持つ直方体
    kBlock = 150,
    /// @brief 直角ウェッジ (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 直方体と類似の形式で定義された、楔形の6面体
    kRightAngularWedge = 152,
    /// @brief 直円柱 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 円柱中心、単位ベクトル、高さ、半径により定義される円柱
    kRightCircularCylinder = 154,
    /// @brief 直円錐台 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 直円柱と類似の形式で定義された、円錐台
    kRightCircularCone = 156,
    /// @brief 球 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 球の中心、半径により定義される球
    kSphere = 158,
    /// @brief トーラス (構成的ソリッド幾何クラス(プリミティブ))
    /// @note トーラスの中心、円板半径、トーラスの半径、軸方向ベクトル
    ///       により定義されるトーラス
    kTorus = 160,
    /// @brief 回転体 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 平面曲線により決定される領域を、指定された同一平面上の軸を
    ///       中心に回転させることで形成される立体
    kSolidOfRevolution = 162,
    /// @brief 直線押し出し体 (構成的ソリッド幾何クラス(プリミティブ))
    /// @note 平面曲線により決まる領域を、平行移動させることにより形成される立体
    kSolidOfLinearExtrusion = 164,
    /// @brief 楕円体 (構成的ソリッド幾何クラス(プリミティブ))
    kEllipsoid = 168,
    /// @brief ブールツリー構造 (構成的ソリッド幾何クラス (統合用))
    /// @note 後順序記法で定義された、ブール演算とオペランドから構成される
    ///       二分木構造を表現する. ブール演算は和集合、差集合、積集合を使用可能
    kBooleanTree = 180,
    /// @brief selected component‡ (構成的ソリッド幾何クラス (統合用))
    kSelectedComponent = 182,
    /// @brief solid assembly (構成的ソリッド幾何クラス (統合用))
    kSolidAssembly = 184,
    /// @brief 多様体B-Repオブジェクト‡ (境界表現ソリッドクラス)
    kManifoldSolidBRepObject = 186,
    /// @brief 平面 (サーフェス)‡ (曲線・面形状クラス)
    /// @note 平面上の点と法線により定義される無限平面
    kPlaneSurface = 190,
    /// @brief 直円筒面‡ (曲線・面形状クラス)
    /// @note 無限に延びる円筒面を定義する
    kRightCircularCylinderSurface = 192,
    /// @brief 直円錐面‡ (曲線・面形状クラス)
    /// @note 無限に延びる円錐面を定義する
    kRightCircularConicalSurface = 194,
    /// @brief 球面‡ (曲線・面形状クラス)
    /// @note 中心点と半径により定義される曲面
    kSphericalSurface = 196,
    /// @brief トーラス面‡ (曲線・面形状クラス)
    kToroidalSurface = 198,
    /// @brief angular dimension (注釈クラス)
    kAngularDimension = 202,
    /// @brief curve dimension‡ (注釈クラス)
    kCurveDimension = 204,
    /// @brief diameter dimension (注釈クラス)
    kDiameterDimension = 206,
    /// @brief flag note (注釈クラス)
    kFlagNote = 208,
    /// @brief general label (注釈クラス)
    kGeneralLabel = 210,
    /// @brief general note‡ (注釈クラス)
    kGeneralNote = 212,
    /// @brief new general note‡ (注釈クラス)
    kNewGeneralNote = 213,
    /// @brief leader (arrow) (注釈クラス)
    kLeaderArrow = 214,
    /// @brief linear dimension‡ (注釈クラス)
    /// @note FORM 0を除きUNTESTED
    kLinearDimension = 216,
    /// @brief ordinate dimension‡ (注釈クラス)
    /// @note FORM 0を除きUNTESTED
    kOrdinateDimension = 218,
    /// @brief point dimension (注釈クラス)
    kPointDimension = 220,
    /// @brief radius dimension (注釈クラス)
    kRadiusDimension = 222,
    /// @brief general symbol‡ (注釈クラス)
    /// @note FORM 0を除きUNTESTED
    kGeneralSymbol = 228,
    /// @brief sectioned area‡ (注釈クラス)
    kSectionedArea = 230,
    /// @brief associativity definition (構造クラス)
    kAssociativityDefinition = 302,
    /// @brief line font definition (構造クラス)
    kLineFontDefinition = 304,
    /// @brief MACRO definition‡ (構造クラス)
    kMacroDefinition = 306,
    /// @brief サブフィギュア定義 (構造クラス)
    kSubfigureDefinition = 308,
    /// @brief text font (構造クラス)
    kTextFont = 310,
    /// @brief text display template (構造クラス)
    kTextDisplayTemplate = 312,
    /// @brief 色定義 (構造クラス)
    /// @note グラフィックデバイスの輝度レベルに対する原色(R,G,B)の関係を規定する.
    ///       数値は0~100(%)の範囲で指定される
    /// @note `314,R,G,B,[Name];`の形式で定義される
    kColorDefinition = 314,
    /// @brief units data‡ (構造クラス)
    kUnitsData = 316,
    /// @brief network subfigure definition (構造クラス)
    kNetworkSubfigureDefinition = 320,
    /// @brief attribute table definition (構造クラス)
    kAttributeTableDefinition = 322,
    /// @brief associativity instance‡ (構造クラス)
    /// @note FORMS 19-21はUNTESTED
    kAssociativityInstance = 402,
    /// @brief 図面‡ (構造クラス)
    /// @note 注釈エンティティとビュー(410)の集合として図面を指定する
    /// @note FORM 1のみUNTESTED
    kDrawing = 404,
    /// @brief プロパティ‡ (構造クラス)
    /// @note ディレクトリエントリ部で指定されたフォーム番号により指定される、
    ///       様々な種類のプロパティを定義する
    /// @note FORMS 18-36はUNTESTED
    kProperty = 406,
    /// @brief サブフィギュアインスタンス (構造クラス)
    /// @note 定義されたサブフィギュア(308)の単一インスタンスを定義する
    kSingularSubfigureInstance = 408,
    /// @brief ビュー‡ (構造クラス)
    /// @note 3次元モデル空間における、オブジェクトの表示方向を指定する
    /// @note FORM 1はUNTESTED
    kView = 410,
    /// @brief rectangular array subfigure instance (構造クラス)
    kRectangularArraySubfigureInstance = 412,
    /// @brief circular array subfigure instance (構造クラス)
    kCircularArraySubfigureInstance = 414,
    /// @brief external reference‡ (構造クラス)
    /// @note FORMS 3-4はUNTESTED
    kExternalReference = 416,
    /// @brief nodal load/constraint (構造クラス)
    kNodalLoadConstraint = 418,
    /// @brief network subfigure instance (構造クラス)
    kNetworkSubfigureInstance = 420,
    /// @brief attribute table instance (構造クラス)
    kAttributeTableInstance = 422,
    /// @brief solid instance (構成的ソリッド幾何クラス (統合用))
    kSolidInstance = 430,
    /// @brief vertex‡ (境界表現ソリッドクラス)
    kVertex = 502,
    /// @brief edge‡ (境界表現ソリッドクラス)
    kEdge = 504,
    /// @brief loop‡ (境界表現ソリッドクラス)
    kLoop = 508,
    /// @brief face‡ (境界表現ソリッドクラス)
    kFace = 510,
    /// @brief shell‡ (境界表現ソリッドクラス)
    kShell = 514
};

/// @brief 数値をエンティティタイプに変換する
/// @param n エンティティタイプ番号
/// @return エンティティタイプ、変換できない場合はstd::nullopt.
///         使用するためには、`result.has_value()`で確認し、
///         `result.value()`でEntityType型の要素を取得すること
std::optional<EntityType> ToEntityType(const int);

/// @brief エンティティタイプを文字列に変換する
/// @param type エンティティタイプ
std::string ToString(const EntityType);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_ENTITY_TYPE_H_
