; ModuleID = 'algebraic_ident_test'
source_filename = "algebraic_ident_test"

define i32 @main() {
entry:
  
  %add = add i32 0, 5
  %tmp = mul i32 %add ,8
  %mul = mul i32 %tmp, 7
  ret i32 %mul
}
