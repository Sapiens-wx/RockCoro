    .text
    .globl ctx_first_swap
    .type  ctx_first_swap, @function

# void ctx_entry_swap(Context* from, Context* to) asm("ctx_first_swap");
# normal context swap function. swaps and saves r12-15, rbx, rbp, rsp, rip
ctx_entry_swap:
    # params : from = rdi, to = rsi
    movq %rsp, 40(%rdi)
    movq %rbx, 48(%rdi)
    movq %rbp, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)

    movq 32(%rsi), %rbp
    movq 40(%rsi), %rsp
    movq 8(%rsp), %rdi # load rdi
    movq 16(%rsp), %rsi # load rsi
    movq (%rsp), %rax # set return address
	jmp %rax

    .globl ctx_swap
    .type  ctx_swap, @function

# void ctx_swap(Context* from, Context* to) asm("ctx_swap");
# normal context swap function. swaps and saves r12-15, rbx, rbp, rsp, rip
ctx_swap:
    # params : from = rdi, to = rsi
    movq %rbx, 48(%rdi)
    movq %rsp, 40(%rdi)
    movq %rbp, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)

    movq 32(%rsi), %rbp
    movq 40(%rsi), %rsp
    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 48(%rsi), %rbx
    movq (%rsp), %rax # set return address
	jmp %rax

    .globl ctx_last_swap
    .type  ctx_last_swap, @function
# void ctx_exit_swap(Context* to) asm("ctx_last_swap");
# swaps the context but does not save the context
# used when a coroutine returns
ctx_exit_swap:
    movq 32(%rdi), %rbp
    movq 40(%rdi), %rsp
    movq (%rdi), %r15
    movq 8(%rdi), %r14
    movq 16(%rdi), %r13
    movq 24(%rdi), %r12
    movq 48(%rdi), %rbx
    movq (%rsp), %rax # set return address
	jmp %rax
