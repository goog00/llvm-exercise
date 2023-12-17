; ModuleID = 'algebraic_ident_test'
source_filename = "algebraic_ident_test"

define i32 @main() {
entry:
  %0 = alloca i32, align 4
  store i32 10, i32* %0, align 4
  %1 = load i32, i32* %0, align 4

  ; 这是一个可以被优化的乘法指令
  %mul = mul i32 %1, 2

  ret i32 %mul
}