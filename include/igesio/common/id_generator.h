/**
 * @file common/id_generator.h
 * @brief IDを生成するためのクラスを定義する
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 * @note エンティティクラスからDEデータやPDデータを取得する際には、IGESファイルとの入出力の
 *       互換性を保つためにint型のIDを使用する必要がある. しかし、int型のIDは32ビット整数であり、
 *       大規模なモデルではIDの衝突が発生する可能性がある. そこで、本クラスでは64ビット整数2つの
 *       ペアでIDを管理し、一意性を確保する. 各オブジェクトはIdentifierの共有ポインタを保持し、
 *       必要に応じて識別子情報を取得する. また、現在保持されているIdentifierについて一意な
 *       int型のIDも管理し、IGESファイルとの入出力の互換性を保つ.
 */
#ifndef IGESIO_COMMON_ID_GENERATOR_H_
#define IGESIO_COMMON_ID_GENERATOR_H_

#include <atomic>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>



namespace igesio {

/// @brief 未設定のID (int型)
/// @note IGESParameterVectorなどで、エンティティを参照しないことを示す場合
///       や、未設定である場合に使用する
constexpr int kInvalidIntID = 0;

/// @brief オブジェクトの種類を表す列挙型
/// @note IDを割り振るオブジェクトの種類
enum class ObjectType : uint8_t {
    /// @brief IGESファイルから読み込まれたエンティティ
    kEntityFromIGES = 1,
    /// @brief プログラム内で新規に作成されたエンティティ
    kEntityNew = 2,
    /// @brief エンティティ描画用のオブジェクト
    kEntityGraphics = 3,
    /// @brief IGESファイルのデータ全体を表すオブジェクト
    kIgesData = 4,
    /// @brief アセンブリオブジェクト
    kAssembly = 5,
};

/// @brief ObjectTypeを文字列に変換する
/// @param type オブジェクトの種類
/// @return 文字列
std::string ToString(const ObjectType);

/// @brief 一意な識別子を生成・管理するクラス (抽象クラス)
/// @note 各オブジェクト、およびIDGeneratorが本クラスのポインタを保持し、
///       必要に応じて識別子情報を取得する. IDは64ビット整数2つのペアで表される.
///       各ビットフィールドの構成は後述する. また、オブジェクトごとにint型のIDも保持する.
///       こちらは主にIGESParameterVectorなど、IGESファイルとの入出力との互換性を保つ
///       ために使用される (IGESファイルではint型でエンティティを参照するため).
class Identifier {
 public:
    /// @brief デストラクタ
    virtual ~Identifier() = default;

    /// @brief ユニークなID（64ビット整数のペア）を取得する
    /// @return 64ビット整数のペア
    /// @note 2つの64ビット整数からなるペアで、プログラム上で一意に識別されるIDを表す.
    virtual const std::pair<uint64_t, uint64_t>& GetUniqueID() const = 0;
    /// @brief IDのprefix部分を取得する
    virtual uint64_t GetIDPrefix() const = 0;
    /// @brief IDのsuffix部分を取得する
    virtual uint64_t GetIDSuffix() const = 0;
    /// @brief int型のIDを取得する
    /// @return int型のID
    /// @note 主にIGESParameterVectorなど、IGESファイルとの入出力との互換性を保つ
    ///       ために使用される (IGESファイルではint型でエンティティを参照するため).
    virtual int GetIntID() const = 0;

    /// @brief オブジェクトの種類を取得する
    virtual ObjectType GetObjectType() const = 0;
    /// @brief エンティティの読み込み元のIGESファイルに対応するIgesDataのint型IDを取得する
    /// @return オブジェクトがIGESファイルから読み込まれたエンティティであればそのID、
    ///         そうでなければnullopt
    virtual std::optional<uint32_t> GetIGESIntID() const = 0;
    /// @brief DEレコードのシーケンス番号を取得する
    /// @return オブジェクトがIGESファイルから読み込まれたエンティティであればその番号、
    ///         そうでなければnullopt
    virtual std::optional<uint32_t> GetDEPointer() const = 0;
    /// @brief エンティティのタイプを取得する
    /// @return オブジェクトがエンティティ関連であればそのタイプ、そうでなければnullopt
    virtual std::optional<uint16_t> GetEntityType() const = 0;
    /// @brief タイムスタンプを取得する
    /// @return IDが生成された日時 (UTC)
    virtual std::chrono::system_clock::time_point GetTimestamp() const = 0;


    /// @brief 等価比較演算子のオーバーロード
    /// @param other 比較対象のIdentifier
    /// @return prefixとsuffixの両方が等しい場合にtrueを返す
    bool operator==(const Identifier&) const;
    /// @brief 不等価比較演算子のオーバーロード
    /// @param other 比較対象のIdentifier
    /// @return prefixまたはsuffixのいずれかが異なる場合にtrueを返す
    bool operator!=(const Identifier&) const;
};



/**
 * ObjectID構造体
 */

/// @brief Identifierを保持する構造体
/// @note 各オブジェクト（Entity, IgesData, Assemblyなど）が本構造体をメンバとして持ち、
///       識別子情報を管理する
struct ObjectID {
    /// @brief Identifierの共有ポインタ
    /// @note IDGeneratorと各オブジェクトが同じIdentifierインスタンスを共有する
    std::shared_ptr<const Identifier> identifier;

 public:
    /// @brief デフォルトコンストラクタ
    /// @note STLコンテナで使用するために追加
    ObjectID() : identifier(nullptr) {}

    /// @brief コンストラクタ
    /// @param id 保持するIdentifierの共有ポインタ (IDGeneratorで生成されたもの)
    explicit ObjectID(const std::shared_ptr<const Identifier> id)
            : identifier(id) {}

    /// @brief Identifierの共有ポインタを取得する
    /// @return 変更不可なIdentifierの共有ポインタ
    const std::shared_ptr<const Identifier>& GetIdentifier() const;
    /// @brief int型のIDを取得する
    /// @return int型のID、Identifierが設定されていない場合はkInvalidIntIDを返す
    /// @note 主にIGESParameterVectorなど、IGESファイルとの入出力との互換性を保つ
    ///       ために使用される (IGESファイルではint型でエンティティを参照するため).
    int ToInt() const;
    /// @brief Identifierが設定されているかを確認する
    /// @return 設定されていればtrue、そうでなければfalse
    bool IsSet() const;

