#pragma once

#include <map>
#include <string>

// Simple JSON parser for i18n string tables
// No external dependencies

class JsonLoader {
public:
  using StringMap = std::map<std::string, std::string>;

  // Parse JSON file, returns true on success
  static bool parse_file(const char* filepath, StringMap& out_strings);

  // Parse JSON from memory
  static bool parse_memory(const char* json_content, size_t length, StringMap& out_strings);

private:
  // Simple tokenizer states
  enum class TokenType {
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Colon,
    Comma,
    String,
    Number,
    True,
    False,
    Null,
    End,
    Error
  };

  struct Token {
    TokenType type;
    std::string value;
    int line;
  };

  class Tokenizer {
  public:
    Tokenizer(const char* json, size_t len);

    Token next_token();

    const char* get_error() const { return error_; }
    int get_line() const { return line_; }

  private:
    char peek() const;
    char advance();
    void skip_whitespace();
    Token read_string();
    Token read_number();
    Token read_identifier();

    const char* json_;
    size_t length_;
    size_t pos_;
    int line_;
    const char* error_;
  };

  static bool parse_object(Tokenizer& tokenizer, StringMap& out);
  static bool expect_token(Tokenizer& tokenizer, TokenType expected, Token& out);
};
