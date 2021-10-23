#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <cstdarg>
#include <iostream>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

class Scanner : public ScannerBase {
public:
  Scanner() = delete;
  explicit Scanner(std::string_view fname, std::ostream &out = std::cout)
      : ScannerBase(std::cin, out), comment_level_(1), char_pos_(1),
        errormsg_(std::make_unique<err::ErrorMsg>(fname)) {
    switchStreams(errormsg_->infile_, out);
  }

  /**
   * Output an error
   * @param message error message
   */
  void Error(int pos, std::string message, ...) {
    va_list ap;
    va_start(ap, message);
    errormsg_->Error(pos, message, ap);
    va_end(ap);
  }

  /**
   * Getter for `tok_pos_`
   */
  [[nodiscard]] int GetTokPos() const { return errormsg_->GetTokPos(); }

  /**
   * Transfer the ownership of `errormsg_` to the outer scope
   * @return unique pointer to errormsg
   */
  [[nodiscard]] std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

  int lex();

private:
  int comment_level_;
  std::string string_buf_;
  int char_pos_;
  std::unique_ptr<err::ErrorMsg> errormsg_;

  static const int CONTROL_CHAR_STRLEN = 3;
  static const int PRINTABLE_CHAR_STRLEN = 4;

  /**
   * NOTE: do not change all the funtion signature below, which is used by
   * flexc++ internally
   */
  int lex__();
  int executeAction__(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum__ type);
  void adjust();
  void adjustStr();
};

inline int Scanner::lex() { return lex__(); }

inline void Scanner::preCode() {
  // Optionally replace by your own code
}

inline void Scanner::postCode(PostEnum__ type) {
  // Optionally replace by your own code
}

inline void Scanner::print() { print__(); }

inline void Scanner::adjust() {
  std::string mStr = matched();
  if (mStr != "\n" && mStr != " "
      && mStr != "\t" && mStr != "\\")  // ignore \f___f\ in string
    errormsg_->tok_pos_ = char_pos_;
  if (mStr == "/*")  // start of comment
    comment_level_++;
  else if (mStr == "*/") // end of comment
    comment_level_--;
  char_pos_ += length();
}

inline void Scanner::adjustStr() {
  std::string mStr = matched();
  int pChar;
  char_pos_ += length();
  if (mStr == "\"") { // end of string
    setMatched(string_buf_);
    string_buf_ = "";
  } else if (mStr == "\\n") { // linefeed in string
    string_buf_ += '\n';
  } else if (mStr == "\\t") { // tab in string
    string_buf_ += '\t';
  } else if (mStr == "\\\"") {  // quote in string
    string_buf_ += '\"';
  } else if (mStr == "\\\\") {  // backslash in string
    string_buf_ += '\\';
  } else if (mStr.size() == CONTROL_CHAR_STRLEN
             && mStr.find("\\^") == 0
             && mStr.back() >= 'A'
             && mStr.back() <= 'Z') { // ascii for control code
    string_buf_ += (char)(mStr.back() - 'A' + 1);
  } else if (mStr.size() == PRINTABLE_CHAR_STRLEN
             && sscanf(mStr.c_str(), "\\%d", &pChar)
                    != std::char_traits<char>::eof()) { // ascii for printable character
    string_buf_ += (char)pChar;
  } else {
    string_buf_ += mStr;
  }
}

#endif // TIGER_LEX_SCANNER_H_
