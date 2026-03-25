#include "ui/input_line.hpp"

#include <catch2/catch_test_macros.hpp>

using grotto::ui::InputLine;

TEST_CASE("InputLine inserts pasted ASCII text", "[input_line]") {
    InputLine line;

    line.insert_text("hello world");

    REQUIRE(line.text() == "hello world");
    REQUIRE(line.cursor_col() == 11);
}

TEST_CASE("InputLine preserves UTF-8 code points", "[input_line]") {
    InputLine line;
    const std::string utf8 = "\xC3\xA4\xC3\xB6\xF0\x9F\x99\x82";

    line.insert_text(utf8);

    REQUIRE(line.text() == utf8);
    REQUIRE(line.cursor_col() == 3);
}

TEST_CASE("InputLine strips line breaks and normalizes tabs", "[input_line]") {
    InputLine line;

    line.insert_text("hello\nworld\t!");

    REQUIRE(line.text() == "helloworld !");
}

TEST_CASE("InputLine kill_to_end deletes from cursor to end of buffer", "[input_line]") {
    InputLine line;
    line.insert_text("hello world");
    // Move cursor to middle (after "hello ")
    for (int i = 0; i < 5; ++i) line.move_left();  // cursor at position 6

    line.kill_to_end();

    REQUIRE(line.text() == "hello ");
    REQUIRE(line.cursor_col() == 6);
}

TEST_CASE("InputLine kill_to_end is no-op at end of buffer", "[input_line]") {
    InputLine line;
    line.insert_text("hello");

    line.kill_to_end();  // cursor is at end

    REQUIRE(line.text() == "hello");
}

TEST_CASE("InputLine kill_to_end stops at newline (inserted via insert())", "[input_line]") {
    InputLine line;
    // Build "first\nsecond" using insert() so newline is in buf_
    line.insert_text("first");
    line.insert(U'\n');
    line.insert_text("second");
    // Move cursor to start of "second" (position 6, after the \n)
    for (int i = 0; i < 6; ++i) line.move_left();  // cursor at 6

    line.kill_to_end();

    // "second" should be gone, "\n" should remain
    REQUIRE(line.text() == "first\n");
}

TEST_CASE("InputLine delete_word_backward removes preceding word", "[input_line]") {
    InputLine line;
    line.insert_text("hello world");  // cursor at end

    line.delete_word_backward();

    REQUIRE(line.text() == "hello ");
    REQUIRE(line.cursor_col() == 6);
}

TEST_CASE("InputLine delete_word_backward skips trailing spaces first", "[input_line]") {
    InputLine line;
    line.insert_text("hello   ");  // cursor after trailing spaces

    line.delete_word_backward();

    REQUIRE(line.text() == "");
}

TEST_CASE("InputLine delete_word_backward is no-op at beginning", "[input_line]") {
    InputLine line;
    line.insert_text("hello");
    line.move_home();

    line.delete_word_backward();

    REQUIRE(line.text() == "hello");
    REQUIRE(line.cursor_col() == 0);
}

TEST_CASE("InputLine delete_word_backward does not cross newline boundary", "[input_line]") {
    InputLine line;
    line.insert_text("first");
    line.insert(U'\n');
    line.insert_text("second");  // cursor at end of "second"

    line.delete_word_backward();

    // "second" deleted but "\n" should still be there
    REQUIRE(line.text() == "first\n");
}
