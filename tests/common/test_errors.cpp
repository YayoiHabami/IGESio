/**
 * @file common/test_errors.cpp
 * @brief common/errors.hのテスト
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - エラークラス階層の継承関係 (コンパイル時; static_assert)
 * - catchの粒度 (実行時; 各エラーがどのカテゴリで捕捉されるか)
 * - IGESioErrorのメッセージ整形 (型名の付加・重複回避) とtype()
 * - FileOpenError::getFilename()
 *
 * NOTE: 個別エラークラスのメッセージ整形は共通ロジック
 *       (IGESioError::formatMessage) に集約されているため、
 *       代表クラスでの検証 + 全クラスのwhat()接頭辞確認で網羅とする。
 */
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <type_traits>

#include "igesio/common/errors.h"

namespace {

namespace iio = igesio;

}  // namespace



/**
 * 継承関係 (コンパイル時の契約)
 */

// 全独自エラーはIGESioErrorを継承する
static_assert(std::is_base_of_v<iio::IGESioError, iio::NotImplementedError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::ImplementationError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::ParseError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::TypeConversionError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::FileError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::FileOpenError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::FileFormatError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::LineFormatError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::SectionFormatError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::DataFormatError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::EntityDataError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::EntityParameterError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::EntityValueError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::ReferenceError>);
static_assert(std::is_base_of_v<iio::IGESioError, iio::ComputationError>);

// 中間カテゴリの親子関係
static_assert(std::is_base_of_v<iio::EntityDataError, iio::EntityParameterError>);
static_assert(std::is_base_of_v<iio::EntityDataError, iio::EntityValueError>);
static_assert(std::is_base_of_v<iio::FileFormatError, iio::LineFormatError>);
static_assert(std::is_base_of_v<iio::FileFormatError, iio::SectionFormatError>);
static_assert(std::is_base_of_v<iio::FileFormatError, iio::DataFormatError>);
static_assert(std::is_base_of_v<iio::ParseError, iio::TypeConversionError>);
static_assert(std::is_base_of_v<iio::FileError, iio::FileOpenError>);

// IGESioErrorはstd::runtime_error系であり、std::logic_error系ではない
// (独自エラー=実行時/ドメインエラー、std::logic_error系=呼び出し側バグの分離)
static_assert(std::is_base_of_v<std::runtime_error, iio::IGESioError>);
static_assert(!std::is_base_of_v<std::logic_error, iio::IGESioError>);

// ReferenceError/ComputationErrorはEntityDataErrorの兄弟 (配下ではない)
static_assert(!std::is_base_of_v<iio::EntityDataError, iio::ReferenceError>);
static_assert(!std::is_base_of_v<iio::EntityDataError, iio::ComputationError>);

// EntityDataError系はファイル解析層 (FileFormatError) とは独立した枝
static_assert(!std::is_base_of_v<iio::FileFormatError, iio::EntityDataError>);
static_assert(!std::is_base_of_v<iio::EntityDataError, iio::DataFormatError>);



/**
 * catchの粒度 (実行時)
 */

namespace {

/// @brief 例外を投げ、カテゴリ別のcatch節のうちどれで捕捉されたかを返す
/// @tparam E 投げる例外の型
/// @param e 投げる例外
/// @return 捕捉したcatch節のカテゴリ名
template <typename E>
std::string CaughtCategory(const E& e) {
    try {
        throw e;
    } catch (const iio::EntityDataError&) {
        return "EntityDataError";
    } catch (const iio::FileFormatError&) {
        return "FileFormatError";
    } catch (const iio::ParseError&) {
        return "ParseError";
    } catch (const iio::FileError&) {
        return "FileError";
    } catch (const iio::IGESioError&) {
        return "IGESioError";
    }
}

}  // namespace

// エンティティ系エラーはEntityDataErrorとして捕捉される
TEST(ErrorCatchTest, EntityErrors_CaughtAsEntityDataError) {
    EXPECT_EQ(CaughtCategory(iio::EntityParameterError("msg")), "EntityDataError");
    EXPECT_EQ(CaughtCategory(iio::EntityValueError("msg")), "EntityDataError");
    EXPECT_EQ(CaughtCategory(iio::EntityDataError("msg")), "EntityDataError");
}

// ファイル解析系エラーはFileFormatErrorとして捕捉される
// (エンティティ層のEntityDataErrorとは別系統)
TEST(ErrorCatchTest, FormatErrors_CaughtAsFileFormatError) {
    EXPECT_EQ(CaughtCategory(iio::DataFormatError("msg")), "FileFormatError");
    EXPECT_EQ(CaughtCategory(iio::LineFormatError("msg")), "FileFormatError");
    EXPECT_EQ(CaughtCategory(iio::SectionFormatError("msg")), "FileFormatError");
}

