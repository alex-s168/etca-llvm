define i16 @test_redefine_r0(i16 %a, i16 %b) {
entry:
  ; Redefine r0 before call (this should trigger the crash)
  %tmp = add i16 %a, 1
  %result = call i16 @helper(i16 %tmp)
  %result2 = add i16 %result, %a
  ret i16 %result2
}

declare i16 @helper(i16)
