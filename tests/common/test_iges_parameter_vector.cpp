/**
 * @file common/test_iges_parameter_vector.cpp
 * @brief common/iges_parameter_vector.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>

#include "igesio/common/iges_parameter_vector.h"



TEST(IGESParameterVectorTest, DefaultConstructor) {
    igesio::IGESParameterVector vec;
    ASSERT_EQ(vec.size(), 0);
}

TEST(IGESParameterVectorTest, VecParamTypeDefinition) {
    using CppPT = igesio::CppParameterType;

    igesio::VecParamType v1 = true;
    igesio::VecParamType v2 = 42;
    igesio::VecParamType v3 = 3.14;
    igesio::VecParamType v4 = igesio::ObjectID(nullptr);
    igesio::VecParamType v5 = std::string("test");

    EXPECT_TRUE(std::holds_alternative<bool>(v1));
    EXPECT_EQ(v1.index(), 0);
    EXPECT_EQ(static_cast<CppPT>(v1.index()), CppPT::kBool);

    EXPECT_TRUE(std::holds_alternative<int>(v2));
    EXPECT_EQ(v2.index(), 1);
    EXPECT_EQ(static_cast<CppPT>(v2.index()), CppPT::kInt);

    EXPECT_TRUE(std::holds_alternative<double>(v3));
    EXPECT_EQ(v3.index(), 2);
    EXPECT_EQ(static_cast<CppPT>(v3.index()), CppPT::kDouble);

    EXPECT_TRUE(std::holds_alternative<igesio::ObjectID>(v4));
    EXPECT_EQ(v4.index(), 3);
    EXPECT_EQ(static_cast<CppPT>(v4.index()), CppPT::kPointer);

    EXPECT_TRUE(std::holds_alternative<std::string>(v5));
    EXPECT_EQ(v5.index(), 4);
    EXPECT_EQ(static_cast<CppPT>(v5.index()), CppPT::kString);
}

TEST(IGESParameterVectorTest, InitializerListConstructor) {
    igesio::IGESParameterVector vec =
            {false, 1, 2.0, igesio::ObjectID(nullptr), std::string("three")};
    ASSERT_EQ(vec.size(), 5);

    EXPECT_EQ(vec.get_type(0), igesio::CppParameterType::kBool);
    EXPECT_NO_THROW(vec.get<bool>(0));
    EXPECT_EQ(vec.get<bool>(0), false);

    EXPECT_EQ(vec.get_type(1), igesio::CppParameterType::kInt);
    EXPECT_NO_THROW(vec.get<int>(1));
    EXPECT_EQ(vec.get<int>(1), 1);

    EXPECT_EQ(vec.get_type(2), igesio::CppParameterType::kDouble);
    EXPECT_NO_THROW(vec.get<double>(2));
    EXPECT_EQ(vec.get<double>(2), 2.0);

    EXPECT_EQ(vec.get_type(3), igesio::CppParameterType::kPointer);
    EXPECT_NO_THROW(vec.get<igesio::ObjectID>(3));
    EXPECT_EQ(vec.get<igesio::ObjectID>(3), igesio::ObjectID(nullptr));

    EXPECT_EQ(vec.get_type(4), igesio::CppParameterType::kString);
    EXPECT_NO_THROW(vec.get<std::string>(4));
    EXPECT_EQ(vec.get<std::string>(4), "three");
}

TEST(IGESParameterVectorTest, Resize) {
    igesio::IGESParameterVector vec;
    vec.resize(5);
    ASSERT_EQ(vec.size(), 5);
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.get<int>(i), 0);
    }
}

TEST(IGESParameterVectorTest, ResizeWithDefaultValue) {
    igesio::IGESParameterVector vec;
    vec.resize(3, 1.5);
    ASSERT_EQ(vec.size(), 3);
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.get<double>(i), 1.5);
    }
}

TEST(IGESParameterVectorTest, ResizeWithDefaultValueAndFormat) {
    igesio::IGESParameterVector vec;
    vec.resize(2, std::string("4Htest"), igesio::ValueFormat::String());
    ASSERT_EQ(vec.size(), 2);
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec.get<std::string>(i), "4Htest");
    }
}

TEST(IGESParameterVectorTest, Reserve) {
    igesio::IGESParameterVector vec;
    vec.reserve(10);
    ASSERT_EQ(vec.capacity(), 10);
}



/**
 * 要素へのアクセス
 */