    /// @brief 等価比較演算子のオーバーロード
    /// @param other 比較対象のObjectID
    /// @return 両方のIdentifierが設定されており、prefixとsuffixの両方が等しい場合、
    ///         または両方のIdentifierがnullptrの場合にtrueを返す
    bool operator==(const ObjectID&) const;
    /// @brief 不等価比較演算子のオーバーロード
    bool operator!=(const ObjectID&) const;
    /// @brief 等価比較演算子のオーバーロード
    /// @param other 比較対象のIdentifierの共有ポインタ
    /// @return 両方のIdentifierが設定されており、prefixとsuffixの両方が等しい場合、
    ///         または両方のIdentifierがnullptrの場合にtrueを返す
    bool operator==(const std::shared_ptr<const Identifier>&) const;
    /// @brief 不等価比較演算子のオーバーロード
    bool operator!=(const std::shared_ptr<const Identifier>&) const;
};

/// @brief ObjectIDを文字列に変換する
/// @param object_id 変換するObjectID
/// @param readable_format 可読形式で出力する場合はtrue (デフォルト: false)
/// @return 文字列
/// @note フォーマット (readable_formatがfalseの場合):
///       O: ObjectType, I: IGESIntID, D: DEPointer, E: EntityType, T: Timestamp,
///       R: Random (いずれも16進数):
///       - Entity (from IGES):
///         "OO-IIIIIIII-DDDDDD-EEEE-TTTTTTTTTTTT"
///       - Entity (in program), EntityGraphics:
///         "OO-RRRRRRRRRRRRRR-EEEE-TTTTTTTTTTTT"
///       - IgesData, Assembly:
///         "OO-RRRRRRRRRRRRRR-RRRR-TTTTTTTTTTTT"
/// @note フォーマット (readable_formatがtrueの場合):
///       - Entity (from IGES):
///         "<obj-type>-<iges-int-id>-<de-pointer>-<entity-type>-<timestamp>"
///       - Entity (in program), EntityGraphics:
///         "<obj-type>-<random>-<entity-type>-<timestamp>"
///       - IgesData, Assembly:
///         "<obj-type>-<random1>-<random2>-<timestamp>"
std::string ToString(const ObjectID&, const bool = false);

/// @brief 出力ストリームへの挿入演算子のオーバーロード
std::ostream& operator<<(std::ostream&, const ObjectID&);

/// @brief DEポインターとエンティティのIDのマッピングを保持する型
using pointer2ID = std::unordered_map<unsigned int, ObjectID>;
/// @brief エンティティのIDとDEポインターのマッピングを保持する型
using id2pointer = std::unordered_map<ObjectID, unsigned int>;



/**
 * IDGeneratorクラス
 */

/// @brief 一意な識別子を生成・管理するクラス
/// @note 各オブジェクト、およびIDGeneratorが本クラスのポインタを保持し、
///       必要に応じて識別子情報を取得する. IDは64ビット整数2つのペアで表される.
///       各ビットフィールドの構成は後述する. また、オブジェクトごとにint型のIDも保持する.
///       こちらは主にIGESParameterVectorなど、IGESファイルとの入出力との互換性を保つ
///       ために使用される (IGESファイルではint型でエンティティを参照するため).
/// @note 基本的な使用方法は以下の通り:
///       (A) 一般のオブジェクトの作成時には`IDGenerator::Generate`を呼び出す
///       (B) IGESファイルからの読み込み時など、オブジェクト本体を読み込む前にIDが
///           必要な場合には`IDGenerator::Reserve`を呼び出す. その後、オブジェクト内部で
///           `IDGenerator::GetReservedID`を呼び出して予約済みIDを取得する
///       (C) オブジェクトが破棄される際には`IDGenerator::Release`を呼び出す (デストラクタ等)
///       (D) 参照がないことを示したり、一時的なプレースホルダ―を生成する場合は
///           `IDGenerator::UnsetID`を呼び出す
class IDGenerator {
    /// @brief 排他制御用ミューテックス
    /// @note IDGenerator内の静的メンバ変数へのアクセスを排他制御するために使用
    inline static std::mutex mutex_;
    /// @brief int型IDとIdentifierの弱参照ポインタのマップ
    /// @note 使用中のint型IDとそれに対応するIdentifierの弱参照ポインタを保持する.
    ///       毎回expiredしているかは確認しないため、Identifierが破棄された場合でも
    ///       そのまま残る可能性があることに注意. `Release`が明示的に呼び出された場合
    ///       には削除される
    inline static std::map<int, std::weak_ptr<Identifier>> int_id_map_;
    /// @brief 再利用可能なexpiredなint型IDの集合
    /// @note この集合に含まれるint型IDはint_id_map_には存在しない
    inline static std::set<int> expired_int_ids_;
    /// @brief 使用可能な最大のint型ID
    /// @note すべての使用中のint型IDがこの値に達した場合、新しいObjectIDを生成できなくなる
    inline static size_t max_int_id_ = std::numeric_limits<int>::max();

    /// @brief ObjectIDとentity_typeのペアのハッシュ関数
    /// @note reserved_ids_のunordered_mapで使用される
    struct PairHash {
        std::size_t operator()(const std::pair<ObjectID, uint16_t>&) const;
    };
    /// @brief 予約済みIDのマップ
    /// @note キーは(ObjectID, entity_type)のペア、値は対応するint型ID
    /// @note Reserveで予約されたIDを管理するために使用される
    inline static std::unordered_map<std::pair<ObjectID, uint16_t>,
                                     int, PairHash> reserved_ids_;

    /// @brief 新しいint型IDを生成する
    /// @throw std::runtime_error 新しいint_idを生成できなかった場合
    /// @note 以下の3段階で新しいint_idを生成
    ///       ① 使用中の最大のint_id (int_id_map_のキーの最大値) + 1が
    ///         max_int_id_未満であればそれを採用
    ///       ② そうでなければ、expired_int_ids_から最も値の小さいint_idを再利用
    ///       ③ それも見つからなければ、int_id_map_を先頭から探索し、expiredな
    ///         Identifierを見つけたらそのint_idを再利用
    ///       ③ それも見つからなければ、エラーを投げる
    static int GenerateNewIntID();

