.intel_syntax noprefix

.global load_next_char_nonfinalstate_template
.global load_next_char_nonfinalstate_template_end
.global load_next_char_finalstate_template
.global load_next_char_finalstate_template_end

load_next_char_nonfinalstate_template:
mov al, byte ptr [rdi]
test al, 0xff
jne else
// al is obviesly zero, so is the rest of rax because we zero it in the beginning
ret
else:
inc rdi
load_next_char_nonfinalstate_template_end:

load_next_char_finalstate_template:
mov al, byte ptr [rdi]
test al, 0xff
jne else2
// al is obviesly zero, so is the rest of rax because we zero it in the beginning
// this sets it to 1
inc eax
ret
else2:
inc rdi
load_next_char_finalstate_template_end:
