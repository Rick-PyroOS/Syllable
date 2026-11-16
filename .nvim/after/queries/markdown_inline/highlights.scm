;; extends

; Hide the backslash of a markdown escape (\\ -> \, \* -> *). clangd escapes every
; special character in doc comments, so popups showed \\ingroup and \*\*\*.
((backslash_escape) @conceal
  (#offset! @conceal 0 0 0 -1)
  (#set! conceal ""))
