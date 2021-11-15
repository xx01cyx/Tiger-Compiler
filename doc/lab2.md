# Lab 2 Document

Here is the document for lab2, which aims to implement a lexical scanner for the Tiger language based on *flex*.

There are 4 start conditions in my flex-based scanner: `INITIAL`, `COMMENT`, `STR` and `IGNORE`. `COMMENT` is for 
comments, `STR` for strings, `IGNORE` for the ignored part in strings, and `INITIAL` is for all the remaining common 
cases. The following sections will respectively elaborate how I handle comments, strings, errors as well as *EOF* while 
scanning.

## Comments

In order to deal with comments, the scanner owns a field named comment level, which indicates whether or not the current 
token is a comment as well as how deep the comment is nested. The initial value of comment level is 0, indicating that
the current token is not a comment, while in a non-zero-comment-level case, the larger the value is, the deeper the 
comment is nested. The contents of a comment are simply ignored instead of being stored as a token.

If the scanner meets with a `/*` under `INITIAL` or `COMMENT` start condition, the comment level will increase by 1.
Correspondingly, if the scanner meets with a `*/` under `COMMENT` start condition, the comment level will decrease by 1. 
The scanner will remain in `COMMENT` start condition until the comment level is reduced to 0. 

## Strings

If the scanner meets with a `"` under `INITIAL` start condition, it will enter `STR` start condition and remain until it
meets with another `"`. Under `STR` start condition, if the scanner meets with a single ``\`` (not ``\n`` or anything 
like that), it will enter `IGNORE` start condition and remain until it meets with another single ``\``, corresponding to 
the string-ignore rule of the Tiger language. The contents wrapped in the two ``\`` are simply ignored instead of being
recognized as part of the string,

## Errors and *EOF*

* If *EOF* is detected under `STR` start condition, the scanner will report an error indicating an unterminated string.

* If *EOF* is detected under `COMMENT` start condition, the scanner will report an error indicating an unterminated 
  comment.
  
* If the input cannot match any of the patterns, the scanner will report an error indicating an illegal token.