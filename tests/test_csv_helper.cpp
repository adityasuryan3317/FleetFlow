/**
 * @file test_csv_helper.cpp
 * @brief Comprehensive tests for CsvHelper single-line CSV parsing and writing.
 *
 * Covers:
 * 1. EMPTY FIELD ROUND-TRIPPING:
 *    - escapeRow({}) -> "" -> parseLine() -> vector<string>{} (0 fields)
 *    - escapeRow({""}) -> "\"\"" -> parseLine() -> vector<string>{""} (1 empty field)
 *    - escapeRow({"", ""}) -> "," -> parseLine() -> vector<string>{"", ""} (2 empty fields)
 *    - escapeRow({"Honda", ""}) -> "Honda," -> parseLine() -> vector<string>{"Honda", ""}
 *    - escapeRow({"", "Honda"}) -> ",Honda" -> parseLine() -> vector<string>{"", "Honda"}
 *    - Distinction between empty record (0 fields) and record with 1 empty field
 * 2. LINE ENDINGS:
 *    - One trailing LF is accepted
 *    - One trailing CRLF is accepted
 *    - Embedded CR/LF is rejected (in unquoted and quoted fields)
 *    - Standalone CR is not silently discarded (trailing, middle, standalone)
 *    - Multiple trailing line endings rejected
 * 3. BASIC CSV PARSING:
 *    - Simple fields, empty fields, commas in quotes, embedded double quotes,
 *      leading/trailing whitespace, trailing empty columns
 * 4. ERROR CASES:
 *    - Unmatched quotes, invalid quote placement, unexpected characters after quote
 * 5. ESCAPING & ALIASES:
 *    - escapeField, escapeRow, parseRow alias
 * 6. ROUND-TRIP FIDELITY:
 *    - Simple, special chars, multi-empty, quotes, whitespace, domain model CSV rows
 */

#include "fleetflow/storage/CsvHelper.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

using namespace fleetflow;
using CsvHelper = storage::CsvHelper;

// ============================================================================
// Test Framework Macros
// ============================================================================

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    do { \
        std::cout << "  TEST: " << name << " ... "; \
        try {

#define PASS \
            std::cout << "PASSED\n"; \
            ++passed; \
        } catch (const std::exception& ex) { \
            std::cout << "FAILED (unexpected exception: " << ex.what() << ")\n"; \
            ++failed; \
        } catch (...) { \
            std::cout << "FAILED (unknown exception)\n"; \
            ++failed; \
        } \
    } while (0)

#define VERIFY(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error("Verification failed: " #cond); \
        } \
    } while (0)

#define EXPECT_THROW(expr, ExType) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const ExType&) { caught = true; } \
        catch (...) {} \
        if (!caught) { \
            throw std::runtime_error("Expected " #ExType " was not thrown"); \
        } \
    } while (0)

static void assertFields(const std::vector<std::string>& actual,
                         const std::vector<std::string>& expected,
                         const std::string& context = "") {
    if (actual.size() != expected.size()) {
        std::cerr << "  Field count mismatch" << (context.empty() ? "" : " (" + context + ")")
                  << ": expected " << expected.size() << ", got " << actual.size() << "\n";
        for (std::size_t i = 0; i < actual.size(); ++i) {
            std::cerr << "    [" << i << "]: '" << actual[i] << "'\n";
        }
        throw std::runtime_error("Field count mismatch: " + context);
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::cerr << "  Field [" << i << "] mismatch"
                      << (context.empty() ? "" : " (" + context + ")")
                      << ": expected '" << expected[i] << "', got '" << actual[i] << "'\n";
            throw std::runtime_error("Field value mismatch at index " + std::to_string(i));
        }
    }
}

// ============================================================================
// 1. EMPTY FIELD ROUND-TRIPPING & DISTINCTION TESTS
// ============================================================================

