.SECTION .time_critical
.SYNTAX UNIFIED
.THUMB_FUNC
.BALIGN 4

.EQU    return_addr_offset, 0x18    // See ARM v6 reference manual, section B1.5.6
.EQU    r0_offset, 0
.EQU    r1_offset, 4
.EQU    task_CONTROL, 2             // SPSEL=1, i.e. tasks use PSP stack, exceptions use MSP stack
.EQU    task_ready, 1               // Must match enum TastState.

.EXTERN atomic_start, atomic_end

// void rtos_internal_init_stacks(Scheduler* exception_stack_top)
.GLOBAL rtos_internal_init_stacks
.TYPE rtos_internal_init_stacks, %function
rtos_internal_init_stacks:
        // Thread mode continues to use current main stack but as processor stack.
        MOV     R3, SP
        MSR     PSP, R3
        MOVS    R3, #task_CONTROL
        MSR     CONTROL, R3

        // Exception mode uses new main stack.
        MSR     MSP, R0

        BX      LR


// void rtos_supervisor_svc_handler()
.GLOBAL rtos_supervisor_svc_handler
.TYPE rtos_supervisor_svc_handler, %function
rtos_supervisor_svc_handler:
        PUSH    {LR}

        // Load proc to call.
        MRS     R3, PSP
        LDR     R2, [R3, #r0_offset]

        // Store 0 as default return value
        MOVS    R1, #0
        STR     R1, [R3, #r0_offset]

        // Load call context.
        LDR     R1, [R3, #r1_offset]

        // Invoke critical section callback.
        ADD     R0, SP, #4
        BLX     R2

        // Context switch if running state not wanted.
        CMP     R0, #0
        BNE     context_switch

        POP     {PC}
        

// void rtos_supervisor_systick_handler
.GLOBAL rtos_supervisor_systick_handler
.TYPE rtos_supervisor_systick_handler, %function
rtos_supervisor_systick_handler:
        // EXC_RETURN value.
        PUSH    {LR}

        // bool rtos_supervisor_systick(Scheduler*)
        LDR     R3, =rtos_supervisor_systick
        ADD     R0, SP, #4
        BLX     R3
        CMP     R0, #0
        BNE     context_switch_ready

        POP     {PC}

// void rtos_supervisor_pendsv_handler()
.GLOBAL rtos_supervisor_pendsv_handler
.TYPE rtos_supervisor_pendsv_handler, %function
rtos_supervisor_pendsv_handler:
        // EXC_RETURN value.
        PUSH    {LR}

        // bool rtos_supervisor_pendsv(Scheduler*)
        LDR     R3, =rtos_supervisor_pendsv
        ADD     R0, SP, #4
        BLX     R3

context_switch_ready:
        // R0 becomes the first parameter of rtos_internal_switch_tasks.
        MOVS    R0, #task_ready

context_switch:
        // Get yielding task's SP.
        MRS     R3, PSP

        // Get yielding task's TCB.
        LDR     R1, [SP, #4]
        MOVS    R2, R1

        // Save SP, R4-R7 in TCB
        STM     R1!, {R3, R4-R7}

        // Save R8-R11 & LR in TCB.
        MOV     R4, R8
        MOV     R5, R9
        MOV     R6, R10
        MOV     R7, R11
        STM     R1!, {R4-R7}

        // Task* rtos_supervisor_context_switch(TaskState new_state, Scheduler*, Task* current);
        ADD     R1, SP, #4
        BL      rtos_supervisor_context_switch

        // Store new TCB.
        STR     R0, [SP, #4]

        // Restore R8-R11 & LR from TCB.
        MOVS    R1, R0
        ADDS    R1, R1, #4*5
        LDM     R1!, {R2-R5}
        MOV     R8, R2
        MOV     R9, R3
        MOV     R10, R4
        MOV     R11, R5

        // Restore SP, R4-R7 from TCB.
        LDM     R0!, {R2, R4-R7}
        MSR     PSP, R2

        // Get return address.
        LDR     R1, [R2, #return_addr_offset]

        // Was task running an atomic operation?
        LDR     R3, =atomic_start
        CMP     R1, R3
        BLT     0f
        LDR     R3, =atomic_end
        CMP     R1, R3
        BGE     0f

        // Was the task in a rollback region?
        LSLS    R3, R1, #27
        LSRS    R3, R3, #27
        CMP     R3, #24
        BGT     0f

        // Rollback atomic operation in progress to beginning.
        LSRS    R1, R1, #5
        LSLS    R1, R1, #5
        STR     R1, [R2, #return_addr_offset]

        // EXC_RETURN value.
0:      POP     {PC}
