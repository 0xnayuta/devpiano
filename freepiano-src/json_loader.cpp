#include "pch.h"
#include "json_loader.h"
#include "fp_log.h"

JsonLoader::Tokenizer::Tokenizer(const char* json, size_t len)
  : json_(json), length_(len), pos_(0), line_(1), error_(nullptr) {
}

char JsonLoader::Tokenizer::peek() const {
  if (pos_ >= length_) return '\0';
  return json_[pos_];
}

char JsonLoader::Tokenizer::advance() {
  if (pos_ >= length_) return '\0';
  char c = json_[pos_++];
  if (c == '\n') line_++;
  return c;
}

void JsonLoader::Tokenizer::skip_whitespace() {
  while (pos_ < length_) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      advance();
    }
    else if (c == '/') {
      // Skip comments (non-standard but useful)
      advance();
      if (peek() == '/') {
        while (pos_ < length_ && peek() != '\n') advance();
      }
      else if (peek() == '*') {
        advance();
        while (pos_ < length_) {
          if (peek() == '*') {
            advance();
            if (peek() == '/') {
              advance();
              break;
            }
          }
          else {
            advance();
          }
        }
      }
      else {
        error_ = "Invalid comment";
        return;
      }
    }
    else {
      break;
    }
  }
}

JsonLoader::Token JsonLoader::Tokenizer::read_string() {
  Token token;
  token.type = TokenType::String;
  token.line = line_;

  advance();  // Skip opening quote

  while (pos_ < length_) {
    char c = peek();

    if (c == '"') {
      advance();  // Skip closing quote
      return token;
    }
    else if (c == '\\') {
      advance();
      c = peek();
      switch (c) {
        case '"': token.value += '"'; advance(); break;
        case '\\': token.value += '\\'; advance(); break;
        case '/': token.value += '/'; advance(); break;
        case 'b': token.value += '\b'; advance(); break;
        case 'f': token.value += '\f'; advance(); break;
        case 'n': token.value += '\n'; advance(); break;
        case 'r': token.value += '\r'; advance(); break;
        case 't': token.value += '\t'; advance(); break;
        case 'u': {
          advance();  // Skip 'u'
          char hex[5] = {0};
          for (int i = 0; i < 4 && pos_ < length_; i++) {
            hex[i] = advance();
          }
          unsigned int codepoint = 0;
          sscanf(hex, "%x", &codepoint);

          // Convert to UTF-8 (including null character for codepoint 0)
          if (codepoint < 0x80) {
            token.value += static_cast<char>(codepoint);
          }
          else if (codepoint < 0x800) {
            token.value += static_cast<char>(0xC0 | (codepoint >> 6));
            token.value += static_cast<char>(0x80 | (codepoint & 0x3F));
          }
          else {
            token.value += static_cast<char>(0xE0 | (codepoint >> 12));
            token.value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            token.value += static_cast<char>(0x80 | (codepoint & 0x3F));
          }
          break;
        }
        default:
          // For unrecognized escapes, just add the character
          token.value += c;
          advance();
          break;
      }
    }
    else {
      token.value += advance();
    }
  }

  error_ = "Unterminated string";
  token.type = TokenType::Error;
  return token;
}

JsonLoader::Token JsonLoader::Tokenizer::read_number() {
  Token token;
  token.type = TokenType::Number;
  token.line = line_;

  while (pos_ < length_) {
    char c = peek();
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' ||
        c == '.' || c == 'e' || c == 'E') {
      token.value += advance();
    }
    else {
      break;
    }
  }

  return token;
}

JsonLoader::Token JsonLoader::Tokenizer::read_identifier() {
  Token token;
  token.line = line_;

  while (pos_ < length_) {
    char c = peek();
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') {
      token.value += advance();
    }
    else {
      break;
    }
  }

  if (token.value == "true") token.type = TokenType::True;
  else if (token.value == "false") token.type = TokenType::False;
  else if (token.value == "null") token.type = TokenType::Null;
  else {
    error_ = "Unknown identifier";
    token.type = TokenType::Error;
  }

  return token;
}

JsonLoader::Token JsonLoader::Tokenizer::next_token() {
  skip_whitespace();

  if (error_) {
    Token token;
    token.type = TokenType::Error;
    token.line = line_;
    return token;
  }

  if (pos_ >= length_) {
    Token token;
    token.type = TokenType::End;
    token.line = line_;
    return token;
  }

  char c = peek();
  Token token;
  token.line = line_;

  switch (c) {
    case '{': advance(); token.type = TokenType::LeftBrace; break;
    case '}': advance(); token.type = TokenType::RightBrace; break;
    case '[': advance(); token.type = TokenType::LeftBracket; break;
    case ']': advance(); token.type = TokenType::RightBracket; break;
    case ':': advance(); token.type = TokenType::Colon; break;
    case ',': advance(); token.type = TokenType::Comma; break;
    case '"': return read_string();
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return read_number();
    default:
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return read_identifier();
      }
      error_ = "Unexpected character";
      token.type = TokenType::Error;
      break;
  }

  return token;
}

