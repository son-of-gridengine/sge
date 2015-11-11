;; Emacs local variables for SGE

((nil
  . ((indent-tabs-mode . nil)
     ;; There are timestamps in man pages in particular
     (time-stamp-start . "\\$Date: ")
     (time-stamp-end . " \\$")
     (time-stamp-format . "%:y-%02m-%02d %02H:%02M:%02S")
     (time-stamp-count . 2)
     (time-stamp-line-limit . 50)))
 (c-mode
  . ((c-file-style . "bsd")             ; ?
     (c-basic-offset . 3)
     (parens-require-spaces . nil)      ; not safe-local by default
     (show-trailing-whitespace . t) ; there's a lot of it, worth seeing
     )))