    /// @brief 新しいidentifierを登録する (上書き)
    /// @param identifier 登録するIdentifierの共有ポインタ
    /// @note int_id_map_にidentifierを登録する
    static void Register(const std::shared_ptr<Identifier>&);

 public:
    inline static ObjectID kUnsetID = ObjectID(nullptr);

    /// @brief 何も設定されていないObjectIDを取得する
    /// @return 未設定のObjectID
    static const ObjectID& UnsetID() { return kUnsetID; }

    /// @brief 新しいObjectIDを作成する (IgesData, Assembly用)
    /// @param obj_type オブジェクトの種類 (kIgesDataまたはkAssembly)
    /// @throw std::runtime_error IDを使用中のオブジェクトが多く、生成できなかった場合
    /// @throw igesio::ImplementationError obj_typeがkIgesDataまたはkAssemblyでない場合
    static ObjectID Generate(const ObjectType);
    /// @brief 新しいObjectIDを作成する (Entity (in program), EntityGraphics用)
    /// @param obj_type オブジェクトの種類 (kEntityNewまたはkEntityGraphics)
    /// @param entity_type エンティティのタイプ
    /// @throw std::runtime_error IDを使用中のオブジェクトが多く、生成できなかった場合
    /// @throw igesio::ImplementationError obj_typeがkEntityNewまたはkEntityGraphicsでない場合
    static ObjectID Generate(const ObjectType, const uint16_t);
    /// @brief 新しいObjectIDを予約する (Entity (from IGES)用)
    /// @param iges_id 読み込み元のIGESファイルに対応するIgesDataのObjectID
    /// @param entity_type エンティティのタイプ
    /// @param de_pointer DEレコードのシーケンス番号
    /// @throw std::runtime_error IDを使用中のオブジェクトが多く、生成できなかった場合
    /// @throw std::invalid_argument iges_idが未設定の場合、
    ///        またはiges_idに対応するオブジェクトがIgesDataでない場合
    static ObjectID Reserve(const ObjectID&, const uint16_t, const uint16_t);
    /// @brief 予約済みのObjectIDを取得する (Entity (from IGES)用)
    /// @param iges_id 読み込み元のIGESファイルに対応するIgesDataのObjectID
    /// @param entity_type エンティティのタイプ
    /// @param de_pointer DEレコードのシーケンス番号
    /// @throw std::runtime_error IDを使用中のオブジェクトが多く、生成できなかった場合
    /// @throw std::invalid_argument 指定されたIDが予約されていない場合、
    ///        または参照するIdentifierがexpiredの場合
    static ObjectID GetReservedID(const ObjectID&, const uint16_t);

    /// @brief int_idからObjectIDを取得する
    /// @param int_id ObjectIDに対応するint型ID
    /// @return 取得したObjectID、指定されたint_idが見つからない場合や
    ///         参照するIdentifierがexpiredの場合はstd::nulloptを返す
    static std::optional<ObjectID> TryGetByIntID(const int);
    /// @brief int_idからObjectIDを取得する
    /// @param int_id ObjectIDに対応するint型ID
    /// @throw std::invalid_argument 指定されたint_idが見つからない場合、
    ///        または参照するIdentifierがexpiredの場合
    static ObjectID GetByIntID(const int);

    /// @brief int_idを解放する
    /// @param int_id 解放するint_id
    /// @note エンティティクラスのデストラクタなど、IDの使用が終了したときに呼び出すことを想定
    static void Release(const int);
};

}  // namespace igesio



namespace std {

/// @brief igesio::ObjectIDのハッシュ関数
template<>
struct hash<igesio::ObjectID> {
    std::size_t operator()(const igesio::ObjectID& id) const {
        // prefixとsuffixの両方を組み合わせてハッシュ値を生成
        if (!id.IsSet()) {
            return 0;  // 未設定のIDの場合は0を返す
        }

        auto [prefix, suffix] = id.identifier->GetUniqueID();
        std::size_t h1 = std::hash<uint64_t>{}(prefix);
        std::size_t h2 = std::hash<uint64_t>{}(suffix);
        return h1 ^ (h2 << 1);  // シンプルな組み合わせ
    }
};

}  // namespace std

#endif  // IGESIO_COMMON_ID_GENERATOR_H_
