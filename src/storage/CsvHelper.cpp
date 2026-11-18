#include "fleetflow/storage/CsvHelper.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <string>
#include <vector>

namespace fleetflow::storage {

std::string CsvHelper::escapeField(const std::string& field) {
    // Reject embedded newlines — single-line mode only
    for (char c : field) {
        if (c == '\n' || c == '\r') {
            throw core::StorageException(
                "Field contains embedded newline character, which is not supported "
                "in single-line CSV mode.");
        }
    }

    bool needsQuoting = false;
    for (char c : field) {
        if (c == ',' || c == '"') {
            needsQuoting = true;
            break;
        }
    }

    // Also quote if field has leading or trailing whitespace to preserve it
    if (!field.empty() && (field.front() == ' ' || field.front() == '\t' ||
                           field.back() == ' ' || field.back() == '\t')) {
        needsQuoting = true;
    }

    if (!needsQuoting) {
        return field;
    }

    std::string escaped;
    escaped.reserve(field.size() + 4);
    escaped.push_back('"');
    for (char c : field) {
        if (c == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(c);
    }
    escaped.push_back('"');
    return escaped;
}

std::vector<std::string> CsvHelper::parseLine(const std::string& line) {
    // Strip trailing line endings (CRLF or LF)
    std::string input = line;
    if (!input.empty() && input.back() == '\n') {
        input.pop_back();
        if (!input.empty() && input.back() == '\r') {
            input.pop_back();
        }
    }

    // Empty input returns empty vector (no fields)
    if (input.empty()) {
        return {};
    }

    // ---- State Machine ----
    enum class State { FieldStart, Unquoted, Quoted, AfterQuote };

    std::vector<std::string> fields;
    std::string current;
    State state = State::FieldStart;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        switch (state) {
            case State::FieldStart:
                if (c == '"') {
                    state = State::Quoted;
                } else if (c == ',') {
                    fields.push_back(std::move(current));
                    current.clear();
                    // Remain in FieldStart for next field
                } else if (c == '\r' || c == '\n') {
                    throw core::StorageException(
                        "Embedded newline character at position " +
                        std::to_string(i) + ".");
                } else {
                    current.push_back(c);
                    state = State::Unquoted;
                }
                break;

            case State::Unquoted:
                if (c == ',') {
                    fields.push_back(std::move(current));
                    current.clear();
                    state = State::FieldStart;
                } else if (c == '"') {
                    throw core::StorageException(
                        "Invalid quote at position " + std::to_string(i) +
                        " in unquoted field.");
                } else if (c == '\r' || c == '\n') {
                    throw core::StorageException(
                        "Embedded newline character at position " +
                        std::to_string(i) + ".");
                } else {
                    current.push_back(c);
                }
                break;

            case State::Quoted:
                if (c == '"') {
                    state = State::AfterQuote;
                } else if (c == '\r' || c == '\n') {
                    throw core::StorageException(
                        "Embedded newline character at position " +
                        std::to_string(i) +
                        " inside quoted field. Multi-line fields are not supported.");
                } else {
                    current.push_back(c);
                }
                break;

            case State::AfterQuote:
                if (c == '"') {
                    // Escaped double-quote → append literal '"'
                    current.push_back('"');
                    state = State::Quoted;
                } else if (c == ',') {
                    fields.push_back(std::move(current));
                    current.clear();
                    state = State::FieldStart;
                } else if (c == '\r' || c == '\n') {
                    throw core::StorageException(
                        "Embedded newline character at position " +
                        std::to_string(i) + ".");
                } else {
                    throw core::StorageException(
                        "Unexpected character '" + std::string(1, c) +
                        "' at position " + std::to_string(i) +
                        " after closing quote.");
                }
                break;
        }
    }

    // ---- End-of-Input Handling ----
    switch (state) {
        case State::FieldStart:
            // Reached here after a trailing comma → emit trailing empty field
            fields.push_back(std::move(current));
            break;
        case State::Unquoted:
            fields.push_back(std::move(current));
            break;
        case State::Quoted:
            throw core::StorageException("Unmatched opening quote in CSV line.");
        case State::AfterQuote:
            fields.push_back(std::move(current));
            break;
    }

    return fields;
}

std::string CsvHelper::escapeRow(const std::vector<std::string>& fields) {
    if (fields.empty()) {
        return "";
    }
    // A single empty field is represented as "" to distinguish it from an empty record (0 fields)
    if (fields.size() == 1 && fields[0].empty()) {
        return "\"\"";
    }
    std::string row;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            row.push_back(',');
        }
        row += escapeField(fields[i]);
    }
    return row;
}

std::vector<std::string> CsvHelper::parseRow(const std::string& line) {
    return parseLine(line);
}

} // namespace fleetflow::storage

