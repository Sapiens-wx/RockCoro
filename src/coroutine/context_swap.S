    .text
    .globl ctx_first_swap
    .type  ctx_first_swap, @function

# normal context swap function. swaps and saves r12-15, rbx, rbp, rsp, rip
ctx_first_swap:
    # params : from = rdi, to = rsi
    leaq (%rsp),%rax
    movq %rax, 56(%rdi) # rsp
    movq %rbx, 48(%rdi)
	movq 0(%rax), %rax # rip
	movq %rax, 40(%rdi) 
    movq %rbp, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)
	xorq %rax, %rax

    movq 32(%rsi), %rbp
    movq 56(%rsi), %rsp
    movq (%rsp), %rdi # load rdi
	leaq 8(%rsp), %rsp # leave space for the next pushq command
	pushq 40(%rsi) # set return address to rip
    movq 8(%rsp), %rsi # load rsi
	ret

    .globl ctx_swap
    .type  ctx_swap, @function

# normal context swap function. swaps and saves r12-15, rbx, rbp, rsp, rip
ctx_swap:
    # params : from = rdi, to = rsi
    leaq (%rsp),%rax
    movq %rax, 56(%rdi) # rsp
    movq %rbx, 48(%rdi)
	movq 0(%rax), %rax # rip
	movq %rax, 40(%rdi) 
    movq %rbp, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)
	xorq %rax, %rax

    movq 32(%rsi), %rbp
    movq 56(%rsi), %rsp
    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 48(%rsi), %rbx
	leaq 8(%rsp), %rsp # leave space for the next pushq command
	pushq 40(%rsi) # set return address to rip
	ret

    .globl ctx_last_swap
    .type  ctx_last_swap, @function
# swaps the context but does not save the context
# used when a coroutine returns
ctx_last_swap:
    movq 32(%rsi), %rbp
    movq 56(%rsi), %rsp
    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 48(%rsi), %rbx
	leaq 8(%rsp), %rsp # leave space for the next pushq command
	pushq 40(%rsi) # set return address to rip
	ret
