// Copyright 2010-2014 RethinkDB, all rights reserved.
#include "unittest/gtest.hpp"

#include "parsing/utf8.hpp"
#include "unittest/unittest_utils.hpp"

namespace unittest {

TEST(UTF8ValidationTest, EmptyStrings) {
    ASSERT_TRUE(utf8::is_valid(""));
    ASSERT_TRUE(utf8::is_valid(std::string("")));
    ASSERT_TRUE(utf8::is_valid(datum_string_t("")));
}

TEST(UTF8ValidationTest, SimplePositives) {
    ASSERT_TRUE(utf8::is_valid("foo"));
    ASSERT_TRUE(utf8::is_valid(std::string("foo")));
    ASSERT_TRUE(utf8::is_valid(datum_string_t("foo")));
}

TEST(UTF8ValidationTest, ValidSurrogates) {
    // U+0024 $
    ASSERT_TRUE(utf8::is_valid("foo$"));
    // U+00A2 cent sign
    ASSERT_TRUE(utf8::is_valid("foo\xc2\xa2"));
    // U+20AC euro sign
    ASSERT_TRUE(utf8::is_valid("foo\xe2\x82\xac"));
    // U+10348 hwair
    ASSERT_TRUE(utf8::is_valid("foo\xf0\x90\x8d\x88"));

    // From RFC 3629 examples:
    // U+0041 U+2262 U+0391 U+002E A<NOT IDENTICAL TO><ALPHA>
    ASSERT_TRUE(utf8::is_valid("\x41\xe2\x89\xa2\xce\x91\x2e"));
    // U+D55C U+AD6D U+C5B4 Korean "hangugeo", the Korean language
    ASSERT_TRUE(utf8::is_valid("\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4"));
    // U+65E5 U+672C U+8A9E Japanese "nihongo", the Japanese language
    ASSERT_TRUE(utf8::is_valid("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));
    // U+233B4 Chinese character meaning 'stump of a tree' prefixed with a UTF-8 BOM
    ASSERT_TRUE(utf8::is_valid("\xef\xbb\xbf\xf0\xa3\x8e\xb4"));
}

TEST(UTF8ValidationTest, InvalidCharacters) {
    utf8::reason_t reason;

    // totally incoherent
    ASSERT_FALSE(utf8::is_valid("\xff"));

    ASSERT_FALSE(utf8::is_valid("\xff", &reason));
    ASSERT_EQ(0, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);
    // also illegal
    ASSERT_FALSE(utf8::is_valid("\xc0\xa2""foo"));
    ASSERT_FALSE(utf8::is_valid("\xc1\xa2""foo"));
    ASSERT_FALSE(utf8::is_valid("\xf5\xa2\xa2\xa2""bar"));

    ASSERT_FALSE(utf8::is_valid("\xc0\xa2""foo", &reason));
    ASSERT_EQ(1, reason.position);
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid("\xc1\xa2""foo", &reason));
    ASSERT_EQ(1, reason.position);
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid("\xf5\xa2\xa2\xa2""bar", &reason));
    ASSERT_EQ(3, reason.position);
    ASSERT_STREQ("Non-Unicode character encoded (beyond U+10FFFF)", reason.explanation);

    // continuation byte with no leading byte
    ASSERT_FALSE(utf8::is_valid("\xbf"));

    ASSERT_FALSE(utf8::is_valid("\xbf", &reason));
    ASSERT_EQ(0, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);

    // two byte character with two continuation bytes
    ASSERT_FALSE(utf8::is_valid("\xc2\xa2\xbf"));

    ASSERT_FALSE(utf8::is_valid("\xc2\xa2\xbf", &reason));
    ASSERT_EQ(2, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);

    // two byte character with no continuation bytes
    ASSERT_FALSE(utf8::is_valid("\xc2"));

    ASSERT_FALSE(utf8::is_valid("\xc2", &reason));
    ASSERT_EQ(1, reason.position);
    ASSERT_STREQ("Expected continuation byte, saw end of string", reason.explanation);

    // three byte leader, then two byte surrogate
    ASSERT_FALSE(utf8::is_valid("\xe0\xc2\xa2"));

    ASSERT_FALSE(utf8::is_valid("\xe0\xc2\xa2", &reason));
    ASSERT_EQ(1, reason.position);
    ASSERT_STREQ("Expected continuation byte, saw something else", reason.explanation);
}

TEST(UTF8ValidationTest, NullBytes) {
    utf8::reason_t reason;

    ASSERT_TRUE(utf8::is_valid("foo\x00\xff")); // C string :/
    // these two are a correct string, with a null byte, and then an
    // invalid character; we're verifying that parsing proceeds past
    // the NULL.
    ASSERT_FALSE(utf8::is_valid(std::string("foo\x00\xff", 5)));
    ASSERT_FALSE(utf8::is_valid(datum_string_t(std::string("foo\x00\xff", 5))));

    ASSERT_FALSE(utf8::is_valid(std::string("foo\x00\xff", 5), &reason));
    ASSERT_EQ(4, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid(datum_string_t(std::string("foo\x00\xff", 5)), &reason));
    ASSERT_EQ(4, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);
}

TEST(UTF8ValidationTest, IllegalCharacters) {
    utf8::reason_t reason;

    // ASCII $ encoded as two characters
    ASSERT_FALSE(utf8::is_valid("foo\xc0\xa4"));
    // U+00A2 cent sign encoded as three characters
    ASSERT_FALSE(utf8::is_valid("foo\xe0\x82\xa2"));
    // U+20AC cent sign encoded as four characters
    ASSERT_FALSE(utf8::is_valid("foo\xf0\x82\x82\xac"));
    // what would be U+2134AC if that existed
    ASSERT_FALSE(utf8::is_valid("foo\xf8\x88\x93\x92\xac"));
    // NULL encoded as two characters ("modified UTF-8")
    ASSERT_FALSE(utf8::is_valid("foo\xc0\x80"));

    ASSERT_FALSE(utf8::is_valid("foo\xc0\xa4", &reason));
    ASSERT_EQ(4, reason.position);
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid("foo\xe0\x82\xa2", &reason));
    ASSERT_EQ(5, reason.position);
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid("foo\xf0\x82\x82\xac", &reason));
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
    ASSERT_EQ(6, reason.position);
    ASSERT_FALSE(utf8::is_valid("foo\xf8\x88\x93\x92\xac", &reason));
    ASSERT_EQ(3, reason.position);
    ASSERT_STREQ("Invalid initial byte seen", reason.explanation);
    ASSERT_FALSE(utf8::is_valid("foo\xc0\x80", &reason));
    ASSERT_EQ(4, reason.position);
    ASSERT_STREQ("Overlong encoding seen", reason.explanation);
}

TEST(UTF8IterationTest, SimpleString) {
    std::string demo = "this is a demonstration string";

    utf8::string_iterator_t it(demo);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U't', *it);
    it++;
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'h', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'e', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'o', *it);
    std::advance(it, 10);
    ASSERT_TRUE(it.is_done());
}

TEST(UTF8IterationTest, SimpleCString) {
    const char *str = "this is a demonstration string";
    utf8::array_iterator_t it(str, str + strlen(str));
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U't', *it);
    it++;
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'h', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'e', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'o', *it);
    std::advance(it, 10);
    ASSERT_TRUE(it.is_done());
}

TEST(UTF8IterationTest, SimpleStringRange) {
    std::string demo = "this is a demonstration string";

    utf8::string_iterator_t it(demo.begin(), demo.end());
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U't', *it);
    it++;
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'h', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'e', *it);
    std::advance(it, 10);
    ASSERT_FALSE(it.is_done());
    ASSERT_EQ(U'o', *it);
    std::advance(it, 10);
    ASSERT_TRUE(it.is_done());
}

TEST(UTF8IterationTest, EmptyString) {
    {
        utf8::string_iterator_t it;
        ASSERT_TRUE(it.is_done());
    }

    {
        const char *c = "";
        utf8::array_iterator_t it(c, c);
        ASSERT_TRUE(it.is_done());
    }

    {
        std::string s = "";
        utf8::string_iterator_t it(s.begin(), s.end());
        ASSERT_TRUE(it.is_done());
    }
}

// if we can handle this, we can probably handle anything
TEST(UTF8IterationTest, Zalgo) {
    const char zalgo[] = "H\xcd\x95"
        "a\xcc\x95\xcd\x8d\xcc\x99\xcd\x8d\xcc\xab\xcd\x87\xcc\xa5\xcc\xa3"
        "v\xcc\xb4"
        "e\xcd\x98\xcc\x96\xcc\xb1\xcd\x96"
        " \xcd\xa1\xcc\xac"
        "s\xcd\x8e\xcc\xa5\xcc\xba\xcd\x88\xcc\xab"
        "o\xcc\xa3\xcc\xb3\xcc\xae\xcd\x85\xcc\xa9"
        "m\xcd\xa2\xcd\x94\xcc\x9e\xcc\x99\xcd\x99\xcc\x9c"
        "e"
        " \xcc\xa5"
        "Z\xcc\xb6"
        "a\xcc\xab\xcc\xa9\xcd\x8e\xcc\xb2\xcc\xac\xcc\xba"
        "l\xcc\x98\xcd\x87\xcd\x94"
        "g\xcc\xb6\xcc\x9e\xcd\x99\xcc\xbc"
        "o"
        ".\xcc\x9b\xcc\xab\xcc\xa9";
    const char32_t zalgo_codepoints[] = U"\u0048\u0355\u0061\u0315\u034d\u0319\u034d"
        "\u032b\u0347\u0325\u0323\u0076\u0334\u0065\u0358\u0316\u0331\u0356\u0020\u0361"
        "\u032c\u0073\u034e\u0325\u033a\u0348\u032b\u006f\u0323\u0333\u032e\u0345\u0329"
        "\u006d\u0362\u0354\u031e\u0319\u0359\u031c\u0065\u0020\u0325\u005a\u0336\u0061"
        "\u032b\u0329\u034e\u0332\u032c\u033a\u006c\u0318\u0347\u0354\u0067\u0336\u031e"
        "\u0359\u033c\u006f\u002e\u031b\u032b\u0329";
    utf8::array_iterator_t it(zalgo, zalgo + strlen(zalgo));
    const char32_t *current = zalgo_codepoints;
    size_t seen = 0;
    while (!it.is_done()) {
        char32_t parsed = *it;
        ASSERT_EQ(*current, parsed);
        ++it; ++current; ++seen;
    }
    ASSERT_EQ(66, seen);
}

};