void testEmptyFieldRoundTripping() {
    TEST("escapeRow({}) produces empty string and parses to 0 fields") {
        std::vector<std::string> input = {};
        std::string csv = CsvHelper::escapeRow(input);
        VERIFY(csv == "");
        auto parsed = CsvHelper::parseLine(csv);
        VERIFY(parsed.empty());
    } PASS;

    TEST("escapeRow({\"\"}) produces \"\" and parses to exactly 1 empty field") {
        std::vector<std::string> input = {""};
        std::string csv = CsvHelper::escapeRow(input);
        VERIFY(csv == "\"\"");
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, {""}, "single empty field");
        VERIFY(parsed.size() == 1);
        VERIFY(parsed[0].empty());
    } PASS;

    TEST("Distinction between 0 fields and 1 empty field is preserved") {
        auto zeroFields = CsvHelper::parseLine(CsvHelper::escapeRow({}));
        auto oneEmptyField = CsvHelper::parseLine(CsvHelper::escapeRow({""}));
        VERIFY(zeroFields.size() == 0);
        VERIFY(oneEmptyField.size() == 1);
        VERIFY(zeroFields != oneEmptyField);
    } PASS;

    TEST("escapeRow({\"\", \"\"}) produces \",\" and parses to 2 empty fields") {
        std::vector<std::string> input = {"", ""};
        std::string csv = CsvHelper::escapeRow(input);
        VERIFY(csv == ",");
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, {"", ""}, "two empty fields");
        VERIFY(parsed.size() == 2);
    } PASS;

    TEST("escapeRow({\"Honda\", \"\"}) produces \"Honda,\" and parses to {\"Honda\", \"\"}") {
        std::vector<std::string> input = {"Honda", ""};
        std::string csv = CsvHelper::escapeRow(input);
        VERIFY(csv == "Honda,");
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, {"Honda", ""}, "trailing empty column");
        VERIFY(parsed.size() == 2);
    } PASS;

    TEST("escapeRow({\"\", \"Honda\"}) produces \",Honda\" and parses to {\"\", \"Honda\"}") {
        std::vector<std::string> input = {"", "Honda"};
        std::string csv = CsvHelper::escapeRow(input);
        VERIFY(csv == ",Honda");
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, {"", "Honda"}, "leading empty column");
        VERIFY(parsed.size() == 2);
    } PASS;
}

// ============================================================================
// 2. LINE ENDINGS TESTS
// ============================================================================

void testLineEndings() {
    TEST("One trailing LF is accepted") {
        auto fields = CsvHelper::parseLine("alpha,beta,gamma\n");
        assertFields(fields, {"alpha", "beta", "gamma"});
    } PASS;

    TEST("One trailing CRLF is accepted") {
        auto fields = CsvHelper::parseLine("alpha,beta,gamma\r\n");
        assertFields(fields, {"alpha", "beta", "gamma"});
    } PASS;

    TEST("Standalone trailing CR is NOT silently discarded (throws StorageException)") {
        // "a,b,c\r" has a trailing CR without LF
        EXPECT_THROW(CsvHelper::parseLine("alpha,beta,gamma\r"), core::StorageException);
    } PASS;

    TEST("Input consisting only of standalone CR throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\r"), core::StorageException);
    } PASS;

    TEST("Standalone CR in middle of field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("alpha\rbeta,gamma"), core::StorageException);
    } PASS;

    TEST("Embedded LF inside unquoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("alpha\nbeta,gamma"), core::StorageException);
    } PASS;

    TEST("Embedded CRLF inside unquoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("alpha\r\nbeta,gamma"), core::StorageException);
    } PASS;

    TEST("Embedded LF inside quoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\"alpha\nbeta\",gamma"), core::StorageException);
    } PASS;

    TEST("Embedded CR inside quoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\"alpha\rbeta\",gamma"), core::StorageException);
    } PASS;

    TEST("Embedded CRLF inside quoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\"alpha\r\nbeta\",gamma"), core::StorageException);
    } PASS;

    TEST("Multiple trailing LFs throw StorageException (not stripped)") {
        // First LF stripped, second LF remains as embedded newline
        EXPECT_THROW(CsvHelper::parseLine("alpha,beta\n\n"), core::StorageException);
    } PASS;

    TEST("Multiple trailing CRLFs throw StorageException (not stripped)") {
        EXPECT_THROW(CsvHelper::parseLine("alpha,beta\r\n\r\n"), core::StorageException);
    } PASS;
}

// ============================================================================
// 3. BASIC CSV PARSING TESTS
// ============================================================================