bool JsonLoader::expect_token(Tokenizer& tokenizer, TokenType expected, Token& out) {
  out = tokenizer.next_token();
  return out.type == expected;
}

bool JsonLoader::parse_object(Tokenizer& tokenizer, StringMap& out) {
  Token token;

  // Expect {
  if (!expect_token(tokenizer, TokenType::LeftBrace, token)) {
    return false;
  }

  // Handle empty object
  token = tokenizer.next_token();
  if (token.type == TokenType::RightBrace) {
    return true;
  }

  // Parse top-level key-value pairs
  while (true) {
    // Key must be string
    if (token.type != TokenType::String) {
      return false;
    }
    std::string key = token.value;

    // Colon
    if (!expect_token(tokenizer, TokenType::Colon, token)) {
      return false;
    }

    // Value
    token = tokenizer.next_token();

    if (key == "strings" && token.type == TokenType::LeftBrace) {
      // This is the main strings object - parse it
      // Put the { back by not consuming it
      // Actually, parse_string_object expects to consume the {
      // So we need to handle this differently

      // Parse the strings object content
      Token inner = tokenizer.next_token();
      while (inner.type != TokenType::RightBrace) {
        if (inner.type != TokenType::String) return false;
        std::string inner_key = inner.value;

        if (!expect_token(tokenizer, TokenType::Colon, inner)) return false;
        if (!expect_token(tokenizer, TokenType::String, inner)) return false;

        out[inner_key] = inner.value;

        inner = tokenizer.next_token();
        if (inner.type == TokenType::Comma) {
          inner = tokenizer.next_token();
        }
        else if (inner.type != TokenType::RightBrace) {
          return false;
        }
      }
    }
    else if (token.type == TokenType::String) {
      // Regular string value - skip (like _meta fields)
    }
    else if (token.type == TokenType::LeftBrace) {
      // Nested object - skip it
      int depth = 1;
      while (depth > 0) {
        Token inner = tokenizer.next_token();
        if (inner.type == TokenType::LeftBrace) depth++;
        else if (inner.type == TokenType::RightBrace) depth--;
        else if (inner.type == TokenType::Error || inner.type == TokenType::End) {
          return false;
        }
      }
    }
    else if (token.type == TokenType::LeftBracket) {
      // Array - skip it
      int depth = 1;
      while (depth > 0) {
        Token inner = tokenizer.next_token();
        if (inner.type == TokenType::LeftBracket) depth++;
        else if (inner.type == TokenType::RightBracket) depth--;
        else if (inner.type == TokenType::Error || inner.type == TokenType::End) {
          return false;
        }
      }
    }
    // Other value types (numbers, booleans) are just skipped

    // Next token
    token = tokenizer.next_token();
    if (token.type == TokenType::Comma) {
      token = tokenizer.next_token();
    }
    else if (token.type != TokenType::RightBrace) {
      return false;
    }
    else {
      break;  // Right brace, done
    }
  }

  return true;
}

bool JsonLoader::parse_file(const char* filepath, StringMap& out_strings) {
  FILE* file = nullptr;
  fopen_s(&file, filepath, "rb");
  if (!file) {
    fp_log_warn(L"JSON open failed: %S", filepath ? filepath : "<null>");
    return false;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Handle empty file
  if (size <= 0) {
    fclose(file);
    fp_log_warn(L"JSON file is empty: %S", filepath ? filepath : "<null>");
    return false;
  }

  // Read file
  std::string content(size, '\0');
  size_t bytes_read = fread(&content[0], 1, size, file);
  fclose(file);

  if (bytes_read != static_cast<size_t>(size)) {
    fp_log_error(L"JSON read failed: %S", filepath ? filepath : "<null>");
    return false;
  }

  return parse_memory(content.c_str(), content.size(), out_strings);
}

bool JsonLoader::parse_memory(const char* json_content, size_t length, StringMap& out_strings) {
  Tokenizer tokenizer(json_content, length);

  if (!parse_object(tokenizer, out_strings)) {
    if (tokenizer.get_error()) {
      fp_log_error(L"JSON parse error at line %d: %S", tokenizer.get_line(), tokenizer.get_error());
    }
    else {
      fp_log_error(L"JSON parse failed near line %d", tokenizer.get_line());
    }
    return false;
  }

  return true;
}
