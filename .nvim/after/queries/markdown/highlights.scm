;; extends

; Hide "*****..." banner lines from old-style comment blocks in doc popups.
((thematic_break) @conceal
  (#lua-match? @conceal "^%*%*%*%*%*")
  (#set! conceal ""))
