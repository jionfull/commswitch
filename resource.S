

.align 5
.global sha1_begin
.global sha1_end
sha1_begin:
.incbin "../.git/refs/heads/master"
sha1_end:


.align 5
.global config_begin
.global config_end
config_begin:
.incbin "../config.ini"
config_end:
