((nil
  (eval
   (lambda ()
     (when (string= (file-name-extension buffer-file-name)
                    "F")
       (f90-mode))))))