// 参照解決・計算エラーはどの中間カテゴリにも属さず、
// IGESioErrorまで伝播して捕捉される
TEST(ErrorCatchTest, ReferenceAndComputationErrors_CaughtOnlyAsIGESioError) {
    EXPECT_EQ(CaughtCategory(iio::ReferenceError("msg")), "IGESioError");
    EXPECT_EQ(CaughtCategory(iio::ComputationError("msg")), "IGESioError");
}

// 解析・型変換エラーはParseErrorとして捕捉される
TEST(ErrorCatchTest, ParseErrors_CaughtAsParseError) {
    EXPECT_EQ(CaughtCategory(iio::TypeConversionError("msg")), "ParseError");
    EXPECT_EQ(CaughtCategory(iio::ParseError("msg")), "ParseError");
}

// 全独自エラーはstd::runtime_errorとしても捕捉可能 (後方互換)
TEST(ErrorCatchTest, AllErrors_CaughtAsStdRuntimeError) {
    auto caught_as_runtime_error = [](const auto& e) {
        try {
            throw e;
        } catch (const std::runtime_error&) {
            return true;
        } catch (...) {
            return false;
        }
    };
    EXPECT_TRUE(caught_as_runtime_error(iio::EntityParameterError("msg")));
    EXPECT_TRUE(caught_as_runtime_error(iio::EntityValueError("msg")));
    EXPECT_TRUE(caught_as_runtime_error(iio::ReferenceError("msg")));
    EXPECT_TRUE(caught_as_runtime_error(iio::ComputationError("msg")));
    EXPECT_TRUE(caught_as_runtime_error(iio::ImplementationError("msg")));
    EXPECT_TRUE(caught_as_runtime_error(iio::DataFormatError("msg")));
}



/**
 * メッセージ整形・type()
 */

// what()はメッセージの冒頭にクラス名を付加する
TEST(ErrorMessageTest, What_PrependsTypeName) {
    EXPECT_STREQ(iio::EntityParameterError("too few parameters").what(),
                 "EntityParameterError: too few parameters");
    EXPECT_STREQ(iio::EntityValueError("radius too small").what(),
                 "EntityValueError: radius too small");
    EXPECT_STREQ(iio::EntityDataError("invalid data").what(),
                 "EntityDataError: invalid data");
    EXPECT_STREQ(iio::ReferenceError("pointer not set").what(),
                 "ReferenceError: pointer not set");
    EXPECT_STREQ(iio::ComputationError("failed to converge").what(),
                 "ComputationError: failed to converge");
    EXPECT_STREQ(iio::ImplementationError("unknown enum value").what(),
                 "ImplementationError: unknown enum value");
}

// メッセージが既に "型名: " で始まる場合は二重に付加しない
TEST(ErrorMessageTest, What_AvoidsDuplicatedTypeName) {
    EXPECT_STREQ(iio::EntityValueError("EntityValueError: already prefixed").what(),
                 "EntityValueError: already prefixed");
}

// 境界: 型名で始まるが ": " が続かない場合は通常通り付加される
TEST(ErrorMessageTest, What_PrependsTypeNameWhenPrefixIsIncomplete) {
    EXPECT_STREQ(iio::EntityValueError("EntityValueErrorX").what(),
                 "EntityValueError: EntityValueErrorX");
}

// type()はコンストラクタに渡したエラータイプを返す
TEST(ErrorMessageTest, Type_ReturnsTypeName) {
    std::string type_name = "MyType";
    std::string msg = "msg";
    EXPECT_EQ(iio::IGESioError(type_name, msg).type(), type_name);
    EXPECT_STREQ(iio::IGESioError(type_name, msg).what(), (type_name + ": " + msg).c_str());
}

// タイプなしコンストラクタはメッセージのみを保持する
TEST(ErrorMessageTest, Type_EmptyWhenConstructedWithoutType) {
    iio::IGESioError error("plain message");
    EXPECT_EQ(error.type(), "");
    EXPECT_STREQ(error.what(), "plain message");
}



/**
 * FileOpenError
 */

// getFilename()は開けなかったファイルのパスを返す
TEST(FileOpenErrorTest, GetFilename_ReturnsPath) {
    iio::FileOpenError error("path/to/missing.igs");
    EXPECT_EQ(error.getFilename(), "path/to/missing.igs");
    EXPECT_STREQ(error.what(), "FileOpenError: path/to/missing.igs");
}
