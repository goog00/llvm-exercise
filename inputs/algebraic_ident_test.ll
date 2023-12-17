; ModuleID = 'algebraic_ident_test'
source_filename = "algebraic_ident_test"

define i32 @main() {
entry:
  
  %add = add i32 0, 5
  %tmp1 = add i32 %add ,8
  %mul = mul i32 1, 7
  %tmp2 = mul i32 %mul ,9
  ret i32 %mul
}