void testBasicParsing() {
    TEST("Simple comma-separated fields") {
        auto fields = CsvHelper::parseLine("alpha,beta,gamma");
        assertFields(fields, {"alpha", "beta", "gamma"});
    } PASS;

    TEST("Single field (no commas)") {
        auto fields = CsvHelper::parseLine("hello");
        assertFields(fields, {"hello"});
    } PASS;

    TEST("Empty fields between commas") {
        auto fields = CsvHelper::parseLine(",,,");
        assertFields(fields, {"", "", "", ""});
    } PASS;

    TEST("Single comma = two empty fields") {
        auto fields = CsvHelper::parseLine(",");
        assertFields(fields, {"", ""});
    } PASS;

    TEST("Commas inside quoted fields") {
        auto fields = CsvHelper::parseLine("\"a,b\",c");
        assertFields(fields, {"a,b", "c"});
    } PASS;

    TEST("Embedded double quotes escaped by doubling") {
        auto fields = CsvHelper::parseLine("\"She said \"\"hello\"\".\",done");
        assertFields(fields, {"She said \"hello\".", "done"});
    } PASS;

    TEST("Leading and trailing whitespace preserved") {
        auto fields = CsvHelper::parseLine(" alpha , beta ");
        assertFields(fields, {" alpha ", " beta "});
    } PASS;

    TEST("Quoted fields preserve whitespace") {
        auto fields = CsvHelper::parseLine("\" spaces \",normal");
        assertFields(fields, {" spaces ", "normal"});
    } PASS;

    TEST("Trailing empty columns preserved") {
        auto fields = CsvHelper::parseLine("a,b,");
        assertFields(fields, {"a", "b", ""});
    } PASS;

    TEST("Multiple trailing empty columns") {
        auto fields = CsvHelper::parseLine("a,,");
        assertFields(fields, {"a", "", ""});
    } PASS;

    TEST("Empty input returns empty vector") {
        auto fields = CsvHelper::parseLine("");
        VERIFY(fields.empty());
    } PASS;

    TEST("Only LF returns empty vector") {
        auto fields = CsvHelper::parseLine("\n");
        VERIFY(fields.empty());
    } PASS;

    TEST("Only CRLF returns empty vector") {
        auto fields = CsvHelper::parseLine("\r\n");
        VERIFY(fields.empty());
    } PASS;

    TEST("Quoted empty field parsed correctly") {
        auto fields = CsvHelper::parseLine("\"\",b,\"\"");
        assertFields(fields, {"", "b", ""});
    } PASS;

    TEST("Mixed quoted and unquoted fields") {
        auto fields = CsvHelper::parseLine("plain,\"quoted,comma\",\"has \"\"quotes\"\"\",end");
        assertFields(fields, {"plain", "quoted,comma", "has \"quotes\"", "end"});
    } PASS;
}

// ============================================================================
// 4. MALFORMED CSV ERROR TESTS
// ============================================================================

void testMalformedCsvErrors() {
    TEST("Unmatched opening quote throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\"hello"), core::StorageException);
    } PASS;

    TEST("Unmatched quote in middle field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("a,\"b,c"), core::StorageException);
    } PASS;

    TEST("Quote in middle of unquoted field throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("hel\"lo,world"), core::StorageException);
    } PASS;

    TEST("Unexpected character after closing quote throws StorageException") {
        EXPECT_THROW(CsvHelper::parseLine("\"hello\"world,next"), core::StorageException);
    } PASS;
}

// ============================================================================
// 5. ESCAPING & ALIAS TESTS
// ============================================================================

void testEscapingAndAliases() {
    TEST("escapeField: simple field unchanged") {
        VERIFY(CsvHelper::escapeField("hello") == "hello");
    } PASS;

    TEST("escapeField: empty field unchanged") {
        VERIFY(CsvHelper::escapeField("") == "");
    } PASS;

    TEST("escapeField: field with comma is quoted") {
        VERIFY(CsvHelper::escapeField("a,b") == "\"a,b\"");
    } PASS;

    TEST("escapeField: field with double-quote is quoted and doubled") {
        VERIFY(CsvHelper::escapeField("say \"hi\"") == "\"say \"\"hi\"\"\"");
    } PASS;

    TEST("escapeField: leading whitespace triggers quoting") {
        VERIFY(CsvHelper::escapeField(" abc") == "\" abc\"");
    } PASS;

    TEST("escapeField: trailing whitespace triggers quoting") {
        VERIFY(CsvHelper::escapeField("abc ") == "\"abc \"");
    } PASS;

    TEST("escapeField: embedded newline throws StorageException") {
        EXPECT_THROW(CsvHelper::escapeField("line1\nline2"), core::StorageException);
        EXPECT_THROW(CsvHelper::escapeField("line1\rline2"), core::StorageException);
    } PASS;

    TEST("escapeRow: simple fields joined with commas") {
        auto row = CsvHelper::escapeRow({"a", "b", "c"});
        VERIFY(row == "a,b,c");
    } PASS;

    TEST("escapeRow: fields with special characters escaped") {
        auto row = CsvHelper::escapeRow({"normal", "has,comma", "has \"quote\""});
        VERIFY(row == "normal,\"has,comma\",\"has \"\"quote\"\"\"");
    } PASS;

    TEST("parseRow is alias for parseLine") {
        auto f1 = CsvHelper::parseLine("a,b,c");
        auto f2 = CsvHelper::parseRow("a,b,c");
        VERIFY(f1 == f2);
    } PASS;
}

// ============================================================================
// 6. COMPREHENSIVE ROUND-TRIP FIDELITY TESTS
// ============================================================================

void testRoundTripFidelity() {
    TEST("Round-trip: empty vector") {
        std::vector<std::string> original = {};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "empty vector round-trip");
    } PASS;

    TEST("Round-trip: single empty field") {
        std::vector<std::string> original = {""};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "single empty field round-trip");
    } PASS;

    TEST("Round-trip: two empty fields") {
        std::vector<std::string> original = {"", ""};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "two empty fields round-trip");
    } PASS;

    TEST("Round-trip: trailing empty field (\"Honda\", \"\")") {
        std::vector<std::string> original = {"Honda", ""};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "trailing empty round-trip");
    } PASS;

    TEST("Round-trip: leading empty field (\"\", \"Honda\")") {
        std::vector<std::string> original = {"", "Honda"};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "leading empty round-trip");
    } PASS;

    TEST("Round-trip: both-ends empty (\"\", \"Honda\", \"\")") {
        std::vector<std::string> original = {"", "Honda", ""};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "both ends empty round-trip");
    } PASS;

    TEST("Round-trip: simple alphanumeric tokens") {
        std::vector<std::string> original = {"alpha", "beta", "gamma", "123", "45.67"};
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "simple alphanumeric round-trip");
    } PASS;

    TEST("Round-trip: commas, quotes, and whitespace tokens") {
        std::vector<std::string> original = {
            "normal",
            "has,comma",
            "has \"quotes\"",
            " leading",
            "trailing ",
            "  both  ",
            "\""
        };
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "special chars round-trip");
    } PASS;

    TEST("Round-trip: realistic Car CSV row") {
        std::vector<std::string> original = {
            "CAR", "1", "Toyota", "Camry", "ABC-1234", "50.0", "Available", "5", "Petrol", "true"
        };
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "Car record round-trip");
    } PASS;

    TEST("Round-trip: realistic User CSV row with commas in values") {
        std::vector<std::string> original = {
            "CUSTOMER", "101", "john_doe", "Doe, John", "john@example.com", "DL-1234,Sec A", "+1 234 567"
        };
        auto csv = CsvHelper::escapeRow(original);
        auto parsed = CsvHelper::parseLine(csv);
        assertFields(parsed, original, "Customer with commas record round-trip");
    } PASS;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n===== 1. EMPTY FIELD ROUND-TRIP & DISTINCTION TESTS =====\n";
    testEmptyFieldRoundTripping();

    std::cout << "\n===== 2. LINE ENDINGS & STANDALONE CR TESTS =====\n";
    testLineEndings();

    std::cout << "\n===== 3. BASIC CSV PARSING TESTS =====\n";
    testBasicParsing();

    std::cout << "\n===== 4. MALFORMED CSV ERROR TESTS =====\n";
    testMalformedCsvErrors();

    std::cout << "\n===== 5. ESCAPING & ALIAS TESTS =====\n";
    testEscapingAndAliases();

    std::cout << "\n===== 6. COMPREHENSIVE ROUND-TRIP FIDELITY TESTS =====\n";
    testRoundTripFidelity();

    std::cout << "\n===== RESULTS =====\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";

    if (failed > 0) {
        std::cerr << "*** SOME CSV HELPER TESTS FAILED ***\n";
        return 1;
    }
    std::cout << "All CSV helper tests passed.\n\n";
    return 0;
}