TEST(IGESParameterVectorTest, PushBack) {
    igesio::IGESParameterVector vec;
    vec.push_back(1);
    vec.push_back(2.5);
    vec.push_back(std::string("test"));
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.get<int>(0), 1);
    EXPECT_EQ(vec.get<double>(1), 2.5);
    EXPECT_EQ(vec.get<std::string>(2), "test");
}

TEST(IGESParameterVectorTest, Set) {
    igesio::IGESParameterVector vec = {1, 2.0, "three"};
    vec.set(0, 5);
    vec.set(1, 3.14);
    vec.set(2, std::string("new_string"));
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.get<int>(0), 5);
    EXPECT_EQ(vec.get<double>(1), 3.14);
    EXPECT_EQ(vec.get<std::string>(2), "new_string");
}

TEST(IGESParameterVectorTest, SetOutOfRange) {
    igesio::IGESParameterVector vec;
    ASSERT_THROW(vec.set(0, 5), std::out_of_range);
}

TEST(IGESParameterVectorTest, Get) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    ASSERT_EQ(vec.get<int>(0), 1);
    ASSERT_EQ(vec.get<double>(1), 2.0);
    ASSERT_EQ(vec.get<std::string>(2), "three");
}

TEST(IGESParameterVectorTest, GetOutOfRange) {
    igesio::IGESParameterVector vec;
    ASSERT_THROW(vec.get<int>(0), std::out_of_range);
}

TEST(IGESParameterVectorTest, GetBadVariantAccess) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    ASSERT_THROW(vec.get<bool>(0), std::bad_variant_access);
    ASSERT_THROW(vec.get<int>(1), std::bad_variant_access);
    ASSERT_THROW(vec.get<double>(2), std::bad_variant_access);
}

TEST(IGESParameterVectorTest, GetAsString) {
    igesio::IGESParameterVector vec = {1, 2.5, std::string("test")};
    igesio::SerializationConfig config;
    EXPECT_EQ(vec.get_as_string(0, config), "1");
    EXPECT_EQ(vec.get_as_string(1, config), "2.5");
    EXPECT_EQ(vec.get_as_string(2, config), "4Htest");
}

TEST(IGESParameterVectorTest, Clear) {
    igesio::IGESParameterVector vec = {1, 2.0, "three"};
    vec.clear();
    ASSERT_EQ(vec.size(), 0);
}

TEST(IGESParameterVectorTest, Copy) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three"), 4, 5.0};
    igesio::IGESParameterVector copied_vec = vec.copy(1, 3);
    ASSERT_EQ(copied_vec.size(), 3);
    EXPECT_EQ(copied_vec.get<double>(0), 2.0);
    EXPECT_EQ(copied_vec.get<std::string>(1), "three");
    EXPECT_EQ(copied_vec.get<int>(2), 4);
}

TEST(IGESParameterVectorTest, CopyOutOfRange) {
    igesio::IGESParameterVector vec = {1, 2.0, "three"};
    ASSERT_THROW(vec.copy(1, 5), std::out_of_range);
    ASSERT_THROW(vec.copy(5, 1), std::out_of_range);
}

TEST(IGESParameterVectorTest, AccessAsBoolFromInt) {
    igesio::IGESParameterVector vec = {1};
    bool val = vec.access_as<bool>(0);
    ASSERT_EQ(val, true);

    igesio::IGESParameterVector vec2 = {0};
    val = vec2.access_as<bool>(0);
    ASSERT_EQ(val, false);
}

TEST(IGESParameterVectorTest, AccessAsObjectIDFromInt) {
    // 登録済みのint型IDであれば、ObjectIDに変換できる
    auto id = igesio::IDGenerator::Generate(igesio::ObjectType::kIgesData);

    igesio::IGESParameterVector vec = {id.ToInt()};
    igesio::ObjectID val = vec.access_as<igesio::ObjectID>(0);
    ASSERT_EQ(val, id);
}

TEST(IGESParameterVectorTest, AccessAsBadVariantAccess) {
    igesio::IGESParameterVector vec = {std::string("test")};
    ASSERT_THROW(vec.access_as<bool>(0), std::bad_variant_access);
    ASSERT_THROW(vec.access_as<igesio::ObjectID>(0), std::bad_variant_access);

    // 未登録のint型IDはObjectIDに変換できない
    igesio::IGESParameterVector vec2 = {999999};
    ASSERT_THROW(vec2.access_as<igesio::ObjectID>(0), std::bad_variant_access);
}



/**
 * 要素の検証など
 */

TEST(IGESParameterVectorTest, IsType) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    ASSERT_TRUE(vec.is_type<int>(0));
    ASSERT_TRUE(vec.is_type<double>(1));
    ASSERT_TRUE(vec.is_type<std::string>(2));
    ASSERT_FALSE(vec.is_type<bool>(0));
    ASSERT_FALSE(vec.is_type<igesio::ObjectID>(1));
}

TEST(IGESParameterVectorTest, IsTypeOutOfRange) {
    igesio::IGESParameterVector vec;
    ASSERT_THROW(vec.is_type<int>(0), std::out_of_range);
}

TEST(IGESParameterVectorTest, GetType) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    ASSERT_EQ(vec.get_type(0), igesio::CppParameterType::kInt);
    ASSERT_EQ(vec.get_type(1), igesio::CppParameterType::kDouble);
    ASSERT_EQ(vec.get_type(2), igesio::CppParameterType::kString);
}

TEST(IGESParameterVectorTest, GetTypeOutOfRange) {
    igesio::IGESParameterVector vec;
    ASSERT_THROW(vec.get_type(0), std::out_of_range);
}

TEST(IGESParameterVectorTest, GetFormat) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    ASSERT_EQ(vec.get_format(0).type, igesio::IGESParameterType::kInteger);
    ASSERT_EQ(vec.get_format(1).type, igesio::IGESParameterType::kReal);
    ASSERT_EQ(vec.get_format(2).type, igesio::IGESParameterType::kString);
}

TEST(IGESParameterVectorTest, GetFormatOutOfRange) {
    igesio::IGESParameterVector vec;
    ASSERT_THROW(vec.get_format(0), std::out_of_range);
}

TEST(IGESParameterVectorTest, SetFormat) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};

    // int -> doubleは許可されない
    igesio::ValueFormat new_format = igesio::ValueFormat::Real();
    ASSERT_THROW(vec.set_format(0, new_format), std::invalid_argument);

    ASSERT_EQ(vec.get_format(0).type, igesio::IGESParameterType::kInteger);
}

TEST(IGESParameterVectorTest, SetFormatOutOfRange) {
    igesio::IGESParameterVector vec;
    igesio::ValueFormat new_format = igesio::ValueFormat::Integer();
    ASSERT_THROW(vec.set_format(0, new_format), std::out_of_range);
}

TEST(IGESParameterVectorTest, SetFormatInvalidType) {
    igesio::IGESParameterVector vec = {1, 2.0, std::string("three")};
    igesio::ValueFormat new_format = igesio::ValueFormat::String();
    ASSERT_THROW(vec.set_format(0, new_format), std::invalid_argument);
}

TEST(IGESParameterVectorTest, Size) {
    igesio::IGESParameterVector vec;
    ASSERT_EQ(vec.size(), 0);
    vec.push_back(1);
    ASSERT_EQ(vec.size(), 1);
    vec.push_back(2.0);
    ASSERT_EQ(vec.size(), 2);
}

TEST(IGESParameterVectorTest, Empty) {
    igesio::IGESParameterVector vec;
    ASSERT_TRUE(vec.empty());
    vec.push_back(1);
    ASSERT_FALSE(vec.empty());
    vec.clear();
    ASSERT_TRUE(vec.empty());
}



/**
 * 出力ストリーム
 */

TEST(IGESParameterVectorTest, OutputStream) {
    std::stringstream ss;
    auto id = igesio::IDGenerator::Generate(igesio::ObjectType::kIgesData);
    igesio::IGESParameterVector vec = {1, 2.5, std::string("test"), true, id};
    ss << vec;
    EXPECT_EQ(ss.str(), "[1, 2.5, test, 1, " + ToString(id) + "]");

    std::stringstream ss2;
    igesio::IGESParameterVector vec2 = {3.0};
    ss2 << vec2;
    EXPECT_EQ(ss2.str(), "[3.0]");

    std::stringstream ss3;
    igesio::IGESParameterVector vec3 = {};
    ss3 << vec3;
    EXPECT_EQ(ss3.str(), "[]");
}

