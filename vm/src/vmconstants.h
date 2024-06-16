#ifndef VMCONSTANTS_H_
#define VMCONSTANTS_H_

#define OP_STACK_SIZE 64
#define FRAME_STACK_SIZE 1024
#define MAX_LOCALS 16

// use a prime number for capacity so it better works with the current `hash set` gc implementation
#define GC_INITIAL_CAP 1069

#endif // VMCONSTANTS_H_
