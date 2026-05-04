; Recursive fibonacci — this should trigger the crash
; because $r0 is used both as argument and return value
; across recursive calls
define i16 @fib(i16 %n) {
entry:
  %cmp = icmp ult i16 %n, 2
  br i1 %cmp, label %return, label %recurse

recurse:
  %n1 = sub i16 %n, 1
  %a = call i16 @fib(i16 %n1)
  %n2 = sub i16 %n, 2
  %b = call i16 @fib(i16 %n2)
  %sum = add i16 %a, %b
  ret i16 %sum

return:
  ret i16 %n
}
