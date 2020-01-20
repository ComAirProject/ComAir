#ifndef INHOUSE_FUNCPTRHOOKS_H
#define INHOUSE_FUNCPTRHOOKS_H

extern "C" {
void func_ptr_hook_init();

void func_ptr_hook_enter_func(unsigned funcId);

void func_ptr_hook_exit_func(unsigned funcId);

void func_ptr_hook_call_inst(unsigned instId);

void func_ptr_hook_final();
}

#endif //INHOUSE_FUNCPTRHOOKS_H
