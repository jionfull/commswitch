

.align 5
.global sha1_begin
.global sha1_end
sha1_begin:
.incbin ".git/refs/heads/master"
sha1_end:
