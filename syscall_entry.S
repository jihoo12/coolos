.text
.global syscall_entry
.extern Syscall_Handler
.extern tss
.extern user_rsp_scratch

# user_rsp_scratch needs to be defined somewhere. 
# Defining it here in .data section of this assembly file.
.section .data
.global user_rsp_scratch
user_rsp_scratch: .quad 0

.section .text
syscall_entry:
    # On entry:
    # RIP -> RCX
    # RFLAGS -> R11
    # CS/SS updated
    # RSP is still User Stack!

    # 1. Save User Stack Pointer
    movq %rsp, user_rsp_scratch(%rip)

    # 2. Switch to Kernel Stack
    # TSS.rsp0 is at offset 4
    # tss is a global symbol
    movq tss+4(%rip), %rsp

    # 3. Save Registers
    pushq %rcx  # User RIP
    pushq %r11  # User RFLAGS
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # 4. Arg Shuffling for C ABI (Windows x64 or System V?)
    # The Makefiles says -target x86_64-unknown-windows
    # Windows ABI: RCX, RDX, R8, R9. Stack.
    # User(Linux/SysV style): RDI, RSI, RDX, R10, R8, R9.
    
    # Wait! The current compilation target is WINDOWS.
    # CFLAGS = -target x86_64-unknown-windows
    # So `Syscall_Handler` expects:
    # Arg1: RCX
    # Arg2: RDX
    # Arg3: R8
    # Arg4: R9
    # Arg5+: Stack
    
    # But I am implementing the OS. I can define the syscall ABI.
    # Let's say User passes args in: RCX, RDX, R8, R9...
    # BUT `syscall` instruction destroys RCX (puts RIP there).
    # So User CANNOT pass arg1 in RCX.
    # Standard choice:
    # User: RDI, RSI, RDX, R10, R8, R9 (Linux style)
    # Kernel C Handler (Windows ABI): RCX, RDX, R8, R9
    
    # We need to map:
    # User RDI (Arg1) -> Kernel RCX
    # User RSI (Arg2) -> Kernel RDX
    # User RDX (Arg3) -> Kernel R8
    # User R10 (Arg4) -> Kernel R9
    # User R8  (Arg5) -> Stack
    # User R9  (Arg6) -> Stack
    # Syscall Num (RAX) -> We can pass this as Arg1? Or separate?
    
    # Let's define Syscall_Handler signature:
    # void Syscall_Handler(uint64_t sys_num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
    # C (Windows) sees:
    # RCX = sys_num
    # RDX = a1
    # R8  = a2
    # R9  = a3
    # Stack = a4
    # Stack = a5
    
    # Mapping:
    # User RAX (sys_num) -> RCX
    # User RDI (a1)      -> RDX
    # User RSI (a2)      -> R8
    # User RDX (a3)      -> R9
    # User R10 (a4)      -> Stack
    # User R8  (a5)      -> Stack
    
    # Also need shadow space (32 bytes) for Windows ABI.
    
    subq $48, %rsp  # 32 bytes shadow + 16 bytes for args 5,6?
    # Actually, we need to push args 5 and 6.
    # And allocate 32 bytes shadow.
    
    # Implementation:
    # movq %rax, %rcx  (sys_num)
    # movq %rdi, %rdx  (a1)
    # movq %rsi, %r8   (a2)
    # movq %rdx, %r9   (a3)
    
    # args on stack:
    # [RSP + 32] = a4 (R10)
    # [RSP + 40] = a5 (R8)
    
    movq %r10, 32(%rsp)
    movq %r8, 40(%rsp)
    
    movq %rax, %rcx
    movq %rdi, %rdx
    movq %rsi, %r8
    movq %rdx, %r9
    
    call Syscall_Handler
    
    addq $48, %rsp

    # 5. Restore Registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    popq %r11  # RFLAGS
    popq %rcx  # RIP

    # 6. Switch Stack
    movq user_rsp_scratch(%rip), %rsp

    # 7. Return
    sysretq
