#pragma once

#include <string>
#include <vector>

namespace fleetflow::storage {

/**
 * @brief Single-line RFC-4180-style CSV parsing and writing utilities.
 *
 * ## Parsing Algorithm (State Machine)
 *
 * The parser processes input one character at a time using four states:
 *
 * - **FieldStart**: Beginning of a new field. A double-quote transitions to Quoted.
 *   A comma emits an empty field and remains in FieldStart. Any other printable
 *   character transitions to Unquoted and is appended.
 *
 * - **Unquoted**: Reading an unquoted field. A comma emits the accumulated field and
 *   transitions to FieldStart. A double-quote is rejected (invalid quote placement).
 *   CR/LF characters are rejected. All other characters are appended.
 *
 * - **Quoted**: Reading inside a quoted field. A double-quote transitions to AfterQuote.
 *   CR/LF characters are rejected (embedded newlines not supported in single-line mode).
 *   All other characters are appended verbatim.
 *
 * - **AfterQuote**: Just encountered a double-quote inside a quoted field.
 *   Another double-quote appends a literal '"' and returns to Quoted (escaped quote).
 *   A comma emits the field and transitions to FieldStart.
 *   CR/LF or end-of-input emits the field.
 *   Any other character is rejected (unexpected character after closing quote).
 *
 * ## End-of-Input Handling
 * - FieldStart (after a comma): emits a trailing empty field.
 * - Unquoted: emits the accumulated field.
 * - Quoted: error — unmatched opening quote.
 * - AfterQuote: emits the accumulated field.
 * - Empty input (zero length after line-ending stripping): returns an empty vector.
 *
 * ## Line Ending Handling
 * - Trailing LF (\\n) or CRLF (\\r\\n) is stripped before parsing.
 * - Embedded CR/LF characters within the parsed content are rejected.
 *
 * @note This is a **single-line** parser. It does NOT support multi-line quoted fields.
 */
class CsvHelper {
public:
    /**
     * @brief Escapes a single field for CSV output.
     *
     * If the field contains commas, double-quotes, or leading/trailing whitespace,
     * it is wrapped in double-quotes. Internal double-quotes are escaped by doubling.
     *
     * @param field The raw field value.
     * @return Escaped field string suitable for CSV output.
     * @throws core::StorageException if the field contains embedded newline characters.
     */
    static std::string escapeField(const std::string& field);

    /**
     * @brief Parses a single CSV line into a vector of field strings.
     *
     * Strips trailing LF or CRLF line endings before parsing.
     * Returns an empty vector for empty input (after stripping).
     *
     * @param line A single line of CSV text (may include trailing line ending).
     * @return Vector of parsed field strings.
     * @throws core::StorageException on malformed CSV (unmatched quotes,
     *         invalid quote placement, unexpected characters after closing quote,
     *         or embedded newline characters).
     */
    static std::vector<std::string> parseLine(const std::string& line);

    /**
     * @brief Escapes and joins multiple fields into a single CSV line.
     * @param fields Vector of raw field values.
     * @return A single CSV line string (no trailing newline).
     * @throws core::StorageException if any field contains embedded newline characters.
     */
    static std::string escapeRow(const std::vector<std::string>& fields);

    /**
     * @brief Alias for parseLine(). Provided for naming symmetry with escapeRow().
     */
    static std::vector<std::string> parseRow(const std::string& line);
};

} // namespace fleetflow::storage

